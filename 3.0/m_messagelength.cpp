/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Peter Powell <petpow@saberuk.com>
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


/// $ModAuthor: Peter "SaberUK" Powell
/// $ModAuthorMail: petpow@saberuk.com
/// $ModDesc: Adds a channel mode which limits the length of messages.
/// $ModDepends: core 3.0

#include "inspircd.h"

class MessageLengthMode : public ParamMode<MessageLengthMode, LocalIntExt>
{
 public:
	MessageLengthMode(Module* Creator)
		: ParamMode<MessageLengthMode, LocalIntExt>(Creator, "message-length", 'W')
	{
	}

	ModeAction OnSet(User*, Channel* channel, std::string& parameter)
	{
		long length = ConvToInt(parameter);
		if (length < 1 || length > ServerInstance->Config->Limits.MaxLine)
			return MODEACTION_DENY;

		this->ext.set(channel, length);
		return MODEACTION_ALLOW;
	}

	void SerializeParam(Channel* channel, int n, std::string& out)
	{
		out += ConvToStr(n);
	}
};

class ModuleMessageLength : public Module
{
 private:
	MessageLengthMode mode;

 public:
	ModuleMessageLength()
		: mode(this)
	{
	}

	ModResult OnUserPreMessage(User*, void* dest, int target_type, std::string& text, char, CUList&, MessageType)
	{
		if (target_type != TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* channel = static_cast<Channel*>(dest);
		if (!channel->IsModeSet(&mode))
			return MOD_RES_PASSTHRU;

		int msglength = mode.ext.get(channel);
		if (text.length() > msglength)
			text.resize(msglength);

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Adds a channel mode which limits the length of messages.", VF_COMMON);
	}
};

MODULE_INIT(ModuleMessageLength)

