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

/* $ModDesc: Creates a snomask with notices for channel creation, join, part, and kick */
/* $ModAuthor: cytrix */
/* $ModDepends: core 1.2 */

class ModuleSeeChan : public Module
{
 public:
	ModuleSeeChan(InspIRCd* Me)
		: Module(Me)
	{
		ServerInstance->SNO->EnableSnomask('j', "CHANNEL");
		ServerInstance->SNO->EnableSnomask('J', "REMOTECHANNEL");

		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPart, I_OnUserKick };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}
	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent, bool created);
	virtual void OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent);
	virtual void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent);

	virtual ~ModuleSeeChan()
	{
		ServerInstance->SNO->DisableSnomask('j');
		ServerInstance->SNO->DisableSnomask('J');
	}
	virtual Version GetVersion()
	{
		return Version("$Id$",0,API_VERSION);
	}
};

void ModuleSeeChan::OnUserJoin(User* user, Channel* channel, bool sync, bool &silent, bool created)
{
	silent = false;
	if (!created)
		ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'j' : 'J',"%s!%s@%s has joined to %s",user->nick.c_str(), user->ident.c_str(), user->host.c_str(),channel->name.c_str());
	else
		ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'j' : 'J',"%s!%s@%s has created %s",user->nick.c_str(), user->ident.c_str(), user->host.c_str(),channel->name.c_str());
}

void ModuleSeeChan::OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent)
{
	silent = false;
	ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'j' : 'J',"%s!%s@%s has parted from %s",user->nick.c_str(), user->ident.c_str(), user->host.c_str(),channel->name.c_str());
}

void ModuleSeeChan::OnUserKick(User* source, User* user, Channel* channel, const std::string &reason, bool &silent)
{
	silent = false;
	ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'j' : 'J',"%s!%s@%s got kicked from %s",user->nick.c_str(), user->ident.c_str(), user->host.c_str(),channel->name.c_str());   
}  

MODULE_INIT(ModuleSeeChan)
