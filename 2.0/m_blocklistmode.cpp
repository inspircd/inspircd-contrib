/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Daniel Vassdal <shutter@canternet.org>
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


/* $ModAuthor: Daniel Vassdal */
/* $ModAuthorMail: shutter@canternet.org */
/* $ModDesc: Allow requiring channel membership to see listmode lists */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleBlockListMode : public Module
{
	std::string blockmodes;
public:
	ModuleBlockListMode()
	{
	};

	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRawMode, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist) / sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Allow requiring channel membership to see listmode lists");
	}

	void OnRehash(User* user)
	{
		blockmodes = ServerInstance->Config->ConfValue("blocklistmode")->getString("modes", "b");
	}

	ModResult OnRawMode(User* source, Channel* channel, const char mode, const std::string& parameter, bool adding, int pcnt)
	{
		if (!channel)
			return MOD_RES_PASSTHRU;

		if (!IS_LOCAL(source))
			return MOD_RES_PASSTHRU;

		if (!adding || pcnt)
			return MOD_RES_PASSTHRU;

		if (blockmodes.find_first_of(mode) == std::string::npos || channel->HasUser(source))
			return MOD_RES_PASSTHRU;

		source->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You do not have access to view the +%c list", source->nick.c_str(), channel->name.c_str(), mode);
		return MOD_RES_DENY;
	}

};

MODULE_INIT(ModuleBlockListMode)
