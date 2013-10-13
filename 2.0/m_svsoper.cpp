/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Peter Powell <petpow@saberuk.com>
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

/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModDesc: Allows services to forcibly oper a user. */
/* $ModDepends: core 2.0 */

class CommandSVSOper : public Command
{
public:
	
	CommandSVSOper(Module* Creator)
		: Command(Creator, "SVSOPER", 2, 2)
	{
		flags_needed = 'o';
	}
	
	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		if (IS_LOCAL(user) || !ServerInstance->ULine(user->server))
			return CMD_EPERM;

		User* target = ServerInstance->FindUUID(parameters[0]);
		if (!target)
			return CMD_FAILURE;

		// I hope whoever came up with the idea to store types like this dies in a fire.
		OperIndex::iterator iter = ServerInstance->Config->oper_blocks.find(" " + parameters[1]);
		if (iter == ServerInstance->Config->oper_blocks.end())
			return CMD_FAILURE;

		target->Oper(iter->second);
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class ModuleSVSOper : public Module
{
private:	
	CommandSVSOper command;

public:
	ModuleSVSOper()
		: command(this) { }
	
	void init()
	{
		ServerInstance->Modules->AddService(command);
	}

	Version GetVersion()
	{
		return Version("Allows services to forcibly oper a user.", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleSVSOper)
