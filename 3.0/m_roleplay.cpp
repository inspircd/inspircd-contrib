/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Elizabeth Myers <elizabeth@interlinked.me>
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
 */

/* This module is a rewrite of the previous m_rpg.cpp for InspIRCd 2.
 * It now behaves more like the Charybdis module. It includes SCENE, SCENEA,
 * NARRATOR, NARRATORA, AMBIANCE, FSAY, FACTION, NPC, and NPCA.
 *
 * Differences between this module and the old one:
 * - NARRATOR and NARRATORA now use the pseudonick =Narrator=, and the module
 *   will no longer attempt to Q:Line the nickname (nicknames starting with =
 *   are invalid).
 * - Added SCENE and SCENEA, which behaves like it does in Charybdis.
 * - NPC and NPCA underline the nicknames, to help with apparent spoofing.
 * - No longer attempts to split up overflow, it's too messy and not worth the
 *   hassle.
 * - FSAY and FACTION behave as the old NPC and NPCA, but are oper-only to
 *   prevent apparent spoofing (requires the channels/roleplay privilege).
 * - Previous versions didn't require a colon to be added in the command for
 *   convenience, but this was non-RFC compliant behaviour and is no longer
 *   possible to implement. An alias should be added to your clients instead.
 * - Opers can override restrictions with the channels/roleplay-override
 *   privilege.
 * - Tags outgoing messages with the inspircd.org/roleplay-msg tag.
 * - Allows some configuration now; see below.
 *	
 * -- Elizafox, 25 November 2020
 */

/// $ModDesc: Provides commands for use in roleplay (tabletop RPG's, etc.)
/// $ModAuthor: Elizabeth Myers (Elizafox)
/// $ModAuthorMail: elizabeth@interlinked.me
/// $ModConfig: <roleplay mode="U" needchanmode=true needop=false npchost="fakeuser.invalid"> <class priv="channels/roleplay channels/roleplay-override">
/// $ModDepends: core 3

/* Helpop lines for the CUSER section
 * Find: '<helpop key="cuser" title="User Commands" ...'
 * Place 'AMBIANCE', 'NARRATOR', 'NARRATORA', 'NPC', and 'NPCA' in the command
 * list accordingly. Re-space as needed.
 * Find: '<helpop key="sslinfo" ...'
 * Replace the mode letters as needed and place just above that line:
<helpop key="ambiance" title="/AMBIANCE <channel> :<message>" value="
Send a message to the channel as if it came from the >Ambiance< user.

Note that your nick will be sent with the hostmask of the >Ambiance<
user.

Depending on the server configuration, you may need +o or above in the
channel, or +U may need to be set.
">

<helpop key="narrator" title="/NARRATOR <channel> :<message>" value="
Send a message to the channel as if it came from the =Narrator= user.

Note that your nick will be sent with the hostmask of the =Narrator=
user.

Depending on the server configuration, you may need +o or above in the
channel, or +U may need to be set.
">

<helpop key="narratora" title="/NARRATORA <channel> :<message>" value="
Send a message to the channel as if it came from the =Narrator= user,
but send it as a CTCP action (as if =Narrator= used /me).

Note that your nick will be sent with the hostmask of the =Narrator=
user.

Depending on the server configuration, you may need +o or above in the
channel, or +U may need to be set.
">

<helpop key="scene" title="/SCENE <channel> :<message>" value="
Send a message to the channel as if it came from the =Scene= user.

Note that your nick will be sent with the hostmask of the =Scene=
user.

Depending on the server configuration, you may need +o or above in the
channel, or +U may need to be set.
">

<helpop key="scenea" title="/SCENEA <channel> :<message>" value="
Send a message to the channel as if it came from the =Scene= user,
but send it as a CTCP action (as if =Narrator= used /me).

Note that your nick will be sent with the hostmask of the =Scene=
user.

Depending on the server configuration, you may need +o or above in the
channel, or +U may need to be set.
">

<helpop key="npc" title="/NPC <channel> <user> :<message>" value="
Send a message to the channel as if it came from the given user.

The user given will be sent formatted as underlined to avoid apparent
spoofing.

Depending on the server configuration, you may need +o or above in the
channel, or +U may need to be set.
">

<helpop key="npca" title="/NPCA <channel> <user> :<message>" value="
Send a message to the channel as if it came from the given user, but
send it as a CTCP action (as if the given user used /me).

The user given will be sent formatted as underlined to avoid apparent
spoofing.

Depending on the server configuration, you may need +o or above in the
channel, or +U may need to be set.
">

 * Helpop lines for the COPER section
 * Find: '<helpop key="coper" title="Oper Commands" ...'
 * Place 'FACTION' and 'FSAY' in the command list accordingly. Re-space as
 * needed.
 * Find: '<helpop key="filter" ...'
 * Place just above that line:
<helpop key="faction" title="/FACTION <channel> <user> :<message>" value="
Send a message to the channel as if it came from the given user, but
send it as a CTCP action (as if the user used /me).

Unlike /NPC, the user will not be underlined.

This command requires the channels/roleplay permission.
">

<helpop key="fsay" title="/FSAY <channel> <user> :<message>" value="
Send a message to the channel as if it came from the given user.

Unlike /NPC, the user will not be underlined.

This command requires the channels/roleplay permission.
">

 * Helpop lines for the CHMODES section
 * Find: '<helpop key="chmodes" title="Channel Modes" ...'
 * Change the mode letter if needed and place the following in the channel mode
 * list accordingly.

 U                  Enable roleplay commands on this channel (requires
                    the roleplay module)
 */

