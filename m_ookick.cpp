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

/* $ModDesc: Provides overriding kick through explicit command */
/* $ModAuthor: danieldg */
/* $ModDepends: core 1.2 */

#include "inspircd.h"

/** Handle /OOKICK
 */
class CommandOokick : public Command
{
 public:
	bool active;
	CommandOokick (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"OOKICK", "o", 2, false, 0)
	{
		syntax = "<target> <kick> {<mode-parameters>}";
		active = false;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		this->active = true;
		ServerInstance->Parser->CallHandler("KICK", parameters, user);
		this->active = false;
		return CMD_LOCALONLY;
	}
};

class ModuleOoKick : public Module
{
	CommandOokick cmd;
 public:
	ModuleOoKick(InspIRCd* Me)
		: Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);

		Implementation eventlist[] = { I_OnUserPreKick };
		ServerInstance->Modules->Attach(eventlist, this, 1);

	}

	virtual ModResult OnUserPreKick(User* source, User* user, Channel* chan, const std::string &reason)
	{
		if (cmd.active)
		{
			ServerInstance->SNO->WriteGlobalSno('a',std::string(source->nick)+" used OOKICK to kick "+std::string(user->nick)+" from "+std::string(chan->name)+" ("+reason+")");
			return -1;
		}
		return 0;
	}

	virtual ~ModuleOoKick()
	{
	}

	void Prioritize()
	{
		Module *override = ServerInstance->Modules->Find("m_override.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserPreKick, PRIORITY_BEFORE, &override);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", 0, API_VERSION);
	}
};

MODULE_INIT(ModuleOoKick)
