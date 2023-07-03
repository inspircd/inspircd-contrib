/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
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
/// $ModDesc: Provides the /GEOLOCATE command which performs Geolocation lookups on arbitrary IP addresses.
/// $ModDepends: core 4


#include "inspircd.h"
#include "modules/geolocation.h"

class CommandGeolocate final
	: public SplitCommand
{
private:
	Geolocation::API geoapi;

public:
	CommandGeolocate(Module* Creator)
		: SplitCommand(Creator, "GEOLOCATE", 1)
		, geoapi(Creator)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<ipaddr> [<ipaddr>]+" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		irc::sockets::sockaddrs sa;

		for (const auto& address : parameters)
		{
			// Try to parse the address.
			if (!sa.from_ip(address))
			{
				user->WriteNotice("*** GEOLOCATE: " + address + " is not a valid IP address!");
				continue;
			}

			// Try to geolocate the IP address.
			Geolocation::Location* location = geoapi ? geoapi->GetLocation(sa) : nullptr;
			if (!location)
			{
				user->WriteNotice("*** GEOLOCATE: " + sa.addr() + " could not be geolocated!");
				continue;
			}

			user->WriteNotice(INSP_FORMAT("*** GEOLOCATE: {} is located in {} ({}).",
				sa.addr(), location->GetName(), location->GetCode()));
		}
		return CmdResult::SUCCESS;
	}
};

class ModuleGeoCommand
	: public Module
{
private:
	CommandGeolocate cmd;

public:
	ModuleGeoCommand()
		: Module(VF_NONE, "Provides the /GEOLOCATE command which performs Geolocation lookups on arbitrary IP addresses.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleGeoCommand)
