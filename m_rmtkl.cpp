/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

/* $ModDesc: Removes all xlines (bans) matching given parameters. */
/* $ModAuthor: Alexey */
/* $ModAuthorMail: Phoenix@RusNet */
/* $ModDepends: core 1.2 */

bool XLineTypeSort(const std::string &f1, const std::string &f2)
{
	return (f1.size()>f2.size());
}


class CommandRMTKL : public Command
{
	std::list<std::string> alltypes;

	public:
		CommandRMTKL (InspIRCd* Instance) : Command(Instance,"RMTKL", "o", 2)
		{
			this->source = "m_rmtkl.so";
			this->syntax = "<type> <hostmask> [<comment mask>]";
			alltypes.clear();
		}

		~CommandRMTKL()
		{
			alltypes.clear();
		}

		CmdResult Handle (const std::vector<std::string>& parameters, User *user)
		{
			GenSortedList();

			std::string matchlinesstr(parameters[0]),matchreason("*");

			if (parameters.size()>2)
				matchreason=parameters[2];

			std::list<std::string> matchtypes; matchtypes.clear();

			//handle "*"
			if (matchlinesstr.find("*")!=std::string::npos)
			{
				//copy all types
				matchtypes=std::list<std::string>(alltypes.begin(),alltypes.end());

				//delete nick matches
				const char * charnicklines[]={"Q","SVSHOLD"};
				std::vector<std::string> nicklines(charnicklines,charnicklines+2);
				for (std::vector<std::string>::iterator nicklinesiter=nicklines.begin();nicklinesiter!=nicklines.end();++nicklinesiter)
				{
					matchtypes.remove(*nicklinesiter);
				}
			}

			for (std::list<std::string>::iterator iter=alltypes.begin();iter!=alltypes.end();++iter)
			{
				/*debug :)*/
				//				user->WriteServ("NOTICE %s :Ordered types '%s'",user->nick.c_str(), (*iter).c_str());
				std::string::size_type meetpos;

				while ((meetpos=matchlinesstr.find(*iter))!=std::string::npos)
				{
					bool minus=false;

					//remove from list and put there again if needed
					matchtypes.remove(*iter);
					if ((meetpos>0) && (matchlinesstr.at(meetpos-1)=='-'))
						minus=true;
					else
						matchtypes.push_back(*iter);

					//remove occurences in line
					matchlinesstr.erase(meetpos,iter->size());
					if (minus)
						matchlinesstr.erase(meetpos-1,1);
				}
			}

			for (std::list<std::string>::iterator iter=matchtypes.begin();iter!=matchtypes.end();++iter)
			{
				XLineLookup* lookup = ServerInstance->XLines->GetAll(*iter);

				if (lookup)
				{
					for (LookupIter i = lookup->begin(); i != lookup->end(); ++i)
					{
						/*K-lines etc. are local*/

						if (!i->second->IsBurstable()&&(!IS_LOCAL(user)))
							break;

						if (InspIRCd::Match(i->second->Displayable(),parameters[1])&&InspIRCd::Match(i->second->reason,matchreason))
							ServerInstance->XLines->DelLine(i->first.c_str(), *iter, user);

					}

				}
			}

			return CMD_SUCCESS;
		}

		void GenSortedList()
		{
			std::vector<std::string> unsorted=ServerInstance->XLines->GetAllTypes();
			alltypes=std::list<std::string> (unsorted.begin(),unsorted.end());
			alltypes.sort(XLineTypeSort);
		}

};

class ModuleRMTKL : public Module
{
	private:
		CommandRMTKL *r;
		ModuleRMTKL *mymodule;

	public:
		ModuleRMTKL(InspIRCd* Me) : Module(Me)
		{
			mymodule = this;

			r = new CommandRMTKL(ServerInstance);
			ServerInstance->AddCommand(r);
		}

		virtual ~ModuleRMTKL()
		{
		}

		virtual Version GetVersion()
		{
			return Version("$Id$", VF_VENDOR, API_VERSION);
		}

};

MODULE_INIT(ModuleRMTKL)

