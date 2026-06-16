/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 reverse <mike.chevronnet@gmail.com>
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

/// $ModAuthor: reverse <mike.chevronnet@gmail.com>
/// $ModConfig: <redaction window="0" retention="1h" minrank="20000" operoverride="yes" maxtracked="200000">
/// $ModDepends: core 4
/// $ModDesc: Provides the DRAFT IRCv3 draft/message-redaction extension (REDACT command).

// draft/message-redaction lets a client delete one of its previously sent messages
// by msgid: REDACT <target> <msgid> [<reason>]. Once the redactor is authorised the
// server relays ":<source> REDACT <target> <msgid> [<reason>]" to recipients that
// negotiated the cap, so their clients can hide or log the deletion.
//
// Who may redact (all under <redaction>):
//   - the original sender (matched by uuid or services account), within <window>
//     seconds of sending it (0 = no limit);
//   - a channel moderator with rank >= minrank (default halfop);
//   - an oper holding channels/redact, when operoverride is enabled.
//
// Ownership is checked against a msgid -> {sender, target, time} map of recently
// seen PRIVMSG/NOTICE/TAGMSG, pruned after <retention> and capped at <maxtracked>.
// msgids and uuids are network-global, so this works across a server link.

#include "inspircd.h"
#include "modules/account.h"
#include "modules/cap.h"
#include "modules/ctctags.h"
#include "modules/ircv3_replies.h"

class ModuleRedaction;

namespace
{
	// What we remember about a recent message so we can authorise a REDACT.
	struct TrackedMsg final
	{
		std::string owneruuid;     // global uuid of the sender
		std::string owneraccount;  // services account at send time (may be empty)
		std::string target;        // channel name or PM peer nick (for scoping)
		time_t when = 0;
	};
}

class CommandRedact final
	: public Command
{
private:
	ModuleRedaction& parent;

public:
	CommandRedact(Module* mod, ModuleRedaction& p);
	CmdResult Handle(User* user, const Params& parameters) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;
};

class ModuleRedaction final
	: public Module
	, public CTCTags::EventListener
{
public:
	Cap::Capability cap;
	Cap::Reference msgtagcap;
	Account::API accountapi;
	IRCv3::Replies::Fail fail;
	ClientProtocol::EventProvider redactprov;
	CommandRedact cmd;

	std::unordered_map<std::string, TrackedMsg> tracked;       // msgid -> info
	std::deque<std::pair<time_t, std::string>> prunequeue;     // (when, msgid) FIFO for pruning

	// config
	time_t window = 0;                 // self-redaction time limit (0 = unlimited)
	time_t retention = 3600;           // how long a msgid stays redactable
	unsigned long minrank = HALFOP_VALUE; // min channel rank for moderator redaction
	bool operoverride = true;          // opers with channels/redact can redact anything
	size_t maxtracked = 200000;        // hard cap on the tracking map

	ModuleRedaction()
		: Module(VF_VENDOR, "Provides the DRAFT IRCv3 draft/message-redaction extension (REDACT command).")
		, CTCTags::EventListener(this)
		, cap(this, "draft/message-redaction")
		, msgtagcap(this, "message-tags")
		, accountapi(this)
		, fail(this)
		, redactprov(this, "REDACT")
		, cmd(this, *this)
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("redaction");
		window = tag->getDuration("window", 0, 0);
		retention = tag->getDuration("retention", 3600, 60);
		minrank = tag->getNum<unsigned long>("minrank", HALFOP_VALUE, 0);
		operoverride = tag->getBool("operoverride", true);
		maxtracked = tag->getNum<size_t>("maxtracked", 200000, 1000);
	}

	std::string AccountOf(User* user) const
	{
		const std::string* acct = accountapi ? accountapi->GetAccountName(user) : nullptr;
		return acct ? *acct : "";
	}

	// Remember a freshly sent message keyed by its msgid (set by m_ircv3_msgid).
	void Track(User* user, const MessageTarget& target, const ClientProtocol::TagMap& tags)
	{
		std::string tname;
		if (target.type == MessageTarget::TYPE_CHANNEL)
			tname = target.Get<Channel>()->name;
		else if (target.type == MessageTarget::TYPE_USER)
			tname = target.Get<User>()->nick;
		else
			return; // not a redactable target

		auto it = tags.find("msgid");
		if (it == tags.end() || it->second.value.empty())
			return; // no msgid (m_ircv3_msgid not loaded) -> nothing to anchor a REDACT to

		const std::string& msgid = it->second.value;
		auto res = tracked.emplace(msgid, TrackedMsg{ user->uuid, AccountOf(user), tname, ServerInstance->Time() });
		if (!res.second)
			return; // already tracked (same msgid delivered again) -> keep the first

		prunequeue.emplace_back(res.first->second.when, msgid);

		// Bound memory: drop the oldest entries if we're over the cap.
		while (tracked.size() > maxtracked && !prunequeue.empty())
		{
			tracked.erase(prunequeue.front().second);
			prunequeue.pop_front();
		}
	}

	void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details) override
	{
		Track(user, target, details.tags_out);
	}

	void OnUserPostTagMessage(User* user, const MessageTarget& target, const CTCTags::TagMessageDetails& details) override
	{
		Track(user, target, details.tags_out);
	}

	void OnBackgroundTimer(time_t now) override
	{
		const time_t cutoff = now - retention;
		while (!prunequeue.empty() && prunequeue.front().first < cutoff)
		{
			tracked.erase(prunequeue.front().second);
			prunequeue.pop_front();
		}
	}

	// Authorise a LOCAL redactor. On denial, sends the spec FAIL and returns false.
	bool Authorise(LocalUser* user, Channel* chan, const std::string& targetname, const std::string& msgid)
	{
		// Oper override.
		if (operoverride && user->HasPrivPermission("channels/redact"))
			return true;

		// Channel moderator (rank >= minrank).
		if (chan)
		{
			Membership* memb = chan->GetUser(user);
			if (memb && chan->GetPrefixValue(user) >= minrank)
				return true;
		}

		// Otherwise: must be the original author of a still-tracked message.
		auto it = tracked.find(msgid);
		if (it == tracked.end())
		{
			fail.SendIfCap(user, &cap, &cmd, "UNKNOWN_MSGID", targetname, msgid,
				"This message does not exist or is too old.");
			return false;
		}

		const TrackedMsg& tm = it->second;
		const bool owner = (tm.owneruuid == user->uuid)
			|| (!tm.owneraccount.empty() && tm.owneraccount == AccountOf(user));
		if (!owner || !irc::equals(tm.target, targetname))
		{
			fail.SendIfCap(user, &cap, &cmd, "REDACT_FORBIDDEN", targetname, msgid,
				"You are not authorised to delete this message.");
			return false;
		}

		if (window && (ServerInstance->Time() - tm.when) > window)
		{
			fail.SendIfCap(user, &cap, &cmd, "REDACT_WINDOW_EXPIRED", targetname, msgid, ConvToStr(window),
				"You can no longer delete this message.");
			return false;
		}

		return true;
	}

	void SendRedact(LocalUser* lu, User* source, const std::string& targetname,
		const std::string& msgid, const std::string& reason)
	{
		if (!cap.IsEnabled(lu) || !msgtagcap.IsEnabled(lu))
			return; // MUST NOT forward REDACT to clients without the capability.

		ClientProtocol::Message msg("REDACT", source);
		msg.PushParam(targetname);
		msg.PushParam(msgid);
		if (!reason.empty())
			msg.PushParam(reason);
		ClientProtocol::Event ev(redactprov, msg);
		lu->Send(ev);
	}

	// Deliver the relayed REDACT to local recipients. Runs on every server the
	// command reaches; cross-server fan-out is handled by command routing.
	void Relay(User* source, Channel* chan, User* tuser, const std::string& targetname,
		const std::string& msgid, const std::string& reason)
	{
		if (chan)
		{
			for (const auto& [u, memb] : chan->GetUsers())
			{
				if (LocalUser* lu = IS_LOCAL(u))
					SendRedact(lu, source, targetname, msgid, reason);
			}
		}
		else if (tuser)
		{
			if (LocalUser* lu = IS_LOCAL(tuser))
				SendRedact(lu, source, targetname, msgid, reason);
			// Echo to the redactor so their own client hides the message.
			if (LocalUser* slu = IS_LOCAL(source))
				SendRedact(slu, source, targetname, msgid, reason);
		}

		tracked.erase(msgid); // it's gone now
	}
};

