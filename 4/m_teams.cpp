/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
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

/// $ModAuthor: Sadie Powell <sadie@witchery.services>
/// $ModDepends: core 3
/// $ModDesc: Allows users to be managed using services-assigned teams.


#include "inspircd.h"
#include "extension.h"
#include "modules/extban.h"
#include "modules/isupport.h"
#include "modules/whois.h"

enum
{
	// InspIRCd specific.
	RPL_WHOISTEAMS = 695
};

// Represents a list of teams that a user is a member of.
typedef insp::flat_set<std::string, irc::insensitive_swo> TeamList;

class TeamExtBan final
	: public ExtBan::MatchingBase
{
private:
	ListExtItem<TeamList>& teamext;

public:
	TeamExtBan(Module* Creator, ListExtItem<TeamList>& ext)
		: ExtBan::MatchingBase(Creator, "team", 't')
		, teamext(ext)
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		TeamList* teams = teamext.Get(user);
		if (!teams)
			return false;

		for (const auto& team : *teams)
		{
			if (InspIRCd::Match(team, text))
				return true;
		}
		return true;
	}
};


class ModuleTeams final
	: public Module
	, public ISupport::EventListener
	, public Whois::EventListener
{
private:
	bool active = false;
	ListExtItem<TeamList> ext;
	TeamExtBan extban;
	std::string teamchar;

	size_t ExecuteCommand(LocalUser* source, const char* cmd, CommandBase::Params& parameters,
		const std::string& team, size_t nickindex)
	{
		size_t targets = 0;
		std::string command(cmd);
		for (const auto& [_, user] : ServerInstance->Users.GetUsers())
		{
			if (!user->IsFullyConnected())
				continue;

			TeamList* teams = ext.Get(user);
			if (!teams || teams->count(team))
				continue;

			parameters[nickindex] = user->nick;
			ModResult modres;
			FIRST_MOD_RESULT(OnPreCommand, modres, (command, parameters, source, true));
			if (modres != MOD_RES_DENY)
			{
				ServerInstance->Parser.CallHandler(command, parameters, source);
				targets++;
			}
		}
		return targets;
	}

	bool IsTeam(const std::string& param, std::string& team)
	{
		if (param.length() <= teamchar.length() || param.compare(0, teamchar.length(), teamchar) != 0)
			return false;

		team.assign(param, teamchar.length() - 1, std::string::npos);
		return true;
	}

	ModResult HandleInvite(LocalUser* source, CommandBase::Params& parameters)
	{
		// Check we have enough parameters and a valid team.
		std::string team;
		if (parameters.size() < 2 || !IsTeam(parameters[0], team))
			return MOD_RES_PASSTHRU;

		active = true;
		size_t penalty = ExecuteCommand(source, "INVITE", parameters, team, 0);
		source->CommandFloodPenalty += std::min(penalty, 5UL);
		active = false;
		return MOD_RES_DENY;
	}

public:
	ModuleTeams()
		: Module(VF_OPTCOMMON, "Allows users to be managed using services-assigned teams.")
		, ISupport::EventListener(this)
		, Whois::EventListener(this)
		, ext(this, "teams", ExtensionType::USER)
		, extban(this, ext)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("teams");
		teamchar = tag->getString("prefix", "^", 1);
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["TEAMCHAR"] = teamchar;
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		if (!user->IsFullyConnected() || !validated || active)
			return MOD_RES_PASSTHRU;

		if (command == "INVITE")
			return HandleInvite(user, parameters);

		return MOD_RES_PASSTHRU;
	}

	void OnWhois(Whois::Context& whois) override
	{
		TeamList* teams = ext.Get(whois.GetTarget());
		if (teams)
		{
			const std::string teamstr = stdalgo::string::join(*teams);
			whois.SendLine(RPL_WHOISTEAMS, teamstr, "is a member of these teams");
		}
	}
};

MODULE_INIT(ModuleTeams)
