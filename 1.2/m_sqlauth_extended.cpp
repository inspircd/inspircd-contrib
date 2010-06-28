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
#include "m_sqlv2.h"
#include "m_sqlutils.h"
#include "m_hash.h"
#include "commands/cmd_privmsg.h"

/* $ModDesc: Allow/Deny connections based upon an arbitary SQL table with extended options. */
/* $ModDep: m_sqlv2.h m_sqlutils.h m_hash.h */

/* Original source from InspIRCd 1.2 modified by Bawitdaba on December 23rd 2008 */
/* Derived from m_sqlauth.cpp rev 10622 by brain */

class ModuleSQLAuth : public Module {
	Module* SQLutils;
	Module* SQLprovider;
	Module* m_customtitle;

	std::string freeformquery;
	std::string killreason;
	std::string killreasonUHost;
	std::string allowpattern;
	std::string databaseid;
	std::string successquery;
	std::string failurequery;
	bool ghosting;
	bool verbose;
	bool setaccount;
	bool servicesident;

public:
	ModuleSQLAuth(InspIRCd* Me)
	: Module(Me) {
		ServerInstance->Modules->UseInterface("SQLutils");
		ServerInstance->Modules->UseInterface("SQL");

		SQLutils = ServerInstance->Modules->Find("m_sqlutils.so");
		if (!SQLutils)
			throw ModuleException("Can't find m_sqlutils.so. Please load m_sqlutils.so before m_sqlauth_extended.so.");

		SQLprovider = ServerInstance->Modules->FindFeature("SQL");
		if (!SQLprovider)
			throw ModuleException("Can't find an SQL provider module. Please load one before attempting to load m_sqlauth_extended.so.");

		m_customtitle = ServerInstance->Modules->Find("m_customtitle.so");
		/*
		Don't force users to have this module compiled, this module can function without it
		if (!m_customtitle)
			throw ModuleException("Can't find m_customtitle.so. Please load m_customtitle.so before m_sqlauth_extended.so.");
		*/

		OnRehash(NULL,"");
		Implementation eventlist[] = { I_OnPostConnect, I_OnPreCommand, I_OnUserConnect, I_OnUserDisconnect, I_OnCheckReady, I_OnRequest, I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 8);

	}

	virtual ~ModuleSQLAuth() {
		ServerInstance->Modules->DoneWithInterface("SQL");
		ServerInstance->Modules->DoneWithInterface("SQLutils");
	}

