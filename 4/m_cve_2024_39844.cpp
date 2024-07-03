/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Sadie Powell <sadie@witchery.services>
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
/// $ModDesc: Prevents clients from sending messages that trigger CVE-2024-39844.
/// $ModDepends: core 3

#include "inspircd.h"

class ModuleCVE202439844 final
	: public Module
{
public:
	ModuleCVE202439844()
		: Module(VF_NONE, "Prevents clients from sending messages that trigger CVE-2024-39844.")
	{
	}

	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& except_list) override
	{
		// HACK HACK HACK
		std::string& mutreason = const_cast<std::string&>(reason);
		std::replace(mutreason.begin(), mutreason.end(), '}', ' ');
	}
};

MODULE_INIT(ModuleCVE202439844)

