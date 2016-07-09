/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 * Copyright (C) 2014 WindowsUser <jasper@jasperswebsite.com>
 * Based off the core xline methods and partially the services account module.
 *
 * This file is part of InspIRCd. InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* $ModAuthor: WindowsUser */
/* $ModDesc: Gives /aline and /galine, short for auth-lines. Users affected by these will have to use SASL to connect, while any users already connected but not identified to services will be disconnected in a similar manner to G-lines. */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
#include "xline.h"
#include "account.h"

static bool isLoggedIn(User* user)
{
	const AccountExtItem* accountext = GetAccountExtItem();
	return (accountext && accountext->get(user));
}

class GALine : public XLine
{
 protected:
	/** Ident mask (ident part only)
	*/
	std::string identmask;
	/** Host mask (host part only)
	*/
	std::string hostmask;

	std::string matchtext;

 public:
	GALine(time_t s_time, long d, const std::string& src, const std::string& re, const std::string& ident, const std::string& host, std::string othertext = "GA")
		: XLine(s_time, d, src, re, othertext), identmask(ident), hostmask(host)
	{
		matchtext = identmask;
		matchtext.append("@").append(this->hostmask);
	}

	void Apply(User* u)
	{
		if (!isLoggedIn(u))
		{
			u->WriteServ("NOTICE %s :*** NOTICE -- You need to identify via SASL to use this server (your host is %s-Lined).", u->nick.c_str(), type.c_str());
			ServerInstance->Users->QuitUser(u, type + "-Lined: "+this->reason);
		}
	}

	void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x',"Removing expired %s-Line %s@%s (set by %s %ld seconds ago)",
			type.c_str(), identmask.c_str(),hostmask.c_str(),source.c_str(),(long)(ServerInstance->Time() - this->set_time));
	}

	bool Matches(User* u)
	{
		if (u->exempt)
			return false;

		if (InspIRCd::Match(u->ident, this->identmask, ascii_case_insensitive_map))
		{
			if (InspIRCd::MatchCIDR(u->host, this->hostmask, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(u->GetIPString(), this->hostmask, ascii_case_insensitive_map))
			{
				return true;
			}
		}

		return false;
	}

	bool Matches(const std::string& s)
	{
		return (matchtext == s);
	}

	const char* Displayable()
	{
		return matchtext.c_str();
	}
};

class ALine : public GALine
{
 public:
	ALine(time_t s_time, long d, const std::string& src, const std::string& re, const std::string& ident, const std::string& host)
		: GALine(s_time, d, src, re, ident, host, "A") {}

	bool IsBurstable()
	{
		return false;
	}
};

class ALineFactory : public XLineFactory
{
 public:
	ALineFactory() : XLineFactory("A") { }

	/** Generate an ALine
	 */
	ALine* Generate(time_t set_time, long duration, std::string source, std::string reason, std::string xline_specific_mask)
	{
		IdentHostPair ih = ServerInstance->XLines->IdentSplit(xline_specific_mask);
		return new ALine(set_time, duration, source, reason, ih.first, ih.second);
	}
};

class GALineFactory : public XLineFactory
{
 public:
	GALineFactory() : XLineFactory("GA") { }

	/** Generate a GALine
	*/
	GALine* Generate(time_t set_time, long duration, std::string source, std::string reason, std::string xline_specific_mask)
	{
		IdentHostPair ih = ServerInstance->XLines->IdentSplit(xline_specific_mask);
		return new GALine(set_time, duration, source, reason, ih.first, ih.second);
	}
};

class CommandGALine: public Command
{
 protected:
	std::string linename;
	char statschar;

