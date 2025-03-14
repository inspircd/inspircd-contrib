/*
 *  InspIRCd -- Internet Relay Chat Daemon
 *
 *  This file is part of InspIRCd.  InspIRCd is free software: you can
 *  redistribute it and/or modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation, version 2, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 */

/// $ModAuthor: reverse <mike.chevronnet@gmail.com>
/// $ModDesc: Implements IRCv3 +draft/typing client capability.
/// $ModConfig: <typingnotify interval="3" timeout="6" pausethreshold="3">
/// $ModDepends: core 4

#include "inspircd.h"
#include "modules/cap.h"
#include "modules/ircv3.h"
#include "clientprotocol.h"
#include "message.h"
#include "timeutils.h"

// Typing state constants
enum class TypingState
{
	ACTIVE,
	PAUSED,
	DONE
};

// Typing state information per user per target
struct TypingInfo
{
	TypingState state;
	time_t last_update;

	TypingInfo() : state(TypingState::DONE), last_update(0) {}
};

// Maps: user UUID → (target name → typing state)
using TargetMap = std::map<std::string, TypingInfo>;
using UserTypingMap = std::map<std::string, TargetMap>;

class ModuleIRCv3Typing final
	: public Module
{
private:
	UserTypingMap typing_users;
	time_t typing_interval;
	time_t typing_timeout;
	time_t pause_threshold;
	ClientProtocol::EventProvider tagmsg_event;
	Cap::Capability typing_cap;

public:
	ModuleIRCv3Typing()
		: Module(VF_VENDOR, "Implements IRCv3 +draft/typing client capability.")
		, typing_interval(3)
		, typing_timeout(6)
		, pause_threshold(3)
		, tagmsg_event(this, "TAGMSG")
		, typing_cap(this, "+draft/typing")
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		auto tag = ServerInstance->Config->ConfValue("typingnotify");
		typing_interval = tag->getDuration("interval", 3, 1, 60);
		typing_timeout = tag->getDuration("timeout", 6, 1, 300);
		pause_threshold = tag->getDuration("pausethreshold", 3, 1, 60);
	}

	void BroadcastTypingUpdate(User* user, const std::string& target, TypingState state)
	{
		const std::string status = (state == TypingState::ACTIVE) ? "active"
			: (state == TypingState::PAUSED) ? "paused"
			: "done";

		ClientProtocol::TagMap tags;
		tags.emplace("+draft/typing", ClientProtocol::MessageTagData(nullptr, status));

		if (!target.empty() && target[0] == '#')
		{
			Channel* chan = ServerInstance->Channels.Find(target);
			if (!chan)
				return;

			ClientProtocol::Message msg("TAGMSG", user);
			for (const auto& [tagname, tagdata] : tags)
			{
				msg.AddTag(tagname, tagdata.tagprov, tagdata.value);
			}
			msg.PushParamRef(target);

			ClientProtocol::Event ev(tagmsg_event, msg);

			// Use IRCv3::WriteNeighborsWithCap to send the message only to users with the capability
			IRCv3::WriteNeighborsWithCap res(user, ev, typing_cap, false);
		}
		else
		{
			User* dest = ServerInstance->Users.Find(target);
			LocalUser* localdest = IS_LOCAL(dest);
			if (localdest && typing_cap.IsEnabled(localdest))
			{
				ClientProtocol::Message msg("TAGMSG", user);
				for (const auto& [tagname, tagdata] : tags)
				{
					msg.AddTag(tagname, tagdata.tagprov, tagdata.value);
				}
				msg.PushParamRef(dest->nick);

				ClientProtocol::Event ev(tagmsg_event, msg);
				localdest->Send(ev);
			}
		}
	}

	bool HasCapability(User* user)
	{
		LocalUser* localuser = IS_LOCAL(user);
		return localuser && typing_cap.IsEnabled(localuser);
	}
};

MODULE_INIT(ModuleIRCv3Typing)
