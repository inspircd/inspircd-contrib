/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2015 Sadie Powell <sadie@witchery.services>
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


/* $ModAuthor: Sadie Powell */
/* $ModAuthorMail: sadie@witchery.services */
/* $ModDesc: Replays messages sent to the server back to the sender. */
/* $ModDepends: core 2.0 */

/**
 * Developer documentation for this module can be found at:
 *
 *  https://github.com/ircv3/ircv3-specifications/blob/master/extensions/echo-message-3.2.md
 *  https://github.com/SadieCat/irc-docs/blob/master/specifications/replay-message.md
 *
 */

#include "inspircd.h"
#include "m_cap.h"

class ModuleReplayMessage : public Module
{
	GenericCap standardCap;
	GenericCap vendorCap;

	void OnMsg(User* user, void* dest, int target_type, const std::string& text, char status, const char* cmd)
	{
		if (!standardCap.ext.get(user) && !vendorCap.ext.get(user))
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
		: standardCap(this, "echo-message")
		, vendorCap(this, "inspircd.org/replay-message")
	{
	}

	void init()
	{
		Implementation eventList[] = { I_OnEvent, I_OnUserMessage, I_OnUserNotice };
		ServerInstance->Modules->Attach(eventList, this, sizeof(eventList)/sizeof(Implementation));
	}

	void OnEvent(Event& event)
	{
		standardCap.HandleEvent(event);
		vendorCap.HandleEvent(event);
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
