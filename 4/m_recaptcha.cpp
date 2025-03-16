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

/// $ModAuthor: reverse <mike.chevronnet@gmail.com>
/// $ModDesc: Google reCAPTCHA v2 verification via JWT tokens.
/// $ModConfig: <captchaconfig 
///             url="https://test.site/recaptcha/verify/" -> URL to your reCAPTCHA verification script
///             secret="create_your_own_secret" -> needed to sign JWT tokens
///             issuer="https://test.site/"
///             whitelistchans="#help,#opers"
///             message="Please resolve the reCAPTCHA challenge to join channels. {url}"
///             whitelistports="8004,6697">
/// $ModDepends: core 4

/// $LinkerFlags: -lcrypto -ljwt-cpp

#include "inspircd.h"
#include "modules/account.h"
#include "extension.h"
#include <jwt-cpp/jwt.h>

class ModuleCaptchaJwt;

class CommandVerify final : public Command
{
private:
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
    std::string jwt_secret;
    std::string jwt_issuer;
    std::string captcha_url;
    std::string verify_message; 
    BoolExtItem captcha_verified;
    CommandVerify cmdverify;
    Account::API accountapi;

public:
    ModuleCaptchaJwt()
        : Module(VF_VENDOR, "Handles Google reCAPTCHA v2 verification via JWT."),
          captcha_verified(this, "captcha-verified", ExtensionType::USER, true),
          cmdverify(this, this),
          accountapi(this) {}

    void ReadConfig(ConfigStatus& status) override
    {
        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");
        jwt_secret = tag->getString("secret");
        jwt_issuer = tag->getString("issuer");
        captcha_url = tag->getString("url");
        verify_message = tag->getString("message", "*** reCAPTCHA: Verify your connection at {url}");

        if (jwt_secret.empty() || captcha_url.empty())
            throw ModuleException(this, "*** reCAPTCHA: 'secret' and 'url' configs are required.");
    }

    ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
    {   
        // Whitelisted IRC operators check
        if (user->IsOper() || captcha_verified.Get(user))
            return MOD_RES_PASSTHRU;

        // Whitelisted NickServ accounts check
        if (accountapi && accountapi->GetAccountName(user))
        {
            captcha_verified.Set(user, true);
            user->WriteNotice("*** reCAPTCHA: NickServ account verified. You may now join channels.");
            return MOD_RES_PASSTHRU;
        }

        auto& tag = ServerInstance->Config->ConfValue("captchaconfig");

        // Whitelisted channels check
        std::set<std::string> whitelist_chans;
        irc::commasepstream chanstream(tag->getString("whitelistchans"));
        std::string whitelisted;
        while (chanstream.GetToken(whitelisted))
            whitelist_chans.insert(whitelisted);

        if (whitelist_chans.find(cname) != whitelist_chans.end())
            return MOD_RES_PASSTHRU;

        // Whitelisted ports check
        std::set<int> whitelist_ports;
        std::stringstream portstream(tag->getString("whitelistports"));
        std::string port;
        while (std::getline(portstream, port, ','))
            whitelist_ports.insert(std::stoi(port));

        if (whitelist_ports.find(user->server_sa.port()) != whitelist_ports.end())
            return MOD_RES_PASSTHRU;

        NotifyUserToVerify(user);
        return MOD_RES_DENY;
    }

    void VerifyJWT(User* user, const std::string& token)
    {
        try
        {
            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                                .allow_algorithm(jwt::algorithm::hs256{jwt_secret})
                                .with_issuer(jwt_issuer);
            verifier.verify(decoded);

            auto exp = decoded.get_expires_at();
            if (exp < std::chrono::system_clock::now())
            {
                user->WriteNotice("*** reCAPTCHA: Token expired. Request a new one by reconnecting.");
                return;
            }

            captcha_verified.Set(user, true);
            user->WriteNotice("*** reCAPTCHA: Verification successful. You may now join channels.");
        }
        catch (const std::exception& ex)
        {
            user->WriteNotice(INSP_FORMAT("*** reCAPTCHA: Invalid token ({})", ex.what()));
        }
    }

private:
    void NotifyUserToVerify(User* user)
    {
        std::string token = GenerateJWT(user);
        std::string verification_link = INSP_FORMAT("{}?token={}", captcha_url, token);
        std::string final_message = verify_message;
        auto pos = final_message.find("{url}");
        if (pos != std::string::npos)
        final_message.replace(pos, 5, verification_link);
        
        // message+url+token
        user->WriteNotice(final_message);
    }

    std::string GenerateJWT(User* user)
    {
        auto token = jwt::create()
            .set_issuer(jwt_issuer)
            .set_subject(user->uuid)
            .set_issued_at(std::chrono::system_clock::now())
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes{30})
            .sign(jwt::algorithm::hs256{jwt_secret});

        return token;
    }
};

CmdResult CommandVerify::Handle(User* user, const Params& parameters)
{
    parent->VerifyJWT(user, parameters[0]);
    return CmdResult::SUCCESS;
}

MODULE_INIT(ModuleCaptchaJwt)