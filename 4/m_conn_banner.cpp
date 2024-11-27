/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
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

/// $ModAuthor: Attila Molnar
/// $ModAuthorMail: attilamolnar@hush.com
/// $ModConfig: <connbanner text="Banner text goes here.">
/// $ModDepends: core 4
/// $ModDesc: Displays a static text to every connecting user before registration


#include "inspircd.h"

class ModuleConnBanner final
	: public Module
{
private:
	std::string text;

public:
	ModuleConnBanner()
		: Module(VF_NONE, "Displays a static text to every connecting user before registration")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("connbanner");
		text = tag->getString("text");
	}

	void OnUserPostInit(LocalUser* user) override
	{
		if (!text.empty())
			user->WriteNotice("*** " + text);
	}
};

MODULE_INIT(ModuleConnBanner)
