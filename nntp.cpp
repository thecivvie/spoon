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

/* Spoon NNTP functions */

#include "spoon.h"

enum {NNTP_OK, NNTP_ERR, NNTP_RETRY};

/*...sgetnntpanswer:0:*/
/* reads & analyzes NNTP answer
   NNTP_OK (1) = 1XX (info), 2XX (command ok) or 3XX (command ok so far)
   NNTP_ERR (0) = 4XX (couldn't perform command), 5XX (command error)
   if quiet is set to nonzero, error messages will not be displayed
   NNTP_RETRY (2) = authentication was required, retry command
   -1 = error */
static int getnntpanswer(Socket &sock, int quiet)
{
   int rval;
   char buf[128];

   rval = sock.RecvTeol();
   if (rval==-1)
   {
      net_error("Receive");
      return -1;
   }

   if (rval==0)
   {
      logprintf("Timeout - NNTP host did not answer\n");
      return -1;
   }

   if (memicmp(sock.szOutBuf, "480", 3) == 0)
   {
      if (nntpuser[0]=='\0')
      {
         logprintf("NNTPUSER not specified - NNTP host returned: %s", sock.szOutBuf);
         return NNTP_ERR;
      }
      sprintf(buf,"AUTHINFO USER %s\r\n", nntpuser);
      if (sock.Send(buf) == -1)
      {
         net_error("Send");
         return NNTP_ERR;
      }
      if (getnntpanswer(sock, quiet) == NNTP_ERR)
         return NNTP_ERR;
      else
         return NNTP_RETRY;
   }
   else if (memicmp(sock.szOutBuf, "381", 3) == 0)
   {
      if (nntppass[0]=='\0')
      {
         logprintf("NNTPPASS not specified - NNTP host returned: %s", sock.szOutBuf);
         return NNTP_ERR;
      }
      sprintf(buf,"AUTHINFO PASS %s\r\n", nntppass);
      if (sock.Send(buf) == -1)
      {
         net_error("Send");
         return NNTP_ERR;
      }
      if (getnntpanswer(sock, quiet) == NNTP_ERR)
         return NNTP_ERR;
      else
         return NNTP_RETRY;
   }
   else if (sock.szOutBuf[0]>='1' && sock.szOutBuf[0]<='3')
   {
      return NNTP_OK;
   }
   else
   {
      if (!quiet)
      {
         /* no newline, sock.szOutBuf ends in \r\n */
         logprintf("NNTP host returned error: %s", sock.szOutBuf);
      }
   
      return NNTP_ERR;
   }
}
/*...e*/