CommandRedact::CommandRedact(Module* mod, ModuleRedaction& p)
	: Command(mod, "REDACT", 2)
	, parent(p)
{
	syntax = { "<target> <msgid> [<reason>]" };
	penalty = 2000;
}

RouteDescriptor CommandRedact::GetRouting(User*, const Params& parameters)
{
	const std::string& t = parameters[0];
	if (!t.empty() && ServerInstance->Channels.IsPrefix(t[0]))
		return ROUTE_BROADCAST; // every server delivers to its local channel members
	if (User* tu = ServerInstance->Users.FindNick(t, true))
		return ROUTE_OPT_UCAST(tu->server); // only the target's server needs it
	return ROUTE_LOCALONLY;
}

CmdResult CommandRedact::Handle(User* user, const Params& parameters)
{
	const std::string& targetname = parameters[0];
	const std::string& msgid = parameters[1];
	const std::string reason = parameters.size() > 2 ? parameters[2] : "";

	Channel* chan = nullptr;
	User* tuser = nullptr;
	if (!targetname.empty() && ServerInstance->Channels.IsPrefix(targetname[0]))
		chan = ServerInstance->Channels.Find(targetname);
	else
		tuser = ServerInstance->Users.FindNick(targetname, true);

	// The origin server (where the redactor is local) validates; other servers
	// just deliver the already-authorised REDACT to their local recipients.
	if (LocalUser* lu = IS_LOCAL(user))
	{
		if (!chan && !tuser)
		{
			parent.fail.SendIfCap(lu, &parent.cap, this, "INVALID_TARGET", targetname,
				"You cannot delete messages from " + targetname + ".");
			return CmdResult::FAILURE;
		}
		if (!parent.Authorise(lu, chan, targetname, msgid))
			return CmdResult::FAILURE; // Authorise() sent the FAIL
	}
	else if (!chan && !tuser)
	{
		return CmdResult::FAILURE; // unknown target on this server
	}

	parent.Relay(user, chan, tuser, targetname, msgid, reason);
	return CmdResult::SUCCESS;
}

MODULE_INIT(ModuleRedaction)
