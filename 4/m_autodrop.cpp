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
/// $ModConfig: <autodrop commands="CONNECT DELETE GET HEAD OPTIONS PATCH POST PUT TRACE">
/// $ModDepends: core 4
/// $ModDesc: Allows clients to be automatically dropped if they execute certain commands before registration.


#include "inspircd.h"

class ModuleAutoDrop final
	: public Module
{
private:
	std::vector<std::string> commands;
	std::string message;

public:
	ModuleAutoDrop()
		: Module(VF_NONE, "Allows clients to be automatically dropped if they execute certain commands before registration.")
	{
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnPreCommand, PRIORITY_FIRST);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("autodrop");

		commands.clear();
		irc::spacesepstream commandstream(tag->getString("commands", "CONNECT DELETE GET HEAD OPTIONS PATCH POST PUT TRACE", 1));
		for (std::string command; commandstream.GetToken(command); )
			commands.push_back(command);

		if (!tag->readString("message", message, true))
			message.clear();
	}

	ModResult OnPreCommand(std::string& command, Command::Params& parameters, LocalUser* user, bool validated) override
	{
		if (user->IsFullyConnected() || !stdalgo::isin(commands, command))
			return MOD_RES_PASSTHRU;

		if (!message.empty())
		{
			user->eh.AddWriteBuf(message);
			user->eh.DoWrite();
		}
		user->eh.SetError("Dropped by " MODNAME);
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleAutoDrop)
