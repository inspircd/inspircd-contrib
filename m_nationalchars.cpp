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

/* Contains a code of Unreal IRCd + Bynets patch ( http://www.unrealircd.com/ and http://www.bynets.org/ )
   Changed at 2008-06-15 - 2008-06-18 
   by Chernov-Phoenix Alexey (Phoenix@RusNet) mailto:phoenix /email address separator/ pravmail.ru */

#include "inspircd.h"
#include "caller.h"

/* $ModDesc: Provides ability to have non-RFC1459 nicks an maybe not only them :) */
/* $ModAuthor: Alexey */
/* $ModAuthorMail: Phoenix@RusNet */
/* $ModDepends: core 1.2 */
/* $ModVersion: $Rev: 78 $ */


DEFINE_HANDLER2(lwbNickHandler, bool, const char*, size_t);

static unsigned char m_reverse_additional[256],m_additionalMB[256],m_additionalUtf8[256],m_additionalUtf8range[256];/*,m_reverse_additionalUp[256];*/

char utf8checkrest(unsigned char * mb, unsigned char cnt)
{
for (unsigned char * tmp=mb; tmp<mb+cnt; tmp++)
    {
    if ((*tmp<128)||(*tmp>191))
	return -1;
    }
return cnt+1;
}

char utf8size(unsigned char * mb)
{
if (!*mb)
    return -1;
if (!(*mb & 128))
    return 1;
if ((*mb & 224)==192)
    return utf8checkrest(mb+1,1);
if ((*mb & 240)==224)
    return utf8checkrest(mb+1,2);
if ((*mb & 248)==240)
    return utf8checkrest(mb+1,3);
return -1;
}


/* Conditions added */
bool lwbNickHandler::Call(const char* n, size_t max)
{
        if (!n || !*n)
                return false;

        unsigned int p = 0;
        for (const char* i = n; *i; i++, p++)
        {
		
		/* 1. 8bit character support */	
	
        	if ( ((*i >= 'A') && (*i <= '}'))
		    || m_reverse_additional[(unsigned char)*i])
                {
                        /* "A"-"}" can occur anywhere in a nickname */
                        continue;
                }

                if ((((*i >= '0') && (*i <= '9')) || (*i == '-')) && (i > n))
                {
                        /* "0"-"9", "-" can occur anywhere BUT the first char of a nickname */
                        continue;
                }
		
		/* 2. Multibyte encodings support:  */
		/* 2.1. 16bit char. areas, e.g. chinese:*/
		
		/* if current character is the last, we DO NOT check it against multibyte table */
		if (i[1])
		{
		    /* otherwise let's take a look at the current character and the following one */
		    bool found=false;
	    	    for(unsigned char * mb=m_additionalMB; (*mb) && (mb<m_additionalMB+sizeof(m_additionalMB)); mb+=4) 
			{
			if ( (i[0]>=mb[0]) && (i[0]<=mb[1]) && (i[1]>=mb[2]) && (i[1]<=mb[3]) )
			    {
			    /* multibyte range character found */
			    i++;p++;
			    found=true;
			    break;
			    }
			}
		    if (found)
			continue;
		}
		
		/* 2.2. Check against a simple UTF-8 characters enumeration */		
		char cursize,ncursize; /*size of a current character*/
		ncursize=utf8size((unsigned char *)i);
		/* do check only if current multibyte character is valid UTF-8 only */
		if (ncursize!=-1)
		    {
		    bool found=false;
	    	    for(unsigned char * mb=m_additionalUtf8; 
			    (utf8size(mb)!=-1) && (mb<m_additionalUtf8+sizeof(m_additionalUtf8)); 
			    mb+=cursize) 
			{
			cursize=utf8size(mb);
			/* Size differs? Pick the next! */
			if (cursize!=ncursize)
			    continue;
			if (!strncmp(i,(char *)mb,cursize))
			    {
			    i+=cursize-1;
			    p+=cursize-1;
			    found=true;
			    break;
			    }
			}    
		    if (found)
			continue;
		    /* 2.3. Check against an UTF-8 ranges: <start character> and <lenght of the range>.
		    Also char. is to be checked if it is a valid UTF-8 one */		

		    found=false;
	    	    for(unsigned char * mb=m_additionalUtf8range; 
			    (utf8size(mb)!=-1) && (mb<m_additionalUtf8range+sizeof(m_additionalUtf8range)); 
			    mb+=cursize+1) 
			{
			cursize=utf8size(mb);
			/* Size differs? Pick the next! */
			if ((cursize!=ncursize)||(!mb[cursize]))
			    continue;

			unsigned char uright[5]={0,0,0,0,0};
			strncpy((char *)uright,(char *)mb, cursize);
			
			if((uright[cursize-1]+mb[cursize]-1>0xff) && (cursize!=1))
			    {
			    uright[cursize-2]+=1;
			    }
			uright[cursize-1]=(uright[cursize-1]+mb[cursize]-1) % 0x100;
			
			if ((strncmp(i,(char *)mb,cursize)>=0) && (strncmp(i,(char *)uright,cursize)<=0))
			    {
			    i+=cursize-1;
			    p+=cursize-1;
			    found=true;
			    break;
			    }
			}    
		    if (found)
			continue;
		    }
		    
                /* invalid character! abort */
                return false;
        }

        /* too long? or not -- pointer arithmetic rocks */
        return (p < max);
}

