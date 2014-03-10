/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
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

#include "inspircd.h"

/* $ModDesc: Provides the CHANGECAP command that allows a channel op to change the capitalization of the channel name */
/* $ModAuthor: Daniel Vassdal */
/* $ModAuthorMail: shutter@canternet.org */
/* $ModDepends: core 2.0 */

/** Handle /CHANGECAP
 */

class ChangeCap : public Command
{
 public:
	ChangeCap(Module* Creator) : Command(Creator, "CHANGECAP", 1)
	{
		allow_empty_last_param = false;
		syntax = "<channel>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		Channel* chan = ServerInstance->FindChan(parameters[0]);

		if (!chan)
		{
			user->WriteServ("NOTICE %s :*** CHANGECAP The channel %s does not exist.", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (IS_LOCAL(user) != NULL)
		{
			if (chan->GetPrefixValue(user) < OP_VALUE)
			{
				user->WriteServ("NOTICE %s :*** CHANGECAP You need to be a channel operator of %s to change its name capitalization", user->nick.c_str(), chan->name.c_str());
				return CMD_FAILURE;
			}

			if (chan->name == parameters[0])
			{
				user->WriteServ("NOTICE %s :*** CHANGECAP The channel is already using this capitalization", user->nick.c_str());
				return CMD_FAILURE;
			}
			user->WriteServ("NOTICE %s :*** CHANGECAP Changed from %s to %s", user->nick.c_str(), chan->name.c_str(), parameters[0].c_str());
		}

		ServerInstance->chanlist->erase(chan->name);
		ServerInstance->chanlist->insert(std::make_pair(parameters[0], chan));
		chan->name = parameters[0];

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_OPT_BCAST;
	}
};

class ModuleChangeCap : public Module
{
	ChangeCap cmd;

 public:
	ModuleChangeCap() : cmd(this) {	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion()
	{
		return Version("Provides the CHANGECAP command that allows a channel op to change the capitalization of the channel name", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleChangeCap)
