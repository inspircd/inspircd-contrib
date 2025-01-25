/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2024 reverse
 *
 * This file contains a third-party module for InspIRCd. You can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: reverse <mike.chevronnet@gmail.com>
/// $ModDesc: Adds city and country information to WHOIS using local MaxMind database and with it's own usermode +y.
/// $ModConfig: <geolite dbpath="path/geodata/GeoLite2-City.mmdb">
/// $ModDepends: core 4

/// $LinkerFlags: -lmaxminddb

/// $PackageInfo: require_system("alpine") libmaxminddb-dev pkgconf
/// $PackageInfo: require_system("arch") pkgconf libmaxminddb
/// $PackageInfo: require_system("darwin") libmaxminddb pkg-config
/// $PackageInfo: require_system("debian~") libmaxminddb-dev pkg-config
/// $PackageInfo: require_system("rhel~") pkg-config libmaxminddb-devel

#include "inspircd.h"
#include "modules/whois.h"
#include "extension.h"
#include <maxminddb.h>

class GeoLiteMode final : public SimpleUserMode
{
public:
    GeoLiteMode(Module* mod)
        : SimpleUserMode(mod, "geolite", 'y', false)
    {
    }
};

class ModuleWhoisGeoLite final : public Module, public Whois::EventListener
{
private:
    MMDB_s mmdb;          
    std::string dbpath;  
    StringExtItem country_item;
    GeoLiteMode geolite_mode;

public:
    ModuleWhoisGeoLite()
        : Module(VF_OPTCOMMON, "Adds city and country information to WHOIS using the MaxMind database.")
        , Whois::EventListener(this)
        , country_item(this, "geo-lite-country", ExtensionType::USER, true) // Sync across servers
        , geolite_mode(this)
    {
    }

    void ReadConfig(ConfigStatus& status) override
    {
        auto& tag = ServerInstance->Config->ConfValue("geolite");
        dbpath = ServerInstance->Config->Paths.PrependConfig(tag->getString("dbpath", "data/GeoLite2-City.mmdb"));

        int status_open = MMDB_open(dbpath.c_str(), MMDB_MODE_MMAP, &mmdb);
        if (status_open != MMDB_SUCCESS) {
            std::string error_msg = "GeoLite2: Failed to open GeoLite2 database: " + std::string(MMDB_strerror(status_open));
            throw ModuleException(this, error_msg.c_str());
        }
    }

    void OnWhois(Whois::Context& whois) override
    {
        User* target = whois.GetTarget();

        // Check if the target user has +y mode set
        if (!target->IsModeSet(geolite_mode))
            return;

        const std::string* info = country_item.Get(target);
        if (info && !info->empty()) {
            whois.SendLine(RPL_WHOISSPECIAL, "is connecting from " + *info);
        } else {
            whois.SendLine(RPL_WHOISSPECIAL, "City: Unknown, Country: Unknown");
        }
    }

    void OnChangeRemoteAddress(LocalUser* user) override
    {
        if (!user->client_sa.is_ip()) {
            country_item.Unset(user);
            return;
        }

        int gai_error = 0;
        const struct sockaddr* addr = &user->client_sa.sa;
        MMDB_lookup_result_s result = MMDB_lookup_sockaddr(&mmdb, addr, &gai_error);

        if (gai_error != 0 || !result.found_entry) {
            country_item.Unset(user);
            return;
        }

        MMDB_entry_data_s city_data = {};
        MMDB_entry_data_s country_data = {};
        int status_city = MMDB_get_value(&result.entry, &city_data, "city", "names", "en", nullptr);
        int status_country = MMDB_get_value(&result.entry, &country_data, "country", "names", "en", nullptr);

        std::string city = (status_city == MMDB_SUCCESS && city_data.has_data) ? std::string(city_data.utf8_string, city_data.data_size) : "Unknown";
        std::string country = (status_country == MMDB_SUCCESS && country_data.has_data) ? std::string(country_data.utf8_string, country_data.data_size) : "Unknown";

        country_item.Set(user, "City: " + city + ", Country: " + country);
    }

    void OnUserQuit(User* user, const std::string& message, const std::string& opermessage) override
    {
        if (user->IsModeSet(geolite_mode))
            country_item.Unset(user);
    }

    ~ModuleWhoisGeoLite() override
    {
        MMDB_close(&mmdb);
    }
};

MODULE_INIT(ModuleWhoisGeoLite)