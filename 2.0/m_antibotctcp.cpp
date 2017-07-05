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
/* $ModConfig: <antibotctcp ctcp="VERSION" msgonreply="true" showlogs="true" accepted="Howdy buddy,you are authorized to use this server!"  declined="You have been blocked!Please get a better client."> */
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
	LocalIntExt ext;
	bool msgonreply;
	bool showlogs;
	std::string accepted;
	std::string declined;
	std::string ctcp;
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
		showlogs = tag->getBool("showlogs", true);
		msgonreply = tag->getBool("msgonreply", true);
		declined = tag->getString("declined");
		accepted = tag->getString("accepted");
	}

	ModResult OnUserRegister(LocalUser* user)
	{
		ConfigTag* tag = user->MyClass->config;
		if (tag->getBool("antibotctcp", true))
		{
			if (ctcp.empty())
			{
				ctcp = "VERSION";
			}
			user->WriteServ("PRIVMSG %s :\001%s\001", user->nick.c_str(), ctcp.c_str());
			ext.set(user, 1);
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser* user, bool validated, const std::string &original_line)
	{
		if (command == "NOTICE" && !validated && parameters.size() > 1 && ext.get(user))
		{
			if (parameters[1].size() > 1 && parameters[1][0] == 0x1 && parameters[1].compare(1, ctcp.length(), ctcp) == 0)
			{
				ext.set(user, 0);
				if (msgonreply)
				{
					if (accepted.empty())
					{
						user->WriteServ("NOTICE " + user->nick + " :*** Howdy buddy,you are authorized to use this server!");
					}
					else
					{
						user->WriteServ("NOTICE " + user->nick + " :*** " + accepted);
					}
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
			if (msgonreply)
			{
				if (declined.empty())
				{
					ServerInstance->Users->QuitUser(user, "You have been blocked!Please get a better client.");
				}
				else
				{
					ServerInstance->Users->QuitUser(user, declined);
				}
			}
			else
			{
				ServerInstance->Users->QuitUser(user, "Disconnected");
			}
			if (showlogs)
			{
				ServerInstance->SNO->WriteGlobalSno('a', "Suspicious connection on port %d (class %s) from %s (%s) was blocked by m_antibotctcp", user->GetServerPort(), user->MyClass->name.c_str(), user->GetFullRealHost().c_str(), user->GetIPString());
			}
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};
MODULE_INIT(ModuleAntiBotCTCP)
