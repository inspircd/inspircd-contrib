/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014-2016 Sadie Powell <sadie@witchery.services>
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
/// $ModDepends: core 3
/// $ModDesc: Stores the ident given in USER as metadata.


#include "inspircd.h"
#include "extension.h"

class ModuleIdentMeta final
	: public Module
{
private:
	StringExtItem ext;

public:
	ModuleIdentMeta()
		: Module(VF_NONE, "Stores the ident given in USER as metadata.")
		, ext(this, "user-ident", ExtensionType::USER, true)
	{
	}

	void OnChangeIdent(User* user, const std::string& ident) override
	{
		if (IS_LOCAL(user) && !ext.Get(user))
		{
			ServerInstance->Logs.Debug(MODNAME, "Setting ident metadata of {} to {}.",
				user->nick, ident);
			ext.Set(user, ident);
		}
	}
};

MODULE_INIT(ModuleIdentMeta)
