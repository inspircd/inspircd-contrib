/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015-2016 reverse Chevronnet
 *   mike.chevronnet@gmail.com
 *
 * This file is part of InspIRCd.  InspIRCd is free software; you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; version 2.
 */

/// $ModAuthor: reverse mike.chevronnet@gmail.com
/// $ModConfig: <captchaconfig conninfo="dbname=example user=postgres password=secret hostaddr=127.0.0.1 port=5432" url="http://example.com/verify/" whitelistchan="#help,#support">
/// $ModDepends: core 4


/// $CompilerFlags: find_compiler_flags("libpq")
/// $LinkerFlags: find_linker_flags("libpq")

/// $PackageInfo: require_system("alpine") libpq-dev pkgconf
/// $PackageInfo: require_system("arch") pkgconf libpq
/// $PackageInfo: require_system("darwin") libpq pkg-config
/// $PackageInfo: require_system("debian~") libpq-dev pkg-config
/// $PackageInfo: require_system("rhel~") pkg-config libmaxminddb-devel


#include "inspircd.h"
#include "extension.h"
#include <libpq-fe.h>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

class ModuleCaptchaCheck : public Module
{
private:
    std::string conninfo;
    std::string captcha_url;
    std::unordered_set<std::string> whitelist_channels;
    PGconn* db;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> ip_cache;
    StringExtItem captcha_success; // Extension item for syncing captcha status

    static constexpr int CACHE_DURATION_MINUTES = 10;

    PGconn* GetConnection()
    {
        if (!db || PQstatus(db) != CONNECTION_OK)
        {
            db = PQconnectdb(conninfo.c_str());
            if (PQstatus(db) != CONNECTION_OK)
            {
                throw ModuleException(this, "Database connection unavailable, unable to verify CAPTCHA.");
            }
        }
        return db;
    }

    bool CheckCaptcha(const std::string& ip)
    {
        auto now = std::chrono::steady_clock::now();

        // Check cache
        if (ip_cache.find(ip) != ip_cache.end() && now < ip_cache[ip])
        {
            return true;
        }

        PGconn* conn = GetConnection();
        if (!conn)
        {
            throw ModuleException(this, "Database connection unavailable, unable to verify CAPTCHA.");
        }

        std::string query = INSP_FORMAT("SELECT COUNT(*) FROM ircaccess_alloweduser WHERE ip_address = '{}'", ip);
        PGresult* res = PQexec(conn, query.c_str());

        if (PQresultStatus(res) != PGRES_TUPLES_OK)
        {
            std::string error_msg = INSP_FORMAT("Failed to execute query: {}", PQerrorMessage(conn));
            PQclear(res);
            throw ModuleException(this, error_msg);
        }

        int count = atoi(PQgetvalue(res, 0, 0));
        PQclear(res);

        if (count > 0)
        {
            ip_cache[ip] = now + std::chrono::minutes(CACHE_DURATION_MINUTES); // Cache for defined duration
            return true;
        }

        return false;
    }

    std::string ExtractIP(const std::string& client_sa_str)
    {
        std::string::size_type pos = client_sa_str.find(':');
        if (pos != std::string::npos)
        {
            return client_sa_str.substr(0, pos);
        }
        return client_sa_str;
    }

    class CommandRecaptcha : public Command
    {
    private:
        ModuleCaptchaCheck* parent;

    public:
        CommandRecaptcha(Module* Creator, ModuleCaptchaCheck* Parent)
            : Command(Creator, "RECAPTCHA", 2, 2), parent(Parent)
        {
            syntax = { "<add|search> <ip>" };
        }

