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

/* Spoon - process SOUP packets
   Copyright (c) 1999-2000 by Tom Torfs */

#define SPOON_C
#include "spoon.h"

/*...sconstants:0:*/
const char osstr[]
#if defined(OS2) || defined(__OS2__)
   = "OS/2";
#elif defined(__unix__) || defined(__linux__)
   = "Linux";
#else
   = "Win32";
#endif

const char weekday[][4]
   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

const char month[][4]
   = {"Jan","Feb","Mar","Apr","May","Jun",
      "Jul","Aug","Sep","Oct","Nov","Dec"};
/*...e*/
/*...sglobal variables:0:*/
FILE *logf = NULL;

char logfile[FILENAME_MAX]
   = "";

char configfile[FILENAME_MAX]
   = "spoon.cfg";

char newsgroupsfile[FILENAME_MAX]
   = "newsrc";

char soupdir[FILENAME_MAX]
   = "";

char hostname[HOSTBUFSIZE] = "";
char popuser[USERBUFSIZE]  = "";
char poppass[PASSBUFSIZE]  = "";
char pophost[HOSTBUFSIZE]  = "";
char smtphost[HOSTBUFSIZE] = "";
char nntphost[HOSTBUFSIZE] = "";
char nntpuser[USERBUFSIZE]  = "";
char nntppass[PASSBUFSIZE]  = "";

int fetchonly=0;
char *fetchonlyaddr[MAXFETCHONLY];
char *fetchonlyaddr2[MAXFETCHONLY];

int separatemail=0;

int transfer[MAILTYPES][TRANSFERTYPES] = {0};

int killreplies = 0;

int verbose=0;
int testmode=0;

int listnews = 0;
char listfilename[FILENAME_MAX];
unsigned listyear, listmonth, listday, listhour, listmin, listsec;

/*...e*/

/*...slogprintf:0:*/
void logprintf(char *s, ...)
{
   va_list ap;
   static char buf[200];

   va_start(ap,s);
   vsprintf(buf,s,ap);
   va_end(ap);

   printf("%s",buf);
   if (logf!=NULL)
      fprintf(logf,"%s",buf);
}
/*...e*/

/*...sexist:0:*/
int exist(char *name)
{
   return access(name,0)==0;
}
/*...e*/
/*...scheckdir:0:*/
void checkdir(char *dir)
{
   char *cp;
   char origchar;

#ifndef __unix__
   cp = strrchr(dir, '\\');
   if (cp==NULL)
#endif
      cp = strrchr(dir, '/');
   if (cp!=NULL)
   {
      origchar = *cp;
      *cp = '\0';
   }

   if (!exist(dir))
   {
      logprintf("FATAL ERROR: subdirectory %s does not exist\n", dir);
      exit(EXIT_FAILURE);
   }

   if (cp!=NULL)
      *cp = origchar;
}
/*...e*/
/*...sgenfname:0:*/
unsigned long genfname(const char *path, const char *ext, char *filename)
{
   unsigned long fileno;
   char *cp;

   for (fileno=0;;fileno++)
   {
      strcpy(filename, path);
      cp = filename + strlen(filename);
      while (cp>filename
#ifndef __unix__
             && cp[-1]!='\\'
#endif
             && cp[-1]!='/')
         *(--cp) = '\0';
      sprintf(cp,"%08lX.%s",fileno,ext);
      if (!exist(filename))
	 return fileno;
   }
}
/*...e*/

/*...smtime:0:*/
/* mtime(), returns a millisecond wall-clock time counter
   no particular effort is made to avoid wraparound (if a
   subsequent value is smaller than a previous one, ignore
   the measurement and keep the previous value);
   an unsigned long is used for accuracy (double would
   risk losing precision for large numbers) */
unsigned long mtime(void)
{
#if defined(__unix__) || defined(__linux__)
   struct timeval t;
   gettimeofday(&t, NULL);
   return (unsigned long)(t.tv_usec/1000)%1000
         +(unsigned long)(t.tv_sec%1000000)*1000;
#elif defined(OS2) || defined(__OS2__)
   DATETIME t;
   DosGetDateTime(&t);
   return (unsigned long)t.hours*3600000LU
                       + t.minutes*60000LU
                       + t.seconds*1000LU
                       + t.hundredths*10LU;
#else
   DWORD t;
   t = GetTickCount();
   return (unsigned long)t;
#endif
}
/*...e*/

