/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Sadie Powell <sadie@witchery.services>
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


/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDesc: Prevents clients from sending messages that trigger CVE-2022-2663.
/// $ModDepends: core 3

#include "inspircd.h"

class ModuleCVE20222663 : public Module
{
 private:
	std::string find;
	std::string replace;

 public:
	ModuleCVE20222663()
		: find("\x1")
	{
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		std::string ctcpname;
		std::string ctcpbody;
		if (!details.IsCTCP(ctcpname, ctcpbody))
			return MOD_RES_PASSTHRU; // Not a CTCP.

		stdalgo::string::replace_all(ctcpname, find, replace);
		if (ctcpname.empty())
			return MOD_RES_DENY; // Malformed CTCP.

		details.text.clear();
		details.text.push_back('\x1');
		details.text.append(ctcpname);
		stdalgo::string::replace_all(ctcpbody, find, replace);
		if (!ctcpbody.empty())
		{
			details.text.push_back(' ');
			details.text.append(ctcpbody);
		}
		details.text.push_back('\x1');

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Prevents clients from sending messages that trigger CVE-2022-2663.");
	}
};

MODULE_INIT(ModuleCVE20222663)