class ModuleNationalChars : public Module
{
	private:
		InspIRCd* ServerInstance;
		std::string charset;
		unsigned char  m_additional[256],m_additionalUp[256],m_lower[256], m_upper[256];
		caller2<bool, const char*, size_t> * rememberer;
	public:
		ModuleNationalChars(InspIRCd* Me)
			: Module(Me)
		{
			rememberer=(caller2<bool, const char*, size_t> *)malloc(sizeof(rememberer));
			ServerInstance=Me;
			*rememberer =ServerInstance->IsNick;
			ServerInstance->IsNick=new lwbNickHandler(ServerInstance);
			OnRehash(NULL, "");
		}

		virtual void OnRehash(User* user, const std::string &parameter)
		{
			ConfigReader* conf = new ConfigReader(ServerInstance);
			charset = conf->ReadValue("nationalchars", "file", 0);
    			charset.insert(0,"../locales/");
			unsigned char * tables[7]={m_additional,m_additionalMB,m_additionalUp,m_lower,m_upper,
			    m_additionalUtf8,m_additionalUtf8range};
    			loadtables(charset,tables,7,5);
			delete conf;
		}

		virtual ~ModuleNationalChars()
		{
//			delete &ServerInstance->IsNick;
			ServerInstance->IsNick= *rememberer;
			free(rememberer);
		}

		virtual Version GetVersion()
		{
			return Version(0,2,1,0,VF_VENDOR,API_VERSION);
		}
		
		//make an array to check against it 8bit characters a bit faster. Whether allowed or uppercase (for your needs).

		void makereverse(unsigned char * from, unsigned  char * to, unsigned int cnt)
		{
	        memset(to, 0, cnt);
	        for(unsigned char * n=from; (*n) && ((*n)<cnt) && (n<from+cnt); n++) 
		    {
			to[*n]=1;
		    }
		}

/*so Bynets Unreal distribution stuff*/
void loadtables(std::string filename, unsigned char ** tables, char cnt, char faillimit)
{	
	const char *fname=filename.c_str();
        FILE *fd = fopen(fname, "r");
        if (!fd)
	    {
            ServerInstance->Logs->Log("m_nationalchars",DEBUG,"INTERNAL ERROR: loadtables() called for missing file: %s", fname);
	    return;
	    }
	for (char n=0;n<cnt;n++)
	    {
            memset(tables[n], 0, 256);
	    }

	for (char n=0;n<cnt;n++)
	    {
	    if (loadtable(fd, tables[n], 256) && (n<faillimit))
		{
                ServerInstance->Logs->Log("m_nationalchars",DEBUG,"INTERNAL ERROR: loadtables() called for illegal file: %s", fname);
		return;
		}
	    }
	
	makereverse(m_additional, m_reverse_additional, sizeof(m_additional));
/*	Do you need faster access to additional 8bit uppercase table? No? Oh, sorry :( Let's comment this out */
/*	makereverse(m_additionalUp, m_reverse_additionalUp, sizeof(m_additional)); */
/* ...some 1. expression to set custom CASEMAPPING array expected with m_lower 
and 2. to set custom CASEMAPPING parameter for InspIRCd::BuildISupport(); */

}

unsigned char symtoi(char *t,unsigned char base)
/* base = 16 for hexadecimal, 10 for decimal, 8 for octal ;) */
{
unsigned char tmp=0,current;
while ((*t)&&(*t!=' ')&&(*t!=13)&&(*t!=10)&&(*t!=','))
    {
    tmp*=base;
    current=lowermap[(unsigned char)*t];
    if (current>='a')
	current=current-'a'+10;
    else 
	current=current-'0';
    tmp+=current;
    t++;
    }
return tmp;
}

int loadtable(FILE *fd, unsigned char *chartable, unsigned int maxindex)
{
#define LINE_BUFFER_LENGTH      (0x10000)

        char *buf=(char *)malloc(LINE_BUFFER_LENGTH);
        unsigned int i=0;
        int fail=0;
        memset(buf, 0, LINE_BUFFER_LENGTH);
        fgets(buf, LINE_BUFFER_LENGTH-0x10, fd);
        if (buf[0]&&(buf[strlen(buf)-1]=='\n'))
	    buf[strlen(buf)-1] = 0;

        if (buf[0]=='.') /* simple plain-text string after dot */
        {
                i=strlen(buf+1);
                if (i>(maxindex+1)) i=maxindex+1;
                memcpy(chartable,buf+1,i);
        }else

        {
        char *p=buf;
        for (;;)
        {
                if (*p!='\'') /* decimal or hexadecimal char code */
                {
                        if (*p=='0') 
			{
			    if (p[1]=='x') 
			        chartable[i] = symtoi(p+2,16); /* hex with the leading "0x" */
			    else
                        	chartable[i] = symtoi(p+1,8);
			}
                        if (*p=='x') /* hex form */
                        {
                                chartable[i] = symtoi(p+1,16);
                        }else /* decimal form */
                        {
                                chartable[i] = symtoi(p,10);
                        }
                } else /* plain-text char between '' */
                {
                        if (*(p+1)=='\\')
                        {
                                chartable[i] = *(p+2);
                                p+=3;
                        }else
                        {
                                chartable[i] = *(p+1);
                                p+=2;
                        }
                }

		
                while (*p&& (*p!=',')&&(*p!=' ')&&(*p!=13)&&(*p!=10) ) p++;
                while (*p&&((*p==',')||(*p==' ')||(*p==13)||(*p==10))) p++;

		i++;
                
		if ((!*p)||(i>maxindex))break;
        }
        }

        free(buf);
        return fail;
}

};


MODULE_INIT(ModuleNationalChars)
