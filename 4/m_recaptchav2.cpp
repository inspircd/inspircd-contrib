/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015-2025 reverse Chevronnet
 *   mike.chevronnet@gmail.com
 *
 * This file is part of InspIRCd.  InspIRCd is free software; you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; version 2.
 */

/// $ModAuthor: reverse mike.chevronnet@gmail.com
/// $ModConfig: mode="master/slave" masterserver="irc1.example.com" url="http://meme.com/verify/" message="*** reCAPTCHA: You need to verify that you are not a bot, to do so log in to Account or validate your connection here: {url}" 
/// whitelistchans="#help,#opers" whitelistports="6667,6697">
/// $ModDepends: core 4

#include "inspircd.h"
#include "modules/sql.h"
#include "extension.h"
#include "modules/account.h"
#include "protocol.h"

// Command to add or check an IP in the whitelist
class ModuleCaptchaCheck;

class CommandRecaptcha : public Command
{
private:
    ModuleCaptchaCheck* parent;

public:
    CommandRecaptcha(Module* Creator, ModuleCaptchaCheck* Parent)
        : Command(Creator, "RECAPTCHA", 2, 2), parent(Parent)
    {
        syntax = { "<add|check> <ip>" };
    }

    CmdResult Handle(User* user, const Params& parameters) override;
};

class ModuleCaptchaCheck : public Module
{
private:
    std::string mode;
    std::string captcha_url;
    std::string masterserver;
    SQL::Provider* sql = nullptr;
    StringExtItem captcha_verified;
    StringExtItem captcha_success;
    CommandRecaptcha cmd;
    Account::API accountapi;

    friend class CommandRecaptcha;

    class AddIPQuery : public SQL::Query
    {
    private:
        ModuleCaptchaCheck* parent;
        User* user;
        std::string ip;

    public:
        AddIPQuery(ModuleCaptchaCheck* Parent, User* User, const std::string& IP)
            : SQL::Query(Parent), parent(Parent), user(User), ip(IP) {}

        void OnResult(SQL::Result& result) override
        {
            user->WriteNotice("*** reCAPTCHA: IP successfully added to the whitelist.");
        }

        void OnError(const SQL::Error& error) override
        {
            ServerInstance->SNO.WriteGlobalSno('r', INSP_FORMAT("*** reCAPTCHA: SQL Error: {}", error.ToString()));
            user->WriteNotice("*** reCAPTCHA: Failed to add IP to the whitelist.");
        }
    };

    class CheckIPQuery : public SQL::Query
{
private:
    ModuleCaptchaCheck* parent;
    User* user;

public:
    CheckIPQuery(ModuleCaptchaCheck* Parent, User* User)
        : SQL::Query(Parent), parent(Parent), user(User) {}

        void OnResult(SQL::Result& result) override
        {
            if (result.Rows() > 0)
            {
                SQL::Row row;
                if (result.GetRow(row))
                {
                    int count = std::stoi(row[0].value());
                    if (count > 0)
                    {
                        parent->captcha_verified.Set(user, "verified");
                        user->WriteNotice("*** reCAPTCHA: Your verification is complete. You may now join channels.");
                    }
                else
                    {
                    user->WriteNotice("*** reCAPTCHA: Verification required. Please complete the CAPTCHA.");
                    }
                }
                else
                {
                    user->WriteNotice("*** reCAPTCHA: Failed to parse database result.");
                }
            }
            else
            {
                user->WriteNotice("*** reCAPTCHA: Verification required. Please complete the CAPTCHA.");
            }
        }

        void OnError(const SQL::Error& error) override
        {
            ServerInstance->SNO.WriteGlobalSno('r', INSP_FORMAT("*** reCAPTCHA: SQL Error: {}", error.ToString()));
            user->WriteNotice("*** reCAPTCHA: Verification failed due to a server error.");
        }
    };

public:
    ModuleCaptchaCheck()
        : Module(VF_VENDOR, "Handles Google reCAPTCHA v2 verification and whitelist management."),
          captcha_verified(this, "captcha-verified", ExtensionType::USER, true),
          captcha_success(this, "captcha-success", ExtensionType::USER, true),
          cmd(this, this),
          accountapi(this)
    {
    }

