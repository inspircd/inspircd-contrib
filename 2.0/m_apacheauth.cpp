/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011-2016 Collabora Ltd <vincent@collabora.co.uk>
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

#include <fstream>
#include "inspircd.h"
#include "hash.h"

/* $ModDesc: Allow/Deny connections based upon an apache auth file */
/* $LinkerFlags: -lcrypt */

class ModuleApacheAuth : public Module
{
	std::string authfile;
	std::string killreason;
	std::string allowpattern;
	bool verbose;
	struct HashedPassword {
		std::string algorithm;
		std::string salt;
		std::string hash;
		std::string hashed;
	};
	std::map<std::string, HashedPassword> logins;

 public:
	ModuleApacheAuth()
	{
	}

	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	void OnRehash(User* user)
	{
		ConfigTag* conf = ServerInstance->Config->ConfValue("apacheauth");
		authfile = conf->getString("authfile");
		killreason = conf->getString("killreason");
		allowpattern = conf->getString("allowpattern");
		verbose = conf->getBool("verbose");
		LoadAuthFile();
	}

	bool SplitRecord(const std::string &record, std::string &record_algorithm, std::string &record_salt, std::string &record_hash)
	{
		size_t i[3];

		for (size_t pos = 0, n = 0; n < 3; ++n) {
			i[n] = pos = record.find('$', pos);
			if (pos == std::string::npos) return false;
			++pos;
		}
		record_algorithm = record.substr(i[0] + 1, i[1]-i[0] - 1);
		record_salt = record.substr(i[1] + 1, i[2]-i[1] - 1);
		record_hash = record.substr(i[2] + 1);
		return true;
	}

	void LoadAuthFile()
	{
		int line=0;
		std::ifstream f;

		logins.clear();
		ServerInstance->SNO->WriteGlobalSno('a', "Loading auth file %s",authfile.c_str());
		f.open(authfile.c_str(), std::ifstream::in);
		if (!f.good()) {
			ServerInstance->SNO->WriteGlobalSno('a', "Auth file failed to open, no connections will be allowed");
			return;
		}
		std::string s;
		while (std::getline(f, s)) {
			++line;
			size_t pos = s.find(':');
			if (pos == std::string::npos) {
				ServerInstance->SNO->WriteGlobalSno('a', "Syntax error at line %d", line);
				continue;
			}
			std::string login = s.substr(0, pos);
			std::string hashed = s.substr(pos + 1);
			if (verbose) {
				ServerInstance->SNO->WriteGlobalSno('a', "Found login %s, hash %s", login.c_str(), hashed.c_str());
			}
			if (logins.find(login) != logins.end()) {
				ServerInstance->SNO->WriteGlobalSno ('a', "Warning: ignoring duplicate login: %s", login.c_str());
			}
			else {
				HashedPassword p;
				p.hashed = hashed;
				if (!SplitRecord(hashed, p.algorithm, p.salt, p.hash)) {
					ServerInstance->SNO->WriteGlobalSno ('a', "Warning: ignoring malformed line: %s", hashed.c_str());
				}
				else if (p.algorithm != "1") { // MD5
					ServerInstance->SNO->WriteGlobalSno ('a', "Warning: ignoring unsupported algorithm: %s", p.algorithm.c_str());
				}
				else {
					logins.insert(std::make_pair(login, p));
				}
			}
		}
		ServerInstance->SNO->WriteGlobalSno ('a', "Done C++ loading auth file, %u users", (unsigned)logins.size());
		f.close();
	}

	ModResult OnUserRegister(LocalUser* user)
	{
		// Note this is their initial (unresolved) connect block
		ConfigTag* tag = user->MyClass->config;
		if (!tag->getBool("useapacheauth", true))
			return MOD_RES_PASSTHRU;

		if (!allowpattern.empty() && InspIRCd::Match(user->ident,allowpattern))
			return MOD_RES_PASSTHRU;

		std::map<std::string, HashedPassword>::const_iterator i = logins.find(user->ident);
		if (i == logins.end()) {
			ServerInstance->SNO->WriteGlobalSno('a', "Denying connection from %s!%s@%s (login not found)",
				user->nick.c_str(), user->ident.c_str(), user->host.c_str());
			ServerInstance->Users->QuitUser(user, killreason);
			return MOD_RES_PASSTHRU;
		}

		const HashedPassword &p = i->second;
		std::string algorithm_and_salt = std::string("$") + p.algorithm + std::string("$") + p.salt;
		std::string hashed = crypt(user->password.c_str(),algorithm_and_salt.c_str());
		if (hashed != p.hashed) {
			ServerInstance->SNO->WriteGlobalSno('a', "Forbiding connection from %s!%s@%s (invalid password)",
				user->nick.c_str(), user->ident.c_str(), user->host.c_str());
			ServerInstance->Users->QuitUser(user, killreason);
			return MOD_RES_PASSTHRU;
		}
		ServerInstance->SNO->WriteGlobalSno('a', "Granting access to connection from %s!%s@%s",
				user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Allow/Deny connections based upon an Apache auth file", VF_NONE);
	}
};

MODULE_INIT(ModuleApacheAuth)

