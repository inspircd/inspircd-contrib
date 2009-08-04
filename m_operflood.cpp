/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Removes flood limits from users upon opering up. */
/* $ModAuthor: ? */
/* $ModAuthorMailXXX:  */
/* $ModDepends: core 1.1 */
/* $ModVersion: $Rev: 78 $ */

class ModuleOperFlood : public Module
{
public:
	ModuleOperFlood(InspIRCd * Me) : Module(Me) {}

	void Implements(char * List)
	{
		List[I_OnPostOper] = 1;
	}

	Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	void OnPostOper(userrec* user, const std::string &opertype)
	{
		if(!IS_LOCAL(user))
			return;

		user->flood = 0;
		user->WriteServ("NOTICE %s :*** You are now free from flood limits.", user->nick);
	}
};

class ModuleOperFloodFactory : public ModuleFactory
{
public:
	Module * CreateModule(InspIRCd * Me)
	{
		return new ModuleOperFlood(Me);
	}
};

extern "C" DllExport void * init_module( void )
{
	return new ModuleOperFloodFactory;
}

