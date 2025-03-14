/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <brain@inspircd.org>
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

/// $ModAuthor: InspIRCd Developers
/// $ModDepends: core 4
/// $ModDesc: Allows the server administrator to define custom rules for applying hostnames to users.
/// $ModConfig: <hostchange mask="*@42.theanswer.example.org" action="addaccount" suffix=".users.example.com">
/// $ModConfig: <hostchange mask="*root@*" action="addnick" prefix="example/users/">
/// $ModConfig: <hostchange mask="a@example.com" action="set" value="foo.bar.baz">
/// $ModConfig: <hostchange mask="*@localhost" ports="7000,7001,7005-7007" action="set" value="blahblah.foo" class="local">
/// $ModLink: https://docs.inspircd.org/4/moved-modules/#hostchange


#include "inspircd.h"
#include "modules/account.h"
#include "utility/string.h"

// Holds information about a <hostchange> rule.
class HostRule final
{
public:
	enum class HostChangeAction
		: uint8_t
	{
		// Add the user's account name to their hostname.
		ADD_ACCOUNT,

		// Add the user's nickname to their hostname.
		ADD_NICK,

		// Set the user's hostname to the specific value.
		SET,
	};

private:
	HostChangeAction action;
	std::string host;
	std::string klass;
	std::string mask;
	insp::flat_set<in_port_t> ports;
	std::string prefix;
	std::string suffix;

	void ReadConfig(const std::shared_ptr<ConfigTag>& tag)
	{
		// Parse <hostchange:class>.
		klass = tag->getString("class");

		// Parse <hostchange:port>.
		const std::string portlist = tag->getString("ports");
		if (!portlist.empty())
		{
			irc::portparser portrange(portlist, false);
			while (long port = portrange.GetToken())
			{
				if (port > std::numeric_limits<in_port_t>::min() && port <= std::numeric_limits<in_port_t>::max())
					ports.insert(static_cast<in_port_t>(port));
			}
		}
	}

public:
	HostRule(const std::shared_ptr<ConfigTag>& tag, const std::string& Mask, const std::string& Host)
		: action(HostChangeAction::SET)
		, host(Host)
		, mask(Mask)
	{
		ReadConfig(tag);
	}

	HostRule(const std::shared_ptr<ConfigTag>& tag, HostChangeAction Action, const std::string& Mask, const std::string& Prefix, const std::string& Suffix)
		: action(Action)
		, mask(Mask)
		, prefix(Prefix)
		, suffix(Suffix)
	{
		ReadConfig(tag);
	}

	HostChangeAction GetAction() const
	{
		return action;
	}

	const std::string& GetHost() const
	{
		return host;
	}

	bool Matches(LocalUser* user) const
	{
		if (!klass.empty() && !insp::equalsci(klass, user->GetClass()->GetName()))
			return false;

		if (!ports.empty() && !ports.count(user->server_sa.port()))
			return false;

		if (InspIRCd::MatchCIDR(user->GetRealUserHost(), mask))
			return true;

		return InspIRCd::MatchCIDR(user->GetUserAddress(), mask);
	}

	void Wrap(const std::string& value, std::string& out) const
	{
		if (!prefix.empty())
			out.append(prefix);

		out.append(value);

		if (!suffix.empty())
			out.append(suffix);
	}
};

typedef std::vector<HostRule> HostRules;

class ModuleHostChange final
	: public Module
{
private:
	Account::API accountapi;
	CharState hostmap;
	HostRules hostrules;

	std::string CleanName(const std::string& name)
	{
		std::string buffer;
		buffer.reserve(name.length());
		for (const auto chr : name)
		{
			if (hostmap.test(static_cast<unsigned char>(chr)))
				buffer.push_back(chr);
		}
		return buffer;
	}

public:
	ModuleHostChange()
		: Module(VF_VENDOR, "Allows the server administrator to define custom rules for applying hostnames to users.")
		, accountapi(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		HostRules rules;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("hostchange"))
		{
			// Ensure that we have the <hostchange:mask> parameter.
			const std::string mask = tag->getString("mask");
			if (mask.empty())
				throw ModuleException(this, "<hostchange:mask> is a mandatory field, at " + tag->source.str());

			// Determine what type of host rule this is.
			const std::string action = tag->getString("action");
			if (insp::equalsci(action, "addaccount"))
			{
				// The hostname is in the format [prefix]<account>[suffix].
				rules.emplace_back(tag, HostRule::HostChangeAction::ADD_ACCOUNT, mask, tag->getString("prefix"), tag->getString("suffix"));
			}
			else if (insp::equalsci(action, "addnick"))
			{
				// The hostname is in the format [prefix]<nick>[suffix].
				rules.emplace_back(tag, HostRule::HostChangeAction::ADD_NICK, mask, tag->getString("prefix"), tag->getString("suffix"));
			}
			else if (insp::equalsci(action, "set"))
			{
				// Ensure that we have the <hostchange:value> parameter.
				const std::string value = tag->getString("value");
				if (value.empty())
					throw ModuleException(this, "<hostchange:value> is a mandatory field when using the 'set' action, at " + tag->source.str());

				// The hostname is in the format <value>.
				rules.emplace_back(tag, mask, value);
				continue;
			}
			else
			{
				throw ModuleException(this, action + " is an invalid <hostchange:action> type, at " + tag->source.str());
			}
		}

		const auto& tag = ServerInstance->Config->ConfValue("hostname");
		const std::string hmap = tag->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789", 1);

		CharState newhostmap;
		for (const auto chr : hmap)
		{
			// A hostname can not contain NUL, LF, CR, or SPACE.
			if (chr == 0x00 || chr == 0x0A || chr == 0x0D || chr == 0x20)
				throw ModuleException(this, INSP_FORMAT("<hostname:charmap> can not contain character 0x{:02X} ({})", chr, chr));
			newhostmap.set(static_cast<unsigned char>(chr));
		}
		std::swap(newhostmap, hostmap);
		hostrules.swap(rules);
	}

	void OnUserConnect(LocalUser* user) override
	{
		for (const auto& rule : hostrules)
		{
			if (!rule.Matches(user))
				continue;

			std::string newhost;
			if (rule.GetAction() == HostRule::HostChangeAction::ADD_ACCOUNT)
			{
				// Retrieve the account name.
				const std::string* accountptr = accountapi ? accountapi->GetAccountName(user) : nullptr;
				if (!accountptr)
					continue;

				// Remove invalid hostname characters.
				std::string accountname = CleanName(*accountptr);
				if (accountname.empty())
					continue;

				// Create the hostname.
				rule.Wrap(accountname, newhost);
			}
			else if (rule.GetAction() == HostRule::HostChangeAction::ADD_NICK)
			{
				// Remove invalid hostname characters.
				const std::string nickname = CleanName(user->nick);
				if (nickname.empty())
					continue;

				// Create the hostname.
				rule.Wrap(nickname, newhost);
			}
			else if (rule.GetAction() == HostRule::HostChangeAction::SET)
			{
				newhost.assign(rule.GetHost());
			}

			if (!newhost.empty())
			{
				user->WriteNotice("Setting your virtual host: " + newhost);
				user->ChangeDisplayedHost(newhost);
				return;
			}
		}
	}

	void GetLinkData(Module::LinkData& data, std::string& compatdata) override
	{
		for (size_t i = 0; i < hostmap.size(); ++i)
			if (hostmap[i])
				data["hostchars"].push_back(static_cast<unsigned char>(i));
	}
};

MODULE_INIT(ModuleHostChange)
