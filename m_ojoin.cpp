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

/*
 * Written for InspIRCd-1.2 by Taros on the Tel'Laerad M&D Team
 * <http://tellaerad.net>
 */

#include "inspircd.h"

/* $ModConfig: <ojoin prefix="!" notice="yes" op="yes">
 *  Specify the prefix that +Y will grant here, it should be unused.
 *  Leave prefix empty if you do not wish +Y to grant a prefix
 *  If notice is set to on, upon ojoin, the server will notice
 *  the channel saying that the oper is joining on network business
 *  If op is set to on, it will give them +o along with +Y */
/* $ModDesc: Provides the /ojoin command, which joins a user to a channel on network business, and gives them +Y, which makes them immune to kick / deop and so on. */
/* $ModAuthor: Taros */
/* $ModAuthorMail: taros34@hotmail.com */
/* $ModDepends: core 1.2 */'

/* A note: This will not protect against kicks from services,
 * ulines, or operoverride. */

#define NETWORK_VALUE 9000000

bool notice;
bool op;

/** Handle /OJOIN
 */
class CommandOjoin : public Command
{
 public:
	CommandOjoin (InspIRCd* Instance) : Command(Instance,"OJOIN", "o", 1, false, 0)
	{
		this->source = "m_ojoin.so";
		syntax = "<channel>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		std::vector<std::string> modechange;
		modechange.push_back(parameters[0]);
		modechange.push_back("+Y");
		modechange.push_back(user->nick);

		std::vector<std::string> opmodechange;
		opmodechange.push_back(parameters[0]);
		opmodechange.push_back("+Yo");
		opmodechange.push_back(user->nick);
		opmodechange.push_back(user->nick);

		std::string targetchannel = parameters[0];

		// Make sure the channel name is allowable.
		if (!ServerInstance->IsChannel(parameters[0].c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in channel name or name too long");
			return CMD_FAILURE;
		}

		// Drag them in, give them +Y and notice the channel/op them if set in the config.
		if (Channel::JoinUser(ServerInstance, user, parameters[0].c_str(), true, "", false, ServerInstance->Time()))
		{
			ServerInstance->SNO->WriteToSnoMask('a', std::string(user->nick)+" used OJOIN to join "+parameters[0]);
			ServerInstance->PI->SendSNONotice("A", std::string(user->nick)+" used OJOIN to join "+parameters[0]);

			if (op)
				ServerInstance->SendMode(opmodechange,this->ServerInstance->FakeClient);
			
			else
				ServerInstance->SendMode(modechange,this->ServerInstance->FakeClient);
				
			ServerInstance->PI->SendMode(targetchannel, ServerInstance->Modes->GetLastParseParams(), ServerInstance->Modes->GetLastParseTranslate());

			if (notice)
			{
				Channel* channel = ServerInstance->FindChan(parameters[0]);
				channel->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s joined on official network business.", parameters[0].c_str(), user->nick.c_str());
				ServerInstance->PI->SendChannelNotice(channel, 0, std::string(user->nick) + " joined on official network business.");
			}

			return CMD_SUCCESS;
		}
		
		else
		{
			user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Could not join "+parameters[0]);
			return CMD_FAILURE;
		}
	}
};

/** Handles basic operation of +Y channel mode
 */
class NetworkPrefixBase
{
 private:
	InspIRCd* MyInstance;
	std::string extend;
	std::string type;
	int list;
	int end;
 public:
	NetworkPrefixBase(InspIRCd* Instance, const std::string &ext, const std::string &mtype, int l, int e) :
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

/** Abstraction of NetworkPrefixBase for channel mode +Y
 */
class NetworkPrefix : public ModeHandler, public NetworkPrefixBase
{
 public:
	NetworkPrefix(InspIRCd* Instance, char my_prefix)
		: ModeHandler(Instance, 'Y', 1, 1, true, MODETYPE_CHANNEL, false, my_prefix, 0, TR_NICK),
		  NetworkPrefixBase(Instance,"cm_network_","official user", 388, 389) { }

	unsigned int GetPrefixRank()
	{
		return NETWORK_VALUE;
	}

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		return NetworkPrefixBase::ModeSet(source, dest, channel, parameter);
	}

	void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		NetworkPrefixBase::RemoveMode(channel, this->GetModeChar(), stack);
	}

