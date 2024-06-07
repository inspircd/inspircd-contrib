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


/// $ModAuthor: Sadie Powell <sadie@witchery.services>
/// $ModConfig: <antisnoop exemptrank="10000">
/// $ModDesc: Adds a channel mode which limits the ability of snoopers.
/// $ModDepends: core 4

#include "inspircd.h"
#include "extension.h"
#include "numerichelper.h"
#include "timeutils.h"

class AntiSnoopMode final
	: public ParamMode<AntiSnoopMode, IntExtItem>
{
public:
	AntiSnoopMode(Module* Creator)
		: ParamMode<AntiSnoopMode, IntExtItem>(Creator, "antisnoop", 'W')
	{
		syntax = "<idletime>";
	}

	bool OnSet(User* source, Channel* channel, std::string& parameter) override
	{
		unsigned long idletime;
		if (!Duration::TryFrom(parameter, idletime) || !idletime)
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return false;
		}

		this->ext.Set(channel, idletime);
		return true;
	}

	void SerializeParam(Channel* channel, unsigned long idletime, std::string& out)
	{
		out += Duration::ToString(idletime);
	}
};

class ModuleAntiSnoop final
	: public Module
{
private:
	IntExtItem lastmsg;
	AntiSnoopMode mode;
	unsigned long exemptrank;

public:
	ModuleAntiSnoop()
		: Module(VF_COMMON, "Adds a channel mode which limits the ability of snoopers.")
		, lastmsg(this, "antisnoop-lastmsg", ExtensionType::MEMBERSHIP)
		, mode(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("antisnoop");
		exemptrank = tag->getNum<ModeHandler::Rank>("exemptrank", VOICE_VALUE);
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		if (target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU; // We only care about channel messages.

		auto* channel = target.Get<Channel>();
		Membership* memb = channel->GetUser(user);
		if (memb)
		{
			// Update idle times for the source. We do this even if the mode is
			// not set so if the mode is activated we can automatically work.
			lastmsg.Set(memb, ServerInstance->Time());
		}

		if (!channel->IsModeSet(mode))
			return MOD_RES_PASSTHRU; // The mode isn't set .

		time_t maxidle = ServerInstance->Time() - mode.ext.Get(channel);
		for (const auto& [targuser, targmemb] : channel->GetUsers())
		{
			if (exemptrank && targmemb->GetRank() >= exemptrank)
				continue;

			if (lastmsg.Get(targmemb) < maxidle)
				details.exemptions.insert(targuser);
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleAntiSnoop)

