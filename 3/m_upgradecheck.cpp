/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023 Sadie Powell <sadie@witchery.services>
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
/// $ModDesc: Checks the server config for deprecated config entries that might cause trouble when upgrading to v4.
/// $ModDepends: core 3

#include <sstream>

#include "inspircd.h"

class ModuleUpgradeCheck CXX11_FINAL
	: public Module
{
 private:
	void CheckKey(const std::string& tag, const std::string& key, const std::string& message)
	{
		ConfigTagList ctags = ServerInstance->Config->ConfTags(tag);
		for (ConfigIter i = ctags.first; i != ctags.second; ++i)
		{
			ConfigTag* ctag = i->second;
			std::string cvalue;
			if (ctag->readString(key, cvalue))
				DoLog(tag, key, "", ctag->getTagLocation(), message);
		}
	}

	void CheckModule(const std::string& name, const std::string& message)
	{
		CheckValue("module", "name", name, message);
		CheckValue("module", "name", ModuleManager::ExpandModName(name), message);
	}

	void CheckNoValue(const std::string& tag, const std::string& key, const std::string& message)
	{
		ConfigTagList ctags = ServerInstance->Config->ConfTags(tag);
		for (ConfigIter i = ctags.first; i != ctags.second; ++i)
		{
			ConfigTag* ctag = i->second;
			std::string cvalue;
			if (!ctag->readString(key, cvalue))
				DoLog(tag, key, "", ctag->getTagLocation(), message);
		}
	}


	void CheckTag(const std::string& tag, const std::string& message)
	{
		ConfigTagList ctags = ServerInstance->Config->ConfTags(tag);
		for (ConfigIter i = ctags.first; i != ctags.second; ++i)
		{
			ConfigTag* ctag = i->second;
			DoLog(tag, "", "", ctag->getTagLocation(), message);
		}
	}

	void CheckValue(const std::string& tag, const std::string& key, const std::string& value, const std::string& message)
	{
		ConfigTagList ctags = ServerInstance->Config->ConfTags(tag);
		for (ConfigIter i = ctags.first; i != ctags.second; ++i)
		{
			ConfigTag* ctag = i->second;
			std::string cvalue;
			if (ctag->readString(key, cvalue))
			{
				if (stdalgo::string::equalsci(cvalue, value))
					DoLog(tag, key, value, ctag->getTagLocation(), message);
			}
		}
	}

	void CheckValueBool(const std::string& tag, const std::string& key, bool value, const std::string& message)
	{
		CheckValue(tag, key, value ? "on" : "off", message);
		CheckValue(tag, key, value ? "true" : "false", message);
		CheckValue(tag, key, value ? "yes" : "no", message);
		CheckNoValue(tag, key, message);
	}

	void DoLog(const std::string& tag, const std::string& key, const std::string& value, const std::string& location, const std::string& message)
	{
		std::ostringstream locationstream;
		locationstream << '<' << tag;
		if (!key.empty())
		{
			if (value.empty())
				locationstream << ':' << key;
			else
				locationstream << ' ' << key << "=\"" << ServerConfig::Escape(value) << "\"";
		}
		locationstream << '>';

		const std::string fulltag = locationstream.str();
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "%s at %s: %s", fulltag.c_str(), location.c_str(), message.c_str());
	}

 public:
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		CheckKey("bind", "ssl", "moved to <bind:sslprofile>");
		CheckKey("channel", "opers", "moved to <oper:maxchans> and <type:maxchans>");
		CheckKey("channel", "users", "moved to <connect:maxchans>");
		CheckKey("connectban", "duration", "moved to <connectban:banduration>");
		CheckKey("exemptfromfilter", "channel", "moved to <exemptfromfilter:target>");
		CheckKey("limits", "maxgecos", "moved to <limits:maxreal>");
		CheckKey("link", "ssl", "moved to <link:sslprofile>");
		CheckKey("options", "moronbanner", "moved to <options:xlinemessage>");
		CheckKey("repeat", "maxsecs", "moved to <repeat:maxtime>");
		CheckKey("security", "hidewhois", "moved to <security:hideserver>");
		CheckModule("censor", "moved to inspircd-contrib");
		CheckModule("clones", "moved to inspircd-contrib");
		CheckModule("hostchange", "moved to inspircd-contrib");
		CheckModule("lockserv", "moved to inspircd-contrib");
		CheckModule("modenotice", "moved to inspircd-contrib");
		CheckModule("regex_tre", "moved to inspircd-contrib");
		CheckModule("userip", "moved to inspircd-contrib");
		CheckNoValue("class", "snomasks", "no longer defaults to *");
		CheckTag("power", "replaced with oper command privs");
		CheckTag("hostchange", "replaced with the cloak_user and cloak_static <cloak> methods");
		CheckValue("bind", "sslprofile", "gnutls", "ssl config moved from <gnutls> to <sslprofile>");
		CheckValue("bind", "sslprofile", "mbedtls", "ssl config moved from <mbedtls> to <sslprofile>");
		CheckValue("bind", "sslprofile", "openssl", "ssl config moved from <openssl> to <sslprofile>");
		CheckValue("link", "sslprofile", "gnutls", "ssl config moved from <gnutls> to <sslprofile>");
		CheckValue("link", "sslprofile", "mbedtls", "ssl config moved from <gnutls> to <sslprofile>");
		CheckValue("link", "sslprofile", "openssl", "ssl config moved from <gnutls> to <sslprofile>");
		CheckValue("oper", "autologin", "if-host-match", "value replaced with relaxed or strict");
		CheckValue("options", "casemapping", "rfc1459", "casemapping removed (replace with ascii)");
		CheckValueBool("cban", "glob", false, "now always enabled");
		CheckValueBool("cgiirc", "opernotice", true, "replaced with oper snomask privileges");
		CheckValueBool("chanhistory", "enableumode", false, "now always enabled");
		CheckValueBool("commonchans", "invite", false, "now always enabled");
		CheckValueBool("deaf", "enableprivdeaf", false, "now always enabled");
		CheckValueBool("disabled", "fakenonexistant", true, "moved to <disabled:fakenonexistent>");
		CheckValueBool("noctcp", "enableumode", false, "now always enabled");
		CheckValueBool("oper", "autologin", true, "value replaced with relaxed or strict");
		CheckValueBool("options", "allowzerolimit", true, "now always disabled");
		CheckValueBool("override", "enableumode", false, "now always enabled");
		CheckValueBool("security", "allowcoreunload", true, "now always disabled");
		CheckValueBool("showwhois", "showfromopers", true, "replaced with the users/secret-whois oper priv");
		CheckValueBool("sslmodes", "enableumode", false, "now always enabled");
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Checks the server config for deprecated config entries that might cause trouble when upgrading to v4.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleUpgradeCheck)

