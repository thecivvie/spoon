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

/* Spoon POP functions */

#include "spoon.h"

enum {POP_OK, POP_ERR};

typedef struct {long msgnum;
                long msgsize;
               } msginfo;

/*...sisdelimiter:0:*/
static int isdelimiter(int c)
{
   return (c=='\0' || c==' ' || c=='\t'
           || c=='<' || c=='>' || c=='(' || c==')'
           || c=='\r' || c=='\n'
           || c=='"');
}
/*...e*/
/*...sprocessemail:0:*/
/* scan email address from s into email
   changes s!*/
static void processemail(char *s, char *email, size_t maxlen)
{
   char *ep;
   char *addrpos;
   int addrlen;

   /* find at-sign of e-mail address */
   ep = strchr(s,'@');
   if (ep==NULL)
   {
      strncpy(email, "unknown", maxlen);
      return;
   }

   /* scan till left delimiter */
   while (ep>s)
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

   if (addrlen >= maxlen)
      addrlen = maxlen - 1;

   memcpy(email, addrpos, addrlen);
   email[addrlen] = '\0';
}
/*...e*/

/*...sgetpopanswer:0:*/
/* reads & analyzes POP answer
   POP_OK (1) = +OK
   POP_ERR (0) = -ERR
   -1 = error/unknown */
static int getpopanswer(Socket &sock)
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
      logprintf("Timeout - POP3 host did not answer\n");
      return -1;
   }

   if (memicmp(sock.szOutBuf, "+OK", 3) == 0)
      return POP_OK;

   if (memicmp(sock.szOutBuf, "-ERR", 4) == 0)
   {
      /* no newline, sock.szOutBuf ends in \r\n */
      logprintf("POP3 host returned error: %s", sock.szOutBuf);
      return POP_ERR;
   }

   logprintf("POP3 host did not send valid answer\n");
   return -1;
}
/*...e*/

