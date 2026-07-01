/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 reverse <mike.chevronnet@gmail.com>
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

/// $ModAuthor: reverse <mike.chevronnet@gmail.com>
/// $ModDepends: core 4
/// $ModDesc: Provides the DRAFT extended-isupport IRCv3 extension.

#include "inspircd.h"

#if INSPIRCD_VERSION_SINCE(4, 12)

// InspIRCd 4.12+ has the core ISupport API; use it directly.
#include "clientprotocolmsg.h"

#include "modules/cap.h"
#include "modules/ircv3_batch.h"
#include "modules/isupport.h"

enum
{
	// From RFC 2812.
	RPL_MYINFO = 4,

	// From RFC 1459.
	RPL_LUSERCLIENT = 251,
};

class ExtendedISupportCap final
	: public Cap::Capability
{
public:
	IRCv3::Batch::CapReference batchcap;
	ISupport::API isupportapi;

	ExtendedISupportCap(Module* mod)
		: Cap::Capability(mod, "draft/extended-isupport")
		, batchcap(mod)
		, isupportapi(mod)
	{
	}

	bool OnList(LocalUser* user) override
	{
		// The spec requires batch so don't offer the cap without it.
		return batchcap && isupportapi;
	}

	bool OnRequest(LocalUser* user, bool adding) override
	{
		return OnList(user);
	}
};

class CommandISupport final
	: public SplitCommand
{
public:
	BoolExtItem earlyisupport;
	ExtendedISupportCap cap;

	CommandISupport(Module* mod)
		: SplitCommand(mod, "ISUPPORT")
		, earlyisupport(mod, "early-isupport", ExtensionType::USER)
		, cap(mod)
	{
		works_before_reg = true;
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (!cap.IsEnabled(user))
		{
			user->WriteNumeric(ERR_UNKNOWNCOMMAND, name, INSP_FORMAT("You need the {} capability to use this command", cap.GetName()));
			return CmdResult::FAILURE;
		}

		if (!cap.isupportapi)
			return CmdResult::FAILURE;

		cap.isupportapi->SendTo(user);

		if (!user->IsFullyConnected())
			earlyisupport.Set(user);

		return CmdResult::SUCCESS;
	}
};

