/* Copyright (c) 2007, Special (special at inspircd org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the names of contributors may be
 *       used to endorse or promote products derived from this software without
 *       specific prior written permission.
 *
 * This software is provided ``as is'' and any express or implied warranties,
 * including, but not limited to, the implied warranties of merchantability
 * and fitness for a particular purpose are disclaimed. In no event shall the
 * copyright holder be liable for any direct, indirect, incidental, special,
 * exemplary, or consequential damages (including, but not limited to,
 * procurement of substitute goods or services; loss of use, data, or profits;
 * or business interruption) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "modes/cmode_b.h"

/* $ModDesc: Automatically synchronize bans between lists of channels */
/* $ModConfig: <syncbans channels="#a,#b,#c"> */
/* $ModAuthor: Special */
/* $ModAuthorMail: special@inspircd.org */
/* $ModDepends: core 1.1 */
/* $ModVersion: $Rev: 78 $ */

class ModuleSyncBans : public Module
{
 private:
	std::map<std::string,std::string*> ChannelList;
	bool doingprop;
	
 public:
	ModuleSyncBans(InspIRCd *Me)
		: Module::Module(Me), doingprop(false)
	{
		OnRehash(NULL, "");
	}
	
	virtual ~ModuleSyncBans()
	{
		for (std::map<std::string,std::string*>::iterator i = ChannelList.begin(); i != ChannelList.end(); i++)
			delete i->second;
		ChannelList.clear();		
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 11, 1, VF_COMMON, API_VERSION);
	}

	void Implements(char *List)
	{
		List[I_OnAddBan] = List[I_OnDelBan] = List[I_OnRehash] = 1;
	}
	
	virtual void OnRehash(userrec *user, const std::string &parameter)
	{
		ConfigReader MyConf(ServerInstance);
		
		for (std::map<std::string,std::string*>::iterator i = ChannelList.begin(); i != ChannelList.end(); i++)
			delete i->second;
		ChannelList.clear();
		
		for (int i = 0; i < MyConf.Enumerate("syncbans"); i++)
		{
			std::string *cset = new std::string(MyConf.ReadValue("syncbans", "channels", i));
			irc::sepstream sep(*cset, ',');
			std::string ch;
			while (sep.GetToken(ch))
			{
				if (ch.empty())
					continue;
				
				ChannelList.insert(std::make_pair(ch, cset));
			}
		}
	}
	
	void SyncBan(userrec *source, chanrec *channel, const std::string &banmask, bool adding, const std::string &channelset)
	{
		std::string banmaskc = banmask;
		
		ModeChannelBan *mh = (ModeChannelBan*) ServerInstance->Modes->FindMode('b', MODETYPE_CHANNEL);
		if (!mh)
			return;
		
		irc::sepstream sep(channelset, ',');
		std::string sch;
		doingprop = true;
		while (sep.GetToken(sch))
		{
			if (sch.empty() || sch == channel->name)
				continue;
			
			chanrec *schannel = ServerInstance->FindChan(sch);
			if (schannel)
			{
				if (adding)
					mh->AddBan(source, banmaskc, schannel, 0);
				else
					mh->DelBan(source, banmaskc, schannel, 0);
				
				// Handle server modes properly
				if (*source->nick)
					schannel->WriteChannel(source, "MODE %s %cb %s", schannel->name, (adding) ? '+' : '-', banmask.c_str());
				else
					schannel->WriteChannelWithServ(NULL, "MODE %s %cb %s", schannel->name, (adding) ? '+' : '-', banmask.c_str());
			}
		}
		doingprop = false;
	}
	
	virtual int OnAddBan(userrec *source, chanrec *channel, const std::string &banmask)
	{
		if (doingprop)
			return 0;
		
		std::map<std::string,std::string*>::iterator it = ChannelList.find(channel->name);
		if (it == ChannelList.end())
			return 0;
		
		SyncBan(source, channel, banmask, true, *it->second);
		
		return 0;
	}
	
	virtual int OnDelBan(userrec *source, chanrec *channel, const std::string &banmask)
	{
		if (doingprop)
			return 0;
		
		std::map<std::string,std::string*>::iterator it = ChannelList.find(channel->name);
		if (it == ChannelList.end())
			return 0;
		
		SyncBan(source, channel, banmask, false, *it->second);
		
		return 0;
	}
};

MODULE_INIT(ModuleSyncBans);

