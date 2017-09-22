/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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


/* $ModAuthor: Attila Molnar */
/* $ModAuthorMail: attilamolnar@hush.com */
/* $ModDesc: Reload SSL credentials on SIGUSR1 */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

static volatile sig_atomic_t signaled;

class ModuleRehashSSLSignal : public Module
{
	static void SignalHandler(int sig)
	{
		signaled = 1;
	}

 public:
	~ModuleRehashSSLSignal()
	{
		signal(SIGUSR1, SIG_IGN);
	}

	void init()
	{
		ServerInstance->Modules->Attach(I_OnBackgroundTimer, this);
		signal(SIGUSR1, SignalHandler);
	}

	void OnBackgroundTimer(time_t currtime)
	{
		if (!signaled)
			return;

		const std::string feedbackmsg = "Got SIGUSR1, reloading SSL credentials";
		ServerInstance->SNO->WriteGlobalSno('a', feedbackmsg);
		ServerInstance->Logs->Log("m_rehashsslsignal", DEFAULT, feedbackmsg);
		const std::string str = "ssl";
		FOREACH_MOD(I_OnModuleRehash, OnModuleRehash(NULL, str));
		signaled = 0;
	}

	Version GetVersion()
	{
		return Version("Reload SSL credentials on SIGUSR1");
	}
};

MODULE_INIT(ModuleRehashSSLSignal)
