/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
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
typedef insp::flat_set<std::string, irc::insensitive_swo> GroupList;

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
	bool active;
	GroupExt ext;
	std::string groupchar;

	size_t ExecuteCommand(LocalUser* source, const char* cmd, CommandBase::Params& parameters,
		const std::string& group, size_t nickindex)
	{
		size_t targets = 0;
		std::string command(cmd);
		const user_hash& users = ServerInstance->Users->GetUsers();
		for (user_hash::const_iterator iter = users.begin(); iter != users.end(); ++iter)
		{
			User* user = iter->second;
			if (user->registered != REG_ALL)
				continue;
	
			GroupList* groups = ext.get(user);
			if (!groups || groups->count(group))
				continue;

			parameters[nickindex] = user->nick;
			ModResult modres;
			FIRST_MOD_RESULT(OnPreCommand, modres, (command, parameters, source, true));
			if (modres != MOD_RES_DENY)
			{
				ServerInstance->Parser.CallHandler(command, parameters, source);
				targets++;
			}
		}
		return targets;
	}

	bool IsGroup(const std::string& param, std::string& group)
	{
		if (param.length() <= groupchar.length() || param.compare(0, groupchar.length(), groupchar) != 0)
			return false;

		group.assign(param, groupchar.length() - 1, std::string::npos);
		return true;
	}

	ModResult HandleInvite(LocalUser* source, CommandBase::Params& parameters)
	{
		// Check we have enough parameters and a valid group.
		std::string group;
		if (parameters.size() < 2 || !IsGroup(parameters[0], group))
			return MOD_RES_PASSTHRU;

		active = true;
		size_t penalty = ExecuteCommand(source, "INVITE", parameters, group, 0);
		source->CommandFloodPenalty += std::min(penalty, 5UL);
		active = false;
		return MOD_RES_DENY;
	}

 public:
	ModuleGroups()
		: Whois::EventListener(this)
		, active(false)
		, ext(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("groups");
		groupchar = tag->getString("prefix", "^", 1);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["GROUPCHAR"] = groupchar;
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

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) CXX11_OVERRIDE
	{
		if (user->registered != REG_ALL || !validated || active)
			return MOD_RES_PASSTHRU;

		if (command == "INVITE")
			return HandleInvite(user, parameters);

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
		return Version("Allows users to be managed using services-assigned groups", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleGroups)
