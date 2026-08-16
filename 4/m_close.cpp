/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 TheUnstoppable <theunstoppable1000@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Carsten Valdemar Munk <carsten.munk+inspircd@gmail.com>
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
/// $ModAuthorMail: noreply@inspircd.org
/// $ModDepends: core 4
/// $ModDesc: Provides the ability to close unregistered connections.


#include "inspircd.h"

class CommandClose : public Command
{
 public:
	CommandClose(Module* Creator)
		: Command(Creator,"CLOSE")
	{
		access_needed = CmdAccess::OPERATOR;
	}

	CmdResult Handle(User* src, const Params& parameters) override
	{
		std::map<std::string,int> closed;

		const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
		for (UserManager::LocalList::const_iterator u = list.begin(); u != list.end(); )
		{
			// Quitting the user removes it from the list
			LocalUser* user = *u;
			++u;
			if (!user->IsFullyConnected())
			{
				ServerInstance->Users.QuitUser(user, "Closing all unknown connections per request");
				std::string key = ConvToStr(user->GetAddress())+"."+ConvToStr(user->server_sa.port());
				closed[key]++;
			}
		}

		int total = 0;
		for (std::map<std::string,int>::iterator ci = closed.begin(); ci != closed.end(); ci++)
		{
			src->WriteNotice("*** Closed " + ConvToStr(ci->second) + " unknown " + (ci->second == 1 ? "connection" : "connections") +
				" from [" + ci->first + "]");
			total += ci->second;
		}
		if (total)
			src->WriteNotice("*** " + ConvToStr(total) + " unknown " + (total == 1 ? "connection" : "connections") + " closed");
		else
			src->WriteNotice("*** No unknown connections found");

		return CmdResult::SUCCESS;
	}
};

class ModuleClose : public Module
{
	CommandClose cmd;
 public:
	ModuleClose() : Module(VF_NONE, "Provides the ability to close unregistered connections."),
		cmd(this)
	{
	}
};

MODULE_INIT(ModuleClose)
