/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Jansteen <pliantcom@yandex.com>
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

/* $ModDesc: Provides support for blocking DCC transfers */
/* $ModAuthor: Jansteen */
/* $ModAuthorMail: pliantcom@yandex.com */
/* $ModConfig: <dccblock users="true" channels="false"> */

/* Documentation
   This module is used to completely block DCC from being used on your
   IRC network in all cases.  Previous work-arounds to accomplish this
   included configuring m_dccallow to block DCC by default and adding an
   extra clause to disable DCCALLOW for non-operators, but there are
   some cases where it is best to make sure DCC is off for everyone,
   operators included.  This module does this simply and without extra
   configuration required.

   Note: You should not load m_dccallow.so simultaneously to m_dccblock.so
   because it will do nothing useful.  It wouldn't prevent this module from
   blocking DCC however.
*/

class ModuleDCCBlock : public Module
{
	bool users, channels;

 public:
	ModuleDCCBlock() : Module()
		, users(true)
		, channels(false)
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		OnRehash(NULL);
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("dccblock");

		users = tag->getBool("users", true);
		channels = tag->getBool("channels");
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list)
	{
		return OnUserPreNotice(user, dest, target_type, text, status, exempt_list);
	}

	ModResult OnUserPreNotice(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		if (strncmp(text.c_str(), "\1DCC ", 5) == 0)
		{
			if (target_type == TYPE_USER && !users)
				return MOD_RES_PASSTHRU;

			if (target_type == TYPE_CHANNEL && !channels)
				return MOD_RES_PASSTHRU;

			// This is a DCC request and we want to block it
			user->WriteNumeric(998, "%s :DCC not allowed on this server.  No exceptions allowed.", user->nick.c_str());
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Provides support for blocking DCC transfers");
	}
};

MODULE_INIT(ModuleDCCBlock)
