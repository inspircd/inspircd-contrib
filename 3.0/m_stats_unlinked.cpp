/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014-2016 Peter Powell <petpow@saberuk.com>
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

/// $ModAuthor: Peter "SaberUK" Powell
/// $ModAuthorMail: petpow@saberuk.com
/// $ModDepends: core 3.0
/// $ModDesc: Adds stats character 'X' which shows unlinked servers.


#include "inspircd.h"

class ModuleStatsUnlinked : public Module
{
 private:
	std::map<std::string, int> LinkableServers;

	static bool IsServerLinked(const ProtocolInterface::ServerList& linkedServers, const std::string& serverName)
	{
		for (ProtocolInterface::ServerList::const_iterator it = linkedServers.begin(); it != linkedServers.end(); ++it)
		{
			if (it->servername == serverName)
				return true;
		}
		return false;
	}

 public:
	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
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

	ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE
	{
		if (stats.GetSymbol() != 'X')
			return MOD_RES_PASSTHRU;

		ProtocolInterface::ServerList linkedServers;
		ServerInstance->PI->GetServerList(linkedServers);

		for (std::map<std::string, int>::iterator it = LinkableServers.begin(); it != LinkableServers.end(); ++it)
		{
			if (!IsServerLinked(linkedServers, it->first))
			{
				// ProtoServer does not have a port so we use the port from the config here.
				stats.AddRow(247, " X " + it->first + " " + ConvToStr(it->second));
			}
		}
		return MOD_RES_DENY;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds stats character 'X' which shows unlinked servers.");
	}
};

MODULE_INIT(ModuleStatsUnlinked)

