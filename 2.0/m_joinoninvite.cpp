/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Lukasz "JustArchi" Domeradzki <JustArchi@JustArchi.net>
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

/*
 * Should be used with caution, as it allows potential abuse by users
 * Perfect when INVITE command is banned and reserved only to net admins
 * Simple, yet powerful
 */

/* $ModAuthor: Lukasz "JustArchi" Domeradzki */
/* $ModAuthorMail: JustArchi@JustArchi.net */
/* $ModDesc: Forces user to join the channel on invite */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleJoinOnInvite : public Module
{
 public:
	void init()
	{
		ServerInstance->Modules->Attach(I_OnUserInvite, this);
	}

	Version GetVersion()
	{
		return Version("Forces user to join the channel on invite");
	}

	void OnUserInvite(User* source, User* dest, Channel* channel, time_t timeout)
	{
		LocalUser* localuser = IS_LOCAL(dest);
		if (!localuser)
			return;

		Channel::JoinUser(localuser, channel->name.c_str(), true, "", false, ServerInstance->Time());
	}
};

MODULE_INIT(ModuleJoinOnInvite)
