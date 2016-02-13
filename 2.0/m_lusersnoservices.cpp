/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008-2016 Naram Qashat <cyberbotx@cyberbotx.com>
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

#include "inspircd.h"

/* $ModDesc: Adds an extra line to LUSERS output to show global user count minus "clients" from U-Lines servers */
/* $ModAuthor: Naram Qashat (CyberBotX) */
/* $ModAuthorMail: cyberbotx@cyberbotx.com */
/* $ModDepends: core 2.0 */

class ModuleLusersNoServices : public Module
{
	int clientsNoServices;

public:
	void init()
	{
		Implementation implementations[] = { I_OnNumeric, I_OnPostConnect, I_OnUserQuit };
		ServerInstance->Modules->Attach(implementations, this, sizeof(implementations) / sizeof(implementations[0]));

		/* Calculate how many clients are not psuedo-clients introduced by the Services package */
		this->clientsNoServices = 0;
		user_hash::const_iterator curr = ServerInstance->Users->clientlist->begin(), end = ServerInstance->Users->clientlist->end();
		for (; curr != end; ++curr)
			if (!ServerInstance->ULine(curr->second->server) && !ServerInstance->ULine(curr->second->nick))
				++this->clientsNoServices;
	}

	Version GetVersion()
	{
		return Version("Adds an extra line to LUSERS output to show global user count minus \"clients\" from U-Lines servers");
	}

	ModResult OnNumeric(User *user, unsigned numeric, const std::string &)
	{
		if (numeric == 266)
			user->WriteNumeric(267, "%s :Current Global Users (Excluding Services): %d", user->nick.c_str(), this->clientsNoServices);
		return MOD_RES_PASSTHRU;
	}

	void OnPostConnect(User *user)
	{
		if (!ServerInstance->ULine(user->server) && !ServerInstance->ULine(user->nick))
			++this->clientsNoServices;
	}

	void OnUserQuit(User *user, const std::string &, const std::string &)
	{
		if (!ServerInstance->ULine(user->server) && !ServerInstance->ULine(user->nick))
			--this->clientsNoServices;
	}
};

MODULE_INIT(ModuleLusersNoServices)
