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
	bool sending = false; // set while re-sending a substituted AWAY, so it can't re-enter OnUserWrite

public:
	ModuleIRCv3PreAway()
		: Module(VF_NONE, "Provides the DRAFT draft/pre-away IRCv3 client capability.")
		, cap(this, "draft/pre-away")
		, awayprov(this, "AWAY")
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		// An away message can't be empty, so reject empty/over-length/"*" values
		// and fall back to the default — substitute is then always usable.
		substitute = ServerInstance->Config->ConfValue("preaway")->getString("substitute", "Away", [](const auto& str) {
			return !str.empty() && str.length() <= ServerInstance->Config->Limits.MaxAway && !irc::equals(str, "*");
		});
	}

	ModResult OnUserWrite(LocalUser* user, ClientProtocol::Message& msg) override
	{
		// draft/pre-away lets a client mark itself away without a message via
		// "AWAY *". Clients that negotiated the cap understand the "*" and keep
		// it; clients without it get the configured message substituted in. The
		// guard stops the substituted AWAY we send below from re-entering here.
		if (sending || cap.IsEnabled(user))
			return MOD_RES_PASSTHRU;

		// Cheap prefilter — only AWAY and the RPL_AWAY (301) numeric are relevant.
		const char* cmd = msg.GetCommand();
		if (cmd[0] != 'A' && cmd[0] != '3')
			return MOD_RES_PASSTHRU;

		const std::string command = cmd;
		const bool isaway = command == "AWAY";
		if (!isaway && command != "301")
			return MOD_RES_PASSTHRU;

		const auto& params = msg.GetParams();
		if (params.empty())
			return MOD_RES_PASSTHRU;

		const std::string& awaymsg = params.back();
		if (awaymsg != "*")
			return MOD_RES_PASSTHRU;

		// RPL_AWAY (301) is a numeric built for this one user, so its parameter
		// can be rewritten in place.
		if (!isaway)
		{
			msg.ReplaceParam(params.size() - 1, substitute);
			return MOD_RES_PASSTHRU;
		}

		// An away-notify AWAY is a single message broadcast to every channel
		// member; mutating it would change it for cap clients too. Send this user
		// their own substituted copy and drop the original.
		User* source = msg.GetSourceUser();
		if (!source)
			return MOD_RES_PASSTHRU;

		ClientProtocol::Message rewritten("AWAY", source);
		rewritten.PushParam(substitute);
		sending = true;
		user->Send(awayprov, rewritten);
		sending = false;
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleIRCv3PreAway)
