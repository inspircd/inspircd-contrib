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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 4
/// $ModDesc: Stores the username given in USER as metadata.


#include "inspircd.h"
#include "extension.h"

class ModuleUsernameMeta final
	: public Module
{
private:
	StringExtItem ext;

public:
	ModuleUsernameMeta()
		: Module(VF_NONE, "Stores the username given in USER as metadata.")
		, ext(this, "user-username", ExtensionType::USER, true)
	{
	}

	void OnChangeUser(User* user, const std::string& newuser) override
	{
		if (IS_LOCAL(user) && !ext.Get(user))
		{
			ServerInstance->Logs.Debug(MODNAME, "Setting username metadata of {} to {}.", user->nick, newuser);
			ext.Set(user, newuser);
		}
	}
};

MODULE_INIT(ModuleUsernameMeta)
