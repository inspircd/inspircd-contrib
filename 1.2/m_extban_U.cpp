/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Shawn Smith <shawn@inspircd.org>
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

/* $ModAuthor: Shawn Smith */
/* $ModAuthorMail: shawn@inspircd.org */
/* $ModDepends: core 1.2-1.3 */
/* $ModDependsModule: m_services_account */
/* $ModDesc: Adds ExtBan +U to match ONLY unregistered users */

#include "inspircd.h"

class ExtBanU : public Module
{
	public:
		ExtBanU(Inspircd* Me) : Module(Me) { }

		virtual void On005Numeric(std::string &t)
		{
			ServerInstance->AddExtBanChar('U');
		}

		virtual int OnCheckBan(User *u, Channel *c)
		{
			// User is registered
			if (u->GetExt("accountname"))
				return 0;

			// If the user matches extban U.
			if (c->GetExtBanStatus(u, 'U')
				return 1;
		}

		virtual ~ExtBanU()
		{
		}

		virtual Version GetVersion()
		{
			return Version("$ID$", VF_COMMON, API_VERSION);
		}
};
