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
​
/// $ModAuthor: reverse mike.chevronnet@gmail.com
/// $ModConfig: <captchaconfig conninfo="dbname=example user=postgres password=secret hostaddr=127.0.0.1 port=5432" mode="master/slave" masterserver="master.example.com" url="http://meme.com/verify/" whitelistchan="#help,#support" whitelistport="6697,6666">
/// $ModDepends: core 4
​
/// $CompilerFlags: find_compiler_flags("libpq")
/// $LinkerFlags: find_linker_flags("libpq")
​
#include "inspircd.h"
#include "extension.h"
#include "modules/account.h"
#include "protocol.h"
#include <libpq-fe.h>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <chrono>
​
class ModuleCaptchaCheck : public Module
{
private:
    std::string mode; // "master" or "slave"
    std::string conninfo;
    std::string captcha_url;
    std::string masterserver;
    std::unordered_set<std::string> whitelist_channels;
    std::set<int> whitelist_ports;
    PGconn* db;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> ip_cache;
    StringExtItem captcha_success;
    Account::API account_api;
​
    static constexpr int CACHE_DURATION_MINUTES = 10;
​
    PGconn* GetConnection()
    {
        if (!db || PQstatus(db) != CONNECTION_OK)
        {
            db = PQconnectdb(conninfo.c_str());
            if (PQstatus(db) != CONNECTION_OK)
            {
                throw ModuleException(this, "reCAPTCHA: Database connection unavailable.");
            }
        }
        return db;
    }
​
    void SyncMetadata(User* user)
    {
        const std::string* status = captcha_success.Get(user);
        if (status && *status == "passed")
        {
            ServerInstance->PI->SendMetadata(user, "captcha-success", "passed");
        }
    }
​
    class CommandRecaptcha : public Command
    {
    private:
        ModuleCaptchaCheck* parent;
​
    public:
        CommandRecaptcha(Module* Creator, ModuleCaptchaCheck* Parent)
            : Command(Creator, "RECAPTCHA", 2, 2), parent(Parent)
        {
            syntax = { "<add|check> <ip>" };
        }
​
        CmdResult Handle(User* user, const Params& parameters) override
        {
            if (!user->HasPrivPermission("users/auspex"))
            {
                user->WriteNotice("*** reCAPTCHA: You do not have permission to use this command.");
                return CmdResult::FAILURE;
            }
​
            const std::string& action = parameters[0];
            const std::string& ip = parameters[1];
​
            if (action == "add")
            {
                if (parent->mode != "master")
                {
                    CommandBase::Params params;
                    params.push_back("add");
                    params.push_back(ip);
​
                    ServerInstance->PI->SendEncapsulatedData(parent->masterserver, "RECAPTCHA", params);
                    user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: Request to add IP {} sent to the master server.", ip));
                    return CmdResult::SUCCESS;
                }
​
                PGconn* conn = parent->GetConnection();
                if (!conn)
                {
                    user->WriteNotice("*** reCAPTCHA: Database connection unavailable.");
                    return CmdResult::FAILURE;
                }
​
                std::string query = INSP_FORMAT("INSERT INTO ircaccess_alloweduser (ip_address) VALUES ('{}')", ip);
                PGresult* res = PQexec(conn, query.c_str());
​
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: Failed to add IP {}: {}", ip, PQerrorMessage(conn)));
                    PQclear(res);
                    return CmdResult::FAILURE;
                }
​
                PQclear(res);
                user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: Successfully added IP {} to the whitelist.", ip));
                return CmdResult::SUCCESS;
            }
            else if (action == "check")
            {
                if (parent->mode == "master")
                {
                    PGconn* conn = parent->GetConnection();
                    if (!conn)
                    {
                        user->WriteNotice("*** reCAPTCHA: Database connection unavailable.");
                        return CmdResult::FAILURE;
                    }
​
                    std::string query = INSP_FORMAT("SELECT COUNT(*) FROM ircaccess_alloweduser WHERE ip_address = '{}'", ip);
                    PGresult* res = PQexec(conn, query.c_str());
​
                    if (PQresultStatus(res) != PGRES_TUPLES_OK)
                    {
                        user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: Failed to check IP {}: {}", ip, PQerrorMessage(conn)));
                        PQclear(res);
                        return CmdResult::FAILURE;
                    }
​
                    int count = atoi(PQgetvalue(res, 0, 0));
                    PQclear(res);
​
                    if (count > 0)
                    {
                        user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: IP {} is verified in the whitelist.", ip));
                        return CmdResult::SUCCESS;
                    }
                    else
                    {
                        user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: IP {} is NOT verified in the whitelist.", ip));
                        return CmdResult::FAILURE;
                    }
                }
                else
                {
                    CommandBase::Params params;
                    params.push_back("check");
                    params.push_back(ip);
                    ServerInstance->PI->SendEncapsulatedData(parent->masterserver, "RECAPTCHA", params);
                    user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: Request sent to master server for IP {}.", ip));
                    return CmdResult::SUCCESS;
                }
            }
​
            user->WriteNotice("*** reCAPTCHA: Unknown action. Use 'add <ip>' or 'check <ip>'.");
            return CmdResult::FAILURE;
        }
    };
