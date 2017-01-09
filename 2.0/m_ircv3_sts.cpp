/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015-2017 Attila Molnar <attilamolnar@hush.com>
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
/* $ModDesc: Implements IRCv3 STS (Strict Transport Security) policy advertisement */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
#include "m_cap.h"

class STSCap
{
	std::string cap;

 public:
	void HandleEvent(Event& ev)
	{
		if (ev.id != "cap_request")
			return;

		// Empty cap name means configuration is invalid
		if (cap.empty())
			return;

		CapEvent* data = static_cast<CapEvent*>(&ev);
		if (data->type == CapEvent::CAPEVENT_LS)
			data->wanted.push_back(cap);
	}

	void SetPolicy(const std::string& newpolicystr)
	{
		cap = "draft/sts=" + newpolicystr;
	}
};

class STSPolicy
{
	unsigned long duration;
	unsigned int port;
	bool preload;

 public:
	STSPolicy(unsigned long Duration = 0, unsigned int Port = 0, bool Preload = false)
		: duration(Duration)
		, port(Port)
		, preload(Preload)
	{
	}

	bool operator==(const STSPolicy& other) const
	{
		return ((duration == other.duration) && (port == other.port) && (preload == other.preload));
	}

	std::string GetString() const
	{
		std::string newpolicystr = "duration=";
		newpolicystr.append(ConvToStr(duration)).append(",port=").append(ConvToStr(port));
		if (preload)
			newpolicystr.append(",preload");
		return newpolicystr;
	}
};

class ModuleIRCv3STS : public Module
{
	STSCap cap;
	STSPolicy policy;

 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist) / sizeof(Implementation));
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("sts");
		unsigned int port = tag->getInt("port", 6697);
		if ((port <= 0) || (port > 65535))
		{
			ServerInstance->Logs->Log("m_ircv3_sts", DEFAULT, "STS: Invalid port specified (%u), not applying policy", port);
			return;
		}

		std::string durationstr;
		if (!tag->readString("duration", durationstr))
		{
			ServerInstance->Logs->Log("m_ircv3_sts", DEFAULT, "STS: Duration not configured, not applying policy");
			return;
		}

		unsigned long duration = ServerInstance->Duration(durationstr);
		bool preload = tag->getBool("preload");
		STSPolicy newpolicy(duration, port, preload);
		if (newpolicy == policy)
			return;

		policy = newpolicy;
		const std::string newpolicystr = policy.GetString();
		ServerInstance->Logs->Log("m_ircv3_sts", DEFAULT, "STS: policy changed to \"%s\"", newpolicystr.c_str());
		cap.SetPolicy(newpolicystr);
	}

	void OnEvent(Event& ev)
	{
		cap.HandleEvent(ev);
	}

	Version GetVersion()
	{
		return Version("Implements IRCv3 STS (Strict Transport Security) policy advertisement");
	}
};

MODULE_INIT(ModuleIRCv3STS)
