/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2018-2019 James Lu <james@overdrivenetworks.com>
 *   Copyright (C) 2026 TheUnstoppable <theunstoppable1000@gmail.com>
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


#include "inspircd.h"
#include "modules/account.h"

/// $ModAuthor: James Lu
/// $ModAuthorMail: james@overdrivenetworks.com
/// $ModDepends: core 4
/// $ModDesc: Turns /list into a honeypot for newly connected users
/// $ModConfig: <fakelist waittime="30s" reason="User hit a spam trap" target="#spamtrap" minusers="20" maxusers="50" topic="SPAM TRAP: DO NOT JOIN, YOU WILL BE DISCONNECTED! (try again later for a real reply)" killonjoin="true">

typedef std::vector<std::string> AllowList;

class ModuleFakeList : public Module
{
	Account::API accountapi;
	AllowList allowlist;
	bool exemptregistered;
	unsigned int WaitTime;

	std::string targetChannel;
	std::string topic;
	std::string reason;
	unsigned int minUsers;
	unsigned int maxUsers;
	bool killOnJoin;

 public:
	ModuleFakeList() :
		Module(VF_NONE, "Turns /list into a honeypot for newly connected users"),
		accountapi(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		AllowList newallows;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("securehost"))
		{
			std::string host = tag->getString("exception");
			if (host.empty())
				throw ModuleException(this, "<securehost:exception> is a required field at " + tag->source.str());
			newallows.push_back(host);
		}

		ConfigTag* tag = ServerInstance->Config->ConfValue("fakelist").get();

		exemptregistered = tag->getBool("exemptregistered");
		WaitTime = tag->getDuration("waittime", 60, 1);
		allowlist.swap(newallows);

		reason = tag->getString("reason", "User hit a spam trap", 1);
		targetChannel = tag->getString("target", "#spamtrap");
		topic = tag->getString("topic", "SPAM TRAP: DO NOT JOIN, YOU WILL BE DISCONNECTED! (try again later for a real reply)");
		minUsers = tag->getNum("minusers", 20U, 1U);
		maxUsers = tag->getNum("maxusers", 50U, minUsers);
		killOnJoin = tag->getBool("killonjoin", true);
	}


	/*
	 * OnPreCommand()
	 *   Intercept the LIST command.
	 */
	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return MOD_RES_PASSTHRU;

		if ((command == "LIST") && (ServerInstance->Time() < (user->signon+WaitTime)) && (!user->IsOper()))
		{
			/* Normally wouldnt be allowed here, are they exempt? */
			for (std::vector<std::string>::iterator x = allowlist.begin(); x != allowlist.end(); x++)
				if (InspIRCd::Match(user->GetRealMask(), *x, ascii_case_insensitive_map))
					return MOD_RES_PASSTHRU;

			if (exemptregistered && accountapi && accountapi->GetAccountName(user))
				return MOD_RES_PASSTHRU;

			// Yeah, just give them some fake channels to ponder.
			unsigned long int userCount = ServerInstance->GenRandomInt(maxUsers-minUsers) + minUsers;

			user->WriteNumeric(RPL_LISTSTART, "Channel", "Users Name");
			user->WriteNumeric(RPL_LIST, targetChannel, userCount, topic);
			user->WriteNumeric(RPL_LISTEND, "End of channel list.");

			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (killOnJoin && irc::equals(cname, targetChannel))
		{
			if (!user->IsOper())
			{
				// They did the unspeakable, kill them!
				ServerInstance->Users.QuitUser(user, reason);
			}
			else
			{
				// Berate opers who try to do the same. (this uses the same numeric as CBAN in 3.0)
				user->WriteNumeric(926, cname, "Cannot join channel (Reserved spamtrap channel for fakelist)");
			}
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleFakeList)
