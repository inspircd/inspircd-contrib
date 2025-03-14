/*
 *  InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2025 reverse Chevronnet
 *   mike.chevronnet@gmail.com
 *
 *  This file is part of InspIRCd. InspIRCd is free software; you can
 *  redistribute it and/or modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; version 2.
 */
 
/// $ModAuthor: reverse <mike.chevronnet@gmail.com>
/// $ModDesc: Google reCAPTCHA v2 verification.
/// $ModConfig: <captchaconfig url="https://your-captcha-site.com" dbid="default" hmac_key="changeThisKey" whitelistchans="#help,#support" whitelistports="6697,6698" message="*** reCAPTCHA: Verify your connection at {url}.">
/// $ModDepends: core 4

#include "inspircd.h"
#include "modules/sql.h"
#include "extension.h"
#include "modules/hash.h"
#include "modules/account.h" //Account API
#include "utility/string.h" // For Template class

class ModuleCaptchaCheck;

// The command of this module /VERIFY <token>
class CommandVerificar final : public Command
{
private:
    ModuleCaptchaCheck* parent;

public:
    CommandVerificar(Module* Creator, ModuleCaptchaCheck* Parent)
        : Command(Creator, "VERIFY", 1, 1), parent(Parent)
    {
        syntax = { "<verification_token>" };
    }

    CmdResult Handle(User* user, const Params& parameters) override;
};

// Query for tokens
class ValidateTokenQuery final : public SQL::Query
{
private:
    User* user;
    BoolExtItem& captcha_verified;

public:
    ValidateTokenQuery(Module* Creator, User* User, BoolExtItem& CaptchaVerified)
        : SQL::Query(Creator), user(User), captcha_verified(CaptchaVerified) {}

    void OnResult(SQL::Result& result) override
    {
        SQL::Row row;
        if (result.GetRow(row))
        {
            const std::string count = row[0].value_or("0");
            if (count == "1")
            {
                captcha_verified.Set(user, true);
                user->WriteNotice("*** reCAPTCHA: Verification successful. You may now join channels.");
            }
            else
            {
                user->WriteNotice("*** reCAPTCHA: Verification failed. Token not found or expired.");
            }
        }
        else
        {
            user->WriteNotice("*** reCAPTCHA: Verification failed. Token not found.");
        }
    }

    void OnError(const SQL::Error& error) override
    {
        user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: SQL Error: {}", error.ToString()));
    }
};

class ModuleCaptchaCheck final : public Module
{
private:
    std::string captcha_url;
    std::string query;
    std::string hmac_key;
    dynamic_reference<SQL::Provider> sql;
    dynamic_reference<HashProvider> sha256;
    BoolExtItem captcha_verified;
    CommandVerificar cmdverificar;
    Account::API accountapi;

public:
    ModuleCaptchaCheck()
        : Module(VF_VENDOR, "Google reCAPTCHA v2+token server-side."),
          sql(this, "SQL"),
          sha256(this, "hash/sha256"),
          captcha_verified(this, "captcha-verified", ExtensionType::USER, true),
          cmdverificar(this, this),
          accountapi(this) {}

    void init() override
    {
        if (!sha256)
        {
            throw ModuleException(this, "*** reCAPTCHA: Cannot initialize - SHA256 module is required but not loaded.");
        }
    }

    void ReadConfig(ConfigStatus& status) override
    {
        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");

        // SQL query
        captcha_url = tag->getString("url");
        query = tag->getString("query", "SELECT COUNT(*) FROM recaptcha_app_verificationtoken WHERE token = $1 AND created_at + INTERVAL '30 minutes' > NOW()", 1);
        
        // HMAC key for hashing
        hmac_key = tag->getString("hmac_key", "recaptchaDefaultKey");

        // Setup SQL provider (recommended posgresql on very large networks)
        std::string dbid = tag->getString("dbid", "default");
        sql.SetProvider("SQL/" + dbid);

        if (!sql)
        {
            throw ModuleException(this, INSP_FORMAT("*** reCAPTCHA: Could not find SQL provider with id '{}'.", dbid));
        }
    }

    ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
    {
        if (user->IsOper())
            return MOD_RES_PASSTHRU;

        // NickServ will help ya to not do the recaptcha
        if (accountapi && accountapi->GetAccountName(user))
        {     // we +r we good
            user->WriteNotice("*** reCAPTCHA: NickServ account verified. You may join channels.");
            return MOD_RES_PASSTHRU;
        }

        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");
        std::set<std::string> whitelist_channels;
        std::stringstream chanstream(tag->getString("whitelistchans"));
        std::string channel_name;
        while (std::getline(chanstream, channel_name, ','))
            whitelist_channels.insert(channel_name);

        if (whitelist_channels.find(cname) != whitelist_channels.end())
            return MOD_RES_PASSTHRU;

        std::set<int> whitelist_ports;
        std::stringstream portstream(tag->getString("whitelistports"));
        std::string port_str;
        while (std::getline(portstream, port_str, ','))
            whitelist_ports.insert(std::stoi(port_str));

        if (whitelist_ports.find(user->server_sa.port()) != whitelist_ports.end())
            return MOD_RES_PASSTHRU;

        if (captcha_verified.Get(user))
            return MOD_RES_PASSTHRU;

        NotifyUserToVerify(user);
        return MOD_RES_DENY;
    }

    void ValidateToken(User* user, const std::string& token)
    {
        if (!sql)
        {
            user->WriteNotice("*** reCAPTCHA: SQL database is not available.");
            return;
        }

        SQL::ParamList params;
        params.push_back(token);

        sql->Submit(new ValidateTokenQuery(this, user, captcha_verified), query, params);
    }   

private:
    void NotifyUserToVerify(User* user)
    {
        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");

        // Generate token
        std::string token = GenerateToken();
        std::string hash = GenerateHash(token);

        // Debug logs
        ServerInstance->Logs.Debug(MODNAME, "Generated token: {}", token);
        ServerInstance->Logs.Debug(MODNAME, "Generated hash: {}", hash);

        // Build verification message
        std::string message = tag->getString("message", "*** reCAPTCHA: Verify your connection at {url}.");
        std::string link = captcha_url + "?code=" + token + "&hn=" + hash;

        // Use Template::Replace instead of manual find and replace
        message = Template::Replace(message, {
            {"url", link}
        });

        user->WriteNotice(message);
    }

    std::string GenerateToken()
    {
        return ServerInstance->GenRandomStr(64);
    }

    std::string GenerateHash(const std::string& token)
    {
        if (!sha256)
        {
            throw ModuleException(this, "*** reCAPTCHA: SHA256 module is required but not loaded.");
        }
        
        return sha256->hmac(hmac_key, token);
    }
};

CmdResult CommandVerificar::Handle(User* user, const Params& parameters)
{
    parent->ValidateToken(user, parameters[0]);
    return CmdResult::SUCCESS;
}

MODULE_INIT(ModuleCaptchaCheck)