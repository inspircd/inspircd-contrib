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

/* $ModDesc: Implements a filter for channel names */
/* $ModAuthor: danieldg */
/* $ModDepends: core 1.2-1.3 */

class ModuleChannelNames: public Module
{
 private:
 public:
	ModuleChannelNames(InspIRCd* Me) : Module(Me)
	{
		ServerInstance->Modules->Attach(I_OnUserPreJoin, this);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON, API_VERSION);
	}

	int OnUserPreJoin(User* user, Channel* chan, const char* name, std::string& privs, const std::string& key)
	{
		if (chan)
			return 0;
		const char* walk = name;
		while (*walk)
		{
			char c = *walk++;
			if (c <= 0x20 || c >= 0x7F)
			{
				user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :Cannot join channel (invalid name)", user->nick.c_str(), name);
				return 1;
			}
		}
		return 0;
	}
};


MODULE_INIT(ModuleChannelNames)


