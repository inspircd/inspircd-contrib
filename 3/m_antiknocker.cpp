/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023 Sadie Powell <sadie@witchery.services>
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

/// $CompilerFlags: -std=c++11

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <antiknock nickregex="(st|sn|cr|pl|pr|fr|fl|qu|br|gr|sh|sk|tr|kl|wr|bl|[bcdfgklmnprstvwz])([aeiou][aeiou][bcdfgklmnprstvwz])(ed|est|er|le|ly|y|ies|iest|ian|ion|est|ing|led|inger|[abcdfgklmnprstvwz])" docmd="yes" donick="yes" donotice="yes" shunduration="15" shunreason="User was caught in an antiknock trap">
/// $ModDesc: Attempts to block a common IRC spambot.
/// $ModDepends: core 3


#include <regex>

#include "inspircd.h"
#include "modules/shun.h"

class ModuleAntiKnocker CXX11_FINAL
	: public Module
{
public:
	bool docmd;
	bool donick;
	bool donotice;
	std::regex nickregex;
	LocalIntExt seenmsg;
	unsigned long shunduration;
	std::string shunreason;

	void PunishUser(LocalUser* user)
	{
		Shun* sh = new Shun(ServerInstance->Time(), shunduration, MODNAME "@" + ServerInstance->Config->ServerName, shunreason, user->GetIPString());
		if (ServerInstance->XLines->AddLine(sh, nullptr))
		{
			ServerInstance->XLines->ApplyLines();
			return;
		}

		// No shunning? Annoying. Just quit em.
		delete sh;

		std::string message;
		if (user->registered != REG_ALL)
			message = "Registration timeout";
		else
			message = InspIRCd::Format("Ping timeout: %u seconds", user->GetClass()->pingtime);
		ServerInstance->Users.QuitUser(user, message);
	}

	ModuleAntiKnocker()
		: seenmsg("seenmsg", ExtensionItem::EXT_USER, this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Attempts to block a common IRC spambot.");
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("antiknock");

		const std::string nick = tag->getString("nickregex", "(st|sn|cr|pl|pr|fr|fl|qu|br|gr|sh|sk|tr|kl|wr|bl|[bcdfgklmnprstvwz])([aeiou][aeiou][bcdfgklmnprstvwz])(ed|est|er|le|ly|y|ies|iest|ian|ion|est|ing|led|inger|[abcdfgklmnprstvwz])");
		try
		{
			std::regex newnickregex(nick);
			std::swap(nickregex, newnickregex);
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Nick regex set to %s", nick.c_str());
		}
		catch (const std::regex_error& err)
		{
			throw ModuleException(InspIRCd::Format("<antiknock:nickregex> is invalid: %s", err.what()));
		}

		docmd = tag->getBool("docmd", true);
		donick = tag->getBool("donick", true);
		donotice = tag->getBool("donotice", false);
		shunduration = tag->getDuration("shunduration", 60*15, 60);
		shunreason = tag->getString("shunreason", "User was caught in an antiknock trap", 1);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) CXX11_OVERRIDE
	{
		if (!docmd || !validated || user->registered != REG_ALL)
			return MOD_RES_PASSTHRU;

		if (command == "PRIVMSG" && irc::equals(parameters[0], "NickServ"))
		{
			seenmsg.set(user, ServerInstance->Time());
			return MOD_RES_PASSTHRU;
		}

		if (command == "LIST")
		{
			time_t whenlist = seenmsg.get(user);
			if (ServerInstance->Time() - whenlist <= 3)
			{
				// This is almost certainly a bot; punish them!
				ServerInstance->SNO.WriteToSnoMask('a', "User %s (%s) was caught in an knocker trap!",
						user->nick.c_str(), user->MakeHost().c_str());

				PunishUser(user);
				return MOD_RES_DENY;
			}
		}

		seenmsg.unset(user);
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) CXX11_OVERRIDE
	{
		if (!donick || !std::regex_match(newnick, nickregex))
			return MOD_RES_PASSTHRU;

		ServerInstance->SNO.WriteToSnoMask('a', "User %s (%s) was prevented from using a knocker nick: %s",
			user->nick.c_str(), user->MakeHost().c_str(), newnick.c_str());

		PunishUser(user);
		return MOD_RES_DENY;
	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		if (donotice)
			user->WriteNotice("*** You are not welcome on this network if you are a malicious bot. If you are not a malicious bot bot please ignore this message.");
	}
};

MODULE_INIT(ModuleAntiKnocker)

