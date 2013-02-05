/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
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
/* $ModDesc: Displays a static text to every connecting user before registration */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleConnBanner : public Module
{
	std::string text;
 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnUserInit };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User* user)
	{
		text = ServerInstance->Config->ConfValue("connbanner")->getString("text");
	}

	void OnUserInit(LocalUser* user)
	{
		if (!text.empty())
			user->WriteServ("NOTICE Auth :*** " + text);
	}

	Version GetVersion()
	{
		return Version("Displays a static text to every connecting user before registration", VF_VENDOR);
	}
};

MODULE_INIT(ModuleConnBanner)
