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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides user mode +m, which converts uline notices to privmsg */
/* $ModAuthor: w00t */
/* $ModAuthorMail: w00t@inspircd.org */
/* $ModDepends: core 1.1 */
/* $ModVersion: $Rev: 78 $ */

/** Handles user mode +m
 */
class UserMessages : public ModeHandler
{
 public:
	UserMessages(InspIRCd* Instance) : ModeHandler(Instance, 'm', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (source != dest)
			return MODEACTION_DENY;

		if (adding)
		{
			if (!dest->IsModeSet('m'))
			{
				dest->SetMode('m',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('m'))
			{
				dest->SetMode('m',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleFoobar : public Module
{
 private:
	 UserMessages *m;
 public:
	ModuleFoobar(InspIRCd* Me) : Module(Me)
	{
		m = new UserMessages(ServerInstance);
		if (!ServerInstance->AddMode(m, 'm'))
			throw ModuleException("Could not add new modes!");
	}
	
	virtual ~ModuleFoobar()
	{
		ServerInstance->Modes->DelMode(m);
		delete m;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreNotice] = 1;
	}

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		userrec *target;

		// Since we rewrite the command as PRIVMSG ..we don't want to do this all the time.
		// Return if sender is not local.
		if (!IS_LOCAL(user))
		{
			return 0;
		}

		if (target_type != TYPE_USER)
		{
			return 0;
		}

		target = (userrec *)dest;

		// Check if sender is ULined, return if not
		if (!ServerInstance->ULine(user->server))
		{
			return 0;
		}

		// Check if target is +m, return if not
		if (!target->IsModeSet('m'))
		{
			return 0;
		}
		// Rewrite message as a PRIVMSG via command parser, drop event
		const char *parameters[2];
		parameters[0] = target->nick;
		parameters[1] = text.c_str();
		ServerInstance->Parser->CallHandler("PRIVMSG", parameters, 2, user);
		return 1;
	}
};


MODULE_INIT(ModuleFoobar)

