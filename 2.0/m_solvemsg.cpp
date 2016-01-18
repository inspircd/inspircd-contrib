/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Peter Powell <petpow@saberuk.com>
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

/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModAuthorMail: petpow@saberuk.com */
/* $ModDesc: Requires users to solve a basic maths problem before messaging others. */
/* $ModDepends: core 2.0 */


#include "inspircd.h"

struct Problem
{
	int first;
	int second;
	bool warned;
};

class CommandSolve : public SplitCommand
{
 private:
	SimpleExtItem<Problem>& ext;

 public:
	CommandSolve(Module* Creator, SimpleExtItem<Problem>& Ext)
		: SplitCommand(Creator, "SOLVE", 1, 1)
		, ext(Ext)
	{
	}

	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user)
	{
		if (user->exempt)
		{
			user->WriteServ("NOTICE %s :*** You do not need to solve a problem!", user->nick.c_str());
			return CMD_FAILURE;
		}
		
		Problem* problem = ext.get(user);
		if (!problem)
		{
			user->WriteServ("NOTICE %s :*** You have already solved your problem!", user->nick.c_str());
			return CMD_FAILURE;
		}

		int result = ConvToInt(parameters[0]);
		if (result != (problem->first + problem->second))
		{
			user->WriteServ("NOTICE %s :*** %s is not the correct answer.", user->nick.c_str(), parameters[0].c_str());
			user->CommandFloodPenalty += 10000;
			return CMD_FAILURE;
		}

		ext.unset(user);
		user->WriteServ("NOTICE %s :*** %s is the correct answer!", user->nick.c_str(), parameters[0].c_str());
		return CMD_SUCCESS;
	}
};

class ModuleSolveMessage : public Module
{
 private:
	SimpleExtItem<Problem> ext;
	CommandSolve cmd;

 public:
	ModuleSolveMessage()
		: ext("solve-message", this)
		, cmd(this, ext)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		ServerInstance->Modules->AddService(ext);
		Implementation eventList[] = { I_OnUserInit, I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventList, this, sizeof(eventList)/sizeof(Implementation));
	}

	void OnUserInit(LocalUser* user)
	{
		Problem problem;
		problem.first = ServerInstance->GenRandomInt(9);
		problem.second = ServerInstance->GenRandomInt(9);
		problem.warned = false;
		ext.set(user, problem);
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list)
	{
		if (user->exempt || target_type != TYPE_USER)
			return MOD_RES_PASSTHRU;

		User* target = static_cast<User*>(dest);
		if (ServerInstance->ULine(target->server))
			return MOD_RES_PASSTHRU;

		Problem* problem = ext.get(user);
		if (!problem)
			return MOD_RES_PASSTHRU;

		if (problem->warned)
			return MOD_RES_DENY;

		user->WriteServ("NOTICE %s :*** Before you can send messages you must solve the following problem:", user->nick.c_str());
		user->WriteServ("NOTICE %s :*** What is %d + %d?", user->nick.c_str(), problem->first, problem->second);
		user->WriteServ("NOTICE %s :*** You can enter your answer using /QUOTE SOLVE <answer>", user->nick.c_str());
		problem->warned = true;
		return MOD_RES_DENY;
	}

	ModResult OnUserPreNotice(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}

	Version GetVersion()
	{
		return Version("Requires users to solve a basic maths problem before messaging others.");
	}
};

MODULE_INIT(ModuleSolveMessage)
