/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 reverse <mike.chevronnet@gmail.com>
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

/// $ModAuthor: reverse <mike.chevronnet@gmail.com>
/// $ModConfig: <preaway substitute="Away">
/// $ModDepends: core 4
/// $ModDesc: Provides the DRAFT draft/pre-away IRCv3 client capability.

#include "inspircd.h"
#include "modules/cap.h"

class ModuleIRCv3PreAway final
	: public Module
{
private:
	Cap::Capability cap;
	ClientProtocol::EventProvider awayprov;
	std::string substitute;

public:
	ModuleIRCv3PreAway()
		: Module(VF_VENDOR, "Provides the DRAFT draft/pre-away IRCv3 client capability.")
		, cap(this, "draft/pre-away")
		, awayprov(this, "AWAY")
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		substitute = ServerInstance->Config->ConfValue("preaway")->getString("substitute", "Away", [](const auto& str) {
			return !str.empty() && str.length() <= ServerInstance->Config->Limits.MaxAway && !irc::equals(str, "*");
		});
	}

	ModResult OnUserWrite(LocalUser* user, ClientProtocol::Message& msg) override
	{
		if (substitute.empty() || cap.IsEnabled(user))
			return MOD_RES_PASSTHRU;

		const char* cmd = msg.GetCommand();
		if (cmd[0] != 'A' && cmd[0] != '3')
			return MOD_RES_PASSTHRU;

		const auto& params = msg.GetParams();
		if (params.empty())
			return MOD_RES_PASSTHRU;

		const std::string& awaymsg = params.back();
		if (awaymsg != "*")
			return MOD_RES_PASSTHRU;

		const std::string command = cmd;
		if (command == "301")
		{
			msg.ReplaceParam(params.size() - 1, substitute);
			return MOD_RES_PASSTHRU;
		}

		if (command == "AWAY")
		{
			User* source = msg.GetSourceUser();
			if (!source)
				return MOD_RES_PASSTHRU;

			ClientProtocol::Message rewritten("AWAY", source);
			rewritten.PushParam(substitute);
			user->Send(awayprov, rewritten);
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleIRCv3PreAway)
