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
#include "xline.h"
#include "m_sqlv2.h"

/* $ModDep: m_sqlv2.h */
/* $ModDesc: Keeps a dynamic log of all XLines created, and stores them in a database. */
/* $ModAuthor: Alexey */
/* $ModAuthorMail: Phoenix@Rusnet */
/* $ModDepends: core 1.2 */

/* based on m_xline_db and m_sql_auth(?)
   by Chernov-Phoenix Alexey (Phoenix@RusNet) mailto:phoenix /email address separator/ pravmail.ru */

/*


Schema:
--
-- Table structure for table `xlines`
--

DROP TABLE IF EXISTS `xlines`;
CREATE TABLE `xlines` (
  `type` varchar(100) NOT NULL default '',
  `displayable` text NOT NULL,
  `servername` varchar(100) NOT NULL default '',
  `settime` int(11) NOT NULL default '0',
  `duration` int(11) NOT NULL default '0',
  `reason` text NOT NULL,
  `nick` varchar(100) NOT NULL default '',
  `fullrealhost` text NOT NULL,
  `expired` tinyint(4) NOT NULL default '0',
  `deleted` tinyint(4) NOT NULL default '0',
  `deletetime` int(11) NOT NULL default '0',
  `deletenick` varchar(100) NOT NULL default '',
  `deletefullrealhost` text NOT NULL
);


 */

enum XLSQLAction { XLSQL_ORDINARY, XLSQL_RENEW, XLSQL_SELECT };

class ModuleXLineSQL : public Module
{
	Module* SQLprovider;

	bool reading_db;

	std::string insertquery;
	std::string expirequery;
	std::string deletequery;
	std::string renewquery;
	std::string selectquery;
	std::string databaseid;

	std::map<unsigned long, XLSQLAction> active_queries;

public:
	ModuleXLineSQL(InspIRCd* Me) : Module(Me)
	{
		ServerInstance->Modules->UseInterface("SQL");

		SQLprovider = ServerInstance->Modules->FindFeature("SQL");
		if (!SQLprovider)
			throw ModuleException("Can't find an SQL provider module. Please load one before attempting to load m_xline_sql.");

		Implementation eventlist[] = { I_OnRequest, I_OnRehash, I_OnAddLine, I_OnDelLine, I_OnExpireLine };
		ServerInstance->Modules->Attach(eventlist, this, 5);
		reading_db = false;

		OnRehash(NULL, "");

	}

	virtual ~ModuleXLineSQL()
	{
		ServerInstance->Modules->DoneWithInterface("SQL");
	}

	virtual void OnRehash(User* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);

		databaseid      = Conf.ReadValue("xlinesql", "dbid", 0);
		insertquery   = Conf.ReadValue("xlinesql", "insertquery",
									   "insert into   $table (type,displayable,servername,settime,duration,reason,nick,fullrealhost) values (\"?\",\"?\",\"?\",?,?,\"?\",\"?\",\"?\")", 0, true);
		expirequery   = Conf.ReadValue("xlinesql", "expirequery",
									   "update        $table set   expired=1  where type=\"?\" and displayable=\"?\" and servername=\"?\" and settime=\"?\"", 0, true);
		deletequery   = Conf.ReadValue("xlinesql", "deletequery",
									   "update        $table set   deleted=1, deletetime=\"?8\",deletenick=\"?6\",deletefullrealhost=\"?7\" where type=\"?\" and displayable=\"?\" and servername=\"?\" and settime=\"?\"", 0, true);
		renewquery    = Conf.ReadValue("xlinesql", "renewquery",
									   "update        $table set   expired=1 where deleted=0 and settime+duration<=UNIX_TIMESTAMP()", 0, true);
		selectquery   = Conf.ReadValue("xlinesql", "selectquery",
									   "select * from $table where expired=0 and   deleted=0 and settime+duration> UNIX_TIMESTAMP()", 0, true);

		std::string tablename = Conf.ReadValue("xlinesql", "table", "xlines", 0, false);

		SearchAndReplace(insertquery, std::string("$table"), tablename);
		SearchAndReplace(expirequery, std::string("$table"), tablename);
		SearchAndReplace(deletequery, std::string("$table"), tablename);
		SearchAndReplace(renewquery,  std::string("$table"), tablename);
		SearchAndReplace(selectquery, std::string("$table"), tablename);

