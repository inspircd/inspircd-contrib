/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 genius3000 <genius3000@g3k.solutions>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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
/* $ModDesc: Provides usermode 'V' - block all INVITEs */
/* $ModDepends: core 2.0 */

/* Helpop Lines for the UMODES section
 * Find: '<helpop key="umodes" value="User Modes'
 * Place just before the 'W    Receives notif...' line
 V            Blocks all INVITEs from other users (requires
              blockinvite extras-module).
 */

#include "inspircd.h"


class ModuleBlockInvite : public Module
{
 private:
	SimpleUserModeHandler bi;

 public:
	ModuleBlockInvite()
		: bi(this, "blockinvite", 'V')
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(bi);
		ServerInstance->Modules->Attach(I_OnUserPreInvite, this);
	}

	ModResult OnUserPreInvite(User*, User* dest, Channel*, time_t)
	{
		if (!IS_LOCAL(dest) || !dest->IsModeSet(bi.GetModeChar()))
			return MOD_RES_PASSTHRU;

		return MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Provides usermode +" + ConvToStr(bi.GetModeChar()) + " to block all INVITEs", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleBlockInvite)
