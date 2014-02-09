/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Daniel Vassdal <shutter@canternet.org>
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

/* $ModDesc: Disable modes for users on specific connection blocks */

class ModuleDisableModes : public Module
{
	// This little hack is needed as we don't want to interfere with m_conn_umodes's business
	LocalIntExt ext;

	ModResult HandleBlocking(User* user, const std::string& modes, const char mode, const char* modetype)
	{
		if (modes.find_first_of(mode) == std::string::npos)
			return MOD_RES_PASSTHRU;

		user->WriteServ("NOTICE %s :*** %s %c has been blocked for your connect class", user->nick.c_str(), modetype, mode);
		return MOD_RES_DENY;
	}

 public:
	ModuleDisableModes() : ext("DISABLEMODES", this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(ext);

		Implementation eventlist[] = { I_OnUserConnect, I_OnRawMode };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist) / sizeof(Implementation));
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_AFTER, ServerInstance->Modules->Find("m_conn_umodes.so"));
		ServerInstance->Modules->SetPriority(this, I_OnRawMode, PRIORITY_FIRST);
	}

	Version GetVersion()
	{
		return Version("Disable modes for users on specific connection blocks");
	}

	void OnUserConnect(LocalUser* user)
	{
		ext.set(user, true);
	}

	ModResult OnRawMode(User* source, Channel* channel, const char mode, const std::string& parameter, bool adding, int pcnt)
	{
		LocalUser* user = NULL;
		if (!(user = IS_LOCAL(source)))
			return MOD_RES_PASSTHRU;

		if (!ext.get(user))
			return MOD_RES_PASSTHRU;

		ConfigTag* tag = user->MyClass->config;
		std::string modes;

		if (channel)
		{
			modes = tag->getString("disablecmodes");
			return HandleBlocking(user, modes, mode, "CMODE");
		}

		modes = tag->getString("disableumodes");
		return HandleBlocking(user, modes, mode, "UMODE");
	}
};

MODULE_INIT(ModuleDisableModes)
