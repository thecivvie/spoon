/*
        SoupGate Open-Source License

        Copyright (c) 1999-2000 by Tom Torfs
        
        The SoupGate software and its documentation may freely be distributed
        and used for all purposes, provided no fee is charged other than to
        cover administration and distribution costs, in other words it may
        not be sold for profit.

        The SoupGate software may freely be modified. Source code need
        not be made available for modified versions or derived programs,
        but if it is not, at least a copy of this license must be included
        in the program or its documentation.

        Modified source code may be made available, provided this license
        remains included, intact and unmodified, and the fact that changes
        were made must clearly be identified in both the source code and
        documentation, and a reference must be provided as to where the
        original, unmodified version can be obtained.

        DISCLAIMER:  THE AUTHOR EXCLUDES ANY AND ALL IMPLIED
        WARRANTIES,  INCLUDING WARRANTIES OF MERCHANTABILITY
        AND FITNESS FOR A PARTICULAR PURPOSE.     THE AUTHOR
        MAKES NO WARRANTY OR REPRESENTATION,  EITHER EXPRESS
        OR IMPLIED,   WITH RESPECT TO THIS SOFTWARE,     ITS
        QUALITY,  PERFORMANCE,  MERCHANTABILITY,  OR FITNESS
        FOR A PARTICULAR PURPOSE.   THE AUTHOR SHALL HAVE NO
        LIABILITY FOR SPECIAL,  INCIDENTAL, OR CONSEQUENTIAL
        DAMAGES ARISING OUT OF  OR RESULTING FROM THE USE OR
        MODIFICATION OF THIS SOFTWARE.

	End of License
*/

/* Spoon configuration functions */

#include "spoon.h"

/*...sconfigwarn:0:*/
void configwarn(const char *text, long lineno, const char *fname)
{
   fprintf(stderr,"Warning: %s in line %ld of %s\n\n",text,lineno,fname);
}
/*...e*/

/*...sscanyesno:0:*/
static void configscanyesno(char *cp, long lineno, char *filename,
               int *yesno)
{
   if (stricmp(cp,"Yes")==0)
      *yesno = 1;
   else if (stricmp(cp,"No")==0)
      *yesno = 0;
   else
   {
      *yesno = 0;
      configwarn("invalid Yes/No parameter",lineno,filename);
   }
}
/*...e*/
/*...sscantext:0:*/
static void configscantext(char *s, long lineno, char *filename,
              char *text, int textbufsize)
{
   strncpy(text,s,textbufsize-1);
   text[textbufsize-1] = '\0';
}
/*...e*/
/*...sscandir:0:*/
static void configscandir(char *s, long lineno, char *filename,
             char *dir)
{
   int len;

   strncpy(dir,s,FILENAME_MAX-1);
   dir[FILENAME_MAX-1] = '\0';

   len = strlen(dir);
   if (
#ifndef __unix__
       dir[len-1]!='\\' &&
#endif
       dir[len-1]!='/' && len<FILENAME_MAX-1)
      strcat(dir, DEFDIRSEP);
}
/*...e*/

