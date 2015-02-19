/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Miguel Peláez <miguel2706@outlook.com>
 *   Copyright (C) 2014-2015 Hira.io Team <staff@hira.io>
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


/* $ModAuthor: Hira.io Team */
/* $ModAuthorMail: miguel2706@outlook.com */
/* $ModDesc: Get the bans from other channel. Usage: /ban J:#channel */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
class ModuleGetBans : public Module
{
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}
	Version GetVersion()
	{
		return Version("Extban 'J': Get the bans from other channel.", VF_COMMON);
	}

	ModResult OnCheckBan(User *user, Channel *channel, const std::string& mask)
	{
		if (!IS_LOCAL(user))
                        return MOD_RES_PASSTHRU;

		if ((mask.length() > 2) && (mask[0] == 'J') && (mask[1] == ':'))
		{
			// Channel to get the bans
			const std::string& chan_ = mask.substr(2);
			Channel* chan = ServerInstance->FindChan (chan_);
			if(!chan)
				return MOD_RES_PASSTHRU;
			for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
		        {
				if ((i->data[0] != 'J' && i->data[1] != ':') && (chan->CheckBan(user, i->data)))
		                        return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
	void On005Numeric(std::string& output)
	{
		ServerInstance->AddExtBanChar('J');
	}
};

MODULE_INIT(ModuleGetBans)
