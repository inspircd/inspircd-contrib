/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Manuel Leiner <satmd@euirc.net>
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


#include "inspircd.h"

/* $ModAuthor: Manuel Leiner */
/* $ModAuthorMail: satmd@euirc.net */
/* $ModDesc: Implement JOIN 0 (joining 0 makes a user part all channels), RFC2812 */
/* $ModDepends: core 2.0 */

class ModuleJoinZero : public Module
{
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		if (validated && command == "JOIN" && (parameters.size() == 1) && parameters[0] == "0")
		{
			UserChanList cl(user->chans);
			for(UCListIter ci=cl.begin();ci != cl.end(); ++ci) {
				std::string reason("Left all channels");
				(*ci)->PartUser(user, reason);
			}
			return MOD_RES_DENY;
		}
		else
			return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Implement JOIN 0 (joining 0 makes a user part all channels), RFC2812");
	}
};

MODULE_INIT(ModuleJoinZero)
