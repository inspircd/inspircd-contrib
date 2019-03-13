/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemonirc@gmail.com>
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

/// $ModAuthor: linuxdaemon
/// $ModAuthorMail: linuxdaemonirc@gmail.com
/// $ModDepends: core 3.0
/// $ModDesc: Slowly disconnects idle users for maintenance
/// $ModConfig: <shedusers shedopers="no" kill="yes" shutdown="no" blockconnect="yes" minidle="3600" maxusers="0" message="This server has entered maintenance mode." blockmessage="This server is in maintenance mode.">

// Maintenance mode can be triggered with the /SHEDUSERS command
// as well as sending SIGUSR2 to the inspircd process

#include "inspircd.h"
#include "exitcodes.h"

inline unsigned long GetIdle(LocalUser* lu)
{
	return ServerInstance->Time() - lu->idle_lastmsg;
}

static volatile sig_atomic_t active;
static volatile sig_atomic_t notified;

class CommandShed
	: public Command
{
	bool enable;
 public:
	CommandShed(Module* me, const std::string& Name, bool Enable)
		: Command(me, Name, 0, 1)
		, enable(Enable)
	{
		flags_needed = 'o';
		syntax = "[servermask]";
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (parameters.empty() || InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0]))
		{
			active = enable;
			notified = 0;
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const CommandBase::Params& parameters) CXX11_OVERRIDE
	{
		if (parameters.empty())
			return ROUTE_LOCALONLY;
		return ROUTE_OPT_BCAST;
	}
};

class ModuleShedUsers
	: public Module
{
 public:
	static void sighandler(int)
	{
		active = 1;
		notified = 0;
	}

 private:
	CommandShed startcmd, stopcmd;

	std::string message;
	std::string blockmessage;

	unsigned long maxusers;
	unsigned long minidle;

	bool shedopers;
	bool shutdown;
	bool blockconnects;
	bool kill;

 public:
	ModuleShedUsers()
		: startcmd(this, "SHEDUSERS", true)
		, stopcmd(this, "STOPSHED", false)
		, maxusers(0)
		, minidle(0)
		, shedopers(false)
		, shutdown(false)
		, blockconnects(false)
		, kill(false)
	{
	}

	void init() CXX11_OVERRIDE
	{
		notified = active = 0;
		signal(SIGUSR2, sighandler);
	}

	~ModuleShedUsers() CXX11_OVERRIDE
	{
		signal(SIGUSR2, SIG_DFL);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("shedusers");

		message = tag->getString("message", "This server has entered maintenance mode.");
		blockmessage = tag->getString("blockmessage", "This server is in maintenance mode.");
		maxusers = tag->getUInt("maxusers", 0);
		minidle = tag->getDuration("minidle", 60, 1);
		shedopers = tag->getBool("shedopers");
		shutdown = tag->getBool("shutdown");
		blockconnects = tag->getBool("blockconnect", true);
		kill = tag->getBool("kill", true);
	}

	bool CanShed(LocalUser* lu) const
	{
		if (!shedopers && lu->IsOper())
			return false;

		if (lu->registered != REG_ALL)
			return false;

		if (GetIdle(lu) < minidle)
			return false;

		return true;
	}

	void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE
	{
		if (active && blockconnects && user->registered != REG_ALL)
			ServerInstance->Users.QuitUser(user, blockmessage);
	}

	void OnBackgroundTimer(time_t) CXX11_OVERRIDE
	{
		if (!active)
			return;

		if (!notified)
		{
			ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, ServerInstance->Config->ServerName, message, MSG_NOTICE);
			ClientProtocol::Event msgevent(ServerInstance->GetRFCEvents().privmsg, msg);

			for (UserManager::LocalList::const_iterator i = ServerInstance->Users.GetLocalUsers().begin(); i != ServerInstance->Users.GetLocalUsers().end(); ++i)
			{
				LocalUser* user = *i;
				user->Send(msgevent);
			}
			notified = true;
		}

		if (ServerInstance->Users.LocalUserCount() <= maxusers)
		{
			if (shutdown)
				ServerInstance->Exit(EXIT_STATUS_NOERROR);

			active = 0;
			return;
		}

		if (!kill)
			return;

		LocalUser* to_quit = NULL;
		const UserManager::LocalList& localusers = ServerInstance->Users.GetLocalUsers();
		for (UserManager::LocalList::const_iterator it = localusers.begin(); it != localusers.end(); ++it)
		{
			LocalUser* lu = *it;
			if (CanShed(lu) && (!to_quit || lu->idle_lastmsg < to_quit->idle_lastmsg))
				to_quit = lu;
		}

		if (!to_quit)
			return;

		ServerInstance->Users.QuitUser(to_quit, message);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Slowly disconnects idle users for maintenance");
	}
};

MODULE_INIT(ModuleShedUsers)
