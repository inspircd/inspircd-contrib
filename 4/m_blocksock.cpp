/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Sadie Powell <sadie@witchery.services>
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
/// $ModDepends: core 4
/// $ModDesc: Allows blocking IP addresses from making any socket connections to the server.


#include "inspircd.h"

class ModuleBlockSock final
	: public Module
{
private:
	bool FindRange(const std::string& rangelist, const irc::sockets::sockaddrs& sa)
	{
		if (rangelist.empty())
			return false; // No ranges set.

		irc::spacesepstream rangestream(rangelist);
		const auto sastr = sa.str();
		for (std::string range; rangestream.GetToken(range); )
		{
			if (InspIRCd::Match(sastr, range, ascii_case_insensitive_map) || irc::sockets::cidr_mask(range).match(sa))
				return true; // Matches the range.
		}

		// No range matched.
		return false;
	}

public:
	ModuleBlockSock()
		: Module(VF_NONE, "Allows blocking IP addresses from making any socket connections to the server.")
	{
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnAcceptConnection, PRIORITY_FIRST);
	}

	ModResult OnAcceptConnection(int, ListenSocket* from, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server) override
	{
		const auto whiteliststr = from->bind_tag->getString("whitelist");
		if (!FindRange(whiteliststr, client))
		{
			const auto blackliststr = from->bind_tag->getString("blacklist");
			if (FindRange(blackliststr, client))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleBlockSock)
