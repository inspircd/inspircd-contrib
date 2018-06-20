/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Peter Powell <petpow@saberuk.com>
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


/* $ModAuthor: Manuel "satmd" Leiner */
/* $ModAuthorMail: satmd@satmd.de */
/* $ModDesc: Prevent users to change nick when banned on a channel they're in. */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleBanNonick : public Module
{

 public:

	void init()
	{
		ServerInstance->Modules->Attach(I_OnUserPreNick, this);
	}

	ModResult OnUserPreNick(User* olduser, const std::string& newnick)
	{
		LocalUser* user = IS_LOCAL(olduser);
		if (!user)
			return MOD_RES_PASSTHRU;

		UserChanList cl(user->chans);
		for(UCListIter ci=cl.begin();ci != cl.end(); ++ci) {
			Channel* channel = *ci;
			if(channel->GetPrefixValue(user) < VOICE_VALUE && channel->IsBanned(olduser))
					return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Prevent users to change nick when banned on a channel they're in.", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleBanNonick)
