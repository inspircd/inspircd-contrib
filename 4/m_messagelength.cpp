/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
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
/// $ModDesc: Adds a channel mode which limits the length of messages.
/// $ModDepends: core 3

#include "inspircd.h"
#include "extension.h"
#include "numerichelper.h"

class MessageLengthMode final
	: public ParamMode<MessageLengthMode, IntExtItem>
{
public:
	MessageLengthMode(Module* Creator)
		: ParamMode<MessageLengthMode, IntExtItem>(Creator, "message-length", 'W')
	{
		syntax = { "<max-length>" };
	}

	bool OnSet(User* source, Channel* channel, std::string& parameter)
	{
		size_t length = ConvToNum<size_t>(parameter);
		if (length == 0 || length > ServerInstance->Config->Limits.MaxLine)
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return false;
		}

		this->ext.Set(channel, length);
		return true;
	}

	void SerializeParam(Channel* channel, size_t n, std::string& out)
	{
		out += ConvToStr(n);
	}
};

class ModuleMessageLength final
	: public Module
{
private:
	MessageLengthMode mode;

public:
	ModuleMessageLength()
		: Module(VF_COMMON, "Adds a channel mode which limits the length of messages.")
		, mode(this)
	{
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) override
	{
		if (target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* channel = target.Get<Channel>();
		if (!channel->IsModeSet(&mode))
			return MOD_RES_PASSTHRU;

		unsigned int msglength = mode.ext.Get(channel);
		if (details.text.length() > msglength)
			details.text.resize(msglength);

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleMessageLength)

