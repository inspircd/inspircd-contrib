/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 genius3000 <genius3000@g3k.solutions>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* $ModAuthor: genius3000 */
/* $ModAuthorMail: genius3000@g3k.solutions */
/* $ModDesc: Adds oper command '/nocreate' to block a user from creating new channels */
/* $ModDepends: core 2.0 */
/* $ModConfig: <nocreate telluser="no" noisy="yes"> */

/* Credit due to the authors and maintainers of 'm_cban', as most of this code is directly borrowed to work in the same manner
 * See <https://github.com/inspircd/inspircd/blob/insp20/src/modules/m_cban.cpp>
 * As well as the base XLine and G/Z-Line commands code.
 */

/* Documentation:
 * Config value of 'telluser' when set to yes, tells the user they are blocked from creating a channel instead of a generic error
 * Config value of 'noisy' when set to yes, sends a SNOTICE when a blocked user tries to create a channel
 * Defaults are telluser=no and noisy=yes
 *
 * Helpop Lines for the COPER section
 * Find: '<helpop key="coper" value="Oper Commands
 * Place 'NOCREATE' before 'FILTER  OJOIN'
 * Find: '<helpop key="cban" ...'
 * Place just above that line:
<helpop key="nocreate" value="/NOCREATE <nick!user@hostmask> [<duration> :<reason>]

Sets or removes a no channel creation ban on a user. You must
specify all three parameters to add a ban, and just the mask to
remove a ban. Mask can be a valid, online nickname and will use
'*!*@<IP>' as a mask.">

 */


#include "inspircd.h"
#include "xline.h"

/* Store the NoCreate mask as an XLine */
class NoCreate : public XLine
{
 public:
	std::string mask;

	NoCreate(time_t s_time, long d, const std::string& src, const std::string& re, const std::string& ma)
		: XLine(s_time, d, src, re, "NOCREATE")
	{
		this->mask = ma;
	}

	~NoCreate()
	{
	}

	bool Matches(User* u)
	{
		return (InspIRCd::Match(u->GetFullHost(), this->mask) ||
			InspIRCd::Match(u->GetFullRealHost(), this->mask) ||
			InspIRCd::MatchCIDR(u->nick+"!"+u->ident+"@"+u->GetIPString(), this->mask));
	}

	bool Matches(const std::string& s)
	{
		return (InspIRCd::MatchCIDR(s, this->mask));
	}

	void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x', "Removing expired NoCreate %s (set by %s %ld seconds ago)",
			this->mask.c_str(), this->source.c_str(), (long int)(ServerInstance->Time() - this->set_time));
	}

	const char* Displayable()
	{
		return this->mask.c_str();
	}
};

/* A specialized XLineFactory for NOCREATE pointers */
class NoCreateFactory : public XLineFactory
{
 public:
	NoCreateFactory() : XLineFactory("NOCREATE") { }

	XLine* Generate(time_t set_time, long duration, std::string source, std::string reason, std::string xline_specific_mask)
	{
		return new NoCreate(set_time, duration, source, reason, xline_specific_mask);
	}

	bool AutoApplyToUserList(XLine* x)
	{
		return false;
	}
};

/* Handle /NOCREATE */
class CommandNoCreate : public Command
{
 private:
	/* Specialized Insane ban check, checks that both nick and userhost match.
	 * To use the existing InspIRCd functions would mean running through the
	 * user list twice, as nick and host insane checks are separate functions. */
	bool MaskIsInsane(const std::string& mask, User* user)
	{
		unsigned int matches = 0;

		float itrigger = ServerInstance->Config->ConfValue("insane")->getFloat("trigger", 95.5);

		std::string nick;
		std::string userhost;

		std::string::size_type n = mask.find('!');
		/* This should never happen; but just incase it does, return false */
		if (n == std::string::npos)
			return false;

		nick = mask.substr(0, n);
		userhost = mask.substr(n+1);

		for (user_hash::iterator u = ServerInstance->Users->clientlist->begin(); u != ServerInstance->Users->clientlist->end(); u++)
		{
			if (InspIRCd::Match(u->second->nick, nick) &&
				(InspIRCd::Match(u->second->MakeHost(), userhost, ascii_case_insensitive_map) ||
				InspIRCd::MatchCIDR(u->second->MakeHostIP(), userhost, ascii_case_insensitive_map)))
			{
				matches++;
			}
		}

		if (!matches)
			return false;

		float percent = ((float)matches / (float)ServerInstance->Users->clientlist->size()) * 100;
		if (percent > itrigger)
		{
			ServerInstance->SNO->WriteToSnoMask('a', "\2WARNING\2: %s tried to set a NoCreate mask of %s, which covers %.2f%% of the network!",
				user->nick.c_str(), mask.c_str(), percent);
			return true;
		}

		return false;
	}

