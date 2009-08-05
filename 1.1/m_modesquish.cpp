/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "mode.h"

/* $ModDesc: Pre-parses MODE command to remove redundant modes */
/* $ModAuthor: Skip */
/* $ModREMOVEAuthorMail: unknown */
/* $ModDepends: core 1.1 */
/* $ModVersion: $Rev: 78 $ */

class module_modesquish : public Module
{
 public:
	
	module_modesquish(InspIRCd* Me)
		: Module(Me)
	{
	}

	void Implements(char* List)
	{
		List[I_OnPreCommand] = 1;
	}

	virtual ~module_modesquish()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,0,API_VERSION);
	}

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
	{
		if (validated && command == "MODE" && pcnt > 1)
		{
			class squishy
			{
				public:
				squishy() {};
				~squishy() {};
				bool adding;
				unsigned char flag;
				std::string *param;
			};

			/*
			 * Find out what we are
			 */
			chanrec* targetchannel	= ServerInstance->FindChan(parameters[0]);
			userrec* targetuser	= ServerInstance->FindNick(parameters[0]);
			ModeType type;
			ModeHandler* handler;
			bool adding = true;
			bool abort = false;

			squishy* squish;
			std::vector<void*> squish_list;

			if (targetchannel)
				type = MODETYPE_CHANNEL;
			else if (targetuser)
				type = MODETYPE_USER;
			else	return 0;

			std::string mode_sequence = parameters[1];

			std::string parameter;
			int parameter_index = 2;

			/*
			 * Loop through all the flags
			 */
			for (std::string::const_iterator letter = mode_sequence.begin(); letter != mode_sequence.end(); letter++)
			{
				unsigned char modechar = *letter;
				switch (modechar)
				{
					case '+':
					case '-':
					adding = (modechar == '+' ? true : false );
					break;

					default:
					handler = ServerInstance->Modes->FindMode(modechar, type);
					parameter.clear();

					/*
					 * Grab us a param ?
					 */
					if (handler && handler->GetModeType() == type
						&& handler->GetNumParams(adding) && parameter_index < pcnt)
						parameter = parameters[parameter_index++];

					squish = NULL;
					abort = false;
					for (std::vector<void*>::iterator i = squish_list.begin(); i != squish_list.end(); i++)
					{
						squish = (squishy*)(*i);
						if (squish->flag == modechar && ((squish->param->empty() && parameter.empty())
							|| !strcmp(squish->param->c_str(),parameter.c_str())))
						{
							abort = true;
							if (adding != squish->adding)
							{
								squish_list.erase(i);
								delete squish->param;
								delete squish;
							}
							
							break;
						}
					}
					
					if (abort)
						break;

					/*
					 * Add it to our squish list
					 */
					squish = new squishy();
					squish->flag = modechar;
					squish->adding = adding;
					squish_list.push_back((void*)squish);
					if (!parameter.empty())
						squish->param = new std::string (parameter);
					else	squish->param = new std::string ();
					
					break;
				}
			}
			

			/*
			 * Ok, build our mode string back from squish list
			 */

			const char *squish_p[127];
			int squish_pcnt = 2;
			std::string squish_mode_sequence;

			squish_p[0] = parameters[0]; /* target */
			adding = true;

			for (std::vector<void*>::iterator i = squish_list.begin(); i != squish_list.end(); i++)
			{
				squish = (squishy*)(*i);
				
				if (squish_mode_sequence.empty() || adding != squish->adding)
					squish_mode_sequence.append((squish->adding ? "+" : "-"));
				adding = squish->adding;
				squish_mode_sequence.append(1, squish->flag);

				if (!squish->param->empty())
					squish_p[squish_pcnt++] = squish->param->c_str();
			}

			squish_p[1] = squish_mode_sequence.c_str();

			/*
			 * Finally, send out our new and improved mode!
			 * (provided we didn't squish it to nothingness)
			 */
			if (!squish_mode_sequence.empty())
				ServerInstance->CallCommandHandler("MODE", squish_p, squish_pcnt, user);
				

			/*
			 * Clean up
			 */
			for (std::vector<void*>::iterator i = squish_list.begin(); i != squish_list.end(); i++)
			{
				squish = (squishy*)(*i);

				delete squish->param;
				delete squish;
			}
			squish_list.clear();

			/*
			 * And stop *this* MODE getting through
			 */
			return 1;

		}

		return 0;
	}
};

MODULE_INIT(module_modesquish)


