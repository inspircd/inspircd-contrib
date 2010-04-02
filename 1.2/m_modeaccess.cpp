/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2008 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *                <omster@gmail.com>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include "inspircd.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Allows fine configuration of what users can set arbitrary channel modes, based on usermodes, channel modes and prefixes. */
/* $ModAuthor: Om */
/* $ModAuthorMail: om@inspircd.org */
/* $ModDepends: core 1.2 */

typedef std::pair<char, bool> CharPair;
typedef std::vector<CharPair> CharPairList;

bool cmp(CharPair a, CharPair b)
{
	return a.second > b.second;
}


class OnOff
{
	private:
		void MakeStrRep()
		{
			bool doneplus;
			bool doneminus;

			doneplus = doneminus = false;

			for(CharPairList::iterator i = modes.begin(); i != modes.end(); i++)
			{
				if((i->second == 1) && !doneplus)
				{
					doneplus = true;
					strrep.append("+");
				}

				if((i->second == 0) && !doneminus)
				{
					doneminus = true;
					strrep.append("-");
				}

				strrep.append(1, i->first);
			}
		}

	public:
		std::string strrep;
		CharPairList modes;
		short modecount;

		OnOff(const std::string &combined)
			: modecount(0)
		{
			bool adding = true;

			for(std::string::const_iterator i = combined.begin(); i != combined.end(); i++)
			{
				switch(*i)
				{
					case '+':
						adding = true;
						break;
					case '-':
						adding = false;
						break;
					default:
						modes.push_back(std::make_pair(*i, adding));
						modecount++;
						break;
				}
			}

			sort(modes.begin(), modes.end(), cmp);

			MakeStrRep();
		}

		OnOff(const std::string &on, const std::string &off)
			: modecount(0)
		{
			for(std::string::const_iterator i = on.begin(); i != on.end(); i++)
			{
				modes.push_back(std::make_pair(*i, 1));
				modecount++;
			}

			for(std::string::const_iterator i = off.begin(); i != off.end(); i++)
			{
				modes.push_back(std::make_pair(*i, 0));
				modecount++;
			}

			MakeStrRep();
		}

		const char* str()
		{
			return strrep.c_str();
		}
};

class Prefix
{
	public:
		InspIRCd* Srv;
		std::string prefixstr;
		unsigned int lowestrequired;
		char lowestprefixrequired;

		Prefix(InspIRCd* me, const std::string &pstr)
			: Srv(me), prefixstr(pstr), lowestrequired(0), lowestprefixrequired(' ')
		{
			bool haveparsedrealprefix = false;

			/* First find what the lowest prefix level required is */
			for(std::string::const_iterator i = pstr.begin(); i != pstr.end(); i++)
			{
				ModeHandler* handler = Srv->Modes->FindPrefix(*i);

				if(handler)
				{
					unsigned int newlevel = handler->GetPrefixRank();

					if(!haveparsedrealprefix || (newlevel < lowestrequired))
					{
						lowestrequired = newlevel;
						lowestprefixrequired = *i;
					}
				}
			}
		}

		/** bool UserQualifies(user, channel)
		 * Return true if the user has the prefixes stored in this class on the channel
		 * passed as an argument, false otherwise.
		 */
		bool UserQualifies(User* user, Channel* chan)
		{
			if(chan->GetPrefixValue(user) >= lowestrequired)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
};

class PermInfo
{
	public:
		char mode;
		OnOff umodes;
		OnOff cmodes;
		Prefix prefix;

		PermInfo(InspIRCd* me, char m, const std::string &u, const std::string &c, const std::string &p) : mode(m), umodes(u), cmodes(c), prefix(me, p) { }
};

typedef std::vector<PermInfo> PermSet;
typedef std::vector<PermSet> PermissionList;

class ModuleModeAccess : public Module
{
	private:
		InspIRCd *Srv;
		PermissionList perms;

	public:
		ModuleModeAccess(InspIRCd* Me)
			: Module::Module(Me), Srv(Me)
		{
			OnRehash(NULL);

			Implementation list[] = { I_On005Numeric, I_OnRawMode, I_OnRehash };
			Me->Modules->Attach(list, this, 3);
		}

