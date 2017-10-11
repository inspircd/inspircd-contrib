/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 genius3000 <genius3000@g3k.solutions>
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
/* $ModDesc: Allows a connect class to match by ident. */
/* $ModDepends: core 2.0 */
/* $ModConfig: Within connect block: matchident="myIdent" */


#include "inspircd.h"

class ModuleConnMatchIdent : public Module
{
 public:
	void init()
	{
		ServerInstance->Modules->Attach(I_OnSetConnectClass, this);
	}

	void Prioritize()
	{
		/* Go after requireident, you better be using that with matching to ident */
		Module* requireident = ServerInstance->Modules->Find("m_ident.so");
		ServerInstance->Modules->SetPriority(this, I_OnSetConnectClass, PRIORITY_AFTER, requireident);
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* connclass)
	{
		const std::string matchident = connclass->config->getString("matchident");

		if (!matchident.empty() && !InspIRCd::Match(user->ident, matchident))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Allows a connect class to match by ident.");
	}
};

MODULE_INIT(ModuleConnMatchIdent)
