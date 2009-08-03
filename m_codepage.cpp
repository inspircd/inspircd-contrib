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


/* $ModDesc: Recodes messages from one encoding to another based on port bindings. */
/* $ModAuthor: Alexey */
/* $ModAuthorMail: Phoenix@RusNet */
/* $ModDepends: core 1.2 */
/* $ModVersion: $Rev: 78 $ */

/* 
   by Chernov-Phoenix Alexey (Phoenix@RusNet) mailto:phoenix /email address separator/ pravmail.ru */

#include "inspircd.h"
#include <iconv.h>
#include "transport.h"


/* a simple class for inter-module communication. Quite simple, but may be useful. */
class lwbBufferedSocket : public BufferedSocket
{
    public:
    
    lwbBufferedSocket(int fdnew):BufferedSocket(NULL)
    {
        this->fd = fdnew;
    }
    
    void cleanup()
    {
	this->fd=-1;
    }
    
};

/* a record containing incoming and outgoing convertion descriptors and encoding name */
struct io_iconv
    {
    iconv_t in,out;
    std::string encoding;
    };

typedef nspace::hash_map<int, int> hash_common;			/* port/file descriptor 	-> encoding index */
typedef nspace::hash_map<std::string, int> hash_str;		/* encoding name 		-> encoding index */
typedef nspace::hash_map<int, Module *> hash_io;		/* file descriptor		-> old io handler */

static hash_common fd_hash, port_hash;
static hash_str name_hash;
static hash_io io_hash;
static std::vector<io_iconv> recode; /* the main encoding storage */

char toUpper_ (char c) { return std::toupper(c); }
void ToUpper(std::string& s) {std::transform(s.begin(), s.end(), s.begin(),toUpper_);}

class CommandCodepage : public Command
{
 public:
        CommandCodepage (InspIRCd* Instance) : Command(Instance, "CODEPAGE", 0, 1)
        {
                this->source = "m_codepage.so";
                syntax = "<codepage name>";
                TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
        }

        CmdResult Handle (const std::vector<std::string>& parameters, User *user)
        {
		
		std::string codepage=parameters[0];
		ToUpper(codepage);
		hash_str::iterator iter;

		if (codepage=="SHOW") /* A horrible expression! to find the name of users's current codepage! Although that works. */
		{
		    codepage=recode[fd_hash[user->GetFd()]].encoding;
		}
		
		iter=name_hash.find(codepage);
		
                if (iter!=name_hash.end())
                {
		    fd_hash[user->GetFd()]=iter->second;
                    user->WriteNumeric(700, "%s :Your codepage is: %s",user->nick.c_str(),codepage.c_str());
                    return CMD_LOCALONLY;	
                }
                else
                {
                    user->WriteNumeric(750, "%s :Unsupported codepage: %s",user->nick.c_str(),codepage.c_str());
                }

                return CMD_FAILURE;
        }
};

class ModuleCodepage : public Module
{
	private:
		InspIRCd* ServerInstance;
		CommandCodepage*  mycommand;
		std::string icodepage, dcodepage;
    		std::vector<std::string> listenports;
	public:
		ModuleCodepage(InspIRCd* Me)
			: Module(Me)
		{
			ServerInstance=Me;
			OnRehash(NULL, "");
            		mycommand = new CommandCodepage(ServerInstance);
            		ServerInstance->AddCommand(mycommand);
			Implementation eventlist[] = { I_OnRehash, I_OnCleanup, I_OnHookUserIO, 
			    I_OnRawSocketRead, I_OnRawSocketWrite, I_OnRawSocketAccept, I_OnRawSocketClose, I_OnRawSocketConnect };
            		ServerInstance->Modules->Attach(eventlist, this, 8);
			HookExisting();
		}
		