		/**
		 * Load the configuration for the priveleges required to use a certain mode.
		 * If a more than one tag is defined for a mode, then the user only needs
		 * to have *one* of those priveleges.
		 * However if more than one privelege is defined inside one tag, all those
		 * requirements much be met.
		 * Prefixes are considered as levels, so if a user has ~ they are considered
		 * to have &@%+ as well etc.
		 * If no requirements are set for a mode it is allowed for prefix % and above (default behaviour) Ex:
		 * <access modes="P" cmodes="s"> - +P may only be set on channels which are +s
		 * <access modes="c" umodes="h"> - Only users with usermode +h set can set +c
		 * <access modes="c" umodes="+h"> - Same as above
		 * <access modes="c" umodes="-h"> - Only users without usermode +h set can set +c
		 * <access modes="c" prefix="@"> - Only users with prefix @ on a channel can set +c on that channel
		 * <access modes="S" prefix="~" umodes="o"> - Only opers who are have the ~ prefix on a channel can set +S on the channel
		 * <access modes="A" cmodes="z"><access modes="A" umodes="z"> - +A can be set by a user with umode +z, or by anyone if the channel has cmode +z
		 */
		virtual void OnRehash(User* user)
		{
			ConfigReader Conf(Srv);

			perms.clear();
			perms.assign(127, PermSet());

			for(int i = 0; i < Conf.Enumerate("access"); i++)
			{
				std::string rmodes = Conf.ReadValue("access", "modes", i);
				std::string umodes = Conf.ReadValue("access", "umodes", i);
				std::string cmodes = Conf.ReadValue("access", "cmodes", i);
				std::string prefix = Conf.ReadValue("access", "prefix", i);

				for(std::string::iterator j = rmodes.begin(); j != rmodes.end(); j++)
				{
					/* Add a structure for permissions for each mode in the tag. */
					perms[ *j ].push_back(PermInfo(Srv, *j, umodes, cmodes, prefix));
					Srv->Logs->Log("m_modeaccess.so",DEBUG, "m_modeaccess.so: Adding mode %c, requires umodes '%s', prefix '%s', and cmodes '%s'", *j, umodes.c_str(), prefix.c_str(), cmodes.c_str());
				}
			}
		}

		virtual void On005Numeric(std::string &output)
		{
			/* Have fun designing some ludicrously complicated format here later... */
			/* ALLOWEDMODES=MODE:UMODES:CMODES:PREFIXES */

			/* Make a copy of this. Then we load in the modes from the core/other modules
			 * into the copy and let the code below generate the 005. */
			PermissionList tempperms = perms;
			std::string umodes = Srv->Modes->ChannelModeList();

			for(std::string::iterator i = umodes.begin(); i != umodes.end(); i++)
			{
				char& mchar = *i;

				if((mchar != ',') && (tempperms[mchar].empty()))
				{
					ModeHandler* handler = Srv->Modes->FindMode(mchar, MODETYPE_CHANNEL);

					if(handler)
					{
						tempperms[mchar].push_back(PermInfo(Srv, mchar, handler->NeedsOper() ? "+o" : "", "", std::string(1, handler->GetNeededPrefix())));
					}
				}
			}

			std::string ourstring(" ALLOWEDMODES=");
			bool addedamode = false;

			for(PermissionList::iterator i = tempperms.begin(); i != tempperms.end(); i++)
			{
				PermSet& modeperms = *i;

				for(PermSet::iterator k = modeperms.begin(); k != modeperms.end(); k++)
				{
					PermInfo& pinfo = *k;

					ourstring.append(1, pinfo.mode).append(1, ':').append(pinfo.umodes.strrep).append(1, ':');
					ourstring.append(pinfo.cmodes.strrep).append(1, ':').append(pinfo.prefix.prefixstr).append(1, ',');
					addedamode = true;
				}
			}

			if(addedamode)
			{
				ourstring.resize(ourstring.size() - 1);
				output.append(ourstring);
			}
		}

