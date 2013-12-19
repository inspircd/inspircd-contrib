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
/* $ModAuthorMail: petpow@saberuk.com */
/* $ModDesc: Replays messages sent to the server back to the sender. */
/* $ModDepends: core 2.0-2.1 */

/* Developer Documentation
 * =======================
 *
 * Use Case
 * --------
 *
 * There are two main use cases for this module:
 *
 *   (1) If you are using a connection which can have a large amount of latency
 *       such as GPRS then it can be many minutes before a loss of connection
 *       can be detected by the client. This can result in a user chatting away
 *       without realising that their messages are going nowhere.
 *
 *   (2) If the server has a module loaded which modifies user messages (e.g.
 *       m_stripcolor) then the user has no useful way of knowing that their
 *       message has been modified. This was traditionally done using ERR_*
 *       numerics but due to the amount of non-standard extensions to the
 *       IRC protocol it is no longer sanely possible to rely on this method.
 *
 * Enabling
 * --------
 *
 * In order to use this module your client needs to support IRCv3 capability
 * negotiation. The CAP name of this module is inspircd.org/replay-message.
 *
 */

#include "inspircd.h"
#include "m_cap.h"

class ModuleReplayMessage : public Module
{
	GenericCap cap;

	void OnMsg(User* user, void* dest, int target_type, const std::string& text, char status, const char* cmd)
	{
		if (!cap.ext.get(user))
			return;

		std::string target;
		if (target_type == TYPE_USER)
		{
			User* destuser = (User*) dest;
			if (destuser == user)
				return;

			target = destuser->nick;
		}
		else if (target_type == TYPE_CHANNEL)
		{
			Channel* chan = (Channel*) dest;
			if (status)
				target.push_back(status);
			target.append(chan->name);
		}
		else if (target_type == TYPE_SERVER)
		{
			const char* destserver = (const char*) dest;
			target.append(destserver);
		}
		else
			return;

		user->WriteFrom(user, "%s %s :%s", cmd, target.c_str(), text.c_str());
	}

 public:
	ModuleReplayMessage()
		: cap(this, "inspircd.org/replay-message")
	{
	}

	void init()
	{
		Implementation eventList[] = { I_OnEvent, I_OnUserMessage, I_OnUserNotice };
		ServerInstance->Modules->Attach(eventList, this, sizeof(eventList)/sizeof(Implementation));
	}

	void OnEvent(Event& event)
	{
		cap.HandleEvent(event);
	}

	void OnUserMessage(User* user, void* dest, int target_type, const std::string& text, char status, const CUList& exempt_list)
	{
		OnMsg(user, dest, target_type, text, status, "PRIVMSG");
	}

	void OnUserNotice(User* user, void* dest, int target_type, const std::string& text, char status, const CUList& exempt_list)
	{
		OnMsg(user, dest, target_type, text, status, "NOTICE");
	}

	Version GetVersion()
	{
		return Version("Replays messages sent to the server back to the sender.");
	}
};

MODULE_INIT(ModuleReplayMessage)
