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
/* $ModDesc: Don't op normal users when they create a new channel */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleNoOpOnCreate : public Module
{
 public:
	void init()
	{
		ServerInstance->Modules->Attach(I_OnUserPreJoin, this);
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string& privs, const std::string& keygiven)
	{
		if ((!chan) && (privs == "o") && (!IS_OPER(user)))
			privs.clear();
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Don't op normal users when they create a new channel");
	}
};

MODULE_INIT(ModuleNoOpOnCreate)