#include "inspircd.h"
#include "modules/ctctags.h"

enum RoleplayNumerics
{
	// This seems to be what Charybdis uses, although I don't know its origin.
	ERR_ROLEPLAY = 573
};

namespace
{
	// These are used everywhere.
	bool need_op;
	bool need_mode;
	std::string npc_host;
}

/* Sigh. I had to copy this from the PRIVMSG module because it's not exported.
 * This isn't great to say the least, but I gutted everything that wasn't
 * needed.
 *
 * --Elizafox
 */
class MessageDetailsImpl : public MessageDetails
{
public:
	MessageDetailsImpl(MessageType mt, const std::string& msg, const ClientProtocol::TagMap& tags)
		: MessageDetails(mt, msg, tags)
	{
	}

	bool IsCTCP(std::string& name) const CXX11_OVERRIDE
	{
		if (!this->IsCTCP())
			return false;

		size_t end_of_name = text.find(' ', 2);
		if (end_of_name == std::string::npos)
		{
			// The CTCP only contains a name.
			size_t end_of_ctcp = *text.rbegin() == '\x1' ? 1 : 0;
			name.assign(text, 1, text.length() - 1 - end_of_ctcp);
			return true;
		}

		// The CTCP contains a name and a body.
		name.assign(text, 1, end_of_name - 1);
		return true;
	}

	bool IsCTCP(std::string& name, std::string& body) const CXX11_OVERRIDE
	{
		// Implementation not required
		return false;
	}

	bool IsCTCP() const CXX11_OVERRIDE
	{
		// According to draft-oakley-irc-ctcp-02 a valid CTCP must begin with SOH and
		// contain at least one octet which is not NUL, SOH, CR, LF, or SPACE. As most
		// of these are restricted at the protocol level we only need to check for SOH
		// and SPACE.
		return (text.length() >= 2) && (text[0] == '\x1') &&  (text[1] != '\x1') && (text[1] != ' ');
	}
};

class RoleplayTag : public ClientProtocol::MessageTagProvider
{
private:
	CTCTags::CapReference ctctagcap;

public:
	RoleplayTag(Module* mod)
		: ClientProtocol::MessageTagProvider(mod)
		, ctctagcap(mod)
	{
	}

	bool ShouldSendTag(LocalUser* user, const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE
	{
		return ctctagcap.get(user);
	}
};

// This is here to make the channel mode optional and configurable.
class RoleplayMode : public SimpleChannelModeHandler
{
public:
	RoleplayMode(Module* Creator)
		: SimpleChannelModeHandler(Creator, "roleplay", '\0')
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("roleplay");
		mode = tag->getString("mode", "U", 1, 1)[0];
		if(!tag->getBool("needchanmode", true))
			DisableAutoRegister();
	}
};

/* This class does the heavy lifting of handling all the sending machinery. It
 * helps cut back heavily on code duplication.
 */
class CommandBaseRoleplay : public Command
{
	SimpleChannelModeHandler& roleplaymode;
	RoleplayTag& roleplaytag;