		void HookExisting()
		{
		std::vector<User*>::iterator iter;
		for (iter=ServerInstance->Users->local_users.begin();iter!=ServerInstance->Users->local_users.end();++iter)
		    {
/*		    OnHookUserIO(iter->second, ) how do I get back listenport's value? */

/*OnHookUserIO code follows then: no way to determine targetip :( :( :(
            	    if (isin(targetip,user->GetPort(),listenports))*/ 
            	    {
                        /* Hook the user with our module... (and save his/her/its (that may be also a bot ;) ) ->io ) */
			int fd=(*iter)->GetFd();
			if ((*iter)->io)
			    io_hash[fd]=(*iter)->io;
                        (*iter)->io = this;
			
			/* restoring default encoding on a port for a user, OnRawSocketAccept code */
			hash_common::iterator iter2;
			iter2=port_hash.find((*iter)->GetPort());
			if (iter2!=port_hash.end())
			    fd_hash[fd]=iter2->second;
            	    }

		    }
		}

    		bool isin(const std::string &host, int port, const std::vector<std::string> &portlist)
    		{
            	    if (std::find(portlist.begin(), portlist.end(), "*:" + ConvToStr(port)) != portlist.end())
                        return true;

            	    if (std::find(portlist.begin(), portlist.end(), ":" + ConvToStr(port)) != portlist.end())
                        return true;

            	    return std::find(portlist.begin(), portlist.end(), host + ":" + ConvToStr(port)) != portlist.end();
    		}
		
		virtual void OnRehash(User* user, const std::string &parameter)
		{
            		listenports.clear();
			fd_hash.clear(); port_hash.clear(); name_hash.clear(); recode.clear();
		    	ConfigReader* conf = new ConfigReader(ServerInstance);
			icodepage="";
			
			/* load the internal && default codepage */

                        for (int i = 0; i < conf->Enumerate("codepage"); i++) /* <- seek for internal */
                        {
			    std::string tmp;
                            tmp = conf->ReadValue("codepage", "internal", i);
			    if (!tmp.empty())
				{
				icodepage=tmp;
				break;
				}
			}

			if (icodepage.empty()) 
			    {
			    /* NO internal encoding set*/
			    ServerInstance->Logs->Log("m_codepage",DEBUG,"WARNING: no internal encoding is set but module loaded");
			    return;
			    }
			
			ToUpper(icodepage);
			dcodepage=icodepage; /* set default codepage to the internal one by default ;) */

                        for (int i = 0; i < conf->Enumerate("codepage"); i++) /* <- seek for default */
                        {
			    std::string tmp;
                            tmp = conf->ReadValue("codepage", "default", i);
			    if (!tmp.empty())
				{
				dcodepage=tmp;
				break;
				}
			}
			ToUpper(dcodepage);
			
			ServerInstance->Logs->Log("m_codepage",DEFAULT,"INFO: internal encoding is %s now, default is %s ",icodepage.c_str(),dcodepage.c_str());

			/* setting up encodings on ports */
			
			io_iconv tmpio;
			
			tmpio.encoding=icodepage; /* first we push a record for internal CP for no any convertion to be applied */
			tmpio.in=tmpio.out=(iconv_t)-1;
			name_hash[icodepage]=0;
			recode.push_back(tmpio);
			
                        for (int i = 0; i < conf->Enumerate("bind"); i++)
                        {
                                std::string type,codepage,port,addr;
				int recodeindex=0;
				
                                type    = conf->ReadValue("bind", "type", i);
                                if (type!="clients")
                                    {continue;} /* oh, that's a server port, sorry, skipping :( */
				    
                                codepage = conf->ReadValue("bind", "codepage", i);
                                port = conf->ReadValue("bind", "port", i);
                                addr = conf->ReadValue("bind", "address", i);
				
                                if (codepage.empty()) /* no encoding specified explicitly. assuming default */
                                    {
				    codepage=dcodepage;
				    } 
				else
				    {
                            	    ToUpper(codepage);
				    }

				
				hash_str::iterator iter=name_hash.find(codepage);
				
				if (iter==name_hash.end())
				/* not found, so let's create it */
				    { /* wrong convertion, assuming default (0) */
                            	    if  (((tmpio.in  = iconv_open(icodepage.c_str(), codepage.c_str())) == (iconv_t)-1) ||
                                         ((tmpio.out = iconv_open(codepage.c_str(), icodepage.c_str())) == (iconv_t)-1))
                                	{
					recodeindex=0;
                                	ServerInstance->Logs->Log("m_codepage.so",DEFAULT, "WARNING: wrong conversion between %s and %s. Assuming internal codepage!",icodepage.c_str(), codepage.c_str());
                                	}
				    else /* right convertion, pushing it into the vector */
					{
					recodeindex=recode.size();
					
					tmpio.encoding=codepage;
					name_hash[codepage]=recode.size();
					recode.push_back(tmpio);
					}
				    }
				else /* it exists already */
				    {
				    recodeindex=iter->second;
				    }
				
                                irc::portparser portrange(port, false);
                                long portno = -1;
                                while ((portno = portrange.GetToken()))
                                {
                                    listenports.push_back(addr + ":" + ConvToStr(portno));
				    port_hash[portno]=recodeindex;
                            	    ServerInstance->Logs->Log("m_codepage.so",DEFAULT, "INFO: adding %s encoding on the port %ld",recode[port_hash[portno]].encoding.c_str(),portno);
                                }

                        }
		
			delete conf;
		}
    		
