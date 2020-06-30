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

/* Spoon SMTP functions */

#include "spoon.h"

enum {SMTP_OK, SMTP_ERR};

#define MAXEMAILCOUNT 500
static char *emaillist[MAXEMAILCOUNT];
static int emailcount = 0;
static char *fromemail = NULL;

/*...sgetsmtpanswer:0:*/
/* reads & analyzes SMTP answer
   SMTP_OK (1) = 1XX (positive preliminary), 2XX (positive completion)
                 or 3XX (positive intermediate)
   SMTP_ERR (0) = 4XX (negative transient), 5XX (negative permanent)
   if quiet is set to nonzero, error messages will not be displayed
   -1 = error */
static int getsmtpanswer(Socket &sock, int quiet)
{
   int rval;

   rval = sock.RecvTeol();
   if (rval==-1)
   {
      net_error("Receive");
      return -1;
   }

   if (rval==0)
   {
      logprintf("Timeout - SMTP host did not answer\n");
      return -1;
   }

   if (sock.szOutBuf[0]>='1' && sock.szOutBuf[0]<='3')
      return SMTP_OK;

   if (!quiet)
   {
      /* no newline, sock.szOutBuf ends in \r\n */
      logprintf("SMTP host returned error: %s", sock.szOutBuf);
   }

   return SMTP_ERR;
}
/*...e*/

/*...sisdelimiter:0:*/
static int isdelimiter(int c)
{
   return (c=='\0' || c==' ' || c=='\t'
           || c=='<' || c=='>' || c=='(' || c==')'
           || c=='\r' || c=='\n');
}
/*...e*/
/*...sprocessemail:0:*/
/* scan email address(es) from s into emaillist, updating emailcount
   (unless isfrom is 1, in which case will be written into fromemail)
   changes s!*/
static void processemail(char *s, int isfrom)
{
   char *cp,*cp2,*ep;
   int inlevel,qlevel;
   char *addrpos;
   int addrlen;

   cp = s;
   while (emailcount<MAXEMAILCOUNT && cp!=NULL)
   {
      /* delimit current address */
      inlevel = 0;
      qlevel = 0;
      for (cp2=cp; *cp2!=',' || inlevel!=0 || qlevel!=0; cp2++)
      {
         if (*cp2=='\0')
         {
            cp2 = NULL;
            break;
         }
         if (*cp2=='\"' || *cp2=='\'')
            qlevel = !qlevel;
         else if (*cp2=='(' || *cp2=='<')
            inlevel++;
         else if (*cp2==')' || *cp2=='>')
            inlevel--;
      }
      if (cp2!=NULL)
        *(cp2++) = '\0';

      /* find at-sign of e-mail address */
      ep = strchr(cp,'@');
      if (ep!=NULL)
      {
         /* scan till left delimiter */
         while (ep>cp)
         {
            if (isdelimiter(ep[-1]))
               break;
            ep--;
         }

         /* scan until right delimiter */
         addrlen = 0;
         addrpos = ep;
         while (!isdelimiter(*ep))
         {
            addrlen++;
            ep++;
         }

         if (isfrom)
         {
            if (fromemail != NULL)
               delete fromemail;
            fromemail = new char[addrlen+1];
            if (fromemail != NULL)
            {
               memcpy(fromemail, addrpos, addrlen);
               fromemail[addrlen] = '\0';
            }
         }
         else
         {
            emaillist[emailcount] = new char[addrlen+1];
            if (emaillist[emailcount] != NULL)
            {
               memcpy(emaillist[emailcount], addrpos, addrlen);
               emaillist[emailcount][addrlen] = '\0';
               emailcount++;
            }
         }
      }
   
      cp = cp2;
   }
}
/*...e*/

