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


/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModAuthorMail: petpow@saberuk.com */
/* $ModDesc: Adds a channel mode which limits the length of messages. */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class MessageLengthMode : public ModeHandler
{
 public:
	LocalIntExt msglength;

	MessageLengthMode(Module* Creator)
		: ModeHandler(Creator, "message-length", 'W', PARAM_SETONLY, MODETYPE_CHANNEL)
		, msglength("message-length", Creator)
	{
	}

	ModeAction OnModeChange(User*, User*, Channel* channel, std::string& parameter, bool adding)
	{
		if (adding == channel->IsModeSet(this))
			return MODEACTION_DENY;

		if (adding)
		{
			long length = ConvToInt(parameter);
			if (length < 1 || length > MAXBUF)
				return MODEACTION_DENY;

			channel->SetModeParam(this, parameter);
			this->msglength.set(channel, length);
			return MODEACTION_ALLOW;
		}

		channel->SetModeParam(this, "");
		this->msglength.set(channel, 0);
		return MODEACTION_ALLOW;
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

	void init()
	{
		Implementation eventList[] = { I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventList, this, sizeof(eventList)/sizeof(Implementation));
		ServerInstance->Modules->AddService(mode);
	}

	ModResult OnUserPreMessage(User*, void* dest, int target_type, std::string& text, char, CUList&)
	{
		if (target_type != TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* channel = static_cast<Channel*>(dest);
		if (!channel->IsModeSet(&mode))
			return MOD_RES_PASSTHRU;

		int msglength = mode.msglength.get(channel);
		if (text.length() > msglength)
			text.resize(msglength);

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNotice(User* source, void* dest, int target_type, std::string& text, char status, CUList& exempt_list)
	{
		return this->OnUserPreMessage(source, dest, target_type, text, status, exempt_list);
	}

	Version GetVersion()
	{
		return Version("Adds a channel mode which limits the length of messages.", VF_COMMON);
	}
};

MODULE_INIT(ModuleMessageLength)

