/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Peter Powell <petpow@saberuk.com>
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


/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModAuthorMail: petpow@saberuk.com */
/* $ModDesc: Rotates the log files after a defined period. */
/* $ModConfig: <rotatelog period="3600"> */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class RotateLogTimer : public Timer
{
 public:
	RotateLogTimer() : Timer(3600, ServerInstance->Time(), true) { }

	void Tick(time_t)
	{
		ServerInstance->Logs->Log("m_rotatelog", DEFAULT, "Rotating log files ...");
		ServerInstance->Logs->CloseLogs();

		ServerInstance->Logs->OpenFileLogs();
		ServerInstance->Logs->Log("m_rotatelog", DEFAULT, "Log files have been rotated!");
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
		ServerInstance->Timers->DelTimer(timer);
	}

	void init()
	{
		OnRehash(NULL);
		ServerInstance->Modules->Attach(I_OnRehash, this);
		ServerInstance->Timers->AddTimer(timer);
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("rotatelog");
		timer->SetTimer(ServerInstance->Time() + tag->getInt("period", 3600));
	}

	Version GetVersion()
	{
		return Version("Rotates the log files after a defined period.");
	}
};

MODULE_INIT(ModuleRotateLog)