/*...susage:0:*/
static void usage(void)
{
   printf("Usage: (all commands & options can be abbreviated to their first letter)\n\n");

   printf("spoon all            fetch/post email & news\n");
   printf("spoon email          fetch/post email only (no news)\n");
   printf("spoon news           fetch/post news only (no email)\n");
   printf("spoon fetch          fetch mail/news only (no posting)\n");
   printf("spoon post           post mail/news only (no fetching)\n");
   printf("(email/news and fetch/post may be combined)\n");

   printf("\nCommandline options:\n\n");

   printf("/config=<filename>   use <filename> as configuration file\n");
   printf("/list=<filename>     list newsgroups into file\n");
   printf("   [;<YYYY-MM-DD[;HH:MM:SS>]]   list new newsgroups since date\n");
   printf("/test                don't delete mail/files after processing\n");
   printf("/verbose             display additional information\n");

   exit(EXIT_FAILURE);
}
/*...e*/

/*...snextmessage:0:*/
long nextmessage(FILE *fp, char format)
{
   static const char RNEWSHEADER[] = "#! rnews ";
   char buffer[256];
   char *bp;
   long messagesize;
   long pos1, pos2;

   switch(format)
   {
   case 'u':
      if (fgets(buffer,sizeof buffer,fp) == NULL)
         return 0;
      if (strncmp(buffer,RNEWSHEADER,strlen(RNEWSHEADER))!=0)
         return -1;
      messagesize = atol(buffer+9);
      break;
   case 'm':
      if (fgets(buffer,sizeof buffer,fp) == NULL)
         return 0;
      if (strncmp(buffer,"From ",5)!=0)
         return -1;
      pos1 = ftell(fp);
      for (;;)
      {
         pos2 = ftell(fp);
         if (fgets(buffer,sizeof buffer,fp) == NULL)
         {
            pos2 = ftell(fp);
            break;
         }
         if (strncmp(buffer,"From ",5)==0)
            break;
      }
      messagesize = pos2 - pos1;
      fseek(fp, pos1, SEEK_SET);
      break;
   case 'M':
      while (fgetc(fp)=='\1') ;
      if (feof(fp))
         return 0;
      fseek(fp,-1L,SEEK_CUR);
      pos1 = ftell(fp);
      for (;;)
      {
         pos2 = ftell(fp);
         if (fgets(buffer,sizeof buffer,fp) == NULL)
         {
            pos2 = ftell(fp);
            break;
         }
         bp = strstr(buffer,"\1\1\1\1");
         if (bp!=NULL)
         {
            pos2 += bp-buffer;
            break;
         }
      }
      messagesize = pos2 - pos1;
      fseek(fp, pos1, SEEK_SET);
      break;
   case 'B':
   case 'b':
      messagesize = (long)fgetc(fp) << 24;
      messagesize |= (long)fgetc(fp) << 16;
      messagesize |= (long)fgetc(fp) << 8;
      messagesize |= (long)fgetc(fp);
      if (feof(fp))
         return 0;
      break;
   default:
      return -1;
   }

   return messagesize;
}
/*...e*/