	void RemoveMode(User* user, irc::modestacker* stack)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode)
	{
		User* theuser = NetworkPrefixBase::FindAndVerify(parameter, channel);

		if (!theuser)
			return MODEACTION_DENY;

		 // source is a server, or ulined, we'll let them +-Y the user.
		if (source == ServerInstance->FakeClient ||
			((source == theuser) && (!adding)) ||
			(ServerInstance->ULine(source->nick.c_str())) ||
			(ServerInstance->ULine(source->server)) ||
			(!*source->server) ||
			(!IS_LOCAL(source))
			)
		{
			return NetworkPrefixBase::HandleChange(source, theuser, adding, channel, parameter);
		}
		else
		{
			// bzzzt, wrong answer!
			source->WriteNumeric(482, "%s %s :Only servers may change this mode.", source->nick.c_str(), channel->name.c_str());
			return MODEACTION_DENY;
		}
	}

};

class ModuleOjoin : public Module
{

	char NPrefix;
	NetworkPrefix* np;
	CommandOjoin*	mycommand;

 public:

	ModuleOjoin(InspIRCd* Me)
		: Module(Me), NPrefix(0), np(NULL)
	{

		/* Load config stuff */
		OnRehash(NULL);

		/* Initialise module variables */

		np = new NetworkPrefix(ServerInstance, NPrefix);

		mycommand = new CommandOjoin(ServerInstance);
		ServerInstance->AddCommand(mycommand);

		if (!ServerInstance->Modes->AddMode(np))
		{
			delete np;
			throw ModuleException("Could not add new mode!");
		}

		Implementation eventlist[] = { I_OnUserKick, I_OnUserPart, I_OnAccessCheck, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	virtual void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent)
	{
		// FIX: when someone gets kicked from a channel we must remove their Extensibles!
		user->Shrink("cm_network_"+std::string(chan->name));
	}

	virtual void OnUserPart(User* user, Channel* channel, std::string &partreason, bool &silent)
	{
		// FIX: when someone parts a channel we must remove their Extensibles!
		user->Shrink("cm_network_"+std::string(channel->name));
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf(ServerInstance);

		std::string npre = Conf.ReadValue("ojoin", "prefix", 0);
		NPrefix = npre.empty() ? 0 : npre[0];

		if (np && ServerInstance->Modes->FindPrefix(NPrefix) == np)
			throw ModuleException("Looks like the +Y prefix you picked for m_ojoin is already in use. Pick another.");

		notice = Conf.ReadFlag("ojoin", "notice", "yes", 0);
		op = Conf.ReadFlag("ojoin", "op", "yes", 0);
	}

	virtual int OnAccessCheck(User* source,User* dest,Channel* channel,int access_type)
	{
		// Here's where we preform access checks, and disallow any kicking/deopping
		// of +Y users. 

		// If there's no dest, it's not for us.
		if (!dest)
			return ACR_DEFAULT;

		// If a ulined nickname, or a server is setting the mode, let it
		// do whatever it wants.
		if ((ServerInstance->ULine(source->nick.c_str())) || (ServerInstance->ULine(source->server)) || (!*source->server))
			return ACR_ALLOW;

		std::string network("cm_network_"+channel->name);

		// Don't do anything if they're not +Y
		if (!dest->GetExt(network))
			return ACR_DEFAULT;

		// Let them do whatever they want to themselves.
		if (source == dest)
			return ACR_DEFAULT;

		switch (access_type)
		{
			// Disallow deopping of +Y users.
			case AC_DEOP:
				source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't deop "+dest->nick+" as they're on official network business.");
				return ACR_DENY;
			break;

			// Kicking people who are here on network business is a no no.
			case AC_KICK:
				source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't kick "+dest->nick+" as they're on official network business.");
				return ACR_DENY;
			break;

			// Yes, they're immune to dehalfopping too.
			case AC_DEHALFOP:
				source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't de-halfop "+dest->nick+" as they're on official network business.");
				return ACR_DENY;
			break;

			// same with devoice.
			case AC_DEVOICE:
				source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't devoice "+dest->nick+" as they're on official network business.");
				return ACR_DENY;
			break;
		}

		// Some other access check that doesn't fall into the above. Let it through.
		return ACR_DEFAULT;
	}

	virtual ~ModuleOjoin()
	{
		ServerInstance->Modes->DelMode(np);
		delete np;
	}

	virtual Version GetVersion()
	{
		return Version("$Id: m_ojoin.cpp 1.0 - Taros $", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleOjoin)
