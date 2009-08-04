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

/* 
   by Chernov-Phoenix Alexey (Phoenix@RusNet) mailto:phoenix /email address separator/ pravmail.ru */

/* $ModDesc: Recodes messages from one encoding to another based on port bindings. */
/* $ModAuthor: Alexey */
/* $ModAuthorMail: Phoenix@RusNet */
/* $ModDepends: core 1.2 */
/* $ModVersion: $Rev: 78 $ */


#include "inspircd.h"
#include <iconv.h>
#include "modules.h"

/* a record containing incoming and outgoing convertion descriptors and encoding name */
struct io_iconv
    {
    iconv_t in,out;
    std::string encoding;
    char * intable;
    char * outtable;
    };

/* a read buffer for incomplete multibyte characters. As they are just characters and they are incomplete, it's 3 bytes long :) */
struct io_buffer
    {
    char buffer[3];
    char count;
    };

typedef nspace::hash_map<int, int> hash_common;			/* port/file descriptor 	-> encoding index */
typedef nspace::hash_map<std::string, int> hash_str;		/* encoding name 		-> encoding index */
typedef nspace::hash_map<int, Module *> hash_io;		/* file descriptor		-> old io handler */
typedef nspace::hash_map<int, std::string> hash_save;		/* file descriptor		-> encoding name */
typedef nspace::hash_map<int, io_buffer> hash_buffer;		/* file descriptor		-> read multibyte buffer */

const char * modulenames[]={"m_ssl_gnutls.so","m_ssl_openssl.so","m_xmlsocket.so"};
static Implementation eventlist[] = { I_OnRehash, I_OnCleanup, I_OnHookUserIO, I_OnUnloadModule,
			    I_OnRawSocketRead, I_OnRawSocketWrite, I_OnRawSocketAccept, I_OnRawSocketClose, I_OnRawSocketConnect };
static hash_common fd_hash, port_hash;
static hash_str name_hash;
static hash_io io_hash;
static hash_save save_hash;
static hash_buffer buffer_hash;
static std::vector<io_iconv> recode; /* the main encoding storage */

char toUpper_ (char c) { return std::toupper(c); }
void ToUpper(std::string& s) {std::transform(s.begin(), s.end(), s.begin(),toUpper_);}

class CommandCodepage : public Command
{
 public:
        CommandCodepage (InspIRCd* Instance) : Command(Instance, "CODEPAGE", 0, 1)
        {
                this->source = "m_codepage.so";
                syntax = "{ <codepage> | SHOW | NEXT }";
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
            	    user->WriteNumeric(700, "%s :Your codepage is: %s",user->nick.c_str(),codepage.c_str());
                    return CMD_LOCALONLY;	
		}
			
		if (codepage=="NEXT")
		{
		    int index=fd_hash[user->GetFd()]+1;
		    if ((unsigned int)index>=recode.size())
			index=0;
		    codepage=recode[index].encoding;
		}
		
		iter=name_hash.find(codepage);
		
                if (iter!=name_hash.end())
                {
		    if (fd_hash[user->GetFd()]!=iter->second)
		    {
			fd_hash[user->GetFd()]=iter->second;
            		user->WriteNumeric(700, "%s :Your codepage is: %s",user->nick.c_str(),codepage.c_str());
                	return CMD_LOCALONLY;	
		    }
		    else
		    {
            		user->WriteNumeric(752, "%s :Codepage is already: %s",user->nick.c_str(),codepage.c_str());
		    }
                }
                else
                {
                    user->WriteNumeric(750, "%s :Wrong or unsupported codepage: %s",user->nick.c_str(),codepage.c_str());
                }

                return CMD_FAILURE;
        }
};

/* took it from SAQUIT :) */
class CommandSacodepage : public Command
{
 public:
        CommandSacodepage (InspIRCd* Instance) : Command(Instance, "SACODEPAGE", "o", 2, false, 0)
        {
                this->source = "m_codepage.so";
                syntax = "<nick> { <codepage> | NEXT }";
                TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
        }