 public:
	CommandNoCreate(Module* Creator) : Command(Creator, "NOCREATE", 1, 3)
	{
		flags_needed = 'o';
		this->syntax = "<nick!user@hostmask> [<duration> :<reason>]";
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		std::string target = parameters[0];

		User* u = ServerInstance->FindNick(target);

		/* Allow just 'nick' if valid, otherwise force use of 'nick!user@host' */
		if (target.find('!') == std::string::npos || target.find('@') == std::string::npos)
		{
			if (u && u->registered == REG_ALL)
			{
				/* Use *!*@IP if valid nick is given */
				target = std::string("*!*@") + u->GetIPString();
			}
			else
			{
				user->WriteServ("NOTICE %s :*** NoCreate: No user '%s' found", user->nick.c_str(), target.c_str());
				return CMD_FAILURE;
			}
		}

		/* Adding */
		if (parameters.size() >= 3)
		{
			if (this->MaskIsInsane(target, user))
			{
				user->WriteServ("NOTICE %s :*** NoCreate mask %s flagged as insane",
					user->nick.c_str(), target.c_str());
				return CMD_FAILURE;
			}

			long duration = ServerInstance->Duration(parameters[1]);
			NoCreate* nc = new NoCreate(ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), target.c_str());

			if (ServerInstance->XLines->AddLine(nc, user))
			{
				if (!duration)
				{
					ServerInstance->SNO->WriteToSnoMask('x', "%s added permanent NoCreate %s: %s",
						user->nick.c_str(), target.c_str(), parameters[2].c_str());
				}
				else
				{
					ServerInstance->SNO->WriteToSnoMask('x', "%s added timed NoCreate %s, expires on %s: %s",
						user->nick.c_str(), target.c_str(),
						ServerInstance->TimeString(duration + ServerInstance->Time()).c_str(), parameters[2].c_str());
				}
			}
			else
			{
				delete nc;
				user->WriteServ("NOTICE %s :*** NoCreate %s already exists",
					user->nick.c_str(), target.c_str());
				return CMD_FAILURE;
			}
		}
		/* Removing */
		else
		{
			if (ServerInstance->XLines->DelLine(target.c_str(), "NOCREATE", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s removed NoCreate %s",
					user->nick.c_str(), target.c_str());
			}
			else
			{
				user->WriteServ("NOTICE %s :*** NoCreate %s not found in list, try /stats N",
					user->nick.c_str(), target.c_str());
				return CMD_FAILURE;
			}
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (IS_LOCAL(user))
			return ROUTE_LOCALONLY; // spanningtree will send ADDLINE

		return ROUTE_BROADCAST;
	}
};

/* Main module class for NOCREATE */
class ModuleNoCreate : public Module
{
	CommandNoCreate cmd;
	NoCreateFactory f;
	bool telluser;
	bool noisy;

 public:
	ModuleNoCreate()
		: cmd(this)
	{
	}

	void init()
	{
		OnRehash(NULL);
		ServerInstance->XLines->RegisterFactory(&f);
		ServerInstance->Modules->AddService(cmd);
		Implementation eventlist[] = { I_OnRehash, I_OnUserPreJoin, I_OnStats };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("nocreate");
		telluser = tag->getBool("telluser", false);
		noisy = tag->getBool("noisy", true);
	}

	virtual ~ModuleNoCreate()
	{
		ServerInstance->XLines->DelAll("NOCREATE");
		ServerInstance->XLines->UnregisterFactory(&f);
	}

	virtual ModResult OnStats(char symbol, User* user, string_list& out)
	{
		if (symbol != 'N')
			return MOD_RES_PASSTHRU;

		ServerInstance->XLines->InvokeStats("NOCREATE", 210, user, out);
		return MOD_RES_DENY;
	}

	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string& privs, const std::string& keygiven)
	{
		/* Do nothing if the channel already exists or if the user is either an oper or exempt */
		if (chan)
			return MOD_RES_PASSTHRU;
		if (IS_OPER(user) || user->exempt)
			return MOD_RES_PASSTHRU;

		XLine* nc = ServerInstance->XLines->MatchesLine("NOCREATE", user);

		if (nc)
		{
			/* User is blocked from creating channels */
			if (telluser)
				user->WriteNumeric(ERR_BANNEDFROMCHAN, "%s %s :You are not allowed to create channels",
					user->nick.c_str(), cname);
			else
				user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :Invalid channel name",
					user->nick.c_str(), cname);

			if (noisy)
				ServerInstance->SNO->WriteGlobalSno('a', "%s tried to create channel %s but is blocked from doing so (%s)",
					user->nick.c_str(), cname, nc->reason.c_str());

			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Gives /nocreate, an XLine to block a user from creating new channels");
	}
};

MODULE_INIT(ModuleNoCreate)
