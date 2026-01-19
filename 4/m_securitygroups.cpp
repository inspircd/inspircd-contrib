/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   (C) 2026 reverse - mike.chevronnet@gmail.com
 *
 * Implements UnrealIRCd 6-style security-groups for InspIRCd 4.
 *
 * Configuration example:
 *
 *   <securitygroup name="webirc-users"
 *     mask="*@webirc.example.com"
 *     webirc="yes"
 *     scoremin="0"
 *     scoremax="100"
 *     public="yes">
 *
 * Supported matching options:
 * - mask: One or more hostmask globs or CIDR ranges (space-separated).
 * - account/registered: Requires the user to be logged into an account.
 * - tls/tls-users: Requires the user to be using TLS.
 * - insecure/insecure-users: Requires the user to NOT be using TLS.
 * - websocket/websocket-users: Requires the user to be connected over WebSocket.
 * - webirc/webirc-users: Requires the user to have authenticated via WEBIRC.
 *
 * Exposes:
 * - user extension "securitygroups" (comma-separated list, synced across the network)
 * - /SECURITYGROUPS [nick] command
 */

/// $ModAuthor: reverse - mike.chevronnet@gmail.com
/// $ModDepends: core 4
/// $ModDesc: Implements UnrealIRCd-style security-groups for InspIRCd 4.
/// $ModConfig: <securitygroup name="example" mask="*@example.com" account="no" tls="no" insecure="no" websocket="no" webirc="no" public="yes">

#include "inspircd.h"
#include "extension.h"
#include "numerichelper.h"
#include "modules/account.h"
#include "modules/extban.h"
#include "modules/ssl.h"
#include "modules/webirc.h"
#include "modules/whois.h"

// One or more hostmask globs or CIDR ranges.
typedef std::vector<std::string> MaskList;

// A set of security group names.
typedef insp::flat_set<std::string, irc::insensitive_swo> SecurityGroupList;

static std::string FormatSecurityGroupList(const SecurityGroupList* list)
{
	return (list && !list->empty()) ? insp::join(*list, ',') : "none";
}

struct SecurityGroup final
{
	std::string name;
	MaskList masks;
	bool require_account = false;
	bool require_tls = false;
	bool require_insecure = false;
	bool require_websocket = false;
	bool require_webirc = false;
	int score_min = -1;
	int score_max = -1;
	bool publicgroup = false;
};