/*...spostemail:0:*/
int postemail(void)
{
   Socket sock;
   char filename[FILENAME_MAX];
   char buffer[256];
   char *cp, *cp2;
   char format;
   FILE *fp = NULL, *fp2 = NULL;
   int mail;
   long msgsize, nextmsgpos;
   int i;
   int into;
   int keepmsg;

   if (hostname[0]=='\0')
   {
      logprintf("Your hostname not defined\n");
      goto error;
   }

   if (smtphost[0]=='\0')
   {
      logprintf("SMTP hostname not defined\n");
      goto error;
   }

   strcpy(filename, soupdir);
   strcat(filename, "REPLIES");
   fp = _fsopen(filename, "rb", SH_DENYNO);
   if (fp==NULL)
   {
      if (verbose)
         logprintf("+ No REPLIES file found, not posting any email\n");
      return 1;
   }

   mail = 0;

   while (fgets(buffer, sizeof buffer, fp) != NULL)
   {
      cp = strchr(buffer, '\t');
      if (cp!=NULL)
      {
         *(cp++) = '\0';
         cp2 = strchr(cp, '\t');
         if (cp2!=NULL)
            *(cp2++) = '\0';
      }
      if (cp==NULL || cp2==NULL)
      {
         logprintf("! Error in REPLIES file\n");
         killreplies = 0;
         continue;
      }
      if (stricmp(cp, "mail") != 0)
      {
         if (stricmp(cp, "news") != 0)
         {
            logprintf("! Unknown message kind in REPLIES\n");
            killreplies = 0;
         }
         else if (!transfer[NEWS][POST])
         {
            logprintf("News found --- will keep REPLIES file\n");
            killreplies = 0;
         }
         continue;
      }
      format = cp2[0];
      if (format!='u' && format!='m' && format!='M'
          && format!='B' && format!='b')
      {
         logprintf("! Unknown message format in REPLIES\n");
         killreplies = 0;
         continue;
      }

      if (!mail)
      {
         if (sock.Create() == -1)
         {
            net_error("Open socket");
            goto error;
         }

         sock.ulTimeout = CONNECT_TIMEOUT;

         logprintf("Posting mail to %s...\n", smtphost);

         if (verbose)
            logprintf("+ Connecting to SMTP host %s...\n", smtphost);

         if (sock.Connect(smtphost, PORT_SMTP) == -1)
         {
            logprintf("Cannot connect to host %s at port %d\n",
                      smtphost, PORT_SMTP);
            goto error;
         }

         if (verbose)
            logprintf("+ Connected, sending HELO...\n");

         if (getsmtpanswer(sock, 0) != SMTP_OK)
            goto error;

         sprintf(sock.szOutBuf, "HELO %s\r\n", hostname);

         if (sock.Send(sock.szOutBuf) == -1)
         {
            net_error("Send");
            goto quit;
         }

         if (getsmtpanswer(sock, 0) != SMTP_OK)
            goto quit;

         sock.ulTimeout = SOCK_TIMEOUT;
 
         mail = 1;
      }

      strcpy(filename, soupdir);
      strcat(filename, buffer);
      strcat(filename, ".MSG");
      fp2 = _fsopen(filename, "rb", SH_DENYNO);
      if (fp2==NULL)
      {
         logprintf("! Cannot open message file %s\n", filename);
         continue;
      }

      keepmsg = 0;

      nextmsgpos = 0;
      for (;;)
      {
         fseek(fp2, nextmsgpos, SEEK_SET);

         msgsize = nextmessage(fp2, format);
         if (msgsize==0)
            break;
         if (msgsize==-1)
         {
            logprintf("! Error in message file %s\n", filename);
            keepmsg = 1;
            break;   /* still terminate message etc. */
         }
         nextmsgpos = ftell(fp2) + msgsize;

         emailcount = 0;
         fromemail = NULL;

         into = 0;
         while (ftell(fp2) < nextmsgpos)
         {
            if (fgets(buffer, sizeof buffer, fp2) == NULL)
               break;
            if (buffer[0]=='\n')
            {
               /* only interested in headers */
               break;
            }
            else if (memicmp(buffer, "From:", 5) == 0)
            {
               processemail(buffer+5, 1);
               into = 0;
            }
            else if (memicmp(buffer, "To:", 3) == 0)
            {
               processemail(buffer+3, 0);
               into = 1;
            }
            else if (memicmp(buffer, "Cc:", 3) == 0)
            {
               processemail(buffer+3, 0);
               into = 1;
            }
            else if (memicmp(buffer, "Bcc:", 4) == 0)
            {
               processemail(buffer+4, 0);
               into = 1;
            }
            else if (isspace(buffer[0]) && into)
            {
               processemail(buffer, 0);
               /* into still 1 */
            }
            else
            {
               into = 0;
            }
         }

         if (fromemail==NULL)
         {
            logprintf("! No From address specified in message - skipping\n");
            for (i=0; i<emailcount; i++)
               delete emaillist[i];
            keepmsg = 1;
            continue;
         }

         if (emailcount==0)
         {
            logprintf("! No To address specified in message - skipping\n");
            if (fromemail!=NULL)
               delete fromemail;
            keepmsg = 1;
            continue;
         }

         for (i=0; i<emailcount; i++)
         {
            if (i==0 || separatemail)
            {
               logprintf("Mailing from %s\n", fromemail);
               sprintf(sock.szOutBuf, "MAIL FROM:%s\r\n", fromemail);
               if (sock.Send(sock.szOutBuf) == -1)
               {
                  net_error("Send");
                  if (fromemail!=NULL)
                     delete fromemail;
                  for (i=0; i<emailcount; i++)
                     delete emaillist[i];
                  goto quit;
               }
               if (getsmtpanswer(sock, 0) != SMTP_OK)
               {
                  logprintf("! Posting to SMTP server failed\n");
                  if (fromemail!=NULL)
                     delete fromemail;
                  for (i=0; i<emailcount; i++)
                     delete emaillist[i];
                  goto quit;
               }
            }
            logprintf("Mailing to %s\n", emaillist[i]);;
            sprintf(sock.szOutBuf, "RCPT TO:%s\r\n", emaillist[i]);
            if (sock.Send(sock.szOutBuf) == -1)
            {
               net_error("Send");
               if (fromemail!=NULL)
                  delete fromemail;
               for (i=0; i<emailcount; i++)
                  delete emaillist[i];
               goto quit;
            }
            if (getsmtpanswer(sock, 0) != SMTP_OK)
               logprintf("! Recipient refused by SMTP server\n");
         }

         if (fromemail!=NULL)
            delete fromemail;
         for (i=0; i<emailcount; i++)
            delete emaillist[i];

         if (sock.Send("DATA\r\n") == -1)
         {
            net_error("Send");
            goto quit;
         }
         if (getsmtpanswer(sock, 0) != SMTP_OK)
         {
            logprintf("! SMTP server won't accept message\n");
            goto quit;
         }

         fseek(fp2, nextmsgpos - msgsize, SEEK_SET);

         while (ftell(fp2) < nextmsgpos)
         {
            if (fgets(buffer, sizeof buffer - 2, fp2) == NULL)
               break;
            cp = strchr(buffer, '\n');
            if (cp!=NULL && (cp==buffer || cp[-1]!='\r'))
            {
               cp[0] = '\r';
               cp[1] = '\n';
               cp[2] = '\0';
            }
            if (buffer[0]=='.')
            {
               memmove(buffer+1, buffer, strlen(buffer) + 1);
               buffer[0] = '.';  /* redundant */
            }
            if (sock.Send(buffer) == -1)
            {
               net_error("Send");
               if (fromemail!=NULL)
                  delete fromemail;
               for (i=0; i<emailcount; i++)
                  delete emaillist[i];
               goto quit;
            }
         }

         if (sock.Send(".\r\n") == -1)
         {
            net_error("Send");
            goto quit;
         }

         if (getsmtpanswer(sock, 0) != SMTP_OK)
         {
            logprintf("! SMTP server did not accept message\n");
            continue;
         }
      }

      fclose(fp2);

      if (!testmode && !keepmsg)
         remove(filename);
   }

   fclose(fp);

   if (mail)
   {
      if (verbose)
         logprintf("+ Signing off...\n");

      if (sock.Send("QUIT\r\n") == -1)
      {
         net_error("Send");
         goto quit;
      }

      if (getsmtpanswer(sock, 0) != SMTP_OK)
         goto error;

      sock.Close();

      fclose(fp);

      if (verbose)
         logprintf("+ SMTP session successfully terminated\n");
   }
   else
   {
      if (verbose)
         logprintf("+ No mail messages to be posted\n");
   }

   if (killreplies == -1)
      killreplies = 1;

   return 1;

quit:

   if (sock.Send("QUIT\r\n") == -1)
   {
      net_error("Send");
      goto quit;
   }

   getsmtpanswer(sock, 0);

error:
   killreplies = 0;

   if (fp!=NULL)
      fclose(fp);
   if (fp2!=NULL)
      fclose(fp2);
   if (sock.iSock != -1)
      sock.Close();
   logprintf("(skipping email post)\n");
   return 0;
}
/*...e*/
