/*
        SoupGate Open-Source License

        Copyright (c) 1999 by Tom Torfs
        
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

/*...sversion info:0:*/
/* Spoon - process SOUP packets
   Copyright (c) 1999 by Tom Torfs */

#define MAJORVERSION 1
#define MINORVERSION 0

//#define BUGFIX

#define BETA
//#define GAMMA
/*...e*/

/*...sinclude files:0:*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#ifdef __unix__

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>

#define _fsopen(name,mode,flags) fopen(name,mode)
#define stricmp(a,b) strcasecmp(a,b)

#define DEFDIRSEP "/"

static int inline memicmp(const void *a, const void *b, size_t n)
{
   const unsigned char *p1=(const unsigned char *)a,
                       *p2=(const unsigned char *)b;
   int c1, c2;
   for (; n>0; n--)
   {
      c1 = tolower(*(p1++));
      c2 = tolower(*(p2++));
      if (c1!=c2)
         return c1 - c2;
   }
   return 0;
}

static void inline strupr(char *s)
{
   for (; *s; s++)
      *s = toupper(*s);
}

static void inline strlwr(char *s)
{
   for (; *s; s++)
      *s = tolower(*s);
}

#else

#include <direct.h>
#include <sys\stat.h>

#include <share.h>
#include <io.h>

#if defined(__OS2__) || defined(OS2)
#include <os2.h>
#else
#include <windows.h>
#endif

#define DEFDIRSEP "\\"

#endif

#include "socket.hpp"

/*...e*/

/*...senums:0:*/
enum {EMAIL, NEWS, MAILTYPES};

enum {NONE, FETCH, POST, BOTH, TRANSFERTYPES};

enum {PORT_FTP=21,  PORT_TELNET=23, PORT_SMTP=25,
      PORT_HTTP=80, PORT_POP3=110,  PORT_NNTP=119};
/*...e*/

/*...sdefines:0:*/
#define CONNECT_TIMEOUT 20
#define SOCK_TIMEOUT    90

#define USERBUFSIZE     64
#define PASSBUFSIZE     64
#define HOSTBUFSIZE    128
#define EMAILBUFSIZE   128

#define CPS_BYTES     5000

#define MAXFETCHONLY   512
/*...e*/

/* SPOON.C */

/*...slogprintf:0:*/
void logprintf(char *s, ...);
/*...e*/
/*...sexist:0:*/
int exist(char *name);
/*...e*/
/*...scheckdir:0:*/
void checkdir(char *dir);
/*...e*/
/*...sgenfname:0:*/
/* generate temporary name with path path and extension ext in filename
   if path is a file (i.e. does not end in a (back)slash) the path of it
   will be used (i.e. upto the last (back)slash)*/
unsigned long genfname(const char *path, const char *ext, char *filename);
/*...e*/

/*...smtime:0:*/
/* mtime(), returns a millisecond wall-clock time counter
   no particular effort is made to avoid wraparound (if a
   subsequent value is smaller than a previous one, ignore
   the measurement and keep the previous value);
   an unsigned long is used for accuracy (double would
   risk losing precision for large numbers) */
unsigned long mtime(void);
/*...e*/

/*...snextmessage:0:*/
/* scans next message (file pointer must be at start of message)
   format may be 'u', 'm', 'M', 'B' or 'b'
   returns message size in bytes; file pointer will be at start
   of message text; next call will expect the file pointer to
   be moved the return value bytes forward from that position
   returns 0 if no more messages in file or -1 on error */
long nextmessage(FILE *fp, char format);
/*...e*/

#ifndef SPOON_C
/*...sconstants:0:*/
extern const char osstr[];

extern const char weekday[][4];
extern const char month[][4];
/*...e*/
/*...sglobal variables:0:*/
extern FILE *logf;

extern char logfile[];
extern char configfile[];

extern char soupdir[];
extern char newsgroupsfile[];

extern char hostname[];
extern char popuser[];
extern char poppass[];
extern char pophost[];
extern char smtphost[];
extern char nntphost[];
extern char nntpuser[];
extern char nntppass[];

extern int fetchonly;
extern char *fetchonlyaddr[];
extern char *fetchonlyaddr2[];

extern int separatemail;

extern int transfer[][TRANSFERTYPES];

extern int killreplies;

extern int verbose;
extern int testmode;

extern int listnews;
extern char listfilename[];
extern unsigned listyear, listmonth, listday, listhour, listmin, listsec;
/*...e*/
#endif

/* CONFIG.C */

/*...sconfig I\47\O functions:0:*/
void configwarn(const char *text, long lineno, const char *fname);

int readconfigfile(char *configfile, char *argv0);
/*...e*/

/* POP.C */

/*...sfetchemail:0:*/
/* fetches email from the POP server
   returns nonzero when succesful */
int fetchemail(void);
/*...e*/

/* NNTP.C */

/*...sfetchnews:0:*/
/* fetches news from the NNTP server
   returns nonzero when succesful */
int fetchnews(void);
/*...e*/
/*...spostnews:0:*/
/* posts news to the NNTP server
   returns nonzero when succesful */
int postnews(void);
/*...e*/

/* SMTP.C */

/*...spostemail:0:*/
/* posts email to the SMTP server
   returns nonzero when succesful */
int postemail(void);
/*...e*/
