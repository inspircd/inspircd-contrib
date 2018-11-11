/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2018 James Lu <james@overdrivenetworks.com>
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

/* $ModAuthor: James Lu */
/* $ModAuthorMail: james@overdrivenetworks.com */
/* $ModConfig: <fakelist waittime="30s" reason="Yeah, no." target="#spamtrap" minusers="20" maxusers="50" topic="SPAM TRAP: DO NOT JOIN, YOU WILL BE DISCONNECTED! (try again later for a real reply)" killonjoin="true"> */
/* $ModDesc: Turns /list into a honeypot for newly connected users */
/* $ModDepends: core 2.0 */

class ModuleFakeList : public Module
{
 private:
	std::vector<std::string> allowlist;
	time_t WaitTime;
	std::string targetChannel;
	std::string targetTopic;
	std::string reason;
	unsigned int minUsers;
	unsigned int maxUsers;
	bool killOnJoin;

 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnPreCommand, I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleFakeList()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Turns /list into a honeypot for newly connected users");
	}

	void OnRehash(User* user)
	{
		allowlist.clear();

		ConfigTagList exemptTags = ServerInstance->Config->ConfTags("securehost");
		for (ConfigIter i = exemptTags.first; i != exemptTags.second; ++i)
			allowlist.push_back(i->second->getString("exception"));

		ConfigTag* tag = ServerInstance->Config->ConfValue("fakelist");

		WaitTime = tag->getInt("waittime", 30);
		reason = tag->getString("reason", "Yeah, no.");

		// Options for the fake channel we're making
		targetChannel = tag->getString("target", "#spamtrap");
		targetTopic = tag->getString("topic", "SPAM TRAP: DO NOT JOIN, YOU WILL BE DISCONNECTED! (try again later for a real reply)");
		minUsers = tag->getInt("minusers", 20);
		maxUsers = tag->getInt("maxusers", 50);
		killOnJoin = tag->getBool("killonjoin", true);
	}


	/*
	 * OnPreCommand()
	 *   Intercept the LIST command.
	 */
	virtual ModResult OnPreCommand(std::string& command, std::vector<std::string>& parameters, LocalUser* user, bool validated, const std::string& original_line)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return MOD_RES_PASSTHRU;

		if ((command == "LIST") && (ServerInstance->Time() < (user->signon+WaitTime)) && (!IS_OPER(user)))
		{
			/* Normally wouldnt be allowed here, are they exempt? */
			for (std::vector<std::string>::iterator x = allowlist.begin(); x != allowlist.end(); x++)
				if (InspIRCd::Match(user->MakeHost(), *x, ascii_case_insensitive_map))
					return MOD_RES_PASSTHRU;

			// Yeah, just give them some fake channels to ponder.
			unsigned long int userCount = minUsers + (rand() % static_cast<int>(maxUsers - minUsers + 1));
			user->WriteNumeric(321, "%s Channel :Users Name", user->nick.c_str());
			user->WriteNumeric(322, "%s %s %lu :%s", user->nick.c_str(), targetChannel.c_str(), userCount,
			                   targetTopic.c_str());
			user->WriteNumeric(323, "%s :End of channel list.", user->nick.c_str());
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string& privs, const std::string& keygiven)
	{
		if (killOnJoin && cname == targetChannel) {
			if (!IS_OPER(user))
			{
				// They did the unspeakable, kill them!
				ServerInstance->Users->QuitUser(user, reason);
			} else
			{
				// Berate opers who try to do the same. (this uses the same numeric as CBAN in 3.0)
				user->WriteNumeric(926, "%s %s :Cannot join channel (Reserved spamtrap channel for fakelist)",
								   user->nick.c_str(), cname);
			}
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

};

MODULE_INIT(ModuleFakeList)
