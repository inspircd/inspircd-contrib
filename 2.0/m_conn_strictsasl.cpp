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
/* $ModDesc: Disconnect users that fail a SASL auth. */
/* $ModDepends: core 2.0 */
/* $ModConfig: <strictsasl reason="Fix your SASL authentication settings and try again."> */

#include "inspircd.h"
#include "account.h"


class ModuleConnStrictSasl : public Module
{
	LocalIntExt sentauth;
	std::string reason;

 public:
	ModuleConnStrictSasl()
		: sentauth("sentauth", this)
	{
	}

	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnPostCommand, I_OnCheckReady, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void Prioritize()
	{
		// m_cap will hold registration until 'CAP END', so SASL can try more than once
		Module* cap = ServerInstance->Modules->Find("m_cap.so");
		ServerInstance->Modules->SetPriority(this, I_OnCheckReady, PRIORITY_AFTER, cap);
	}

	void OnRehash(User*)
	{
		reason = ServerInstance->Config->ConfValue("strictsasl")->getString("reason", "Fix your SASL authentication settings and try again.");
	}

	void OnPostCommand(const std::string& command, const std::vector<std::string>&, LocalUser* user, CmdResult, const std::string&)
	{
		if (command == "AUTHENTICATE")
			sentauth.set(user, 1);
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		// Check that they have sent the AUTHENTICATE command
		if (!sentauth.get(user))
			return MOD_RES_PASSTHRU;

		const AccountExtItem* accountname = GetAccountExtItem();
		if (!accountname)
			return MOD_RES_PASSTHRU;

		// Let them through if they have an account
		std::string* account = accountname->get(user);
		if (account && !account->empty())
			return MOD_RES_PASSTHRU;

		ServerInstance->Logs->Log("m_conn_strictsasl", DEBUG, "Failed SASL auth from: %s (%s) [%s]",
			user->GetFullRealHost().c_str(), user->GetIPString(), user->fullname.c_str());
		ServerInstance->Users->QuitUser(user, reason);
		return MOD_RES_DENY;

	}

	void OnUserConnect(LocalUser* user)
	{
		if (sentauth.get(user))
			sentauth.set(user, 0);
	}

	Version GetVersion()
	{
		return Version("Disconnect users that fail a SASL auth.");
	}
};

MODULE_INIT(ModuleConnStrictSasl)
