/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Colgate Minuette <rabbit@minuette.net>
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


/* $ModAuthor: Colgate Minuette */
/* $ModDesc: Allows setting hosts on users based on their account name on login. */
/* $ModDepends: core 2.0 */
/* $ModConfig: <accounthost prefix="foo" suffix="bar" changereal="false"> */

#include "inspircd.h"
#include "account.h"

class ModuleAccountHost : public Module
{
 private:
	std::string hostprefix;
	std::string hostsuffix;
	bool changereal;

 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnEvent, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnEvent(Event& event)
	{
		if (event.id != "account_login")
			return;
		AccountEvent* accev = (AccountEvent*)&event;
		if (!IS_LOCAL(accev->user))
			return;
		std::string hostname = hostprefix + accev->account + hostsuffix;
		if (changereal && accev->user->registered != REG_ALL)
			accev->user->host = hostname;
		accev->user->ChangeDisplayedHost(hostname.c_str());
		accev->user->InvalidateCache();
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("accounthost");
		this->hostprefix = tag->getString("prefix");
		this->hostsuffix = tag->getString("suffix");
		this->changereal = tag->getBool("changereal");
	}

	Version GetVersion()
	{
		return Version("Allows setting hosts on users based on their account name on login.");
	}
};

MODULE_INIT(ModuleAccountHost)

