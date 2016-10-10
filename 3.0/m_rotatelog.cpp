/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015-2016 Peter Powell <petpow@saberuk.com>
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

/// $ModAuthor: Peter "SaberUK" Powell
/// $ModAuthorMail: petpow@saberuk.com
/// $ModConfig: <rotatelog period="3600">
/// $ModDepends: core 3.0
/// $ModDesc: Rotates the log files after a defined period.


#include "inspircd.h"

class RotateLogTimer : public Timer
{
 public:
	RotateLogTimer() : Timer(3600, true) { }

	bool Tick(time_t) CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Rotating log files ...");
		ServerInstance->Logs->CloseLogs();

		ServerInstance->Logs->OpenFileLogs();
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Log files have been rotated!");
		return true;
	}
};

class ModuleRotateLog : public Module
{
 private:
	 RotateLogTimer* timer;

 public:
	ModuleRotateLog()
	{
		timer = new RotateLogTimer();
	}

	~ModuleRotateLog()
	{
		ServerInstance->Timers.DelTimer(timer);
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Timers.AddTimer(timer);
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("rotatelog");
		timer->SetInterval(tag->getInt("period", 3600));
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Rotates the log files after a defined period.");
	}
};

MODULE_INIT(ModuleRotateLog)