	/* Function for matching ident/hostmasks */
	bool OneOfMatches(const char* host, const char* ip, const char* hostlist) {
		std::stringstream hl(hostlist);
		std::string xhost;
		while (hl >> xhost) {
			if (InspIRCd::Match(host, xhost, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(ip, xhost, ascii_case_insensitive_map)) {
				return true;
			}
		}
		return false;
	}

	/* IRCd has been rehased, reload config vars */
	virtual void OnRehash(User* user, const std::string &parameter) {
		ConfigReader Conf(ServerInstance);

		databaseid		= Conf.ReadValue("sqlauth_extended", "dbid", 0);				/* Database ID, given to the SQL service provider */
		freeformquery	= Conf.ReadValue("sqlauth_extended", "query", 0);				/* Field name where username can be found */
		successquery	= Conf.ReadValue("sqlauth_extended", "successquery", 0);		/* Query to run when a user is registerd and authed ok */
		failurequery	= Conf.ReadValue("sqlauth_extended", "failurequery", 0);		/* Query to run when a user enters wrong username or password */
		killreason		= Conf.ReadValue("sqlauth_extended", "killreason", 0);			/* Kill Reason to give when access is denied to a user (put your reg details here) */
		killreasonUHost	= Conf.ReadValue("sqlauth_extended", "killreasonuhost", 0);		/* Kill Reason to give when user doesn't match allowed user@hostname in SQL $allowedident $allowedhost replaceable */
		allowpattern	= Conf.ReadValue("sqlauth_extended", "allowpattern",0 );		/* Allow nicks matching this pattern without requiring auth */
		verbose			= Conf.ReadFlag("sqlauth_extended", "verbose", 0);				/* Set to true if failed connects should be reported to operators */		
		ghosting		= Conf.ReadFlag("sqlauth_extended", "ghosting", 0);				/* Set to true to kill connected users with same nick as connecting user */		
		setaccount		= Conf.ReadFlag("sqlauth_extended", "setaccount", 0);			/* Set account name for m_services_account */		
		servicesident	= Conf.ReadFlag("sqlauth_extended", "servicesident", 0);		/* Auto identify to NickServ (Anope/Atheme) */		

	}

	/* @return 1 to block the command, 0 to allow */
	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line) {
		if (!validated || !ghosting) { return 0; }

		if (command == "NICK") {
			if (user->registered != REG_ALL) {
				/* Nick in use, ghosting is on if we reach here so process */
				if (ServerInstance->FindNickOnly(parameters[0])) {
					/* We just let the user keep his UUID nick and fake further
					 * down that the initial NICK was ok. For now just store
					 * their wanted nick.
					 */
					std::string* authnick = new std::string(parameters[0]);
					user->Extend("wantsnick", authnick);
					/* since we cheat here, make them look as if they have passed
					 * REG_NICK checks. HACK warning! Well its all hack this bit :)
					 */
					user->registered = (user->registered | REG_NICK);

					/* if NICK is sent after USER thus making the user fully NICKUSER
					 * regged, we better trigger OnUserRegister since cmd_nick would
					 * normally do this. Since we handle NICK we got to check and do it.
					 */
					if (user->registered == REG_NICKUSER) {
						int MOD_RESULT = 0;
						FOREACH_RESULT(I_OnUserRegister,OnUserRegister(user));
						if (MOD_RESULT > 0) { return 1; }
					}

					/* Dont let inspircd process the command, we just did */
					return 1;
				}
			}
		}
		return 0;
	}

	/* User is attempting to connect to IRCd */
	virtual int OnUserRegister(User* user) {
		bool checkResult;

		std::string* wnick;
		if (user->GetExt("wantsnick", wnick)) {
			/* Override Allowpattern */
			if ((!allowpattern.empty()) && (InspIRCd::Match(wnick->c_str(),allowpattern))) {
				user->Extend("sqlauthed");
				return 0;
			}
		} else {
			/* Override Allowpattern */
			if ((!allowpattern.empty()) && (InspIRCd::Match(user->nick,allowpattern))) {
				user->Extend("sqlauthed");
				return 0;
			}
		}

		checkResult = CheckCredentials(user);

		if (!checkResult) { return 1; }

		return 0;
	}

	/* Find and Replace function */
	void SearchAndReplace(std::string& newline, const std::string &find, const std::string &replace) {
		std::string::size_type x = newline.find(find);
		while (x != std::string::npos) {
			newline.erase(x, find.length());
			if (!replace.empty())
				newline.insert(x, replace);
			x = newline.find(find);
		}
	}

