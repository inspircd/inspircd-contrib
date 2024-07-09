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
        // Use static variables to avoid reinitializing random number generator each time
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis(100000000000000000, 999999999999999999);
        uint64_t random_number = dis(gen);
        return std::to_string(random_number);
    }

    void AppendRandomID(std::string& message)
    {
        // Generate the random ID to append
        std::string random_id = " - ID: " + GenerateRandomID();
        // Ensure the message length does not exceed the limit after appending
        size_t max_reason_length = 510 - random_id.length();  // 510 to account for possible CR LF at the end
        if (message.length() > max_reason_length)
        {
            message = message.substr(0, max_reason_length);
        }
        // Append the random ID
        message += random_id;
    }

    ModResult HandleLineCommand(const std::string& command, User* source, CommandBase::Params& parameters)
    {
        if (parameters.size() > 1)
        {
            // Append random ID to the existing reason parameter
            AppendRandomID(parameters[1]);
        }
        else
        {
            // Create a new reason with the random ID if it doesn't exist
            std::string id_message = "- ID: " + GenerateRandomID();
            AppendRandomID(id_message);
            parameters.push_back(id_message);
        }

        // Log the command with the appended random ID
        std::string log_message = fmt::format("{} {} {}: {}", source->nick, command, parameters[0], parameters[1]);
        ServerInstance->SNO.WriteToSnoMask('a', log_message);

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

        // Handle specific commands to append the random ID
        if (command == "ZLINE" || command == "GLINE" || command == "KLINE" || command == "KILL")
        {
            return HandleLineCommand(command, user, parameters);
        }

        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleRandomIDxLines)
