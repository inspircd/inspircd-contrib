/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 bigfoot547 <bigfoot@bigfootslair.net>
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

/* $ModAuthor: bigfoot547 */
/* $ModAuthorMail: bigfoot@bigfootslair.net */
/* $ModDesc: Replaces the quit message of a quitting user if they have been connected for less than a configurable time */
/* $ModDepends: core 2.0 */
/* $ModConfig: <timedstaticquit quitmsg="Client Quit" mintime="300"> */

#include "inspircd.h"

class ModuleTimedStaticQuit : public Module
{
 private:
	std::string quitmsg;
	unsigned int mintime;

 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnPreCommand, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnPreCommand(std::string& command, std::vector<std::string>&, LocalUser* user, bool validated, const std::string&)
	{
		// We check if the user has done his due time on the network
		if (validated && (command == "QUIT"))
		{
			time_t now = ServerInstance->Time();
			if ((now - user->signon) < mintime)
			{
				ServerInstance->Users->QuitUser(user, quitmsg);
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("timedstaticquit");
		this->quitmsg = tag->getString("quitmsg", "Client Quit");
		int duration = ServerInstance->Duration(tag->getString("mintime", "5m")); /* Duration is in the user-friendly format (1y2w3d4h5m6s) */
		this->mintime = duration <= 0 ? 1 : duration;                             /* The minimum time needs to be at least 1 second */
	}

	Version GetVersion()
	{
		// It was late, mk? Make me a better description pls :)
		return Version("Replaces the quit message of a quitting user if they have been connected for less than a configurable time", VF_OPTCOMMON);
	}

	void Prioritize()
	{
		// Since we take the quit command, we go last
		ServerInstance->Modules->SetPriority(this, I_OnPreCommand, PRIORITY_LAST);
	}
};

MODULE_INIT(ModuleTimedStaticQuit)
