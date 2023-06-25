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
/// $ModDepends: core 3
/// $ModDesc: Allows forcing idents on users based on their connect class.


#include "inspircd.h"

class ModuleForceIdent final
	: public Module
{
public:
	ModuleForceIdent()
		: Module(VF_NONE, "Allows forcing idents on users based on their connect class.")
	{
	}

	void OnUserConnect(LocalUser* user) override
	{
		const std::string ident = user->GetClass()->config->getString("forceident");
		if (ServerInstance->IsIdent(ident))
		{
			ServerInstance->Logs.Debug(MODNAME, "Setting ident of user '{}' ({}) in class '{}' to '{}'.",
				user->nick, user->uuid, user->GetClass()->name, ident);
			user->ident = ident;
			user->InvalidateCache();
		}
	}
};

MODULE_INIT(ModuleForceIdent)
