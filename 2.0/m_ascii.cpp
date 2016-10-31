/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Adam <Adam@anope.org>
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

/* $ModAuthor: Adam */
/* $ModAuthorMail: Adam@anope.org */
/* $ModDesc: Implements CASEMAPPING=ascii */
/* $ModDepends: core 2.0 */
/* $ModConflicts: m_nationalchars.so */

class ModuleASCII : public Module
{
 public:
	void init()
	{
		national_case_insensitive_map = ascii_case_insensitive_map;

		Implementation eventlist[] = { I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	~ModuleASCII()
	{
		national_case_insensitive_map = rfc_case_insensitive_map;
	}

	Version GetVersion()
	{
		return Version("Implements CASEMAPPING=ascii", VF_COMMON);
	}

	void On005Numeric(std::string &output)
	{
		SearchAndReplace<std::string>(output, "CASEMAPPING=rfc1459", "CASEMAPPING=ascii");
	}

};

MODULE_INIT(ModuleASCII)
