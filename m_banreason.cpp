#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Adds the abiliy to store the reason a user was banned from a channel */
/* $ModAuthor: Om */
/* $ModAuthorMail: om@inspircd.org */
/* $ModDepends: core 1.1 */
/* $ModVersion: $Rev: 78 $ */

/* Written by Om<om@inspircd.org>, April 2005. */
/* Originally based on m_exception, which was based on m_chanprotect and m_silence */

/*
 * This module stores a reason with every +b channel ban.
 * Instead of doing this by modifying the command, m_banreason detects the next kick after the ban is set,
 * and if the kicked user matches the ban host, then the kick reason is stored as the ban reason for that ban.
 */

class ModuleBanReason : public Module
{
	InspIRCd* Srv;
 public:
	ModuleBanReason(InspIRCd* serv)
	: Module::Module(serv), Srv(serv)
	{
	}
	
	void Implements(char* List)
	{
		List[I_OnUserKick] = List[I_On005Numeric] = List[I_OnRawMode] = List[I_OnChannelDelete] = List[I_OnCleanup] = List[I_OnSyncUserMetaData] = List[I_OnDecodeMetaData] = 1;
	}
	
	virtual void OnUserKick(userrec* kicker, userrec* user, chanrec* chan, const std::string &rsn)
	{
		BanList bl = chan->bans;
		if(bl.size() > 0)
		{
			BanItem ban = chan->bans.back();
			if(Srv->MatchText(user->GetFullHost(), ban.data) || Srv->MatchText(user->GetFullRealHost(), ban.data))
			{
				// If the host of the kicked user matches the last ban set...
				Srv->Log(DEBUG, "m_banreason.so: %s was kicked by %s, and they match the last ban set on the channel (%s)", user->nick, kicker->nick, chan->name);
				
				if(get_reason(chan, ban.data).empty())
				{
					// And there isn't a ban reason set for the last ban. 
					// If this was the second kick after a ban then the reason would already be set, and we wouldn't need to do anything here.
					// But it was, so we make an extension to the channel record, holding the ban reason.
					add_reason(chan, ban.data, rsn);
				}
			}
		}
	}
	
	virtual void On005Numeric(std::string &output)
	{
		output += " BANREASON";
	}
	
	virtual int OnRawMode(userrec* user, chanrec* chan, char mode, const std::string &param, bool adding, int pcnt)
	{
		if(mode == 'b')
		{
			if((pcnt > 0) && (!adding))
			{
				// Basically, if someone sets -b...
				remove_reason(chan, param);
			}
			else
			{
				// We're listing the bans...
				Srv->Log(DEBUG, "Listing ban reasons...");
	
				user->WriteServ("NOTICE %s :%s Listing ban reasons", user->nick, chan->name);
					
				for(BanList::iterator it = chan->bans.begin(); it != chan->bans.end(); it++)
				{
					std::string rsn = get_reason(chan, it->data);
					
					if(rsn.empty())
					{
						Srv->Log(DEBUG, "No ban reason set for ban %s not sending list item", it->data);
						continue;
					}
		
					user->WriteServ("NOTICE %s :%s %s %s", user->nick, chan->name, it->data, rsn.c_str());
						
					Srv->Log(DEBUG, "Sent reason (%s) for ban %s", rsn.c_str(), it->data);
				}
				
				user->WriteServ("NOTICE %s :%s End of channel ban reason list", user->nick, chan->name);
			}
		}
		return 0;
	}
	
	virtual void OnChannelDelete(chanrec* channel)
	{
		// For tidying up our ban reason lists when the last user leaves the channel...

		for (BanList::iterator it = channel->bans.begin(); it != channel->bans.end(); it++)
		{
			// For each one of the bans, try and remove the associated reason.
			remove_reason(channel, std::string(it->data));
		}
	}
	
	virtual void OnSyncChannelMetaData(chanrec* chan, Module* proto, void* opaque, const std::string &extname)
	{
		// check if the linking module wants to know about some of our metadata
		if(extname.substr(0, 11) == "ban_reason_")
		{
			std::string reason = get_reason(chan, extname.substr(11));
			
			if(reason.length())
			{
				// Should always be true, but check the data actually exists before we try and sync it.
				// And give it to the protocol module to send out across the network
				proto->ProtoSendMetaData(opaque, TYPE_CHANNEL, chan, extname, reason);
				Srv->Log(DEBUG, "m_banreason.so: Sent the reason (" + reason + ") for ban '" + extname.substr(11) + "' on channel '" + std::string(chan->name) + "' out");
			}
			else
			{
				Srv->Log(DEBUG, "m_banreason.so: Core asked us to sync some channel metadata (Key: " + extname + ") that doesn't exsist :/!");
			}
		}
	}

	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_CHANNEL) && (extname.substr(0, 11) == "ban_reason_"))
		{
			chanrec* chan = (chanrec*)target;
			std::string mask = extname.substr(11);
			
			if(get_reason(chan, mask).empty())
			{
				add_reason(chan, mask, extdata);
			}
		}
	}
	
	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_CHANNEL)
		{
			chanrec* chan = (chanrec*)item;
			
			for(BanList::iterator it = chan->bans.begin(); it != chan->bans.end(); it++)
			{
				remove_reason(chan, it->data);
			}
		}
	}
	
	virtual ~ModuleBanReason()
	{
		
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 2, 0, 3, 0, API_VERSION);// VF_STATIC);
	}
	
	void add_reason(chanrec* chan, const std::string &mask, const std::string &reason)
	{
		std::string* rs = new std::string(reason);
		chan->Extend("ban_reason_" + std::string(mask), rs);
		Srv->Log(DEBUG, "m_banreason.so: Extended %s record with ban reason %s for banmask %s", chan->name, reason.c_str(), mask.c_str());
	}
	
	std::string get_reason(chanrec* chan, const std::string &mask)
	{
		std::string* reason;
			
		if(chan->GetExt("ban_reason_" + mask, reason))
		{
			Srv->Log(DEBUG, "m_banreason.so: Retreived reason '" + *reason + "' for banmask '" + mask + "' on '" + std::string(chan->name) + "'");
			return *reason;
		}
		else
		{
			Srv->Log(DEBUG, "m_banreason.so: Failed to retrieve ban reason for mask '" + mask + "' on '" + std::string(chan->name) + "'");
			return "";
		}
	}
	
	void remove_reason(chanrec* chan, const std::string &mask)
	{
		std::string* reason;
					
		if(chan->GetExt("ban_reason_" + mask, reason))
		{					
			if(chan->Shrink("ban_reason_" + mask))
			{
				Srv->Log(DEBUG, "m_banreason.so: Ban reason (" + *reason + ") successfully removed on -b, banmask: " + mask);
				delete reason;
			}
			else
			{
				Srv->Log(DEBUG, "m_banreason.so: Failed to remove a ban reason which apparently existed. WTF?");
			}
		}
		else
		{
			Srv->Log(DEBUG, "m_banreason.so: No ban reason to remove for ban " + mask);
		}
	}
};


class ModuleBanReasonFactory : public ModuleFactory
{
 public:
	ModuleBanReasonFactory()
	{
	}
	
	~ModuleBanReasonFactory()
	{
	}
	
	virtual Module* CreateModule(InspIRCd* serv)
	{
		return new ModuleBanReason(serv);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleBanReasonFactory;
}
