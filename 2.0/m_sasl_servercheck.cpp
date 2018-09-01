/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
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

/* $ModAuthor: genius3000 */
/* $ModAuthorMail: genius3000@g3k.solutions */
/* $ModDesc: Fail SASL auth if the defined target is unavailable. */
/* $ModDepends: core 2.0 */
/* $ModConfig: <saslservercheck reason="SASL is currently unavailable."> */

#include "inspircd.h"
#include "account.h"


class ModuleSaslServerCheck : public Module
{
	std::string reason;
	std::string target;

 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User*)
	{
		reason = ServerInstance->Config->ConfValue("saslservercheck")->getString("reason", "SASL is currently unavailable.");
		target = ServerInstance->Config->ConfValue("sasl")->getString("target");

		if (target.empty() || target == "*")
			throw ModuleException("This module is useless without setting the <sasl target=\"services.mynetwork.com\"> value");
	}

	ModResult OnPreCommand(std::string& command, std::vector<std::string>&, LocalUser* user, bool validated, const std::string&)
	{
		if (!validated || command != "AUTHENTICATE" || (target.empty() || target == "*"))
			return MOD_RES_PASSTHRU;

		ProtoServerList servers;
		ServerInstance->PI->GetServerList(servers);
		if (servers.empty())
			return MOD_RES_PASSTHRU;

		for (ProtoServerList::const_iterator i = servers.begin(); i != servers.end(); ++i)
		{
			if (i->servername == target)
				return MOD_RES_PASSTHRU;
		}

		// Target server not found, return SASL Fail and deny the command
		user->WriteNumeric(904, "%s :SASL authentication failed: %s", user->nick.c_str(), reason.c_str());
		return MOD_RES_DENY;

	}

	Version GetVersion()
	{
		return Version("Fail SASL auth if the defined target is unavailable.");
	}
};

MODULE_INIT(ModuleSaslServerCheck)
