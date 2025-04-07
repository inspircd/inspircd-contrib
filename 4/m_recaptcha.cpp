/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2024 Jean reverse Chevronnet <mike.chevronnet@gmail.com>
 *
 * This program is distributed under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: reverse Chevronnet <mike.chevronnet@gmail.com>
/// $ModDesc: Google reCAPTCHA v2 verification via JWT with HTTP backend check.
/// $ModConfig: <captchaconfig url="https://chaat.site/recaptcha/verify/"
///             checkurl="https://chaat.site/recaptcha/check_token/"
///             secret="your_jwt_secret"
///             issuer="https://chaat.site"
///             whitelistchans="#help,#opers"
///             whitelistports="6697,7000"
///             message="*** reCAPTCHA: Verify your connection at {url}">
/// $ModDepends: core 4

/// $LinkerFlags: -lcrypto -lcurl
/// $PackageInfo: require_system("arch") jwt-cpp curl nlohmann-json
/// $PackageInfo: require_system("debian") libjwt-dev libcurl4-openssl-dev nlohmann-json3-dev
/// $PackageInfo: require_system("ubuntu") libjwt-dev libcurl4-openssl-dev nlohmann-json3-dev
/// $PackageInfo: require_system("fedora") libjwt-devel libcurl-devel nlohmann-json-devel

#include "inspircd.h"
#include "modules/account.h"
#include "extension.h"
#include <jwt-cpp/jwt.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ModuleCaptchaJwt;

class CommandVerify final : public Command
{
    ModuleCaptchaJwt* parent;

public:
    CommandVerify(Module* Creator, ModuleCaptchaJwt* Parent)
        : Command(Creator, "VERIFY", 1, 1), parent(Parent)
    {
        syntax = { "<jwt_token>" };
    }

    CmdResult Handle(User* user, const Params& parameters) override;
};

class ModuleCaptchaJwt final : public Module
{
private:
    std::string jwt_secret, jwt_issuer, captcha_url, check_url, verify_message;
    BoolExtItem captcha_verified;
    CommandVerify cmdverify;
    Account::API accountapi;

public:
    ModuleCaptchaJwt()
        : Module(VF_VENDOR, "reCAPTCHA JWT verification via CURL endpoint."),
          captcha_verified(this, "captcha-verified", ExtensionType::USER, true),
          cmdverify(this, this),
          accountapi(this) {}

    void ReadConfig(ConfigStatus& status) override
    {
        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");
        jwt_secret = tag->getString("secret");
        jwt_issuer = tag->getString("issuer");
        captcha_url = tag->getString("url");
        check_url = tag->getString("checkurl");
        verify_message = tag->getString("message", "*** reCAPTCHA: Verify your connection at {url}");

        if (jwt_secret.empty() || captcha_url.empty() || check_url.empty())
            throw ModuleException(this, "You must configure 'secret', 'url', and 'checkurl'.");
    }

    ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
    {
        if (user->IsOper() || captcha_verified.Get(user) || (accountapi && accountapi->GetAccountName(user)))
            return MOD_RES_PASSTHRU;

        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");

        std::set<std::string> whitelist_chans;
        irc::commasepstream chanstream(tag->getString("whitelistchans"));
        std::string whitelisted;
        while (chanstream.GetToken(whitelisted))
            whitelist_chans.insert(whitelisted);

        if (whitelist_chans.count(cname))
            return MOD_RES_PASSTHRU;

        std::set<int> whitelist_ports;
        irc::commasepstream portstream(tag->getString("whitelistports"));
        std::string port;
        while (portstream.GetToken(port))
            whitelist_ports.insert(std::stoi(port));

        if (whitelist_ports.count(user->server_sa.port()))
            return MOD_RES_PASSTHRU;

        NotifyUserToVerify(user);
        return MOD_RES_DENY;
    }

    bool CheckTokenWithDjango(const std::string& jwt_token)
    {
        CURL* curl = curl_easy_init();
        if (!curl)
            return false;

        std::string url = check_url + "?token=" + jwt_token;
        std::string response_data;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, true); // false for testing this must be true in production
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t size, size_t nmemb, std::string* data) {
            data->append((char*)ptr, size * nmemb);
            return size * nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
            return false;

        try {
            auto json_response = json::parse(response_data);
            return json_response.value("verified", false);
        } catch (...) {
            return false;
        }
    }

    void VerifyJWT(User* user, const std::string& token)
    {
        try
        {
            auto decoded = jwt::decode(token);
            jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{jwt_secret})
                .with_issuer(jwt_issuer)
                .verify(decoded);

            if (!CheckTokenWithDjango(token))
            {
                user->WriteNotice("*** reCAPTCHA: You must complete verification at the provided link first.");
                return;
            }

            captcha_verified.Set(user, true);
            user->WriteNotice("*** reCAPTCHA: Verification successful. You may now join channels.");
        }
        catch (const std::exception& ex)
        {
            user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: Invalid JWT token ({})", ex.what()));
        }
    }

    void NotifyUserToVerify(User* user)
    {
        std::string token = GenerateJWT(user);
        std::string link = captcha_url + "?token=" + token;

        std::string message = verify_message;
        size_t pos = message.find("{url}");
        if (pos != std::string::npos)
            message.replace(pos, 5, link);

        user->WriteNotice(message);
    }

    std::string GenerateJWT(User* user)
    {
        return jwt::create()
            .set_issuer(jwt_issuer)
            .set_subject(user->uuid)
            .set_issued_at(std::chrono::system_clock::now())
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes{30})
            .sign(jwt::algorithm::hs256{jwt_secret});
    }
};

CmdResult CommandVerify::Handle(User* user, const Params& parameters)
{
    parent->VerifyJWT(user, parameters[0]);
    return CmdResult::SUCCESS;
}

MODULE_INIT(ModuleCaptchaJwt)
