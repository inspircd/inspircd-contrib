/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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
/// $ModDesc: Adds support for Discord-style #1234 nick tags.


#include "inspircd.h"
#include "extension.h"

namespace
{
	std::function<bool(const std::string_view&)> origisnick;

	bool IsDiscordNick(const std::string_view& nick)
	{
		if (nick.empty() || nick.length() > ServerInstance->Config->Limits.MaxNick)
			return false;

		size_t hashpos = nick.find('#');
		if (hashpos == std::string::npos)
			return false;

		for (size_t pos = hashpos + 1; pos < nick.length(); ++pos)
			if (!isdigit(nick[pos]))
				return false;

		std::string_view basenick = nick;
		basenick.remove_suffix(nick.length() - hashpos - 1);
		return origisnick(basenick);
	}
}

class ModuleDiscordNick final
	: public Module
{
private:
	IntExtItem ext;

public:
	ModuleDiscordNick()
		: Module(VF_COMMON, "Adds support for Discord-style #1234 nick tags.")
		, ext(this, "nicktag", ExtensionType::USER)
	{
		origisnick = ServerInstance->IsNick;
		ServerInstance->IsNick = &IsDiscordNick;
	}

	~ModuleDiscordNick() override
	{
		ServerInstance->IsNick = origisnick;
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		if (validated && command == "NICK" && parameters[0] != "0")
			parameters[0].append(INSP_FORMAT("{:04}", ext.Get(user)));
		return MOD_RES_PASSTHRU;
	}

	void OnUserPostInit(LocalUser* user) override
	{
		ext.Set(user, ServerInstance->GenRandomInt(9999));
	}
};

MODULE_INIT(ModuleDiscordNick)
