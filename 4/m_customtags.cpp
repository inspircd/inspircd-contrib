/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell <sadie@witchery.services>
/// $ModDepends: core 3
/// $ModDesc: Allows services to add custom tags to messages sent by clients.


#include "inspircd.h"
#include "modules/cap.h"
#include "modules/ctctags.h"
#include "modules/ircv3.h"
#include "modules/who.h"

typedef insp::flat_map<std::string, std::string, irc::insensitive_swo> CustomTagMap;
typedef insp::flat_map<std::string, size_t, irc::insensitive_swo> SpecialMessageMap;

class CustomTagsExtItem final
	: public SimpleExtItem<CustomTagMap>
{
private:
	CTCTags::CapReference& ctctagcap;
	ClientProtocol::EventProvider tagmsgprov;

public:
	bool broadcastchanges;

	CustomTagsExtItem(Module* Creator, CTCTags::CapReference& capref)
		: SimpleExtItem<CustomTagMap>(Creator, "custom-tags", ExtensionType::USER)
		, ctctagcap(capref)
		, tagmsgprov(Creator, "TAGMSG")
	{
	}

	void FromInternal(Extensible* container, const std::string& value) noexcept override
	{
		User* user = static_cast<User*>(container);
		if (!user)
			return;

		auto* list = new CustomTagMap();
		irc::spacesepstream ts(value);
		while (!ts.StreamEnd())
		{
			std::string tagname;
			std::string tagvalue;
			if (!ts.GetToken(tagname) || !ts.GetToken(tagvalue))
			{
				ServerInstance->Logs.Debug(MODNAME, "Malformed tag list received for {}: {}",
					user->uuid, value);
				delete list;
				return;
			}

			list->insert(std::make_pair(tagname, tagvalue));
		}

		if (!list->empty())
		{
			Set(user, list);
			if (!broadcastchanges || !ctctagcap)
				return;

			ClientProtocol::TagMap tags;
			CTCTags::TagMessage tagmsg(user, "*", tags);
			ClientProtocol::Event tagev(tagmsgprov, tagmsg);
			IRCv3::WriteNeighborsWithCap(user, tagev, *ctctagcap, true);
		}
		else
		{
			Unset(container);
			delete list;
		}
	}

	std::string ToInternal(const Extensible* container, void* item) const noexcept override
	{
		auto* list = static_cast<CustomTagMap*>(item);
		std::string buf;
		for (auto iter = list->begin(); iter != list->end(); ++iter)
		{
			if (iter != list->begin())
				buf.push_back(' ');

			buf.append(iter->first);
			buf.push_back(' ');
			buf.append(iter->second);
		}
		return buf;
	}
};

class CustomTags final
	: public ClientProtocol::MessageTagProvider
{
private:
	CTCTags::CapReference ctctagcap;

	User* UserFromMsg(ClientProtocol::Message& msg)
	{
		auto iter = specialmsgs.find(msg.GetCommand());
		if (iter == specialmsgs.end())
			return nullptr; // Not a special message.

		size_t nick_index = iter->second;
		if (irc::equals(msg.GetCommand(), "354"))
		{
			// WHOX gets special treatment as the nick field isn't in a static position.
			if (whox_index == -1)
				return nullptr; // No nick field.

			nick_index = whox_index + 1;
		}

		if (msg.GetParams().size() <= nick_index)
			return nullptr; // Not enough params.

		return ServerInstance->Users.FindNick(msg.GetParams()[nick_index]);
	}

public:
	CustomTagsExtItem ext;
	SpecialMessageMap specialmsgs;
	std::string vendor;
	int whox_index{-1};

	CustomTags(Module* mod)
		: ClientProtocol::MessageTagProvider(mod)
		, ctctagcap(mod)
		, ext(mod, ctctagcap)
		 
	{
	}

	void OnPopulateTags(ClientProtocol::Message& msg) override
	{
		User* user = msg.GetSourceUser();
		if (!user || IS_SERVER(user))
		{
			user = UserFromMsg(msg);
			if (!user)
				return; // No such user.
		}

		CustomTagMap* tags = ext.Get(user);
		if (!tags)
			return;

		for (const auto & tag : *tags)
			msg.AddTag(vendor + tag.first, this, tag.second);
	}

	ModResult OnProcessTag(User* user, const std::string& tagname, std::string& tagvalue) override
	{
		// Check that the tag begins with the customtags vendor prefix.
		if (irc::find(tagname, vendor) == 0)
			return MOD_RES_ALLOW;

		return MOD_RES_PASSTHRU;
	}

	bool ShouldSendTag(LocalUser* user, const ClientProtocol::MessageTagData& tagdata) override
	{
		return ctctagcap.IsEnabled(user);
	}
};

class ModuleCustomTags final
	: public Module
	, public CTCTags::EventListener
	, public Who::EventListener
{
private:
	CustomTags ctags;

	ModResult AddCustomTags(User* user, ClientProtocol::TagMap& tags)
	{
		CustomTagMap* tagmap = ctags.ext.Get(user);
		if (!tagmap)
			return MOD_RES_PASSTHRU;

		for (const auto & iter : *tagmap)
			tags.insert(std::make_pair(ctags.vendor + iter.first, ClientProtocol::MessageTagData(&ctags, iter.second)));
		return MOD_RES_PASSTHRU;
	}

public:
	ModuleCustomTags()
		: Module(VF_NONE, "Allows services to add custom tags to messages sent by clients")
		, CTCTags::EventListener(this)
		, Who::EventListener(this)
		, ctags(this)
	{
	}

	ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric) override
	{
		size_t nick_index;
		ctags.whox_index = request.GetFieldIndex('n', nick_index) ? nick_index : -1;
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) override
	{
		return AddCustomTags(user, details.tags_out);
	}

	ModResult OnUserPreTagMessage(User* user, const MessageTarget& target, CTCTags::TagMessageDetails& details) override
	{
		return AddCustomTags(user, details.tags_out);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		SpecialMessageMap specialmsgs;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("specialmsg"))
		{
			const std::string command = tag->getString("command");
			if (command.empty())
				throw ModuleException(this, "<specialmsg:command> must be a S2C command name!");

			specialmsgs[command] = tag->getNum<size_t>("index", 0, 0, 20);
		}
		std::swap(specialmsgs, ctags.specialmsgs);

		const auto& tag = ServerInstance->Config->ConfValue("customtags");
		ctags.ext.broadcastchanges = tag->getBool("broadcastchanges");
		ctags.vendor = tag->getString("vendor", ServerInstance->Config->ServerName, 1) + "/";
	}
};

MODULE_INIT(ModuleCustomTags)
