/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
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
/* $ModDesc: Enforces a delay between nick changes per user */
/* $ModDepends: core 2.0 */
/* $ModConfig: <nickdelay delay="10" operoverride="true" hint="true"> */

#include "inspircd.h"

class ModuleNickDelay : public Module
{
	LocalIntExt lastchanged;
	int delay;
	bool operoverride;
	bool hint;

 public:
	ModuleNickDelay()
		: lastchanged("nickdelay", this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(lastchanged);
		Implementation eventlist[] = { I_OnRehash, I_OnUserPostNick, I_OnUserPreNick };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	void OnUserPostNick(User* user, const std::string& oldnick)
	{
		// Ignore remote users and nick changes to uuid
		if ((IS_LOCAL(user)) && (user->nick != user->uuid))
			lastchanged.set(user, ServerInstance->Time());
	}

	ModResult OnUserPreNick(User* user, const std::string& newnick)
	{
		if ((!IS_LOCAL(user)) || (ServerInstance->NICKForced.get(user)) || ((IS_OPER(user)) && (operoverride)))
			return MOD_RES_PASSTHRU;

		time_t lastchange = lastchanged.get(user);
		time_t wait = lastchange + delay - ServerInstance->Time();
		if (wait > 0)
		{
			if (hint)
				user->WriteNumeric(ERR_CANTCHANGENICK, "%s :You cannot change your nickname (try again in %d second%s)", user->nick.c_str(), (int)wait, (wait == 1) ? "" : "s");
			else
				user->WriteNumeric(ERR_CANTCHANGENICK, "%s :You cannot change your nickname (try again later)", user->nick.c_str());

			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("nickdelay");
		delay = tag->getInt("delay", 10);
		operoverride = tag->getBool("operoverride", true);
		hint = tag->getBool("hint", true);

		if (delay < 1)
			delay = 10;
	}

	Version GetVersion()
	{
		return Version("Enforces a delay between nick changes per user");
	}
};

MODULE_INIT(ModuleNickDelay)
