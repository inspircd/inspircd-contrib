/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 genius3000 <genius3000@g3k.solutions>
 *   Copyright (C) 2016 Johanna Abrahamsson <johanna-a@mjao.org>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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

/* $ModAuthor: genius3000 */
/* $ModAuthorMail: genius3000@g3k.solutions */
/* $ModDesc: Provides extban 'n' - Connect class ban */
/* $ModDepends: core 2.0 */

/* Helpop Lines for the EXTBANS section
 * Find: '<helpop key="extbans" value="Extended Bans'
 * Place just before the 'r:<realname>' line:
 n:<class>     Matches users in a matching connect class (requires
               classban extras-module).
 */

#include "inspircd.h"


class ModuleClassBan : public Module
{
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnCheckBan(User* user, Channel* c, const std::string& mask)
	{
		if ((mask.length() > 2) && (mask[0] == 'n') && (mask[1] == ':'))
		{
			if (InspIRCd::Match(user->GetClass()->GetName(), mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::string& output)
	{
		ServerInstance->AddExtBanChar('n');
	}

	Version GetVersion()
	{
		return Version("Extban 'n' - connect class ban", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleClassBan)
