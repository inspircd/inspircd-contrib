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

/// $ModAuthor: Sadie Powell <sadie@witchery.services>
/// $ModConfig: <antiknock nickregex="(st|sn|cr|pl|pr|fr|fl|qu|br|gr|sh|sk|tr|kl|wr|bl|[bcdfgklmnprstvwz])([aeiou][aeiou][bcdfgklmnprstvwz])(ed|est|er|le|ly|y|ies|iest|ian|ion|est|ing|led|inger|[abcdfgklmnprstvwz])" docmd="yes" dokill="yes" donick="yes" donotice="yes" doshun="yes" shunduration="15" shunreason="User was caught in an antiknock trap">
/// $ModDesc: Attempts to block a common IRC spambot.
/// $ModDepends: core 4


#include <regex>

#include "inspircd.h"
#include "extension.h"
#include "modules/shun.h"

class ModuleAntiKnocker final
	: public Module
{
public:
	bool docmd;
	bool dokill;
	bool donick;
	bool donotice;
	bool doshun;
	std::regex nickregex;
	IntExtItem seenmsg;
	unsigned long shunduration;
	std::string shunreason;

	void PunishUser(LocalUser* user)
	{
		if (doshun)
		{
			auto* sh = new Shun(ServerInstance->Time(), shunduration, MODNAME "@" + ServerInstance->Config->ServerName, shunreason, user->GetAddress());
			if (ServerInstance->XLines->AddLine(sh, nullptr))
			{
				ServerInstance->XLines->ApplyLines();
				return;
			}

			// No shunning? Annoying. Just quit em.
			delete sh;
		}

		if (dokill)
		{
			std::string message;
			if (!user->IsFullyConnected())
				message = "Connection timeout";
			else
				message = INSP_FORMAT("Ping timeout: {} seconds", user->GetClass()->pingtime);
			ServerInstance->Users.QuitUser(user, message);
		}
	}

	ModuleAntiKnocker()
		: Module(VF_NONE, "Attempts to block a common IRC spambot.")
		, seenmsg(this, "seenmsg", ExtensionType::USER)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("antiknock");

		const std::string nick = tag->getString("nickregex", "(st|sn|cr|pl|pr|fr|fl|qu|br|gr|sh|sk|tr|kl|wr|bl|[bcdfgklmnprstvwz])([aeiou][aeiou][bcdfgklmnprstvwz])(ed|est|er|le|ly|y|ies|iest|ian|ion|est|ing|led|inger|[abcdfgklmnprstvwz])");
		try
		{
			std::regex newnickregex(nick);
			std::swap(nickregex, newnickregex);
			ServerInstance->Logs.Debug(MODNAME, "Nick regex set to {}", nick);
		}
		catch (const std::regex_error& err)
		{
			throw ModuleException(this, INSP_FORMAT("<antiknock:nickregex> is invalid: {}", err.what()));
		}

		docmd = tag->getBool("docmd", true);
		dokill = tag->getBool("dokill", true);
		donick = tag->getBool("donick", true);
		donotice = tag->getBool("donotice", true);
		doshun = tag->getBool("doshun", true);
		shunduration = tag->getDuration("shunduration", 60*15, 60);
		shunreason = tag->getString("shunreason", "User was caught in an antiknock trap", 1);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		if (!docmd || !validated || !user->IsFullyConnected())
			return MOD_RES_PASSTHRU;

		if (command == "PRIVMSG" && irc::equals(parameters[0], "NickServ"))
		{
			seenmsg.Set(user, ServerInstance->Time());
			return MOD_RES_PASSTHRU;
		}

		if (command == "LIST")
		{
			time_t whenlist = seenmsg.Get(user);
			if (ServerInstance->Time() - whenlist <= 3)
			{
				// This is almost certainly a bot; punish them!
				ServerInstance->SNO.WriteToSnoMask('a', "User {} ({}) was caught in an knocker trap!",
						user->nick, user->GetRealUserHost());

				PunishUser(user);
				return MOD_RES_DENY;
			}
		}

		seenmsg.Unset(user);
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) override
	{
		if (!donick || !std::regex_match(newnick, nickregex))
			return MOD_RES_PASSTHRU;

		ServerInstance->SNO.WriteToSnoMask('a', "User {} ({}) was prevented from using a knocker nick: {}",
			user->nick, user->GetRealUserHost(), newnick);

		PunishUser(user);
		return MOD_RES_DENY;
	}

	void OnUserConnect(LocalUser* user) override
	{
		if (donotice)
			user->WriteNotice("*** You are not welcome on this network if you are a malicious bot. If you are not a malicious bot bot please ignore this message.");
	}
};

MODULE_INIT(ModuleAntiKnocker)

