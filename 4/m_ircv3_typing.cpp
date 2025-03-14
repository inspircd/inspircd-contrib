/*
 *  InspIRCd -- Internet Relay Chat Daemon
 *
 *  This file is part of InspIRCd.  InspIRCd is free software: you can
 *  redistribute it and/or modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either version 2, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 */

/// $ModAuthor: revrsefr <mike.chevronnet@gmail.com>
/// $ModDesc: Implements IRCv3 +draft/typing client capability (always active)
/// $ModDepends: core 4
/// $ModConfig: <typingnotify interval="3" timeout="6" pausethreshold="3">

#include "inspircd.h"
#include "modules/cap.h"
#include "modules/ctctags.h"
#include "modules/ircv3.h"
#include "clientprotocol.h"
#include "message.h"


// Status constants for typing notifications
enum class TypingState
{
	ACTIVE,  // User is actively typing
	PAUSED,  // User has paused typing
	DONE     // User has finished typing
};

// Typing state information for a user in a particular destination
struct TypingInfo
{
	TypingState state;
	time_t last_update;

	TypingInfo()
		: state(TypingState::DONE)
		, last_update(0)
	{
	}
};

// Typing maps: keyed by user->uuid, then by target name
using TargetMap     = std::map<std::string, TypingInfo>;
using UserTypingMap = std::map<std::string, TargetMap>;

class ModuleIRCv3Typing;

class TypingTag final
	: public IRCv3::CapTag<TypingTag>
{
private:
	UserTypingMap* typing_users;

public:
	TypingTag(Module* mod, UserTypingMap* users_map)
		: IRCv3::CapTag<TypingTag>(mod, "+draft/typing", "+draft/typing")
		, typing_users(users_map)
	{
	}

	const std::string* GetValue(ClientProtocol::Message&)
	{
		// We don't add this tag automatically to outbound messages here.
		return nullptr;
	}
};

