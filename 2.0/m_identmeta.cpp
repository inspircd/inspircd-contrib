/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Sadie Powell <sadie@witchery.services>
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
/* $ModDesc: Stores the ident given in USER as metadata. */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleIdentMeta : public Module
{
 private:
	StringExtItem ext;

 public:
	ModuleIdentMeta()
		: ext("user-ident", this) { }

	void init()
	{
		ServerInstance->Modules->Attach(I_OnChangeIdent, this);
	}

	void OnChangeIdent(User* user, const std::string& ident)
	{
		if (IS_LOCAL(user) && ext.get(user) == NULL)
		{
			ServerInstance->Logs->Log("m_identmeta", DEBUG, "Setting ident metadata of %s to %s.",
				user->nick.c_str(), ident.c_str());
			ext.set(user, ident);
		}
	}

	Version GetVersion()
	{
		return Version("Stores the ident given in USER as metadata.");
	}
};

MODULE_INIT(ModuleIdentMeta)
