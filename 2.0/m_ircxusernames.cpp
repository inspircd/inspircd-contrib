/*
* InspIRCd -- Internet Relay Chat Daemon
*
* Copyright (C) 2012 SimosNap IRC Network <staff@simosnap.org>
*
* This file is part of InspIRCd. InspIRCd is free software: you can
* redistribute it and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation, version 2.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
* details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/* $ModAuthor: SimosNap IRC Network */
/* $ModDesc: Provides support for IRCX usernames */
/* $ModDepends: core 2.0-2.1 */

#include "inspircd.h"

class ModuleIRCXUsernames : public Module
{
	public: ModuleIRCXUsernames()
	{
		Implementation eventlist[] = { I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser* user, bool validated, const std::string &original_line)
	{
		size_t position = std::string::npos;

		if (validated && !(user->registered & REG_USER) && (command == "USER"))
		{
			if (!parameters.empty() && (position = parameters[0].find("@", 0)) != std::string::npos)
			{
				std::string username = parameters[0].substr(0, position);
				ServerInstance->SNO->WriteGlobalSno('a', "IRCX USERNAME: changing invalid username \""+parameters[0]+"\" to \""+username+"\"");
				parameters[0] = username;
			}
		}
		return MOD_RES_PASSTHRU;
	}


	~ModuleIRCXUsernames()
	{
	}

	Version GetVersion()
	{
		return Version("Provides support for IRCX usernames", VF_VENDOR);
	}

};

MODULE_INIT(ModuleIRCXUsernames)
