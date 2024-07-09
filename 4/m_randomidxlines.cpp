/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2024 Jean reverse Chevronnet <mike.chevronnet@gmail.com>
 *
 * This program is distributed under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: Jean reverse Chevronnet <mike.chevronnet@gmail.com>
/// $ModDesc: Enhances /zline, /gline, /kline, /kill and similar commands by adding a random ID to the end for better log identification.
/// $ModDepends: core 4

#include "inspircd.h"
#include "xline.h"
#include <random>

class ModuleRandomIDxLines : public Module
{
private:
    std::string GenerateRandomID()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis(1000000000, 9999999999);
        return ConvToStr(dis(gen));
    }

    void AppendRandomID(std::string& message)
    {
        std::string random_id = " - ID: " + GenerateRandomID();
        size_t max_reason_length = 510 - random_id.length();  // 510 to account for possible CR LF at the end
        if (message.length() > max_reason_length)
        {
            message = message.substr(0, max_reason_length);
        }
        message += random_id;
    }

    bool IsValidHostMask(const std::string& mask)
    {
        return mask.find('@') != std::string::npos || (mask.length() > 0 && mask[0] == '@');
    }

    bool IsValidDuration(const std::string& duration)
    {
        for (char c : duration)
        {
            if (!isdigit(c) && c != 's' && c != 'm' && c != 'h' && c != 'd' && c != 'w')
                return false;
        }
        return true;
    }

    bool XLineExists(const std::string& command, const std::string& target)
    {
        if (command == "ZLINE")
            return ServerInstance->XLines->MatchesLine("Z", target) != nullptr;
        else if (command == "GLINE")
            return ServerInstance->XLines->MatchesLine("G", target) != nullptr;
        else if (command == "KLINE")
            return ServerInstance->XLines->MatchesLine("K", target) != nullptr;
        return false;
    }

    ModResult HandleLineCommand(const std::string& command, User* source, CommandBase::Params& parameters)
    {
        // Only add random ID if there are enough parameters to set a line (target, duration, reason)
        if (parameters.size() > 2 && IsValidHostMask(parameters[0]) && IsValidDuration(parameters[1]))
        {
            if (!XLineExists(command, parameters[0]))
            {
                // Append random ID to the existing reason parameter
                AppendRandomID(parameters.back());
                std::string log_message = INSP_FORMAT("{} {} {}: {}", source->nick, command, parameters[0], parameters.back());
                ServerInstance->SNO.WriteToSnoMask('a', log_message);
            }
        }

        return MOD_RES_PASSTHRU;
    }

public:
    ModuleRandomIDxLines()
        : Module(VF_VENDOR, "Enhances /zline, /gline, /kline, /kill and similar commands by adding a random ID to the end for better log identification.")
    {
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
    {
        if (!validated)
            return MOD_RES_PASSTHRU;

        // Handle commands
        if ((command == "ZLINE" || command == "GLINE" || command == "KLINE" || command == "KILL"))
        {
            return HandleLineCommand(command, user, parameters);
        }

        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleRandomIDxLines)

