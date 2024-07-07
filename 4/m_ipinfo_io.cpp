    /*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2024 Jean Chevronnet <mike.chevronnet@gmail.com>
 *
 * This file contains a third party module for InspIRCd.  You can
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

/// $ModAuthor: Jean Chevronnet (reverse) <mike.chevronnet@gmail.com>
/// $ModDesc: Ip information from Ipinfo.io in /WHOIS (only irc operators), found more information at https://ipinfo.io/developers.
/// $ModDepends: core 4
/// $ModConfig: <ipinfo apikey="YOUR IP INFO.IO APIKEY">
/// $CompilerFlags: find_compiler_flags("RapidJSON")
/// $CompilerFlags: find_compiler_flags("libcurl")
/// $LinkerFlags: find_linker_flags("libcurl")

#include "inspircd.h"
#include "extension.h"
#include "modules/httpd.h"
#include "modules/whois.h"
#include "thread.h"
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <curl/curl.h>
#include <mutex>
#include <regex>
#include <fmt/core.h>

class IPInfoResolver : public Thread
{
private:
    std::mutex mtx;
    StringExtItem& cachedinfo;
    std::string apikey;
    std::string theiruuid;
    User* resolved_user; // Renamed to avoid shadowing

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s)
    {
        s->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    void OnStart() override
    {
        CURL* curl;
        CURLcode res;
        std::string response;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        if (curl)
        {
            std::string url = "https://ipinfo.io/" + resolved_user->client_sa.addr() + "?token=" + apikey;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            res = curl_easy_perform(curl);
            if (res != CURLE_OK)
            {
                std::lock_guard<std::mutex> lock(mtx);
                ServerInstance->SNO.WriteGlobalSno('a', fmt::format("IPInfo: Failed to get data for {}: {}", resolved_user->nick, curl_easy_strerror(res)));
            }
            else
            {
                ParseResponse(resolved_user, response);
            }
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
    }

    void ParseResponse(User* user, const std::string& response)
    {
        rapidjson::Document document;
        if (document.Parse(response.c_str()).HasParseError())
        {
            std::lock_guard<std::mutex> lock(mtx);
           ServerInstance->SNO.WriteGlobalSno('a', "IPInfo: Failed to parse JSON for {}: {}", user->nick, rapidjson::GetParseError_En(document.GetParseError()));
            return;
        }

        std::string city = document.HasMember("city") ? document["city"].GetString() : "Unknown";
        std::string region = document.HasMember("region") ? document["region"].GetString() : "Unknown";
        std::string country = document.HasMember("country") ? document["country"].GetString() : "Unknown";
        std::string org = document.HasMember("org") ? document["org"].GetString() : "Unknown";

        std::string info = "City: " + city + ", Region: " + region + ", Country: " + country + ", Org: " + org;
        cachedinfo.Set(user, info);

        std::lock_guard<std::mutex> lock(mtx);
        user->WriteNumeric(RPL_WHOISSPECIAL, user->nick, "ip info: " + info);
    }

public:
    IPInfoResolver(Module* Creator, User* user, StringExtItem& cache, const std::string& key)
        : Thread(), cachedinfo(cache), apikey(key), theiruuid(user->uuid), resolved_user(user)
    {
        if (user->client_sa.is_ip())
        {
            this->Start();
        }
    }
};

class ModuleIPInfo : public Module, public Whois::EventListener
{
private:
    StringExtItem cachedinfo;
    std::string apikey;

    bool IsPrivateIP(const std::string& ip)
    {
        // Regex patterns for private IPv4 addresses
        std::regex private_ipv4_patterns[] = {
            std::regex("^10\\..*"),
            std::regex("^172\\.(1[6-9]|2[0-9]|3[0-1])\\..*"),
            std::regex("^192\\.168\\..*")
        };

        // Check IPv4 addresses
        for (const auto& pattern : private_ipv4_patterns)
        {
            if (std::regex_match(ip, pattern))
                return true;
        }

        // Check IPv6 link-local addresses (fe80::/10)
        if (ip.find("fe80:") == 0)
            return true;

        // Check loopback addresses
        if (ip == "127.0.0.1" || ip == "::1")
            return true;

        return false;
    }

public:
    ModuleIPInfo()
        : Module(VF_VENDOR, "Adds IPinfo.io information to WHOIS responses for opers, using a configured API key.")
        , Whois::EventListener(this)
        , cachedinfo(this, "ipinfo", ExtensionType::USER, true) // Enable synchronization across the network
    {
    }

    void ReadConfig(ConfigStatus& status) override
    {
        auto& tag = ServerInstance->Config->ConfValue("ipinfo");
        apikey = tag->getString("apikey", "");

        if (apikey.empty())
        {
            throw ModuleException(this, "<ipinfo:apikey> No APIKEY? This is a required configuration option.");
        }

        const UserManager::LocalList& users = ServerInstance->Users.GetLocalUsers();
        for (const auto& user : users)
        {
            cachedinfo.Unset(user);
        }
    }

    void OnWhois(Whois::Context& whois) override
    {
        User* target = whois.GetTarget();

        if (target->server->IsService())
            return;

        UserModeReference botmode(this, "bot");
        if (target->IsModeSet(botmode))
            return;

        if (!whois.GetSource()->IsOper())
            return;

        if (!target->client_sa.is_ip())
        {
            whois.SendLine(RPL_WHOISSPECIAL, "ip info: no IP address found, maybe using UNIX socket connection.");
            return;
        }

        // Check for private IP addresses
        if (IsPrivateIP(target->client_sa.addr()))
        {
            whois.SendLine(RPL_WHOISSPECIAL, "ip info: user is connecting from a private IP address.");
            return;
        }

        const std::string* cached = cachedinfo.Get(target);
        if (cached)
        {
            whois.SendLine(RPL_WHOISSPECIAL, "ip info (cached): " + *cached);
        }
        else
        {
            new IPInfoResolver(this, target, cachedinfo, apikey);
        }
    }
};

MODULE_INIT(ModuleIPInfo)
