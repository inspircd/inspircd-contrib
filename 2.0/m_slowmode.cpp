/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Adam <Adam@anope.org>
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


#include "inspircd.h"

/* $ModAuthor: Adam */
/* $ModAuthorMail: adam@anope.org */
/* $ModDesc: Provides channel mode +W (slow mode) */
/* $ModDepends: core 2.0 */

/** Holds slowmode settings and state for mode +W
 */
class slowmodesettings
{
 public:
	unsigned int lines;
	unsigned int secs;

	unsigned int counter;
	time_t reset;

	slowmodesettings(int l, int s) : lines(l), secs(s)
	{
		counter = 0;
		reset = ServerInstance->Time() + secs;
	}

	bool addmessage()
	{
		if (ServerInstance->Time() > reset)
		{
			counter = 0;
			reset = ServerInstance->Time() + secs;
		}

		return ++counter >= lines;
	}
};

/** Handles channel mode +W
 */
class MsgFlood : public ModeHandler
{
	SimpleExtItem<slowmodesettings> &ext;
 public:

	MsgFlood(Module* Creator, SimpleExtItem<slowmodesettings> &e)
		: ModeHandler(Creator, "slowmode", 'W', PARAM_SETONLY, MODETYPE_CHANNEL)
		, ext(e)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			std::string::size_type colon = parameter.find(':');
			if (colon == std::string::npos || parameter.find('-') != std::string::npos)
			{
				source->WriteNumeric(608, "%s %s :Invalid slowmode parameter", source->nick.c_str(),channel->name.c_str());
				return MODEACTION_DENY;
			}

			/* Set up the slowmode parameters for this channel */
			unsigned int nlines = ConvToInt(parameter.substr(0, colon));
			unsigned int nsecs = ConvToInt(parameter.substr(colon+1));

			if ((nlines < 2) || nsecs < 1)
			{
				source->WriteNumeric(608, "%s %s :Invalid slowmode parameter", source->nick.c_str(), channel->name.c_str());
				return MODEACTION_DENY;
			}

			slowmodesettings* f = ext.get(channel);
			if (f && nlines == f->lines && nsecs == f->secs)
				// mode params match
				return MODEACTION_DENY;

			ext.set(channel, new slowmodesettings(nlines, nsecs));
			parameter = ConvToStr(nlines) + ":" + ConvToStr(nsecs);
			channel->SetModeParam(GetModeChar(), parameter);
			return MODEACTION_ALLOW;
		}
		else
		{
			if (!channel->IsModeSet(GetModeChar()))
				return MODEACTION_DENY;

			ext.unset(channel);
			channel->SetModeParam(GetModeChar(), "");
			return MODEACTION_ALLOW;
		}
	}
};

class ModuleMsgFlood : public Module
{
	MsgFlood mf;
	SimpleExtItem<slowmodesettings> ext;

	ModResult ProcessMessages(User* user, Channel* dest)
	{
		if (ServerInstance->ULine(user->server) || !dest->IsModeSet(mf.GetModeChar()))
			return MOD_RES_PASSTHRU;

		if (ServerInstance->OnCheckExemption(user, dest, "slowmode") == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		slowmodesettings *f = ext.get(dest);
		if (f == NULL || !f->addmessage())
			return MOD_RES_PASSTHRU;

		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		user->WriteNumeric(404, "%s %s :Message throttled due to flood", user->nick.c_str(), dest->name.c_str());
		return MOD_RES_DENY;
	}

 public:

	ModuleMsgFlood()
		: mf(this, ext)
		, ext("slowmode", this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(mf);
		ServerInstance->Modules->AddService(ext);

		Implementation eventlist[] = { I_OnUserPreNotice, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnUserPreMessage(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
			return ProcessMessages(user, static_cast<Channel*>(dest));

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNotice(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
			return ProcessMessages(user, static_cast<Channel*>(dest));

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Provides channel mode +" + ConvToStr(mf.GetModeChar()) + " (slowmode)");
	}
};

MODULE_INIT(ModuleMsgFlood)
