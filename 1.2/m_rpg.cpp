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

/* $ModDesc: Provides NPC, NPCA, AMBIANCE, NARRATOR, and NARRATORA commands for use by Game Masters doing pen & paper RPGs via IRC */
/* $ModAuthor: Naram Qashat (CyberBotX) */
/* $ModAuthorMail: cyberbotx@cyberbotx.com */
/* $ModDepends: core 1.2-1.3 */

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
			if (c != '!')
				newnick += c;
		}
		return newnick;
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user, bool action)
	{
		Channel *c = ServerInstance->FindChan(parameters[0]);
		if (c)
		{
			if (!c->HasUser(user))
			{
				user->WriteNumeric(ERR_NOTONCHANNEL, "%s %s :You are not on that channel!", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
			if (c->GetStatus(user) < STATUS_OP)
			{
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You're not a channel operator", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
		}
		else
		{
			user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :No such channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		/* Source is in the form of: *[nick]*!npc@[server-name] */
		std::string npc_source = std::string("*") + strip_npc_nick(parameters[1]) + "*!npc@" + ServerInstance->Config->ServerName;

		c->WriteChannelWithServ(npc_source.c_str(), "PRIVMSG %s :%s%s%s", c->name.c_str(), action ? "\1ACTION " : "", parameters[2].c_str(), action ? "\1" : "");

		/* we want this routed out! */
		return CMD_SUCCESS;
	}
};

/** Handle /NPC
 */
class cmd_npc : public Command, public NPCx
{
public:
	cmd_npc(InspIRCd *Instance) : Command(Instance, "NPC", 0, 3, 3), NPCx(Instance)
	{
		this->source = "m_rpg.so";
		syntax = "<channel> <npc-name> <npc-text>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		return NPCx::Handle(parameters, user, false);
	}
};

/** Handle /NPCA
 */
class cmd_npca : public Command, public NPCx
{
public:
	cmd_npca(InspIRCd *Instance) : Command(Instance, "NPCA", 0, 3, 3), NPCx(Instance)
	{
		this->source = "m_rpg.so";
		syntax = "<channel> <npc-name> <npc-action>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		return NPCx::Handle(parameters, user, true);
	}
};

/** Handle /AMBIANCE
 */
class cmd_ambiance : public Command
{
public:
	cmd_ambiance(InspIRCd *Instance) : Command(Instance, "AMBIANCE", 0, 2, 2)
	{
		this->source = "m_rpg.so";
		syntax = "<channel> <text>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		Channel *c = ServerInstance->FindChan(parameters[0]);
		if (c)
		{
			if (!c->HasUser(user))
			{
				user->WriteNumeric(ERR_NOTONCHANNEL, "%s %s :You are not on that channel!", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
			if (c->GetStatus(user) < STATUS_OP)
			{
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You're not a channel operator", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
		}
		else
		{
			user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :No such channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		/* Source is in the form of: >Ambiance<!npc@[server-name] */
		std::string amb_source = std::string(">Ambiance<!npc@") + ServerInstance->Config->ServerName;

		c->WriteChannelWithServ(amb_source.c_str(), "PRIVMSG %s :%s", c->name.c_str(), parameters[1].c_str());

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

	CmdResult Handle(const std::vector<std::string> &parameters, User *user, bool action)
	{
		Channel *c = ServerInstance->FindChan(parameters[0]);
		if (c)
		{
			if (!c->HasUser(user))
			{
				user->WriteNumeric(ERR_NOTONCHANNEL, "%s %s :You are not on that channel!", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
			if (c->GetStatus(user) < STATUS_OP)
			{
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You're not a channel operator", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
		}
		else
		{
			user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :No such channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		/* Source is in the form of: -Narrator-!npc@[server-name] */
		std::string narr_source = std::string("-Narrator-!npc@") + ServerInstance->Config->ServerName;

		c->WriteChannelWithServ(narr_source.c_str(), "PRIVMSG %s :%s%s%s", c->name.c_str(), action ? "\1ACTION " : "", parameters[1].c_str(), action ? "\1" : "");

		/* we want this routed out! */
		return CMD_SUCCESS;
	}
};

/** Handle /NARRATOR
 */
class cmd_narrator : public Command, public Narrator
{
public:
	cmd_narrator(InspIRCd *Instance) : Command(Instance, "NARRATOR", 0, 2, 2), Narrator(Instance)
	{
		this->source = "m_rpg.so";
		syntax = "<channel> <text>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		return Narrator::Handle(parameters, user, false);
	}
};

/** Handle /NARRATORA
 */
class cmd_narratora : public Command, public Narrator
{
public:
	cmd_narratora(InspIRCd *Instance) : Command(Instance, "NARRATORA", 0, 2, 2), Narrator(Instance)
	{
		this->source = "m_rpg.so";
		syntax = "<channel> <text>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		return Narrator::Handle(parameters, user, true);
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
	RPGCommandsModule(InspIRCd *Me) : Module(Me)
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
		return Version("1.0", VF_COMMON, API_VERSION);
	}
};

MODULE_INIT(RPGCommandsModule)
