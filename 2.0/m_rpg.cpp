/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008-2016 Naram Qashat <cyberbotx@cyberbotx.com>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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
 * ============================================================================
 * This module was inspired by the roleplay commands created by Falerin for
 * MagicStar. The NPC, NPCA and AMBIANCE commands were used there, while
 * NARRATOR and NARRATORA were my ideas.
 */

/* $ModDesc: Provides NPC, NPCA, AMBIANCE, NARRATOR, and NARRATORA commands for use by Game Masters doing pen & paper RPGs via IRC */
/* $ModAuthor: Naram Qashat (CyberBotX) */
/* $ModAuthorMail: cyberbotx@cyberbotx.com */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
#include "xline.h"

/* Removes any ! characters from a given nick */
static std::string strip_npc_nick(const std::string &nick)
{
	std::string newnick;
	unsigned len = nick.size();
	for (unsigned x = 0; x < len; ++x)
	{
		char c = nick[x];
		if (c != '!')
			newnick += c;
	}
	return newnick;
}

/* Sends a message to a channel, splitting it as needed if the message is too long.
 * It'll usually only split into 2 messages because of the 512 character limit. */
static void send_message(Channel *c, const std::string &source, std::string text, bool action)
{
	/* 510 - colon prefixing source - PRIVMSG - colon prefixing text - 3 spaces = 498
	 * Subtracting the source and the channel name to get how many characters we are allowed left
	 * If doing an action, subtract an additional 9 for the startind and ending ASCII character 1, ACTION and space */
	size_t allowedMessageLength = 498 - source.size() - c->name.size() - (action ? 9 : 0);
	/* This will keep attempting to determine if there is text to send */
	do
	{
		std::string textToSend = text;
		/* Check if the current text to send exceeds the length allowed */
		if (textToSend.size() > allowedMessageLength)
		{
			/* Look for the last space at or before the length allowed */
			size_t lastSpace = textToSend.find_last_of(' ', allowedMessageLength);
			/* If a space was found, split off the text to send */
			if (lastSpace != std::string::npos)
			{
				textToSend = text.substr(0, lastSpace);
				text = text.substr(lastSpace + 1);
			}
			/* Otherwise, we'll send whatever we have left, even if it may be too long */
			else
				text.clear();
		}
		else
			text.clear();

		c->WriteChannelWithServ(source, "PRIVMSG %s :%s%s%s", c->name.c_str(), action ? "\1ACTION " : "", textToSend.c_str(), action ? "\1" : "");
	} while (!text.empty());
}

/*
 * NOTE: For all commands, the user in the Handle function is checked to be local or not.
 *
 * If they are local, then the command passed through the module's OnPreCommand and the
 * text was set accordingly to prevent colon eating from happening. Channel is checked,
 * and if valid, user status for being an op in the channel is checked. Assuming all that
 * succeeds, then the command is set to the channel locally and then broadcast via ENCAP.
 * The reason that the ENCAP is created manually instead of automatically through the
 * GetRouting function is to prevent the same colon eating issue we handle in OnPreCommand.
 *
 * If they are not local, then the command must've come remotely and thus is being sent
 * directly to the handler. No channel or user checks are done, as they are assumed to
 * have been valid on the originating server, but the text was passed via ENCAP in such
 * a way that colon eating is not an issue and the text must be set, otherwise the
 * previous text from a local usage will be used. Broadcasting is skipped, as it would
 * be pretty bad to broadcast infinitely.
 */

/** Base class for /NPC and /NPCA
 */
