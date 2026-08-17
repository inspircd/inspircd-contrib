/*
* InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 TheUnstoppable <theunstoppable1000@gmail.com>
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

/// $ModAuthor: TheUnstoppable
/// $ModAuthorMail: theunstoppable1000@gmail.com
/// $ModConfig: <connect matchgecos="John Doe\nJane Doe"> # Separate values with new line
/// $ModDepends: core 4
/// $ModDesc: Allows a connect class to match by real name(s)/gecos.

#include "inspircd.h"

class ModuleConnMatchGecos : public Module {
public:
    ModuleConnMatchGecos() : Module(VF_NONE, "Allows a connect class to match by real name(s)/gecos.") {}

    ModResult OnPreChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, std::optional<Numeric::Numeric>& errnum) override
    {
        auto connClass = klass.get();

        std::string matches;
        if (!connClass->config->readString("matchgecos", matches, true))
        {
            return MOD_RES_PASSTHRU;
        }

        std::istringstream ss(matches);
        while (!ss.eof())
        {
            std::string line;
            std::getline(ss, line);

            if (line.empty())
            {
                continue;
            }

            if (InspIRCd::Match(line, user->GetRealName()))
            {
                return MOD_RES_PASSTHRU;
            }
        }
        
        return MOD_RES_DENY;
    }
};

MODULE_INIT(ModuleConnMatchGecos)