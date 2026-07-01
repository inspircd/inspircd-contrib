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
/// $ModConfig: <vlink server="irc.legacy.example.org" parent="irc.example.org" hops="2" desc="Legacy network">
/// $ModDepends: core 4
/// $ModDesc: Shows cosmetic virtual servers (vlinks) in /LINKS.

// vlinks are fake server entries shown in /LINKS that don't correspond to a real
// linked server — handy for keeping a merged network's old domains visible after
// they've been standardised away. Each <vlink> tag adds one entry; they are
// appended to the end of the /LINKS list (just before RPL_ENDOFLINKS) and are
// purely cosmetic, so they never affect routing or appear anywhere else.

#include "inspircd.h"

namespace
{
	struct VLink final
	{
		std::string server; // virtual server name shown in /LINKS
		std::string parent; // server it is shown linked to
		std::string desc;   // its description
		unsigned long hops; // hop count shown next to it
	};
}

class ModuleVLink final
	: public Module
{
private:
	std::vector<VLink> vlinks;

public:
	ModuleVLink()
		: Module(VF_NONE, "Shows cosmetic virtual servers (vlinks) in /LINKS.")
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		std::vector<VLink> newlinks;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("vlink"))
		{
			VLink vl;
			vl.server = tag->getString("server");
			if (vl.server.empty() || vl.server.find(' ') != std::string::npos)
				throw ModuleException(this, "<vlink:server> (" + vl.server + ") is not a valid server name.");

			vl.parent = tag->getString("parent", ServerInstance->Config->GetServerName());
			if (vl.parent.find(' ') != std::string::npos)
				throw ModuleException(this, "<vlink:parent> (" + vl.parent + ") is not a valid server name.");

			vl.desc = tag->getString("desc", "Virtual link", 1);
			vl.hops = tag->getNum<unsigned long>("hops", 1, 1);
			newlinks.push_back(vl);
		}
		vlinks = std::move(newlinks);
	}

	ModResult OnNumeric(User* user, const Numeric::Numeric& numeric) override
	{
		// /LINKS always ends with RPL_ENDOFLINKS, so append the virtual servers
		// right before it — this works whether the list came from spanningtree or
		// the single-server stub, and naturally respects /LINKS hiding (maphide
		// denies the command before any numeric is sent).
		if (numeric.GetNumeric() != RPL_ENDOFLINKS || vlinks.empty())
			return MOD_RES_PASSTHRU;

		for (const auto& vl : vlinks)
			user->WriteNumeric(RPL_LINKS, vl.server, vl.parent, INSP_FORMAT("{} {}", vl.hops, vl.desc));

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleVLink)
