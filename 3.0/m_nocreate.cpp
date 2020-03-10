/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016-2019 Matt Schatz <genius3000@g3k.solutions>
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <nocreate telluser="no" noisy="yes">
/// $ModDepends: core 3
/// $ModDesc: Adds oper command '/nocreate' to block a user from creating new channels

/* Credit due to the authors and maintainers of 'm_cban', as most of this code is directly borrowed to work in the same manner
 * See <https://github.com/inspircd/inspircd/blob/insp3/src/modules/m_cban.cpp>
 * As well as the base XLine and G/Z-Line commands code.
 *
 * Configuration and defaults:
 * telluser: tell the user they are blocked from creating a channel instead of a generic error. Default: no
 * noisy:    sends an SNOTICE when a blocked user tries to create a channel. Default: yes
 *
 * Helpop Lines for the COPER section:
 * Find: '<helpop key="coper" value="Oper Commands
 * Place 'NOCREATE' in the appropriate ordered spot.
 * Find: '<helpop key="wallops" ...'
 * Place just above that line:
<helpop key="nocreate" value="/NOCREATE <nick!user@hostmask> [<duration> :<reason>]

Sets or removes a no channel creation ban on a user. You must
specify all three parameters to add a ban, and just the mask to
remove a ban. Mask can be a valid, online nickname and will use
'*!*@<IP>' as a mask.">

 */


#include "inspircd.h"
#include "xline.h"
#include "modules/stats.h"

// Store the NoCreate mask as an XLine
class NoCreate : public XLine
{
	std::string mask;

 public:

	NoCreate(time_t s_time, unsigned long d, const std::string& src, const std::string& re, const std::string& ma)
		: XLine(s_time, d, src, re, "NOCREATE")
		, mask(ma)
	{
	}

	bool Matches(User* u) CXX11_OVERRIDE
	{
		return (InspIRCd::Match(u->GetFullHost(), this->mask) ||
			InspIRCd::Match(u->GetFullRealHost(), this->mask) ||
			InspIRCd::MatchCIDR(u->nick+"!"+u->ident+"@"+u->GetIPString(), this->mask));
	}

	bool Matches(const std::string& s) CXX11_OVERRIDE
	{
		return (InspIRCd::MatchCIDR(s, this->mask));
	}

	void DisplayExpiry() CXX11_OVERRIDE
	{
		ServerInstance->SNO->WriteToSnoMask('x', "Removing expired NoCreate %s (set by %s %s ago): %s",
			this->mask.c_str(), this->source.c_str(),
			InspIRCd::DurationString(ServerInstance->Time() - this->set_time).c_str(), this->reason.c_str());
	}

	std::string& Displayable() CXX11_OVERRIDE
	{
		return this->mask;
	}
};

// A specialized XLineFactory for NOCREATE pointers
class NoCreateFactory : public XLineFactory
{
 public:
	NoCreateFactory() : XLineFactory("NOCREATE") { }

	XLine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) CXX11_OVERRIDE
	{
		return new NoCreate(set_time, duration, source, reason, xline_specific_mask);
	}

	bool AutoApplyToUserList(XLine* x) CXX11_OVERRIDE
	{
		return false;
	}
};

