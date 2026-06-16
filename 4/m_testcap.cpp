/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015-2016 Sadie Powell <sadie@sadiepowell.dev>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@sadiepowell.dev
/// $ModDepends: core 3
/// $ModDesc: Adds capabilities for testing client capability negotiation implementations.


#include "inspircd.h"
#include "modules/cap.h"

class DummyCap final
	: public Cap::Capability
{
private:
	std::string value;

	const std::string* GetValue(LocalUser* user) const override
	{
		return &this->value;
	}

	bool OnRequest(LocalUser* user, bool add) override
	{
		return ServerInstance->GenRandomInt(10) == 5;
	}

public:
	DummyCap(Module* mod, size_t i)
		: Cap::Capability(mod, INSP_FORMAT("inspircd.org/test-cap-{:02}", i))
	{
		if (i % 2)
			this->value = ServerInstance->GenRandomStr(ServerInstance->GenRandomInt(32));
		SetActive(true);
	}
};

class ModuleTestCap final
	: public Module
{
private:
	std::vector<std::unique_ptr<DummyCap>> caps;

public:
	ModuleTestCap()
		: Module(VF_NONE, "Adds capabilities for testing client capability negotiation implementations.")
	{
	}

	void init() override
	{
		for (size_t i = 1; i <= Cap::MAX_CAPS; ++i)
		{
			try
			{
				auto cap = std::make_unique<DummyCap>(this, i);
				this->caps.push_back(std::move(cap));
			}
			catch (const CoreException& ex)
			{
				ServerInstance->Logs.Debug(MODNAME, "Exception from cap {}: {}", i, ex.what());
				break;
			}
		}
	}
};

MODULE_INIT(ModuleTestCap)
