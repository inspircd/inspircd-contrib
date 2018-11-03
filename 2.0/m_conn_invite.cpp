/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 * Copyright (C) 2016 Christoph Kern <sheogorath@shivering-isles.com>
 * Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 * Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 * Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 * Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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
/* $ModAuthor: Damian West */
/* $ModDesc: Invites all users to a channel when they connect to the network. */
/* $ModDepends: core 2.0 */
/* $ModConfig: <autoinvite channel="#one,#two,#three"> */ static void InviteChannels(LocalUser* u, const std::string& chanlist) {
	irc::commasepstream chans(chanlist);
	std::string chan;
 
	while (chans.GetToken(chan))
	{
		if (IS_LOCAL(u)) {
			IS_LOCAL(u)->InviteTo(chan.c_str(), 0);
			u->WriteServ("INVITE %s :%s", u->nick.c_str(), chan.c_str());
		}
	}
}
 
class ModuleConnInvite : public Module {
	public:
		void init()
		{
			Implementation eventlist[] = { I_OnLoadModule, I_OnPostConnect };
			ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		}
		void Prioritize()
		{
			ServerInstance->Modules->SetPriority(this, I_OnPostConnect, PRIORITY_LAST);
		}
		Version GetVersion()
		{
			return Version("Invites users to join the specified channel(s) on connect", VF_VENDOR);
		}
		void OnPostConnect(User* user)
		{
			LocalUser* localuser = IS_LOCAL(user);
			if (!localuser)
				return;
			std::string chanlist = localuser->GetClass()->config->getString("autoinvite");
			
			if (chanlist.empty())
			{
				ConfigTag* tag = ServerInstance->Config->ConfValue("autoinvite");
				chanlist = tag->getString("channel");
			}
			if (chanlist.empty())
				return;
 
			InviteChannels(localuser, chanlist);
		}
};

MODULE_INIT(ModuleConnInvite)