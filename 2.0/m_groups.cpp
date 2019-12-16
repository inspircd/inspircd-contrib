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

/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModAuthorMail: petpow@saberuk.com */
/* $ModDepends: core 2.0 */
/* $ModDesc: Allows users to be managed using services-assigned groups. */


#include "inspircd.h"

enum
{
	// InspIRCd specific.
	RPL_WHOISGROUPS = 695
};

// Represents a list of groups that a user is a member of.
typedef std::vector<std::string> GroupList;

class GroupExt : public ExtensionItem
{
 public:
	GroupExt(Module* Creator)
		: ExtensionItem("groups", Creator)
	{
	}

	void free(void* item)
	{
		delete static_cast<GroupList*>(item);
	}

	GroupList* get(const Extensible* container) const
	{
		return static_cast<GroupList*>(get_raw(container));
	}

	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		GroupList* grouplist = static_cast<GroupList*>(item);
		if (!grouplist)
			return "";

		std::string buffer;
		for (GroupList::const_iterator iter = grouplist->begin(); iter != grouplist->end(); ++iter)
		{
			if (!buffer.empty())
				buffer.push_back(' ');
			buffer.append(*iter);
		}
		return buffer;
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		// Create a new group list from the input.
		GroupList* newgrouplist = new GroupList();
		irc::spacesepstream groupstream(value);
		for (std::string groupname; groupstream.GetToken(groupname); )
			newgrouplist->push_back(groupname);

		if (newgrouplist->empty())
		{
			// If the new group list is empty then delete both the new and old group lists.
			void* oldgrouplist = unset_raw(container);
			free(oldgrouplist);
			free(newgrouplist);
		}
		else
		{
			// Otherwise install the new group list.
			void* oldgrouplist = set_raw(container, newgrouplist);
			free(oldgrouplist);
		}
	}
};

class ModuleGroups : public Module
{
 private:
	GroupExt ext;

 public:
	ModuleGroups()
		: ext(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(ext);
		Implementation eventlist[] = { I_OnCheckBan, I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnCheckBan(User* user, Channel* channel, const std::string& mask)
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

	void OnWhois(User* source, User* dest)
	{
		GroupList* groups = ext.get(dest);
		if (groups)
		{
			const std::string groupstr = irc::stringjoiner(" ", *groups, 0, groups->size() - 1).GetJoined();
			ServerInstance->SendWhoisLine(source, dest, RPL_WHOISGROUPS, "%s %s %s :is a member of these groups",
				source->nick.c_str(), dest->nick.c_str(), groupstr.c_str());
		}
	}

	Version GetVersion()
	{
		return Version("Allows users to be managed using services-assigned groups", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleGroups)