int main(int argc, char *argv[])
{
/*...svariables:0:*/
   int i,j;
   int email, news, fetch, post;
   int optselected;
   char filename[FILENAME_MAX];
   char *cp, *cp2;
/*...e*/

/*...stitle:0:*/
   printf("Spoon-%s v%d.%02d - fetches your SOUP!\n",
           osstr,MAJORVERSION,MINORVERSION);
   printf("Copyright (c) 1999-2000 by Tom Torfs, all rights reserved\n\n");

#if defined(BETA)
   printf("Restricted beta version; compiled %s %s\n\n",__DATE__,__TIME__);
#elif defined(GAMMA)
   printf("Public gamma version; compiled %s %s\n\n",__DATE__,__TIME__);
#elif defined(BUGFIX)
   printf("Bugfix version; compiled %s %s\n\n",__DATE__,__TIME__);
#else
   printf("Release version; compiled %s %s\n\n",__DATE__,__TIME__);
#endif

   printf("This is FREEWARE. Read spoon.doc for more information.\n\n");
/*...e*/
/*...scheck commandline parameters:0:*/
   email = 1;
   news = 1;
   fetch = 1;
   post = 1;
   optselected = 0;
   for (i=1; i<argc; i++)
   {
      if (stricmp(argv[i],"all")==0
          || stricmp(argv[i],"a")==0)
      {
         email = 1;
         news = 1;
         optselected = 1;
      }
      else if (stricmp(argv[i],"email")==0
               || stricmp(argv[i],"e")==0)
      {
         email = 1;
         news = 0;
         optselected = 1;
      }
      else if (stricmp(argv[i],"news")==0
               || stricmp(argv[i],"n")==0)
      {
         email = 0;
         news = 1;
         optselected = 1;
      }
      else if (stricmp(argv[i],"fetch")==0
               || stricmp(argv[i],"f")==0)
      {
         fetch = 1;
         post = 0;
         optselected = 1;
      }
      else if (stricmp(argv[i],"post")==0
               || stricmp(argv[i],"p")==0)
      {
         fetch = 0;
         post = 1;
         optselected = 1;
      }
      else if (argv[i][0]=='/' || argv[i][0]=='-')
      {
         if (stricmp(argv[i]+1,"test")==0
             || stricmp(argv[i]+1,"t")==0)
            testmode = 1;
         else if (stricmp(argv[i]+1,"verbose")==0
             || stricmp(argv[i]+1,"v")==0)
            verbose = 1;
         else if (memicmp(argv[i]+1,"config=",7)==0)
         {
            strcpy(configfile,argv[i]+8);
         }
         else if (memicmp(argv[i]+1,"c=",2)==0)
         {
            strcpy(configfile,argv[i]+3);
         }
         else if (memicmp(argv[i]+1,"list=",5)==0
               || memicmp(argv[i]+1, "l=", 2)==0)
         {
            listnews = 1;
            listyear = 0;
            cp = strchr(argv[i] + 1, '=') + 1;   /* can't fail */
            cp2 = strchr(cp, ';');
            if (cp2!=NULL)
               *cp2++ = '\0';
            strcpy(listfilename,cp);
            if (cp2!=NULL)
            {
               if (sscanf(cp2, "%u-%u-%u", &listyear, &listmonth,
                                           &listday) != 3)
                  usage();
               cp = cp2;
               cp2 = strchr(cp, ';');
               if (cp2!=NULL)
               {
                  *cp2++ = '\0';
                  if (sscanf(cp2, "%u:%u:%u", &listhour, &listmin,
                                              &listsec) != 3)
                     usage();
               }
               else
               {
                  listhour = 0;
                  listmin = 0;
                  listsec = 0;
               }
            }
         }
         else
            usage();
      }
      else
         usage();
   }

   if (!optselected)
      usage();

   transfer[EMAIL][FETCH] = email && fetch;
   transfer[EMAIL][POST] = email && post;
   transfer[NEWS][FETCH] = news && fetch;
   transfer[NEWS][POST] = news && post;
/*...e*/

/*...sread configfile:0:*/
   /* global initializations must be done here (recursive function!) */

   if (readconfigfile(configfile,argv[0])!=0)
      return 1;

   if (soupdir[0]=='\0')
   {
      fprintf(stderr,"Error: SOUPDIR not defined in %s\n",configfile);
      return 1;
   }

   checkdir(soupdir);

   if (
#ifndef __unix__
       strchr(newsgroupsfile, '\\')==NULL &&
#endif
       strchr(newsgroupsfile, '/')==NULL)
   {
      strncpy(filename,argv[0],FILENAME_MAX-1);
#ifndef __unix__
      cp = strrchr(filename,'\\');
      if (cp==NULL)
#endif
         cp = strrchr(filename,'/');
      if (cp==NULL)
         cp = filename;
      else
         cp++;
      strcpy(cp,newsgroupsfile);
      strcpy(newsgroupsfile, filename);
   }
/*...e*/

/*...swrite log header:0:*/
   if (logfile[0]!='\0')
      logf = _fsopen(logfile,"a",SH_DENYWR);

   if (logf!=NULL)
   {
      time_t t;
      struct tm *tm;

      time(&t);
      tm = localtime(&t);
      if (ftell(logf)>0)
         fprintf(logf,"\n");
      fprintf(logf,"--- Spoon-%s v%d.%02d  %s %02d-%-3s-%04d %02d:%02d:%02d\n",
                    osstr,MAJORVERSION,MINORVERSION,
                    weekday[tm->tm_wday],
                    tm->tm_mday,month[tm->tm_mon],1900+tm->tm_year,
                    tm->tm_hour,tm->tm_min,tm->tm_sec);
   }
/*...e*/

/*...sdo work:0:*/
   if (sock_init() != 0)
   {
      fprintf(stderr,"Error: unable to initialize TCP/IP socket API\n");
      return EXIT_FAILURE;
   }
   atexit(sock_deinit);

   killreplies = -1;    /* unknown yet */

   if (transfer[EMAIL][POST])
      postemail();

   if (transfer[NEWS][POST])
      postnews();

   if (transfer[EMAIL][FETCH])
      fetchemail();

   if (transfer[NEWS][FETCH])
      fetchnews();

   if (!testmode && killreplies==1)
   {
      strcpy(filename, soupdir);
      strcat(filename, "REPLIES");
      remove(filename);
   }

   printf("\nDone.\n");
/*...e*/

/*...sclean up \38\ exit:0:*/
   if (logf!=NULL)
      fclose(logf);

   return EXIT_SUCCESS;
/*...e*/
}
