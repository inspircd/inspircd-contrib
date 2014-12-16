/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010-2012 Attila Molnar <attilamolnar@hush.com>
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
/* $ModDesc: Provides the +a usermode that hides idle and signon time in WHOIS from non-opers */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class HideIdleMode : public SimpleUserModeHandler
{
 public:
	HideIdleMode(Module* Creator) : SimpleUserModeHandler(Creator, "hideidle", 'a')
	{
	}
};

class ModuleHideIdle : public Module
{
	HideIdleMode hideidle;

 public:
	ModuleHideIdle()
		:  hideidle(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(hideidle);
		ServerInstance->Modules->Attach(I_OnWhoisLine, this);
	}

	ModResult OnWhoisLine(User* user, User* dest, int& numeric, std::string& text)
	{
		if ((numeric == 317) && (dest->IsModeSet('a')) && (!user->HasPrivPermission("users/auspex")) && (user != dest))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Provides the +a usermode that hides idle and signon time in WHOIS from non-opers", VF_NONE);
	}
};

MODULE_INIT(ModuleHideIdle)
