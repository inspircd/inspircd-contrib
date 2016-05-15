/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Christoph Kern <sheogorath@shivering-isles.com>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModAuthor: Christoph "Sheogorath" Kern */
/* $ModDesc: Replaces m_conn_join.so and forces users to join the specified channel(s) after a specified delay on connect */
/* $ModDepends: core 2.0 */
/* $ModConfig: <autojoin channel="#one,#two,#three" delay="30"> */
static void JoinChannels(LocalUser* u, const std::string& chanlist)
{
	irc::commasepstream chans(chanlist);
	std::string chan;
 
	while (chans.GetToken(chan))
	{
		if (ServerInstance->IsChannel(chan.c_str(), ServerInstance->Config->Limits.ChanMax))
			Channel::JoinUser(u, chan.c_str(), false, "", false, ServerInstance->Time());
	}
}
 
class JoinTimer : public Timer
{
 private:
	LocalUser* const user;
	std::string uuid;
	const std::string channels;
 
 public:
	JoinTimer(LocalUser* u, const std::string& chans, unsigned int delay)
		: Timer(delay, ServerInstance->Time(), false)
		, user(u), channels(chans)
	{
		uuid = user->uuid;
		ServerInstance->Timers->AddTimer(this);
	}
 
	void Tick(time_t time)
	{
		if (ServerInstance->FindUUID(uuid) != user)
			return;
 
		if (user->chans.empty())
			JoinChannels(user, channels);
	}
};

class ModuleConnJoin : public Module
{
	public:
		void init()
		{
			OnModuleLoad(NULL);
			Implementation eventlist[] = { I_OnLoadModule, I_OnPostConnect };
			ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		}

		void Prioritize()
		{
			ServerInstance->Modules->SetPriority(this, I_OnPostConnect, PRIORITY_LAST);
		}

		Version GetVersion()
		{
			return Version("Forces users to join the specified channel(s) on connect", VF_VENDOR);
		}

		void OnPostConnect(User* user)
		{
			LocalUser* localuser = IS_LOCAL(user);
			if (!localuser)
				return;

			std::string chanlist = localuser->GetClass()->config->getString("autojoin");
			unsigned int chandelay = localuser->GetClass()->config->getInt("autojoindelay", 0);
			
			if (chanlist.empty())
			{
				ConfigTag* tag = ServerInstance->Config->ConfValue("autojoin");
				chanlist = tag->getString("channel");
				chandelay = tag->getInt("delay", 0);
			}

			if (chanlist.empty())
				return;
 
			if (!chandelay)
				JoinChannels(localuser, chanlist);
			else
				new JoinTimer(localuser, chanlist, chandelay);
		}

		void OnModuleLoad(Module* mod)
		{
			if (ServerInstance->Modules->Find("m_conn_join.so") != NULL)
				throw ModuleException("This module replaces m_conn_join.so. You can't load it together with m_conn_join.so.");
		}
};


MODULE_INIT(ModuleConnJoin)