	/* Auth Function, builds SQL query for connecting user */
	bool CheckCredentials(User* user) {
		std::string thisquery = freeformquery;
		std::string safepass = user->password;

		/* Search and replace the escaped nick and escaped pass into the query */
		SearchAndReplace(safepass, "\"", "");
		std::string* wnick;
		if (user->GetExt("wantsnick", wnick)) {
			SearchAndReplace(thisquery, "$nick", *wnick);
		} else {
			SearchAndReplace(thisquery, "$nick", user->nick);
		}

		SearchAndReplace(thisquery, "$pass", safepass);
		SearchAndReplace(thisquery, "$host", user->host);
		SearchAndReplace(thisquery, "$ip", user->GetIPString());
		
		Module* HashMod = ServerInstance->Modules->Find("m_md5.so");

		if (HashMod) {
			HashResetRequest(this, HashMod).Send();
			SearchAndReplace(thisquery, "$md5pass", HashSumRequest(this, HashMod, user->password).Send());
		}

		HashMod = ServerInstance->Modules->Find("m_sha256.so");

		if (HashMod) {
			HashResetRequest(this, HashMod).Send();
			SearchAndReplace(thisquery, "$sha256pass", HashSumRequest(this, HashMod, user->password).Send());
		}

		/* Build the query */
		SQLrequest req = SQLrequest(this, SQLprovider, databaseid, SQLquery(thisquery));

		if(req.Send()) {
			/* When we get the query response from the service provider we will be given an ID to play with,
			 * just an ID number which is unique to this query. We need a way of associating that ID with a User
			 * so we insert it into a map mapping the IDs to users.
			 * Thankfully m_sqlutils provides this, it will associate a ID with a user or channel, and if the user quits it removes the
			 * association. This means that if the user quits during a query we will just get a failed lookup from m_sqlutils - telling
			 * us to discard the query.
		 	 */
			AssociateUser(this, SQLutils, req.id, user).Send();

			return true;
		} else {
			if (verbose) {
				ServerInstance->SNO->WriteToSnoMask('A', "Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), req.error.Str());
			}
			return false;
		}
	}

	/* SQL Request */
	virtual const char* OnRequest(Request* request) {
		if(strcmp(SQLRESID, request->GetId()) == 0) {
			SQLresult* res = static_cast<SQLresult*>(request);

			User* user = GetAssocUser(this, SQLutils, res->id).S().user;
			UnAssociate(this, SQLutils, res->id).S();

			if(user) {
				if(res->error.Id() == SQL_NO_ERROR) {
					std::string* wnick;
					bool result;

					if(res->Rows()) {
						int rowcount=res->Rows(),i;

						/* Clean Custom User Metadata */
						user->Shrink("sqlAllowedIdent");
						user->Shrink("sqlAllowedHost");
						user->Shrink("sqlvHost");
						user->Shrink("sqlTitle");
						user->Shrink("sqlumodes");

						std::string sqlvHost;
						std::string sqlTitle;
						std::string sqlumodes;
						std::string sqlAllowedIdent;
						std::string sqlAllowedHost;

						/* Get Data from SQL (using freeform query. "query" in modules.conf) */
						for (i=0; i<rowcount; ++i) {
							SQLfieldList& currow = res->GetRow();
							sqlAllowedIdent = currow[1].d.c_str();
							sqlAllowedHost = currow[2].d.c_str();
							sqlvHost = currow[3].d.c_str();
							sqlTitle = currow[4].d.c_str();
							sqlumodes = currow[5].d.c_str();
						}

						std::string* pAllowedIdent = new std::string(sqlAllowedIdent);
						std::string* pAllowedHost = new std::string(sqlAllowedHost);
						std::string* pvHost = new std::string(sqlvHost);
						std::string* pTitle = new std::string(sqlTitle);
						std::string* pumodes = new std::string(sqlumodes);

						user->Extend("sqlAllowedIdent",pAllowedIdent);
						user->Extend("sqlAllowedHost",pAllowedHost);
						user->Extend("sqlvHost",pvHost);
						user->Extend("sqlTitle",pTitle);
						user->Extend("sqlumodes",pumodes);

						/* Check Allowed Ident@Hostname from SQL */
						if (sqlAllowedIdent != "" && sqlAllowedHost != "") {
							char TheHost[MAXBUF];
							char TheIP[MAXBUF];
							char TheAllowedUHost[MAXBUF];

							snprintf(TheHost,MAXBUF,"%s@%s",user->ident.c_str(), user->host.c_str());
							snprintf(TheIP, MAXBUF,"%s@%s",user->ident.c_str(), user->GetIPString());
							snprintf(TheAllowedUHost, MAXBUF, "%s@%s", sqlAllowedIdent.c_str(), sqlAllowedHost.c_str());

							if (!OneOfMatches(TheHost,TheIP,TheAllowedUHost)) {
								if (killreasonUHost == "") { killreasonUHost = "Your ident or hostmask did not match the one registered to this nickname. Allowed: $allowedident@$allowedhost"; }
								std::string tmpKillReason = killreasonUHost;
								SearchAndReplace(tmpKillReason, "$allowedident", sqlAllowedIdent.c_str());
								SearchAndReplace(tmpKillReason, "$allowedhost", sqlAllowedHost.c_str());

								/* Run Failure SQL Insert Query (For Logging) */
								std::string repfquery = failurequery;
								if (repfquery != "") {
									if (user->GetExt("wantsnick", wnick)) {
										SearchAndReplace(repfquery, "$nick", *wnick);
									} else {
										SearchAndReplace(repfquery, "$nick", user->nick);
									}

									SearchAndReplace(repfquery, "$host", user->host);
									SearchAndReplace(repfquery, "$ip", user->GetIPString());
									SearchAndReplace(repfquery, "$reason", tmpKillReason.c_str());

									SQLrequest req = SQLrequest(this, SQLprovider, databaseid, SQLquery(repfquery));
									result = req.Send();
								}

								ServerInstance->Users->QuitUser(user, tmpKillReason);

								user->Extend("sqlauth_failed");
								return NULL;
							}
						}

						/* We got a result, auth user */
						user->Extend("sqlauthed");

						/* possible ghosting? */
						if (user->GetExt("wantsnick", wnick)) {
							/* no need to check ghosting, this is done in OnPreCommand
							 * and if ghosting is off, user wont have the Extend 
							 */
							User* InUse = ServerInstance->FindNickOnly(wnick->c_str());
							if (InUse) {
								/* change his nick to UUID so we can take it */
								//InUse->ForceNickChange(InUse->uuid.c_str());
								/* put user on cull list */
								ServerInstance->Users->QuitUser(InUse, "Ghosted by connecting user with same nick.");
							}
							/* steal the nick ;) */
							user->ForceNickChange(wnick->c_str());
							user->Shrink("wantsnick");
						}

						/* Set Account Name (for m_services_account +R/+M channels) */
						if (setaccount) {
							std::string* pAccount = new std::string(user->nick.c_str());

							user->Extend("accountname",pAccount);
						}

						/* Run Success SQL Update Query */
						std::string repsquery = successquery;
						if (successquery != "") {
							SearchAndReplace(repsquery, "$nick", user->nick);
							SearchAndReplace(repsquery, "$host", user->host);
							SearchAndReplace(repsquery, "$ip", user->GetIPString());

							SQLrequest req = SQLrequest(this, SQLprovider, databaseid, SQLquery(repsquery));
							result = req.Send();
						}

					/* Returned No Rows */
					} else {
						if (verbose) {
							/* No rows in result, this means there was no record matching the user */
							ServerInstance->SNO->WriteToSnoMask('A', "Forbidden connection from %s!%s@%s (SQL query returned no matches)", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
						}

						/* Run Failure SQL Insert Query (For Logging) */
						std::string repfquery = failurequery;
						if (repfquery != "") {
							if (user->GetExt("wantsnick", wnick)) {
								SearchAndReplace(repfquery, "$nick", *wnick);
							} else {
								SearchAndReplace(repfquery, "$nick", user->nick);
							}

							SearchAndReplace(repfquery, "$host", user->host);
							SearchAndReplace(repfquery, "$ip", user->GetIPString());
							SearchAndReplace(repfquery, "$reason", killreason.c_str());

							SQLrequest req = SQLrequest(this, SQLprovider, databaseid, SQLquery(repfquery));
							result = req.Send();
						}

						/* Kill user that entered invalid credentials */
						ServerInstance->Users->QuitUser(user, killreason);

						user->Extend("sqlauth_failed");
					}
				/* SQL Failure */
				} else {
					if (verbose) {
						ServerInstance->SNO->WriteToSnoMask('A', "Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), res->error.Str());
					}
					
					user->Extend("sqlauth_failed");
				}
			} else {
				return NULL;
			}

			if (!user->GetExt("sqlauthed")) {
				ServerInstance->Users->QuitUser(user, killreason);
			}
			return SQLSUCCESS;
		}
		return NULL;
	}

	/* User has connected to the IRCd */
	virtual void OnUserConnect(User* user) {
		std::string sqlAllowedIdent;
		std::string sqlAllowedHost;
		std::string sqlvHost;
		std::string sqlTitle;
		std::string sqlumodes;

		std::string* pAllowedIdent;
		std::string* pAllowedHost;
		std::string* pvHost;
		std::string* pTitle;
		std::string* pumodes;

		user->GetExt("sqlAllowedIdent",pAllowedIdent);
		user->GetExt("sqlAllowedHost",pAllowedHost);
		user->GetExt("sqlvHost",pvHost);
		user->GetExt("sqlTitle",pTitle);
		user->GetExt("sqlumodes",pumodes);

		sqlAllowedIdent = pAllowedIdent->c_str();
		sqlAllowedHost = pAllowedHost->c_str();
		sqlvHost = pvHost->c_str();
		sqlTitle = pTitle->c_str();
		sqlumodes = pumodes->c_str();

		/* Change User vHost (If Enabled) */
		if (sqlvHost != "") {
			user->ChangeDisplayedHost(sqlvHost.c_str());
		}

		/* Change User Whois Title (If Enabled, requires m_customtitle to be loaded) */
		if (sqlTitle != "" && m_customtitle) {
			std::string* text;

			if (user->GetExt("ctitle", text)) {
				user->Shrink("ctitle");
				delete text;
			}

			if (!user->GetExt("ctitle", text)) {
				text = new std::string(sqlTitle);
				user->Extend("ctitle",text);
			} else {
				(*text) = sqlTitle;
			}

			ServerInstance->PI->SendMetaData(user, TYPE_USER, "ctitle", *text);
		}

		/* Set Modes on Connected User (If Enabled) */
		if (sqlumodes != "") {
			std::string buf;
			std::stringstream ss(sqlumodes);

			std::vector<std::string> tokens;

			// split ThisUserModes into modes and mode params
			while (ss >> buf)
				tokens.push_back(buf);

			std::vector<std::string> modes;
			modes.push_back(user->nick);
			modes.push_back(tokens[0]);

			if (tokens.size() > 1) {
				// process mode params
				for (unsigned int k = 1; k < tokens.size(); k++) {
					modes.push_back(tokens[k]);
				}
			}

			// ServerInstance->Parser->CallHandler("MODE", modes, user);
			ServerInstance->SendMode(modes, user); 
		}
	}

	/* User has fully connected to IRCd */
	virtual void OnPostConnect(User* user) {
		/* Identify with NickServ */
		if (servicesident) {
			Command* pm_command = ServerInstance->Parser->GetHandler("PRIVMSG");

			if (pm_command) {
				std::vector<std::string> params;
				params.push_back("NickServ");
				params.push_back("IDENTIFY "+user->password);
				pm_command->Handle(params, user);
			}

			/*
			User * u = ServerInstance->FindNick("NickServ");

			if (u != NULL) {
				u->Write(":%s!%s@%s PRIVMSG NickServ :IDENTIFY %s", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), user->password.c_str());
			}
			*/
		}
	}

	virtual void OnUserDisconnect(User* user) {
		user->Shrink("sqlauthed");
		user->Shrink("sqlauth_failed");
		user->Shrink("sqlAllowedIdent");
		user->Shrink("sqlAllowedHost");
		user->Shrink("sqlvHost");
		user->Shrink("sqlTitle");
		user->Shrink("sqlumodes");
		user->Shrink("wantsnick");
	}

	virtual bool OnCheckReady(User* user) {
		return user->GetExt("sqlauthed");
	}

	virtual Version GetVersion() {
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSQLAuth)
