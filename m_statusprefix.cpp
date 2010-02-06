/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "m_override.h"

/* $ModDesc: Provides channel mode +V, adding the - prefix
 *  which does nothing but serves as a status symbol. */
/* $ModDepends: core 1.2-1.3 */

#define STATUS_VALUE 1

/** Handles basic operation of +V channel mode
 */
class StatusPrefixBase
{
 private:
	InspIRCd* MyInstance;
	std::string extend;
	std::string type;
	int list;
	int end;
 public:
	StatusPrefixBase(InspIRCd* Instance, const std::string &ext, const std::string &mtype, int l, int e) :
		MyInstance(Instance), extend(ext), type(mtype), list(l), end(e)
	{
	}

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		User* x = MyInstance->FindNick(parameter);
		if (x)
		{
			if (!channel->HasUser(x))
			{
				return std::make_pair(false, parameter);
			}
			else
			{
				std::string item = extend+std::string(channel->name);
				if (x->GetExt(item))
				{
					return std::make_pair(true, x->nick);
				}
				else
				{
					return std::make_pair(false, parameter);
				}
			}
		}
		return std::make_pair(false, parameter);
	}

	void RemoveMode(Channel* channel, char mc, irc::modestacker* stack)
	{
		CUList* cl = channel->GetUsers();
		std::string item = extend + std::string(channel->name);
		std::vector<std::string> mode_junk;
		mode_junk.push_back(channel->name);
		irc::modestacker modestack(MyInstance, false);
		std::deque<std::string> stackresult;

		for (CUList::iterator i = cl->begin(); i != cl->end(); i++)
		{
			if (i->first->GetExt(item))
			{
				if (stack)
					stack->Push(mc, i->first->nick);
				else
					modestack.Push(mc, i->first->nick);
			}
		}

		if (stack)
			return;

		while (modestack.GetStackedLine(stackresult))
		{
			mode_junk.insert(mode_junk.end(), stackresult.begin(), stackresult.end());
			MyInstance->SendMode(mode_junk, MyInstance->FakeClient);
			mode_junk.erase(mode_junk.begin() + 1, mode_junk.end());
		}
	}

	User* FindAndVerify(std::string &parameter, Channel* channel)
	{
		User* theuser = MyInstance->FindNick(parameter);
		if ((!theuser) || (!channel->HasUser(theuser)))
		{
			parameter.clear();
			return NULL;
		}
		return theuser;
	}

	ModeAction HandleChange(User* source, User* theuser, bool adding, Channel* channel, std::string &parameter)
	{
		std::string item = extend+std::string(channel->name);

		if (adding)
		{
			if (!theuser->GetExt(item))
			{
				theuser->Extend(item);
				parameter = theuser->nick;
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (theuser->GetExt(item))
			{
				theuser->Shrink(item);
				parameter = theuser->nick;
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}
};

/** Abstraction of StatusPrefixBase for channel mode +a
 */
class StatusPrefix : public ModeHandler, public StatusPrefixBase
{
 public:
	StatusPrefix(InspIRCd* Instance, char my_prefix)
		: ModeHandler(Instance, 'V', 1, 1, true, MODETYPE_CHANNEL, false, my_prefix, 0, TR_NICK),
		  StatusPrefixBase(Instance,"cm_protect_","protected user", 388, 389) { }

	unsigned int GetPrefixRank()
	{
		return STATUS_VALUE;
	}

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		return StatusPrefixBase::ModeSet(source, dest, channel, parameter);
	}

	void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		StatusPrefixBase::RemoveMode(channel, this->GetModeChar(), stack);
	}

	void RemoveMode(User* user, irc::modestacker* stack)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		User* theuser = StatusPrefixBase::FindAndVerify(parameter, channel);

		int status = channel->GetStatus(source);

		if (!theuser)
			return MODEACTION_DENY;

		std::string founder = "cm_founder_"+std::string(channel->name);

		if ((!adding))
		{
			return StatusPrefixBase::HandleChange(source, theuser, adding, channel, parameter);
		}

		char isoverride=0;
		Module *Override = ServerInstance->Modules->FindFeature("Override");
		if (Override)
		{
			OVRrequest ovr(NULL,Override,source,"OTHERMODE");
			const char * tmp = ovr.Send();
			isoverride = tmp[0];
		}
		// source has +h or higher, is a server, or ulined, we'll let them +-V the user.
		if (source == ServerInstance->FakeClient ||
			(ServerInstance->ULine(source->nick.c_str())) ||
			(ServerInstance->ULine(source->server)) ||
			(!*source->server) ||
			((status >= STATUS_HOP) || (ServerInstance->ULine(source->server))) ||
			(!IS_LOCAL(source)) ||
			isoverride
			)
		{
			return StatusPrefixBase::HandleChange(source, theuser, adding, channel, parameter);
		}
		else
		{
			// bzzzt, wrong answer!
			source->WriteNumeric(482, "%s %s :You're not a channel (half)operator", source->nick.c_str(), channel->name.c_str());
			return MODEACTION_DENY;
		}
	}

};

class ModuleStatusPrefix : public Module
{

	char SPrefix;
	bool booting;
	StatusPrefix* cs;

 public:

	ModuleStatusPrefix(InspIRCd* Me)
		: Module(Me), SPrefix(0), booting(true), cs(NULL)
	{
		/* Load config stuff */
		LoadSettings();
		booting = false;

		/* Initialise module variables */

		cs = new StatusPrefix(ServerInstance, SPrefix);

		if (!ServerInstance->Modes->AddMode(cs))
		{
			delete cs;
			throw ModuleException("Could not add new mode!");
		}

		Implementation eventlist[] = { I_OnUserKick, I_OnUserPart };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent)
	{
		// FIX: when someone gets kicked from a channel we must remove their Extensibles!
		user->Shrink("cm_status_"+std::string(chan->name));
	}

	virtual void OnUserPart(User* user, Channel* channel, std::string &partreason, bool &silent)
	{
		// FIX: when someone parts a channel we must remove their Extensibles!
		user->Shrink("cm_status_"+std::string(channel->name));
	}

	void LoadSettings()
	{
		ConfigReader Conf(ServerInstance);

		std::string spre = Conf.ReadValue("statusprefix", "prefix", 0);
		SPrefix = spre.empty() ? 0 : spre[0];

		if (cs && ServerInstance->Modes->FindPrefix(SPrefix) == cs)
			throw ModuleException("Looks like the +V prefix you picked for m_statusprefix is already in use. Pick another.");
	}

	virtual ~ModuleStatusPrefix()
	{
		ServerInstance->Modes->DelMode(cs);
		delete cs;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleStatusPrefix)