    		virtual void OnRawSocketClose(int fd)
    		{
		    fd_hash.erase(fd);
		    io_hash.erase(fd);

		    hash_io::iterator iter2;
		    iter2=io_hash.find(fd);
		    if (iter2!=io_hash.end())
			{
			iter2->second->OnRawSocketClose(fd);
			}
		}
		
    		virtual void OnRawSocketConnect(int fd)
		{
		    hash_io::iterator iter2;
		    iter2=io_hash.find(fd);
		    if (iter2!=io_hash.end())
			{
			iter2->second->OnRawSocketConnect(fd);
			}
		}
    		virtual void OnRawSocketAccept(int fd, const std::string &ip, int localport)
    		{
		    hash_common::iterator iter;
		    iter=port_hash.find(localport);
		    if (iter!=port_hash.end())
			fd_hash[fd]=iter->second;

		    hash_io::iterator iter2;
		    iter2=io_hash.find(fd);
		    if (iter2!=io_hash.end())
			{
			iter2->second->OnRawSocketAccept(fd,ip,localport);
			}
		}
	
    		virtual void OnHookUserIO(User* user, const std::string &targetip)
    		{
            	    if (isin(targetip,user->GetPort(),listenports))
            	    {
                        /* Hook the user with our module... (and save his/her/its (that may be also a bot ;) ) ->io ) */
			if (user->io)
			    io_hash[user->GetFd()]=user->io;
                        user->io = this;
            	    }
	        }

                size_t i_convert(iconv_t cd,char* dest,char* src,int countin,int countout)
                {

                        size_t ret_val=(size_t)(-1);
                        size_t inbytesleft=countin,outbytesleft=countout;
                        char* src1=src;
                        char* dest1=dest;
                        if (cd!=(iconv_t)-1)
                            {
                            ret_val = iconv(cd, &src1, &inbytesleft, &dest1, &outbytesleft);
/*                            if (ret_val==(size_t)-1)
                                {
                            	ServerInstance->Logs->Log("m_codepage.so",DEFAULT, "INFO: error converting: %s",src);
                                strncpy(dest,src,count);
				dest[count]=0;
                                }
                            else*/
                                {
/*                        ServerInstance->Logs->Log("m_codepage.so",DEFAULT, "INFO: endof converting");*/
                                dest[countout-outbytesleft]=0;
                    		return countout-outbytesleft;
                                }
                            }
			else
			    {
			    strncpy(dest,src,countin);
			    dest[countin]=0;
			    }
			return countin;
                }

		bool checkio(Module* io, int fd)
		{
		lwbBufferedSocket* sock=new lwbBufferedSocket(fd);
		Request* req=new ISHRequest(this,io,"IS_HSDONE",sock);
		
		const char * answer=req->Send();
		
		delete req;
		sock->cleanup();
		delete sock;

		if (answer==NULL)
		    {
		    return false;
		    }

		if (strcmp("OK",answer)==0)
		    return true;
		return false;
		}
		