// Handle /NOCREATE
class CommandNoCreate : public Command
{
 private:
	// Specialized Insane ban check, checks that both nick and userhost match.
	// To use the existing InspIRCd functions would mean running through the
	// user list twice, as nick and host insane checks are separate functions.
	bool MaskIsInsane(const std::string& mask, User* user)
	{
		unsigned int matches = 0;

		float itrigger = ServerInstance->Config->ConfValue("insane")->getFloat("trigger", 95.5);

		std::string nick;
		std::string userhost;

		std::string::size_type n = mask.find('!');
		// This should never happen; but just incase it does, return false
		if (n == std::string::npos)
			return false;

		nick = mask.substr(0, n);
		userhost = mask.substr(n+1);

		const user_hash& users = ServerInstance->Users->GetUsers();
		for (user_hash::const_iterator u = users.begin(); u != users.end(); ++u)
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

		float percent = ((float)matches / (float)users.size()) * 100;
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

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		std::string target = parameters[0];
		User* u = ServerInstance->FindNick(target);

		// Allow just 'nick' if valid, otherwise force use of 'nick!user@host'
		if (target.find('!') == std::string::npos || target.find('@') == std::string::npos)
		{
			if (u && u->registered == REG_ALL)
			{
				// Use *!*@IP if valid nick is given
				target = std::string("*!*@") + u->GetIPString();
			}
			else
			{
				user->WriteNotice(InspIRCd::Format("*** NoCreate: No user '%s' found", target.c_str()));
				return CMD_FAILURE;
			}
		}

		// Adding
		if (parameters.size() >= 3)
		{
			if (this->MaskIsInsane(target, user))
			{
				user->WriteNotice(InspIRCd::Format("*** NoCreate mask %s flagged as insane", target.c_str()));
				return CMD_FAILURE;
			}

			unsigned long duration;
			if (!InspIRCd::Duration(parameters[1], duration))
			{
				user->WriteNotice("*** Invalid duration for NoCreate.");
				return CMD_FAILURE;
			}

			NoCreate* nc = new NoCreate(ServerInstance->Time(), duration, user->nick, parameters[2], target);
			if (ServerInstance->XLines->AddLine(nc, user))
			{
				if (!duration)
				{
					ServerInstance->SNO->WriteToSnoMask('x', "%s added permanent NoCreate for %s: %s",
						user->nick.c_str(), target.c_str(), parameters[2].c_str());
				}
				else
				{
					ServerInstance->SNO->WriteToSnoMask('x', "%s added timed NoCreate for %s, expires in %s (on %s): %s",
						user->nick.c_str(), target.c_str(), InspIRCd::DurationString(duration).c_str(),
						ServerInstance->TimeString(ServerInstance->Time() + duration).c_str(), parameters[2].c_str());
				}
			}
			else
			{
				delete nc;
				user->WriteNotice(InspIRCd::Format("*** NoCreate for %s already exists", target.c_str()));
				return CMD_FAILURE;
			}
		}
		// Removing
		else
		{
			std::string reason;
			if (ServerInstance->XLines->DelLine(target.c_str(), "NOCREATE", reason, user))
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s removed NoCreate on %s: %s",
					user->nick.c_str(), target.c_str(), reason.c_str());
			}
			else
			{
				user->WriteNotice(InspIRCd::Format("*** NoCreate %s not found in list, try /stats N", target.c_str()));
				return CMD_FAILURE;
			}
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (IS_LOCAL(user))
			return ROUTE_LOCALONLY; // spanningtree will send ADDLINE

		return ROUTE_BROADCAST;
	}
};

// Main module class for NOCREATE
class ModuleNoCreate : public Module, public Stats::EventListener
{
	CommandNoCreate cmd;
	NoCreateFactory f;
	bool telluser;
	bool noisy;

 public:
	ModuleNoCreate()
		: Stats::EventListener(this)
		, cmd(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->XLines->RegisterFactory(&f);
	}

	~ModuleNoCreate()
	{
		ServerInstance->XLines->DelAll("NOCREATE");
		ServerInstance->XLines->UnregisterFactory(&f);
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("nocreate");
		telluser = tag->getBool("telluser");
		noisy = tag->getBool("noisy", true);
	}

	ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE
	{
		if (stats.GetSymbol() != 'N')
			return MOD_RES_PASSTHRU;

#if defined INSPIRCD_VERSION_BEFORE && INSPIRCD_VERSION_BEFORE(3, 6)
		ServerInstance->XLines->InvokeStats("NOCREATE", 210, stats);
#else
		ServerInstance->XLines->InvokeStats("NOCREATE", stats);
#endif
		return MOD_RES_DENY;
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		// Do nothing if the channel already exists or if the user is either an oper or exempt
		if (chan)
			return MOD_RES_PASSTHRU;
		if (user->IsOper() || user->exempt)
			return MOD_RES_PASSTHRU;

		XLine* nc = ServerInstance->XLines->MatchesLine("NOCREATE", user);

		if (nc)
		{
			// User is blocked from creating channels
			if (telluser)
				user->WriteNumeric(ERR_BANNEDFROMCHAN, cname, "You are not allowed to create channels");
			else
				user->WriteNumeric(Numerics::NoSuchChannel(cname));

			if (noisy)
				ServerInstance->SNO->WriteGlobalSno('a', "%s tried to create channel %s but is blocked from doing so (%s)",
					user->nick.c_str(), cname.c_str(), nc->reason.c_str());

			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Gives /nocreate, an XLine to block a user from creating new channels");
	}
};

MODULE_INIT(ModuleNoCreate)
