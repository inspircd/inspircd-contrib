/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2016-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
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

/// $ModAuthor: InspIRCd Developers
/// $ModDepends: core 4
/// $ModDesc: Adds the /LOCKSERV and /UNLOCKSERV commands which allows server operators to control whether users can connect to the local server.
/// $ModLink: https://docs.inspircd.org/4/moved-modules/#lockserv


#include "inspircd.h"

/** Adds numerics
 * 988 <nick> <servername> :Closed for new connections
 * 989 <nick> <servername> :Open for new connections
 */
enum
{
	// InspIRCd-specific.
	RPL_SERVLOCKON = 988,
	RPL_SERVLOCKOFF = 989
};

class CommandLockserv final
	: public Command
{
	std::string& locked;

 public:
	CommandLockserv(Module* Creator, std::string& lock)
		: Command(Creator, "LOCKSERV", 0, 1)
		, locked(lock)
	{
		allow_empty_last_param = false;
		access_needed = CmdAccess::OPERATOR;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (!locked.empty())
		{
			user->WriteNotice("The server is already locked.");
			return CmdResult::FAILURE;
		}

		locked = parameters.empty() ? "Server is temporarily closed. Please try again later." : parameters[0];
		user->WriteNumeric(RPL_SERVLOCKON, user->server->GetName(), "Closed for new connections");
		ServerInstance->SNO.WriteGlobalSno('a', "Oper {} used LOCKSERV to temporarily disallow new connections", user->nick);
		return CmdResult::SUCCESS;
	}
};

class CommandUnlockserv final
	: public Command
{
	std::string& locked;

 public:
	CommandUnlockserv(Module* Creator, std::string& lock)
		: Command(Creator, "UNLOCKSERV")
		, locked(lock)
	{
		access_needed = CmdAccess::OPERATOR;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (locked.empty())
		{
			user->WriteNotice("The server isn't locked.");
			return CmdResult::FAILURE;
		}

		locked.clear();
		user->WriteNumeric(RPL_SERVLOCKOFF, user->server->GetName(), "Open for new connections");
		ServerInstance->SNO.WriteGlobalSno('a', "Oper {} used UNLOCKSERV to allow new connections", user->nick);
		return CmdResult::SUCCESS;
	}
};

class ModuleLockserv final
	: public Module
{
	std::string locked;
	CommandLockserv lockcommand;
	CommandUnlockserv unlockcommand;

 public:
	ModuleLockserv()
		: Module(VF_NONE, "Adds the /LOCKSERV and /UNLOCKSERV commands which allows server operators to control whether users can connect to the local server.")
		, lockcommand(this, locked)
		, unlockcommand(this, locked)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		// Emergency way to unlock
		if (!status.srcuser)
			locked.clear();
	}

	void OnModuleRehash(User* user, const std::string& param) override
	{
		if (irc::equals(param, "lockserv") && !locked.empty())
			locked.clear();
	}

	ModResult OnUserRegister(LocalUser* user) override
	{
		if (!locked.empty())
		{
			ServerInstance->Users.QuitUser(user, locked);
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		return !locked.empty() ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleLockserv)
