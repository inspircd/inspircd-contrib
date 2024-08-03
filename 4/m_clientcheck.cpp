/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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
/// $ModDesc: Allows detection of clients by version string.
/// $ModDepends: core 4

#include "inspircd.h"
#include "clientprotocolmsg.h"
#include "extension.h"
#include "modules/regex.h"

enum ClientAction
{
	// Kill clients that match the check.
	CA_KILL,

	// Send a NOTICE to clients that match the check.
	CA_NOTICE,

	// Send a PRIVMSG to clients that match the check.
	CA_PRIVMSG
};

struct ClientInfo final
{
	// The action to take against a client that matches this action.
	ClientAction action;

	// The message to give when performing the action.
	std::string message;

	// A regular expression which matches a client version string.
	Regex::PatternPtr pattern;
};

class ModuleClientCheck final
	: public Module
{
private:
	BoolExtItem ext;
	std::vector<ClientInfo> clients;
	Regex::EngineReference rf;
	std::string origin;
	std::string originnick;

public:
	ModuleClientCheck()
		: Module(VF_NONE, "Allows detection of clients by version string.")
		, ext(this, "checking-client-version", ExtensionType::USER)
		, rf(this, "regex")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& clientcheck = ServerInstance->Config->ConfValue("clientcheck");

		const std::string engine = clientcheck->getString("engine");
		Regex::EngineReference newrf(this, engine.empty() ? "regex": "regex/" + engine);
		if (!newrf)
			throw ModuleException(this, "<clientcheck:engine> (" + engine + ") is not a recognised regex engine.");

		const std::string neworigin = clientcheck->getString("origin", ServerInstance->Config->ServerName);
		if (neworigin.empty() || neworigin.find(' ') != std::string::npos)
			throw ModuleException(this, "<clientcheck:origin> (" + neworigin + ") is not a valid nick!user@host mask.");

		std::vector<ClientInfo> newclients;

		for (const auto& [_, tag] :  ServerInstance->Config->ConfTags("clientmatch"))
		{
			ClientInfo ci;
			ci.message = tag->getString("message");

			ci.action = tag->getEnum("action", CA_PRIVMSG, {
				{ "kill",    CA_KILL    },
				{ "notice",  CA_NOTICE  },
				{ "privmsg", CA_PRIVMSG },
			});

			const std::string actionstr = tag->getString("action", "privmsg", 1);
			try
			{
				ci.pattern = newrf->Create(tag->getString("pattern"));
			}
			catch (const Regex::Exception& err)
			{
				throw ModuleException(this, "<clientmatch:pattern> is not a well formed regular expression: " + err.GetReason());
			}

			ServerInstance->Logs.Debug(MODNAME, "Client check: {} -> {} ({})",
				ci.pattern->GetPattern(), (int)ci.action, ci.message);
			newclients.push_back(ci);
		}

		rf.SetProvider(newrf.GetProvider());
		std::swap(clients, newclients);
		origin = neworigin;
		originnick = neworigin.substr(0, origin.find('!'));
	}

	void OnUserConnect(LocalUser* user) override
	{
		ext.Set(user);

		ClientProtocol::Messages::Privmsg msg(origin, user, "\x1VERSION\x1", MessageType::PRIVMSG);
		user->Send(ServerInstance->GetRFCEvents().privmsg, msg);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		if (validated || !ext.Get(user) || !rf)
			return MOD_RES_PASSTHRU;

		if (command != "NOTICE" || parameters.size() < 2)
			return MOD_RES_PASSTHRU;

		if (parameters[0] != originnick)
			return MOD_RES_PASSTHRU;

		if (parameters[1].length() < 10 || parameters[1][0] != '\x1')
			return MOD_RES_PASSTHRU;

		const std::string prefix = parameters[1].substr(0, 9);
		if (!irc::equals(prefix, "\1VERSION "))
			return MOD_RES_PASSTHRU;

		size_t msgsize = parameters[1].size();
		size_t lastpos = msgsize - (parameters[1][msgsize - 1] == '\x1' ? 9 : 10);

		const std::string versionstr = parameters[1].substr(9, lastpos);
		for (const auto& ci : clients)
		{
			if (!ci.pattern->Matches(versionstr))
				continue;

			switch (ci.action)
			{
				case CA_KILL:
				{
					ServerInstance->Users.QuitUser(user, ci.message);
					break;
				}
				case CA_NOTICE:
				{
					ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy,
						origin, user, ci.message, MessageType::NOTICE);
					user->Send(ServerInstance->GetRFCEvents().privmsg, msg);
					break;
				}
				case CA_PRIVMSG:
				{
					ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy,
						origin, user, ci.message, MessageType::PRIVMSG);
					user->Send(ServerInstance->GetRFCEvents().privmsg, msg);
					break;
				}
			}
			break;
		}

		ext.Unset(user);
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleClientCheck)
