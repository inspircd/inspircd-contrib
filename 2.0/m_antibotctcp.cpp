/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *       Copyright (C) Nikos `UrL` Papakonstantinou <url.euro@gmail.com>
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
 * along with this program.      If not, see <http://www.gnu.org/licenses/>.
 */
#include "inspircd.h"
/* $ModConfig: <antibotctcp ctcp="VERSION" quitmsg="true" msgonreply="true" msg="If you are having problems connecting to this server, please get a better client."> */
/* $ModDesc: Blocks clients not replying to CTCP like botnets/spambots/floodbots. */
/* $ModAuthor: Nikos `UrL` Papakonstantinou */
/* $ModAuthorMail: url@mirc.com.gr */
/* $ModDepends: core 2.0 */

/*
 * Supports connect class param
 * Add antibotctcp="true" which enables or disables the module on that port(s).
 */

class ModuleAntiBotCTCP : public Module
{
	bool quitmsg;
	bool msgonreply;
	LocalIntExt ext;
	std::string blockmsg;
	std::string ctcp;
	std::string tmp;
 public:
	ModuleAntiBotCTCP()
	: ext("ctcptime_wait", this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(ext);
		Implementation eventlist[] = { I_OnUserRegister, I_OnPreCommand, I_OnCheckReady, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	Version GetVersion()
	{
		return Version("Blocks clients not replying to CTCP.");
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("antibotctcp");
		ctcp = tag->getString("ctcp");
		if(ctcp.empty())
		{
			ServerInstance->SNO->WriteGlobalSno('a', "m_antibotctcp: Invalid ctcp value in config: %s", ctcp.c_str());
			ServerInstance->Logs->Log("m_antibotctcp",DEFAULT, "m_antibotctcp: Invalid ctcp value in config: %s", ctcp.c_str());
		}
		else
		{
			tmp = "\001" + ctcp      + " ";
		}
		quitmsg = tag->getBool("quitmsg", true);
		msgonreply = tag->getBool("msgonreply", true);
		blockmsg = tag->getString("msg");
	}

	ModResult OnUserRegister(LocalUser* user)
	{
		ConfigTag* tag = user->MyClass->config;
		if (tag->getBool("antibotctcp", true))
		{
			user->WriteNumeric(931, "%s :Malicious or potentially unwanted softwares are not WELCOME here!", user->nick.c_str());
			user->WriteServ("PRIVMSG %s :\001%s\001", user->nick.c_str(), ctcp.c_str());
			ext.set(user, 1);
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser* user, bool validated, const std::string &original_line)
	{
		if (command == "NOTICE" && !validated && parameters.size() > 1 && ext.get(user))
		{
			if (parameters[1].compare(0, tmp.length(), tmp) == 0)
			{
				ext.set(user, 0);
				if (msgonreply)
				{
					user->WriteServ("NOTICE " + user->nick + " :*** Howdy buddy,you are authorized to use this server!");
				}
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		if (ext.get(user))
		{
			if (quitmsg)
			{
				ServerInstance->Users->QuitUser(user, blockmsg);
			}
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};
MODULE_INIT(ModuleAntiBotCTCP)
