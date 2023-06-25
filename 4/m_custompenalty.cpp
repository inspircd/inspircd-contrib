/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2020 Sadie Powell <sadie@witchery.services>
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
/// $ModConfig: <penalty name="INVITE" value="60">
/// $ModDepends: core 3
/// $ModDesc: Allows the customisation of penalty levels.


#include "inspircd.h"

class ModuleCustomPenalty final
	: public Module
{
private:
	static void SetPenalties()
	{
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("penalty"))
		{
			const std::string name = tag->getString("name");
			Command* command = ServerInstance->Parser.GetHandler(name);
			if (!command)
			{
				ServerInstance->Logs.Normal(MODNAME, "Warning: unable to find command: " + name);
				continue;
			}

			unsigned int penalty = tag->getNum<unsigned int>("value", 1, 1, UINT_MAX);
			ServerInstance->Logs.Debug(MODNAME, "Setting the penalty for {} to {}", command->name, penalty);
			command->penalty = penalty;
		}
	}

public:
	ModuleCustomPenalty()
		: Module(VF_NONE, "Allows the customisation of penalty levels.")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		SetPenalties();
	}

	void OnLoadModule(Module* mod) override
	{
		SetPenalties();
	}
};

MODULE_INIT(ModuleCustomPenalty)