/*...sreadconfigfile:0:*/
int readconfigfile(char *configfile, char *argv0)
{
   char configname[FILENAME_MAX]; /* for recursion */
   char buffer[256];
   FILE *fp;
   int len;
   char *cp, *cp2;
   long lineno;

   if (
#ifndef __unix__
       strchr(configfile,'\\')==NULL &&
#endif
       strchr(configfile,'/')==NULL)
   {
      strncpy(configname,argv0,FILENAME_MAX-1);
#ifndef __unix__
      cp = strrchr(configname,'\\');
      if (cp==NULL)
#endif
         cp = strrchr(configname,'/');
      if (cp==NULL)
         cp = configname;
      else
         cp++;
      strcpy(cp,configfile);
   }
   else
      strcpy(configname,configfile);

   fp = _fsopen(configname,"r",SH_DENYNO);
   if (fp==NULL)
   {
      fprintf(stderr,"Error: can't read %s\n",configname);
      return 1;
   }

   lineno = 0;
   for (;;)
   {
      if (!fgets(buffer, sizeof buffer, fp))
         break;
      lineno++;
      len = strlen(buffer);
      /* remove both CR and LF in case DOS file on Unix */
      while (len>0
             && (buffer[len-1]=='\n' || buffer[len-1]=='\r'))
         len--;
      buffer[len] = '\0';
      if (buffer[0]==';' || buffer[0]=='\0')
         continue;
      cp = strchr(buffer,' ');
      if (cp==NULL)
         cp = buffer + strlen(buffer);
      else
      {
         while (*cp==' ')
            *(cp++) = '\0';
      }
      if (stricmp(buffer,"HOSTNAME")==0)
      {
         configscantext(cp,lineno,configname,hostname,HOSTBUFSIZE);
      }
      else if (stricmp(buffer,"POPUSER")==0)
      {
         configscantext(cp,lineno,configname,popuser,USERBUFSIZE);
      }
      else if (stricmp(buffer,"POPHOST")==0)
      {
         configscantext(cp,lineno,configname,pophost,HOSTBUFSIZE);
      }
      else if (stricmp(buffer,"POPPASS")==0)
      {
         configscantext(cp,lineno,configname,poppass,PASSBUFSIZE);
      }
      else if (stricmp(buffer,"SMTPHOST")==0)
      {
         configscantext(cp,lineno,configname,smtphost,HOSTBUFSIZE);
      }
      else if (stricmp(buffer,"NNTPHOST")==0)
      {
         configscantext(cp,lineno,configname,nntphost,HOSTBUFSIZE);
      }
      else if (stricmp(buffer,"NNTPUSER")==0)
      {
         configscantext(cp,lineno,configname,nntpuser,USERBUFSIZE);
      }
      else if (stricmp(buffer,"NNTPPASS")==0)
      {
         configscantext(cp,lineno,configname,nntppass,PASSBUFSIZE);
      }
      else if (stricmp(buffer,"SOUPDIR")==0)
      {
         configscandir(cp,lineno,configname,soupdir);
      }
      else if (stricmp(buffer,"NEWSGROUPS")==0)
      {
         configscantext(cp,lineno,configname,newsgroupsfile,FILENAME_MAX);
      }
      else if (stricmp(buffer,"FETCHONLY")==0)
      {
         if (fetchonly>=MAXFETCHONLY)
         {
            configwarn("too many FetchOnly keywords",lineno,configname);
            continue;
         }
         cp2 = strchr(cp, ' ');
         if (cp2!=NULL)
            *(cp2++) = 0;
         fetchonlyaddr[fetchonly] = new char[strlen(cp)+1];
         if (fetchonlyaddr[fetchonly]==NULL)
         {
            configwarn("out of memory",lineno,configname);
            continue;
         }
         strcpy(fetchonlyaddr[fetchonly], cp);
         if (cp2!=NULL)
         {
            fetchonlyaddr2[fetchonly] = new char[strlen(cp2)+1];
            if (fetchonlyaddr2[fetchonly]==NULL)
            {
               configwarn("out of memory",lineno,configname);
               continue;
            }
            strcpy(fetchonlyaddr2[fetchonly], cp2);
         }
         else
            fetchonlyaddr2[fetchonly] = NULL;
         fetchonly++;
      }
      else if (stricmp(buffer,"LOGFILE")==0)
      {
         configscantext(cp,lineno,configname,logfile,FILENAME_MAX);
         if (
#ifndef __unix__
             strchr(logfile,'\\')==NULL &&
#endif
             strchr(logfile,'/')==NULL)
         {
            strncpy(logfile,argv0,FILENAME_MAX-1);
#ifndef __unix__
            cp2 = strrchr(logfile,'\\');
            if (cp2==NULL)
#endif
               cp2 = strrchr(logfile,'/');
            if (cp2==NULL)
               cp2 = logfile;
            else
               cp2++;
            *cp2 = '\0';
            configscantext(cp,lineno,configname,cp2,FILENAME_MAX - strlen(logfile));
         }
      }
      else if (stricmp(buffer,"SMTPSEPARATEMAIL")==0)
      {
         configscanyesno(cp,lineno,configname,&separatemail);
      }
      else if (stricmp(buffer,"INCLUDE")==0)
      {
         /* must be last, buffer destroyed on recursive call */
         readconfigfile(cp,argv0);
      }
      else
      {
         configwarn("unknown keyword",lineno,configname);
         continue;
      }
   }

   fclose(fp);
   return 0;
}
/*...e*/
