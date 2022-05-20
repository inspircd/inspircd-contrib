/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Sadie Powell <sadie@witchery.services>
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


/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <antisnoop exemptrank="10000" modechar="W">
/// $ModDesc: Adds a channel mode which limits the ability of snoopers.
/// $ModDepends: core 3

#include "inspircd.h"

class AntiSnoopMode CXX11_FINAL
	: public ParamMode<AntiSnoopMode, LocalIntExt>
{
 public:
	AntiSnoopMode(Module* Creator)
		: ParamMode<AntiSnoopMode, LocalIntExt>(Creator, "antisnoop", ServerInstance->Config->ConfValue("antisnoop")->getString("modechar", "W", 1, 1)[0])
	{
		syntax = "<idletime>";
	}

	ModeAction OnSet(User* source, Channel* channel, std::string& parameter)
	{
		unsigned long idletime;
		if (!InspIRCd::Duration(parameter, idletime) || !idletime)
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return MODEACTION_DENY;
		}

		this->ext.set(channel, idletime);
		return MODEACTION_ALLOW;
	}

	void SerializeParam(Channel* channel, unsigned int idletime, std::string& out)
	{
		out += InspIRCd::DurationString(idletime);
	}
};

class ModuleAntiSnoop : public Module
{
 private:
	LocalIntExt lastmsg;
	AntiSnoopMode mode;
	unsigned long exemptrank;

 public:
	ModuleAntiSnoop()
		: lastmsg("antisnoop-lastmsg", ExtensionItem::EXT_MEMBERSHIP, this)
		, mode(this)
	{
	}

	void ReadConfig(ConfigStatus& status)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("antisnoop");
		exemptrank = tag->getUInt("exemptrank", VOICE_VALUE);
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if (target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU; // We only care about channel messages.

		Channel* channel = target.Get<Channel>();
		Membership* memb = channel->GetUser(user);
		if (memb)
		{
			// Update idle times for the source. We do this even if the mode is
			// not set so if the mode is activated we can automatically work.
			lastmsg.set(memb, ServerInstance->Time());
		}

		if (!channel->IsModeSet(mode))
			return MOD_RES_PASSTHRU; // The mode isn't set .

		time_t maxidle = ServerInstance->Time() - mode.ext.get(channel);
		const Channel::MemberMap& users = channel->GetUsers();
		for (Channel::MemberMap::const_iterator iter = users.begin(); iter != users.end(); ++iter)
		{
			Membership* targmemb = iter->second;
			if (exemptrank && targmemb->getRank() >= exemptrank)
				continue;

			if (lastmsg.get(targmemb) < maxidle)
				details.exemptions.insert(iter->first);
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds a channel mode which limits the ability of snoopers.", VF_COMMON);
	}
};

MODULE_INIT(ModuleAntiSnoop)

