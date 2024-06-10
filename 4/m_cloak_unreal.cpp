/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2024 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: InspIRCd Developers
/// $ModConfig: <cloak method="unreal-md5|unreal-md5-ip|unreal-sha256|unreal-sha256-ip" key1="foo" key2="bar" key3="baz" prefix="Clk">
/// $ModDepends: core 4
/// $ModDesc: Adds the unreal-md5, unreal-md5-ip, unreal-sha256, and unreal-sha256-ip cloaking methods for use with the cloak module.

#include "inspircd.h"
#include "modules/cloak.h"
#include "modules/hash.h"

class UnrealMethod final
	: public Cloak::Method
{
private:
	// Whether to cloak the hostname if available.
	const bool cloakhost;

	// Dynamic reference to the hash implementation.
	dynamic_reference_nocheck<HashProvider>& hash;

	// The secrets used for generating cloaks.
	const std::string key1;
	const std::string key2;
	const std::string key3;

	// The prefix for host cloaks (e.g. MyNet).
	const std::string prefix;

	std::string CloakAddress(const irc::sockets::sockaddrs& sa)
	{
		switch (sa.family())
		{
			case AF_INET:
				return CloakIPv4(sa.in4.sin_addr.s_addr);
			case AF_INET6:
				return CloakIPv6(sa.in6.sin6_addr.s6_addr);
		}

		// Probably AF_UNIX?
		return {};
	}

	std::string CloakIPv4(unsigned long address)
	{
		unsigned int a = (unsigned int)(address)       & 0xFF;
		unsigned int b = (unsigned int)(address >> 8)  & 0xFF;
		unsigned int c = (unsigned int)(address >> 16) & 0xFF;
		unsigned int d = (unsigned int)(address >> 24) & 0xFF;

		const auto alpha1 = hash->GenerateRaw(INSP_FORMAT("{}:{}.{}.{}.{}:{}", key2, a, b, c, d, key3));
		const auto beta1  = hash->GenerateRaw(INSP_FORMAT("{}:{}.{}.{}:{}", key3, a, b, c, key1));
		const auto gamma1 = hash->GenerateRaw(INSP_FORMAT("{}:{}.{}:{}", key1, a, b, key2));

		const auto alpha2 = hash->GenerateRaw(alpha1 + key1);
		const auto beta2 = hash->GenerateRaw(beta1 + key2);
		const auto gamma2 = hash->GenerateRaw(gamma1 + key3);

		return INSP_FORMAT("{:X}.{:X}.{:X}.IP", Downsample(alpha2), Downsample(beta2), Downsample(gamma2));
	}

	std::string CloakIPv6(const unsigned char* address)
	{
		const uint16_t* address16 = reinterpret_cast<const uint16_t*>(address);
		unsigned int a = ntohs(address16[0]);
		unsigned int b = ntohs(address16[1]);
		unsigned int c = ntohs(address16[2]);
		unsigned int d = ntohs(address16[3]);
		unsigned int e = ntohs(address16[4]);
		unsigned int f = ntohs(address16[5]);
		unsigned int g = ntohs(address16[6]);
		unsigned int h = ntohs(address16[7]);

		const auto alpha1 = hash->GenerateRaw(INSP_FORMAT("{}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{}", key2, a, b, c, d, e, f, g, h, key3));
		const auto beta1  = hash->GenerateRaw(INSP_FORMAT("{}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{}", key3, a, b, c, d, e, f, g, key1));
		const auto gamma1 = hash->GenerateRaw(INSP_FORMAT("{}:{:x}:{:x}:{:x}:{:x}:{}", key1, a, b, c, d, key2));

		const auto alpha2 = hash->GenerateRaw(alpha1 + key1);
		const auto beta2 = hash->GenerateRaw(beta1 + key2);
		const auto gamma2 = hash->GenerateRaw(gamma1 + key3);

		return INSP_FORMAT("{:X}:{:X}:{:X}:IP", Downsample(alpha2), Downsample(beta2), Downsample(gamma2));
	}

	std::string CloakHost(const std::string& host)
	{
		const auto alpha1 = hash->GenerateRaw(INSP_FORMAT("{}:{}:{}", key1, host, key2));
		const auto alpha2 = hash->GenerateRaw(alpha1 + key3);
		const auto suffix = Cloak::VisiblePart(host, SIZE_MAX, '.');
		if (suffix.empty())
			return INSP_FORMAT("{}-{:X}", prefix, Downsample(alpha2));
		return INSP_FORMAT("{}-{:X}.{}", prefix, Downsample(alpha2), suffix);
	}

	static unsigned int Downsample16(const std::string& hash)
	{
		uint8_t b1 = hash[0] ^ hash[1] ^ hash[2] ^ hash[3];
		uint8_t b2 = hash[4] ^ hash[5] ^ hash[6] ^ hash[7];
		uint8_t b3 = hash[8] ^ hash[9] ^ hash[10] ^ hash[11];
		uint8_t b4 = hash[12] ^ hash[13] ^ hash[14] ^ hash[15];

		return (
			((unsigned int)b1 << 24) + ((unsigned int)b2 << 16) +
			((unsigned int)b3 << 8) + ((unsigned int)b4)
		);
	}

	static unsigned int Downsample32(const std::string& hash)
	{
		uint8_t b1 = hash[0] ^ hash[1] ^ hash[2] ^ hash[3] ^ hash[4] ^ hash[5] ^ hash[6] ^ hash[7];
		uint8_t b2 = hash[8] ^ hash[9] ^ hash[10] ^ hash[11] ^ hash[12] ^ hash[13] ^ hash[14] ^ hash[15];
		uint8_t b3 = hash[16] ^ hash[17] ^ hash[18] ^ hash[19] ^ hash[20] ^ hash[21] ^ hash[22] ^ hash[23];
		uint8_t b4 = hash[24] ^ hash[25] ^ hash[26] ^ hash[27] ^ hash[28] ^ hash[29] ^ hash[30] ^ hash[31];

		return (
			((unsigned int)b1 << 24) + ((unsigned int)b2 << 16) +
			((unsigned int)b3 << 8) + ((unsigned int)b4)
		);
	}

	static unsigned int Downsample(const std::string& hash)
	{
		switch (hash.size())
		{
			case 16: // MD5
				return Downsample16(hash);
			case 32: // SHA256
				return Downsample32(hash);
		}
		return 0; // BUG
	}

public:
	UnrealMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, dynamic_reference_nocheck<HashProvider>& h, bool ch) ATTR_NOT_NULL(2)
		: Cloak::Method(engine, tag)
		, cloakhost(ch)
		, hash(h)
		, key1(tag->getString("key1"))
		, key2(tag->getString("key2"))
		, key3(tag->getString("key3"))
		, prefix(ch ? tag->getString("prefix") : "")
	{
	}

	void GetLinkData(Module::LinkData& data, std::string& compatdata) override
	{
		// The value we use for cloaks when the hash module is missing.
		const std::string broken = "missing-hash-module";

		// IMPORTANT: link data is sent over unauthenticated server links so we
		// can't directly send the key here. Instead we use dummy cloaks that
		// allow verification of or less the same thing.
		data["cloak-v4"] = hash ? Generate("123.123.123.123")                        : broken;
		data["cloak-v6"] = hash ? Generate("dead:beef:cafe::")                       : broken;

		if (cloakhost)
		{
			data["cloak-host"] = hash ? Generate("extremely.long.inspircd.cloak.example")  : broken;
			data["prefix"]     = prefix;
		}
	}

	std::string Generate(LocalUser* user) override ATTR_NOT_NULL(2)
	{
		if (!hash || !MatchesUser(user))
			return {};

		irc::sockets::sockaddrs sa(false);
		if (!cloakhost || (sa.from(user->GetRealHost()) && sa.addr() == user->client_sa.addr()))
			return CloakAddress(user->client_sa);

		return CloakHost(user->GetRealHost());
	}

	std::string Generate(const std::string& hostip) override
	{
		if (!hash)
			return {};

		irc::sockets::sockaddrs sa(false);
		if (sa.from(hostip))
			return CloakAddress(sa);

		if (cloakhost)
			return CloakHost(hostip);

		return {}; // Only reachable on unreal-{md5,sha256}-ip.
	}
};

