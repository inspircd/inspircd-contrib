/// $ModAuthor: linuxdaemon
/// $ModAuthorMail: linuxdaemonirc@gmail.com
/// $ModDepends: core 3.0
/// $ModDesc: Slowly disconnects idle users for maintenance
/// $ModConfig: <shedusers shedopers="no" kill="yes" shutdown="no" blockconnect="yes" minidle="3600" maxusers="0" message="This server has entered maintenance mode." blockmessage="This server is in maintenance mode.">

// Maintenance mode can be triggered with the /SHEDUSERS command
// as well as sending SIGUSR2 to the inspircd process

#include "inspircd.h"
#include "exitcodes.h"

inline unsigned long GetIdle(LocalUser* lu)
{
	return ServerInstance->Time() - lu->idle_lastmsg;
}

struct UserSorter
{
	bool operator()(LocalUser* a, LocalUser* b) const
	{
		// Order: NULL, NULL, InactiveUser, ActiveUser, MoreActiveUser
		if (a && b)
			if (a->idle_lastmsg != b->idle_lastmsg)
				return a->idle_lastmsg < b->idle_lastmsg;

		return a < b;
	}
};

static volatile sig_atomic_t active;
static volatile sig_atomic_t notified;

class CommandShed
	: public Command
{
 public:
	CommandShed(Module* me)
		: Command(me, "SHEDUSERS", 1, 1)
	{
		flags_needed = 'o';
		syntax = "<servermask>";
		allow_empty_last_param = false;
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0]))
		{
			active = 1;
			notified = 0;
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const CommandBase::Params& parameters) CXX11_OVERRIDE
	{
		return ROUTE_OPT_BCAST;
	}
};

class ModuleShedUsers
	: public Module
{
 public:
	typedef std::set<LocalUser*, UserSorter> NewUserList;

	static void sighandler(int)
	{
		active = 1;
		notified = 0;
	}

 private:
	CommandShed cmd;

	std::string message;
	std::string blockmessage;

	unsigned long maxusers;
	unsigned long minidle;

	bool shedopers;
	bool shutdown;
	bool blockconnects;
	bool kill;

 public:
	ModuleShedUsers()
		: cmd(this)
		, maxusers(0)
		, minidle(0)
		, shedopers(false)
		, shutdown(false)
		, blockconnects(false)
		, kill(false)
	{
	}

	void init() CXX11_OVERRIDE
	{
		notified = active = 0;
		signal(SIGUSR2, sighandler);
	}

	~ModuleShedUsers() CXX11_OVERRIDE
	{
		signal(SIGUSR2, SIG_DFL);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("shedusers");

		message = tag->getString("message", "This server has entered maintenance mode.");
		blockmessage = tag->getString("blockmessage", "This server is in maintenance mode.");
		maxusers = tag->getUInt("maxusers", 0);
		minidle = tag->getDuration("minidle", 60, 1);
		shedopers = tag->getBool("shedopers");
		shutdown = tag->getBool("shutdown");
		blockconnects = tag->getBool("blockconnect", true);
		kill = tag->getBool("kill", true);
	}

	bool CanShed(LocalUser* lu) const
	{
		if (!shedopers && lu->IsOper())
			return false;

		if (GetIdle(lu) < minidle)
			return false;

		return true;
	}

	void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE
	{
		if (active && blockconnects && user->registered != REG_ALL)
			ServerInstance->Users.QuitUser(user, blockmessage);
	}

	void GetSortedUsers(NewUserList& users) const
	{
		users.clear();
		UserManager::LocalList localusers = ServerInstance->Users.GetLocalUsers();
		for (UserManager::LocalList::iterator it = localusers.begin(); it != localusers.end(); ++it)
		{
			LocalUser* lu = *it;
			if (CanShed(lu))
				users.insert(lu);
		}
	}

	void OnBackgroundTimer(time_t) CXX11_OVERRIDE
	{
		if (!active)
			return;

		if (!notified)
		{
			ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, ServerInstance->Config->ServerName, message, MSG_NOTICE);
			ClientProtocol::Event msgevent(ServerInstance->GetRFCEvents().privmsg, msg);

			for (UserManager::LocalList::const_iterator i = ServerInstance->Users.GetLocalUsers().begin(); i != ServerInstance->Users.GetLocalUsers().end(); ++i)
			{
				LocalUser* user = *i;
				user->Send(msgevent);
			}
			notified = true;
		}

		if (ServerInstance->Users.LocalUserCount() <= maxusers)
		{
			if (shutdown)
				ServerInstance->Exit(EXIT_STATUS_NOERROR);

			active = 0;
			return;
		}

		if (!kill)
			return;

		NewUserList users;
		GetSortedUsers(users);

		NewUserList::iterator it = users.begin();
		if (it == users.end())
			return;

		LocalUser* lu = *it;
		users.erase(it);

		if (!lu)
			return;

		ServerInstance->Users.QuitUser(lu, message);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Slowly disconnects idle users for maintenance");
	}
};

MODULE_INIT(ModuleShedUsers)
