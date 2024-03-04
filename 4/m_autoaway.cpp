/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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
/// $ModConfig: <autoaway checkperiod="5m" idleperiod="24h" message="Idle">
/// $ModDepends: core 4
/// $ModDesc: Automatically marks idle users as away.


#include "inspircd.h"
#include "extension.h"
#include "modules/away.h"

enum
{
	// From RFC 1459.
	RPL_UNAWAY = 305,
	RPL_NOWAWAY = 306
};

class ModuleAutoAway final
	: public Module
	, public Timer
	, public Away::EventListener
{
private:
	BoolExtItem autoaway;
	Away::EventProvider awayevprov;
	unsigned long idleperiod;
	std::string message;
	bool setting = false;

public:
	ModuleAutoAway()
		: Module(VF_NONE, "Automatically marks idle users as away.")
		, Timer(0, true)
		, Away::EventListener(this)
		, autoaway(this, "autoaway", ExtensionType::CHANNEL)
		, awayevprov(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("autoaway");
		SetInterval(tag->getDuration("checkperiod", 5*60));
		idleperiod = tag->getDuration("idleperiod", 24*60*60);
		message = tag->getString("message", "Idle");
	}

	bool Tick() override
	{
		ServerInstance->Logs.Debug(MODNAME, "Checking for idle users ...");
		setting = true;
		time_t idlethreshold = ServerInstance->Time() - idleperiod;

		for (auto* user : ServerInstance->Users.GetLocalUsers())
		{
			// Skip users who are already away or who are not idle.
			if (user->IsAway() || user->idle_lastmsg > idlethreshold)
				continue;

			autoaway.Set(user);
			const auto prevstate = user->away;
			user->away.emplace(message);
			user->WriteNumeric(RPL_NOWAWAY, "You have been automatically marked as being away");
			awayevprov.Call(&Away::EventListener::OnUserAway, user, prevstate);
		}
		setting = false;
		return true;
	}

	void OnUserAway(User* user, const std::optional<AwayState>& prevstate) override
	{
		// If the user is changing their away status then unmark them.
		if (IS_LOCAL(user) && !setting)
			autoaway.Unset(user);
	}

	void OnUserBack(User* user, const std::optional<AwayState>& prevstate) override
	{
		// If the user is unsetting their away status then unmark them.
		if (IS_LOCAL(user))
			autoaway.Unset(user);
	}

	void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details) override
	{
		if (!IS_LOCAL(user) || !autoaway.Get(user))
			return;

		const auto prevstate = user->away;
		autoaway.Unset(user);
		user->away.reset();
		user->WriteNumeric(RPL_UNAWAY, "You are no longer automatically marked as being away");
		awayevprov.Call(&Away::EventListener::OnUserBack, user, prevstate);
	}
};

MODULE_INIT(ModuleAutoAway)
