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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 3
/// $ModDesc: Implements support for the freenode identify-msg extension.


#include "inspircd.h"
#include "modules/account.h"
#include "modules/cap.h"

class ModuleIdentifyMsg : public Module
{
 private:
	Cap::Capability cap;

 public:
	ModuleIdentifyMsg()
		: cap(this, "identify-msg")
	{
	}

	ModResult OnUserWrite(LocalUser* user, ClientProtocol::Message& msg) CXX11_OVERRIDE
	{
		if (!msg.GetSourceUser() || msg.GetParams().size() < 2 || !cap.get(user))
			return MOD_RES_PASSTHRU;

		if (!irc::equals(msg.GetCommand(), "NOTICE") && !irc::equals(msg.GetCommand(), "PRIVMSG"))
			return MOD_RES_PASSTHRU;

		const AccountExtItem* accountext = GetAccountExtItem();
		const std::string* account = accountext ? accountext->get(msg.GetSourceUser()) : NULL;
		const bool identified_to_nick = account && irc::equals(*account, msg.GetSourceUser()->nick);

		std::string newparam = msg.GetParams()[1];
		newparam.insert(0, 1, identified_to_nick ? '+' : '-');
		msg.ReplaceParam(1, newparam);

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements support for the freenode identify-msg extension.");
	}
};

MODULE_INIT(ModuleIdentifyMsg)
