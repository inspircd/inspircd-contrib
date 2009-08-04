                                                                     
                                                                     
                                                                     
                                             
/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides NPC, NPCA, and AMBIANCE commands for use by Game Masters doing pen & paper RPGs via IRC */
/* $ModAuthor: Naram Qashat (CyberBotX) */
/* $ModAuthorMail: cyberbotx@cyberbotx.com */
/* $ModDepends: core 1.2 */
/* $ModVersion: $Rev: 78 $ */

/** Base class for /NPC and /NPCA
 */
class NPCx
{
	InspIRCd *ServerInstance;
public:
	NPCx(InspIRCd *Instance) : ServerInstance(Instance) { }

	/* Removes any ! characters from a given nick */
	std::string strip_npc_nick(const std::string &nick)
	{
		std::string newnick = "";
		unsigned len = nick.size();
		for (unsigned x = 0; x < len; ++x)
		{
			char c = nick[x];
			if (c != '!') newnick += c;
		}
		return newnick;
	}

	CmdResult Handle(const char **parameters, int pcnt, userrec *user, bool action)
	{
		chanrec *c = ServerInstance->FindChan(parameters[0]);
		if (c)
		{
			if (!c->HasUser(user))
			{
				user->WriteServ("492 %s %s :You are not on that channel!", user->nick, parameters[0]);
				return CMD_FAILURE;
			}
			if (c->GetStatus(user) < STATUS_OP)
			{
				user->WriteServ("482 %s %s :You're not a channel operator", user->nick, parameters[0]);
				return CMD_FAILURE;
			}
		}
		else
		{
			user->WriteServ("401 %s %s :No such channel", user->nick, parameters[0]);
			return CMD_FAILURE;
		}

		/* Source is in the form of: *[nick]*!npc@[server-name] */
		std::string source = std::string("*") + strip_npc_nick(parameters[1]) + "*!npc@" + ServerInstance->Config->ServerName;

		std::string line = "";
		for (int i = 2; i < pcnt - 1; ++i)
		{
			line = line + parameters[i] + " ";
		}
		line = line + parameters[pcnt - 1];

		c->WriteChannelWithServ(source.c_str(), "PRIVMSG %s :%s%s%s", c->name, action ? "\1ACTION " : "", line.c_str(), action ? "\1" : "");

		/* we want this routed out! */
		return CMD_SUCCESS;
	}
};

/** Handle /NPC
 */
class cmd_npc : public command_t, public NPCx
{
public:
	cmd_npc(InspIRCd *Instance) : command_t(Instance, "NPC", 0, 3), NPCx(Instance)
	{
		this->source = "m_rpg.so";
		syntax = "<channel> <npc-name> <npc-text>";
	}

	CmdResult Handle(const char **parameters, int pcnt, userrec *user)
	{
		return NPCx::Handle(parameters, pcnt, user, false);
	}
};

/** Handle /NPCA
 */
class cmd_npca : public command_t, public NPCx
{
public:
	cmd_npca(InspIRCd *Instance) : command_t(Instance, "NPCA", 0, 3), NPCx(Instance)
	{
		this->source = "m_rpg.so";
		syntax = "<channel> <npc-name> <npc-action>";
	}

	CmdResult Handle(const char **parameters, int pcnt, userrec *user)
	{
		return NPCx::Handle(parameters, pcnt, user, true);
	}
};

/** Handle /AMBIANCE
 */
class cmd_ambiance : public command_t
{
public:
	cmd_ambiance(InspIRCd *Instance) : command_t(Instance, "AMBIANCE", 0, 2)
	{
		this->source = "m_rpg.so";
		syntax = "<channel> <text>";
	}

