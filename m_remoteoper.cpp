/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: A module which shows remote /oper up as REMOTEOPER to snomasks */
/* $ModAuthor: psychon */
/* $ModAuthorMail: psychon@inspircd.org */
/* $ModDepends: core 1.2 */
/* $ModVersion: $Rev: 78 $ */

class ModuleRemoteOper : public Module
{
 private:

 public:
	ModuleRemoteOper(InspIRCd* Me) : Module(Me)
	{

		Implementation eventlist[] = { I_OnSendSnotice };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModuleRemoteOper()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", 0, API_VERSION);
	}

	virtual int OnSendSnotice(char &snomask, std::string &type, const std::string &message)
	{
		/* Is this a oper notice? */
		if (snomask != 'o')
			return 0;

		/* Is it from a remote server? */
		if (message.substr(0, 5) != "From ")
			return 0;

		/* Make it REMOTEOPER then */
		type = "REMOTEOPER";
		return 0;
	}
};


MODULE_INIT(ModuleRemoteOper)