/*...sfetchnews:0:*/
int fetchnews(void)
{
   Socket sock;
   char tempfilename[FILENAME_MAX];
   char filename[FILENAME_MAX];
   char ngname[256];
   char *cp, *cp2;
   FILE *listf = NULL, *listf2 = NULL;
   FILE *fp = NULL;
   long filenum;
   int percent, ppercent;
   long firstold, lastold;
   long msgcount, msgfirst, msglast;
   long count;
   long pos1, pos2;
   int inbody;
   long size;
   int noposting;
   unsigned long cpsstart, cpsend, totalcpsstart;
   unsigned long cpsbytecount, totalcpsbytecount;
   unsigned cps, pcps;
   unsigned totalcps, ptotalcps;
   int rval;

   if (nntphost[0]=='\0')
   {
      logprintf("NNTP hostname not defined\n");
      goto error;
   }

   listf = _fsopen(newsgroupsfile, "r", SH_DENYNO);
   if (listf==NULL)
   {
      logprintf("Newsgroups list file %s not found\n", newsgroupsfile);
      goto error;
   }

   if (!testmode)
   {
      genfname(newsgroupsfile, "TMP", tempfilename);

      listf2 = _fsopen(tempfilename, "w", SH_DENYWR);
      if (listf2==NULL)
      {
         logprintf("Temporary file %s can't be written\n", tempfilename);
         goto error;
      }
   }
   
   if (sock.Create() == -1)
   {
      net_error("Open socket");
      goto error;
   }

   sock.ulTimeout = CONNECT_TIMEOUT;

   logprintf("Fetching news from %s...\n", nntphost);

   if (verbose)
      logprintf("+ Connecting to NNTP host %s...\n", nntphost);

   if (sock.Connect(nntphost, PORT_NNTP) == -1)
   {
      logprintf("Cannot connect to host %s at port %d\n",
                nntphost, PORT_NNTP);
      goto error;
   }

   if (getnntpanswer(sock, 0) == NNTP_ERR)
      goto error;

   sock.ulTimeout = SOCK_TIMEOUT;

   if (listnews)
   {
      fp = _fsopen(listfilename, "w", SH_DENYWR);
      if (fp==NULL)
      {
         logprintf("Cannot write to newsgroup list file %s\n", listfilename);
      }
      else
      {
         do
         {
            if (listyear==0)
            {
               logprintf("* Fetching newsgroup list into %s\n", listfilename);
               strcpy(sock.szOutBuf, "LIST\r\n");
            }
            else
            {
               logprintf("* Fetching new newsgroups since %02u-%02u-%02u %02u:%02u:%02u into %s\n",
                      listyear%100, listmonth, listday,
                      listhour, listmin, listsec,
                      listfilename);
               sprintf(sock.szOutBuf, "NEWGROUPS %02u%02u%02u %02u%02u%02u\r\n",
                         listyear%100, listmonth, listday,
                         listhour, listmin, listsec);
            }
            if (sock.Send(sock.szOutBuf) == -1)
            {
               net_error("Send");
               goto quit;
            }
            rval = getnntpanswer(sock, 0);
         }
         while (rval==NNTP_RETRY);
         if (rval != NNTP_OK)
         {
            logprintf("! Wasn't able to retrieve newsgroup list - skipping\n");
         }
         else
         {
            for (;;)
            {
               if (sock.RecvTeol() <= 0)
               {
                  logprintf("Error fetching newsgroup list\n");
                  fclose(fp);
                  goto quit;
               }
               if (strcmp(sock.szOutBuf, ".\r\n") == 0)
                  break;
               cp = sock.szOutBuf;
               if (*cp=='.')
                  cp++;
               cp2 = strchr(cp, '\r');
               if (cp2!=NULL)
                  *cp2 = '\0';
               if (cp2!=NULL && cp2>cp && tolower(cp2[-1])=='n')
                  noposting = 1;
               else
                  noposting = 0;
               cp2 = strchr(cp, ' ');
               if (cp2!=NULL)
                  *cp2 = '\0';
               fprintf(fp, "%s%s\n",
                           cp,
                           noposting? " [no posting]" : "");
            }
         }
         fclose(fp);
      }
   }

   while (fgets(ngname, sizeof ngname, listf) != NULL)
   {
      cp = strchr(ngname, '\n');
      if (cp!=NULL)
         *cp = '\0';
      lastold = 0;
      cp = strrchr(ngname, ':');
      if (cp!=NULL)
      {
         *(cp++) = '\0';
         while (isspace((unsigned char)*cp))
           *(cp++) = '\0';
         sscanf(cp, "%ld-%ld", &firstold, &lastold);
      }
      do
      {
         logprintf("* Fetching %s\n", ngname);
         sprintf(sock.szOutBuf, "GROUP %s\r\n", ngname);
         if (sock.Send(sock.szOutBuf) == -1)
         {
            net_error("Send");
            goto quit;
         }
         rval = getnntpanswer(sock, 0);
      }
      while (rval == NNTP_RETRY);
      if (rval != NNTP_OK)
      {
         logprintf("! Wasn't able to select newsgroup %s - skipping\n",
                ngname);
         continue;
      }

      msgcount = 0;
      msgfirst = 0;
      msglast = 0;
      sscanf(sock.szOutBuf+4, "%ld %ld %ld", &msgcount, &msgfirst, &msglast);

      if (msglast <= lastold)
      {
         msgcount = 0;
      }
      else if (msgfirst <= lastold)
      {
         msgcount = msglast - lastold;
         msgfirst = lastold + 1;
      }

      if (msgcount==0)
      {
         logprintf("There are no new messages in this newsgroup.\n");
         if (!testmode)
            fprintf(listf2, "%s: %ld-%ld\n", ngname, firstold, lastold);
         continue;
      }

      logprintf("There are %ld new messages (%ld-%ld) in this newsgroup.\n",
                msgcount, msgfirst, msglast);

      filenum = genfname(soupdir, "MSG", filename);
      fp = _fsopen(filename, "wb", SH_DENYWR);
      if (fp==NULL)
      {
         logprintf("Cannot write to %s\n", filename);
         goto quit;
      }

      ppercent = -1;
      cpsstart = mtime();
      totalcpsstart = cpsstart;
      cpsbytecount = 0;
      totalcpsbytecount = 0;
      cps = 0;
      pcps = 0;
      totalcps = 0;
      ptotalcps = 0;
      for (count=msgfirst; count<=msglast; count++)
      {
         percent = (int)((count-msgfirst) * 100 / (msglast-msgfirst+1));
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
         sprintf(sock.szOutBuf, "ARTICLE %ld\r\n", count);
         if (sock.Send(sock.szOutBuf) == -1)
         {
            net_error("Send");
            fclose(fp);
            goto quit;
         }
         if (getnntpanswer(sock, 1) != NNTP_OK)
         {
            /* possibly gap, quietly skip message */
            continue;
         }
         size = 0;   /* size unknown, will be filled in later */
         pos1 = ftell(fp);
         fwrite(&size, 4, 1, fp);
         inbody = 0;
         for (;;)
         {
            if (sock.RecvTeol() <= 0)
            {
               logprintf("Unable to fetch message\n");
               fclose(fp);
               goto quit;
            }
            if (strcmp(sock.szOutBuf, ".\r\n") == 0)
               break;
            cpsbytecount += strlen(sock.szOutBuf);
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
                  totalcpsbytecount += cpsbytecount;
                  totalcps = (unsigned)(totalcpsbytecount
                                        * 1000LU
                                       / (cpsend - totalcpsstart));
               }
               cpsstart = mtime();
               cpsbytecount = 0;
            }
            cp = sock.szOutBuf;
            if (*cp=='.')
               cp++;
            cp2 = strstr(cp, "\r\n");
            if (cp2!=NULL)
               strcpy(cp2, "\n");
            fputs(cp, fp);
            /* string written, we may change it now */
            if (!inbody)
            {
               if (strcmp(sock.szOutBuf, "\n") == 0)
                  inbody = 1;
            }
         }
         pos2 = ftell(fp);
         /* set size to number of bytes written, with \r removed
            in network order */
         size = htonl(pos2 - pos1 - 4);
         fseek(fp, pos1, SEEK_SET);
         fwrite(&size, 1, 4, fp);
         fseek(fp, pos2, SEEK_SET);
         lastold = count;
      }

      fclose(fp);

      printf("\r                          \r");
      fflush(stdout);
      if (totalcps!=0)
         logprintf("Average CPS fetching news: %u\n", totalcps);

      if (!testmode)
         fprintf(listf2, "%s: %ld-%ld\n", ngname, firstold, lastold);

      strcpy(filename, soupdir);
      strcat(filename, "AREAS");
      fp = _fsopen(filename, "ab", SH_DENYWR);
      if (fp==NULL)
      {
         logprintf("Cannot append to %s\n", filename);
         goto quit;
      }
      else
      {
         fprintf(fp, "%08lX\t%s\tBn\n", filenum, ngname);
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

   if (getnntpanswer(sock, 0) != NNTP_OK)
      goto error;

   sock.Close();

   fclose(listf);

   if (!testmode)
   {
      fclose(listf2);
      remove(newsgroupsfile);
      rename(tempfilename, newsgroupsfile);
   }

   if (verbose)
      logprintf("+ NNTP session successfully terminated\n");

   return 1;

quit:

   if (sock.Send("QUIT\r\n") == -1)
   {
      net_error("Send");
      goto quit;
   }

   getnntpanswer(sock, 0);

error:

   if (listf!=NULL)
      fclose(listf);
   if (listf2!=NULL)
   {
      fclose(listf2);
      remove(tempfilename);
   }
   if (sock.iSock != -1)
      sock.Close();
   logprintf("(skipping news fetch)\n");
   return 0;
}
/*...e*/

