/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Peter Powell <petpow@saberuk.com>
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

/// $ModAuthor: Peter "SaberUK" Powell
/// $ModAuthorMail: petpow@saberuk.com
/// $ModDepends: core 3
/// $ModDesc: Allows users to be managed using services-assigned groups.


#include "inspircd.h"
#include "modules/whois.h"

enum
{
	// InspIRCd specific.
	RPL_WHOISGROUPS = 695
};

// Represents a list of groups that a user is a member of.
typedef insp::flat_set<std::string> GroupList;

class GroupExt : public SimpleExtItem<GroupList>
{
 public:
	GroupExt(Module* Creator)
		: SimpleExtItem<GroupList>("groups", ExtensionItem::EXT_USER, Creator)
	{
	}

	std::string ToNetwork(const Extensible* container, void* item) const CXX11_OVERRIDE
	{
		GroupList* grouplist = static_cast<GroupList*>(item);
		return grouplist ? stdalgo::string::join(*grouplist) : "";
	}

	void FromNetwork(Extensible* container, const std::string& value) CXX11_OVERRIDE
	{
		// Create a new group list from the input.
		GroupList* newgrouplist = new GroupList();
		irc::spacesepstream groupstream(value);
		for (std::string groupname; groupstream.GetToken(groupname); )
			newgrouplist->insert(groupname);

		if (newgrouplist->empty())
		{
			// If the new group list is empty then delete both the new and old group lists.
			unset(container);
			delete newgrouplist;
		}
		else
		{
			// Otherwise install the new group list.
			set(container, newgrouplist);
		}
	}
};

class ModuleGroups
	: public Module
	, public Whois::EventListener
{
 private:
	GroupExt ext;

 public:
	ModuleGroups()
		: Whois::EventListener(this)
		, ext(this)
	{
	}

	ModResult OnCheckBan(User* user, Channel* channel, const std::string& mask) CXX11_OVERRIDE
	{
		if (mask.length() <= 2 || mask[0] != 'g' || mask[1] != ':')
			return MOD_RES_PASSTHRU;

		GroupList* groups = ext.get(user);
		if (!groups)
			return MOD_RES_PASSTHRU;

		const std::string submask = mask.substr(2);
		for (GroupList::const_iterator iter = groups->begin(); iter != groups->end(); ++iter)
		{
			if (InspIRCd::Match(*iter, submask))
				return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
	{
		GroupList* groups = ext.get(whois.GetTarget());
		if (groups)
		{
			const std::string groupstr = stdalgo::string::join(*groups);
			whois.SendLine(RPL_WHOISGROUPS, groupstr, "is a member of these groups");
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows users to be managed using services-assigned groups");
	}
};

MODULE_INIT(ModuleGroups)