	CmdResult Handle(const char **parameters, int pcnt, userrec *user)
	{
		chanrec *c = ServerInstance->FindChan(parameters[0]);
		if (c)
		{
			if (!c->HasUser(user))
			{
				user->WriteServ("492 %s %s :You are not on that channel!", user->nick, parameters[0]);
				return CMD_FAILURE;
			}
			if (c->GetStatus(user) < STATUS_OP)
			{
				user->WriteServ("482 %s %s :You're not a channel operator", user->nick, parameters[0]);
				return CMD_FAILURE;
			}
		}
		else
		{
			user->WriteServ("401 %s %s :No such channel", user->nick, parameters[0]);
			return CMD_FAILURE;
		}

		/* Source is in the form of: >Ambiance<!npc@[server-name] */
		std::string source = std::string(">Ambiance<!npc@") + ServerInstance->Config->ServerName;

		std::string line = "";
		for (int i = 1; i < pcnt - 1; ++i)
		{
			line = line + parameters[i] + " ";
		}
		line = line + parameters[pcnt - 1];

		c->WriteChannelWithServ(source.c_str(), "PRIVMSG %s :%s", c->name, line.c_str());

		/* we want this routed out! */
		return CMD_SUCCESS;
	}
};

/** Base class for /NARRATOR and /NARRATORA
 */
class Narrator
{
	InspIRCd *ServerInstance;
public:
	Narrator(InspIRCd *Instance) : ServerInstance(Instance) { }

	CmdResult Handle(const char **parameters, int pcnt, userrec *user, bool action)
	{
		chanrec *c = ServerInstance->FindChan(parameters[0]);
		if (c)
		{
			if (!c->HasUser(user))
			{
				user->WriteServ("492 %s %s :You are not on that channel!", user->nick, parameters[0]);
				return CMD_FAILURE;
			}
			if (c->GetStatus(user) < STATUS_OP)
			{
				user->WriteServ("482 %s %s :You're not a channel operator", user->nick, parameters[0]);
				return CMD_FAILURE;
			}
		}
		else
		{
			user->WriteServ("401 %s %s :No such channel", user->nick, parameters[0]);
			return CMD_FAILURE;
		}

		/* Source is in the form of: -Narrator-!npc@[server-name] */
		std::string source = std::string("-Narrator-!npc@") + ServerInstance->Config->ServerName;

		std::string line = "";
		for (int i = 1; i < pcnt - 1; ++i)
		{
			line = line + parameters[i] + " ";
		}
		line = line + parameters[pcnt - 1];

		c->WriteChannelWithServ(source.c_str(), "PRIVMSG %s :%s%s%s", c->name, action ? "\1ACTION " : "", line.c_str(), action ? "\1" : "");

		/* we want this routed out! */
		return CMD_SUCCESS;
	}
};

/** Handle /NARRATOR
 */
class cmd_narrator : public command_t, public Narrator
{
public:
	cmd_narrator(InspIRCd *Instance) : command_t(Instance, "NARRATOR", 0, 2), Narrator(Instance)
	{
		this->source = "m_rpg.so";
		syntax = "<channel> <text>";
	}

	CmdResult Handle(const char **parameters, int pcnt, userrec *user)
	{
		return Narrator::Handle(parameters, pcnt, user, false);
	}
};

/** Handle /NARRATORA
 */
class cmd_narratora : public command_t, public Narrator
{
public:
	cmd_narratora(InspIRCd *Instance) : command_t(Instance, "NARRATORA", 0, 2), Narrator(Instance)
	{
		this->source = "m_rpg.so";
		syntax = "<channel> <text>";
	}

	CmdResult Handle(const char **parameters, int pcnt, userrec *user)
	{
		return Narrator::Handle(parameters, pcnt, user, true);
	}
};

class RPGCommandsModule : public Module
{
	cmd_npc *npc;
	cmd_npca *npca;
	cmd_ambiance *ambiance;
	cmd_narrator *narrator;
	cmd_narratora *narratora;
public:
	RPGCommandsModule(InspIRCd *Me) : Module::Module(Me)
	{
		npc = new cmd_npc(ServerInstance);
		ServerInstance->AddCommand(npc);
		npca = new cmd_npca(ServerInstance);
		ServerInstance->AddCommand(npca);
		ambiance = new cmd_ambiance(ServerInstance);
		ServerInstance->AddCommand(ambiance);
		narrator = new cmd_narrator(ServerInstance);
		ServerInstance->AddCommand(narrator);
		narratora = new cmd_narratora(ServerInstance);
		ServerInstance->AddCommand(narratora);
	}

	virtual ~RPGCommandsModule() { }

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON, API_VERSION);
	}
};

MODULE_INIT(RPGCommandsModule)