class UnrealEngine final
	: public Cloak::Engine
{
private:
	// Whether to cloak the hostname if available.
	const bool cloakhost;

	// Dynamic reference to the hash implementation.
	dynamic_reference_nocheck<HashProvider>& hash;

public:
	UnrealEngine(Module* Creator, const std::string& Name, dynamic_reference_nocheck<HashProvider>& h, bool ch)
		: Cloak::Engine(Creator, Name)
		, cloakhost(ch)
		, hash(h)
	{
	}

	Cloak::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag, bool primary) override
	{
		if (primary)
			throw ModuleException(creator, "The " + name.substr(6) + " cloak engine can not be used as a primary cloak engine!");

		if (!hash)
			throw ModuleException(creator, "Unable to create a " + name.substr(6) + " cloak without the " + hash.GetProvider().substr(5) + " module, at" + tag->source.str());

		return std::make_shared<UnrealMethod>(this, tag, hash, cloakhost);
	}
};

class ModuleCloakUnreal final
	: public Module
{
private:
	dynamic_reference_nocheck<HashProvider> md5;
	UnrealEngine md5hostcloak;
	UnrealEngine md5ipcloak;

	dynamic_reference_nocheck<HashProvider> sha256;
	UnrealEngine sha256hostcloak;
	UnrealEngine sha256ipcloak;

public:
	ModuleCloakUnreal()
		: Module(VF_VENDOR, "Adds the unreal-md5, unreal-md5-ip, unreal-sha256, and unreal-sha256-ip cloaking methods for use with the cloak module.")
		, md5(this, "hash/md5")
		, md5hostcloak(this, "unreal-md5", md5, true)
		, md5ipcloak(this, "unreal-md5-ip", md5, false)
		, sha256(this, "hash/sha256")
		, sha256hostcloak(this, "unreal-sha256", sha256, true)
		, sha256ipcloak(this, "unreal-sha256-ip", sha256, false)
	{
	}
};

MODULE_INIT(ModuleCloakUnreal)
