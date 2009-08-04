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

/* $ModDesc: Provides overriding join into a channel through explicit command */
/* $ModAuthor: danieldg */
/* $ModDepends: core 1.2 */

#include "inspircd.h"

/** Handle /OOJOIN
 */
class CommandOojoin : public Command
{
 public:
	bool active;
	CommandOojoin (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"OOJOIN", "o", 1, 1, false, 0)
	{
		syntax = "<channel>";
		active = false;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		this->active = true;
		ServerInstance->Parser->CallHandler("JOIN", parameters, user);
		this->active = false;
		return CMD_LOCALONLY;
	}
};

class ModuleOoJoin : public Module
{
	CommandOojoin cmd;
 public:
	ModuleOoJoin(InspIRCd* Me)
		: Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);

		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);

	}

	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (cmd.active)
		{
			ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick) + " used OOJOIN to join " + cname);
			return -1;
		}
		return 0;
	}

	virtual ~ModuleOoJoin()
	{
	}

	void Prioritize()
	{
		Module *override = ServerInstance->Modules->Find("m_override.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserPreJoin, PRIORITY_BEFORE, &override);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", 0, API_VERSION);
	}
};

MODULE_INIT(ModuleOoJoin)