class CommandSecurityGroups final
	: public Command
{
private:
	ListExtItem<SecurityGroupList>& allgroups;
	ListExtItem<SecurityGroupList>& publicgroups;

public:
	CommandSecurityGroups(Module* Creator, ListExtItem<SecurityGroupList>& all, ListExtItem<SecurityGroupList>& pub)
		: Command(Creator, "SECURITYGROUPS", 0, 1)
		, allgroups(all)
		, publicgroups(pub)
	{
		syntax = { "[<nick>]" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		User* target = user;
		bool self = true;

		if (!parameters.empty())
		{
			target = ServerInstance->Users.FindNick(parameters[0]);
			if (!target)
			{
				user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
				return CmdResult::FAILURE;
			}
			self = (target == user);
		}

		const bool canseeall = self || user->HasPrivPermission("users/auspex");
		const SecurityGroupList* list = canseeall ? allgroups.Get(target) : publicgroups.Get(target);
		user->WriteNotice(INSP_FORMAT("Security groups for {}: {}",
			target->nick, FormatSecurityGroupList(list)));
		return CmdResult::SUCCESS;
	}
};

class ModuleSecurityGroups final
	: public Module
	, public Account::EventListener
	, public WebIRC::EventListener
	, public Whois::EventListener
{
private:
	class SecurityGroupExtBan final
		: public ExtBan::MatchingBase
	{
	private:
		ListExtItem<SecurityGroupList>& sgext;

	public:
		SecurityGroupExtBan(Module* Creator, ListExtItem<SecurityGroupList>& ext)
			: ExtBan::MatchingBase(Creator, "securitygroup", 'g')
			, sgext(ext)
		{
		}

		bool IsMatch(User* user, Channel* channel, const std::string& text) override
		{
			const SecurityGroupList* list = sgext.Get(user);
			if (!list || list->empty() || text.empty())
				return false;

			for (const auto& group : *list)
			{
				if (InspIRCd::Match(group, text, ascii_case_insensitive_map))
					return true;
			}

			return false;
		}
	};

	std::vector<SecurityGroup> groups;
	bool usesreputation = false;

	Account::API accountapi;
	UserCertificateAPI sslapi;
	BoolExtItem webircext;
	ListExtItem<SecurityGroupList> sgext;
	ListExtItem<SecurityGroupList> sgpublicext;
	SecurityGroupExtBan extban;
	CommandSecurityGroups cmd;

	static bool HasExt(const Extensible* ext, const std::string& extname)
	{
		ExtensionItem* item = ServerInstance->Extensions.GetItem(extname);
		if (!item)
			return false;

		const auto& extlist = ext->GetExtList();
		return extlist.find(item) != extlist.end();
	}

	static bool IsWebSocketUser(LocalUser* user)
	{
		// Prefer checking the websocket module's extension items as this is
		// stable even when hook ordering differs (testing).
		if (HasExt(user, "websocket-origin") || HasExt(user, "websocket-realhost") || HasExt(user, "websocket-realip"))
			return true;

		// Fallback: check if the user's socket has a websocket I/O hook.
		for (IOHook* hook = user->eh.GetIOHook(); hook; )
		{
			if (hook->prov && insp::equalsci(hook->prov->name, "websocket"))
				return true;

			IOHookMiddle* middle = IOHookMiddle::ToMiddleHook(hook);
			hook = middle ? middle->GetNextHook() : nullptr;
		}
		return false;
	}

	static bool MatchesMask(LocalUser* user, const std::string& mask)
	{
		if (mask.empty())
			return true;

		// Check common user masks.
		if (InspIRCd::Match(user->GetRealMask(), mask))
			return true;
		if (InspIRCd::Match(user->GetRealUserHost(), mask))
			return true;
		if (InspIRCd::Match(user->GetUserHost(), mask))
			return true;
		if (InspIRCd::Match(user->GetRealHost(), mask))
			return true;

		// Check IP/CIDR.
		if (InspIRCd::MatchCIDR(user->GetAddress(), mask))
			return true;

		return false;
	}

	static IntExtItem* GetReputationExt()
	{
		auto* extitem = ServerInstance->Extensions.GetItem("reputation");
		if (!extitem || extitem->extype != ExtensionType::USER)
			return nullptr;

		return static_cast<IntExtItem*>(extitem);
	}

	bool Matches(LocalUser* user, const SecurityGroup& group)
	{
		if (!group.masks.empty())
		{
			bool anymask = false;
			for (const auto& mask : group.masks)
			{
				if (MatchesMask(user, mask))
				{
					anymask = true;
					break;
				}
			}
			if (!anymask)
				return false;
		}

		if (group.require_webirc && !webircext.Get(user))
			return false;

		if (group.require_websocket && !IsWebSocketUser(user))
			return false;

		const bool istls = sslapi ? sslapi->IsSecure(user) : (SSLIOHook::IsSSL(&user->eh) != nullptr);
		if (group.require_tls && !istls)
			return false;
		if (group.require_insecure && istls)
			return false;

		if (group.require_account)
		{
			if (!accountapi)
				return false;
			if (!accountapi->GetAccountName(user))
				return false;
		}

		if (group.score_min != -1 || group.score_max != -1)
		{
			IntExtItem* repouserext = GetReputationExt();
			if (!repouserext)
				return false;

			const intptr_t raw = repouserext->Get(user);
			const int score = static_cast<int>(std::max<intptr_t>(0, raw));
			if (group.score_min != -1 && score < group.score_min)
				return false;
			if (group.score_max != -1 && score > group.score_max)
				return false;
		}

		return true;
	}

	void Rebuild(LocalUser* user)
	{
		SecurityGroupList matched;
		SecurityGroupList matchedpublic;

		for (const auto& group : groups)
		{
			if (!Matches(user, group))
				continue;

			matched.insert(group.name);
			if (group.publicgroup)
				matchedpublic.insert(group.name);
		}

		if (matched.empty())
			sgext.Unset(user);
		else
			sgext.Set(user, matched);

		if (matchedpublic.empty())
			sgpublicext.Unset(user);
		else
			sgpublicext.Set(user, matchedpublic);
	}

public:
	ModuleSecurityGroups()
		: Module(VF_VENDOR, "Implements security-groups for InspIRCd 4")
		, Account::EventListener(this)
		, WebIRC::EventListener(this)
		, Whois::EventListener(this)
		, accountapi(this)
		, sslapi(this)
		, webircext(this, "securitygroups-webirc", ExtensionType::USER)
		, sgext(this, "securitygroups", ExtensionType::USER, true)
		, sgpublicext(this, "securitygroups-public", ExtensionType::USER, true)
		, extban(this, sgext)
		, cmd(this, sgext, sgpublicext)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		std::vector<SecurityGroup> newgroups;
		usesreputation = false;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("securitygroup"))
		{
			SecurityGroup group;
			group.name = tag->getString("name", "", 1);
			if (group.name.empty())
				throw ModuleException(this, "<securitygroup:name> is empty at " + tag->source.str());

			irc::spacesepstream maskstream(tag->getString("mask"));
			for (std::string mask; maskstream.GetToken(mask); )
				group.masks.push_back(mask);

			group.publicgroup = tag->getBool("public", false);

			group.require_account = tag->getBool("account", tag->getBool("registered", false));
			group.require_tls = tag->getBool("tls", tag->getBool("tls-users", false));
			group.require_insecure = tag->getBool("insecure", tag->getBool("insecure-users", false));
			group.require_websocket = tag->getBool("websocket", tag->getBool("websocket-users", false));
			group.require_webirc = tag->getBool("webirc", tag->getBool("webirc-users", false));
			group.score_min = tag->getNum<int>("scoremin", -1);
			group.score_max = tag->getNum<int>("scoremax", -1);
			if ((group.score_min != -1 && group.score_min < 0) || (group.score_max != -1 && group.score_max < 0))
				throw ModuleException(this, "<securitygroup> scoremin/scoremax must be >= 0 (or omitted) at " + tag->source.str());
			if (group.score_min != -1 && group.score_max != -1 && group.score_min > group.score_max)
				throw ModuleException(this, "<securitygroup> scoremin must be <= scoremax at " + tag->source.str());
			if (group.score_min != -1 || group.score_max != -1)
				usesreputation = true;

			if (group.require_tls && group.require_insecure)
				throw ModuleException(this, "<securitygroup> can not have both tls and insecure enabled at " + tag->source.str());

			newgroups.push_back(group);
		}

		if (usesreputation && !GetReputationExt())
			throw ModuleException(this, "<securitygroup> uses scoremin/scoremax but the reputation user extension (from the reputation module) is not loaded");

		groups.swap(newgroups);

		for (auto* user : ServerInstance->Users.GetLocalUsers())
			Rebuild(user);
	}

	void OnUnloadModule(Module* mod) override
	{
		// If reputation is unloaded then immediately drop any score-based memberships.
		if (!usesreputation)
			return;
		for (auto* user : ServerInstance->Users.GetLocalUsers())
			Rebuild(user);
	}

	void OnUserPostInit(LocalUser* user) override
	{
		// Called after I/O hooks have been checked and the user has a connect class.
		Rebuild(user);
	}

	void OnUserConnect(LocalUser* user) override
	{
		Rebuild(user);
	}

	void OnAccountChange(User* user, const std::string& newaccount) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (luser)
			Rebuild(luser);
	}

	void OnWebIRCAuth(LocalUser* user, const WebIRC::FlagMap* flags) override
	{
		// m_gateway calls this before it changes the user's remote address; we
		// just mark the user and let OnChangeRemoteAddress recalculate.
		webircext.Set(user);
	}

	void OnChangeRemoteAddress(LocalUser* user) override
	{
		Rebuild(user);
	}

	void OnWhois(Whois::Context& whois) override
	{
		const bool canseeall = whois.IsSelfWhois() || whois.GetSource()->HasPrivPermission("users/auspex");
		const SecurityGroupList* list = canseeall ? sgext.Get(whois.GetTarget()) : sgpublicext.Get(whois.GetTarget());
		if (!list || list->empty())
			return;

		whois.SendLine(RPL_WHOISSPECIAL, "is in security groups: " + insp::join(*list, ','));
	}
};

MODULE_INIT(ModuleSecurityGroups)