		ReadDatabase();
	}

	void ReadDatabase()
	{
		if (!renewquery.empty())
			SendQuery(renewquery, XLSQL_RENEW);

		reading_db = true;
		SendQuery(selectquery, XLSQL_SELECT);
	}

	void SendQuery(const std::string &query, XLSQLAction querytype = XLSQL_ORDINARY)
	{
		SQLrequest req = SQLrequest(this, SQLprovider, databaseid, SQLquery(query));
		if (req.Send())
		{
			active_queries[req.id] = querytype;
		}
	}

	virtual const char* OnRequest(Request* request)
	{

		if (strcmp(SQLRESID, request->GetId()) == 0)
		{
			//				ServerInstance->Logs->Log("m_xline_sql",DEBUG, "XLineSQL: executed step 2");
			SQLresult* res;
			std::map<unsigned long, XLSQLAction>::iterator n;

			res = static_cast<SQLresult*>(request);
			n = active_queries.find(res->id);

			if (n != active_queries.end())
			{
				if (n->second == XLSQL_SELECT)
				{
					int rowcount = res->Rows(), i;
					for (i = 0; i < rowcount; ++i)
					{
						SQLfieldList& currow = res->GetRow();
						//populate xlines
						XLineFactory* xlf = ServerInstance->XLines->GetFactory(currow[0].d);

						if (!xlf)
						{
							ServerInstance->SNO->WriteToSnoMask('x', "database: Unknown line type (%s).", currow[0].d.c_str());
							continue;
						}

						XLine* xl = xlf->Generate(ServerInstance->Time(), atoi(currow[4].d.c_str()), currow[2].d.c_str(), currow[5].d.c_str(), currow[1].d.c_str());
						xl->SetCreateTime(atoi(currow[3].d.c_str()));

						if (ServerInstance->XLines->AddLine(xl, NULL))
						{
							ServerInstance->SNO->WriteToSnoMask('x', "database: Added a line of type %s", currow[0].d.c_str());
						}

					}
					reading_db = false;
				}
				active_queries.erase(n);
			}

			return SQLSUCCESS;
		}

		return NULL;
	}

	void FormatAndSendQuery(std::string query, User* source, XLine* line)
	{
		if (!source)
			source = ServerInstance->FakeClient;
		SQLquery ourquery(query);
		ourquery % (line->type.c_str())
		% (line->Displayable())
		% (ServerInstance->Config->ServerName)
		% ((unsigned long)line->set_time)
		% ( (unsigned long)line->duration)
		% ( line->reason)
		% ( source->nick)
		% ( source->GetFullRealHost())
		% ((unsigned long)ServerInstance->Time())
		;
		SQLrequest req = SQLrequest(this, SQLprovider, databaseid, ourquery);
		if (req.Send())
		{
			active_queries[req.id] =  XLSQL_ORDINARY;
		}
	}

	/** Called whenever an xline is added by a local user.
	 * This method is triggered after the line is added.
	 * @param source The sender of the line or NULL for local server
	 * @param line The xline being added
	 */
	void OnAddLine(User* source, XLine* line)
	{
		if (reading_db)
		{
			return;
		}
		ServerInstance->Logs->Log("m_xline_sql", DEBUG, "XLineSQL: Adding a line");
		FormatAndSendQuery(insertquery, source, line);

	}

	/** Called whenever an xline is deleted.
	 * This method is triggered after the line is deleted.
	 * @param source The user removing the line or NULL for local server
	 * @param line the line being deleted
	 */
	void OnDelLine(User* source, XLine* line)
	{
		if (reading_db)
		{
			return;
		}
		ServerInstance->Logs->Log("m_xline_sql", DEBUG, "XLineSQL: Deleting a line");
		FormatAndSendQuery(deletequery, source, line);
	}

	void OnExpireLine(XLine *line)
	{
		if (reading_db)
		{
			return;
		}
		ServerInstance->Logs->Log("m_xline_sql", DEBUG, "XLineSQL: Expiring a line");
		FormatAndSendQuery(expirequery, ServerInstance->FakeClient, line);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", 0, API_VERSION);
	}
};

MODULE_INIT(ModuleXLineSQL)

