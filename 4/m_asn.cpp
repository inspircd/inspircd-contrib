/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell <sadie@witchery.services>
/// $ModDepends: core 4
/// $ModDesc: Allows banning users based on Autonomous System number.


#include "inspircd.h"
#include "extension.h"
#include "modules/dns.h"
#include "modules/extban.h"
#include "modules/stats.h"
#include "modules/whois.h"

enum
{
	// InspIRCd-specific.
	RPL_WHOISASN = 569,
	RPL_STATSASN = 800,
};

class ASNExtBan final
	: public ExtBan::MatchingBase
{
private:
	IntExtItem& asnext;

public:
	ASNExtBan(Module* Creator, IntExtItem& asn)
		: ExtBan::MatchingBase(Creator, "asn", 'n')
		, asnext(asn)
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		const std::string asnstr = ConvToStr(asnext.Get(user));
		return stdalgo::string::equalsci(asnstr, text);
	}
};

class ASNResolver final
	: public DNS::Request
{
private:
	irc::sockets::sockaddrs theirsa;
	std::string theiruuid;
	IntExtItem& asnext;
	BoolExtItem& asnpendingext;

	std::string GetDNS(LocalUser* user)
	{
		std::stringstream buffer;
		switch (user->client_sa.family())
		{
			case AF_INET:
			{
				unsigned int d = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 24) & 0xFF;
				unsigned int c = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 16) & 0xFF;
				unsigned int b = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 8) & 0xFF;
				unsigned int a = (unsigned int) user->client_sa.in4.sin_addr.s_addr & 0xFF;
				buffer << d << '.' << c << '.' << b << '.' << a << ".origin.asn.cymru.com";
				break;
			}
			case AF_INET6:
			{
				const std::string hexip = Hex::Encode(user->client_sa.in6.sin6_addr.s6_addr, 16);
				for (const auto hexchr : hexip)
					buffer << hexchr << '.';
				buffer << "origin6.asn.cymru.com";
				break;
			}
			default:
				break;
		}
		return buffer.str();
	}


public:
	ASNResolver(DNS::Manager* dns, Module* Creator, LocalUser* user, IntExtItem& asn, BoolExtItem& asnpending)
		: DNS::Request(dns, Creator, GetDNS(user), DNS::QUERY_TXT, true)
		, theirsa(user->client_sa)
		, theiruuid(user->uuid)
		, asnext(asn)
		, asnpendingext(asnpending)
	{
	}

	void OnLookupComplete(const DNS::Query* result) override
	{
		auto* them = ServerInstance->Users.FindUUID<LocalUser>(theiruuid);
		if (!them || them->client_sa != theirsa)
			return;

		// The DNS reply must contain an TXT result.
		const DNS::ResourceRecord* record = result->FindAnswerOfType(DNS::QUERY_TXT);
		if (!record)
		{
			asnpendingext.Unset(them);
			return;
		}

		size_t pos = record->rdata.find_first_not_of("0123456789");
		intptr_t asn = ConvToNum<uintptr_t>(record->rdata.substr(0, pos));
		asnext.Set(them, asn);
		asnpendingext.Unset(them);
		ServerInstance->Logs.Debug(MODNAME, "ASN for {} ({}) is {}", them->uuid, them->GetAddress(), asn);
	}

	void OnError(const DNS::Query* query) override
	{
		auto* them = ServerInstance->Users.FindUUID<LocalUser>(theiruuid);
		if (!them || them->client_sa != theirsa)
			return;

		asnpendingext.Unset(them);
		ServerInstance->SNO.WriteGlobalSno('a', "ASN lookup error for {}: {}",
			them->GetAddress(), manager->GetErrorStr(query->error));
	}
};

class ModuleASN final
	: public Module
	, public Stats::EventListener
	, public Whois::EventListener
{
private:
	IntExtItem asnext;
	ASNExtBan asnextban;
	BoolExtItem asnpendingext;
	DNS::ManagerRef dns;

public:
	ModuleASN()
		: Module(VF_OPTCOMMON, "Allows banning users based on Autonomous System number.")
		, Stats::EventListener(this)
		, Whois::EventListener(this)
		, asnext(this, "asn", ExtensionType::USER, true)
		, asnextban(this, asnext)
		, asnpendingext(this, "asn-pending", ExtensionType::USER)
		, dns(this)
	{
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		// Block until ASN info is available.
		return asnpendingext.Get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	void OnChangeRemoteAddress(LocalUser* user) override
	{
		if (user->quitting)
			return;

		if (!user->GetClass() || !user->GetClass()->config->getBool("useasn", true))
			return;

		asnext.Unset(user);
		if (!user->client_sa.is_ip())
			return;

		auto* resolver = new ASNResolver(*dns, this, user, asnext, asnpendingext);
		try
		{
			asnpendingext.Set(user);
			dns->Process(resolver);
		}
		catch (DNS::Exception& error)
		{
			asnpendingext.Unset(user);
			delete resolver;
			ServerInstance->SNO.WriteGlobalSno('a', "ASN lookup error for {}: {}",
				user->GetAddress(), error.GetReason());
		}
	}

	ModResult OnPreChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, std::optional<Numeric::Numeric>& errnum) override
	{
		const std::string asn = klass->config->getString("asn");
		if (asn.empty())
			return MOD_RES_PASSTHRU;

		const std::string asnstr = ConvToStr(asnext.Get(user));
		irc::spacesepstream asnstream(asn);
		for (std::string token; asnstream.GetToken(token); )
		{
			// If the user matches this ASN then they can use this connect class.
			if (stdalgo::string::equalsci(asnstr, token))
				return MOD_RES_PASSTHRU;
		}

		// A list of ASNs were specified but the user didn't match any of them.
		ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as the origin ASN ({}) is not any of {}",
			klass->GetName(), asnstr, asn);
		return MOD_RES_DENY;
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() != 'b')
			return MOD_RES_PASSTHRU;

		std::map<intptr_t, size_t> counts;
		for (const auto& [_, u] : ServerInstance->Users.GetUsers())
		{
			intptr_t asn = asnext.Get(u);
			if (!counts.insert(std::make_pair(asn, 1)).second)
				counts[asn]++;
		}

		for (const auto& [asn, count] : counts)
			stats.AddRow(RPL_STATSASN, asn, count);
		return MOD_RES_DENY;
	}

	void OnWhois(Whois::Context& whois) override
	{
		if (whois.GetTarget()->server->IsService())
			return;

		intptr_t asn = asnext.Get(whois.GetTarget());
		if (asn)
			whois.SendLine(RPL_WHOISASN, asn, "is connecting from AS" + ConvToStr(asn));
		else
			whois.SendLine(RPL_WHOISASN, "*", "is connecting from an unknown autonomous system");
	}
};

MODULE_INIT(ModuleASN)
