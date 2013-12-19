/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Peter Powell <petpow@saberuk.com>
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


/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModDesc: Replays messages sent to the server back to the sender. */
/* $ModDepends: core 2.0-2.1 */

#include "inspircd.h"
#include "m_cap.h"

class ModuleReplayMessage : public Module
{
	GenericCap cap;

 public:
	 ModuleReplayMessage()
		 : cap(this, "inspircd.org/replay-message") { }

	void init()
	{
		Implementation eventList[] = { I_OnEvent, I_OnPostCommand };
		ServerInstance->Modules->Attach(eventList, this, sizeof(eventList)/sizeof(Implementation));
	}

	void OnEvent(Event& event)
	{
		cap.HandleEvent(event);
	}

	void OnPostCommand(const std::string& command, const std::vector<std::string>& parameters, LocalUser* user, CmdResult result, const std::string&)
	{
		if (result != CMD_SUCCESS)
			return;

		if (command != "NOTICE" && command != "PRIVMSG")
			return;

		if (!cap.ext.get(user))
			return;

		if (ServerInstance->FindNick(parameters[0]) == user)
			return;

		user->WriteFrom(user, "%s %s :%s", command.c_str(), parameters[0].c_str(), parameters[1].c_str());
	}

	Version GetVersion()
	{
		return Version("Replays messages sent to the server back to the sender.");
	}
};

MODULE_INIT(ModuleReplayMessage)
