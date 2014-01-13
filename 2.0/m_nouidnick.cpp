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
/* $ModDesc: Disallows changing nick to UID using /NICK */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleNoUIDNicks : public Module
{
 public:
	void init()
	{
		ServerInstance->Modules->Attach(I_OnUserPreNick, this);
	}

	ModResult OnUserPreNick(User* user, const std::string& newnick)
	{
		if ((!IS_LOCAL(user)) || (newnick[0] > '9') || (newnick[0] < '0'))
			return MOD_RES_PASSTHRU;

		if (ServerInstance->NICKForced.get(user))
			return MOD_RES_PASSTHRU;

		user->WriteNumeric(432, "%s 0 :Erroneous Nickname", user->nick.c_str());
		return MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Disallows changing nick to UID using /NICK");
	}
};

MODULE_INIT(ModuleNoUIDNicks)