        CmdResult Handle(User* user, const Params& parameters) override
        {
            if (!user->HasPrivPermission("users/auspex"))
            {
                user->WriteNotice("*** reCAPTCHA: You do not have permission to use this command.");
                return CmdResult::FAILURE;
            }

            if (parameters[0] == "add")
            {
                const std::string& ip = parameters[1];
                PGconn* conn = parent->GetConnection();

                if (!conn)
                {
                    user->WriteNotice("*** reCAPTCHA: Database connection error.");
                    return CmdResult::FAILURE;
                }

                std::string query = INSP_FORMAT("INSERT INTO ircaccess_alloweduser (ip_address) VALUES ('{}')", ip);
                PGresult* res = PQexec(conn, query.c_str());

                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: Failed to add IP: {}", PQerrorMessage(conn)));
                    PQclear(res);
                    return CmdResult::FAILURE;
                }

                PQclear(res);
                user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: Successfully added IP: {}", ip));
                return CmdResult::SUCCESS;
            }
            else if (parameters[0] == "search")
            {
                const std::string& ip = parameters[1];
                PGconn* conn = parent->GetConnection();

                if (!conn)
                {
                    user->WriteNotice("*** reCAPTCHA: Database connection error.");
                    return CmdResult::FAILURE;
                }

                std::string query = INSP_FORMAT("SELECT ip_address FROM ircaccess_alloweduser WHERE ip_address = '{}'", ip);
                PGresult* res = PQexec(conn, query.c_str());

                if (PQresultStatus(res) != PGRES_TUPLES_OK)
                {
                    user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: Failed to search for IP: {}", PQerrorMessage(conn)));
                    PQclear(res);
                    return CmdResult::FAILURE;
                }

                if (PQntuples(res) > 0)
                {
                    user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: IP found: {}", ip));
                }
                else
                {
                    user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: IP not found: {}", ip));
                }

                PQclear(res);
                return CmdResult::SUCCESS;
            }
            else
            {
                user->WriteNotice("*** reCAPTCHA: Unknown subcommand. Use add <ip> or search <ip>.");
                return CmdResult::FAILURE;
            }
        }
    };

    CommandRecaptcha cmd; // Command is a member variable of the module

public:
    ModuleCaptchaCheck()
        : Module(VF_VENDOR, "Requires users to solve a Google reCAPTCHA before joining channels."),
          db(nullptr),
          captcha_success(this, "captcha-success", ExtensionType::USER, true), // Sync with all servers
          cmd(this, this) // Initialize the command
    {
    }

    void init() override
    {
        // Command is automatically registered
    }

    void ReadConfig(ConfigStatus& status) override
    {
        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");

        conninfo = tag->getString("conninfo");
        if (conninfo.empty())
        {
            throw ModuleException(this, "<captchaconfig:conninfo> is a required configuration option.");
        }

        captcha_url = tag->getString("url");
        if (captcha_url.empty())
        {
            throw ModuleException(this, "<captchaconfig:url> is a required configuration option.");
        }

        std::string whitelist = tag->getString("whitelistchan");
        if (!whitelist.empty())
        {
            irc::commasepstream whiteliststream(whitelist);
            std::string channel;
            while (whiteliststream.GetToken(channel))
            {
                whitelist_channels.insert(channel);
            }
        }

        db = GetConnection();
    }

    void OnUnloadModule(Module* mod) override
    {
        if (db)
        {
            PQfinish(db);
        }
    }

    ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
    {
        // Allow users to bypass CAPTCHA check for whitelisted channels
        if (whitelist_channels.count(cname))
            return MOD_RES_PASSTHRU;

        // Sync and check if user has already passed CAPTCHA
        if (captcha_success.Get(user))
            return MOD_RES_PASSTHRU;

        std::string client_sa_str = user->client_sa.str();
        std::string ip = ExtractIP(client_sa_str);

        if (!CheckCaptcha(ip))
        {
            user->WriteNotice("*** reCAPTCHA: Google recaptcha verification is required: You must verify at " + captcha_url + " before joining channels. Need assistance? Join #help .");
            return MOD_RES_DENY;
        }

        captcha_success.Set(user, "passed");
        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleCaptchaCheck)
