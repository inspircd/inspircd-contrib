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
/// $ModDesc: Adds city and country information to WHOIS using a local MaxMind GeoLite2 database with user mode +y.
/// $ModConfig: <geomaxlite dbpath="path/geodata/GeoLite2-City.mmdb">
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

class ModuleGeoMaxLite final
    : public Module
    , public Whois::EventListener
{
private:
    // MaxMind DB handle
    MMDB_s mmdb;

    // Path to the MaxMind database file
    std::string dbpath;

    // Extension item to store a user's city/country info
    StringExtItem country_item;

    // +y user mode for enabling GeoMaxLite lookups
    SimpleUserMode geomaxlite_mode;

public:
    ModuleGeoMaxLite()
        : Module(VF_OPTCOMMON, "Adds city and country information to WHOIS using a local MaxMind GeoLite2 database.")
        , Whois::EventListener(this)
        , country_item(this, "geomaxlite-country", ExtensionType::USER, true)
        , geomaxlite_mode(this, "geomaxlite", 'y', false)
    {
    }

    void ReadConfig(ConfigStatus& status) override
    {
        // Read the <geomaxlite> block from the config
        auto& tag = ServerInstance->Config->ConfValue("geomaxlite");
        dbpath = ServerInstance->Config->Paths.PrependConfig(
            tag->getString("dbpath", "data/GeoLite2-City.mmdb"));

        int status_open = MMDB_open(dbpath.c_str(), MMDB_MODE_MMAP, &mmdb);
        if (status_open != MMDB_SUCCESS)
        {
            // If the database can't be opened, throw a module exception
            std::string error_msg = "GeoMaxLite: Failed to open GeoLite2 database: "
                                  + std::string(MMDB_strerror(status_open));
            throw ModuleException(this, error_msg.c_str());
        }
    }

    void OnWhois(Whois::Context& whois) override
    {
        User* target = whois.GetTarget();

        // Only display city/country data if the target user has +y (geomaxlite) set
        if (!target->IsModeSet(geomaxlite_mode))
            return;

        const std::string* info = country_item.Get(target);
        if (info && !info->empty())
        {
            whois.SendLine(RPL_WHOISSPECIAL, "is connecting from " + *info);
        }
        else
        {
            whois.SendLine(RPL_WHOISSPECIAL, "City: Unknown, Country: Unknown");
        }
    }

    void OnChangeRemoteAddress(LocalUser* user) override
    {
        // If the address is not an IP, unset any stored geo info
        if (!user->client_sa.is_ip())
        {
            country_item.Unset(user);
            return;
        }

        int gai_error = 0;
        const struct sockaddr* addr = &user->client_sa.sa;
        MMDB_lookup_result_s result = MMDB_lookup_sockaddr(&mmdb, addr, &gai_error);

        // Unset if we got a lookup error or didn't find anything
        if (gai_error != 0 || !result.found_entry)
        {
            country_item.Unset(user);
            return;
        }

        // Fetch city and country strings (in English)
        MMDB_entry_data_s city_data = {};
        MMDB_entry_data_s country_data = {};

        int status_city = MMDB_get_value(&result.entry, &city_data, "city", "names", "en", nullptr);
        int status_country = MMDB_get_value(&result.entry, &country_data, "country", "names", "en", nullptr);

        std::string city = (status_city == MMDB_SUCCESS && city_data.has_data)
            ? std::string(city_data.utf8_string, city_data.data_size)
            : "Unknown";

        std::string country = (status_country == MMDB_SUCCESS && country_data.has_data)
            ? std::string(country_data.utf8_string, country_data.data_size)
            : "Unknown";

        // Store
        country_item.Set(user, "City: " + city + ", Country: " + country);
    }

    ~ModuleGeoMaxLite() override
    {
        // Close the MaxMind DB on unload
        MMDB_close(&mmdb);
    }
};

MODULE_INIT(ModuleGeoMaxLite)
