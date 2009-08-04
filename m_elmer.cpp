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

/* $ModDesc: Gives /elmer: makes someone talk like Elmer Fudd, that is, letters 'l' and 'r' become 'w', (More info at http://www.geocities.com/Hollywood/Park/2326/character1.html#elmer.) */
/* $ModAuthor: Sazpiamon */
/* $ModAuthorMail: w00t@inspircd.org */
/* $ModDepends: core 1.2 */
 
/** Holds an Elmer item
 */
class Elmer : public XLine
{
public:
	std::string matchtext;

	Elmer(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char *match) : XLine(Instance, s_time, d, src, re, "ELMER")
	{
		this->matchtext = match;
	}

	~Elmer()
	{
	}

	bool Matches(User *u)
	{
		if (InspIRCd::Match(u->GetFullHost(), matchtext) || InspIRCd::Match(u->GetFullRealHost(), matchtext) ||  InspIRCd::Match(std::string("*@") + u->GetIPString(), matchtext))
			return true;

		return false;
	}

	bool Matches(const std::string &s)
	{
		if (matchtext == s)
			return true;
		return false;
	}

	
	void Apply(User *u)
	{
		if (!u->GetExt("elmered"))
			u->Extend("elmered");
	}
	
	//Elmer's are always permanent, should I even bother with this?
	virtual void DisplayExpiry() { }

	const char* Displayable()
	{
		return matchtext.c_str();
	}
};

/** An XLineFactory specialized to generate elmer pointers
 */
class ElmerFactory : public XLineFactory
{
 public:
	ElmerFactory(InspIRCd* Instance) : XLineFactory(Instance, "ELMER") { }

	/** Generate a shun
 	*/
	XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
	{
		return new Elmer(ServerInstance, set_time, duration, source, reason, xline_specific_mask);
	}


};

/** Handle /ELMER
 */
class CommandElmer : public Command
{
 public:
 	char* identmask;
	char* hostmask;
	CommandElmer(InspIRCd* Me) : Command(Me, "ELMER", "o", 1, 1)
	{
		this->source = "m_elmer.so";
		this->syntax = "[+|-]<nick|mask>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		std::string target = parameters[0].substr(1);
		char action = parameters[0][0];
		if(action != '+' && action != '-') 
		{
			user->WriteServ("NOTICE %s : Syntax /ELMER [+|-]<nick|mask>", user->nick.c_str());
			return CMD_FAILURE;
		}
		IdentHostPair ih;
		User* find = ServerInstance->FindNick(target.c_str());
		if (find)
		{
				ih.first = "*";
				ih.second = find->GetIPString();
				target = std::string("*@") + find->GetIPString();
		}
		else
				ih = ServerInstance->XLines->IdentSplit(target.c_str());
		if (ih.first.empty())
		{
			user->WriteServ("NOTICE %s :*** Target not found", user->nick.c_str());
			return CMD_FAILURE;
		}
		identmask = strdup(ih.first.c_str());
		hostmask = strdup(ih.second.c_str());
		std::string matchtext = identmask;
		matchtext.append("@").append(hostmask);
		if (action == '-')
		{

			if (ServerInstance->XLines->DelLine(target.c_str(), "ELMER", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s made %s no longer talk like Elmer Fudd.",user->nick.c_str(),target.c_str());
				if (find)
					find->WriteServ("NOTICE %s :*** You are no longer talking like Elmer Fudd", find->nick.c_str());
			}
			else
			{
				user->WriteServ("NOTICE %s :*** %s is not talking like Elmer, try /stats J.",user->nick.c_str(),target.c_str());
			}

			return CMD_SUCCESS;
		}
		else if (action == '+')
		{
			Elmer *r = NULL;
			try
			{
				r = new Elmer(ServerInstance, ServerInstance->Time(), 0, user->nick.c_str(), "", matchtext.c_str());
			}
			catch (...)
			{
				; // Do nothing.
			}

			if (r)
			{
				if (ServerInstance->XLines->AddLine(r, user))
				{
					ServerInstance->SNO->WriteToSnoMask('x',"%s made %s talk like Elmer Fudd.",user->nick.c_str(),target.c_str());
					if (find)
						find->WriteServ("NOTICE %s :*** You are now talking like Elmer Fudd", find->nick.c_str());
					ServerInstance->XLines->ApplyLines();
				}
				else
				{
					delete r;
				}
			}
		}


		return CMD_SUCCESS;
	}
};

class ModuleElmer : public Module
{
	CommandElmer *mycommand;
	ElmerFactory *s;


 public:
	ModuleElmer(InspIRCd* Me) : Module(Me)
	{
		s = new ElmerFactory(ServerInstance);
		ServerInstance->XLines->RegisterFactory(s);
		mycommand = new CommandElmer(Me);
		ServerInstance->AddCommand(mycommand);
		Implementation eventlist[] = { I_OnUserPreNick, I_OnSyncOtherMetaData, I_OnDecodeMetaData, I_OnStats, I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}

	
	virtual int OnStats(char symbol, User* user, string_list &out)
	{
		if(symbol != 'J')
			return 0;

		ServerInstance->XLines->InvokeStats("ELMER", 210, user, out);
		return 1;
	}

	virtual void OnUserConnect(User* user)
	{
		if (!IS_LOCAL(user))
			return;

		// Check for Elmer on connect
		XLine *rl = ServerInstance->XLines->MatchesLine("ELMER", user);

		if (rl)
		{
		rl->Apply(user);
		}
	}

	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
			if (!IS_LOCAL(user))
				return 0;

			if (!user->GetExt("elmered"))
				return 0;
					
			if (!ServerInstance->XLines->MatchesLine("ELMER", user))
			{
				user->Shrink("elmered");
				return 0;
			}
			if (user->GetExt("elmered"))
			{
				for (std::string::iterator i = text.begin(); i != text.end(); i++)
				{
					switch (*i)
					{
						case 'l':
						case 'r':
							*i = 'w';
							break;
						case 'L':
						case 'R':
							*i = 'W';
					}
				}
			}

			return 0;
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
			return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual ~ModuleElmer()
	{
		ServerInstance->XLines->DelAll("ELMER");
		ServerInstance->XLines->UnregisterFactory(s);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleElmer)