class NPCx
{
	std::string cmdName, text;

public:
	NPCx(const std::string &cmd) : cmdName(cmd)
	{
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user, bool action)
	{
		Channel *c = ServerInstance->FindChan(parameters[0]);
		LocalUser *localUser = IS_LOCAL(user);
		if (localUser)
		{
			if (c)
			{
				if (!c->HasUser(user))
				{
					user->WriteNumeric(ERR_NOTONCHANNEL, "%s %s :You are not on that channel!", user->nick.c_str(), parameters[0].c_str());
					return CMD_FAILURE;
				}
				if (c->GetPrefixValue(user) < OP_VALUE)
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
		}
		else
			this->text = parameters[2];

		/* Source is in the form of: *[nick]*!npc@[server-name] */
		std::string npc_nick = strip_npc_nick(parameters[1]);
		std::string npc_source = "*" + npc_nick + "*!npc@" + ServerInstance->Config->ServerName;

		send_message(c, npc_source, this->text, action);

		if (localUser)
		{
			std::vector<std::string> params;
			params.push_back("*");
			params.push_back(this->cmdName);
			params.push_back(parameters[0]);
			params.push_back(npc_nick);
			params.push_back(":" + this->text);
			ServerInstance->PI->SendEncapsulatedData(params);
		}

		return CMD_SUCCESS;
	}

	void SetText(const std::string &newText)
	{
		this->text = newText;
	}
};

/** Handle /NPC
 */
class CommandNPC : public Command, public NPCx
{
public:
	CommandNPC(Module *parent) : Command(parent, "NPC", 3, 3), NPCx("NPC")
	{
		this->syntax = "<channel> <npc-name> <npc-text>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		return NPCx::Handle(parameters, user, false);
	}
};

/** Handle /NPCA
 */
class CommandNPCA : public Command, public NPCx
{
public:
	CommandNPCA(Module *parent) : Command(parent, "NPCA", 3, 3), NPCx("NPCA")
	{
		this->syntax = "<channel> <npc-name> <npc-text>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		return NPCx::Handle(parameters, user, true);
	}
};

/** Handle /AMBIANCE
 */
class CommandAmbiance : public Command
{
	std::string text;

public:
	CommandAmbiance(Module *parent) : Command(parent, "AMBIANCE", 2, 2)
	{
		this->syntax = "<channel> <text>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		Channel *c = ServerInstance->FindChan(parameters[0]);
		LocalUser *localUser = IS_LOCAL(user);
		if (localUser)
		{
			if (c)
			{
				if (!c->HasUser(user))
				{
					user->WriteNumeric(ERR_NOTONCHANNEL, "%s %s :You are not on that channel!", user->nick.c_str(), parameters[0].c_str());
					return CMD_FAILURE;
				}
				if (c->GetPrefixValue(user) < OP_VALUE)
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
		}
		else
			this->text = parameters[1];

		/* Source is in the form of: >Ambiance<!npc@[server-name] */
		std::string amb_source = ">Ambiance<!npc@" + ServerInstance->Config->ServerName;

		send_message(c, amb_source, this->text, false);

		if (localUser)
		{
			std::vector<std::string> params;
			params.push_back("*");
			params.push_back("AMBIANCE");
			params.push_back(parameters[0]);
			params.push_back(":" + this->text);
			ServerInstance->PI->SendEncapsulatedData(params);
		}

		return CMD_SUCCESS;
	}

	void SetText(const std::string &newText)
	{
		this->text = newText;
	}
};

/** Base class for /NARRATOR and /NARRATORA
 */
class Narratorx
{
	std::string cmdName, text;

public:
	Narratorx(const std::string &cmd) : cmdName(cmd)
	{
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user, bool action)
	{
		Channel *c = ServerInstance->FindChan(parameters[0]);
		LocalUser *localUser = IS_LOCAL(user);
		if (localUser)
		{
			if (c)
			{
				if (!c->HasUser(user))
				{
					user->WriteNumeric(ERR_NOTONCHANNEL, "%s %s :You are not on that channel!", user->nick.c_str(), parameters[0].c_str());
					return CMD_FAILURE;
				}
				if (c->GetPrefixValue(user) < OP_VALUE)
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
		}
		else
			this->text = parameters[1];

		/* Source is in the form of: -Narrator-!npc@[server-name] */
		std::string narr_source = std::string("-Narrator-!npc@") + ServerInstance->Config->ServerName;

		send_message(c, narr_source, this->text, action);

		if (localUser)
		{
			std::vector<std::string> params;
			params.push_back("*");
			params.push_back(this->cmdName);
			params.push_back(parameters[0]);
			params.push_back(":" + this->text);
			ServerInstance->PI->SendEncapsulatedData(params);
		}

		return CMD_SUCCESS;
	}

	void SetText(const std::string &newText)
	{
		this->text = newText;
	}
};

/** Handle /NARRATOR
 */
class CommandNarrator : public Command, public Narratorx
{
public:
	CommandNarrator(Module *parent) : Command(parent, "NARRATOR", 2, 2), Narratorx("NARRATOR")
	{
		this->syntax = "<channel> <text>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		return Narratorx::Handle(parameters, user, false);
	}
};

/** Handle /NARRATORA
 */
class CommandNarratorA : public Command, public Narratorx
{
public:
	CommandNarratorA(Module *parent) : Command(parent, "NARRATORA", 2, 2), Narratorx("NARRATORA")
	{
		this->syntax = "<channel> <text>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		return Narratorx::Handle(parameters, user, true);
	}
};

class ModuleRPGCommands : public Module
{
	CommandNPC npc;
	CommandNPCA npca;
	CommandAmbiance ambiance;
	CommandNarrator narrator;
	CommandNarratorA narratora;

public:
	ModuleRPGCommands() : npc(this), npca(this), ambiance(this), narrator(this), narratora(this)
	{
		QLine *ql = new QLine(ServerInstance->Time(), 0, ServerInstance->Config->ServerName, "Reserved for m_rpg.so", "-Narrator-");
		if (!ServerInstance->XLines->AddLine(ql, NULL))
			delete ql;
	}

	~ModuleRPGCommands()
	{
		ServerInstance->XLines->DelLine("-Narrator-", "Q", NULL);
	}

	void init()
	{
		ServiceProvider *services[] = { &this->npc, &this->npca, &this->ambiance, &this->narrator, &this->narratora };
		ServerInstance->Modules->AddServices(services, sizeof(services) / sizeof(services[0]));

		ServerInstance->Modules->Attach(I_OnPreCommand, this);
	}

	Version GetVersion()
	{
		return Version("Provides NPC, NPCA, AMBIANCE, NARRATOR, and NARRATORA commands for use by Game Masters doing pen & paper RPGs via IRC", VF_COMMON);
	}

	/** The purpose of this is to make it so the command text doesn't require a colon prefixing the text but also to allow a colon to start a word anywhere in the line.
	 */
	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		irc::spacesepstream sep(original_line);
		std::string text;
		sep.GetToken(text);
		sep.GetToken(text);
		if (command == "NPC" || command == "NPCA")
		{
			sep.GetToken(text);
			text = sep.GetRemaining();
			if (command == "NPC")
				this->npc.SetText(text);
			else
				this->npca.SetText(text);
		}
		else if (command == "AMBIANCE" || command == "NARRATOR" || command == "NARRATORA")
		{
			text = sep.GetRemaining();
			if (command == "AMBIANCE")
				this->ambiance.SetText(text);
			else if (command == "NARRATOR")
				this->narrator.SetText(text);
			else
				this->narratora.SetText(text);
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleRPGCommands)