​
    CommandRecaptcha cmd;
​
public:
    ModuleCaptchaCheck()
        : Module(VF_VENDOR, "Requires users to solve a Google reCAPTCHA before joining channels."),
          db(nullptr),
          captcha_success(this, "captcha-success", ExtensionType::USER, true),
          account_api(this),
          cmd(this, this) // Command is constructed here
    {}
​
    void ReadConfig(ConfigStatus& status) override
    {
        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");
        mode = tag->getString("mode");
        conninfo = tag->getString("conninfo", "", mode == "master");
        captcha_url = tag->getString("url");
        masterserver = tag->getString("masterserver");
​
        std::string whitelist = tag->getString("whitelistchan");
        irc::commasepstream whiteliststream(whitelist);
        std::string channel;
        while (whiteliststream.GetToken(channel))
        {
            whitelist_channels.insert(channel);
        }
​
        std::string whitelistport = tag->getString("whitelistport");
        irc::commasepstream portstream(whitelistport);
        std::string port;
        while (portstream.GetToken(port))
        {
            try
            {
                whitelist_ports.insert(std::stoi(port));
            }
            catch (const std::exception&)
            {
                ServerInstance->SNO.WriteToSnoMask('a', INSP_FORMAT("Invalid port in whitelistport: {}", port));
            }
        }
​
        if (mode == "master")
        {
            db = GetConnection();
        }
​
        ServerInstance->SNO.WriteToSnoMask('a', "Captcha module configuration loaded.");
    }
​
    void OnUserConnect(LocalUser* user) override
    {
        SyncMetadata(user);
    }
​
    void OnDecodeMetadata(Extensible* target, const std::string& key, const std::string& value) override
    {
        if (key == "captcha-success")
        {
            User* user = IS_LOCAL(static_cast<User*>(target));
            if (user)
            {
                captcha_success.Set(user, value);
                ServerInstance->SNO.WriteToSnoMask('a', INSP_FORMAT("reCAPTCHA: Metadata synced for user {}: {}", user->nick, value));
            }
        }
    }
​
    ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
    {
        if (whitelist_channels.count(cname))
        {
            return MOD_RES_PASSTHRU;
        }
​
        const std::string* status = captcha_success.Get(user);
        if (status && *status == "passed")
        {
            return MOD_RES_PASSTHRU;
        }
​
        int port = user->server_sa.port();
        if (whitelist_ports.count(port))
        {
            return MOD_RES_PASSTHRU;
        }
​
        std::string ip = user->client_sa.addr();
        if (!CheckCaptcha(ip, user))
        {
            user->WriteNotice("*** reCAPTCHA: Please verify at " + captcha_url + " before joining channels.");
            return MOD_RES_DENY;
        }
​
        captcha_success.Set(user, "passed");
        SyncMetadata(user);
        return MOD_RES_PASSTHRU;
    }