        virtual int OnRawMode(User* source, Channel* channel, const char modechar, const std::string &param, bool adding, int pcnt, bool servermode)
			{
			/* We're only concerned with channel modes */
			if(!channel)
				return ACR_DEFAULT;

			Srv->Logs->Log("m_modeaccess.so",DEBUG, "OnRawMode(%s, %s, %c, %s, %s, %d)", source->nick.c_str(), channel->name.c_str(), modechar, param.c_str(), adding ? "true" : "false", pcnt);

			/* We don't care about remote users, this module on the remote server handles them */
			if(!IS_LOCAL(source))
				return ACR_DEFAULT;

			PermSet& criteria = perms[modechar];
			char error[MAXBUF];
			*error = '\0';

			/* Search the rules for this mode, now we iterate over them until we find one that matches */
			for(PermSet::iterator iter = criteria.begin(); iter != criteria.end(); iter++)
			{
				Srv->Logs->Log("m_modeaccess.so",DEBUG, "Checking over rules for mode %c, possible: umodes='%s', prefix='%s', cmodes='%s'", modechar, iter->umodes.str(), iter->prefix.prefixstr.c_str(), iter->cmodes.str());

				/* If the user trying to set the mode matches the criteria for setting it, allow it by returning true */
				if(this->HasModes(channel, iter->cmodes))
				{
					if(this->HasModes(source, iter->umodes))
					{
						if(iter->prefix.UserQualifies(source, channel))
						{
							return ACR_ALLOW;
						}
						else
						{
							snprintf(error, MAXBUF, "482 %s %s :You must have channel privilege %c or above to %sset channel mode %c", source->nick.c_str(), channel->name.c_str(), iter->prefix.lowestprefixrequired, adding ? "" : "un", modechar);
						}
					}
					else
					{
						snprintf(error, MAXBUF, "482 %s %s :You must have user mode%s %s to %sset channel mode %c", source->nick.c_str(), channel->name.c_str(), (iter->umodes.modecount > 1) ? "s" : "",  iter->umodes.str(), adding ? "" : "un", modechar);
					}
				}
				else
				{
					snprintf(error, MAXBUF, "482 %s %s :Channel mode%s %s must to set to %sset channel mode %c", source->nick.c_str(), channel->name.c_str(), (iter->cmodes.modecount > 1) ? "s" : "", iter->cmodes.str(), adding ? "" : "un", modechar);
				}
			}

			if(*error)
			{
				source->WriteServ("%s",error);
				return ACR_DENY;
			}
			else
			{
				return ACR_DEFAULT;
			}
		}

		/** HasModes(user, modes)
		 * Return true if user has all the usermodes in 'modes' set.
		 * Return false otherwise.
		 */
		bool HasModes(User* user, const OnOff &modes)
		{
			for(CharPairList::const_iterator i = modes.modes.begin(); i != modes.modes.end(); i++)
			{
				if(user->IsModeSet(i->first) == i->second)
				{
					Srv->Logs->Log("m_modeaccess.so",DEBUG, "Mode %c was %s on %s as configured", i->first, i->second ? "set" : "not set", user->nick.c_str());
				}
				else
				{
					Srv->Logs->Log("m_modeaccess.so",DEBUG, "Mode %c was %s on %s, not allowed by configuration", i->first, i->second ? "set" : "not set", user->nick.c_str());
					return false;
				}
			}

			return true;
		}

		/** HasModes(chan, modes)
		 * Return true if all the chanmodes in 'modes' are set on 'chan'
		 * Return false otherwise.
		 */
		bool HasModes(Channel* chan, const OnOff &modes)
		{
			for(CharPairList::const_iterator i = modes.modes.begin(); i != modes.modes.end(); i++)
			{
				if(chan->IsModeSet(i->first) == i->second)
				{
					Srv->Logs->Log("m_modeaccess.so",DEBUG, "Mode %c was %s on %s as configured", i->first, i->second ? "set" : "not set", chan->name.c_str());
				}
				else
				{
					Srv->Logs->Log("m_modeaccess.so",DEBUG, "Mode %c was %s on %s, not allowed by configuration", i->first, i->second ? "set" : "not set", chan->name.c_str());
					return false;
				}
			}

			return true;
		}

		virtual ~ModuleModeAccess()
		{
		}

		virtual Version GetVersion()
		{
			return Version("$Id$",0,API_VERSION);
		}
};

MODULE_INIT(ModuleModeAccess)