    void ReadConfig(ConfigStatus& status) override
    {
        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");
        mode = tag->getString("mode");
        captcha_url = tag->getString("url");
        masterserver = tag->getString("masterserver");

        std::string dbid = tag->getString("conninfo", "default");

        for (const auto& [name, provider] : ServerInstance->Modules.DataProviders)
        {
            if (provider->name.compare(0, 4, "SQL/") == 0)
            {
                std::string provider_id = provider->name.substr(4);
                if (provider_id == dbid)
                {
                    sql = static_cast<SQL::Provider*>(provider);
                    break;
                }
            }
        }

        if (!sql)
        {
            throw ModuleException(this, INSP_FORMAT("*** reCAPTCHA: Could not find SQL provider with id '{}'.", dbid));
        }
    }

    ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
    {
        if (user->IsOper())
        {
            return MOD_RES_PASSTHRU; // Oper users bypass reCAPTCHA
        }

        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");
        std::string whitelistchans = tag->getString("whitelistchans");
        std::set<std::string> channels;
        std::stringstream chanstream(whitelistchans);
        std::string chname;
        while (std::getline(chanstream, chname, ','))
        {
            channels.insert(chname);
        }

        if (channels.find(cname) != channels.end())
        {
            return MOD_RES_PASSTHRU; // Whitelisted channels bypass reCAPTCHA
        }

        std::string whitelistports = tag->getString("whitelistports");
        std::set<int> ports;
        std::stringstream portstream(whitelistports);
        std::string port;
        while (std::getline(portstream, port, ','))
        {
            try
            {
                ports.insert(std::stoi(port));
            }
            catch (const std::exception&)
            {
                ServerInstance->Logs.Warning(MODNAME, INSP_FORMAT("*** reCAPTCHA: Invalid port in whitelistports: {}", port));
            }
        }
        if (ports.find(user->server_sa.port()) != ports.end())
        {
            return MOD_RES_PASSTHRU; // Whitelisted ports bypass reCAPTCHA
        }

        if (accountapi && accountapi->IsIdentifiedToNick(user))
        {
            return MOD_RES_PASSTHRU; // Identified users bypass reCAPTCHA
        }

        const std::string* status = captcha_verified.Get(user);
        if (!status || *status != "verified")
        {
            std::string custom_message = tag->getString("message", "*** reCAPTCHA: You must be identified to an account or complete the reCAPTCHA before joining channels. Link: {url}");
            size_t pos = custom_message.find("{url}");
            if (pos != std::string::npos)
            {
                custom_message.replace(pos, 5, captcha_url);
            }

            user->WriteNotice(custom_message);

            std::string query = INSP_FORMAT("SELECT COUNT(*) FROM ircaccess_alloweduser WHERE ip_address = '{}'", user->server_sa.addr());
            sql->Submit(new ModuleCaptchaCheck::CheckIPQuery(this, user), query);

            return MOD_RES_DENY;
        }

        return MOD_RES_PASSTHRU;
    }

    void OnUserPostInit(LocalUser* user) override
    {
        const std::string* status = captcha_success.Get(user);
        if (status && *status == "passed")
        {
            captcha_verified.Set(user, "verified");
            ServerInstance->SNO.WriteGlobalSno('r', INSP_FORMAT("reCAPTCHA: User {} has passed the CAPTCHA.", user->nick));
        }
    }
};

CmdResult CommandRecaptcha::Handle(User* user, const Params& parameters)
{
    if (!user->HasPrivPermission("users/auspex"))
    {
        user->WriteNotice("*** reCAPTCHA: You do not have permission to use this command.");
        return CmdResult::FAILURE;
    }
    const std::string& action = parameters[0];
    const std::string& ip = parameters[1];

    if (action == "add")
    {
        if (parent->mode == "master")
        {
            std::string query = INSP_FORMAT("INSERT INTO ircaccess_alloweduser (ip_address) VALUES ('{}')", ip);
            parent->sql->Submit(new ModuleCaptchaCheck::AddIPQuery(parent, user, ip), query);
            return CmdResult::SUCCESS;
        }
        else
        {
            CommandBase::Params params;
            params.push_back("add");
            params.push_back(ip);
            ServerInstance->PI->SendEncapsulatedData(parent->masterserver, "RECAPTCHA", params);
            user->WriteNotice("*** reCAPTCHA: Request to add IP sent to the master server.");
            return CmdResult::SUCCESS;
        }
    }
    else if (action == "check")
    {
        if (parent->mode == "master")
        {
            std::string query = INSP_FORMAT("SELECT COUNT(*) FROM ircaccess_alloweduser WHERE ip_address = '{}'", ip);
            parent->sql->Submit(new ModuleCaptchaCheck::CheckIPQuery(parent, user), query);
            return CmdResult::SUCCESS;
        }
        else
        {
            CommandBase::Params params;
            params.push_back("check");
            params.push_back(ip);
            ServerInstance->PI->SendEncapsulatedData(parent->masterserver, "RECAPTCHA", params);
            user->WriteNotice("*** reCAPTCHA: Request to check IP sent to the master server.");
            return CmdResult::SUCCESS;
        }
    }

    user->WriteNotice("*** reCAPTCHA: Invalid action. Use 'add <ip>' or 'check <ip>'.");
    return CmdResult::FAILURE;
}

MODULE_INIT(ModuleCaptchaCheck)
