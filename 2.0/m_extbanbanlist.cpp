/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 genius3000 <genius3000@g3k.solutions>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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

/* $ModAuthor: genius3000 */
/* $ModAuthorMail: genius3000@g3k.solutions */
/* $ModDesc: Provides extban 'b' - Ban list from another channel */
/* $ModDepends: core 2.0 */

/* Helpop Lines for the EXTBANS section
 * Find: '<helpop key="extbans" value="Extended Bans'
 * Place just before the 'j:<channel>' line:
 b:<channel>   Matches users banned in the given channel (requires
               extbanbanlist extras-module).
 */

#include "inspircd.h"


class ExtbanBanlist : public ModeWatcher
{
 public:
	ExtbanBanlist(Module* parent) : ModeWatcher(parent, 'b', MODETYPE_CHANNEL)
	{
	}

	bool BeforeMode(User* source, User* dest, Channel* channel, std::string& param, bool adding, ModeType type)
	{
		if (!IS_LOCAL(source) || type != MODETYPE_CHANNEL || !channel || !adding || param.length() < 3)
			return true;

		// Check for a match to both a regular and nested extban
		if ((param[0] != 'b' || param[1] != ':') && param.find(":b:") == std::string::npos)
			return true;

		std::string chan = param.substr(param.find("b:") + 2);

		Channel* c = ServerInstance->FindChan(chan);
		if (!c)
		{
			source->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :No such channel", source->nick.c_str(), chan.c_str());
			return false;
		}

		if (c == channel)
		{
			source->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :Target channel must be a different channel", source->nick.c_str(), chan.c_str());
			return false;
		}

		ModeHandler* mh = ServerInstance->Modes->FindMode('b', MODETYPE_CHANNEL);
		if (mh->GetLevelRequired() > c->GetPrefixValue(source))
		{
			source->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must have access to modify the banlist to use it", source->nick.c_str(), chan.c_str());
			return false;
		}

		return true;
	}
};

class ModuleExtbanBanlist : public Module
{
	ExtbanBanlist eb;
	bool checking;

 public:
	ModuleExtbanBanlist() : eb(this), checking(false)
	{
	}

	void init()
	{
		if (!ServerInstance->Modes->AddModeWatcher(&eb))
			throw ModuleException("Could not add mode watcher");

		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	~ModuleExtbanBanlist()
	{
		ServerInstance->Modes->DelModeWatcher(&eb);
	}

	ModResult OnCheckBan(User* user, Channel* c, const std::string& mask)
	{
		if (!checking && (mask.length() > 2) && (mask[0] == 'b') && (mask[1] == ':'))
		{
			Channel* chan = ServerInstance->FindChan(mask.substr(2));
			if (!chan)
				return MOD_RES_PASSTHRU;

			for (BanList::iterator ban = chan->bans.begin(); ban != chan->bans.end(); ++ban)
			{
				checking = true;
				bool hit = chan->CheckBan(user, ban->data);
				checking = false;

				if (hit)
					return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::string& output)
	{
		ServerInstance->AddExtBanChar('b');
	}

	Version GetVersion()
	{
		return Version("Extban 'b' - ban list from another channel", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleExtbanBanlist)
