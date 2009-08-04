/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides nicer-looking SAMODE */
/* $ModAuthor: danieldg */
/* $ModDepends: core 1.2 */

#include "inspircd.h"

/** Handle /OOMODE
 */
class CommandOomode : public Command
{
 public:
	bool active;
	CommandOomode (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"OOMODE", "o", 2, false, 0)
	{
		syntax = "<target> <modes> {<mode-parameters>}";
		active = false;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		this->active = true;
		ServerInstance->Parser->CallHandler("MODE", parameters, user);
		if (ServerInstance->Modes->GetLastParse().length())
			ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick) + " used OOMODE: " +ServerInstance->Modes->GetLastParse());
		this->active = false;
		return CMD_LOCALONLY;
	}
};

class ModuleOoMode : public Module
{
	CommandOomode cmd;
 public:
	ModuleOoMode(InspIRCd* Me)
		: Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);

		Implementation eventlist[] = { I_OnAccessCheck };
		ServerInstance->Modules->Attach(eventlist, this, 1);

	}

	virtual ModResult OnAccessCheck(User* source,User* dest,Channel* channel,int access_type)
	{
		if (cmd.active)
			return -1;
		return 0;
	}

	virtual ~ModuleOoMode()
	{
	}

	void Prioritize()
	{
		Module *override = ServerInstance->Modules->Find("m_override.so");
		ServerInstance->Modules->SetPriority(this, I_OnAccessCheck, PRIORITY_BEFORE, &override);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", 0, API_VERSION);
	}
};

MODULE_INIT(ModuleOoMode)
