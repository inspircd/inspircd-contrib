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

/* $ModDesc: Allows modes to be customised for channel creation. e.g. <modeoncreate privs="@%"> would set +oh on a creating user. Also allows <modeoncreate affectsoper="true"> if you want opers affected too. */
/* $ModAuthor: w00t */
/* $ModAuthorMail: w00t@inspircd.org */
/* $ModDepends: core 1.2 */


class ModuleModeOnCreate : public Module
{
 private:
	std::string privstogive;
	bool affectsoper;

 public:
	ModuleModeOnCreate(InspIRCd* Me)
		: Module(Me)
	{
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		OnRehash(NULL);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf(ServerInstance);
		privstogive = Conf.ReadValue("modeoncreate", "privs", "@", 0, false);
		affectsoper = Conf.ReadFlag("modeoncreate", "affectsoper", 0);
	}


	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
			return 0;

		if (!affectsoper && IS_OPER(user))
			return 0;

		privs = this->privstogive;
		return 0;
	}

	virtual ~ModuleModeOnCreate()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", 0, API_VERSION);
	}
};

MODULE_INIT(ModuleModeOnCreate)