    		virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult)		
		{
		    
		    io_iconv tmpio;
		    int result;
            	    User* user = dynamic_cast<User*>(ServerInstance->FindDescriptor(fd));

            	    if (user == NULL)
                        return -1;

		    hash_io::iterator iter2;
		    iter2=io_hash.find(fd);
		    if (iter2!=io_hash.end())
			{
			result = iter2->second->OnRawSocketRead(fd, buffer, count, readresult);
/*			ServerInstance->Logs->Log("m_codepage",DEBUG,"INFO: reading SSL (%d) %d %d %s",count,result,readresult,buffer);*/
			}
		    else
			{
            		result = user->ReadData(buffer, count);
/*			ServerInstance->Logs->Log("m_codepage",DEBUG,"INFO: reading non-SSL %d %s",result,buffer);*/
            		readresult = result;
			}
		    
 		    hash_common::iterator iter=fd_hash.find(fd);
		    if (iter==fd_hash.end()) /* no any value in a hash? */
			tmpio.in=(iconv_t)-1;
		    else
			tmpio=recode[iter->second];

            	    if ((result == -1) && (errno == EAGAIN))
                        return -1;
            	    else if (result < 1)
                        return 0;
		    
		    if (tmpio.in!=(iconv_t)-1)
			{
			/* translating encodings here */
			char * tmpbuffer=new char[count+1];

			size_t cnt;
			cnt=i_convert(tmpio.in,tmpbuffer,buffer,count,count);
			strncpy(buffer,tmpbuffer,cnt);
			buffer[cnt]=0;
			delete [] tmpbuffer;
			}
		    
		    
            	    return result;
		    
		}

    		virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
		{
		    hash_io::iterator iter2;
		    iter2=io_hash.find(fd);
		    
		    io_iconv tmpio;
            	    User* user = dynamic_cast<User*>(ServerInstance->FindDescriptor(fd));

            	    if (user == NULL)
                        return -1;

		    hash_common::iterator iter=fd_hash.find(fd);
		    if (iter==fd_hash.end()) /* no any value in a hash? */
			tmpio.out=(iconv_t)-1;
		    else
			tmpio=recode[iter->second];
			
		    size_t cnt;
		    char * tmpbuffer=new char[count*4+1];
		    if (tmpio.out!=(iconv_t)-1)
			{
			/* translating encodings here */
			cnt=i_convert(tmpio.out,tmpbuffer,(char *)buffer,count,count*4);
			}
		    else
			{
			memcpy(tmpbuffer, buffer, count);
			tmpbuffer[count]=0;
			cnt=count;
			}	    
		
			
		    if (iter2!=io_hash.end())
			{
/*			ServerInstance->Logs->Log("m_codepage",DEBUG,"INFO: writing SSL %s",tmpbuffer);*/
			int tmpres=iter2->second->OnRawSocketWrite(fd, tmpbuffer, cnt);
			delete [] tmpbuffer;
			return tmpres;
			}
		    else
			{
/*			ServerInstance->Logs->Log("m_codepage",DEBUG,"INFO: writing non-SSL %s",tmpbuffer);*/
            		user->AddWriteBuf(std::string(tmpbuffer,cnt));
			}
		    delete [] tmpbuffer;
		    return 1;
		}
		
        virtual void OnCleanup(int target_type, void* item)
        {
                if(target_type == TYPE_USER)
                {
                        User* user = (User*)item;

                        if(user->io==this)
                        {
			    hash_io::iterator iter=io_hash.find(user->GetFd());
			    if (iter!=io_hash.end())
				{
				user->io=iter->second;
				io_hash.erase(user->GetFd());
				}
			    else
				{
				user->io=NULL;
				}
                        }
                }
        }
		
		virtual ~ModuleCodepage()
		{
			for (hash_io::iterator iter=io_hash.begin();iter!=io_hash.end();iter++)
			    {
            		    User* user = dynamic_cast<User*>(ServerInstance->FindDescriptor(iter->first));
			    if (user==NULL)
				continue;
			    user->io=iter->second;
			    }
            		listenports.clear(); fd_hash.clear(); port_hash.clear(); name_hash.clear(); recode.clear(); io_hash.clear();
		}

		virtual Version GetVersion()
		{
			return Version(0,0,0,0,VF_VENDOR,API_VERSION);
		}

};


MODULE_INIT(ModuleCodepage)