        CmdResult Handle (const std::vector<std::string>& parameters, User *user)
        {
                User* dest = ServerInstance->FindNick(parameters[0]);
                if (dest)
                {
                        if (ServerInstance->ULine(dest->server))
                        {
                                user->WriteNumeric(990, "%s :Cannot use an SA command on a u-lined client",user->nick.c_str());
                                return CMD_FAILURE;
                        }
			
			std::string codepage=parameters[1];
			ToUpper(codepage);

                        ServerInstance->SNO->WriteToSnoMask('A', std::string(user->nick)+" used SACODEPAGE to force "+std::string(dest->nick)+" have a codepage of "+codepage);

                        /* Pass the command on, so the client's server can handle it properly.*/
                        if (!IS_LOCAL(dest))
                                return CMD_SUCCESS;

/* SACODEPAGE will be a bit different */
			hash_str::iterator iter;

			if (codepage=="SHOW") 
			{
			    codepage=recode[fd_hash[dest->GetFd()]].encoding;
            		    dest->WriteNumeric(700, "%s :Your codepage is: %s",dest->nick.c_str(),codepage.c_str());
                	    return CMD_LOCALONLY;	
			}
			
			if (codepage=="NEXT")
			{
			    int index=fd_hash[dest->GetFd()]+1;
			    if ((unsigned int)index>=recode.size())
				index=0;
			    codepage=recode[index].encoding;
			}
		
			iter=name_hash.find(codepage);
		
            		if (iter!=name_hash.end())
            		{
			    if (fd_hash[dest->GetFd()]!=iter->second)
			    {
				fd_hash[dest->GetFd()]=iter->second;
            			dest->WriteNumeric(700, "%s :Your codepage is: %s",dest->nick.c_str(),codepage.c_str());
                		return CMD_LOCALONLY;	
			    }
			    else
			    {
            			user->WriteNumeric(752, "%s :Codepage is already: %s",user->nick.c_str(),codepage.c_str());
			    }
            		}
            		else
            		{
                	    user->WriteNumeric(750, "%s :Wrong or unsupported codepage: %s",user->nick.c_str(),codepage.c_str());
            		}

            		return CMD_FAILURE;

/* -- end of CODEPAGE cut */
            	}
            	else
            	{
                    user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[0].c_str());
            	}

                return CMD_FAILURE;
        }
};

class CommandCodepages : public Command
{
 public:
        CommandCodepages (InspIRCd* Instance) : Command(Instance, "CODEPAGES", 0, 0, false, 0)
        {
                this->source = "m_codepage.so";
                syntax = "[server]";
                TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
        }

        CmdResult Handle (const std::vector<std::string>& parameters, User *user)
        {	
		std::string servname;
		if (parameters.size()<1)
		    {
		    servname=std::string(ServerInstance->Config->ServerName);
		    }
		else
		    {
		    servname=parameters[0];
		    }
		    
                if (ServerInstance->FindServerName(servname))
                {
                    /* Pass the command on, so the client's server can handle it properly. */
                    if (servname!=ServerInstance->Config->ServerName)
                        return CMD_SUCCESS;
		    
		    for (unsigned int i=0; i<recode.size(); ++i)
            		user->WriteNumeric(701, "%s : Codepage available: %s",user->nick.c_str(),recode[i].encoding.c_str());
			
            	    user->WriteNumeric(702, "%s :*** End of CODEPAGES",user->nick.c_str());
            	    return CMD_LOCALONLY;
            	}
            	else
            	{
                    user->WriteServ("NOTICE %s :*** Invalid server name '%s'", user->nick.c_str(), parameters[0].c_str());
            	}

                return CMD_FAILURE;
        }
};

class ModuleCodepage : public Module
{
	private:
		InspIRCd* ServerInstance;
		CommandCodepage* mycommand;
		CommandSacodepage* mycommand2;
		CommandCodepages* mycommand3;
		std::string icodepage, dcodepage;
	public:
		ModuleCodepage(InspIRCd* Me)
			: Module(Me)
		{
			ServerInstance=Me;
			recode.clear(); fd_hash.clear();
			OnRehash(NULL);
            		mycommand = new CommandCodepage(ServerInstance);
            		ServerInstance->AddCommand(mycommand);
            		mycommand2 = new CommandSacodepage(ServerInstance);
            		ServerInstance->AddCommand(mycommand2);
            		mycommand3 = new CommandCodepages(ServerInstance);
            		ServerInstance->AddCommand(mycommand3);
            		ServerInstance->Modules->Attach(eventlist, this, 9);
		}
		
		void SaveExisting()
		{
		std::vector<User*>::iterator iter;
		save_hash.clear();
		for (iter=ServerInstance->Users->local_users.begin();iter!=ServerInstance->Users->local_users.end();++iter)
		    {
		    int fd=(*iter)->GetFd();
		    hash_common::iterator iter2=fd_hash.find(fd);
		    if (iter2!=fd_hash.end())
			{
			std::string codepage=recode[iter2->second].encoding;
			save_hash[fd]=codepage;
			}
		    }
		}
		
