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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDesc: Attempts to block a common IRC spambot.
/// $ModDepends: core 4


#include <regex>

#include "inspircd.h"
#include "extension.h"
#include "modules/shun.h"

enum
{
	// From RFC 1459.
	ERR_NICKNAMEINUSE = 433
};

class ModuleAntiKnocker final
	: public Module
{
public:
	std::regex nickregex;
	IntExtItem seenlist;
	unsigned long shunduration;
	std::string shunreason;

	ModuleAntiKnocker()
		: Module(VF_NONE, "Attempts to block a common IRC spambot.")
		, seenlist(this, "seenlist", ExtensionType::USER)
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

		shunduration = tag->getDuration("shunduration", 60*60, 60);
		shunreason = tag->getString("shunreason", "User was caught in an antiknock trap", 1);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		if (!validated || !user->IsFullyConnected())
			return MOD_RES_PASSTHRU;

		if (command == "LIST")
		{
			seenlist.Set(user, ServerInstance->Time());
			return MOD_RES_PASSTHRU;
		}

		if (command == "PRIVMSG" && irc::equals(parameters[0], "NickServ"))
		{
			time_t whenlist = seenlist.Get(user);
			if (ServerInstance->Time() - whenlist <= 3)
			{
				// This is almost certainly a bot; punish them!
				ServerInstance->SNO.WriteToSnoMask('a', "User {} ({}) was caught in an knocker trap!",
						user->nick, user->GetRealUserHost());

				auto* sh = new Shun(ServerInstance->Time(), shunduration, MODNAME "@" + ServerInstance->Config->ServerName, shunreason, user->GetAddress());
				if (ServerInstance->XLines->AddLine(sh, nullptr))
				{
					ServerInstance->XLines->ApplyLines();
					return MOD_RES_DENY;
				}

				// No shunning? Annoying. Just quit em.
				delete sh;

				const std::string message = INSP_FORMAT("Ping timeout: {} seconds", user->GetClass()->pingtime);
				ServerInstance->Users.QuitUser(user, message);
				return MOD_RES_DENY;
			}
		}

		seenlist.Unset(user);
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) override
	{
		if (!std::regex_match(newnick, nickregex))
			return MOD_RES_PASSTHRU;

		ServerInstance->SNO.WriteToSnoMask('a', "User {} ({}) was prevented from using a knocker nick: {}",
			user->nick, user->GetRealUserHost(), newnick);
		user->WriteNumeric(ERR_NICKNAMEINUSE, newnick, "Nickname is already in use.");
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleAntiKnocker)

