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

/// $ModAuthor: WindowsUser
/// $ModDesc: Gives /ALINE and /GALINE, short for auth-lines. Users affected by these will have to use SASL to connect, while any users already connected but not identified to services will be disconnected in a similar manner to G-lines.
/// $ModDepends: core 4

#include "inspircd.h"
#include "timeutils.h"
#include "modules/account.h"
#include "modules/stats.h"
#include "xline.h"

Account::API* g_accountapi = nullptr;

static bool isLoggedIn(const User* user)
{
	return *g_accountapi && (*g_accountapi)->GetAccountName(user);
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

	void Apply(User* u) override
	{
		if (!isLoggedIn(u))
		{
			u->WriteNotice("*** NOTICE -- You need to identify via SASL to use this server (your host is " + type + "-lined).");
			ServerInstance->Users.QuitUser(u, type + "-lined: "+this->reason);
		}
	}

	void DisplayExpiry() override
	{
		ServerInstance->SNO.WriteToSnoMask('x', "Removing expired {}-line {}@{} (set by {} {} ago): {}",
			type, identmask, hostmask, source, Duration::ToString(ServerInstance->Time() - this->set_time), reason);
	}

	bool Matches(User* u) const override
	{
		LocalUser* lu = IS_LOCAL(u);
		if (lu && lu->exempt)
			return false;

		if (InspIRCd::Match(u->GetRealUser(), this->identmask, ascii_case_insensitive_map))
		{
			if (InspIRCd::MatchCIDR(u->GetRealHost(), this->hostmask, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(u->GetAddress(), this->hostmask, ascii_case_insensitive_map))
			{
				return true;
			}
		}

		return false;
	}

	bool Matches(const std::string& s) const override
	{
		return (matchtext == s);
	}

	const std::string& Displayable() const override
	{
		return matchtext;
	}
};

class ALine : public GALine
{
 public:
	ALine(time_t s_time, long d, const std::string& src, const std::string& re, const std::string& ident, const std::string& host)
		: GALine(s_time, d, src, re, ident, host, "A") {}

	bool IsBurstable() override
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
	ALine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) override
	{
		auto ih = ServerInstance->XLines->SplitUserHost(xline_specific_mask);
		return new ALine(set_time, duration, source, reason, ih.first, ih.second);
	}
};

class GALineFactory : public XLineFactory
{
 public:
	GALineFactory() : XLineFactory("GA") { }

	/** Generate a GALine
	*/
	GALine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) override
	{
		auto ih = ServerInstance->XLines->SplitUserHost(xline_specific_mask);
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
		access_needed = CmdAccess::OPERATOR;
		this->syntax = { "<user@host> [<duration> :<reason>]" };
		this->linename = linetype;
		statschar = stats;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		std::string target = parameters[0];
		if (parameters.size() >= 3)
		{
			UserHostPair ih;
			User* find = ServerInstance->Users.FindNick(target, true);
			if (find)
			{
				ih.first = "*";
				ih.second = find->GetAddress();
				target = std::string("*@") + find->GetAddress();
			}
			else
				ih = ServerInstance->XLines->SplitUserHost(target);

			if (ih.first.empty())
			{
				user->WriteNotice("*** Target not found.");
				return CmdResult::FAILURE;
			}

			else if (target.find('!') != std::string::npos)
			{
				user->WriteNotice(linename + "-line cannot operate on nick!user@host masks.");
				return CmdResult::FAILURE;
			}

			XLineFactory* xlf = ServerInstance->XLines->GetFactory(linename);
			if (!xlf)
				return CmdResult::FAILURE;

			unsigned long duration;
			if (!Duration::TryFrom(parameters[1], duration))
			{
				user->WriteNotice("*** Invalid duration for " + linename + "-line.");
				return CmdResult::FAILURE;
			}
			XLine* al = xlf->Generate(ServerInstance->Time(), duration, user->nick, parameters[2], target);
			if (ServerInstance->XLines->AddLine(al, user))
			{
				if (!duration)
				{
					ServerInstance->SNO.WriteToSnoMask('x', "{} added permanent {}-line for {}: {}",
						user->nick, linename, target, parameters[2]);
				}
				else
				{
					ServerInstance->SNO.WriteToSnoMask('x', "{} added timed {}-line for {}, expires in {} (on {}): {}",
						user->nick, linename, target, Duration::ToString(duration),
						Time::ToString(ServerInstance->Time() + duration), parameters[2]);
				}
				ServerInstance->XLines->ApplyLines();
			}
			else
			{
				delete al;
				user->WriteNotice("*** " + linename + "-line for " + target + " already exists.");
			}
		}
		else
		{
			std::string reason;
			if (ServerInstance->XLines->DelLine(target.c_str(), linename, reason, user))
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} removed {}-line on {}: {}",
					user->nick, linename, target, reason);
			}
			else
			{
				user->WriteNotice("*** " + linename + "-line " + target + " not found in list, try /stats " + ConvToStr(statschar) + ".");
			}
		}

		return CmdResult::SUCCESS;
	}

};

class CommandALine: public CommandGALine
{
 public:
	CommandALine(Module* c) : CommandGALine(c, "A", 'a') {}
};

class ModuleRequireAuth : public Module, public Stats::EventListener
{
	CommandALine cmd1;
	CommandGALine cmd2;
	ALineFactory fact1;
	GALineFactory fact2;
	Account::API accountapi;

 public:
	ModuleRequireAuth()
		: Module(VF_COMMON, "Gives /ALINE and /GALINE, short for auth-lines. Users affected by these will have to use SASL to connect, while any users already connected but not identified to services will be disconnected in a similar manner to G-lines.")
		, Stats::EventListener(this)
		, cmd1(this)
		, cmd2(this)
		, accountapi(this)
	{
		g_accountapi = &accountapi;
	}

	void init() override
	{
		ServerInstance->XLines->RegisterFactory(&fact1);
		ServerInstance->XLines->RegisterFactory(&fact2);
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		/*stats A does global lines, stats a local lines.*/
		if (stats.GetSymbol() == 'A')
		{
			ServerInstance->XLines->InvokeStats("GA", stats);
			return MOD_RES_DENY;
		}
		else if (stats.GetSymbol() == 'a')
		{
			ServerInstance->XLines->InvokeStats("A", stats);
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

	ModResult OnCheckReady(LocalUser* user) override
	{
		/*I'm afraid that using the normal xline methods would then result in this line being checked at the wrong time.*/
		if (!isLoggedIn(user))
		{
			XLine* locallines = ServerInstance->XLines->MatchesLine("A", user);
			XLine* globallines = ServerInstance->XLines->MatchesLine("GA", user);
			if (locallines)
			{
				user->WriteNotice("*** NOTICE -- You need to identify via SASL to use this server (your host is A-lined).");
				ServerInstance->Users.QuitUser(user, "A-lined: "+locallines->reason);
				return MOD_RES_DENY;
			}
			else if (globallines)
			{
				user->WriteNotice("*** NOTICE -- You need to identify via SASL to use this server (your host is GA-lined).");
				ServerInstance->Users.QuitUser(user, "GA-lined: "+globallines->reason);
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleRequireAuth)