/*...sfetchemail:0:*/
int fetchemail(void)
{
   Socket sock;
   long nummessages, numbytes;
   long size, pos1, pos2;
   long filenum;
   msginfo *msglist;
   char filename[FILENAME_MAX];
   char fromemail[EMAILBUFSIZE];
   char toemail[EMAILBUFSIZE];
   FILE *fp;
   char *cp, *cp2;
   int i,j,ok;
   int inbody;
   long bytes;
   int percent, ppercent;
   unsigned long cpsstart, cpsend, totalcpsstart;
   unsigned long cpsbytecount, totalcpsbytecount;
   unsigned cps, pcps;
   unsigned totalcps, ptotalcps;

   if (pophost[0]=='\0'
       || popuser[0]=='\0'
       || poppass[0]=='\0')
   {
      logprintf("POP3 hostname/username/password not defined\n");
      goto error;
   }

   if (sock.Create() == -1)
   {
      net_error("Open socket");
      goto error;
   }

   sock.ulTimeout = CONNECT_TIMEOUT;

   logprintf("Fetching email from %s...\n", pophost);

   if (verbose)
      logprintf("+ Connecting to POP3 host %s...\n", pophost);

   if (sock.Connect(pophost, PORT_POP3) == -1)
   {
      logprintf("Cannot connect to host %s at port %d\n", pophost, PORT_POP3);
      goto error;
   }

   if (getpopanswer(sock) != POP_OK)
      goto error;

   sock.ulTimeout = SOCK_TIMEOUT;

   if (verbose)
      logprintf("+ Sending USER command...\n");

   sprintf(sock.szOutBuf, "USER %s\r\n", popuser);
   if (sock.Send(sock.szOutBuf) == -1)
   {
      net_error("Send");
      goto quit;
   }

   if (getpopanswer(sock) != POP_OK)
      goto quit;

   if (verbose)
      logprintf("+ Sending PASS command...\n");

   sprintf(sock.szOutBuf, "PASS %s\r\n", poppass);
   if (sock.Send(sock.szOutBuf) == -1)
   {
      net_error("Send");
      goto quit;
   }

   if (getpopanswer(sock) != POP_OK)
      goto quit;

   if (verbose)
      logprintf("+ Getting message information...\n");

   if (sock.Send("STAT\r\n") == -1)
   {
      net_error("Send");
      goto quit;
   }

   if (getpopanswer(sock) != POP_OK)
      goto quit;

   nummessages = 0;
   numbytes = 0;
   sscanf(sock.szOutBuf+4, "%ld %ld", &nummessages, &numbytes);

   if (nummessages==0)
   {
      logprintf("There are no messages on the POP3 server.\n");
   }
   else
   {
      logprintf("There are %ld messages (%ld bytes) on the POP3 server.\n",
             nummessages, numbytes);

      msglist = new msginfo[nummessages];
      if (msglist==NULL)
      {
         logprintf("Out of memory reading message list\n");
         goto quit;
      }

      if (sock.Send("LIST\r\n") == -1)
      {
         net_error("Send");
         goto quit;
      }

      if (getpopanswer(sock) != POP_OK)
         goto quit;

      /* one more, last one is terminating dot */
      for (i=0; i<=nummessages; i++)
      {
         if (sock.RecvTeol() <= 0)
         {
            logprintf("Unable to retrieve list of messages\n");
            goto quit;
         }
         if (i < nummessages)   /* not dot */
         {
            msglist[i].msgnum = i+1;
            msglist[i].msgsize = 0;
            sscanf(sock.szOutBuf, "%ld %ld",
                   &msglist[i].msgnum, &msglist[i].msgsize);
         }
         else if (strcmp(sock.szOutBuf, ".\r\n") != 0)
         {
            logprintf("List of messages not properly terminated\n");
            goto quit;
         }
      }

      filenum = genfname(soupdir, "MSG", filename);
      fp = _fsopen(filename, "wb", SH_DENYWR);
      if (fp==NULL)
      {
         logprintf("Cannot write to %s\n", filename);
         goto quit;
      }

      totalcpsstart = mtime();
      totalcpsbytecount = 0;
      totalcps = 0;
      ptotalcps = 0;

      for (i=0; i<nummessages; i++)
      {
         if (msglist[i].msgsize == 0)
            continue;
         if (fetchonly)
         {
            printf("Examining headers of message %d of %d...\n", i+1, nummessages);
            sprintf(sock.szOutBuf, "TOP %ld 0\r\n", msglist[i].msgnum);
            if (sock.Send(sock.szOutBuf) == -1)
            {
               net_error("Send");
               fclose(fp);
               goto quit;
            }
            if (getpopanswer(sock) != POP_OK)
            {
               fclose(fp);
               goto quit;
            }
            strcpy(fromemail, "unknown");
            strcpy(toemail, "unknown");
            for (;;)
            {
               if (sock.RecvTeol() <= 0)
               {
                  logprintf("Unable to fetch headers\n");
                  fclose(fp);
                  goto quit;
               }
               if (strcmp(sock.szOutBuf, ".\r\n") == 0)
                  break;
               cp = sock.szOutBuf;
               if (*cp=='.')
                  cp++;
               cp2 = strstr(cp, "\r\n");
               if (cp2!=NULL)
                  strcpy(cp2, "\n");
               if (memicmp(cp, "From:", 5) == 0)
               {
                  cp2 = strchr(cp, '\n');
                  if (cp2!=NULL)
                     *cp2 = '\0';
                  cp2 = cp + 5;
                  while (isspace(*cp2))
                     cp2++;
                  printf("\r");
                  logprintf("* Message from %s\n", cp2);
                  printf("%d%%", percent);
                  fflush(stdout);
                  /* will change buffer */
                  processemail(cp2, fromemail, sizeof fromemail);
               }
               else if (memicmp(cp, "To:", 3) == 0)
               {
                  cp2 = strchr(cp, '\n');
                  if (cp2!=NULL)
                     *cp2 = '\0';
                  cp2 = cp + 3;
                  while (isspace(*cp2))
                     cp2++;
                  printf("\r");
                  logprintf("* Message to %s\n", cp2);
                  printf("%d%%", percent);
                  fflush(stdout);
                  /* will change buffer */
                  processemail(cp2, toemail, sizeof toemail);
               }
            }
            ok = 0;
            for (j=0; j<fetchonly; j++)
            {
            	printf("DEBUG from [%s] to [%s]\n", fetchonlyaddr[j], fetchonlyaddr2[j]);
               if (strstr(fromemail, fetchonlyaddr[j])!=NULL
                   && (fetchonlyaddr2[j]==NULL || strstr(toemail, fetchonlyaddr2[j])!=NULL))
               {
                  ok = 1;
                  break;
               }
               if (fetchonlyaddr[j][0]=='!'
                   && strstr(fromemail, fetchonlyaddr[j]+1)==NULL
                   && (fetchonlyaddr2[j]==NULL || strstr(toemail, fetchonlyaddr2[j])!=NULL))
               {
                  ok = 1;
                  break;
               }
            }
            if (!ok)
            {
               logprintf("From %s to %s: does not meet fetch conditions --- skipping\n",
                         fromemail, toemail);
               continue;
            }
         }
         printf("Fetching message %d of %d (%ld bytes)...\n",
                i+1, nummessages, msglist[i].msgsize);
         sprintf(sock.szOutBuf, "RETR %ld\r\n", msglist[i].msgnum);
         if (sock.Send(sock.szOutBuf) == -1)
         {
            net_error("Send");
            fclose(fp);
            goto quit;
         }
         if (getpopanswer(sock) != POP_OK)
         {
            fclose(fp);
            goto quit;
         }
         size = htonl(msglist[i].msgsize);
         pos1 = ftell(fp);
         fwrite(&size, 4, 1, fp);
         inbody = 0;
         bytes = 0;
         ppercent = -1;
         cpsstart = mtime();
         cpsbytecount = 0;
         cps = 0;
         pcps = 0;
         for (;;)
         {
            percent = (int)(bytes * 100 / msglist[i].msgsize);
            if (percent!=ppercent || cps!=pcps || totalcps!=ptotalcps)
            {
               printf("\r%d%%", percent);
               if (cps != 0)
                  printf(" [%u cps, %u avg]      \b\b\b\b\b\b", cps, totalcps);
               fflush(stdout);
               ppercent = percent;
               pcps = cps;
               ptotalcps = totalcps;
            }
            if (sock.RecvTeol() <= 0)
            {
               logprintf("Unable to fetch message\n");
               fclose(fp);
               goto quit;
            }
            if (strcmp(sock.szOutBuf, ".\r\n") == 0)
               break;
            cpsbytecount += strlen(sock.szOutBuf);
            totalcpsbytecount += strlen(sock.szOutBuf);
            if (totalcpsbytecount > CPS_BYTES * 2)
            {
               cpsend = mtime();
               /* in case of wraparound, we discard this measurement
                  and keep the previous cps value (if any) */
               if (totalcpsstart < cpsend)
               {
                  totalcps = (unsigned)(totalcpsbytecount
                                        * 1000LU
                                       / (cpsend - totalcpsstart));
               }
            }
            if (cpsbytecount > CPS_BYTES)
            {
               cpsend = mtime();
               /* in case of wraparound, we discard this measurement
                  and keep the previous cps value (if any) */
               if (cpsstart < cpsend)
               {
                  cps = (unsigned)(cpsbytecount
                                   * 1000LU
                                   / (cpsend - cpsstart));
               }
               cpsstart = mtime();
               cpsbytecount = 0;
            }
            cp = sock.szOutBuf;
            if (*cp=='.')
               cp++;
            bytes += strlen(cp);    /* including CR and LF, not dot */
            cp2 = strstr(cp, "\r\n");
            if (cp2!=NULL)
               strcpy(cp2, "\n");
            fputs(cp, fp);
            /* string written, we may change it now */
            if (!inbody)
            {
               if (strcmp(sock.szOutBuf, "\n") == 0)
                  inbody = 1;
               else if (memicmp(cp, "From:", 5) == 0
                        && !fetchonly)
               {
                  cp2 = strchr(cp, '\n');
                  if (cp2!=NULL)
                     *cp2 = '\0';
                  cp2 = cp + 5;
                  while (isspace(*cp2))
                     cp2++;
                  printf("\r");
                  logprintf("* Message from %s\n", cp2);
                  printf("%d%%", percent);
                  fflush(stdout);
               }
            }
         }
         printf("\r                          \r");
         fflush(stdout);
         pos2 = ftell(fp);
         /* set size to number of bytes written, with \r removed */
         size = pos2 - pos1 - 4;
#if 0    // check disabled --- fails on several POP servers
         /* bytes = number of bytes read, including \r
            msglist[i].msgsize = expected message size, including \r */
         if (bytes != msglist[i].msgsize
             && size != msglist[i].msgsize)  /* required for some servers */
         {
            logprintf("Warning: size mismatch: expected %ld bytes, read %ld bytes\n",
                      msglist[i].msgsize, bytes);
            logprintf("(message may be corrupted or POP3 server incorrect)\n");
         }
#endif
         /* set size to number of bytes written, with \r removed
            in network order */
         /* convert size to network order */
         size = htonl(size);
         fseek(fp, pos1, SEEK_SET);
         fwrite(&size, 1, 4, fp);
         fseek(fp, pos2, SEEK_SET);
         if (!testmode)
         {
            if (verbose)
               logprintf("+ Deleting message from POP3 server...\n");
            sprintf(sock.szOutBuf, "DELE %ld\r\n", msglist[i].msgnum);
            if (sock.Send(sock.szOutBuf) == -1)
            {
               net_error("Send");
               goto quit;
            }
            if (getpopanswer(sock) != POP_OK)
               goto quit;
         }
      }

      fclose(fp);

      if (totalcps!=0)
         logprintf("Average CPS fetching email: %u\n", totalcps);

      delete msglist;

      strcpy(filename, soupdir);
      strcat(filename, "AREAS");
      fp = _fsopen(filename, "ab", SH_DENYWR);
      if (fp==NULL)
      {
         logprintf("Cannot append to %s\n", filename);
      }
      else
      {
         fprintf(fp, "%08lX\tEmail\tbn\n", filenum);
         fclose(fp);
      }
   }

   if (verbose)
      logprintf("+ Signing off...\n");

   if (sock.Send("QUIT\r\n") == -1)
   {
      net_error("Send");
      goto quit;
   }

   if (getpopanswer(sock) != POP_OK)
      goto error;

   sock.Close();

   if (verbose)
      logprintf("+ POP3 session successfully terminated\n");

   return 1;

quit:

   if (sock.Send("QUIT\r\n") == -1)
   {
      net_error("Send");
      goto quit;
   }

   getpopanswer(sock);

error:

   if (sock.iSock != -1)
      sock.Close();
   logprintf("(skipping email fetch)\n");
   return 0;
}
/*...e*/
