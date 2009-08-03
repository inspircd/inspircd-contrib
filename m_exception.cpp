#include <stdio.h>
#include <string>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "mode.h"
#include "u_listmode.h"

/* $ModDesc: Provides support for the +x channel mode */
/* $ModAuthor: Om */
/* $ModAuthorMail: om@inspircd.org */
/* $ModDepends: core 1.1 */
/* $ModVersion: $Rev: 78 $ */

/* Written by Om <om@inspircd.org>, April 2005. */
/* Rewritten by Om, December 2005 */
/* Originally based on m_chanprotect and m_silence */

// The +x channel mode takes a nick!ident@host, glob patterns allowed,
// and if a user matches an entry on the +x list then they can join the channel, overriding _all_ other restrictions (+k, +i, +b, +whatever)

class GeneralException : public ListModeBase
{
 public:
	GeneralException(InspIRCd* serv) : ListModeBase(serv, 'x', "End of General Exception List", "446", "447", true) { }
};

class ModuleGeneralException : public Module
{
	GeneralException* be;
	InspIRCd* Srv;

public:
	ModuleGeneralException(InspIRCd* serv)
	: Module::Module(serv), Srv(serv)
	{
		be = new GeneralException(serv);
		Srv->AddMode(be, 'x');
	}
	
	virtual void Implements(char* List)
	{
		be->DoImplements(List);
		List[I_On005Numeric] = List[I_OnUserPreJoin] = 1;
	}
	
	virtual void On005Numeric(std::string &output)
	{
		output.append(" EX=x");
	}

	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname, std::string &privs)
	{
		if(chan != NULL)
		{
			modelist* list;
			
			if(chan->GetExt(be->GetInfoKey(), list))
				for(modelist::iterator it = list->begin(); it != list->end(); it++)
					if (match(user->GetFullRealHost(), it->mask.c_str()) || match(user->GetFullHost(), it->mask.c_str()))
						// They match an entry on the list, so let them in.
						return -1;
			// or if there wasn't a list, there can't be anyone on it, so we don't need to do anything.
		}
		return 0;
	}
	
	virtual void OnCleanup(int target_type, void* item)
	{
		be->DoCleanup(target_type, item);
	}

	virtual void OnSyncChannel(chanrec* chan, Module* proto, void* opaque)
	{
		be->DoSyncChannel(chan, proto, opaque);
	}

	virtual void OnChannelDelete(chanrec* chan)
	{
		be->DoChannelDelete(chan);
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		be->DoRehash();
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 4, VF_STATIC|VF_COMMON, API_VERSION);
	}
	
	virtual ~ModuleGeneralException()
	{
		DELETE(be);	
	}
};

MODULE_INIT(ModuleGeneralException)
