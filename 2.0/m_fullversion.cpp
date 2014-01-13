/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
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
/* $ModDesc: Displays the full version of a server to an oper */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class CommandFullVersion : public Command
{
 public:
	CommandFullVersion(Module* mod)
		: Command(mod, "FULLVERSION", 0, 1)
	{
		flags_needed = 'o';
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		if (!parameters.empty() && parameters[0] != ServerInstance->Config->ServerName)
			return CMD_SUCCESS;

		std::string fullversion = ServerInstance->GetVersionString(true);
		user->SendText(":%s 351 %s :%s", ServerInstance->Config->ServerName.c_str(), user->nick.c_str(), fullversion.c_str());
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (!parameters.empty())
			return ROUTE_OPT_UCAST(parameters[0]);
		return ROUTE_LOCALONLY;
	}
};

class ModuleFullVersion : public Module
{
	CommandFullVersion cmd;

 public:
	ModuleFullVersion()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion()
	{
		return Version("Displays the full version of a server to an oper", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleFullVersion)
