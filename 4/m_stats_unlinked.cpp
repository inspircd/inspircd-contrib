/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014-2016 Sadie Powell <sadie@witchery.services>
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
/// $ModDepends: core 4
/// $ModDesc: Adds stats character 'X' which shows unlinked servers.


#include "inspircd.h"
#include "modules/stats.h"

class ModuleStatsUnlinked final
	: public Module
	, public Stats::EventListener
{
private:
	std::map<std::string, unsigned int> LinkableServers;

	static bool IsServerLinked(const ProtocolInterface::ServerList& linkedServers, const std::string& serverName)
	{
		for (const auto& linkedServer : linkedServers)
		{
			if (linkedServer.servername == serverName)
				return true;
		}
		return false;
	}

public:
	ModuleStatsUnlinked()
		: Module(VF_NONE, "Adds stats character 'X' which shows unlinked servers.")
		, Stats::EventListener(this)
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		LinkableServers.clear();

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("link"))
		{
			std::string serverName = tag->getString("name");
			in_port_t serverPort = tag->getNum<in_port_t>("port", 0);
			if (!serverName.empty() && serverName.size() <=  ServerInstance->Config->Limits.MaxHost && serverName.find('.') != std::string::npos && serverPort)
			{
				// There is currently no way to prioritize the init() function so we
				// reimplement the checks from m_spanningtree here.
				LinkableServers[serverName] = serverPort;
			}
		}
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() != 'X')
			return MOD_RES_PASSTHRU;

		ProtocolInterface::ServerList linkedServers;
		ServerInstance->PI->GetServerList(linkedServers);

		for (std::map<std::string, unsigned int>::const_iterator it = LinkableServers.begin(); it != LinkableServers.end(); ++it)
		{
			if (!IsServerLinked(linkedServers, it->first))
			{
				// ProtoServer does not have a port so we use the port from the config here.
				stats.AddRow(247, " X " + it->first + " " + ConvToStr(it->second));
			}
		}
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleStatsUnlinked)

