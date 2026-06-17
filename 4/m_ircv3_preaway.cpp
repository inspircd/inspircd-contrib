/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 reverse <mike.chevronnet@gmail.com>
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

/// $ModAuthor: reverse <mike.chevronnet@gmail.com>
/// $ModDepends: core 4
/// $ModDesc: Provides the DRAFT pre-away IRCv3 extension.


#include "inspircd.h"
#include "modules/cap.h"

class ModuleIRCv3PreAway final
	: public Module
{
private:
	Cap::Capability cap;

public:
	ModuleIRCv3PreAway()
		: Module(VF_VENDOR, "Provides the IRCv3 draft/pre-away client capability.")
		, cap(this, "draft/pre-away")
	{
		// CommandAway already sets works_before_reg, so a pre-away client can send
		// AWAY mid-registration as-is; this just advertises that support.
	}
};

MODULE_INIT(ModuleIRCv3PreAway)