	bool CheckMessage(User* user, MessageTarget& msgtarget, MessageDetailsImpl& msgdetails)
	{
		// Bypass restrictions/checks if this permission is active
		if(!user->HasPrivPermission("channels/roleplay-override"))
		{
			// Don't allow CTCP spoofing.
			std::string ctcpname;
			if(msgdetails.IsCTCP(ctcpname) && !irc::equals(ctcpname, "ACTION"))
				return false;

			/* Inform modules that a message wants to be sent and check the
			 * results. This stops ban/censor evasion, etc. To do this we
			 * simply present this as a message from the user (effectively,
			 * that's what this is).
			 *
			 * This works just like the privmsg logic (I stole it from
			 * there. :p)
			 *
			 * --Elizafox
			 */
			ModResult modres;

			FIRST_MOD_RESULT(OnUserPreMessage, modres, (user, msgtarget, msgdetails));
			if (modres == MOD_RES_DENY)
			{
				// Inform modules that a module blocked the mssage.
				FOREACH_MOD(OnUserMessageBlocked, (user, msgtarget, msgdetails));
				return false;
			}

			// Check whether a module zapped the message body.
			if (msgdetails.text.empty())
			{
				user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
				return false;
			}
		}

		return true;
	}

	bool CheckChannelPermissions(User* user, Channel* c)
	{
		if(!c->HasUser(user))
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, c->name, "You're not on that channel");
			return false;
		}

		// Ignore restrictions if the operator has channels/roleplay-override
		if(!user->HasPrivPermission("channels/roleplay-override"))
		{
			if(need_op && c->GetPrefixValue(user) < OP_VALUE)
			{
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, c->name, "You're not a channel operator");
				return false;
			}

			if(need_mode && !c->IsModeSet(roleplaymode))
			{
				user->WriteNumeric(ERR_ROLEPLAY, c->name, InspIRCd::Format("Channel mode +%c must be set", roleplaymode.GetModeChar()));
				return false;
			}
		}

		return true;
	}

	void SendMessage(User* user, Channel* c, const std::string& source, MessageTarget& msgtarget, MessageDetails& msgdetails)
	{
		// Inform modules that a message is about to be sent.
		FOREACH_MOD(OnUserMessage, (user, msgtarget, msgdetails));

		ClientProtocol::Messages::Privmsg privmsg(source, c, msgdetails.text, MSG_PRIVMSG);
		privmsg.AddTag("inspircd.org/roleplay-msg", &roleplaytag, user->nick);
		c->Write(ServerInstance->GetRFCEvents().privmsg, privmsg);
		ServerInstance->PI->SendMessage(c, 0, msgdetails.text, MSG_PRIVMSG);

		// Inform modules that a message was sent.
		FOREACH_MOD(OnUserPostMessage, (user, msgtarget, msgdetails));
	}

	std::string MakeFakeHostmask(User* user, const std::string& source)
	{
		/* Include the user's nickname as the ident as to differentiate
		 * various NPC's.
		 */
		return InspIRCd::Format("%s!%s@%s", source.c_str(), user->nick.c_str(), npc_host.c_str());
	}

protected:
	/* This may be a static string, or generated from the parameters list
	 * Alternatively, you can return nothing to signal an invalid nickname.
	 */
	virtual std::string GetSource(const Params&) = 0;

	/* The message position can vary based on command, but this also makes a
	 * convenient point for transforming the message (like making it an
	 * ACTION).
	 */
	virtual std::string GetMessage(const Params&) = 0;

	// Transform str into a CTCP action
	std::string MakeAction(const std::string& str)
	{
		return InspIRCd::Format("\1ACTION %s\1", str.c_str());
	}

	// Transform str into an underlined string
	std::string MakeUnderline(const std::string& str)
	{
		return InspIRCd::Format("\x1F%s\x1F", str.c_str());
	}