​
    bool CheckCaptcha(const std::string& ip, User* user)
    {
        auto now = std::chrono::steady_clock::now();
​
        // Check local cache first.
        if (ip_cache.find(ip) != ip_cache.end() && now < ip_cache[ip])
        {
            ServerInstance->SNO.WriteToSnoMask('a', INSP_FORMAT("reCAPTCHA: Cached verification for IP {}.", ip));
            return true;
        }
​
        if (mode != "master")
        {
            CommandBase::Params params;
            params.push_back("check");
            params.push_back(ip);
            ServerInstance->PI->SendEncapsulatedData(masterserver, "RECAPTCHA", params);
​
            // Mark the IP temporarily while waiting for master response.
            ip_cache[ip] = now + std::chrono::seconds(30); // Cache for 30 seconds to prevent spamming.
            ServerInstance->SNO.WriteToSnoMask('a', INSP_FORMAT("reCAPTCHA: Sent verification request for IP {} to master server.", ip));
            return false;
        }
​
        PGconn* conn = GetConnection();
        if (!conn)
        {
            ServerInstance->SNO.WriteToSnoMask('a', "reCAPTCHA: Database connection unavailable.");
            return false;
        }
​
        std::string query = INSP_FORMAT("SELECT COUNT(*) FROM ircaccess_alloweduser WHERE ip_address = '{}'", ip);
        PGresult* res = PQexec(conn, query.c_str());
​
        if (PQresultStatus(res) != PGRES_TUPLES_OK)
        {
            ServerInstance->SNO.WriteToSnoMask('a', INSP_FORMAT("reCAPTCHA: Database query error: {}", PQerrorMessage(conn)));
            PQclear(res);
            return false;
        }
​
        int count = atoi(PQgetvalue(res, 0, 0));
        PQclear(res);
​
        if (count > 0)
        {
            ip_cache[ip] = now + std::chrono::minutes(CACHE_DURATION_MINUTES);
            return true;
        }
​
        return false;
    }
​
    void OnEncapsulatedData(const std::string& source, const std::string& command, CommandBase::Params& parameters)
{
    if (command == "RECAPTCHA")
    {
        const std::string& action = parameters[0];
        const std::string& ip = parameters[1];
​
        if (action == "add")
        {
            PGconn* conn = GetConnection();
            CommandBase::Params response_params;
            if (!conn)
            {
                response_params.push_back("add");
                response_params.push_back(ip);
                response_params.push_back("failure");
                ServerInstance->PI->SendEncapsulatedData(source, "RECAPTCHA-REPLY", response_params);
                return;
            }
​
            std::string query = INSP_FORMAT("INSERT INTO ircaccess_alloweduser (ip_address) VALUES ('{}')", ip);
            PGresult* res = PQexec(conn, query.c_str());
​
            response_params.push_back("add");
            response_params.push_back(ip);
            response_params.push_back((PQresultStatus(res) == PGRES_COMMAND_OK) ? "success" : "failure");
            ServerInstance->PI->SendEncapsulatedData(source, "RECAPTCHA-REPLY", response_params);
​
            PQclear(res);
        }
        else if (action == "check")
        {
            PGconn* conn = GetConnection();
            CommandBase::Params response_params;
            if (!conn)
            {
                response_params.push_back("check");
                response_params.push_back(ip);
                response_params.push_back("failure");
                ServerInstance->PI->SendEncapsulatedData(source, "RECAPTCHA-REPLY", response_params);
                return;
            }
​
            std::string query = INSP_FORMAT("SELECT COUNT(*) FROM ircaccess_alloweduser WHERE ip_address = '{}'", ip);
            PGresult* res = PQexec(conn, query.c_str());
​
            response_params.push_back("check");
            response_params.push_back(ip);
            response_params.push_back((PQresultStatus(res) == PGRES_TUPLES_OK && atoi(PQgetvalue(res, 0, 0)) > 0) ? "success" : "failure");
            ServerInstance->PI->SendEncapsulatedData(source, "RECAPTCHA-REPLY", response_params);
​
            PQclear(res);
        }
    }
    else if (command == "RECAPTCHA-REPLY")
    {
        const std::string& action = parameters[0];
        const std::string& ip = parameters[1];
        const std::string& result = parameters[2];
​
        if (action == "add" && result == "success")
        {
            ServerInstance->SNO.WriteToSnoMask('a', INSP_FORMAT("reCAPTCHA: Successfully added IP {} via master server.", ip));
        }
        else if (action == "add")
        {
            ServerInstance->SNO.WriteToSnoMask('a', INSP_FORMAT("reCAPTCHA: Failed to add IP {} via master server.", ip));
        }
        else if (action == "check")
        {
            if (result == "success")
            {
                ServerInstance->SNO.WriteToSnoMask('a', INSP_FORMAT("reCAPTCHA: IP {} is verified in the whitelist via master server.", ip));
            }
            else
            {
                ServerInstance->SNO.WriteToSnoMask('a', INSP_FORMAT("reCAPTCHA: IP {} is NOT verified in the whitelist via master server.", ip));
            }
        }
    }
}
​
};
​
MODULE_INIT(ModuleCaptchaCheck)