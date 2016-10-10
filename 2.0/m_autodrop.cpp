/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Peter Powell <petpow@saberuk.com>
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


/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModAuthorMail: petpow@saberuk.com */
/* $ModDesc: Allows clients to be automatically dropped if they execute certain commands before registration. */
/* $ModDepends: core 2.0 */
/* $ModConfig: <autodrop commands="CONNECT DELETE GET HEAD OPTIONS PATCH POST PUT TRACE"> */

#include "inspircd.h"

class ModuleAutoDrop : public Module
{
 private:
	std::vector<std::string> Commands; 

 public:
	void init()
	{
		Implementation eventList[] = { I_OnPreCommand, I_OnRehash };
		ServerInstance->Modules->Attach(eventList, this, sizeof(eventList)/sizeof(Implementation));
		OnRehash(NULL);
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnPreCommand, PRIORITY_FIRST);
	}

	void OnRehash(User*)
	{
		Commands.clear();

		ConfigTag* tag = ServerInstance->Config->ConfValue("autodrop");
		std::string commandList = tag->getString("commands", "CONNECT DELETE GET HEAD OPTIONS PATCH POST PUT TRACE");

		irc::spacesepstream stream(commandList);
		std::string token;
		while (stream.GetToken(token))
		{
			Commands.push_back(token);
		}
	}

	ModResult OnPreCommand(std::string& command, std::vector<std::string>&, LocalUser* user, bool, const std::string&)
	{
		if (user->registered == REG_ALL || std::find(Commands.begin(), Commands.end(), command) == Commands.end())
			return MOD_RES_PASSTHRU;

		user->eh.SetError("Dropped by " + ModuleSourceFile);
		return MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Allows clients to be automatically dropped if they execute certain commands before registration.");
	}
};

MODULE_INIT(ModuleAutoDrop)
