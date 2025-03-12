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
 /// $ModDepends: core 4
 
 /// $LinkerFlags: -lcrypto
 
 #include "inspircd.h"
 #include "modules/sql.h"
 #include "extension.h"
 #include <openssl/sha.h>
 #include "modules/account.h" //Account API
 
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
     dynamic_reference<SQL::Provider> sql;
     BoolExtItem captcha_verified;
     CommandVerificar cmdverificar;
     Account::API accountapi;
 
 public:
     ModuleCaptchaCheck()
         : Module(VF_VENDOR, "Google reCAPTCHA v2+token server-side."),
           sql(this, "SQL"),
           captcha_verified(this, "captcha-verified", ExtensionType::USER, true),
           cmdverificar(this, this),
           accountapi(this) {}
 
     void ReadConfig(ConfigStatus& status) override
     {
         auto& tag = ServerInstance->Config->ConfValue("captchaconfig");
 
         // SQL query
         captcha_url = tag->getString("url");
         query = tag->getString("query", "SELECT COUNT(*) FROM recaptcha_app_verificationtoken WHERE token = $1 AND created_at + INTERVAL '30 minutes' > NOW()", 1);
 
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
 
         std::string formatted_query = INSP_FORMAT(
             "SELECT COUNT(*) FROM recaptcha_app_verificationtoken WHERE token = '{}' AND created_at + INTERVAL '30 minutes' > NOW()",
             token);
 
         sql->Submit(new ValidateTokenQuery(this, user, captcha_verified), formatted_query);
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
 
         size_t pos = message.find("{url}");
         if (pos != std::string::npos)
             message.replace(pos, 5, link);
 
         user->WriteNotice(message);
     }
 
     std::string GenerateToken()
     {
         static const char hex_chars[] = "0123456789abcdef";
         std::string token;
 
         for (int i = 0; i < 64; ++i)
             token += hex_chars[std::rand() % 16];
 
         return token;
     }
 
     std::string GenerateHash(const std::string& token)
     {
         unsigned char hash[SHA256_DIGEST_LENGTH];
         SHA256(reinterpret_cast<const unsigned char*>(token.c_str()), token.size(), hash);
 
         std::string hash_string;
         for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
         {
             hash_string += "0123456789abcdef"[hash[i] >> 4];
             hash_string += "0123456789abcdef"[hash[i] & 0xf];
         }
 
         return hash_string;
     }
 };
 
 CmdResult CommandVerificar::Handle(User* user, const Params& parameters)
 {
     const std::string& token = parameters[0];
     parent->ValidateToken(user, token);
     return CmdResult::SUCCESS;
 }
 
 MODULE_INIT(ModuleCaptchaCheck)