		void HookExisting()
		{
		std::vector<User*>::iterator iter;
		for (iter=ServerInstance->Users->local_users.begin();iter!=ServerInstance->Users->local_users.end();++iter)
		    {
                        /* Hook the user with our module for stacking... (and save his/her/its (that may be also a bot ;) ) ->IOHook ) */
			int fd=(*iter)->GetFd();
			Module * hk = (*iter)->GetIOHook();
			if (hk && (hk!=this))
			{
			    hash_io::iterator iter3=io_hash.find(fd);
			    if (iter3==io_hash.end())
			    {
				io_hash[fd]=hk;
			    }
			}
			(*iter)->DelIOHook();
			(*iter)->AddIOHook(this);
			/* restoring saved or the default encoding on a port for a user, OnRawSocketAccept code */
			bool found=false;
			hash_save::iterator iter4=save_hash.find(fd);
			if (iter4!=save_hash.end())
			{
			    hash_str::iterator iter5=name_hash.find(iter4->second);
			    if (iter5!=name_hash.end())
			    {
				fd_hash[fd]=iter5->second;
				found=true;
			    }
			}
			if (!found)
			{
			    hash_common::iterator iter2;
			    iter2=port_hash.find((*iter)->GetPort());
			    if (iter2!=port_hash.end())
				fd_hash[fd]=iter2->second;
			}
		    }
		save_hash.clear();
		}

    		bool isin(const std::string &host, int port, const std::vector<std::string> &portlist)
    		{
            	    if (std::find(portlist.begin(), portlist.end(), "*:" + ConvToStr(port)) != portlist.end())
                        return true;

            	    if (std::find(portlist.begin(), portlist.end(), ":" + ConvToStr(port)) != portlist.end())
                        return true;

            	    return std::find(portlist.begin(), portlist.end(), host + ":" + ConvToStr(port)) != portlist.end();
    		}
		
		void itableconvert(char* table, char* dest, const char* source, int n)
		{
		--n;
		for (;n>=0;--n)
		    {
		    dest[n]=table[(unsigned char)source[n]];
		    }
		}
		
		void makeitable(iconv_t cd, char * &table)
		{
		int i;
		char tmp[2]; tmp[1]=0; /* trailing 0 */

		table=new char[256];
		for (i=0;i<0x100;++i)
		{
		    tmp[0]=(char)i;
		    char * src=tmp;
		    char * dest=table+i;
		    size_t inbytesleft=1,outbytesleft=1;
		    size_t ret_val = iconv(cd, &src, &inbytesleft, &dest, &outbytesleft);
		    if (ret_val==size_t(-1))
		    {
			if (errno==EILSEQ)
			    table[i]='?';
			else
			{
			    delete [] table;
			    table=NULL;
			    break;
			}
		    }
		}
		return;
		}
		
		unsigned int addavailable(const std::string &codepage)
		{
		    io_iconv tmpio;

		    hash_str::iterator iter=name_hash.find(codepage);
				
		    if (iter==name_hash.end()) /* not found, so let's create it */
		    { /* wrong convertion, assuming default (0) */
                        if  (((tmpio.in  = iconv_open(icodepage.c_str(), codepage.c_str())) == (iconv_t)-1) ||
                            ((tmpio.out = iconv_open(codepage.c_str(), icodepage.c_str())) == (iconv_t)-1))
                        {
                            ServerInstance->Logs->Log("m_codepage.so",DEFAULT, "WARNING: wrong conversion between %s and %s. Assuming internal codepage!",icodepage.c_str(), codepage.c_str());
                        }
			else /* right convertion, pushing it into the vector */
			{
			    tmpio.encoding=codepage;
			    makeitable(tmpio.in ,tmpio.intable );
			    makeitable(tmpio.out,tmpio.outtable);
			    name_hash[codepage]=recode.size();
			    recode.push_back(tmpio);
			    return recode.size()-1;
			}
		    }
		    else /* it exists already */
		    {
			return iter->second;
		    }
		    
		    return 0;
		}
		
