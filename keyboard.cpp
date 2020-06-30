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

/* Spoon keyboard functions - not used for now */

#include "spoon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct termios savesettings;
static int initialized = 0;

/* set non-blocking mode, returns 0 on success */
int init_nonblock(void)
{
#ifdef __unix__
   struct termios settings;
   int rc;

   if (!isatty(STDIN_FILENO))
      return -1;

   rc = tcgetattr(STDIN_FILENO, &savesettings);
   if (rc < 0)
      return rc;

   memcpy(&settings, &savesettings, sizeof settings);

   settings.c_lflag &= ~(ICANON|ECHO);
   settings.c_cc[VMIN] = 0;
   settings.c_cc[VTIME] = 0;
   rc = tcsetattr(STDIN_FILENO, TCSANOW, &settings);
   if (rc < 0)
      return rc;
#endif

   initialized = 1;
 
   return 0;
}

/* restore normal mode (void to allow registering with atexit()) */
void deinit_nonblock(void)
{
#ifdef __unix__
   if (initialized)
      (void)tcsetattr(STDIN_FILENO, TCSANOW, &savesettings);
#endif
}

/* non-blocking getchar()
   returns character, or EOF if none pending */
int getchar_nonblock(void)
{
#ifdef __unix__
   if (initialized)
      return getchar();
   else
      return EOF;    /* perhaps stdin is not a terminal device */
#else
   if (kbhit())
      return getch();
   else
      return EOF;
#endif
}

/* check for abort; returns nonzero if aborted */
int checkabort(void)
{
   int ch;

   ch = getchar_nonblock();
   if (ch==27)
   {
      printf("\nAbort - are you sure ? (Y/N) ");
      do
         ch = toupper(getchar_nonblock());
      while (ch!='Y' && ch!='N');
      printf("%c\n", ch);
      if (ch=='Y')
         return 1;
   }
   else if (ch!=EOF)
   {
      printf("\nUnknown keypress %c (%d) --- press ESC to abort\n",
             ch, ch);
   }

   return 0;
}
