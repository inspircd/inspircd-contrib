/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Sadie Powell <sadie@witchery.services>
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
/* $ModDesc: Adds stats character 'X' which shows unlinked servers. */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleStatsUnlinked : public Module
{
 private:
	 std::map<std::string, int> LinkableServers;

	 static bool IsServerLinked(const ProtoServerList& linkedServers, const std::string& serverName)
	 {
		 for (ProtoServerList::const_iterator it = linkedServers.begin(); it != linkedServers.end(); ++it)
		 {
			 if (it->servername == serverName)
				 return true;
		 }
		 return false;
	 }

 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventList[] = { I_OnRehash, I_OnStats };
		ServerInstance->Modules->Attach(eventList, this, sizeof(eventList)/sizeof(Implementation));
	}

	void OnRehash(User*)
	{
		LinkableServers.clear();

		ConfigTagList tags = ServerInstance->Config->ConfTags("link");
		for (ConfigIter it = tags.first; it != tags.second; ++it)
		{
			std::string serverName = it->second->getString("name");
			int serverPort = it->second->getInt("port");
			if (!serverName.empty() && serverName.size() <= 64 && serverName.find('.') != std::string::npos && serverPort)
			{
				// There is currently no way to prioritize the init() function so we
				// reimplement the checks from m_spanningtree here.
				LinkableServers[serverName] = serverPort;
			}
		}
	}

	ModResult OnStats(char symbol, User* user, string_list& results)
	{
		if (symbol != 'X')
			return MOD_RES_PASSTHRU;

		ProtoServerList linkedServers;
		ServerInstance->PI->GetServerList(linkedServers);

		for (std::map<std::string, int>::iterator it = LinkableServers.begin(); it != LinkableServers.end(); ++it)
		{
			if (!IsServerLinked(linkedServers, it->first))
			{
				// ProtoServer does not have a port so we use the port from the config here.
				results.push_back(ServerInstance->Config->ServerName + " 247 " + user->nick + " X " + it->first + " " + ConvToStr(it->second));
			}
		}
		return MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Adds stats character 'X' which shows unlinked servers.");
	}
};

MODULE_INIT(ModuleStatsUnlinked)

