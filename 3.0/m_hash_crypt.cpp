/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 * Copyright (C) 2020 Elizabeth Myers
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

/* Implements the crypt(3) hash provider.
 * This is useful for old passwords hashed in the crypt format ($6$ etc.). This
 * relies on your system crypt(3).
 *
 * See https://en.wikipedia.org/wiki/Crypt_(C) for information on what your
 * system supports.
 *
 * This module does not (yet) work on Windows as it does not provide crypt(3).
 * It also refuses to build on MacOS as crypt(3) is incredibly insecure on this
 * platform.
 *
 * The following algorithms are implemented:
 * - crypt-generic (passthru to system crypt(3)): this is only intended for
 *   checking passwords that might be in an unimplemented algorithm, such as
 *   MD5
 * - crypt-sha256 ($5$) (if your system supports it)
 * - crypt-sha512 ($6$) (if your system supports it)
 *
 * It is strongly advised you avoid insecure password formats such as the old
 * DES one and MD5.
 *
 * Do NOT use /MKPASSWD with crypt-generic! It will use the old DES scheme, and
 * you definitely don't want this.
 *
 * bcrypt ($2a$) support can be found in the bcrypt module.
 *
 * In a future update, I might add support to fill in the gaps in various
 * operating systems.
 *
 * -- Elizafox, 27 November 2020
 */

/// $ModAuthor: Elizabeth Myers
/// $ModAuthorMail: elizabeth@interlinked.me
/// $ModDesc: Implements hash functions using crypt(3)
/// $ModDepends: core 3
/// $LinkerFlags: require_system("linux") -lcrypt
/// $LinkerFlags: require_system("netbsd") -lcrypt
/// $LinkerFlags: require_system("freebsd") -lcrypt


#include "inspircd.h"
#include "modules/hash.h"

#include <unistd.h>

#ifdef __APPLE__
#	error "crypt(3) is insecure on this platform, refusing to build."
#endif


/* Check if we can use the SHA algorithms.
 * Feel free to add your platform to this if you're sure it works, and let me know.
 *
 * -- Elizafox
 */
#if defined(__linux__) || defined(__FreeBSD__) || defined(__sun)
	/* All the major libc's on Linux support SHA passwords.
	 *
	 * FreeBSD 8.3+ has support for this, you shouldn't use anything older
	 * than that as it's unsupported.
	 *
	 * Strangely, Solaris supports these too.
	 */
#	define HAS_SHA 1
#endif


class CryptHashProvider : public HashProvider
{
	const std::string hash_id;
	const size_t salt_size;
	size_t rounds;

	std::string Salt()
	{
		std::string salt = hash_id;
		if(rounds)
		{
			// Valid for SHA at least
			salt += InspIRCd::Format("rounds=%zu$", rounds);
		}

		salt += BinToBase64(ServerInstance->GenRandomStr(salt_size, false));
		return salt;
	}

	std::string Generate(const std::string& data, const std::string& salt)
	{
		return crypt(data.c_str(), salt.c_str());
	}

public:
	CryptHashProvider(Module* parent, const std::string& Name, const std::string& id, size_t ssize)
		: HashProvider(parent, Name)
		, hash_id(id)
		, salt_size(ssize)
		, rounds(0)

	{
		/* Check if we have a sham crypt(1)
		 * This is unlikely to happen with most systems, but better to
		 * be safe than sorry.
		 *
		 * If this does happen, POSIX says you'll only get ENOSYS back.
		 * This isn't true on Linux, where we can get EPERM (if DES is
		 * disallowed).
		 */
		if(!crypt("abc", "12") && errno == ENOSYS)
			throw ModuleException("crypt(3) is not implemented on your system, likely due to export restrictions.");

		// Smoke test to make sure the hash *really* works.
		if(!hash_id.empty())
		{
			std::string test_hash = GenerateRaw("abc");
			if(hash_id != test_hash.substr(0, hash_id.size()))
			{
				throw ModuleException("Hash %s does not work with your crypt implementation.");
			}
		}
	}

	bool Compare(const std::string& input, const std::string& hash) CXX11_OVERRIDE
	{
		return InspIRCd::TimingSafeCompare(Generate(input, hash), hash);
	}

	std::string GenerateRaw(const std::string& data) CXX11_OVERRIDE
	{
		return Generate(data, Salt());
	}

	std::string ToPrintable(const std::string& raw) CXX11_OVERRIDE
	{
		// No need to do anything
		return raw;
	}

	void SetRounds(size_t r)
	{
		rounds = r;
	}
};

class ModuleHashCrypt : public Module
{
	CryptHashProvider cryptprov_generic;
#ifdef HAS_SHA
	CryptHashProvider cryptprov_sha256;
	CryptHashProvider cryptprov_sha512;
#endif

public:
	ModuleHashCrypt()
		: cryptprov_generic(this, "crypt-generic", "", 2)
#ifdef HAS_SHA
		, cryptprov_sha256(this, "crypt-sha256", "$5$", 16)
		, cryptprov_sha512(this, "crypt-sha512", "$6$", 16)
#endif
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
#ifdef HAS_SHA
		// 1000 is the lowest amount the algorithms will take
		ConfigTag* tag = ServerInstance->Config->ConfValue("crypt");
		long rounds = tag->getInt("rounds", 0, 1000);

		tag = ServerInstance->Config->ConfValue("cryptsha256");
		long rounds_sha256 = tag->getInt("rounds", 0, 1000);

		tag = ServerInstance->Config->ConfValue("cryptsha512");
		long rounds_sha512 = tag->getInt("rounds", 0, 1000);

		if(rounds && !rounds_sha256)
			rounds_sha256 = rounds;

		if(rounds && !rounds_sha512)
			rounds_sha512 = rounds;

		cryptprov_sha256.SetRounds(rounds_sha256);
		cryptprov_sha512.SetRounds(rounds_sha512);
#endif
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements hash functions using crypt(3)");
	}
};

MODULE_INIT(ModuleHashCrypt)