		virtual void OnRehash(User* user)
		{	
			SaveExisting();
			iClose();
			fd_hash.clear(); port_hash.clear(); name_hash.clear(); recode.clear(); buffer_hash.clear();
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
			
			io_iconv tmpio;
			
			/* first we push a record for internal CP for no any convertion to be applied */
			tmpio.encoding=icodepage; 
			tmpio.in=tmpio.out=(iconv_t)-1;
			tmpio.intable=tmpio.outtable=NULL;
			name_hash[icodepage]=0;
			recode.push_back(tmpio);
			
			/* list of available encodings */
			
                        for (int i = 0; i < conf->Enumerate("codepage"); i++) /* <- seek for default */
                        {
			    std::string tmp;
                            tmp = conf->ReadValue("codepage", "available", i);
			    if (!tmp.empty())
			    {
                    		irc::commasepstream css(tmp.c_str());
                    		std::string tok;
				while(css.GetToken(tok))
				{
				    ToUpper(tok);
				    addavailable(tok);
				}
				break;
			    }
			}
			
			/* setting up encodings on ports */
			
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

				recodeindex=addavailable(codepage);
                                irc::portparser portrange(port, false);
                                long portno = -1;
                                while ((portno = portrange.GetToken()))
                                {
				    port_hash[portno]=recodeindex;
                            	    ServerInstance->Logs->Log("m_codepage.so",DEFAULT, "INFO: adding %s encoding on the port %ld",recode[port_hash[portno]].encoding.c_str(),portno);
                                }

                        }
		