class ModuleIRCv3ExtendedISupport final
	: public Module
	, public ClientProtocol::EventHook
	, public ISupport::EventListener
{
private:
	IRCv3::Batch::Batch batch;
	IRCv3::Batch::API batchmanager;
	CommandISupport cmd;
	bool skipisupport = true;

public:
	ModuleIRCv3ExtendedISupport()
		: Module(VF_NONE, "Provides the IRCv3 draft/extended-isupport client capability.")
		, ClientProtocol::EventHook(this, "NUMERIC")
		, ISupport::EventListener(this)
		, batch("draft/extended-isupport")
		, batchmanager(this)
		, cmd(this)
	{
	}

	ModResult OnNumeric(User* user, const Numeric::Numeric& numeric) override
	{
		if (!IS_LOCAL(user) || user->IsFullyConnected())
			return MOD_RES_PASSTHRU;

		// Connect order is 004 -> 005 -> 251. Hold back the plain 005 burst so
		// extended-isupport clients receive it batched instead.
		switch (numeric.GetNumeric())
		{
			case RPL_MYINFO:
				skipisupport = true;
				break;

			case RPL_ISUPPORT:
				if (skipisupport)
					return MOD_RES_DENY;
				break;

			case RPL_LUSERCLIENT:
				skipisupport = false;
				break;
		}
		return MOD_RES_PASSTHRU;
	}

	void OnPostConnect(User* user) override
	{
		if (IS_LOCAL(user))
			cmd.earlyisupport.Unset(user);
	}

	ModResult OnPreEventSend(LocalUser* user, const ClientProtocol::Event& ev, ClientProtocol::MessageList& messagelist) override
	{
		if (!batchmanager || !cmd.cap.batchcap.IsEnabled(user) || !cmd.cap.IsEnabled(user))
			return MOD_RES_PASSTHRU;

		for (auto* message : messagelist)
		{
			auto* numeric = static_cast<ClientProtocol::Messages::Numeric*>(message);
			if (numeric->GetNumeric() != RPL_ISUPPORT)
				continue;

			if (!batch.IsRunning())
				batchmanager->Start(batch);
			batch.AddToBatch(*numeric);
		}
		return MOD_RES_PASSTHRU;
	}

	void OnPostEventSend(LocalUser* user, const ClientProtocol::Event& ev, const ClientProtocol::MessageList& messagelist) override
	{
		if (batch.IsRunning())
			batchmanager->End(batch);
	}

	ModResult OnSendISupportDiff(LocalUser* user, const ISupport::TokenMap& tokens) override
	{
		if (user->IsFullyConnected())
			return MOD_RES_PASSTHRU; // The core already handles these.

		return cmd.earlyisupport.Get(user) ? MOD_RES_ALLOW : MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleIRCv3ExtendedISupport)

#else

// Older cores lack ISupport::API; build the ISUPPORT list by hand.
#include "modules/cap.h"
#include "modules/isupport.h"
#include "modules/ircv3_batch.h"

namespace
{
	static void AppendValue(std::string& buffer, const std::string& value)
	{
		if (value.empty())
			return;

		buffer.push_back('=');
		for (const auto chr : value)
		{
			if (chr == '\0' || chr == '\n' || chr == '\r' || chr == ' ' || chr == '=' || chr == '\\')
				buffer.append(INSP_FORMAT("\\x{:02X}", chr));
			else
				buffer.push_back(chr);
		}
	}

	static void BuildNumerics(const ISupport::TokenMap& tokens, std::vector<Numeric::Numeric>& numerics)
	{
		Numeric::Numeric numeric(RPL_ISUPPORT);
		for (auto it = tokens.cbegin(); it != tokens.cend(); ++it)
		{
			numeric.push(it->first);
			std::string& token = numeric.GetParams().back();
			AppendValue(token, it->second);

			if (numeric.GetParams().size() == 12 || std::distance(it, tokens.cend()) == 1)
			{
				numeric.push("are supported by this server");
				numerics.push_back(numeric);
				numeric.GetParams().clear();
			}
		}
	}

	static ISupport::TokenMap BuildTokens(ISupport::EventProvider& isupportevprov, const std::shared_ptr<ConnectClass>& klass)
	{
		ISupport::TokenMap tokens = {
			{ "AWAYLEN",     ConvToStr(ServerInstance->Config->Limits.MaxAway)    },
			{ "CASEMAPPING", ServerInstance->Config->CaseMapping                  },
			{ "CHANNELLEN",  ConvToStr(ServerInstance->Config->Limits.MaxChannel) },
			{ "CHANTYPES",   "#"                                                  },
			{ "HOSTLEN",     ConvToStr(ServerInstance->Config->Limits.MaxHost)    },
			{ "KEYLEN",      ConvToStr(ServerInstance->Config->Limits.MaxKey)     },
			{ "KICKLEN",     ConvToStr(ServerInstance->Config->Limits.MaxKick)    },
			{ "LINELEN",     ConvToStr(ServerInstance->Config->Limits.MaxLine)    },
			{ "MAXTARGETS",  ConvToStr(ServerInstance->Config->MaxTargets)        },
			{ "MODES",       ConvToStr(ServerInstance->Config->Limits.MaxModes)   },
			{ "NETWORK",     ServerInstance->Config->Network                      },
			{ "NAMELEN",     ConvToStr(ServerInstance->Config->Limits.MaxReal)    },
			{ "NICKLEN",     ConvToStr(ServerInstance->Config->Limits.MaxNick)    },
			{ "TOPICLEN",    ConvToStr(ServerInstance->Config->Limits.MaxTopic)   },
			{ "USERLEN",     ConvToStr(ServerInstance->Config->Limits.MaxUser)    },
		};
		isupportevprov.Call(&ISupport::EventListener::OnBuildISupport, tokens);
		isupportevprov.Call(&ISupport::EventListener::OnBuildClassISupport, klass, tokens);
		return tokens;
	}
}

class ExtendedISupportCap final
	: public Cap::Capability
{
public:
	ExtendedISupportCap(Module* mod)
		: Cap::Capability(mod, "draft/extended-isupport")
	{
	}
};

class CommandISupport final
	: public SplitCommand
{
private:
	Cap::Capability& cap;
	ISupport::EventProvider& isupportevprov;
	IRCv3::Batch::API& batchmanager;
	IRCv3::Batch::CapReference& batchcap;

	void SendNumericList(LocalUser* user, std::vector<Numeric::Numeric>& numerics)
	{
		if (batchmanager && batchcap.IsEnabled(user))
		{
			IRCv3::Batch::Batch batch("draft/isupport");
			batchmanager->Start(batch);
			for (auto& numeric : numerics)
			{
				numeric.AddTag("batch", batchmanager.operator->(), batch.GetRefTagStr(), &batch);
				user->WriteNumeric(numeric);
			}
			batchmanager->End(batch);
			return;
		}

		for (const auto& numeric : numerics)
			user->WriteNumeric(numeric);
	}

public:
	CommandISupport(Module* mod, Cap::Capability& capref, ISupport::EventProvider& isupprov,
		IRCv3::Batch::API& batchmgr, IRCv3::Batch::CapReference& batchcapref)
		: SplitCommand(mod, "ISUPPORT")
		, cap(capref)
		, isupportevprov(isupprov)
		, batchmanager(batchmgr)
		, batchcap(batchcapref)
	{
		works_before_reg = true;
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (!cap.IsEnabled(user))
		{
			user->WriteNumeric(ERR_UNKNOWNCOMMAND, name, "You must request the draft/extended-isupport capability to use this command");
			return CmdResult::FAILURE;
		}

		ISupport::TokenMap tokens = BuildTokens(isupportevprov, user->GetClass());
		std::vector<Numeric::Numeric> numerics;
		BuildNumerics(tokens, numerics);
		SendNumericList(user, numerics);
		return CmdResult::SUCCESS;
	}
};

class ModuleIRCv3ExtendedISupport final
	: public Module
{
private:
	ExtendedISupportCap cap;
	IRCv3::Batch::API batchmanager;
	IRCv3::Batch::CapReference batchcap;
	ISupport::EventProvider isupportevprov;
	CommandISupport isupportcmd;

	bool rewriting = false;

public:
	ModuleIRCv3ExtendedISupport()
		: Module(VF_NONE, "Provides the IRCv3 draft/extended-isupport client capability.")
		, cap(this)
		, batchmanager(this)
		, batchcap(this)
		, isupportevprov(this)
		, isupportcmd(this, cap, isupportevprov, batchmanager, batchcap)
	{
	}

	ModResult OnNumeric(User* user, const Numeric::Numeric& numeric) override
	{
		if (rewriting)
			return MOD_RES_PASSTHRU;

		if (numeric.GetNumeric() != RPL_ISUPPORT)
			return MOD_RES_PASSTHRU;

		auto* localuser = IS_LOCAL(user);
		if (!localuser)
			return MOD_RES_PASSTHRU;

		if (!cap.IsEnabled(localuser))
			return MOD_RES_PASSTHRU;

		if (!batchmanager || !batchcap.IsEnabled(localuser))
			return MOD_RES_PASSTHRU;

		// Already batched.
		const auto& tags = numeric.GetParams().GetTags();
		if (tags.find("batch") != tags.end())
			return MOD_RES_PASSTHRU;

		// Rewrite this numeric into a draft/isupport batch.
		rewriting = true;
		IRCv3::Batch::Batch batch("draft/isupport");
		batchmanager->Start(batch);

		Numeric::Numeric out(RPL_ISUPPORT);
		out.GetParams() = numeric.GetParams();
		out.AddTag("batch", batchmanager.operator->(), batch.GetRefTagStr(), &batch);
		localuser->WriteNumeric(out);

		batchmanager->End(batch);
		rewriting = false;
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleIRCv3ExtendedISupport)

#endif
