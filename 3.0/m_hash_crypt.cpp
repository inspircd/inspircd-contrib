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
 * This module does not (yet) work on Windows. It's also kind of useless on
 * MacOS as it doesn't really support any modern or secure formats.
 *
 * DES is supported for compatibility (but really should not be used). $2a$ is
 * not implemented; use the bcrypt module for that. Likewise, $3$ is not
 * implemented either as it is a very rarely used format and is easily broken.
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

/* Use a basic whitelist to determine what platforms can support what
 * It isn't pretty, but that's the way this is.
 */
#if defined(__linux__)
	// Musl and glibc support these
#	define HAS_MD5 1
#	define HAS_SHA 1
#elif defined(__FreeBSD__) || defined(__sun)
	/* FreeBSD 8.3+ has support for this, you shouldn't use anything older
	 * than that as it's unsupported.
	 *
	 * Strangely, Solaris supports these too.
	 */
#	define HAS_MD5 1
#	define HAS_SHA 1
#	define HAS_BLOWFISH 1
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	// No support for SHA :(
#	define HAS_MD5 1
#	define HAS_BLOWFISH 1
#endif


class CryptHashProvider : public HashProvider
{
	const std::string hash_id;
	const size_t salt_size;

	std::string Salt()
	{
		return hash_id + BinToBase64(ServerInstance->GenRandomStr(salt_size, false));
	}
	
	std::string Generate(const std::string& data, const std::string& salt)
	{
		return crypt(data.c_str(), salt.c_str());
	}

public:
	CryptHashProvider(Module* parent, const std::string& Name, const std::string& id, const size_t ssize)
		: HashProvider(parent, Name)
		, hash_id(id)
		, salt_size(ssize)

	{
		// Run a self-test
		std::string test_hash = GenerateRaw("abc");
		if(!hash_id.empty() && hash_id != test_hash.substr(0, hash_id.size()))
			// This shouldn't happen 
			throw ModuleException("Hash %s does not work with your crypt implementation.");
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
};

class ModuleHashCrypt : public Module
{
	CryptHashProvider cryptprov_des;
#ifdef HAS_MD5
	CryptHashProvider cryptprov_md5;
#endif
#ifdef HAS_BLOWFISH
	CryptHashProvider cryptprov_blowfish;
#endif
#ifdef HAS_SHA
	CryptHashProvider cryptprov_sha256;
	CryptHashProvider cryptprov_sha512;
#endif

public:
	ModuleHashCrypt()
		: cryptprov_des(this, "hash/crypt-des", "", 2)  // DES is everywhere, although it's shite...
#ifdef HAS_MD5
		, cryptprov_md5(this, "hash/crypt-md5", "$1$", 8)
#endif
#ifdef HAS_BLOWFISH
		, cryptprov_blowfish(this, "hash/crypt-blowfish", "$2$", 22)
#endif
#ifdef HAS_SHA
		, cryptprov_sha256(this, "hash/crypt-sha256", "$5$", 16)
		, cryptprov_sha512(this, "hash/crypt-sha512", "$6$", 16)
#endif
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements hash functions using crypt(3)");
	}
};

MODULE_INIT(ModuleHashCrypt)
