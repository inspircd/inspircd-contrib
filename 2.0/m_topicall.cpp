/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

/* $ModAuthor: Attila Molnar */
/* $ModAuthorMail: attilamolnar@hush.com */
/* $ModDesc: Provides the /TOPICALL command that changes the topic on all channels */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class CommandTopicAll : public SplitCommand
{
 public:
	CommandTopicAll(Module* mod)
		: SplitCommand(mod, "TOPICALL", 1, 1)
	{
		allow_empty_last_param = false;
		flags_needed = 'o';
		syntax = "<topic>";
	}

	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user)
	{
		std::string newtopic = parameters[0];
		const chan_hash& chans = *ServerInstance->chanlist;
		for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); ++i)
		{
			Channel* chan = i->second;
			if (chan->topic != newtopic)
				chan->SetTopic(user, newtopic, true);
		}

		return CMD_SUCCESS;
	}
};

class ModuleTopicAll : public Module
{
	CommandTopicAll cmd;

 public:
	ModuleTopicAll()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion()
	{
		return Version("Provides the /TOPICALL command that changes the topic on all channels");
	}
};

MODULE_INIT(ModuleTopicAll)
