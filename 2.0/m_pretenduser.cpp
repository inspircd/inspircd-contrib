/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Chernov-Phoenix Alexey <phoenix@pravmail.ru>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
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
/* $ModDesc: Provides the PRETENDUSER command that lets opers execute commands on behalf of other users */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class CommandPretendUser : public Command
{
	bool active;
 public:
	CommandPretendUser(Module* mod)
		: Command(mod, "PRETENDUSER", 2, 2)
		, active(false)
	{
#if INSPIRCD_VERSION_API >= 3
		allow_empty_last_param = false;
#endif
		flags_needed = 'o';
		syntax = "<nick> <a line>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		// stop PRETENDUSER <nick> PRETENDUSER <nick> PRETENDUSER <nick> ...
		if (active)
		{
			ServerInstance->Logs->Log("m_pretenduser", DEBUG, "Refused to execute PRETENDUSER because we're in the middle of processing the previous one");
			return CMD_FAILURE;
		}

		User* u = ServerInstance->FindNick(parameters[0]);
		if ((!u) || (IS_SERVER(u)))
		{
			user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (user == u)
		{
			user->WriteServ("NOTICE %s :*** Cannot target yourself", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (IS_OPER(u) && IS_OPER(user))
		{
			unsigned int dest_level = ConvToInt(u->oper->getConfig("level"));
			unsigned int source_level = ConvToInt(user->oper->getConfig("level"));
			if (dest_level > source_level)
			{
				user->WriteServ("NOTICE %s :*** Cannot target IRC operator with higher level than yourself", user->nick.c_str());
				return CMD_FAILURE;
			}
		}

		if (IS_LOCAL(u))
		{
			if (!ServerInstance->ULine(user->server))
			{
				ServerInstance->SNO->WriteGlobalSno('a', "%s used PRETENDUSER to send '%s' from %s", user->nick.c_str(), parameters[1].c_str(), u->nick.c_str());
			}

			std::string line = parameters[1];
			active = true;
			ServerInstance->Parser->ProcessBuffer(line, IS_LOCAL(u));
			active = false;
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};

class ModulePretendUser : public Module
{
	CommandPretendUser cmd;
 public:
	ModulePretendUser()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion()
	{
		return Version("Provides the PRETENDUSER command that lets opers execute commands on behalf of other users", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModulePretendUser)
