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
#include "modules/stats.h"
#include "modules/whois.h"
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <curl/curl.h>
#include <thread>
#include <mutex>

enum
{
    // deff custom numeric for this information in whois
    RPL_WHOISIPINFO = 695,
};

class IPInfoResolver
{
private:
    std::thread worker;
    std::mutex mtx;
    StringExtItem& cachedinfo;
    std::string apikey;
    std::string theiruuid;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s)
    {
        s->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    void DoRequest(LocalUser* user)
    {
        CURL* curl;
        CURLcode res;
        std::string response;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        if (curl)
        {
            std::string url = "http://ipinfo.io/" + user->client_sa.addr() + "?token=" + apikey;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            res = curl_easy_perform(curl);
            if (res != CURLE_OK)
            {
                std::lock_guard<std::mutex> lock(mtx);
                ServerInstance->SNO.WriteGlobalSno('a', "IPInfo: Failed to get data for %s: %s", user->nick.c_str(), curl_easy_strerror(res));
            }
            else
            {
                ParseResponse(user, response);
            }
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
    }

    void ParseResponse(LocalUser* user, const std::string& response)
    {
        rapidjson::Document document;
        if (document.Parse(response.c_str()).HasParseError())
        {
            std::lock_guard<std::mutex> lock(mtx);
            ServerInstance->SNO.WriteGlobalSno('a', "IPInfo: Failed to parse JSON for %s: %s", user->nick.c_str(), rapidjson::GetParseError_En(document.GetParseError()));
            return;
        }

        std::string city = document.HasMember("city") ? document["city"].GetString() : "Unknown";
        std::string region = document.HasMember("region") ? document["region"].GetString() : "Unknown";
        std::string country = document.HasMember("country") ? document["country"].GetString() : "Unknown";
        std::string org = document.HasMember("org") ? document["org"].GetString() : "Unknown";

        std::string info = "City: " + city + ", Region: " + region + ", Country: " + country + ", Org: " + org;
        cachedinfo.Set(user, info);

        std::lock_guard<std::mutex> lock(mtx);
        user->WriteNumeric(RPL_WHOISIPINFO, user->nick, "hes/her ip information: " + info);
    }

public:
    IPInfoResolver(Module* Creator, LocalUser* user, StringExtItem& cache, const std::string& key)
        : cachedinfo(cache), apikey(key), theiruuid(user->uuid)
    {
        worker = std::thread(&IPInfoResolver::DoRequest, this, user);
    }

    ~IPInfoResolver()
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
};

class ModuleIPInfo : public Module, public Stats::EventListener, public Whois::EventListener
{
private:
    StringExtItem cachedinfo;
    std::string apikey;

public:
    ModuleIPInfo()
        : Module(VF_VENDOR, "Adds IPinfo.io information to WHOIS responses for opers, using a configured API key.")
        , Stats::EventListener(this)
        , Whois::EventListener(this)
        , cachedinfo(this, "ipinfo", ExtensionType::USER)
    {
    }

    void ReadConfig(ConfigStatus& status) override
    {
        auto& tag = ServerInstance->Config->ConfValue("ipinfo");
        apikey = tag->getString("apikey", "", 1);

        const UserManager::LocalList& users = ServerInstance->Users.GetLocalUsers();
        for (const auto& user : users)
        {
            cachedinfo.Unset(user);
        }
    }

    void OnWhois(Whois::Context& whois) override
    {
        User* target = whois.GetTarget();

        if (target->server->IsService() || target->IsModeSet('B'))
            return;

        if (!whois.GetSource()->IsOper())
            return;

        const std::string* cached = cachedinfo.Get(target);
        if (cached)
        {
            whois.SendLine(RPL_WHOISIPINFO, "hes/her ip information(cached): " + *cached);
        }
        else
        {
            new IPInfoResolver(this, IS_LOCAL(target), cachedinfo, apikey);
        }
    }

    ModResult OnStats(Stats::Context& stats) override
    {
        return MOD_RES_PASSTHRU;
    }

    void OnUnloadModule(Module* mod) override
    {
        const UserManager::LocalList& users = ServerInstance->Users.GetLocalUsers();
        for (const auto& user : users)
        {
            cachedinfo.Unset(user);
        }
    }
};

MODULE_INIT(ModuleIPInfo)
