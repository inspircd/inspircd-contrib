/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018 genius3000 <genius3000@g3k.solutions>
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

/* $ModAuthor: genius3000 */
/* $ModAuthorMail: genius3000@g3k.solutions */
/* $ModDesc: Sets a connect block configured vhost on users when they connect */
/* $ModDepends: core 2.0 */
/* $ModConfig: Within connect block: vhost="vhost.here" */
/* The use of '$ident' will be replaced with the user's ident */
/* The use of '$account' will be replaced with the user's account name */

#include "inspircd.h"
#include "account.h"


class ModuleVhostOnConnect : public Module
{
	std::bitset<UCHAR_MAX> hostmap;

	const std::string GetAccount(LocalUser* user)
	{
		std::string result;

		const AccountExtItem* accountname = GetAccountExtItem();
		if (!accountname)
			return result;

		std::string* account = accountname->get(user);
		if (account)
			result = *account;

		return result;
	}

 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User*)
	{
		/* from m_sethost: use the same configured host character map if it exists */
		std::string hmap = ServerInstance->Config->ConfValue("hostname")->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789");

		hostmap.reset();
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			hostmap.set(static_cast<unsigned char>(*n));
	}

	void Prioritize()
	{
		/* Let's go after conn_umodes in case +x also gets set */
		Module* connumodes = ServerInstance->Modules->Find("m_conn_umodes.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_AFTER, connumodes);
	}

	void OnUserConnect(LocalUser* user)
	{
		ConfigTag* tag = user->MyClass->config;
		std::string vhost = tag->getString("vhost");
		std::string replace;

		if (vhost.empty())
			return;

		replace = "$ident";
		if (vhost.find(replace) != std::string::npos)
		{
			std::string ident = user->ident;
			if (ident[0] == '~')
				ident.erase(0, 1);

			SearchAndReplace(vhost, replace, ident);
		}

		replace = "$account";
		if (vhost.find(replace) != std::string::npos)
		{
			std::string account = GetAccount(user);
			if (account.empty())
				account = "unidentified";

			SearchAndReplace(vhost, replace, account);
		}

		if (vhost.length() > 64)
		{
			ServerInstance->Logs->Log("m_conn_vhost", DEFAULT, "m_conn_vhost: vhost in connect block %s is too long", user->MyClass->name.c_str());
			return;
		}

		/* from m_sethost: validate the characters */
		for (std::string::const_iterator x = vhost.begin(); x != vhost.end(); x++)
		{
			if (!hostmap.test(static_cast<unsigned char>(*x)))
			{
				ServerInstance->Logs->Log("m_conn_vhost", DEFAULT, "m_conn_vhost: vhost in connect block %s has invalid characters", user->MyClass->name.c_str());
				return;
			}
		}

		user->ChangeDisplayedHost(vhost.c_str());
	}

	Version GetVersion()
	{
		return Version("Sets a connect block configured vhost on users when they connect");
	}
};

MODULE_INIT(ModuleVhostOnConnect)
