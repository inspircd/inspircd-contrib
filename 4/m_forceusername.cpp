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
/// $ModConfig: <connect forceident="example">
/// $ModDepends: core 4
/// $ModDesc: Allows forcing idents on users based on their connect class.
/// $ModConfig: <connect forceusername="example">
/// $ModDepends: core 4
/// $ModDesc: Allows forcing usernames on users based on their connect class.


#include "inspircd.h"

class ModuleForceUser final
	: public Module
{
public:
	ModuleForceUser()
		: Module(VF_NONE, "Allows forcing usernames on users based on their connect class.")
	{
	}

	void OnUserConnect(LocalUser* user) override
	{
		const auto& tag = user->GetClass()->config;
		const std::string username = tag->getString("forceusername", tag->getString("forceident"));
		if (ServerInstance->IsUser(username))
		{
			ServerInstance->Logs.Debug(MODNAME, "Setting username of user '{}' ({}) in class '{}' to '{}'.",
				user->nick, user->uuid, user->GetClass()->name, username);

			user->ChangeDisplayedUser(username);
		}
	}
};

MODULE_INIT(ModuleForceUser)
