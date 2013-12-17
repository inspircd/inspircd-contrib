/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Peter Powell <petpow@saberuk.com>
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


#include "inspircd.h"

/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModDesc: Allows forcing idents on users based on their connect class. */
/* $ModDepends: core 2.0-2.1 */
/* $ModConfig: <connect forceident="example"> */

class ModuleForceIdent : public Module
{
	public:

		void init()
		{
			ServerInstance->Modules->Attach(I_OnUserConnect, this);
		}

		void Prioritize()
		{
			ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_LAST);
		}

		void OnUserConnect(LocalUser* user)
		{
			ConfigTag* tag = user->MyClass->config;
			user->ident = tag->getString("forceident", user->ident);
			user->InvalidateCache();
		}

		Version GetVersion()
		{
			return Version("Allows forcing idents on users based on their connect class.");
		}
};

MODULE_INIT(ModuleForceIdent)
