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

 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: reverse <mike.chevronnet@gmail.com>
/// $ModDesc: Adds city information to WHOIS using the MaxMind database.
/// $ModConfig: add path <geolite dbpath="path/geodata/GeoLite2-City.mmdb">
/// $ModDepends: core 4

/// $CompilerFlags: find_compiler_flags("libmaxminddb")
/// $LinkerFlags: find_linker_flags("libmaxminddb")

/// $PackageInfo: require_system("alpine") libmaxminddb-dev pkgconf
/// $PackageInfo: require_system("arch") pkgconf libmaxminddb
/// $PackageInfo: require_system("darwin") libmaxminddb pkg-config
/// $PackageInfo: require_system("debian~") libmaxminddb-dev pkg-config
/// $PackageInfo: require_system("rhel~") pkg-config libmaxminddb-devel

#include "inspircd.h"
#include "modules/whois.h"
#include <maxminddb.h>

class ModuleWhoisGeoLite final
	: public Module
	, public Whois::EventListener
{
private:
	MMDB_s mmdb;       // MaxMind database object
	bool db_loaded;    // Flag to indicate if the database was successfully loaded
	std::string dbpath;

public:
	ModuleWhoisGeoLite()
		: Module(VF_OPTCOMMON, "Adds city information to WHOIS using the MaxMind database.")
		, Whois::EventListener(this), db_loaded(false)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		// Load configuration and get the path to the GeoLite2 database
		auto& tag = ServerInstance->Config->ConfValue("geolite");
		dbpath = tag->getString("dbpath", "/etc/GeoLite2-City.mmdb");

		// Attempt to open the MaxMind GeoLite2-City database
		int status_open = MMDB_open(dbpath.c_str(), MMDB_MODE_MMAP, &mmdb);
		if (status_open != MMDB_SUCCESS) {
			ServerInstance->SNO.WriteGlobalSno('a', "GeoLite2: Failed to open GeoLite2 database: " + std::string(MMDB_strerror(status_open)));
			db_loaded = false;
		} else {
			ServerInstance->SNO.WriteGlobalSno('a', "GeoLite2: Successfully opened GeoLite2 database.");
			db_loaded = true;
		}
	}

	void OnWhois(Whois::Context& whois) override
	{
		User* source = whois.GetSource();  // The user issuing the WHOIS command
		User* target = whois.GetTarget();  // The user being WHOIS'd

		// Only allow IRC operators to see the city information
		if (!source->IsOper()) {
			return;
		}

		// Check if the user is remote
		if (!IS_LOCAL(target)) {
			whois.SendLine(RPL_WHOISSPECIAL, "*", "City: User is connected from a remote server.");
			return;
		}

		LocalUser* luser = IS_LOCAL(target);

		// Ensure the user is local and has a valid IP address
		if (!luser || !luser->client_sa.is_ip()) {
			whois.SendLine(RPL_WHOISSPECIAL, "*", "City: No valid IP address (possibly using a Unix socket).");
			return;
		}

		// Ensure the MaxMind database is loaded
		if (!db_loaded) {
			whois.SendLine(RPL_WHOISSPECIAL, "*", "City: GeoLite2 database not loaded.");
			return;
		}

		// Perform the GeoLite2 lookup using the socket address
		int gai_error = 0;
		const struct sockaddr* addr = reinterpret_cast<const struct sockaddr*>(&luser->client_sa);
		MMDB_lookup_result_s result = MMDB_lookup_sockaddr(&mmdb, addr, &gai_error);

		if (gai_error != 0) {
			ServerInstance->SNO.WriteGlobalSno('a', "GeoLite2: getaddrinfo error: " + std::string(gai_strerror(gai_error)));
			whois.SendLine(RPL_WHOISSPECIAL, "*", "City: Unknown (lookup error).");
			return;
		}

		if (!result.found_entry) {
			whois.SendLine(RPL_WHOISSPECIAL, "*", "City: Unknown (no database entry).");
			return;
		}

		// Retrieve the city name
		MMDB_entry_data_s city_data = {};
		int status = MMDB_get_value(&result.entry, &city_data, "city", "names", "en", nullptr);

		if (status == MMDB_SUCCESS && city_data.has_data) {
			// If the city is found, add it to the WHOIS response
			std::string city(city_data.utf8_string, city_data.data_size);
			whois.SendLine(RPL_WHOISSPECIAL, "*", "is connecting from City: " + city);
		} else {
			// City not found
			whois.SendLine(RPL_WHOISSPECIAL, "*", "is connecting from City: Unknown.");
		}
	}

	~ModuleWhoisGeoLite() override
	{
		// Close the MaxMind database when the module is unloaded
		if (db_loaded) {
			MMDB_close(&mmdb);
		}
	}
};

MODULE_INIT(ModuleWhoisGeoLite)
