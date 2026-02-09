/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2020 Elizabeth Myers <elizabeth@interlinked.me>
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

/// $ModAuthor: Adam
/// $ModAuthorMail: Adam@anope.org
/// $ModDesc: Implements "target change" detection to help prevent spam bots
/// $ModDepends: core 3

/*
 * tgchange. Detects users messaging many different targets rapidly.
 * Ported from Charybdis's tgchange.c
 */

#include "inspircd.h"
#include "extension.h"

enum
{
	ERR_TARGCHANGE = 707
};

namespace
{
	unsigned TGCHANGE_NUM;   /* how many targets we keep track of */
	unsigned TGCHANGE_REPLY; /* how many reply targets */
}

typedef void Target;

class TGInfo
{
	time_t last;

	std::deque<Target *> targets;
	std::deque<Target *> reply_targets;

 public:
	TGInfo() : last(0)
	{
	}

	bool AddTarget(Target *target)
	{
		/* already exists? */
		std::deque<Target *>::iterator it = std::find(targets.begin(), targets.end(), target);
		if (it != targets.end())
		{
			/* yes. move it to the beginning */
			targets.erase(it);
			targets.push_front(target);
			return true;
		}

		/* or as a reply target? */
		it = std::find(reply_targets.begin(), reply_targets.end(), target);
		if (it != reply_targets.end())
		{
			reply_targets.erase(it);
			reply_targets.push_front(target);
			return true;
		}

		/* try to clear some old targets  */
		time_t t = (ServerInstance->Time() - last) / 60;
		if (t > 0)
		{
			/* clear one target per minute */

			size_t sz = targets.size();
			if (static_cast<size_t>(t) > sz)
				sz = 0;
			else
				sz -= t;

			targets.resize(sz);

			last = ServerInstance->Time();
		}

		if (targets.size() >= TGCHANGE_NUM)
		{
			/* still no free targets? */
			return false;
		}

		/* add new target */
		targets.push_front(target);

		return true;
	}

	void AddReply(User *target)
	{
		std::deque<Target *>::iterator it = std::find(reply_targets.begin(), reply_targets.end(), target);
		if (it != reply_targets.end())
			/* already exists, move to front */
			reply_targets.erase(it);
		else if (reply_targets.size() >= TGCHANGE_REPLY)
			/* list growing too large? */
			reply_targets.pop_back();

		reply_targets.push_front(target);
	}
};

class TGExtInfo : public SimpleExtItem<TGInfo>
{
 public:
	TGExtInfo(const std::string& Key, Module* Creator)
		: SimpleExtItem<TGInfo>(Creator, Key, ExtensionType::USER)
	{
	}

	TGInfo *get_user(LocalUser *user)
	{
		TGInfo *t = Get(user);
		if (!t)
		{
			t = new TGInfo();
			Set(user, t);
		}
		return t;
	}
};

class ModuleTGChange final
	: public Module
{
	TGExtInfo tginfo;

public:
	ModuleTGChange()
		: Module(VF_NONE, "Implements \"target change\" detection to help prevent spam bots")
		, tginfo("tginfo", this)
	{
	}

	void ReadConfig(ConfigStatus& config) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("tgchange");
		TGCHANGE_NUM = tag->getNum<unsigned>("num", 10);
		TGCHANGE_REPLY = tag->getNum<unsigned>("reply", 5);
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		/* don't count ctcp replies */
		if (details.type == MessageType::NOTICE || details.IsCTCP())
			return MOD_RES_PASSTHRU;

		switch (target.type)
		{
		case MessageTarget::TYPE_USER:
			return Target(user, target.Get<User>());

		case MessageTarget::TYPE_CHANNEL:
			return Target(user, target.Get<Channel>());

		default:
			return MOD_RES_PASSTHRU;
		}
	}

	ModResult OnUserPreInvite(User* source, User* dest, Channel* channel, time_t timeout) override
	{
		return Target(source, dest);
	}

 private:
	bool Allowed(LocalUser *source, User *target)
	{
		for (User::ChanList::iterator i = source->chans.begin(); i != source->chans.end(); i++)
		{
			Membership *m = *i;

			if (m->GetRank() >= VOICE_VALUE && std::find(target->chans.begin(), target->chans.end(), m) != target->chans.end())
				return true;
		}

		return false;
	}

	ModResult Target(User *source, User *dest)
	{
		if (LocalUser *lsource = IS_LOCAL(source))
		{
			if (source != dest && !dest->server->IsService() && !Allowed(lsource, dest) && !source->IsOper())
			{
				ModResult m = Target(lsource, dest, dest->nick);
				if (m != MOD_RES_PASSTHRU)
					return m;
			}
		}

		if (LocalUser *ldest = IS_LOCAL(dest))
		{
			if (source != dest && !source->server->IsService() && !dest->IsOper())
			{
				TGInfo *tg = tginfo.get_user(ldest);
				tg->AddReply(source);
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult Target(User *source, Channel *dest)
	{
		return IS_LOCAL(source) && !source->IsOper() ? Target(IS_LOCAL(source), dest, dest->name) : MOD_RES_PASSTHRU;
	}

	ModResult Target(LocalUser *source, ::Target *target, const std::string &name)
	{
		TGInfo *tg = tginfo.get_user(source);
		if (!tg->AddTarget(target))
		{
			source->WriteNumeric(ERR_TARGCHANGE, name, "Targets changing too fast, message dropped");
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleTGChange)
