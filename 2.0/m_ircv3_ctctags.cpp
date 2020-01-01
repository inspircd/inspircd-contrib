/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
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


/* $ModAuthor: Sadie Powell */
/* $ModDesc: Dummy module to allow usage of the ircv3_ctctags module on 3.0 servers. */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class CommandTagMsg : public Command
{
 public:
	CommandTagMsg(Module* Creator)
		: Command(Creator, "TAGMSG")
	{
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		// This intentionally does nothing.
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_LOCALONLY;
	}
};

class ModuleTagMsg : public Module
{
	CommandTagMsg command;

 public:
	ModuleTagMsg()
		: command(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(command);
	}

	Version GetVersion()
	{
		return Version("Dummy module to allow usage of the ircv3_ctctags module on 3.0 servers.", VF_COMMON);
	}
};

MODULE_INIT(ModuleTagMsg)