			delete conf;
			HookExisting();
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
                        /* Hook the user with our module... (and save his/her/its (that may be also a bot ;) ) ->IOHook ) */
			if (user->GetIOHook())
			    {
			    io_hash[user->GetFd()]=user->GetIOHook();
			    }
			user->DelIOHook();
                        user->AddIOHook(this);
	        }

                size_t i_convert(iconv_t cd,char* dest,char* src,int countin,int countout,bool omiteinval=true, int fd=-1)
                {

                        size_t ret_val=(size_t)0;
                        size_t inbytesleft=countin,outbytesleft=countout;
                        char* src1=src;
                        char* dest1=dest;
                        if (cd!=(iconv_t)-1)
                            {
				for(;inbytesleft && !((ret_val==(size_t)-1)&&((errno==E2BIG)||(errno==EINVAL)));--inbytesleft,++src1)
				{
                        	    ret_val = iconv(cd, &src1, &inbytesleft, &dest1, &outbytesleft);
				    if (!inbytesleft) break;
				}
				
				/* Saving incomplete character. let's be paranoid, (inbytesleft<4) */
				if ((errno==EINVAL)&&(!omiteinval)&&(inbytesleft<4))
				{
				    io_buffer tmpio_b;
				    memcpy(tmpio_b.buffer,src1,inbytesleft);
				    tmpio_b.count=inbytesleft;
				    buffer_hash[fd]=tmpio_b;
				}
				
                    		return countout-outbytesleft;
                            }
			memcpy(dest,src,countin);
			return countin;
                }

    		virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult)		
		{
		    
		    io_iconv tmpio;
		    int result;
            	    User* user = dynamic_cast<User*>(ServerInstance->SE->GetRef(fd));

            	    if (user == NULL)
                        return -1;

		    hash_io::iterator iter2;
		    iter2=io_hash.find(fd);
		    if (iter2!=io_hash.end())
			{
			result = iter2->second->OnRawSocketRead(fd, buffer, count, readresult);
			}
		    else
			{
            		result = user->ReadData(buffer, count);
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
			char * tmpbuffer=new char[count+4];
			char * writestart=tmpbuffer;
			
			hash_buffer::iterator iter3=buffer_hash.find(fd);
			if (iter3!=buffer_hash.end())
			{
			    memcpy(tmpbuffer,iter3->second.buffer,iter3->second.count);
			    writestart+=iter3->second.count;
			    buffer_hash.erase(iter3);
			}
			
			memcpy(writestart,buffer,readresult);
			
			if (tmpio.intable!=NULL)
			{
			    itableconvert(tmpio.intable, buffer, tmpbuffer, readresult);
			}
			else
			{
			    size_t cnt=i_convert(tmpio.in,buffer,tmpbuffer,readresult,readresult, false, fd);
			    readresult=cnt;
			}
			delete [] tmpbuffer;
			}
		    
		    
            	    return result;
		    
		}

    		virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
		{
		    hash_io::iterator iter2;
		    iter2=io_hash.find(fd);
		    
		    io_iconv tmpio;
            	    User* user = dynamic_cast<User*>(ServerInstance->SE->GetRef(fd));

            	    if (user == NULL)
                        return -1;

		    hash_common::iterator iter=fd_hash.find(fd);
		    if (iter==fd_hash.end()) /* no any value in a hash? */
			tmpio.out=(iconv_t)-1;
		    else
			tmpio=recode[iter->second];
			
		    size_t cnt=count;
		    char * tmpbuffer=new char[count*4+1]; /* assuming UTF-8 is 4 chars wide max. */
		    if (tmpio.out!=(iconv_t)-1)
			{
			/* translating encodings here */
			if (tmpio.outtable!=NULL)
			{
			    itableconvert(tmpio.outtable, tmpbuffer, buffer, count);
			    tmpbuffer[count]=0;
			}
			else
			    cnt=i_convert(tmpio.out,tmpbuffer,(char *)buffer,count,count*4);
			}
		    else
			{
			memcpy(tmpbuffer, buffer, count);
			tmpbuffer[count]=0;
			}	    
		
			
		    if (iter2!=io_hash.end())
			{
			int tmpres=iter2->second->OnRawSocketWrite(fd, tmpbuffer, cnt);
			delete [] tmpbuffer;
			return tmpres;
			}
		    else
			{
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

                        if(user->GetIOHook()==this)
                        {
			    hash_io::iterator iter=io_hash.find(user->GetFd());
			    if (iter!=io_hash.end())
				{
				user->DelIOHook();
				user->AddIOHook(iter->second);
				}
			    else
				{
				user->DelIOHook();
				}
                        }
                }
        }
	
        void Prioritize()
        {
	    for (unsigned int i=0;i<3;++i)
	    {
                Module* mod = ServerInstance->Modules->Find(modulenames[i]);
		if (mod==NULL)
		{
		    continue;
		}
		for (unsigned int j=0;j<9;++j)
		{
            	    ServerInstance->Modules->SetPriority(this, eventlist[j], PRIORITY_AFTER, &mod,1);
		}
	    }
        }

		void OnUnloadModule  (Module* mod, const std::string &name)
		{
		    std::vector<User *> cleanup;
		    cleanup.clear();
		    for (hash_io::iterator iter=io_hash.begin();iter!=io_hash.end();++iter)
		    {
			if (iter->second==mod)
			    {
            		    User* user = dynamic_cast<User*>(ServerInstance->SE->GetRef(iter->first));
			    user->DelIOHook();
			    user->AddIOHook(mod);
			    cleanup.push_back(user);
			    }
		    }

		    for(std::vector<User *>::iterator iter2=cleanup.begin();iter2!=cleanup.end();++iter2)
		    {
			int fd=(* iter2)->GetFd();
			mod->OnCleanup(TYPE_USER,(* iter2));
			io_hash.erase(fd);
			/* Let's handle XML Socket etc. properly */
			if ((* iter2)->quitting)
			    fd_hash.erase(fd);
			else
			    (* iter2)->DelIOHook();
			    (* iter2)->AddIOHook(this);
		    }
		    
		    /* give us back our users!!! >:( */
		    if (mod!=this) /* A horrible bug, yes :E */
			for (hash_common::iterator iter=fd_hash.begin();iter!=fd_hash.end();++iter)
			{
            		    User* user = dynamic_cast<User*>(ServerInstance->SE->GetRef(iter->first));
			    user->DelIOHook();
			    user->AddIOHook(this);
			    /* Welcome back ;) */
			}
		    
		}
		
		void iClose()
		{
			for (std::vector<io_iconv>::iterator iter=recode.begin();iter!=recode.end();iter++)
			    {
			    if ((*iter).in!=(iconv_t)-1)
				{
				iconv_close((*iter).in);
				if ((*iter).intable!=NULL)
				    delete [] (*iter).intable;
				}
			    if ((*iter).out!=(iconv_t)-1)
				iconv_close((*iter).out);
				if ((*iter).outtable!=NULL)
				    delete [] (*iter).outtable;
			    }
		}
		
		virtual ~ModuleCodepage()
		{
			for (hash_io::iterator iter=io_hash.begin();iter!=io_hash.end();iter++)
			    {
            		    User* user = dynamic_cast<User*>(ServerInstance->SE->GetRef(iter->first));
		    	    if (user==NULL)
				continue;
			    user->DelIOHook();
			    user->AddIOHook(iter->second);
			    }
			iClose();
            		save_hash.clear(); fd_hash.clear(); port_hash.clear(); name_hash.clear(); recode.clear(); io_hash.clear(); buffer_hash.clear();
		}

		virtual Version GetVersion()
		{
		return Version("$Id$",VF_VENDOR,API_VERSION);
		}

};

MODULE_INIT(ModuleCodepage)

