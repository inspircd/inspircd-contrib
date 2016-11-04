/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Adam <Adam@anope.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/* $ModAuthor: Adam */
/* $ModAuthorMail: Adam@anope.org */
/* $ModDesc: Implements CAP chghost */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
#include "m_cap.h"

class ModuleCapChghost : public Module
{
	GenericCap cap;

	/* this taken from m_ircv3.so */
	void WriteNeighboursWithExt(User* user, const std::string& line, const LocalIntExt& ext)
	{
		UserChanList chans(user->chans);

		std::map<User*, bool> exceptions;
		FOREACH_MOD(I_OnBuildNeighborList, OnBuildNeighborList(user, chans, exceptions));

		// Send it to all local users who were explicitly marked as neighbours by modules and have the required ext
		for (std::map<User*, bool>::const_iterator i = exceptions.begin(); i != exceptions.end(); ++i)
		{
			LocalUser* u = IS_LOCAL(i->first);
			if ((u) && (i->second) && (ext.get(u)))
				u->Write(line);
		}

		// Now consider sending it to all other users who has at least a common channel with the user
		std::set<User*> already_sent;
		for (UCListIter i = chans.begin(); i != chans.end(); ++i)
		{
			const UserMembList* userlist = (*i)->GetUsers();
			for (UserMembList::const_iterator m = userlist->begin(); m != userlist->end(); ++m)
			{
				/*
				 * Send the line if the channel member in question meets all of the following criteria:
				 * - local
				 * - not the user who is doing the action (i.e. whose channels we're iterating)
				 * - has the given extension
				 * - not on the except list built by modules
				 * - we haven't sent the line to the member yet
				 *
				 */
				LocalUser* member = IS_LOCAL(m->first);
				if ((member) && (member != user) && (ext.get(member)) && (exceptions.find(member) == exceptions.end()) && (already_sent.insert(member).second))
					member->Write(line);
			}
		}
	}

 public:
	ModuleCapChghost()
		: cap(this, "chghost")
	{
	}

	void init()
	{
		Implementation eventList[] = { I_OnEvent, I_OnChangeHost, I_OnChangeIdent };
		ServerInstance->Modules->Attach(eventList, this, sizeof(eventList)/sizeof(Implementation));
	}

	void OnEvent(Event& event)
	{
		cap.HandleEvent(event);
	}

	void OnChangeHost(User* user, const std::string &newhost)
	{
		WriteNeighboursWithExt(user, ":" + user->GetFullHost() + " CHGHOST " + user->ident + " " + newhost, cap.ext);
	}

	void OnChangeIdent(User* user, const std::string &ident)
	{
		WriteNeighboursWithExt(user, ":" + user->GetFullHost() + " CHGHOST " + ident + " " + user->dhost, cap.ext);
	}

	Version GetVersion()
	{
		return Version("Implements CAP chghost");
	}
};

MODULE_INIT(ModuleCapChghost)