public:
	CommandBaseRoleplay(Module* Creator, const std::string& cmd, int params, RoleplayMode& mode, RoleplayTag& tag)
		: Command(Creator, cmd, params, params)
		, roleplaymode(mode)
		, roleplaytag(tag)
	{
	}

	/* Actually send out the message (or an error)
	 * The machinery for transforming the message/source is in GetSource/GetMessage.
	 */
	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		Channel* c = ServerInstance->FindChan(parameters[0]);
		LocalUser* luser = IS_LOCAL(user);

		// Only bother with strict checks if this user is local
		if(luser)
		{
			if(c)
			{
				if(!CheckChannelPermissions(user, c))
					return CMD_FAILURE;
			}
			else
			{
				user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
				return CMD_FAILURE;
			}
		}

		std::string source = GetSource(parameters);
		if(source.empty())
		{
			user->WriteNumeric(ERR_ROLEPLAY, c->name, "Invalid roleplay nickname");
			return CMD_FAILURE;
		}

		MessageDetailsImpl msgdetails(MSG_PRIVMSG, GetMessage(parameters), parameters.GetTags());
		MessageTarget msgtarget(c, 0);

		if(!CheckMessage(user, msgtarget, msgdetails))
			return CMD_FAILURE;

		// Spoof the hostmask for the source nickname before sending
		source = MakeFakeHostmask(user, source);
		SendMessage(user, c, source, msgtarget, msgdetails);

		/* Since this is a message, if the user is local, then update
		 * their idle time.
		 */
		if(luser)
			luser->idle_lastmsg = ServerInstance->Time();

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const CommandBase::Params& parameters) CXX11_OVERRIDE
	{
		return ROUTE_OPT_BCAST;
	}
};

// This command was inspired by Charybdis's m_roleplay module.
class CommandScene : public CommandBaseRoleplay
{
protected:
	std::string GetSource(const Params& parameters) CXX11_OVERRIDE
	{
		// Similar to the old Charybdis module
		return "=Scene=";
	}

	std::string GetMessage(const Params& parameters) CXX11_OVERRIDE
	{
		return parameters[1];
	}

public:
	CommandScene(Module* Creator, RoleplayMode& mode, RoleplayTag& tag)
		: CommandBaseRoleplay(Creator, "SCENE", 2, mode, tag)
	{
		syntax = "<channel> :<message>";
	}
};

class CommandSceneA : public CommandBaseRoleplay
{
protected:
	std::string GetSource(const Params& parameters) CXX11_OVERRIDE
	{
		return "=Scene=";
	}

	std::string GetMessage(const Params& parameters) CXX11_OVERRIDE
	{
		return MakeAction(parameters[1]);
	}

public:
	CommandSceneA(Module* Creator, RoleplayMode& mode, RoleplayTag& tag)
		: CommandBaseRoleplay(Creator, "SCENEA", 2, mode, tag)
	{
		syntax = "<channel> :<message>";
	}
};

class CommandAmbiance : public CommandBaseRoleplay
{
protected:
	std::string GetSource(const Params& parameters) CXX11_OVERRIDE
	{
		// Compatibility with the old m_rpg module
		return ">Ambiance<";
	}

	std::string GetMessage(const Params& parameters) CXX11_OVERRIDE
	{
		return parameters[1];
	}

public:
	CommandAmbiance(Module* Creator, RoleplayMode& mode, RoleplayTag& tag)
		: CommandBaseRoleplay(Creator, "AMBIANCE", 2, mode, tag)
	{
		syntax = "<channel> :<message>";
	}
};

class CommandNarrator : public CommandBaseRoleplay
{
protected:
	std::string GetSource(const Params& parameters) CXX11_OVERRIDE
	{
		return "=Narrator=";
	}

	std::string GetMessage(const Params& parameters) CXX11_OVERRIDE
	{
		return parameters[1];
	}

public:
	CommandNarrator(Module* Creator, RoleplayMode& mode, RoleplayTag& tag)
		: CommandBaseRoleplay(Creator, "NARRATOR", 2, mode, tag)
	{
		syntax = "<channel> :<message>";
	}
};

class CommandNarratorA : public CommandBaseRoleplay
{
protected:
	// Compatibility with the old m_rpg module
	std::string GetSource(const Params& parameters) CXX11_OVERRIDE
	{
		return "=Narrator=";
	}

	std::string GetMessage(const Params& parameters) CXX11_OVERRIDE
	{
		return MakeAction(parameters[1]);
	}

public:
	CommandNarratorA(Module* Creator, RoleplayMode& mode, RoleplayTag& tag)
		: CommandBaseRoleplay(Creator, "NARRATORA", 2, mode, tag)
	{
		syntax = "<channel> :<message>";
	}
};

class CommandFSay : public CommandBaseRoleplay
{
protected:
	std::string GetSource(const Params& parameters) CXX11_OVERRIDE
	{
		return (ServerInstance->IsNick(parameters[1]) ? parameters[1] : "");
	}