 public:
	CommandGALine(Module* c, const std::string& linetype = "GA", char stats = 'A')
		: Command(c, linetype+"LINE", 1, 3)
	{
		flags_needed = 'o';
		this->syntax = "<ident@host> [<duration> :<reason>]";
		this->linename = linetype;
		statschar = stats;
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		std::string target = parameters[0];
		if (parameters.size() >= 3)
		{
			IdentHostPair ih;
			User* find = ServerInstance->FindNick(target);
			if ((find) && (find->registered == REG_ALL))
			{
				ih.first = "*";
				ih.second = find->GetIPString();
				target = std::string("*@") + find->GetIPString();
			}
			else
				ih = ServerInstance->XLines->IdentSplit(target);

			if (ih.first.empty())
			{
				user->WriteServ("NOTICE %s :*** Target not found", user->nick.c_str());
				return CMD_FAILURE;
			}

			if (ServerInstance->HostMatchesEveryone(ih.first+"@"+ih.second,user))
				return CMD_FAILURE;
			else if (target.find('!') != std::string::npos)
			{
				std::string message = "NOTICE %s :*** "+linename+"-Line cannot operate on nick!user@host masks";
				user->WriteServ(message);
				return CMD_FAILURE;
			}

			XLineFactory* xlf = ServerInstance->XLines->GetFactory(linename);
			if (!xlf)
				return CMD_FAILURE;

			long duration = ServerInstance->Duration(parameters[1]);
			XLine* al = xlf->Generate(ServerInstance->Time(), duration, user->nick, parameters[2], target);
			if (ServerInstance->XLines->AddLine(al, user))
			{
				if (!duration)
				{
					ServerInstance->SNO->WriteToSnoMask('x', "%s added permanent %s-line for %s: %s",user->nick.c_str(), linename.c_str(), target.c_str(), parameters[2].c_str());
				}
				else
				{
					time_t c_requires_crap = duration + ServerInstance->Time();
					std::string timestr = ServerInstance->TimeString(c_requires_crap);
					ServerInstance->SNO->WriteToSnoMask('x',"%s added timed %s-line for %s, expires on %s: %s",user->nick.c_str(),linename.c_str(),target.c_str(),timestr.c_str(), parameters[2].c_str());
				}
				ServerInstance->XLines->ApplyLines();
			}
			else
			{
				delete al;
				user->WriteServ("NOTICE %s :*** %s-Line for %s already exists",user->nick.c_str(),linename.c_str(),target.c_str());
			}
		}
		else
		{
			if (ServerInstance->XLines->DelLine(target.c_str(),linename,user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s removed %s-line on %s",user->nick.c_str(),linename.c_str(),target.c_str());
			}
			else
			{
				user->WriteServ("NOTICE %s :*** %s-Line %s not found in list, try /stats %c.",user->nick.c_str(),linename.c_str(),target.c_str(), statschar);
			}
		}

		return CMD_SUCCESS;
	}

};

class CommandALine: public CommandGALine
{
 public:
	CommandALine(Module* c) : CommandGALine(c, "A", 'a') {}
};

class ModuleRequireAuth : public Module
{
	CommandALine cmd1;
	CommandGALine cmd2;
	ALineFactory fact1;
	GALineFactory fact2;

 public:
	ModuleRequireAuth() : cmd1(this), cmd2(this) {}

	void init()
	{
		ServerInstance->XLines->RegisterFactory(&fact1);
		ServerInstance->XLines->RegisterFactory(&fact2);
		ServerInstance->Modules->AddService(cmd1);
		ServerInstance->Modules->AddService(cmd2);
		Implementation eventlist[] = { I_OnCheckReady, I_OnStats };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnStats(char symbol, User* user, string_list& out)
	{
		/*stats A does global lines, stats a local lines.*/
		if (symbol == 'A')
		{
			ServerInstance->XLines->InvokeStats("GA", 210, user, out);
			return MOD_RES_DENY;
		}
		else if (symbol == 'a')
		{
			ServerInstance->XLines->InvokeStats("A", 210, user, out);
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	~ModuleRequireAuth()
	{
		ServerInstance->XLines->DelAll("A");
		ServerInstance->XLines->DelAll("GA");
		ServerInstance->XLines->UnregisterFactory(&fact1);
		ServerInstance->XLines->UnregisterFactory(&fact2);
	}

	Version GetVersion()
	{
		return Version("Gives /aline and /galine, short for auth-lines. Users affected by these will have to use SASL to connect, while any users already connected but not identified to services will be disconnected in a similar manner to G-lines.", VF_COMMON);
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		/*I'm afraid that using the normal xline methods would then result in this line being checked at the wrong time.*/
		if (!isLoggedIn(user))
		{
			XLine* locallines = ServerInstance->XLines->MatchesLine("A", user);
			XLine* globallines = ServerInstance->XLines->MatchesLine("GA", user);
			if (locallines)
			{
				user->WriteServ("NOTICE %s :*** NOTICE -- You need to identify via SASL to use this server (your host is A-Lined).", user->nick.c_str());
				ServerInstance->Users->QuitUser(user, "A-Lined: "+locallines->reason);
				return MOD_RES_DENY;
			}
			else if (globallines)
			{
				user->WriteServ("NOTICE %s :*** NOTICE -- You need to identify via SASL to use this server (your host is GA-Lined).", user->nick.c_str());
				ServerInstance->Users->QuitUser(user, "GA-Lined: "+globallines->reason);
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleRequireAuth)