class ModuleIRCv3Typing final
	: public Module
	, public CTCTags::EventListener
{
private:
	TypingTag typing_tag;
	UserTypingMap typing_users;
	unsigned int typing_interval;
	unsigned int typing_timeout;
	unsigned int pause_threshold;
	ClientProtocol::EventProvider tagmsg_event;

public:
	ModuleIRCv3Typing()
		: Module(VF_VENDOR, "Implements IRCv3 +draft/typing client capability.")
		, CTCTags::EventListener(this)
		, typing_tag(this, &typing_users)
		, typing_interval(3)
		, typing_timeout(6)
		, pause_threshold(3)
		, tagmsg_event(this, "TAGMSG")
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		auto tag = ServerInstance->Config->ConfValue("typingnotify");
		typing_interval = tag->getDuration("interval", 3, 1, 60);
		typing_timeout  = tag->getDuration("timeout", 6, 1, 300);
		pause_threshold = tag->getDuration("pausethreshold", 3, 1, 60);
	}

	void init() override
	{
	}

    ModResult OnUserPreTagMessage(User* user, MessageTarget& target, CTCTags::TagMessageDetails& details) override
    {
        return MOD_RES_PASSTHRU;
    }

	// Handle CTCP TYPING for older clients
	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		if (!typing_tag.GetCap().IsEnabled(user))
			return MOD_RES_PASSTHRU;

		std::string targetname;
		if (target.type == MessageTarget::TYPE_USER)
			targetname = target.Get<User>()->nick;
		else if (target.type == MessageTarget::TYPE_CHANNEL)
			targetname = target.Get<Channel>()->name;
		else
			return MOD_RES_PASSTHRU;

		UpdateTypingState(user, targetname, TypingState::ACTIVE);
		return MOD_RES_PASSTHRU;
	}

	// Mark as done typing after sending normal text
	void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails&) override
	{
		std::string targetname;
		if (target.type == MessageTarget::TYPE_USER)
			targetname = target.Get<User>()->nick;
		else if (target.type == MessageTarget::TYPE_CHANNEL)
			targetname = target.Get<Channel>()->name;
		else
			return;

		UpdateTypingState(user, targetname, TypingState::DONE);
	}

	// Remove stale entries and handle transitions from ACTIVE to PAUSED
	void OnBackgroundTimer(time_t now) override
	{
		for (auto it_user = typing_users.begin(); it_user != typing_users.end(); )
		{
			auto& target_map = it_user->second;

			for (auto it_target = target_map.begin(); it_target != target_map.end(); )
			{
				auto& info = it_target->second;
				
				// Check for transition from ACTIVE to PAUSED
				if (info.state == TypingState::ACTIVE && (now - info.last_update > pause_threshold))
				{
					// User hasn't typed for a while, transition to PAUSED
					User* user = ServerInstance->Users.FindUUID(it_user->first);
					if (user)
					{
						// Update state to PAUSED
						info.state = TypingState::PAUSED;
						// Update timestamp to track paused state timeout
						info.last_update = now;
						// Broadcast the update
						BroadcastTypingUpdate(user, it_target->first, TypingState::PAUSED);
					}
					++it_target;
				}
				// Remove entries that have been inactive for too long
				else if ((now - info.last_update > typing_timeout))
				{
					if (info.state != TypingState::DONE)
					{
						// Auto-mark as DONE after timeout
						User* user = ServerInstance->Users.FindUUID(it_user->first);
						if (user)
						{
							BroadcastTypingUpdate(user, it_target->first, TypingState::DONE);
						}
					}
					it_target = target_map.erase(it_target);
				}
				else
				{
					++it_target;
				}
			}

			if (target_map.empty())
				it_user = typing_users.erase(it_user);
			else
				++it_user;
		}
	}

	void OnUserQuit(User* user, const std::string&, const std::string&) override
	{
		typing_users.erase(user->uuid);
	}

	void OnChannelDelete(Channel* chan) override
	{
		for (auto& [uuid, target_map] : typing_users)
			target_map.erase(chan->name);
	}

	void UpdateTypingState(User* user, const std::string& target, TypingState state)
	{
		const time_t now = ServerInstance->Time();
		TargetMap& targets = typing_users[user->uuid];

		auto it = targets.find(target);
		if (it != targets.end())
		{
			// If the state is the same and it's too soon, skip
			if ((state == it->second.state) &&
			    ((now - it->second.last_update) < typing_interval) &&
			    (state != TypingState::DONE))
			{
				return;
			}

			// If new state is DONE, remove it
			if (state == TypingState::DONE)
			{
				targets.erase(it);
				if (targets.empty())
					typing_users.erase(user->uuid);

				BroadcastTypingUpdate(user, target, state);
				return;
			}
		}

		// Insert/update record if not done
		if (state != TypingState::DONE)
		{
			TypingInfo& info = targets[target];
			info.state = state;
			info.last_update = now;
			BroadcastTypingUpdate(user, target, state);
		}
	}

	void BroadcastTypingUpdate(User* user, const std::string& target, TypingState state)
	{
		const std::string status =
			(state == TypingState::ACTIVE) ? "active" :
			(state == TypingState::PAUSED) ? "paused" : 
			"done"; 

        ClientProtocol::TagMap tags;
        tags.emplace("+draft/typing", ClientProtocol::MessageTagData(nullptr, status));

		// If it's a channel, broadcast
		if (!target.empty() && target[0] == '#')
		{
			Channel* chan = ServerInstance->Channels.Find(target);
			if (!chan)
				return;

			CTCTags::TagMessage tagmsg(user, chan, tags);
			ClientProtocol::Event ev(tagmsg_event, tagmsg);
			IRCv3::WriteNeighborsWithCap(user, ev, typing_tag.GetCap(), false);

			// Provide a fallback CTCP notice for older clients
			std::string typing_msg = "\001TYPING " + status + "\001";
			for (const auto& [muser, _] : chan->GetUsers())
			{
				LocalUser* lu = IS_LOCAL(muser);
				if (!lu || lu == user)
					continue;
				lu->WriteNotice(typing_msg);
			}
		}
		else
		{
			// Target is a user
			User* dest = (target.size() == UIDGenerator::UUID_LENGTH)
				? ServerInstance->Users.FindUUID(target)
				: ServerInstance->Users.Find(target);

			if (!dest)
				return;

			LocalUser* lu = IS_LOCAL(dest);
			if (lu && typing_tag.GetCap().IsEnabled(lu))
			{
				CTCTags::TagMessage tagmsg(user, lu, tags);
				ClientProtocol::Event ev(tagmsg_event, tagmsg);
				lu->Send(ev);

				std::string typing_msg = "\001TYPING " + status + "\001";
				lu->WriteNotice(typing_msg);
			}
		}
	}
};

MODULE_INIT(ModuleIRCv3Typing)