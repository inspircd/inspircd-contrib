/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

/* $ModAuthor: Attila Molnar */
/* $ModAuthorMail: attilamolnar@hush.com */
/* $ModDesc: Replaces the nick!user@host in RPL_WELCOME (001) with the user's nick */
/* $ModDepends: core 2.0 */

/*
 * It was observed that certain (SOHO) routers rewrite their external IP address
 * in the payload of the TCP packet containing the 001 numeric, causing disconnection
 * if the lengths of the ASCII representations of the IP addresses differ.
 *
 * As a workaround, this module replaces the nick!user@host in the 001 numeric
 * with only the nick.
 *
 * Thanks to Glen Miner from Digital Extremes who discovered and reported this.
 */

#include "inspircd.h"

class ModuleNickIn001 : public Module
{
	bool active;

 public:
	ModuleNickIn001()
		: active(false)
	{
	}

	void init()
	{
		ServerInstance->Modules->Attach(I_OnNumeric, this);
	}

	ModResult OnNumeric(User* user, unsigned int numeric, const std::string& text)
	{
		// If active is true then we're sending our own RPL_WELCOME - don't act on that
		if (numeric != RPL_WELCOME || active)
			return MOD_RES_PASSTHRU;

		active = true;
		user->WriteNumeric(RPL_WELCOME, "%s :Welcome to the %s IRC Network %s", user->nick.c_str(), ServerInstance->Config->Network.c_str(), user->nick.c_str());
		active = false;
		// Hide the original 001 numeric
		return MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Replaces the nick!user@host in RPL_WELCOME (001) with the user's nick");
	}
};

MODULE_INIT(ModuleNickIn001)
