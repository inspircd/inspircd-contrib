/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2019 Matt Schatz <genius3000@g3k.solutions>
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <connect vhost="vhost.here">
/// $ModDepends: core 3
/// $ModDesc: Sets a connect block configured vhost on users when they connect
// The use of '$ident' will be replaced with the user's ident
// The use of '$account' will be replaced with the user's account name


#include "inspircd.h"
#include "modules/account.h"

class ModuleVhostOnConnect : public Module
{
	std::bitset<UCHAR_MAX> hostmap;

	const std::string GetAccount(LocalUser* user)
	{
		std::string result;

		const AccountExtItem* accountext = GetAccountExtItem();
		const std::string* account = accountext ? accountext->get(user) : NULL;
		if (account)
			result = *account;

		return result;
	}

 public:
	void Prioritize() CXX11_OVERRIDE
	{
		// Let's go after conn_umodes in case +x also gets set
		Module* connumodes = ServerInstance->Modules->Find("m_conn_umodes.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_AFTER, connumodes);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		// From m_sethost: use the same configured host character map if it exists
		std::string hmap = ServerInstance->Config->ConfValue("hostname")->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789");

		hostmap.reset();
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			hostmap.set(static_cast<unsigned char>(*n));
	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
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

			stdalgo::string::replace_all(vhost, replace, ident);
		}

		replace = "$account";
		if (vhost.find(replace) != std::string::npos)
		{
			std::string account = GetAccount(user);
			if (account.empty())
				account = "unidentified";

			stdalgo::string::replace_all(vhost, replace, account);
		}

		if (vhost.length() > ServerInstance->Config->Limits.MaxHost)
		{
			ServerInstance->Logs->Log("m_conn_vhost", LOG_DEFAULT, "m_conn_vhost: vhost in connect block %s is too long", user->MyClass->name.c_str());
			return;
		}

		// From m_sethost: validate the characters
		for (std::string::const_iterator x = vhost.begin(); x != vhost.end(); x++)
		{
			if (!hostmap.test(static_cast<unsigned char>(*x)))
			{
				ServerInstance->Logs->Log("m_conn_vhost", LOG_DEFAULT, "m_conn_vhost: vhost in connect block %s has invalid characters", user->MyClass->name.c_str());
				return;
			}
		}

		user->ChangeDisplayedHost(vhost.c_str());
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Sets a connect block configured vhost on users when they connect");
	}
};

MODULE_INIT(ModuleVhostOnConnect)