/*...spostnews:0:*/
int postnews(void)
{
   Socket sock;
   char filename[FILENAME_MAX];
   char buffer[256];
   char *cp, *cp2;
   char format;
   FILE *fp = NULL, *fp2 = NULL;
   int news;
   long msgsize, nextmsgpos;
   int keepmsg;
   int inbody;
   int rval;

   if (nntphost[0]=='\0')
   {
      logprintf("NNTP hostname not defined\n");
      goto error;
   }

   strcpy(filename, soupdir);
   strcat(filename, "REPLIES");
   fp = _fsopen(filename, "rb", SH_DENYNO);
   if (fp==NULL)
   {
      if (verbose)
         logprintf("+ No REPLIES file found, not posting any news\n");
      return 1;
   }

   news = 0;
 
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
      if (stricmp(cp, "news") != 0)
      {
         if (stricmp(cp, "mail") != 0)
         {
            if (!transfer[EMAIL][POST])   /* give error only once */
               logprintf("! Unknown message kind in REPLIES\n");
            killreplies = 0;
         }
         else if (!transfer[EMAIL][POST])
         {
            logprintf("Email found --- will keep REPLIES file\n");
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

      if (!news)
      {
         if (sock.Create() == -1)
         {
            net_error("Open socket");
            goto error;
         }

         sock.ulTimeout = CONNECT_TIMEOUT;

         logprintf("Posting news to %s...\n", nntphost);

         if (verbose)
            logprintf("+ Connecting to NNTP host %s...\n", nntphost);

         if (sock.Connect(nntphost, PORT_NNTP) == -1)
         {
            logprintf("Cannot connect to host %s at port %d\n",
                      nntphost, PORT_NNTP);
            goto error;
         }

         if (getnntpanswer(sock, 0) == NNTP_ERR)
            goto error;

         sock.ulTimeout = SOCK_TIMEOUT;
 
         news = 1;
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

         do
         {
            if (sock.Send("POST\r\n") == -1)
            {
               net_error("Send");
               goto quit;
            }
            rval = getnntpanswer(sock, 0);
         }
         while (rval==NNTP_RETRY);
         if (rval != NNTP_OK)
         {
            printf("! NNTP server doesn't allow us to post\n");
            goto error;
         }

         inbody = 0;

         while (ftell(fp2) < nextmsgpos)
         {
            fgets(buffer, sizeof buffer - 2, fp2);
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
               goto quit;
            }
            /* string sent, we may change it now */
            if (!inbody)
            {
               if (strcmp(buffer, "\r\n") == 0)
                  inbody = 1;
               else if (memicmp(buffer, "Newsgroups:", 11) == 0)
               {
                  cp = strchr(buffer, '\r');
                  if (cp!=NULL)
                     *cp = '\0';
                  cp = buffer + 11;
                  while (isspace(*cp))
                     cp++;
                  logprintf("* Posting article to %s\n", cp);
               }
            }
         }

         if (sock.Send(".\r\n") == -1)
         {
            net_error("Send");
            goto quit;
         }

         if (getnntpanswer(sock, 0) != NNTP_OK)
         {
            printf("! NNTP server did not accept message\n");
            continue;
         }
      }

      fclose(fp2);

      if (!testmode && !keepmsg)
         remove(filename);
   }

   fclose(fp);

   if (news)
   {
      if (verbose)
         logprintf("+ Signing off...\n");

      if (sock.Send("QUIT\r\n") == -1)
      {
         net_error("Send");
         goto quit;
      }

      if (getnntpanswer(sock, 0) != NNTP_OK)
         goto error;

      sock.Close();

      fclose(fp);

      if (verbose)
         logprintf("+ NNTP session successfully terminated\n");
   }
   else
   {
      if (verbose)
         logprintf("+ No news messages to be posted\n");
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

   getnntpanswer(sock, 0);

error:

   if (fp!=NULL)
      fclose(fp);
   if (fp2!=NULL)
      fclose(fp2);
   if (sock.iSock != -1)
      sock.Close();
   logprintf("(skipping news post)\n");
   return 0;
}
/*...e*/
