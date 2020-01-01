/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Sadie Powell <sadie@witchery.services>
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


/* $ModAuthor: Sadie Powell */
/* $ModAuthorMail: sadie@witchery.services */
/* $ModDesc: Allows forcing idents on users based on their connect class. */
/* $ModDepends: core 2.0 */
/* $ModConfig: <connect forceident="example"> */

#include "inspircd.h"

class ModuleForceIdent : public Module
{
 public:
	void init()
	{
		ServerInstance->Modules->Attach(I_OnUserConnect, this);
	}

	void OnUserConnect(LocalUser* user)
	{
		ConfigTag* tag = user->MyClass->config;
		std::string ident = tag->getString("forceident");
		if (ServerInstance->IsIdent(ident.c_str()))
		{
			ServerInstance->Logs->Log("m_forceident", DEBUG, "Setting ident of user '%s' (%s) in class '%s' to '%s'.",
				user->nick.c_str(), user->uuid.c_str(), user->MyClass->name.c_str(), ident.c_str());
			user->ident = ident;
			user->InvalidateCache();
		}
	}

	Version GetVersion()
	{
		return Version("Allows forcing idents on users based on their connect class.");
	}
};

MODULE_INIT(ModuleForceIdent)
