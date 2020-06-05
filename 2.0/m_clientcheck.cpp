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


/* $ModAuthor: Sadie Powell */
/* $ModAuthorMail: sadie@witchery.services */
/* $ModDesc: Allows detection of clients by version string. */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
#include "m_regex.h"

enum ClientAction
{
	// Kill clients that match the check.
	CA_KILL,

	// Send a NOTICE to clients that match the check.
	CA_NOTICE,

	// Send a PRIVMSG to clients that match the check.
	CA_PRIVMSG
};

struct ClientInfo
{
	// The action to take against a client that matches this action.
	ClientAction action;

	// The message to give when performing the action.
	std::string message;

	// A regular expression which matches a client version string.
	Regex* pattern;
};

class ModuleClientCheck : public Module
{
 private:
	LocalIntExt ext;
	std::vector<ClientInfo> clients;
	dynamic_reference<RegexFactory> rf;

 public:
	ModuleClientCheck()
		: ext("checking-client-version", this)
		, rf(this, "regex")
	{
	}

	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnUserConnect, I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist) / sizeof(Implementation));
	}

	void OnRehash(User* user)
	{
		const std::string engine = ServerInstance->Config->ConfValue("clientcheck")->getString("engine");
		dynamic_reference<RegexFactory> newrf(this, engine.empty() ? "regex": "regex/" + engine);
		if (!newrf)
			throw ModuleException("<clientcheck:engine> (" + engine + ") is not a recognised regex engine.");

		std::vector<ClientInfo> newclients;
		ConfigTagList tags = ServerInstance->Config->ConfTags("clientmatch");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;

			ClientInfo ci;
			ci.message = tag->getString("message");

			const irc::string actionstr = tag->getString("action", "privmsg").c_str();
			if (actionstr == "kill")
				ci.action = CA_KILL;
			else if (actionstr == "notice")
				ci.action = CA_NOTICE;
			else if (actionstr == "privmsg")
				ci.action = CA_PRIVMSG;
			else
				throw ModuleException("<clientmatch:action>; must be set to one of 'gline', 'kill', 'notice', or 'privmsg'.");

			try
			{
				ci.pattern = newrf->Create(tag->getString("pattern"));
			}
			catch (const ModuleException& err)
			{
				throw ModuleException("<clientmatch:pattern> is not a well formed regular expression.");
			}

			ServerInstance->Logs->Log("m_clientcheck", DEFAULT, "Client check: %s -> %s (%s)",
				ci.pattern->GetRegexString().c_str(),
				actionstr.c_str(), ci.message.c_str());
			newclients.push_back(ci);
		}

		rf.SetProvider(newrf.GetProvider());
		std::swap(clients, newclients);

}

	void OnUserConnect(LocalUser* user)
	{
		ext.set(user, 1);
		user->WriteServ("PRIVMSG %s :\x1VERSION\x1", user->nick.c_str());
	}

	ModResult OnPreCommand(std::string& command, std::vector<std::string>& parameters, LocalUser* user, bool validated, const std::string& original_line)
	{
		if (validated || !ext.get(user) || !rf)
			return MOD_RES_PASSTHRU;

		if (command != "NOTICE" || parameters.size() < 2)
			return MOD_RES_PASSTHRU;

		if (parameters[0] != ServerInstance->Config->ServerName)
			return MOD_RES_PASSTHRU;

		if (parameters[1].length() < 10 || parameters[1][0] != '\x1')
			return MOD_RES_PASSTHRU;

		const irc::string prefix = assign(parameters[1].substr(0, 9));
		if (prefix !=  "\1VERSION ")
			return MOD_RES_PASSTHRU;

		size_t msgsize = parameters[1].size();
		size_t lastpos = msgsize - (parameters[1][msgsize - 1] == '\x1' ? 9 : 10);

		const std::string version = parameters[1].substr(9, lastpos);
			for (std::vector<ClientInfo>::const_iterator iter = clients.begin(); iter != clients.end(); ++iter)
			{
				const ClientInfo& ci = *iter;
				if (!ci.pattern->Matches(version))
					continue;

				switch (ci.action)
				{
					case CA_KILL:
						ServerInstance->Users->QuitUser(user, ci.message);
						break;
					case CA_NOTICE:
						user->WriteServ("NOTICE %s :%s", user->nick.c_str(), ci.message.c_str());
						break;
					case CA_PRIVMSG:
						user->WriteServ("PRIVMSG %s :%s", user->nick.c_str(), ci.message.c_str());
						break;
				}
				break;
			}

		ext.set(user, 0);
		return MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Allows detection of clients by version string.");
	}
};

MODULE_INIT(ModuleClientCheck)