	std::string GetMessage(const Params& parameters) CXX11_OVERRIDE
	{
		return parameters[2];
	}

public:
	CommandFSay(Module* Creator, RoleplayMode& mode, RoleplayTag& tag)
		: CommandBaseRoleplay(Creator, "FSAY", 3, mode, tag)
	{
		syntax = "<channel> <nickname> :<message>";
		flags_needed = 'o';
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if(!user->HasPrivPermission("channels/roleplay"))
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "Permission Denied - You do not have the required operator privileges");
			return CMD_FAILURE;
		}

		return CommandBaseRoleplay::Handle(user, parameters);
	}
};

class CommandFAction : public CommandBaseRoleplay
{
protected:
	std::string GetSource(const Params& parameters) CXX11_OVERRIDE
	{
		return (ServerInstance->IsNick(parameters[1]) ? parameters[1] : "");
	}

	std::string GetMessage(const Params& parameters) CXX11_OVERRIDE
	{
		return MakeAction(parameters[2]);
	}

public:
	CommandFAction(Module* Creator, RoleplayMode& mode, RoleplayTag& tag)
		: CommandBaseRoleplay(Creator, "FACTION", 3, mode, tag)
	{
		syntax = "<channel> <nickname> :<message>";
		flags_needed = 'o';
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if(!user->HasPrivPermission("channels/roleplay"))
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "Permission Denied - You do not have the required operator privileges");
			return CMD_FAILURE;
		}

		return CommandBaseRoleplay::Handle(user, parameters);
	}
};

class CommandNPC : public CommandBaseRoleplay
{
protected:
	std::string GetSource(const Params& parameters) CXX11_OVERRIDE
	{
		if(!ServerInstance->IsNick(parameters[1]))
			return "";

		return MakeUnderline(parameters[1]);
	}

	std::string GetMessage(const Params& parameters) CXX11_OVERRIDE
	{
		return parameters[2];
	}

public:
	CommandNPC(Module* Creator, RoleplayMode& mode, RoleplayTag& tag)
		: CommandBaseRoleplay(Creator, "NPC", 3, mode, tag)
	{
		syntax = "<channel> <nickname> :<message>";
	}
};

class CommandNPCA : public CommandBaseRoleplay
{
protected:
	std::string GetSource(const Params& parameters) CXX11_OVERRIDE
	{
		if(!ServerInstance->IsNick(parameters[1]))
			return "";

		return MakeUnderline(parameters[1]);
	}

	std::string GetMessage(const Params& parameters) CXX11_OVERRIDE
	{
		return MakeAction(parameters[2]);
	}

public:
	CommandNPCA(Module* Creator, RoleplayMode& mode, RoleplayTag& tag)
		: CommandBaseRoleplay(Creator, "NPCA", 3, mode, tag)
	{
		syntax = "<channel> <nickname> :<message>";
	}
};

class ModuleRoleplay : public Module
{
	RoleplayMode roleplaymode;
	RoleplayTag roleplaytag;

	CommandScene cscene;
	CommandSceneA cscenea;
	CommandAmbiance cambiance;
	CommandNarrator cnarrator;
	CommandNarratorA cnarratora;
	CommandFSay cfsay;
	CommandFAction cfaction;
	CommandNPC cnpc;
	CommandNPCA cnpca;

public:
	ModuleRoleplay()
		: roleplaymode(this)
		, roleplaytag(this)
		, cscene(this, roleplaymode, roleplaytag)
		, cscenea(this, roleplaymode, roleplaytag)
		, cambiance(this, roleplaymode, roleplaytag)
		, cnarrator(this, roleplaymode, roleplaytag)
		, cnarratora(this, roleplaymode, roleplaytag)
		, cfsay(this, roleplaymode, roleplaytag)
		, cfaction(this, roleplaymode, roleplaytag)
		, cnpc(this, roleplaymode, roleplaytag)
		, cnpca(this, roleplaymode, roleplaytag)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("roleplay");
		need_op = tag->getBool("needop", false);
		npc_host = tag->getString("npchost", "fakeuser.invalid", InspIRCd::IsHost);

		// The mode can only be enabled at load-time, so check this instead
		need_mode = (roleplaymode.GetId() != ModeParser::MODEID_MAX);

		// Warn about possibly insecure configuration
		if(!(need_mode || need_op))
			ServerInstance->SNO->WriteToSnoMask('a', "WARNING: Roleplay configuration has needchanmode and needop both disabled, this could allow for apparent spoofing!");
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides commands for use in roleplay (tabletop RPGs, etc.)", VF_COMMON);
	}
};

MODULE_INIT(ModuleRoleplay)
