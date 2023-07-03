/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2016 Sadie Powell <sadie@witchery.services>
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
/// $ModDepends: core 4
/// $ModDesc: Allows services to forcibly oper a user.


#include "inspircd.h"

class CommandSVSOper final
	: public Command
{
public:
	CommandSVSOper(Module* Creator)
		: Command(Creator, "SVSOPER", 2, 2)
	{
		access_needed = CmdAccess::SERVER;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (!user->server->IsService())
			return CmdResult::FAILURE;

		User* target = ServerInstance->Users.FindUUID(parameters[0]);
		if (!target)
			return CmdResult::FAILURE;

		if (IS_LOCAL(target))
		{
			auto iter = ServerInstance->Config->OperTypes.find(parameters[1]);
			if (iter == ServerInstance->Config->OperTypes.end())
				return CmdResult::FAILURE;

			auto account = std::make_shared<OperAccount>(MODNAME, iter->second, ServerInstance->Config->EmptyTag);
			target->OperLogin(account, false, true);
		}
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		User* target = ServerInstance->Users.FindUUID(parameters[0]);
		if (!target)
			return ROUTE_LOCALONLY;
		return ROUTE_OPT_UCAST(target->server);
	}
};

class ModuleSVSOper final
	: public Module
{
private:
	CommandSVSOper command;

public:
	ModuleSVSOper()
		: Module(VF_OPTCOMMON, "Allows services to forcibly oper a user.")
		, command(this)
	{
	}
};

MODULE_INIT(ModuleSVSOper)
