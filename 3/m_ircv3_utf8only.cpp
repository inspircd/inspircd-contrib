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

/// $CompilerFlags: -Ivendor_directory("utfcpp")
/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <utf8 sanitise="yes">
/// $ModDesc: Implements the IRCv3 UTF8ONLY extension.
/// $ModDepends: core 3



#include "inspircd.h"
#include "modules/stats.h"

#define UTF_CPP_CPLUSPLUS 199711L
#include <unchecked.h>

namespace
{
	// The number of messages which were invalid UTF-8.
	unsigned long invalidmsg;

	// Whether to sanitize malformed UTF-8 messages.
	bool sanitize;

	// The number of messages which were invalid UTF-8.
	unsigned long validmsg;
}

class UTF8Serializer CXX11_FINAL
	: public ClientProtocol::Serializer
{
 public:
	ClientProtocol::Serializer* serializer;

	UTF8Serializer(Module* mod, ClientProtocol::Serializer* ser)
		: ClientProtocol::Serializer(mod, "utf8-" + ser->name.substr(11))
		, serializer(ser)
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Creating UTF-8 wrapper for %s: %s", serializer->name.c_str(), name.c_str());
	}

	~UTF8Serializer()
	{
		ServerInstance->Modules->DelService(*this);
	}

	bool Parse(LocalUser* user, const std::string& line, ClientProtocol::ParseOutput& parseoutput) CXX11_OVERRIDE
	{
		if (sanitize)
		{
			std::string newline;
			newline.reserve(line.length());
			utf8::unchecked::replace_invalid(line.begin(), line.end(), std::back_inserter(newline));

			(line == newline ? validmsg : invalidmsg)++;
			return serializer->Parse(user, newline, parseoutput);
		}
		else
		{
			(utf8::is_valid(line.begin(), line.end()) ? validmsg : invalidmsg)++;
			return serializer->Parse(user, line, parseoutput);
		}
	}

	ClientProtocol::SerializedMessage Serialize(const ClientProtocol::Message& msg, const ClientProtocol::TagSelection& tagwl) const CXX11_OVERRIDE
	{
		return serializer->Serialize(msg, tagwl);
	}
};

class ModuleIRCv3UTF8Only CXX11_FINAL
	: public Module
	, public Stats::EventListener
{
 private:
	typedef insp::flat_map<std::string, UTF8Serializer*> SerializerMap;
	SerializerMap serializers;

 public:
	ModuleIRCv3UTF8Only()
		: Stats::EventListener(this)
	{
	}

	~ModuleIRCv3UTF8Only()
	{
		for (SerializerMap::const_iterator iter = serializers.begin(); iter != serializers.end(); ++iter)
			delete iter->second;
	}

	void init() CXX11_OVERRIDE
	{
		// Replace local user's serializers with a UTF-8 version.
		const UserManager::LocalList& lusers = ServerInstance->Users.GetLocalUsers();
		for (UserManager::LocalList::const_iterator iter = lusers.begin(); iter != lusers.end(); ++iter)
			OnUserInit(*iter);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("utf8");
		sanitize = tag->getBool("sanitize", false);
	}

	void Prioritize() CXX11_OVERRIDE
	{
		ServerInstance->Modules->SetPriority(this, I_OnUserInit, PRIORITY_LAST);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		if (!sanitize)
			return;

		tokens["CHARSET"] = "UTF-8";
		tokens["UTF8ONLY"];
	}

	void OnCleanup(ExtensionItem::ExtensibleType type, Extensible* item) CXX11_OVERRIDE
	{
		if (type != ExtensionItem::EXT_USER)
			return;

		LocalUser* const user = IS_LOCAL(static_cast<User*>(item));
		if (user && user->serializer->creator == this)
			user->serializer = static_cast<UTF8Serializer*>(user->serializer)->serializer;
	}

	void OnServiceDel(ServiceProvider& service) CXX11_OVERRIDE
	{
		if (service.creator == this || service.name.compare(0, 11, "serializer/", 11))
			return; // Not a serializer or provided by us.

		SerializerMap::iterator serializer = serializers.find(service.name.substr(11));
		if (serializer == serializers.end())
			return; // Not a wrapped serializer.

		// Remove the wrapped serializer.
		delete serializer->second;
		serializers.erase(serializer);
	}

	ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE
	{
		if (stats.GetSymbol() != '8')
			return MOD_RES_PASSTHRU;

		unsigned long totalmsg = validmsg + invalidmsg;
		stats.AddRow(RPL_STATS, stats.GetSymbol(), InspIRCd::Format("UTF-8: valid %lu, invalid %lu, total: %lu", validmsg, invalidmsg, totalmsg));

		double validpct = static_cast<double>(validmsg) / static_cast<double>(totalmsg) * 100;
		double invalidpct = static_cast<double>(invalidmsg) / static_cast<double>(totalmsg) * 100;
		stats.AddRow(RPL_STATS, stats.GetSymbol(), InspIRCd::Format("UTF-8: valid%% %3.2f%%, invalid%% %3.2f%%", (float)validpct, (float)invalidpct));

		return MOD_RES_DENY;
	}

	void OnUserInit(LocalUser* user) CXX11_OVERRIDE
	{
		if (!user->serializer || user->serializer->creator == this)
			return; // No serializer or they already have a UTF-8 serializer.

		SerializerMap::const_iterator iter = serializers.find(user->serializer->name);
		if (iter == serializers.end())
		{
			// Serializer doesn't exist, create it.
			UTF8Serializer* serializer = new UTF8Serializer(this, user->serializer);
			std::pair<SerializerMap::iterator, bool> res = serializers.insert(std::make_pair(user->serializer->name, serializer));
			if (res.second)
				iter = res.first;
		}

		if (iter != serializers.end())
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Replacing serializer %s with %s for %s",
				user->serializer->name.c_str(), iter->second->name.c_str(), user->uuid.c_str());
			user->serializer = iter->second;
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements the IRCv3 UTF8ONLY extension.", VF_COMMON);
	}
};

MODULE_INIT(ModuleIRCv3UTF8Only)

