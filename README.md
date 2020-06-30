# spoon
Spoon is an easily configurable program to fetch and post mail
and/or news to/from SOUP packets. It can be used in combination
with other software to read or convert the SOUP packets.


; Sample Spoon configuration file
; Adjust and rename to spoon.cfg.
; See spoon.doc for detailed information.

; SOUP directory
SoupDir c:\soupgate\soup

; log file
LogFile spoon.log

; our hostname (required for posting email)
HostName myhost.net

; POP3 username (required for fetching email)
PopUser john_doe

; POP3 hostname (required for fetching email)
PopHost pop.isp.com

; POP3 password (required for fetching email)
PopPass secret0923

; SMTP hostname (required for posting email)
SmtpHost smtp.isp.com

; NNTP hostname (required for fetching/posting news)
NntpHost news.isp.com

; NNTP username (if required)
;NntpUser username

; NNTP password (if required)
;NntpPass password

; Resend SMTP MAIL command for every recipient
SmtpSeparateMAIL No
