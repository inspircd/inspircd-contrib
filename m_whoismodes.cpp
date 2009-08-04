// m_whoismodes.cpp - ViaraiX
// Updated for new API - LeaChim
// Updated for even newer API - Om
// Updated for 1.1-beta5 - AnMaster

#include "inspircd.h"
#include "modules.h"

/* $ModDesc: Allows opers to see users modes on whois */
/* $ModAuthor: Om */
/* $ModAuthorMail: om@inspircd.org */
/* $ModDepends: core 1.1 */
/* $ModVersion: $Rev: 78 $ */

class ModuleWhoismodes : public Module
{
public:
	ModuleWhoismodes(InspIRCd* Me)
	: Module::Module(Me)
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,0,API_VERSION);
	}
	
	virtual void Implements(char* List)
	{
		List[I_OnWhois] = 1;
	}

	virtual void OnWhois(userrec* source, userrec* dest)
	{
		if(*source->oper) {
			if (dest->modes[UM_SNOMASK] != 0) {
				ServerInstance->SendWhoisLine(source, dest, 379, "%s %s :usermodes [+%s +%s]",
				                              source->nick, dest->nick, dest->FormatModes(),
				                              dest->FormatNoticeMasks());
			} else {
				ServerInstance->SendWhoisLine(source, dest, 379, "%s %s :usermodes [+%s]",
				                              source->nick, dest->nick, dest->FormatModes());
			}
		}
	}
};

MODULE_INIT(ModuleWhoismodes)

