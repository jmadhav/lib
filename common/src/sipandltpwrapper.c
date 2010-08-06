/**
 Copyright (c) 2009 Geodesic Limited <http://www.geodesic.com>.
 
 This file is part of Spokn software.
 
 Spokn is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 
 the Free Software Foundation, version 2 of the License.
  
 Spokn is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with Spokn.  If not, see <http://www.gnu.org/licenses/>.
 */



#include <ltpmobile.h>
#include "sipandltpwrapper.h"
#include <stdlib.h>
#include <string.h>

#define USERAGENT "spokn-iphone-1.0.0"
//#define USERAGENT "ltpmobile"
#ifdef _WIN32_WCE
#define stricmp _stricmp
#elif defined WIN32
#define stricmp _stricmp
#else
#define stricmp strcasecmp
#endif 

//#undef DEBUG

#ifdef DEBUG 
#include <winsock.h>


/* Note:
 
 This is a modified version of the original LTP stack 5 (ltpstack5.cpp). 
 The major change is that the chat messages are now handled with their state being
 preserved in the struct Contact.
 
 The struct Contact will all be empty unless a message arrives or has to be sent.
 in such cases, the first thing is to locate a contact with the same userid already
 available with a message sent or received from the same peer.
 
 upon locating such a contact, the date of last activity is noted and if it is beyond
 two minutes (or the last message failed to deliver) the routing information will be
 trashed and the message is sent directly to the server. the server will then return 
 the redirect address or handle the message by itself.
 
 */


/* debug requires us to use printf to print the traces of the debug messages
 which is why we will require to include stdio.h, otherwise, there is no need
 to include it. We are trying to keep the ltp independent of any standard C library
 */

static char *netGetIPString(long address)
{
	struct in_addr	addr;
	
	addr.s_addr = address;
	return inet_ntoa(addr);
}

#endif



/* C library functions:
 some library functions are required to do basic string stuff and move bytes around
 we wrote our own to optimise on the c library requirement. (for instance,
 the symbian won't provide it out of box) */

/* zeroMemory: set all the bytes to zero */
static void zeroMemory(void *p, int countBytes)
{
	char *q;
	
	q = (char *)p;
	while(countBytes--)
		*q++ = 0;
}

#if 0

/* if you don't know these functions, you are early here, get yourself a
 copy of K&R and work to chapter 5 and then head back to this place */
int strlen(char *p)
{
	int i = 0;
	while (*p++)
		i++;
	return i;
}

char *strncpy(char *d, const char *s, short n)
{
	char *dst = d;
	
	if (n != 0) 
	{
		do 
		{
			if ((*d++ = *s++) == 0)
			{
				/* NUL pad the remaining n-1 bytes */
				while (--n != 0)
					*d++ = 0;
				break;
			}
		} 
		while (--n != 0);
	}
    return (dst);
}

int strcmp(char *p, char *q)
{
	while (*p == *q++)
		if (*p++ == 0)
			return 0;
	
	return (*(const unsigned char *)p - *(const unsigned char *)(q - 1));
}



void memcpy(void *q, const void *p, int count)
{
	char *pc, *qc;
	
	pc = (char *)p;
	qc = (char *)q;
	while (count--)
		*qc++ = *pc++;
}
#endif
/* md5 functions :
 as md5 alogrithm is sensitive big/little endian integer representation,
 we have adapted the original md5 source code written by Colim Plum that
 as then hacked by John Walker.
 If you are studying the LTP source code, then skip this section and move onto
 the queue section. */

/*typedef unsigned int32 uint32;
 
 struct MD5Context {
 uint32 buf[4];
 uint32 bits[2];
 unsigned char in[64];
 };*/



/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */


/* a hash function to compute chat string hashes */
static unsigned long hash(void *data, int length){
	unsigned long hash = 5381;
    int c;
	unsigned char *str = (unsigned char *)data;
	
    while (length--)
	{
		c = *str++;
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}
    return hash;
}


static unsigned short16 flip16(unsigned short16 input)
{
	unsigned short16 output;
	unsigned char *p, *q;
	
	
	p = (unsigned char *)&input;
	q = (unsigned char *)&output;
	
	*q++ = p[1];
	*q = p[0];
	return output;
}

static int32 flip32(int32 input)
{
	int32 output;
	unsigned char *p, *q;
	
	
	p = (unsigned char *)&input;
	q = (unsigned char *)&output;
	
	*q++ = p[3];
	*q++ = p[2];
	*q++ = p[1];
	*q = p[0];
	return output;
}

static void ltpFlip(struct ltp *ppack)
{
	ppack->version	= flip16(ppack->version);
	ppack->command	= flip16(ppack->command);
	ppack->response	= flip16(ppack->response);
	ppack->msgid	= flip32(ppack->msgid);
	ppack->wparam	= flip16(ppack->wparam);
	ppack->lparam	= flip32(ppack->lparam);
}

/* LTP_callInit:
 Initializes the call to be ready for the next call. Becareful in not
 initializing by calling zeroMemory on this structure, that will free
 up the code structures that are dynamically allocated in ltpInit()
 */
static void LTP_callInit(struct Call *p, short16 defaultCodec)
{
	int	i;
	
	p->remoteUserid[0]	= 0;
	p->title[0] = 0;
	p->remoteIp	= 0;
	p->remotePort	= 0;
	p->ltpSession = 0;
	p->ltpState	= CALL_IDLE;
	p->maxTime	= -1;
	p->ltpSeq	= 0;
	p->ltpSession = 0;
	p->retryNext	= 0;
	p->retryCount	= 0;
    p->rtpSequence = 0;
    p->rtpTimestamp = 0;
	p->timeStart = p->timeStop = 0;
	p->ltpLastMsgReceived = 0;
	p->codec = defaultCodec;
	p->remoteVoice = 0;
	p->InConference = 0;
	p->kindOfCall = 0;
	p->fwdIP = 0;
	p->fwdPort = 0;
	p->nRingMessages = 0;
	p->prevSample = 0;
	p->inTalk = 0;
	p->directMedia = 0;
	
	for (i = 0; i < 12; i++)
		p->delay[i] = 0;
	
	p->coef[0] =	   2087;
	p->coef[1] =	   5979;
	p->coef[2] =     -7282;
	p->coef[3] =     -6747;
	p->coef[4] =	  28284;
	p->coef[5] =	  52763;
	p->coef[6] =	  28284;
	p->coef[7] =     -6747;
	p->coef[8] =     -7282;
	p->coef[9] =	   5979;
	p->coef[10] =	   2087;
	p->coef[11] =    -4816;
	
	
	queueInit(&(p->pcmOut));
	queueInit(&(p->pcmIn));
}

/* 
 ltpInit:
 this is the big one. It calls malloc (from standard c library). If your system doesn't support
 dynamic memory, no worry, just pass a pointer to a preallocated block of the size of ltpStack. It is
 a one time allocation. The call structures inside are also allocated just once in the entire stack's
 lifecycle. The memory is nevery dynamically allocated during the runtime.
 
 We could have declared ltpStack and Calls as global data, however a number of systems do not allow
 global writeable data (like the Symbian) hence, we request for memory. Most systems can simply return 
 a pointer to preallocated memory */

struct ltpStack  *LTP_ltpInit(int maxslots, int maxbitrate, int framesPerPacket)
{
	int	i;
#ifdef SUPPORT_SPEEX
	int x;
#endif
	struct Call *p;
	struct ltpStack *ps;
	
	ps = (struct ltpStack *)malloc(sizeof(struct ltpStack));
	if (!ps)
		return NULL;
	
	zeroMemory(ps, sizeof(struct ltpStack));	
	
	ps->defaultCodec = (short16) maxbitrate;
	ps->loginCommand = CMD_LOGIN;
	ps->nextCallSession=1;
	ps->nat=1;
	ps->loginStatus = LOGIN_STATUS_OFFLINE;
	ps->myPriority = NORMAL_PRIORITY;
	ps->ringTimeout  = 30;
	ps->nextCallSession = 786;
	ps->soundBlockSize = framesPerPacket * 160;
	ps->myPriority = NORMAL_PRIORITY;
	strncpy(ps->userAgent,USERAGENT, MAX_USERID);
	ps->ltpPresence = NOTIFY_ONLINE;
	ps->updateTimingAdvance = 0;
	
	ps->maxSlots = maxslots;
	ps->call = (struct Call *) malloc(sizeof(struct Call) * maxslots);
	if (!ps->call)
		return NULL;
	zeroMemory(ps->call, sizeof(struct Call) * maxslots);	
	
	ps->chat = (struct Message *) malloc(sizeof(struct Call) * MAX_CHATS);
	if (!ps->chat)
		return NULL;
	zeroMemory(ps->chat, sizeof(struct Message) * MAX_CHATS);
	ps->maxMessageRetries = 3;
	
	for (i=0; i<maxslots; i++)
	{
		p = ps->call + i;
		LTP_callInit(p, ps->defaultCodec);
		
		/* init gsm codec */
		p->gsmIn = gsm_create();
		p->gsmOut = gsm_create();
		
#ifdef SUPPORT_SPEEX
		/* init speex codec */
		speex_bits_init(&(p->speexBitEnc));
		p->speex_enc = speex_encoder_init(&speex_nb_mode);
		
		//		x = ps->defaultCodec; /*set the bit-rate*/
		x = 8000;
		speex_encoder_ctl(p->speex_enc,SPEEX_SET_BITRATE,&x);
		//		x = 0;
		//		speex_encoder_ctl(p->speex_enc,SPEEX_SET_COMPLEXITY,&x);
		x = 8000;  /*set the sampling rate*/
		speex_encoder_ctl(p->speex_enc,SPEEX_SET_SAMPLING_RATE,&x);	
		
		
		speex_bits_init(&(p->speexBitDec));		
		p->speex_dec = speex_decoder_init(&speex_nb_mode);
		
		/* speex wideband codec */
		speex_bits_init(&(p->speexWideBitEnc));
		p->speex_wb_enc = speex_encoder_init(&speex_wb_mode);
		x = 22000; /* set the bit-rate */
		speex_encoder_ctl(p->speex_wb_enc,SPEEX_SET_BITRATE,&x);
		x = 16000;  /* set the sampling rate */
		speex_encoder_ctl(p->speex_wb_enc,SPEEX_SET_SAMPLING_RATE,&x);	
		
		speex_bits_init(&(p->speexWideBitDec));
		p->speex_wb_dec = speex_decoder_init(&speex_wb_mode);
#endif
		/* assign lineId */
		p->lineId = i;
	}
	
	
	/* determine if we are on a big-endian architecture */
	i = 1;
	if (*((char *)&i))
		ps->bigEndian = 0;
	else
		ps->bigEndian = 1;
	
	return ps;
}


/* LTP_ltpWrite:
 Sends an ltp packet out, it might require some bits to be flipped over
 if you are running on a big endian system */

int LTP_ltpWrite(struct ltpStack *ps, struct ltp *ppack, unsigned int length, unsigned int32 ip, unsigned short16  port)
{
	int ret;
#ifdef DEBUG
	if (ps->doDebug)
		ltpTrace(ppack);
#endif	
	if (ps->bigEndian)
		ltpFlip(ppack);
	
	ret = netWrite(ppack, length, ip, port);
	
	/* added this so that the ppack refered to after sending the packet is still readable */
	if (ps->bigEndian)
		ltpFlip(ppack);
	
	return ret;
}
/* LTP_callStartRequest:
 A Call repeatedly sends an ltp message to a remote end-point (or the ltp server)
 until a response is received or time-out */
static void LTP_callStartRequest(struct ltpStack *ps, struct Call *pc, struct ltp *pltp)
{
	struct ltp	*p;
	
	/* copy the ltp request to the per-call buffer */
	if (pltp)
		memcpy(pc->ltpRequest, pltp, sizeof(struct ltp) + strlen(pltp->data));
	
	/* calculate the length of the ltp buffer (it ends with a human readable, null terminated string) */
	p = (struct ltp *) pc->ltpRequest;
	pc->ltpRequestLength = sizeof(struct ltp) + strlen(p->data);
	if (pc->ltpRequestLength > 1000)
		return;
	
	/* send out the packet */
	LTP_ltpWrite(ps, (struct ltp *)pc->ltpRequest, pc->ltpRequestLength, pc->remoteIp, pc->remotePort);
	
#if DEBUG
	
	printf("out[%d] %02d %03d %s %s to %s %d\n", pc->lineId,
		   p->command, p->response, p->from, p->to, 
		   netGetIPString(pc->remoteIp), ntohs(pc->remotePort));
#endif
	
	/* schedule at least 10 retries of the request  at five seconds intervals*/
	pc->retryCount = 10;
	pc->retryNext = ps->now + 5;
}



/* callStopRequest:
 Stop resending the packets, often called when a proper response is received for a request
 or it has timed-out */
static void LTP_callStopRequest(struct Call *pc)
{
	pc->retryCount = 0;
}

/*  
 callFindIdle:
 finds an empty slot to start a new call
 returns NULL if not found (quite possible), always check for null */
struct Call *LTP_callFindIdle(struct ltpStack *ps)
{
	int     i;
	struct Call *p;
	
	for (i = 0; i < ps->maxSlots; i++)
	{
		p = ps->call + i;
		if(p->ltpState == CALL_IDLE) 
		{
			LTP_callInit(p, ps->defaultCodec);
#if DEBUG
			printf("free slot %d\n", p->lineId);
#endif
			return p;
		}
	}
#if DEBUG
	printf("free slot: none\n");
#endif
	return NULL;
}


/* callSearchByRTP:
 When an incoming RTP packet arrives, we need to match that with an existing call.
 This function cannot use userids etc. to do the matching, hence it is less
 restrictive than the search used for ltp packets.
 */
struct Call *LTP_callSearchByRtp(struct ltpStack *ps, unsigned int32 ltpIp, unsigned short16 ltpPort, unsigned int32 fwdip, unsigned short16 fwdport, unsigned int session)
{
	int     i;
	struct Call *call = ps->call;
	
	for (i = 0; i < ps->maxSlots; i++)
		if (call[i].remoteIp == ltpIp && call[i].remotePort == ltpPort && 
			call[i].fwdIP == fwdip && call[i].fwdPort == fwdport &&
			call[i].ltpState != CALL_IDLE && call[i].ltpSession == session)
			return call + i;
	
	return NULL;
}

/* callMatch:
 Matches an incoming LTP message (request or response) to a call.
 */
static struct Call *LTP_callMatch(struct ltpStack *ps, struct ltp *p, unsigned int32 ip, unsigned short16 port, 
								  unsigned int sessionid)
{
	char	*remote;
	struct Call	*pc;
	int		i;
	
	if (p->response)
		remote = p->to;
	else
		remote = p->from;
	
#if DEBUG
	printf("searching for %s:%d from %s:%d\n", p->from, p->session, netGetIPString(ip), ntohs(port));
#endif
	
	for (i = 0; i < ps->maxSlots; i++)
	{
		pc = ps->call + i;
		if (pc->ltpState != CALL_IDLE 
			&& pc->ltpSession == p->session
			&& ip == pc->remoteIp && port == pc->remotePort)
		{
#if DEBUG
			printf("findSlot:%d\n", pc->lineId);
#endif
			return pc;
		}
	}
	
	/* 
	 A message can arrive after a call is ended and the slot is freed.
	 but we dont destroy the details of the previous call in the freed slot
	 unless it is claimed by a new call so even after the call is ended, a message
	 can be received by that slot.
	 mostly used to inform the user about call charges etc.
	 '*/
	if (p->command == CMD_MSG)
	{
		for (i = 0; i < ps->maxSlots; i++)
		{
			pc = ps->call + i;
			if (pc->ltpState == CALL_IDLE //&& !strcmp(remote, pc->remoteUserid) 
				&& pc->ltpSession == p->session
				&& ip == pc->remoteIp && port == pc->remotePort)
			{
#if DEBUG
				printf("matched expired Slot:%d\n", pc->lineId);
#endif
				return pc;
			}
		}		
	}
	
	return NULL;
}

/* callTimeout:
 called when requests to a remote user have timed out
 */
static void callTimeOut(struct Call *pc)
{
	struct ltp *pm = (struct ltp *)pc->ltpRequest;
	
	switch (pm->command)
	{
		case CMD_RING:
		case CMD_TALK:
		case CMD_ANSWER:
		case CMD_REFUSE:
		case CMD_HANGUP:
			/*we just mark the slot idle and preserve the other variables
			 so that we can still accept out-of-call messages for this call
			 like a message or a late hangup */
			pc->ltpState = CALL_IDLE;
			alert(pc->lineId, ALERT_DISCONNECTED, "Not reachable");
			break;
			
		case CMD_MSG:
			/* alert(pc->lineId, ALERT_MESSAGE, pm->data); */
			break;
	}
}

/* 
 callFree:
 free a call */
/*
 static void callFree(struct ltpStack *ps, struct Call *pc)
 {
 int i;
 
 //this check is for a valid pc pointer
 for (i=0; i < ps->maxSlots; i++)
 if (pc == ps->call + i)
 LTP_callInit(pc, ps->defaultCodec);
 }
 */

/*
 first try locating a chat session with userd,
 if found, check if it has an active session and return the session.
 if not found, search for a free chat session and initialize it.
 */
struct Message *LTP_getMessage(struct ltpStack *ps, char *userid)
{
	int i;
	struct Message *pchat;
	struct Message *pbest;
	
	if (!*userid)
		return NULL;
	
	//do we have an existing and active chat with this userid?
	for (i = 0; i < MAX_CHATS; i++){
		pchat = ps->chat + i;
		if (!strcmp(pchat->userid, userid)){
			//has the session expired
			if (pchat->dateLastMsg + 120 < ps->now && !pchat->retryCount){
				//null everything
				pchat->ip = ps->ltpServer;
				pchat->fwdip = 0;
				pchat->fwdport = 0;
				pchat->port = ps->ltpServerPort;
				pchat->retryCount = 0;
				//				pchat->session = ps->nextCallSession++;
				return pchat;
			}
			//ok, we have a chat session (it might be busy)
			return pchat;
		}
	}
	
	//search for an empty chat object
	//init it with the userid, we are careful not to initialize other fields
	//they will get initialized by the sending or receiving functions
	for (i = 0; i < MAX_CHATS; i++){
		pchat = ps->chat + i;
		if (!pchat->userid[0]){
			strcpy(pchat->userid, userid);
			pchat->session = ps->nextCallSession++;
			return pchat;
		}
	}
	
	//we didn't find any empty chat slot either, 
	//now search for an inactive chat slot of another userid
	//pbest holds the least recently used, inactive chat slot
	pbest = NULL;
	for (i = 0; i < MAX_CHATS; i++){
		pchat = ps->chat + i;
		if (pchat->retryCount) //skip all the slots with pending outgoing messages
			continue;
		
		if (!pbest)
			pbest = pchat;
		else if (pbest->dateLastMsg > pchat->dateLastMsg) //least recently used
			pchat = pbest;
	}
	
	if (pbest){
		pbest->ip = ps->ltpServer;
		pbest->fwdip = 0;
		pbest->fwdport = 0;
		pbest->port = ps->ltpServerPort;
		pbest->retryCount = 0;
		pchat->session = ps->nextCallSession++;
	}
	return pbest; //might be NULL if all slots were busy
}

/* redirect:
 the redirection is a response to an request originated by us.
 usually, when we send a call/ptt/msg request packet to the ltp server,
 ltp server redirects us to the destination's ip/port and the current and future
 messages are to be exchanged with the remote destination directly
 */
static void LTP_redirect(struct ltpStack *ps, struct ltp *response, unsigned int fromip, unsigned short fromport)
{	
	int32	ip;
	short16 port;
	struct Call *pc=NULL;
	int	i;
	
	//26 april, 2009, farhan
	//if we are redirecting the ring, then the call object must be updated to point to the new end-point
	//otherwise, we will end up spawing redirects in several directions.
	if (response->command == CMD_RING){
		
		for (i = 0; i < ps->maxSlots; i++){
			pc = ps->call + i;
			if (!strcmp(pc->remoteUserid, response->to) && 
				pc->ltpSession == response->session && 
				pc->remoteIp == fromip && 
				pc->remotePort == fromport)
			{
				pc->remoteIp = response->contactIp;
				pc->remotePort = response->contactPort;
			}
		}
		return;
	}
	
	
	//ensure that redirect message is one that is acceptable
	if (response->command == CMD_RING ||
		response->command == CMD_MSG ||
		response->command == CMD_NOTIFY)
	{
		ip = response->contactIp;
		port = response->contactPort;
		
		response->response = 0;
		//response->data[0]=0;
		response->contactIp = 0;
		response->contactPort = 0;
		
		LTP_ltpWrite(ps, response, sizeof(struct ltp) + strlen(response->data), ip, port);
	}
}


/* login/logout:
 
 This function is called for login as well as logut.
 
 Though login is called every second through the tick function,
 the login() decides whether to (a) do nothing, (b) repeat a 
 pending login or (c) start a new login. It also checks if
 a login has timedout/
 
 login keeps checking the time and tries logging onto the server
 after every LTP_LOGIN_INTERVAL number of seconds. 
 usually logging in every 15 minutes is a good idea.
 as login serves others to know if you are online or not
 
 you can force the login or logout by supplying CMD_LOGIN or
 CMD_LOGOUT as a parameter when calling this function
 
 how it works:
 every few, a series of CMD_LOGIN messages are sent
 to the server until a response is obtained and then there is
 peace and quite until the time for the next login.
 
 Notes:
 1. the retry interval between successive packets is LTP_RETRY_INTERVAL
 2. after a successful login, you wait for LTP_LOGIN_INTERVAL seconds before starting again
 */
void LTP_ltpLogin(struct ltpStack *ps, int command)
{
    struct ltp *ppack = (struct ltp *)ps->scratch;
	int i;
	
	/* if the command is non-zero then force a fresh
	 login/logout starting now */
	if (command)
	{
		ps->loginNextDate = 0;
		ps->loginCommand = command;
		ps->loginRetryCount = 0;
		if (command == CMD_LOGIN)
			ps->myPriority = NORMAL_PRIORITY + 1;
		/* added for fast login */
		zeroMemory(ps->ltpNonce, sizeof(ps->ltpNonce));
	}
	
	/* 
	 on each login attempt (every few minutes), login packets are sent to the server 
	 repeatedly every few seconds until we get a response or timeout.
	 
	 loginNextDate tells us when we should retransmit the login message. on each login attempt 
	 Note:  'now' contains a recently read julian time value. Don't know what is julian date? google it.
	 */
	if((unsigned int) ps->now < ps->loginNextDate)
		return;
	
	/* if loginRetry count is zero, it means, it is time for a fresh login */
	if (ps->loginRetryCount == 0)
	{
		/* If the operating system can resolve the ip address of a domain name,
		 it means that the internet connectivity is working and we can actually try a login.
		 
		 trying to resolve the server's domain name into an ip adddress
		 is a hackful way of knowing if we have internet connectivity.
		 */
		ps->ltpServer = lookupDNS(ps->ltpServerName);
		if (!ps->ltpServer || ps->ltpServer == -1)
		{
			/* Implementors Note : lookupDNS is expected to return a 0 if it fails to
			 resolve the server name or if the net is down.
			 
			 if we cannot resolve the ip address, then lets hang around and retry later
			 abandon login attempt for the time being */
			ps->loginNextDate = (unsigned int)ps->now + LTP_LOGIN_INTERVAL;
			ps->loginStatus = LOGIN_STATUS_NO_ACCESS;
			
			ps->myPriority = NORMAL_PRIORITY;
			return;
		}
		
		/* LTP keeps it's users online by re-logging them in every few minutes. While
		 an attempt is on to re-login, the ltp stack should not be signalled as offline.
		 On the other hand, if the user has requested a login manually, then the ltp stack
		 should be set to offline until the login succeeds.
		 set loginstatus to offline if login() was called with CMD_LOGIN from the user interface */
		if (command == CMD_LOGIN)
		{
			ps->loginStatus = LOGIN_STATUS_OFFLINE;
			ps->myPriority = NORMAL_PRIORITY+1;
		}
		
		/* Do we have userid and password ready? if not probably
		 we are dealing with a new user. ask her to get a userid from the server
		 abandon login attempt for the time being */
		if (!strlen(ps->ltpUserid) || !strlen(ps->ltpPassword))
		{
			ps->loginNextDate = (unsigned int)ps->now + LTP_RETRY_INTERVAL;
			return;
		}
		
		/* if we are logged off then display to the user that we
		 are attempting to login again */
		if (ps->loginStatus != LOGIN_STATUS_ONLINE)
			ps->loginStatus = LOGIN_STATUS_TRYING_LOGIN;
		
		if (ps->loginStatus == LOGIN_STATUS_ONLINE && command == CMD_LOGOUT)
		{
			ps->loginStatus = LOGIN_STATUS_TRYING_LOGOUT;
			for (i = 0; i < ps->maxSlots; i++)
				if (ps->call[i].ltpState != CALL_IDLE)
					LTP_ltpHangup(ps, i);
		}
		
		/* on each login attempt, we increment the session count
		 initialise the number of retries to be made and how many seconds we
		 wait between the retries */
		ps->loginSession++;
		ps->loginRetryCount = LTP_MAX_RETRY;
		ps->loginRetryDate = ps->now + LTP_RETRY_INTERVAL;
	}
	/* this is the block for retrying an ongoing login attempt */
	else if (ps->now > ps->loginRetryDate)
	{
		/* decrement the count */
		ps->loginRetryCount--;
		if (ps->loginRetryCount <= 0)
		{
			/* if we have retried LTP_MAX_RETRY times, then we 
			 give up and declare ourselves offline */
			if (ps->loginCommand == CMD_LOGIN)
				ps->loginStatus = LOGIN_STATUS_NO_ACCESS;
			else
				ps->loginStatus = LOGIN_STATUS_OFFLINE;
			
			ps->loginStatus = LOGIN_STATUS_TIMEDOUT;
			ps->myPriority = NORMAL_PRIORITY;
			/* let's rest and try after sometime */
			ps->loginNextDate = (unsigned int)ps->now + LTP_LOGIN_INTERVAL;
			alert(-1, ALERT_OFFLINE, 0);
			return;
		}
		/*
		 there are still more retry attempts left
		 we are going ahead with the sending off another packet (see the code a few lines down)
		 also schedule another one after a few seconds */
		ps->loginRetryDate = ps->now + LTP_RETRY_INTERVAL;
	}
	else
	/* we return if all is fine and there we are logged in at the moment 
	 and there is no need to start another attempt yet */
		return;
	
	
	/* SENDING A LOGIN PACKET !!! we are here because we have eliminated all the cases
	 that can abort our attempt at a login */
	
	/* won't happen unless we have a userid, password, server ip are in place */
	if (!strlen(ps->ltpUserid) || !strlen(ps->ltpPassword) || ps->ltpServer == 0)
		return;
	
	/* init the packet */
	zeroMemory(ppack, sizeof(struct ltp));
	ppack->version = 1;
	ppack->command = (short16) ps->loginCommand;
	strncpy(ppack->from, ps->userAgent, MAX_USERID);
	strncpy(ppack->to, ps->ltpUserid, MAX_USERID);
	ppack->session = ps->loginSession;
	ppack->msgid = ps->msgCount;
	ppack->wparam = ps->myPriority;
	
	ppack->contactIp = 0;
	ppack->lparam = ps->ltpPresence;
	strncpy(ppack->data, ps->ltpLabel, 128);
	
	LTP_ltpWrite(ps, ppack, sizeof(struct ltp) + strlen(ppack->data), ps->ltpServer, (short16)(ps->bigEndian ? RTP_PORT : flip16(RTP_PORT)));
#if DEBUG
	printf("lgn %02d %03d %s %s contact:%u to %s %d\n", ppack->command, ppack->response, ppack->from, ppack->to, 
		   ppack->contactIp, netGetIPString(ps->ltpServer), RTP_PORT);
#endif
}


/*loginCancel:
 cancels any ongoing login or logout */
void LTP_ltpLoginCancelLtp(struct ltpStack *ps)
{
	if (ps->loginStatus == LOGIN_STATUS_TRYING_LOGIN)
	{
		ps->ltpPassword[0] = 0;
		ps->loginStatus = LOGIN_STATUS_OFFLINE;
		ps->myPriority = NORMAL_PRIORITY;
	}
	
	ps->loginRetryCount = 0;
	ps->loginNextDate = ps->now + LTP_LOGIN_INTERVAL;
}

/* callTick:
 This is called from a timer function every second. Don't panic
 if you can't accurately call this every second. This is just an approximation.
 
 It is called only on non-idle channels
 */

static void LTP_callTick(struct ltpStack *ps, struct Call *pc)
{
	struct ltp *p = (struct ltp *)pc->ltpRequest;
	//struct Call *pcactiveP;
	
	/** farhan, aug 20 2009, bug id 23023
	 the call on hold was getting dropped by the asterisk side as there were no media packets received at all.
	 instead, 
	 */
	/*int noconfOn=1;
	if(ps->activeLine>=0 && ps->activeLine<ps->maxSlots)
	{	
		pcactiveP = ps->call + ps->activeLine;
		noconfOn = !pcactiveP->InConference;
		
	}*///(noconfOn || pc->InConference == 0)
		if (pc->ltpState == CALL_CONNECTED && pc->InConference==0 && ps->activeLine != pc->lineId){
		
		short silence[160] = {0};
		LTP_rtpOut(ps, pc, 160, silence, 0);
	}
	
	if (pc->ltpState == CALL_RING_RECEIVED && pc->timeStart + ps->ringTimeout < ps->now)
	{
		//added by mukesh to remove hungup button
		pc->ltpState = CALL_IDLE;
		alert(pc->lineId, ALERT_DISCONNECTED, "Missed");
		ltpRefuse(ps, pc->lineId, "missed");
		return;
	}
	
	/* if no packet out is pending, then skip this slot */
	
	if (pc->retryCount == 0)
	{
		if (pc->ltpState == CALL_HANGING)
			pc->ltpState = CALL_IDLE;
		return;
	}
	
	/* complain if the retrycount dips below 0: shouldn't happen */
	if (pc->retryCount < 0)
	{
#if DEBUG
		printf("retryCount = %d for call to %s, state=%d\n", 
			   pc->retryCount, pc->remoteUserid, pc->ltpState);
#endif
		if (pc->ltpState == CALL_HANGING)
			pc->ltpState = CALL_IDLE;
		return;
	}
	
	/* retry not due yet? */
	if (ps->now < pc->retryNext)
		return;
	
	/* added on feb 11, 2006. the packets are proxied
	 by force if they were initiated before onRing decided
	 to go through the proxy
	 */
	if (pc->fwdIP && !p->contactIp)
	{
		p->contactIp = pc->fwdIP;
		p->contactPort = pc->fwdPort;
	}
	/* the message is already lying in the ltpRequest array, just send it out again */
	LTP_ltpWrite(ps, (struct ltp *)pc->ltpRequest, pc->ltpRequestLength, pc->remoteIp, pc->remotePort);
	
#if DEBUG
	
	printf("outr %02d %03d %s %s to %s %d\n", p->command, 
		   p->response, p->from, p->to, netGetIPString(pc->remoteIp), ntohs(pc->remotePort));
#endif
	/* decrement the request retry count */
	pc->retryCount--;
	if (!pc->retryCount)
		callTimeOut(pc);
	else
		pc->retryNext = ps->now + 5;
}


/* tick:
 Make you system timer call this every second (approximately).
 If the system freezes for a few seconds, dont panic, nothing bad is going to happen, relax.
 
 the timeNow is the current julian date (google out the definition). typically, time(NULL)
 will return this on a system with standard c lib support */
void LTP_ltpTick(struct ltpStack *ps, unsigned int timeNow)
{
	int		i;
	struct Call	*p;
	int soundRequired;
	//	struct Contact *pcon;
	
	// we'd normally call time(NULL) here but some crazy systems don't support it 
	ps->now = timeNow;
	
	// login() does not do something every second,
	// usually retuns quickly if it is not in the middle of a login 
	ltpLogin(ps, 0);
	
	// we keep writing a very small packet to our network to keep our NAT session alive
	// through a proxy. this is set to 2 minutes at the moment (120 seconds) 
	
	//it is probably intefering with gprs 2006/11/4
	if (ps->loginStatus == LOGIN_STATUS_ONLINE && ps->nat && ps->dateNextEcho < ps->now)
	{
		LTP_ltpWrite(ps, (struct ltp *)&ps->loginSession, 4, ps->ltpServer, (short16)(ps->bigEndian ? RTP_PORT : flip16(RTP_PORT)));
		ps->dateNextEcho = timeNow + 60;
	}
	
	
	
	/* give each non-idle call a slice of time */
	soundRequired = 0;
	for (i =0; i < ps->maxSlots; i++)
	{
		p = ps->call + i;
		if (p->ltpState == CALL_IDLE)
			continue;
		
		LTP_callTick(ps, p);
		
		/* check if any of the calls still require the sound card */
		if (p->ltpState == CALL_CONNECTED)
			soundRequired = 1;
	}
	
	
	
	//every five seconds, retry sending pending messages
	if ((timeNow % 5)== 0)
		for (i=0; i < MAX_CHATS; i++){
			struct Message *pmsg = ps->chat + i;
			if (pmsg->retryCount <= 0)
				continue;
			LTP_ltpWrite(ps, (struct ltp *)pmsg->outBuff, pmsg->length, pmsg->ip, pmsg->port);
			pmsg->retryCount--;
			if (!pmsg->retryCount)
				alert(-1, ALERT_MESSAGE_ERROR, pmsg);
		}
	
	// closeSound to be called by alert_disconnected
	/* free up the card if required 
	 if (!soundRequired)
	 closeSound(); */
}


/* 
 onChallenge:
 any request can be challenged by the server.
 the challenge works like this ...
 First: you send server a message, the server responds back 
 with a) response code 407 and b) some random bits in authenticate field (called the challenge)
 Second: you take an md5 of the entire original request + your password + the challenge 
 put the md5 checksum in the authenticate field and resend the request back to the server.
 
 As the server knows your password and the random challenge it had issued, it can check if the md5
 is proper by recalcuating it and hence authenticates you without you sending your password over
 the pubic Internet.
 
 mainly used with login attempts
 
 dont know what is md5 and challenge? read the RFC on http digest authentication 
 */
static void LTP_onChallenge(struct ltpStack *ps, struct ltp *response, unsigned int fromip, unsigned short fromport)
{
	struct  MD5Context      md5;
	unsigned char digest[16];
	unsigned int32		tempIp;
	unsigned short16 tempPort;
	
	/* back up authenticate field of the response
	 zero authenticate field as it was in the orignal request
	 the challenge issued by the server is usually called 'Nonce' (short for nonsense)
	 as it contains random bit pattern that is unpredictable */
	if (response->command == CMD_LOGIN || response->command == CMD_LOGOUT)
	{
		zeroMemory(ps->ltpNonce, 16);
		strncpy(ps->ltpNonce, response->authenticate, 16);
	}
	
	zeroMemory(response->authenticate, 16);
	zeroMemory(&md5,sizeof(md5));
	
	/* convert the response back into request by resetting the response code */
	response->response = 0;
	
	/* we exclude the contact info from digest as
	 the contact info field can be legitimately rewritten by proxies and the server
	 THOUGHT: should the redirected ip/port be stored somewhere?
	 */
	tempIp = response->contactIp;
	tempPort = response->contactPort;
	
	response->contactIp = 0;
	response->contactPort = 0;
	
	/* flip to the original shape */
	if (ps->bigEndian)
		ltpFlip(response);
	
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char *)response, sizeof(struct ltp) + strlen(response->data), ps->bigEndian);
	MD5Update(&md5, (unsigned char *)ps->ltpPassword, strlen(ps->ltpPassword), ps->bigEndian);
	MD5Update(&md5, (unsigned char *)ps->ltpNonce, 16, ps->bigEndian);
	MD5Final(digest,&md5);
	
	/* back to the host byte-order */
	if (ps->bigEndian)
		ltpFlip(response);
	
	/* put the md5 response to the challenge back into the */ 
	memcpy(response->authenticate, digest, 16);
	
	/* restore the contact info back to the message */
	response->contactIp = tempIp;
	response->contactPort = tempPort;
	
	LTP_ltpWrite(ps, response, sizeof(struct ltp) + strlen(response->data), fromip, fromport);
	
#if DEBUG
	printf("out challenge response %02d %03d %s %s from %s %d: %s\n", response->command, response->response, response->from, 
		   response->to, netGetIPString(fromip), ntohs(fromport), response->authenticate);
#endif
}

/* loginResponse:
 A login response can be on of the four:
 
 RESPONSE_OK: login has suceeded and you are logged in
 RESPONSE_REDIRECT: login has to be tried at the end point indicated in contact fields
 RESPONSE_AUTHENTICATION_REQUIRED: login has been challenged with a challenge set in the authenticate field
 40x: login has failed.
 
 AUTHENTICATE_REQUIRED is universally handled for all incoming messages within the universal onResponse()
 hence it is not handled here
 */
void LTP_loginResponse(struct ltpStack *ps, struct ltp *msg)
{
    int i;
	//	char	*p, *q;
	
	/* server redirection?
	 the server might ask us to retry login in through a proxy or a different server (different domain) */
	if (msg->response == RESPONSE_REDIRECT)
	{
		ps->ltpServer = msg->contactIp;
		msg->response = 0;
		LTP_ltpWrite(ps, msg, sizeof(struct ltp) + strlen(msg->data), ps->ltpServer, (short16)(ps->bigEndian? RTP_PORT : flip16(RTP_PORT)));
		return;
	}
	
	/* set the next login date and reset the current retry count */
	ps->loginNextDate = ps->now + LTP_LOGIN_INTERVAL;
	ps->loginRetryCount = 0;
	
	/* check if this was a response to a pending request */
	if (msg->response == RESPONSE_OK)
	{
		/* store the authenticate field just in case */
		strncpy(ps->ltpNonce, msg->authenticate, 16);
		
		/* usually there is a message of the day with each login */
		strncpy(ps->ltpTitle, msg->data, MAX_TITLE);
		for (i = 0; i < sizeof(ps->motd)-1 && msg->data[i] > 0; i++)
			ps->motd[i] = msg->data[i];
		ps->motd[i] = 0;
		
		/* if not already logged in, make some song and dance about it (alert!)*/
		if (ps->loginStatus != LOGIN_STATUS_ONLINE)
			alert(-1, ALERT_ONLINE, "Online");
		
		ps->loginStatus = LOGIN_STATUS_ONLINE;
		ps->myPriority = NORMAL_PRIORITY;
	}
	else if (msg->response == RESPONSE_BUSY)
	{
		ps->loginStatus = LOGIN_STATUS_BUSY_OTHERDEVICE;
		ps->loginNextDate = ps->now + LTP_LOGIN_INTERVAL;
		ps->loginRetryCount = 0;
		ps->myPriority = NORMAL_PRIORITY;
		
		alert(-1, ALERT_OFFLINE, msg->data);
	}
	else
	{
		ps->loginStatus = LOGIN_STATUS_FAILED;
		ps->loginNextDate = ps->now + LTP_LOGIN_INTERVAL;
		ps->loginRetryCount = 0;
		ps->myPriority = NORMAL_PRIORITY;
		
		alert(-1, ALERT_OFFLINE, msg->data);
	}
}

/*
 logoutResponse:
 It is pretty trivial right now, assume that you are logged out in anycase upon
 a response.
 */
static void LTP_logoutResponse(struct ltpStack *ps, struct ltp *msg)
{
	/* server redirection?
	 the server might ask us to retry logout in through a proxy or a different server (different domain) */
	if (msg->response == RESPONSE_REDIRECT)
	{
		ps->ltpServer = msg->contactIp;
		msg->response = 0;
		LTP_ltpWrite(ps, msg, sizeof(struct ltp) + strlen(msg->data), ps->ltpServer, (short16)(ps->bigEndian? RTP_PORT : flip16(RTP_PORT)));
		return;
	}
	
	ps->loginNextDate = ps->now + LTP_LOGIN_INTERVAL;
	ps->loginRetryCount = 0;
	
	ps->loginStatus = LOGIN_STATUS_OFFLINE;
	ps->myPriority = NORMAL_PRIORITY;
	
	ps->ltpUserid[0] = 0;
	ps->ltpPassword[0] = 0;
	ps->ltpLabel[0] = 0;
	ps->ltpTitle[0] = 0;
	
	/* check if this was a response to a pending request */
	if (msg->response == RESPONSE_OK)
		alert(-1, ALERT_OFFLINE, "");
	else
		alert(-1, ALERT_OFFLINE, msg->data);
}



static void  LTP_messageResponse(struct ltpStack *ps, struct ltp *pack, unsigned int32 ip, unsigned short16 port)
{
	struct Message *pchat=NULL;
	
	pchat = LTP_getMessage(ps, pack->to);
	if (!pchat)
		return;
	
	//if this is a confirmation of some other message, skip it.
	if (pchat->lastMsgSent != pack->msgid)
		return;
	
	//if we are redirected directly to the peer, so be it.
	if (pack->response == RESPONSE_REDIRECT){
		pchat->ip = pack->contactIp;
		pchat->port = pack->contactPort;
		LTP_ltpWrite(ps, pack, pchat->length, pchat->ip, pchat->port);
		return;
	}
	
	//this is a successful response
	pchat->retryCount = 0;
	pchat->ip = ip;
	pchat->port = port;
	pchat->dateLastMsg = ps->now;
	
	if (pack->contactIp){
		pchat->fwdip = pack->contactIp;
		pchat->fwdport = pack->contactPort;
	}
	
	if (pack->response >= 400)
		alert(-1, ALERT_MESSAGE_ERROR, pack);
	else
		alert(-1, ALERT_MESSAGE_DELIVERED, pack);
}


/*
 ringResponse:
 ring response is one of the trickiest part of the LTP protocol.
 Try understanding what it does before hacking into it.
 
 It is best if you read the protocol definiton before dipping into it.
 
 Here is what happens:
 ring response is received usually after having despatched one or more ring
 command messages to the server, the server would have redirected you (by a 
 RESPONSE_REDIRECT : 302 response either directly the remote party's end-point or to a relay 
 that the remote party is logged in from.
 
 The 302 response (redirection) is handled by onResponse() which calls ringResponse
 only if the response was either a 20x or a 40x message.
 
 The ringResponse might be directly coming in from the remote end-point rather than 
 from the end-point that you sent your original ring command to. Hence, the
 standard callMatch() function wont' work and we use a slightly less strict algorithm
 to find a matching call for this ring response.
 
 If the response is 40x we assume the call failed and end the call right there.
 
 If the response is a RESPONSE_OK, then it means we were able to get a direct hit from the
 called end-point. and hence we set the directMedia flag which tells us that we are
 able to send packets between each other without requiring a relay.
 
 
 The remote party will try sending a couple of RESPONSE_OK responses to check if it can 
 make through directly, in the end it will send a RESPONSE_OK_RELAYEd response (meaning, relaying
 is required).
 
 It might happen that we get a RESPONSE_OK_RELAYED as well RESPONSE_OK response in which case, 
 directMedia flag helps us avoid unecessary relaying.
 */
static void  LTP_ringResponse(struct ltpStack *ps, struct ltp *pack, unsigned int ip, unsigned short port)
{
	int i;
	struct Call *pc=NULL;
	
	for (i = 0; i < ps->maxSlots; i++)
	{
		pc = ps->call + i;
		if (!strcmp(pc->remoteUserid, pack->to) && pc->ltpSession == pack->session)
			break;
	}
	
	if (i == ps->maxSlots || !pc)
		return;
	
	/* stop sending more ring requests */
	if(pc->ltpState == CALL_RING_SENT)
		LTP_callStopRequest(pc);
	
	if (pack->response >= 400)
	{
		if (pc->ltpState != CALL_IDLE)
		{
			pc->kindOfCall = CALLTYPE_MISSED;
			if (pack->response == RESPONSE_OFFLINE)
				pc->kindOfCall = CALLTYPE_OFFLINE;
			else if (pack->response == RESPONSE_NOT_FOUND)
				pc->kindOfCall = CALLTYPE_BADADDRESS;
			else
				pc->kindOfCall = CALLTYPE_BUSY;
		}
		pc->ltpState = CALL_IDLE;
		alert(pc->lineId, ALERT_DISCONNECTED, pack->data);
	}
	else
	{
		if (pack->response == 202 && !pc->directMedia)
		{
			pc->remoteIp = ip;
			pc->remotePort = port;
			pc->fwdIP = pack->contactIp;
			pc->fwdPort = pack->contactPort;
			if (pc->ltpState == CALL_RING_SENT)
				pc->ltpState = CALL_RING_ACCEPTED;
			alert(pc->lineId, ALERT_RINGING, pack->data);			
		}
		
		if (pack->response == RESPONSE_OK)
		{
			pc->remoteIp = ip;
			pc->remotePort = port;
			pc->fwdIP = 0;
			pc->fwdPort = 0;
			if (pc->ltpState == CALL_RING_SENT)
				pc->ltpState = CALL_RING_ACCEPTED;
			pc->directMedia = 1;
			alert(pc->lineId, ALERT_RINGING, pack->data);			
		}
	}
}

/*
 hangupResponse:
 here regardless of what the other side says, you wanted to drop the call
 you have asked the other side to do that, now that the message has made its
 way to the other side, forget about it */
static void LTP_hangupResponse(struct ltpStack *ps, struct Call *pc /*, struct ltp *pack, int ip, short port*/)
{
	/* reset the request state */
	LTP_callStopRequest(pc);
	
	pc->ltpState = CALL_IDLE;
	pc->timeStop = ps->now;
	//20070722 changed the data to what the remote is saying
	alert(pc->lineId, ALERT_DISCONNECTED, "");
}




/* onResponse:
 All the response packet are handled by this big switch.
 The challenge, redirect etc are all handled similarly and hence 
 they all branch to the same functions.
 there are specific handlers for individual requests
 */

/* onResponse:
 All the response packet are handled by this big switch.
 The challenge, redirect etc are all handled similarly and hence 
 they all branch to the same functions.
 there are specific handlers for individual requests
 */
static void LTP_onResponse(struct ltpStack *ps, struct ltp *pack, unsigned int ip, unsigned short port)
{
	struct Call	*pc;
	
	if (pack->response == RESPONSE_AUTHENTICATION_REQUIRED)
	{
		LTP_onChallenge(ps, pack, ip, port);
		return;
	}
	
	if (pack->command == CMD_LOGIN)
	{
		LTP_loginResponse(ps, pack);
		return;
	}
	
	if (pack->command == CMD_LOGOUT)
	{
		LTP_logoutResponse(ps, pack);
		return;
	}
	
	if (pack->command == CMD_MSG){
		LTP_messageResponse(ps, pack, ip, port);
		return;
	}
	
	if(pack->response == RESPONSE_REDIRECT)
	{
		LTP_redirect(ps, pack, ip, port);
		return;
	}
	
	/* ring response will not match ordinarily */ 
	if(pack->command == CMD_RING || pack->command == CMD_TALK)
	{
		LTP_ringResponse(ps, pack, ip, port);
		return;
	}
	
	/* the rest of the messages are call slot specific, hence we need to match this
	 with specific calls*/
	
	pc = LTP_callMatch(ps, pack, ip, port, pack->session);
	if (!pc)
		return;
	
	switch(pack->command)
	{
		case CMD_ANSWER:
			if (pack->response == RESPONSE_OK)
			{
				pc->ltpState = CALL_CONNECTED;
				/* pc->timeStart = now; */
				
				// the sound has already been opened on call ltpAnswer, it is pointless opening it again on response to it
				//if (!openSound(1))
				//	alert(-1, ALERT_ERROR, "Trouble with sound system");
			}
			else
			{
				if (pc->ltpState != CALL_IDLE)
				{
					if (pc->ltpState != CALL_IDLE)
					{
						pc->kindOfCall = CALLTYPE_MISSED;
						if (pack->response == RESPONSE_OFFLINE)
							pc->kindOfCall = CALLTYPE_OFFLINE;
						else if (pack->response == RESPONSE_NOT_FOUND)
							pc->kindOfCall = CALLTYPE_BADADDRESS;
						else
							pc->kindOfCall = CALLTYPE_BUSY;
					}
				}
				pc->ltpState = CALL_IDLE;
			}
			break;
			
		case CMD_REFUSE:
			if (pc && pc->ltpState == CALL_HANGING)
			{
				pc->kindOfCall |= CALLTYPE_MISSED;
				pc->ltpState = CALL_IDLE;
				alert(pc->lineId, ALERT_DISCONNECTED, "Refused");
			}
			break;
			
		case CMD_HANGUP:
			LTP_hangupResponse(ps, pc/*, pack, ip, port*/);
			break;		
	}
	
	LTP_callStopRequest(pc);
}

/*
 sendResponse:
 This sends a response to a packet with a response code and an optional message in the data field
 
 responses are sent always and only upon receiving a request. the response are never retransmitted
 if the remote side sending the request fails to send receive the response, 
 the remote retransmit the original request and the response will be resent */
static void sendResponse(struct ltpStack *ps, struct ltp *pack, short16 responseCode, char *message, unsigned int ip, unsigned short16 port)
{
	char scratch[1000];
	struct ltp *p;
	
	p = (struct ltp *) scratch;
	memcpy(p, pack, sizeof(struct ltp));
	strncpy(p->data, message, 256);
	p->response = responseCode;
	
#if DEBUG
	printf("response cmd:%02d/%03d %s %s sent to %s %d\n",  p->command, p->response, p->from, 
		   p->to, netGetIPString(ip), ntohs(port));
#endif
	LTP_ltpWrite(ps, p, sizeof(struct ltp) + strlen(p->data), ip, port);
}

/*onRing:
 A  big one, read this along with the ringResponse comments.
 
 1. For a fresh call, first a struct Call is allocated from the pool.
 2. If the contact fields are set, it means, it has been relayed. Hence, the
 fwd fields are set in the Call. Otherwise, we know it is direct packet from the caller
 and hence, we set directMedia flag in the Call to '1'.
 3. For relayed rings, unless we are forced to proxy (forceProxy set), we try punch a UDP
 hole for at least three times before deciding use the relay.
 4. This function compares it's own default Codec with the codec recommended by the call
 and tries arriving at a compromise.
 */
static struct Call *LTP_onRing(struct ltpStack *ps, struct ltp *ppack, unsigned int fromip, unsigned short fromport)
{
	//struct Contact *pcon = NULL;
	struct Call *pc=NULL;
	int	i;
	short responseCode = RESPONSE_OK;
	
	for (i = 0; i < ps->maxSlots; i++)
	{
		pc = ps->call + i;
		if (pc->ltpSession == ppack->session && !strcmp(ppack->from, pc->remoteUserid)
			&& pc->ltpState != CALL_IDLE)
			break;
		pc = NULL;
	}
	
	
	if (pc)
	{
		if (pc->ltpState == CALL_HANGING)
		{
			sendResponse(ps, ppack, RESPONSE_BUSY, "Busy", fromip, fromport);
			return pc;
		}
		
		/* if direct media is set */
		if (pc->directMedia)
		{
			sendResponse(ps, ppack,RESPONSE_OK,"OK", pc->remoteIp, pc->remotePort);
			return pc;
		}
		
		/* did we get this directly from the remote end */
		if (!ppack->contactIp)
		{
			pc->remoteIp = fromip;
			pc->remotePort = fromport;
			pc->fwdIP = 0;
			pc->fwdPort = 0;
			pc->directMedia = 1;
			/* directly try responding to the ring's originaiting ep */ 
			sendResponse(ps, ppack, RESPONSE_OK, "OK", pc->remoteIp, pc->remotePort);
			return pc;
		}
		
		/* further processing only for packets being relayed (contactIp set)
		 and directmedia not yet set */
		pc->nRingMessages++;
		
		/* try sending it off to the remote end-point event */
		if (pc->nRingMessages < 3 && !ps->forceProxy)
		{
			/* directly try responding to the ring's originaiting ep  */
			sendResponse(ps, ppack, RESPONSE_OK, "OK", pc->remoteIp, pc->remotePort);
			return pc;
		}
		/* we have tried this three times already, lets give up trying directly
		 and request for proxy*/
		else
		{
			pc->fwdIP =  ppack->contactIp;
			pc->fwdPort = ppack->contactPort;
			pc->remoteIp = fromip;
			pc->remotePort = fromport;
			
			ppack->response = RESPONSE_OK_RELAYED;
			sendResponse(ps, ppack, 202, "OK", fromip, fromport);
#if DEBUG
			
			puts("* call set to proxy and set through proxy server ");
#endif
			return pc;
		}
	}
	else
	{
		/* this is a new call */
		pc = LTP_callFindIdle(ps);
		if (!pc)
		{
			sendResponse(ps, ppack, RESPONSE_BUSY,"Busy", fromip, fromport);
			return NULL;
		}
		
		LTP_callInit(pc, ps->defaultCodec);
		
		strncpy(pc->remoteUserid, ppack->from, MAX_USERID);
		pc->ltpSession = ppack->session;
		pc->ltpSeq = 0;
		pc->ltpState = CALL_RING_RECEIVED;
		pc->kindOfCall = CALLTYPE_IN | CALLTYPE_CALL;	
		strncpy(pc->remoteUserid, ppack->from, MAX_USERID);
		strncpy(pc->title, ppack->data, MAX_TITLE);
		pc->timeStart = ps->now;
		pc->timeEllapsed = -1;
		
		if (ppack->command == CMD_TALK)
			pc->inTalk = 1;
		
		if(ppack->contactIp)
		{
			pc->remoteIp = ppack->contactIp;
			pc->remotePort = ppack->contactPort;
		}
		else
		{
			/* this means, we have a direct hit. */
			pc->remoteIp = fromip;
			pc->remotePort = fromport;
			pc->directMedia = 1;
		}
		
		/* negotiate the codec */
		if (ppack->wparam ==  LTP_CODEC_SPEEX && ps->defaultCodec == LTP_CODEC_SPEEX)
			pc->codec = LTP_CODEC_SPEEX;
		else
			pc->codec = LTP_CODEC_GSM;
		
		ppack->wparam = pc->codec;	
		
		if (ps->forceProxy)
		{
			pc->fwdIP =  ppack->contactIp;
			pc->fwdPort = ppack->contactPort;
			pc->remoteIp = fromip;
			pc->remotePort = fromport;
			responseCode = 202;
		}
		
		sendResponse(ps, ppack, responseCode, "OK", pc->remoteIp, pc->remotePort);
		alert(pc->lineId, ALERT_INCOMING_CALL, pc->remoteUserid);
	}
	return pc;
}

/*
 onAnswer:
 We have sent a ring request and we might or might not have received a ringResponse yet
 but if the remote has answered, so we need to move to that CALL_CONNECTED state and
 open up the media streams.
 */
static void LTP_onAnswer(struct ltpStack *ps, struct ltp *ppack, unsigned int fromip, unsigned short fromport)
{
	struct Call	*pc;
	
	pc = LTP_callMatch(ps, ppack, fromip, fromport, ppack->session);
	
	if (!pc)
	{
		sendResponse(ps, ppack, RESPONSE_NOT_FOUND, "Not found", fromip, fromport);
		return;
	}
	
	pc->remoteIp = fromip;
	pc->remotePort = fromport;
	if (strlen(ppack->data) > 0)
		strncpy(pc->title, ppack->data, MAX_TITLE);
	
	/* if the call is an earlier state, then transit to CALL_CONNECTED */
	if (pc->ltpState == CALL_RING_ACCEPTED || pc->ltpState == CALL_RING_SENT)
	{
		LTP_callStopRequest(pc);
		pc->ltpState = CALL_CONNECTED;
		pc->timeStart = ps->now;
		pc->timeEllapsed=0;
		//Tasvir Rohila, 17/7/2009, bug#21083, latest call to be established should become active.
		ps->activeLine = pc->lineId;
		
		pc->kindOfCall = CALLTYPE_OUT | CALLTYPE_CALL;
		
		// alert_connected opens sound within the app
		//if (!openSound(1))
		//	alert(-1, ALERT_ERROR, "Trouble with sound system");
		
		alert(pc->lineId, ALERT_CONNECTED, pc->title);
	}
	
	/* we assume that ANSWER request carries a codec that is satisfactory to both sides
	 this is because ANSWER request is always from the actual end-point */
	pc->codec = ppack->wparam;
	
	ppack->contactIp = pc->fwdIP;
	ppack->contactPort = pc->fwdPort;
	
	sendResponse(ps, ppack, RESPONSE_OK, "OK", fromip, fromport);
}


/* onRefuse:
 If we get a refuse request for a non-existing call, just be polite and accept it.
 The other side might have missed an earlier response sent by you and you would have
 released the call structure in anycase */
static void LTP_onRefuse(struct ltpStack *ps, struct ltp *ppack, unsigned int fromip, unsigned short fromport)
{
	struct Call	*pc;
	
	pc = LTP_callMatch(ps, ppack, fromip, fromport, ppack->session);
	
	if (!pc)
	{
		sendResponse(ps, ppack, RESPONSE_NOT_FOUND, "Not found", fromip, fromport);
		return;
	}
	
	ppack->contactIp = pc->fwdIP;
	ppack->contactPort = pc->fwdPort;
	pc->timeStop = ps->now;
	alert(pc->lineId, ALERT_DISCONNECTED, "Busy");
	pc->ltpState = CALL_IDLE;
	pc->ltpSession = 0;
	sendResponse(ps, ppack, RESPONSE_OK, "OK", fromip, fromport);
	
}

/* onRefuse:
 If we get a hnagup request for a non-existing call, just be polite and accept it.
 The other side might have missed an earlier response sent by you and you would have
 released the call structure in anycase */

static void LTP_onHangup(struct ltpStack *ps, struct ltp *ppack, unsigned int fromip, unsigned short fromport)
{
	struct Call	*pc;
	
	pc = LTP_callMatch(ps, ppack, fromip, fromport, ppack->session);
	
	if (!pc)
	{
		sendResponse(ps, ppack, RESPONSE_OK, "OK", fromip, fromport);
		return;
	}
	
	ppack->contactIp = pc->fwdIP;
	ppack->contactPort = pc->fwdPort;
	pc->timeStop = ps->now;
	//added by mukesh to remove hungup button
	
	if (pc->ltpState != CALL_IDLE)
	{
		if (pc->ltpState != CALL_CONNECTED)
			pc->kindOfCall |= CALLTYPE_MISSED;
	}
	pc->ltpState = CALL_IDLE;
	//20070722 changed the data to what the remote is saying
	alert(pc->lineId, ALERT_DISCONNECTED, ppack->data);
	pc->ltpState = CALL_IDLE;
	pc->ltpSession = 0;
	
	sendResponse(ps, ppack, RESPONSE_OK, "OK", fromip, fromport);
}

static void LTP_onMessage(struct ltpStack *ps, struct ltp *ppack, unsigned int fromip, unsigned short fromport)
{
	struct Message *pmsg;
	//int iContact=-1;
	unsigned short16 prevport;
	unsigned int32	previp;
	
	if (strcmp(ps->ltpUserid, ppack->to))
		return;
	
	pmsg = LTP_getMessage(ps, ppack->from);
	previp = pmsg->fwdip;
	prevport = pmsg->fwdport;
	
	/* fresh notification? */
	if (pmsg->lastMsgRecvd != ppack->msgid || pmsg->session != ppack->session)
	{
		pmsg->lastMsgRecvd = ppack->msgid;
		pmsg->session = ppack->session;
		pmsg->dateLastMsg = ps->now;
		alert(-1, ALERT_MESSAGE, ppack);
	}
	
	pmsg->ip	= fromip;
	pmsg->port	= fromport;
	
	/* relayed message? */
	if (ppack->contactIp)
	{
		pmsg->fwdip = ppack->contactIp;
		pmsg->fwdport = ppack->contactPort;
	}
	else {
		pmsg->fwdip = 0;
		pmsg->fwdport = 0;
	}
	
	sendResponse(ps, ppack, 202, ps->ltpTitle, fromip, fromport);
}


#if DEBUG
void LTP_ltpTrace(struct ltp *msg)
{
	switch (msg->command)
	{
        case CMD_RING:
			printf("RING ");
			break;
        case CMD_NEWUSER:
			printf("NEWUSER ");
			break;
        case CMD_LOGIN:
			printf("LOGIN ");
			break;
        case CMD_LOGOUT:
			printf("LOGOFF ");
			break;
        case CMD_MSG:
			printf("MSG ");
			break;
        case CMD_ANSWER:
			printf("ANSWER ");
			break;
        case CMD_HANGUP:
			printf("HANGUP ");
			break;
        case CMD_CANCEL:
			printf("CANCEL ");
			break;
        case CMD_NOTIFY:
			printf("NOTIFY ");
			switch(msg->wparam)
		{
			case NOTIFY_ONLINE: printf(" ONLINE "); break;
			case NOTIFY_SILENT: printf(" SILENT "); break;
		}
			break;
        default:
			printf("CMD(%d) ", (int)msg->command);
	}
	printf(" /%d to:%s from:%s msgid:%d session:%d contact:%s:%d <%s>\n", (int)msg->response,
		   msg->to, msg->from, msg->msgid, msg->session, netGetIPString(msg->contactIp), 
		   flip16(msg->contactPort), msg->data);
}
#endif


/* ltpIn:
 Establishes entry point for all incoming LTP packets (the RTP are handled separately)
 */
static void LTP_ltpIn(struct ltpStack *ps, unsigned int fromip, short unsigned fromport, char *buffin, int len)
{
	struct ltp *ppack;
	struct Call *pc;
	
	/* we are using a single buffer to handle all incoming ltp packets. 
	 This means, when we multi-thread, we have to use a thread-specific buffer */
	memcpy(ps->inbuff, buffin, len);
	
	if (len < sizeof(struct ltp))
		return;
	
	ppack = (struct ltp *) ps->inbuff;
	
	/* flip it in place to the host's byte order */
	if (ps->bigEndian)
		ltpFlip(ppack);
	
#if DEBUG
	
	printf("RECV: [%s:%d]\r\n", netGetIPString(fromip), ntohs(fromport));
	ltpTrace(ppack);
	
#endif
	if (ppack->version != 1)
		return;
	
	if (ppack->response)
	{
		LTP_onResponse(ps, ppack, fromip, fromport);
		return;
	}
	
	
	/* handle incoming requests only when logged in. */
	if (ps->loginStatus != LOGIN_STATUS_ONLINE)
		return;
	
    /* handle the incoming message, based on the message type */ 
	switch(ppack->command)
	{
		case CMD_RING:
			LTP_onRing(ps, ppack, fromip, fromport);
			break;
		case CMD_TALK:
			pc = LTP_onRing(ps, ppack, fromip, fromport);
			if (pc)
				pc->inTalk;
			break;
		case CMD_ANSWER:
			LTP_onAnswer(ps, ppack, fromip, fromport);
			break;
		case CMD_REFUSE:
			LTP_onRefuse(ps, ppack, fromip, fromport);
			break; 
		case CMD_HANGUP:
			LTP_onHangup(ps, ppack, fromip, fromport);
			break;
		case CMD_MSG:
			LTP_onMessage(ps, ppack, fromip, fromport);
			break;
	}
}


/* ltpMessage:
 A simple request to send text in the data field of the ltp message 
 to the remote end-point in a call
 */

int LTP_ltpMessage(struct ltpStack *ps, char *userid, char *msg)
{
	struct Message *pmsg;
	struct ltp *ppack;
	
	if (strlen(msg) > 999)
		return 0;
	
	pmsg = LTP_getMessage(ps, userid);
	if (!pmsg)
		return 0;
	if (pmsg->retryCount)
		return -1;
	
	//if nothing was successfully sent or received in the last two minutes
	//reset the remote endpoint details
	if (pmsg->dateLastMsg + 120 < ps->now){
		pmsg->ip = ps->ltpServer;
		pmsg->port = ps->bigEndian ? RTP_PORT : flip16(RTP_PORT);
		pmsg->fwdip = 0;
		pmsg->fwdport = 0;
	}
	
	ppack = (struct ltp *) pmsg->outBuff;
	zeroMemory(ppack, sizeof(struct ltp));
	
	ppack->version = 1;
	ppack->command = CMD_MSG;
    ppack->msgid = ps->nextMsgID++;
	strncpy(ppack->to, userid, MAX_USERID);
	strncpy(ppack->from, ps->ltpUserid, MAX_USERID);
	strncpy(ppack->data, msg, 999);
	ppack->wparam = 1; //for replyable messages
	ppack->session = pmsg->session;
	ppack->contactIp = pmsg->fwdip;
	ppack->contactPort = pmsg->fwdport;
	
	pmsg->length = sizeof(struct ltp) + strlen(ppack->data);
	pmsg->retryCount = ps->maxChatRetries;
	pmsg->lastMsgSent = ppack->msgid;
	pmsg->dateLastMsg = ps->now;
	
	LTP_ltpWrite(ps, (struct ltp *)pmsg->outBuff, pmsg->length, pmsg->ip, pmsg->port);
	return 1;
}

/* ltpRing:
 sends a ring reqest
 It can get quite complex in the way this is received at the other
 end. read the comments on onRing()
 */

int LTP_ltpRing(struct ltpStack *ps, char *remoteid, int mode)
{
	struct Call *pc;
	struct ltp *ppack;
	
	pc = LTP_callFindIdle(ps);
	if (!pc)
	{
		alert(-1, ALERT_ERROR, "Disconnect before calling");
		return 0;
	}
	LTP_callInit(pc, ps->defaultCodec);
	
	strncpy(pc->remoteUserid, remoteid, MAX_USERID);
	pc->ltpSession = ++ps->nextCallSession;
	pc->ltpSeq = 0;
	pc->ltpState = CALL_RING_SENT;
	pc->remoteIp = ps->ltpServer;
	pc->remotePort = ps->bigEndian ? RTP_PORT : flip16(RTP_PORT);
	pc->kindOfCall = CALLTYPE_OUT | CALLTYPE_CALL;
	pc->timeStart = ps->now;
	pc->timeEllapsed = -1;
	if (mode == CMD_TALK)
		pc->inTalk = 1;
	
	ppack = (struct ltp *)pc->ltpRequest;
	
	zeroMemory(ppack, sizeof(struct ltp));
	ppack->version = 1;
	ppack->command = mode;
	strncpy(ppack->from, ps->ltpUserid, MAX_USERID);
	strncpy(ppack->to, remoteid, MAX_USERID);
	ppack->session = pc->ltpSession;
	ppack->msgid = pc->ltpSeq++;
	
	/* sub version */
	ppack->wparam = ps->defaultCodec;
	strncpy(ppack->data, ps->ltpTitle, MAX_TITLE);
	
	LTP_callStartRequest(ps, pc, NULL);
	
	return pc->lineId;
}

/* ltpAnswer, ltpRefuse and ltpHangup assume
 that the route to the remote end-point is already established
 through the CMD_RING's negotiations
 */

void LTP_ltpAnswer(struct ltpStack *ps, int lineid)
{
	struct Call *pc;
	struct ltp *ppack;
	
	if (lineid < 0 || ps->maxSlots <= lineid)
		return;
	
	pc = ps->call + lineid;
	
	if (pc->ltpState != CALL_RING_RECEIVED)
		return;
	
	ppack = (struct ltp *) pc->ltpRequest;
	zeroMemory(ppack, sizeof(struct ltp));
    ppack->version = 1;
    ppack->command = CMD_ANSWER;
    strncpy(ppack->from, ps->ltpUserid, MAX_USERID);
    strncpy(ppack->to, pc->remoteUserid, MAX_USERID);
	strncpy(ppack->data, ps->ltpTitle, MAX_USERID);
    ppack->session = pc->ltpSession;
    ppack->msgid = pc->ltpSeq++;
	ppack->wparam = pc->codec;
	
	ppack->contactIp = pc->fwdIP;
	ppack->contactPort = pc->fwdPort;
	
	pc->timeStart = ps->now;
	pc->timeEllapsed = 0;
    pc->ltpState = CALL_CONNECTED;
	alert(pc->lineId, ALERT_CONNECTED, pc->title);
	LTP_callStartRequest(ps, pc, NULL);
	
	// the app opens sound on alert_connected
	//	openSound(1);
}

void LTP_ltpRefuse(struct ltpStack *ps, int lineid, char *msg)
{
	struct Call *pc;
	struct ltp *ppack;
	
	if (lineid < 0 || ps->maxSlots <= lineid)
		return;
	
	pc = ps->call + lineid;
	
	if (pc->ltpState != CALL_RING_RECEIVED)
		return;
	
	ppack = (struct ltp *) pc->ltpRequest;
	zeroMemory(ppack, sizeof(struct ltp));
	
    ppack->version = 1;
    ppack->command = CMD_REFUSE;
    strncpy(ppack->from, ps->ltpUserid, MAX_USERID);
    strncpy(ppack->to, pc->remoteUserid, MAX_USERID);
    ppack->session = pc->ltpSession;
    ppack->msgid = pc->ltpSeq++;
	
	ppack->contactIp = pc->fwdIP;
	ppack->contactPort = pc->fwdPort;
	
    
	if (!msg)
		strncpy(ppack->data, "Busy. Please try after sometime.", MAX_TITLE);
    else
		strncpy(ppack->data, msg, MAX_TITLE);
	
    pc->ltpState = CALL_HANGING;
	pc->kindOfCall |= CALLTYPE_MISSED;
	
	LTP_callStartRequest(ps, pc, NULL);
}

void LTP_ltpHangup(struct ltpStack *ps, int lineid)
{
	struct Call *pc;
	struct ltp *ppack;
	
	if (lineid < 0 || ps->maxSlots <= lineid)
		return;
	
	pc = ps->call + lineid;
	
	if (pc->ltpState == CALL_IDLE || pc->ltpState == CALL_HANGING)
		return;
	
	if (pc->ltpState != CALL_IDLE)
	{
		if (pc->ltpState != CALL_CONNECTED)
			pc->kindOfCall |= CALLTYPE_MISSED;
	}
	
	//if there is no reply yet from the called party
	if (pc->ltpState == CALL_RING_SENT)
	{
		pc->ltpState = CALL_IDLE;
		pc->timeStop = 0;
		alert(pc->lineId, ALERT_DISCONNECTED, "noreply");
		return;
	}
	
	//set the timer
	if (pc->ltpState == CALL_CONNECTED)
		pc->timeStop = ps->now;
	else
		pc->timeStop = 0;
	
	pc->ltpState = CALL_HANGING;
	
	ppack = (struct ltp *) pc->ltpRequest;
	zeroMemory(ppack, sizeof(struct ltp));
	ppack->version = 1;
	ppack->command = CMD_HANGUP;
	ppack->session = pc->ltpSession;
    ppack->msgid = pc->ltpSeq++;
	strncpy(ppack->to, pc->remoteUserid, MAX_USERID);
	strncpy(ppack->from, ps->ltpUserid, MAX_USERID);
	
	ppack->contactIp = pc->fwdIP;
	ppack->contactPort = pc->fwdPort;
	
	
	LTP_callStartRequest(ps, pc, NULL);
	pc->retryCount = 3;
	alert(pc->lineId, ALERT_DISCONNECTED, "");
}



/*
 rtpIn:
 All the rtp packets are read in here.
 They are matched against the existing active calls
 The codec are detected from the rtp stack and the pcm is decoded
 
 If the rtp packet belongs to an active call (and it is not in confernece)
 then the pcm samples are directly sent to the outputSound routine
 
 If the rtp packets belong to a call in conference, they are simply queued up to be
 mixed and played in sync with the audio from the local source (microphone). check
 ltpSoundInput()
 */

//static unsigned int prevstamp = 0;
void LTP_rtpIn(struct ltpStack *ps, unsigned int ip, unsigned short port, char *buff, int length)
{
	int	i, nsamples=0, fwdip = 0, nframes, x;
	unsigned char	*data;
	struct Call	*pc = NULL;
	struct rtp *prtp;
	struct rtpExt *pex;
	short frame[160], fwdport = 0, prev, *p, *pcmBuff;
	
	pcmBuff = ps->rtpInBuff;
	
	prtp = (struct rtp *)buff;
	
	if(prtp->flags & RTP_FLAG_EXT)
	{
		pex = (struct rtpExt *)prtp->data;
		fwdip = pex->fwdIP;
		fwdport = pex->fwdPort;
		pc = LTP_callSearchByRtp(ps, ip, port, fwdip, fwdport, prtp->ssrc);
		data = prtp->data + RTP_EXT_SIZE;
		length -= RTP_EXT_SIZE;	
	}
	else
	{
		pc = LTP_callSearchByRtp(ps, ip, port,0,0,prtp->ssrc);
		data = prtp->data;
	}
	
	if (!pc)
		return;
	
	length -= RTP_HEADER_SIZE;
	
	if (prtp->payload & 0x80)
		pc->remoteVoice = 1;
	else
		pc->remoteVoice = 0;
	
	//now play the bloody thing!
	if ((prtp->payload & 0x7f)== GSM)
	{
		nframes = length/33;
		prev=0;
		for (i = 0; i < nframes; i++)
		{
			gsm_decode(pc->gsmIn, data+(i*33), frame);
			x=0;
			p = pcmBuff + (i * 160);
			
			for (x = 0; x < 160; x++)
				*p++ = frame[x];
			nsamples += 160;
		}
	} 
#ifdef SUPPORT_SPEEX
	else if((prtp->payload & 0x7f)== SPEEX && pc->codec < 16000) 
	{
		speex_bits_read_from(&(pc->speexBitDec), (char *)data, length);
		for (i =0; i < 4000; i += 160)
		{
			if (speex_decode_int(pc->speex_dec, &(pc->speexBitDec), frame))
				break;
			
			p = pcmBuff + i;
			for (x = 0; x < 160; x++)
				*p++ = frame[x];
			nsamples += 160;
		}
	}
	else
	{
		printf("payload different");
	}
#endif
	//play this directly if you are on the active line and you are not in conference
	if (pc->lineId == ps->activeLine && !pc->InConference)
		outputSound(ps, pc, pcmBuff, nsamples);
	else //otherwise stuff the audio up the pipe
	{
		for (i =0; i < nsamples; i++)
			queueEnque(&(pc->pcmIn), pcmBuff[i]);
	}
}

extern int lastCount;
static int LTP_rtpOut(struct ltpStack *ps, struct Call *pc, int nsamples, short *pcm, int isSpeaking)
{
	struct rtp *q;
	struct rtpExt	*pex;
	int i, j = RTP_HEADER_SIZE, x;
	short	frame[160];
	char	scratch[1000];
	
	q = (struct rtp *)scratch;
	q->flags = 0x80;
	q->sequence = ps->bigEndian ? pc->rtpSequence : flip16(pc->rtpSequence);
	q->ssrc = pc->ltpSession;
	q->timestamp = ps->bigEndian? pc->rtpTimestamp: flip32(pc->rtpTimestamp);
	pc->rtpSequence++;
	
	//add the extension if required
	
	if (pc->fwdIP)
	{
		q->flags |= RTP_FLAG_EXT;
		pex = (struct rtpExt *)q->data;
		pex->extID = ps->bigEndian ? 786 : flip16(786);
		pex->intCount = ps->bigEndian ? 2 : flip16(2);
		pex->fwdIP = pc->fwdIP;
		pex->fwdPort = pc->fwdPort;
		ps->debugCount = 73;
		
		j+= RTP_EXT_SIZE;
	}
	
	//new implementation of speex
#ifdef SUPPORT_SPEEX
	if (pc->codec == LTP_CODEC_SPEEX){
		q->payload = SPEEX;
		pc->rtpTimestamp += nsamples;
		
		for (i = 0; i < nsamples; i += 160, j+=20)
		{
			speex_bits_reset(&(pc->speexBitEnc));
			speex_encode_int(pc->speex_enc, pcm + i, &(pc->speexBitEnc));		
			speex_bits_write(&(pc->speexBitEnc), scratch+j, 1000);
		}
	}
	else
#endif
	{
		q->payload = GSM;
		pc->rtpTimestamp += nsamples;
		
		for (i = 0; i < nsamples; i += 160, j+=33)
		{
			for (x = 0; x < 160; x++)
				frame[x] = (short)pcm[i+x];
			gsm_encode(pc->gsmOut, frame, (unsigned char *)scratch+j);
		}	
	}
	
	//turn on the marker bit for voiced segments
	if (isSpeaking)
		q->payload |= 0x80;
	
	if (ps->doDebug > 3)
	{
#if DEBUG
		printf("rtp via %s:%u\r\n", 
			   netGetIPString(pc->fwdIP), flip16(pc->fwdPort));
#endif
	}
	netWrite(scratch, j, pc->remoteIp, pc->remotePort);
	
	return j;
}

/*
 ltpSoundInput:
 
 Called by the application when it has pcm samples ready.
 
 If there is no conference on, then it is easy, just pass it on to the LTP_rtpOut.
 
 The conference is done very simply, one sample is read from each of the participant's
 input pcm queue. All the samples are added up and then written back to each participant's
 output pcm queue (after subtracting that paricipant's own sample: to prevent echo).
 
 When all the participant queues are processed, the output queues are read back one 
 by one and output through LTP_rtpOut()
 */
void ltpSoundInput(struct ltpStack *ps, short *pcm, int nsamples, int isSpeaking)
{
	//count conferenceed calls
	int		count, i, j;
	int	activeInConference=0;
	short sample[100], mixer;
	struct Call	*pc=NULL;
	short pcmBuff[3200];
	
	count = 0;
	for (i=0; i < ps->maxSlots; i++)
		if (ps->call[i].InConference)
			count++;
	
	//if the activeLine is not on conference, handle it quickly
	if (ps->activeLine >= 0 && ps->activeLine < ps->maxSlots)
	{
		pc = ps->call + ps->activeLine;
		
		if (pc->inTalk)
		{
			if (isSpeaking)
				LTP_rtpOut(ps, pc, nsamples, pcm, isSpeaking);
			return;
		}
		
		
		if (!pc->InConference && pc->ltpState == CALL_CONNECTED)
			LTP_rtpOut(ps, pc, nsamples, pcm, isSpeaking);
		else if (pc->ltpState == CALL_CONNECTED)
			activeInConference = 1;
	}
	
	if (!count)
		return;
	
	zeroMemory(pcmBuff, sizeof(short) * nsamples);
	
	for (i = 0; i < nsamples; i++)
	{
		//collect and add one sample from each participant
		if (activeInConference)
			mixer = pcm[i];
		else
			mixer = 0;
		
		for (j = 0; j < ps->maxSlots; j++)
		{
			pc = ps->call + j;
			if (!pc->InConference)
				continue;
			sample[j] = queueDeque(&(pc->pcmIn)); 
			mixer += (short)sample[j];
		}
		
		if (activeInConference)
			pcmBuff[i] = (short)(mixer - pcm[i]);
		
		//now we have the mixed sample with us, replicate it back to each participant
		//after subtracting the echo
		for (j = 0; j < ps->maxSlots; j++)
		{
			pc = ps->call + j;
			if (!pc->InConference || pc->ltpState != CALL_CONNECTED)
				continue;
			
			queueEnque(&(pc->pcmOut), (short)(mixer - sample[pc->lineId]));
		}
	}
	
	//pcmBuff has temporarily the mixed output of all the remote participants
	if (activeInConference)
		outputSound(ps, ps->call + ps->activeLine, pcmBuff, nsamples);
	
	//now we have all the samples in each participant's queue
	for (i = 0; i < ps->maxSlots; i++)
	{
		pc = ps->call + i;
		
		if (pc->pcmOut.count < nsamples)
			continue;
		
		for (j = 0; j < nsamples; j++)
			pcmBuff[j] = queueDeque(&(pc->pcmOut));
		LTP_rtpOut(ps, pc, nsamples, pcmBuff, 0);
	}
}

/*
 ltpOnPacket()
 called by the application when any packet arrives at the ltp udp port
 */
void ltpOnPacket(struct ltpStack *ps, char *msg, int length, unsigned int address, unsigned short port)
{	
	if (length == 4)
		return;
	
	if (!(msg[0] & 0x80))
		LTP_ltpIn(ps, address, port, msg, length);
	else
		LTP_rtpIn(ps, address, port, msg, length);
}
void LTP_ltpMessageDTMF(struct ltpStack *ps, int lineid, char *msg)
{
	struct Call *pc;
	struct ltp *ppack;
	char *buff;
	buff = malloc(1000);
	if (lineid < 0 || ps->maxSlots <= lineid)
	{
		free(buff);	
		return;
	}
	pc = ps->call + lineid;
	
	if (pc->ltpState == CALL_IDLE || pc->ltpState == CALL_HANGING || strlen(msg) > 256)
		return;
	
	ppack = (struct ltp *) pc->ltpRequest;
	
	ppack = (struct ltp *)buff;
	zeroMemory(ppack, sizeof(struct ltp));
	ppack->version = 1;
	ppack->command = CMD_MSG;
	ppack->session = pc->ltpSession;
    ppack->msgid = pc->ltpSeq++;
	strncpy(ppack->to, pc->remoteUserid, MAX_USERID);
	strncpy(ppack->from, ps->ltpUserid, MAX_USERID);
	strncpy(ppack->data, msg, 256);
	
	LTP_callStartRequest(ps, pc, ppack);
	free(buff);	
}


























/** SIP wrapper

	this module proxies the sip callbacks and apis so that they appear consistent to the
	rest of the user agent as ltp calls and callbacks.
*/

#include <ltpmobile.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
//#define USERAGENT  "desktop-windows-d2-1.0"
#include <pjsua-lib/pjsua.h>
int bMissedCallReported = 0;

#ifdef _WIN32_WCE
#define stricmp _stricmp
#elif defined WIN32
#define stricmp _stricmp
#else
#define stricmp strcasecmp
#endif 


extern struct ltpStack *pstack;
#define THIS_FILE	"APP"
#define SIP_DOMAIN	"spokn.com"

pjsua_acc_config acccfg;

/* md5 functions :
as md5 alogrithm is sensitive big/little endian integer representation,
we have adapted the original md5 source code written by Colim Plum that
as then hacked by John Walker.
If you are studying the LTP source code, then skip this section and move onto
the queue section. */

/*typedef unsigned int32 uint32;

struct MD5Context {
		uint32 buf[4];
		uint32 bits[2];
		unsigned char in[64];
};*/

void byteReverse(unsigned char *buf, unsigned longs)
{
	uint32 t;
	do {
		t = (uint32) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
			((unsigned) buf[1] << 8 | buf[0]);
		*(uint32 *) buf = t;
		buf += 4;
	} while (--longs);
}

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
		( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
void MD5Transform(uint32 *buf, uint32 *in)
{
	register uint32 a, b, c, d;

	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];

	MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
	MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
	MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
	MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
	MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
	MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
	MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
	MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
	MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
	MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
	MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
	MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
	MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
	MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
	MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
	MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
	MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
	MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
	MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
	MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
	MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
	MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
	MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}

/*
 * Start MD5 accumulation.	Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void MD5Init(struct MD5Context *ctx)
{
	ctx->buf[0] = 0x67452301;
	ctx->buf[1] = 0xefcdab89;
	ctx->buf[2] = 0x98badcfe;
	ctx->buf[3] = 0x10325476;

	ctx->bits[0] = 0;
	ctx->bits[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void MD5Update(struct MD5Context *ctx, unsigned char const  *buf, unsigned len, int isBigEndian)
{
	uint32 t;

	/* Update bitcount */

	t = ctx->bits[0];
	if ((ctx->bits[0] = t + ((uint32) len << 3)) < t)
		ctx->bits[1]++; 		/* Carry from low to high */
	ctx->bits[1] += len >> 29;

	t = (t >> 3) & 0x3f;		/* Bytes already in shsInfo->data */

	/* Handle any leading odd-sized chunks */

	if (t) {
		unsigned char *p = (unsigned char *) ctx->in + t;

		t = 64 - t;
		if (len < t) {
			memcpy(p, buf, len);
			return;
		}
		memcpy(p, buf, (size_t) t);
		if (isBigEndian)
			byteReverse(ctx->in, 16);
		MD5Transform(ctx->buf, (uint32 *) ctx->in);
		buf += t;
		len -= (int) t;
	}
	/* Process data in 64-byte chunks */

	while (len >= 64) {
		memcpy(ctx->in, buf, 64);
		if (isBigEndian)
			byteReverse(ctx->in, 16);
		MD5Transform(ctx->buf, (uint32 *) ctx->in);
		buf += 64;
		len -= 64;
	}

	/* Handle any remaining bytes of data. */

	memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
void MD5Final(unsigned char *digest, struct MD5Context *ctx)
{
	unsigned count;
	unsigned char *p;

	/* Compute number of bytes mod 64 */
	count = (unsigned) ((ctx->bits[0] >> 3) & 0x3F);

	/* Set the first char of padding to 0x80.  This is safe since there is
	   always at least one byte free */
	p = ctx->in + count;
	*p++ = 0x80;

	/* Bytes of padding needed to make 64 bytes */
	count = 64 - 1 - count;

	/* Pad out to 56 mod 64 */
	if (count < 8) {
		/* Two lots of padding:  Pad the first block to 64 bytes */
		memset(p, 0, count);
		byteReverse(ctx->in, 16);
		MD5Transform(ctx->buf, (uint32 *) ctx->in);

		/* Now fill the next block with 56 bytes */
		memset(ctx->in, 0, 56);
	} else {
		/* Pad block to 56 bytes */
		memset(p, 0, count - 8);
	}
	byteReverse(ctx->in, 14);

	/* Append length in bits and transform */
	((uint32 *) ctx->in)[14] = ctx->bits[0];
	((uint32 *) ctx->in)[15] = ctx->bits[1];

	MD5Transform(ctx->buf, (uint32 *) ctx->in);
	byteReverse((unsigned char *) ctx->buf, 4);
	memcpy(digest, ctx->buf, 16);
    memset(ctx, 0, sizeof(ctx));        /* In case it's sensitive */
}



/* queue functions:
we use these queues for piping pcm samples.

You basically init the queue, enque samples and then read them back
with deque. Quite handy in other places too.
  
I suspect that these queues are thread-safe.
The reason is that the enque deque functions dont access
the same variables. so as long as you read from one thread
and write in another all the time, you are fine. 

There are a couple of interesting, non-standard behaviours in this queue.
First, these never block. second, dequeing from an empty queue will
return '0'samples.

*/


void queueInit(struct Queue *p)
{
	p->head = 0;
	p->tail = 0;
	p->count = 0;
	p->stall = 1;
}

void queueEnque(struct Queue *p, short16 w)
{
	if (p->count == MAX_Q)
		return;

	p->data[p->head++] = w;
	p->count++;
	if (p->head == MAX_Q)
		p->head = 0;
	if (p->count > 1600 && p->stall == 1)
	{
		p->stall = 0;
	}
}

short16 queueDeque(struct Queue *p)
{
	short data;

	if (!p->count || p->stall)
		return 0;

	data = p->data[p->tail++];
	p->count--;
	if (p->tail == MAX_Q)
		p->tail = 0;
	return data;
}


/*  
callFindIdle:
finds an empty slot to start a new call
returns NULL if not found (quite possible), always check for null */
static struct Call *sip_callFindIdle(struct ltpStack *ps)
{
	int     i;
	struct Call *p;

	for (i = 0; i < ps->maxSlots; i++)
	{
		p = ps->call + i;
		if(p->ltpState == CALL_IDLE) 
		{
			p->remoteUserid[0] = 0;
			p->ltpState = 0;
			p->timeStart = p->timeStop = 0;
			p->kindOfCall = 0;
			return p;
		}
	}

	return NULL;
}


/* pj sip routines */

/* Callback called by the library upon receiving incoming call */
static void sip_on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
			     pjsip_rx_data *rdata)
{
	struct pjsua_call_info ci;
	struct Call *pc = NULL;
	char *p;
	int i;

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    pjsua_call_get_info(call_id, &ci);

	//return the call if it too many calls
	pc = sip_callFindIdle(pstack);
	if (!pc){
		pjsua_call_hangup(call_id, 0, NULL, NULL);
		return;
	}

	pc->kindOfCall = CALLTYPE_IN | CALLTYPE_CALL;
	#ifdef _MACOS_
		pstack->now = time(NULL);
	#endif

	pc->timeEllapsed=-1;

	if(pstack->now==0)
	{
		pstack->now = time(NULL);
	}
	pc->timeStart = pstack->now;
	//example of what is in c.remote_info.ptr "+919849026029" <sip:+919849026029@spokn.com>
	//skip ahead of sip and '+' sign if any
	p = ci.remote_info.ptr;
	p = strstr(p, "sip:");
	if (p)
		p+= 4;

	if (*p == '+')
		p++;
	//copy until the end of string or '@' sign
	for (i = 0; i < MAX_USERID-1; i++){
		if (!*p || *p == '@')
			break;
		else
			pc->remoteUserid[i] = *p++;
	}
	pc->remoteUserid[i] = 0; //close the string
	pc->ltpState = CALL_RING_RECEIVED;
	pc->ltpSession = call_id; //bug#26252 - Set the call_id.
	pjsua_call_answer(call_id, PJSIP_SC_RINGING /*180*/, NULL, NULL);
	alert(pc->lineId, ALERT_INCOMING_CALL, "");
	
}

/* Callback called by the library when call's state has changed */
static void sip_on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
	int	i;
	struct pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);
	for (i = 0; i < pstack->maxSlots; i++)
		if (pstack->call[i].ltpSession == (unsigned int)call_id && pstack->call[i].ltpState != CALL_IDLE){
			switch(ci.state){
			case PJSIP_INV_STATE_CALLING:	    /**< After INVITE is sent		    */
				pstack->call[i].ltpState = CALL_RING_SENT;
				break;
			case PJSIP_INV_STATE_INCOMING:	    /**< After INVITE is received.	    */
				pjsua_call_answer(call_id, PJSIP_SC_RINGING /*180*/, NULL, NULL);	
				alert(pstack->call[i].lineId, ALERT_INCOMING_CALL, "");
				pstack->call[i].ltpState = CALL_RING_RECEIVED;
				break;
			//case PJSIP_INV_STATE_EARLY:	    /**< After response with To tag.	    */				
			case PJSIP_INV_STATE_CONNECTING:	    /**< After 2xx is sent/received.	    */
				pstack->call[i].ltpState = CALL_RING_ACCEPTED;
				break;
			case PJSIP_INV_STATE_CONFIRMED:	    /**< After ACK is sent/received.	    */
				pstack->call[i].ltpState = CALL_CONNECTED;
				#ifdef _MACOS_
					pstack->now = time(NULL);
				#endif  	
				pstack->call[i].timeEllapsed=0;
				pstack->call[i].timeStart = pstack->now; /* reset the call timer for the correct duration */
				alert(pstack->call[i].lineId, ALERT_CONNECTED, NULL);
				break;
			case PJSIP_INV_STATE_DISCONNECTED:   /**< Session is terminated.		    */
				//bug#26361 - Check if its a missed call (not already reported in ltpHangup()).
				if ((pstack->call[i].kindOfCall & CALLTYPE_IN) &&
					(pstack->call[i].ltpState != CALL_CONNECTED) &&
					(pstack->call[i].ltpState != CALL_HANGING) &&
					!bMissedCallReported)
					pstack->call[i].kindOfCall |= CALLTYPE_MISSED;
				bMissedCallReported = 0;	
				pstack->call[i].ltpState = CALL_IDLE;
				pstack->call[i].InConference = 0;
				#ifdef _MACOS_
				pstack->now = time(NULL);
				#endif	
				pstack->call[i].timeStop = pstack->now;
				alert(pstack->call[i].lineId, ALERT_DISCONNECTED, "");
				break;
//			case PJSIP_INV_STATE_NULL:
//			default:
			}

			return;
		}
}

static void sip_joinLine(int aline, int bline, int doit)
{
	int i;
	struct pjsua_call_info ca, cb;
	pjsua_call_id call_a=-1, call_b=-1;

	if (aline == bline)
		return;

	for (i = 0; i < pstack->maxSlots; i++)
		if (aline == pstack->call[i].lineId && pstack->call[i].ltpState != CALL_IDLE)
			call_a = pstack->call[i].ltpSession;


	for (i = 0; i < pstack->maxSlots; i++)
		if (bline == pstack->call[i].lineId && pstack->call[i].ltpState != CALL_IDLE)
			call_b = pstack->call[i].ltpSession;

	if (call_a == -1 || call_b == -1)
		return;

	pjsua_call_get_info(call_a, &ca);
	pjsua_call_get_info(call_b, &cb);

	if (doit){
		pjsua_conf_connect(ca.conf_slot, cb.conf_slot);
		pjsua_conf_connect(cb.conf_slot, ca.conf_slot);
	}
	else {
		pjsua_conf_disconnect(ca.conf_slot, cb.conf_slot);
		pjsua_conf_disconnect(cb.conf_slot, ca.conf_slot);
	}
}

static void sip_connectLineToSoundCard(int aline, int doit)
{
	int i;
	struct pjsua_call_info ca;
	pjsua_call_id call_a=-1;

	if (aline == -1)
		return;

	for (i = 0; i < pstack->maxSlots; i++)
		if (aline == pstack->call[i].lineId && pstack->call[i].ltpState != CALL_IDLE)
			call_a = pstack->call[i].ltpSession;

	if (call_a == -1)
		return;

	pjsua_call_get_info(call_a, &ca);

	if (doit){
		pjsua_conf_connect(ca.conf_slot, 0);
		pjsua_conf_connect(0, ca.conf_slot);
	}
	else {
		pjsua_conf_disconnect(ca.conf_slot, 0);
		pjsua_conf_disconnect(0, ca.conf_slot);
	}			
}

static void sip_joinCalls()
{
	int i, j;

	for (i = 0; i < pstack->maxSlots; i++){
		if (pstack->call[i].ltpState != CALL_CONNECTED || !pstack->call[i].InConference)
			continue;

		for (j = 0; j < pstack->maxSlots; j++)
			if (j==i || pstack->call[j].ltpState != CALL_CONNECTED || !pstack->call[j].InConference)
				continue;
			else
				sip_joinLine(pstack->call[i].lineId, pstack->call[j].lineId, 1);
	}
}

/* Callback called by the library when call's media state has changed */
static void sip_on_call_media_state(pjsua_call_id call_id)
{
	int	 i, j;
	struct Call *pc;
	struct pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);

    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
		// When media is active, connect call to sound device.
		pjsua_conf_connect(ci.conf_slot, 0);
		pjsua_conf_connect(0, ci.conf_slot);
    }

	//update the call status
	for (i = 0; i < pstack->maxSlots; i++){
		pc = pstack->call + i;
		if (pc->ltpSession == call_id){
			if (ci.media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD)
				pc->remoteVoice = 0;
			if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
				pc->remoteVoice = 1;
				
				if (pc->InConference)
					sip_joinCalls();
				else 
				{ //handle the non-conference call
					for (j = 0; j < pstack->maxSlots; j++){
						if (pstack->call[j].ltpSession != pc->ltpSession &&
							pstack->call[j].ltpState == CALL_CONNECTED)
						{
							pjsua_call_id other_call = (pjsua_call_id)pstack->call[j].ltpSession;
							pjsua_call_get_info(other_call, &ci);
							pjsua_conf_disconnect(ci.conf_slot, 0);
							pjsua_conf_disconnect(0, ci.conf_slot);
						}
					}
				} // end of handling non-conference call

			}
			//alert(pstack->call[i].lineId, ALERT_CONNECTED, NULL);
		}
	}
}

static void sip_on_reg_state(pjsua_acc_id acc_id)
{
	char	buff[100];
	pjsua_acc_info	info;
	pj_status_t status;
	
	if (!pstack)
		return;
	
	status = pjsua_acc_get_info(acc_id, &info);
	
	if (info.status == 0){
		pstack->loginStatus = LOGIN_STATUS_OFFLINE;
		alert(-1, ALERT_OFFLINE, info.status_text.ptr);
	}
	else if (info.status >= 100 && 200 > info.status)
		pstack->loginStatus = LOGIN_STATUS_TRYING_LOGIN;
	else if (info.status == 200){
		if (info.expires > 0){
			pstack->loginStatus = LOGIN_STATUS_ONLINE;
			alert(-1, ALERT_ONLINE, info.status_text.ptr);
		}
		else {
			pstack->loginStatus = LOGIN_STATUS_OFFLINE;
			alert(-1, ALERT_OFFLINE, info.status_text.ptr);
		}
	}
	else if (info.status == 401 || info.status == 407)
		pstack->loginStatus = LOGIN_STATUS_TRYING_LOGIN;
	//bug#26354, Handle additional range between PJSIP_EFAILEDCREDENTIAL & PJSIP_EAUTHSTALECOUNT for authentication failure.
	else if ((info.status >= 400 && info.status < 500 && (info.status != 401 ||info.status != 407)) || 
			 (info.status >= PJSIP_EFAILEDCREDENTIAL && info.status <= PJSIP_EAUTHSTALECOUNT))
	{
		//Sign-in attempt failed. Maximum number of stale retries exceeded. This happens when server 
		//keeps rejecting our authorization request with stale=true. 
		if (info.status == PJSIP_EAUTHSTALECOUNT || info.status == PJSIP_SC_REQUEST_TIMEOUT)
			pstack->loginStatus = LOGIN_STATUS_TIMEDOUT;
		else
		{
			//ltpLogin(pstack, CMD_LOGOUT);
			pstack->loginStatus = LOGIN_STATUS_FAILED;
		}
		alert(-1, ALERT_OFFLINE, info.status_text.ptr);
	}
	else if (info.status >= 500 && info.status <= 606)
	{
		pstack->loginStatus = LOGIN_STATUS_NO_ACCESS;
#ifdef _MAC_OSX_CLIENT_
		alert(-1, ALERT_LOGIN_STATUS_CHANGED, info.status_text.ptr);
#endif
	}
	sprintf(buff, "%d", info.status);
	//	SetDlgItemTextA(wndMain, IDC_STATUS, buff);
}
void       callbackpjsip(int level, const char *data, int len)
{
	if(data)
	{	
		if(strstr(data,"200"))
		{	
			printf("\n%s",data);
		}	
	}	
}
int sip_spokn_pj_Create(struct ltpStack *ps)
{
	pj_status_t status;
		
	status = pjsua_create();
	
	if (status != PJ_SUCCESS){
		
		return 0;
	}
	
	return 1;	
	
}
int sip_destroy_transation(struct ltpStack *ps)
{
	/*if(ps->tranportID>=0 && ps->sipOnB)
	{
		pjsua_transport_close(ps->tranportID,0);
		ps->tranportID = -1;
		
	}*/
	return 1;
}
int sip_set_randomVariable(struct ltpStack *ps,int randVariable)
{
	ps->randVariable = randVariable;
	return 0;
}
int sip_set_udp_transport(struct ltpStack *ps,char *userId,char *errorstring,int *p_id)
{
	
    /* Add UDP transport. */
	pj_status_t status = 1;
	int idUser=0;
	pjsua_transport_config transcfg;
	int dummy_start_port = 5060;
	unsigned range;
	int diffport;
	pjsua_transport_config rtp_cfg;
	if(p_id)
	{
		if(*p_id>=0)
		{
			return 1 ;
		}
	}
	
	pjsua_transport_config_default(&transcfg);
	//transcfg.public_addr = pj_str("192.168.175.102");
	//transcfg.bound_addr = pj_str(" 127.0.0.1");
	if(userId)
	{
		idUser = atoi(userId);
		if(idUser)
		{
			int tmpNo;
			int tmpNo2;
			tmpNo2 = idUser/1000;
			tmpNo = idUser - tmpNo2*1000;
			idUser = tmpNo+tmpNo2+ps->randVariable;//this variable is constant per user it is set once build install
			if(idUser<5061)
				idUser = idUser+5070;
			idUser = idUser&0xFFFE;
			transcfg.port = idUser;
			status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transcfg, p_id);
			//printf("\n\n\n\nport %d\n\n\n",transcfg.port);
		}
	
	 }
	else
	{
		#ifndef _MACOS_
			transcfg.port = 8060;
			status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transcfg, p_id);
		#endif
	}
	if(status!=PJ_SUCCESS)
	{	
		range = (10000-dummy_start_port);
		transcfg.port = dummy_start_port + 
		((pj_rand() % range) & 0xFFFE);
		if(transcfg.port==5060)//change to some other port
		{
			transcfg.port = transcfg.port + 102;
			
		}
			
		status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transcfg, p_id);
		
		if (status != PJ_SUCCESS){
			range = (65535-dummy_start_port);
			transcfg.port = dummy_start_port + 
			((pj_rand() % range) & 0xFFFE);
			if(transcfg.port==5060)//change to some other port
			{
				transcfg.port = transcfg.port + 102;
				
			}
			status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transcfg, p_id);
			if (status != PJ_SUCCESS)
			{	
				
				
				transcfg.port = 8060;	
				status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transcfg, p_id);
				if (status != PJ_SUCCESS)
				{
					transcfg.port = 5060;
					status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transcfg, p_id);
					if (status != PJ_SUCCESS)
					{	 
						sprintf(errorstring, "Error in pjsua_transport_create(). [status:%d]",status);
						return 0;
					}	 
				}
			}	
		}
	}	
	ps->lport = transcfg.port;
//	transcfg.public_addr = pj_str("192.168.175.102");
	//return 1;
	pjsua_transport_config_default(&rtp_cfg);
	{
		enum { START_MEDIA_PORT=4000 };
		unsigned range;
		
		if(idUser==0)
		{	
			range = (10000-START_MEDIA_PORT-PJSUA_MAX_CALLS*2);
			rtp_cfg.port = START_MEDIA_PORT + 
			((pj_rand() % range) & 0xFFFE);
		}
		else {
			rtp_cfg.port = START_MEDIA_PORT + ((idUser>START_MEDIA_PORT)?(idUser+20-START_MEDIA_PORT):idUser);
		}

		diffport = transcfg.port-rtp_cfg.port;
		if(diffport<0)
		{
			diffport = diffport*-1;
		}
		if(rtp_cfg.port==5060 || diffport<10)//change to some other port
		{
			rtp_cfg.port = rtp_cfg.port + 102;
			
		}
		if(rtp_cfg.port&1)//if odd address
		{
			rtp_cfg.port = rtp_cfg.port + 1;
		}
	}
	
	status = pjsua_media_transports_create(&rtp_cfg);
	if(status!=PJ_SUCCESS)
	{
		
			enum { START_PORT=4000 };
			unsigned range;
			
			range = (65535-START_PORT-PJSUA_MAX_CALLS*2);
			rtp_cfg.port = START_PORT + 
			((pj_rand() % range) & 0xFFFE);
			
			diffport = transcfg.port-rtp_cfg.port;
			if(diffport<0)
			{
				diffport = diffport*-1;
			}
			if(rtp_cfg.port==5060 || diffport<10)//change to some other port
				
			{
				rtp_cfg.port = rtp_cfg.port + 102;
				
			}
		status = pjsua_media_transports_create(&rtp_cfg);
		if(status!=PJ_SUCCESS)
		{
			rtp_cfg.port = 4000;			
			status = pjsua_media_transports_create(&rtp_cfg);
			
		}
	}	
		
		
	return 1;
	

}
char *getLogFile(struct ltpStack *ps)
{
	#ifdef _PJSIP_LOG_
	return ps->logfile;
	#endif
	return NULL;
	
}
void setLog(struct ltpStack *ps, int onB,char *pathP)
{
#ifdef _PJSIP_LOG_
	ps->writeLogB = onB;
	if(ps->writeLogB)
	{
		if(pathP)
		{	
			strcpy(ps->logfile,pathP);
		}
		else {
			ps->logfile[0] = 0;
		}

	}
#endif

}
void  tsx_callback(void *token, pjsip_event *event)
{
	
    //pj_status_t status;
	SipOptionDataType *sipOptionP;
	struct ltpStack *lpsP;
	
    //pjsip_regc *regc = (pjsip_regc*) token;
    pjsip_transaction *tsx = event->body.tsx_state.tsx;
	
	if (event->type == PJSIP_EVENT_TSX_STATE)
	{
		sipOptionP = (SipOptionDataType *)token;
		printf("\n code=%d",tsx->status_code);
		
		if(sipOptionP==0)
		{
			return ;
		}
		lpsP = sipOptionP->dataP;
		if(lpsP->gotOpenPortB==1)
		{
			return ;
		}
		sipOptionP->errorCode = 0;
		if(tsx->status_code ==408)//mean error
		{
			sipOptionP->errorCode = 1;
			return;
		}
		lpsP->gotOpenPortB = 1;
		strcpy(lpsP->registerUrl,sipOptionP->connectionUrl);
		printf("url %s",lpsP->registerUrl);
		/* Ignore provisional response, if any */
		if (tsx->status_code < 200)
			return;
		
		if (event->body.tsx_state.type == PJSIP_EVENT_RX_MSG &&
			(tsx->status_code == 501)) 
		{
		//	pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
			
			PJ_LOG(4,(THIS_FILE, "Method Not Supported Here"));
			
		}
		alert(0, ATTEMPT_LOGIN_ON_OPEN_PORT, 0);
		
	}
	
	
}


/*
 * Send arbitrary request to remote host
 */
extern unsigned int sleep(unsigned int);
int send_request(char *cstr_method, char *ldst_uriP,void *uDataP)
{
    pj_str_t str_method;
    pjsip_method method;
    pjsip_tx_data *tdata;
    pjsip_endpoint *endpt;
    pj_status_t status;
    pjsua_acc_id acc_id;
    pj_str_t dst_uri;
    
    endpt = pjsua_get_pjsip_endpt();
	
    str_method = pj_str(cstr_method);
	
    dst_uri = pj_str(ldst_uriP);
	
    pjsip_method_init_np(&method, &str_method);
	
    acc_id = pjsua_acc_get_default();
	
    status = pjsua_acc_create_request(acc_id, &method, &dst_uri, &tdata);
	
    status = pjsip_endpt_send_request(endpt, tdata, 200, uDataP, &tsx_callback);
    
    if (status != PJ_SUCCESS)
    {
		printf("error in port assign");
		pjsua_perror(THIS_FILE, "Unable to send request", status);
		return 1;
    }
	return 0;
}


int sip_spokn_pj_config(struct ltpStack *ps, char *userAgentP,char *errorstring)
{
	
	
	//char *hostP;
	static SipOptionDataType sipOptionDataPort1;
	static SipOptionDataType sipOptionDataPort2;
	pjsua_config cfg;
	pjsua_logging_config log_cfg;
	pj_status_t status;
	pjsua_media_config cfgmedia;
#ifdef _MACOS_
	#ifndef  _MAC_OSX_CLIENT_
		pj_thread_desc desc;
		pj_thread_t *  thread=0;
		memset(&desc,0,sizeof(pj_thread_desc));
		pj_thread_register(NULL,desc,&thread);	
	
	#endif	
#endif
	pjsua_config_default(&cfg);
	cfg.cb.on_incoming_call = &sip_on_incoming_call;
	cfg.cb.on_call_media_state = &sip_on_call_media_state;
	cfg.cb.on_call_state = &sip_on_call_state;
	cfg.cb.on_reg_state = &sip_on_reg_state;
	
	ps->pjpool = pjsua_pool_create("pjsua", 2000, 2000);
	//pj_str(
	//cfg.stun_ignore_failure	= 0;
	pjsua_logging_config_default(&log_cfg);
	log_cfg.console_level = 0;
//	log_cfg.cb = callbackpjsip;
	#ifdef _PJSIP_LOG_
		
		if(ps->writeLogB)
		{
			log_cfg.console_level = 4;
			log_cfg.log_filename = pj_strdup3(ps->pjpool, 
											  ps->logfile);
			
		}
	#endif
	
	
	pjsua_media_config_default(&cfgmedia);
	cfgmedia.clock_rate = 8000;
	cfgmedia.snd_clock_rate = 8000;
	//cfgmedia.ec_options = 1;
	cfgmedia.snd_auto_close_time = 2;
	//cfgmedia.ec_tail_len = 0;
	//cfgmedia.enable_ice=1;
	//cfgmedia.enable_ice = 1;
	/*cfgmedia.ice_max_host_cands = 1;
	 pj_strdup2_with_null(ps->pjpool, 
	 &(cfgmedia.turn_server), 
	 "turn.spokn.com");
	 cfgmedia.enable_turn = 1; 
	 //cfgmedia.ec_tail_len = 0;
	 //cfgmedia.no_vad = 1;
	 cfgmedia.turn_auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
	 
	 
	 cfgmedia.turn_auth_cred.data.static_cred.realm = pj_str(SIP_DOMAIN);
	 cfgmedia.turn_auth_cred.data.static_cred.username = pj_str(pstack->ltpUserid);
	 cfgmedia.turn_auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
	 cfgmedia.turn_auth_cred.data.static_cred.data = pj_str(pstack->ltpPassword);;
	 */
	//ps->stunB = 0;
//	pjsip_cfg()->regc.add_xuid_param = 1;

	if(ps->stunB)
	{	
		#ifdef SRV_RECORD
		const pj_str_t *hostP;
		hostP = pj_gethostname();
		if(hostP)
		{	
			if(hostP->slen)//if got host name
			{	
				printf("\n host %s",hostP->ptr);
				pj_strdup2_with_null(ps->pjpool, 
								 &(cfg.nameserver[cfg.nameserver_count++]), 
								 hostP->ptr);
				pj_strdup2_with_null(ps->pjpool, 
								 &(cfg.stun_srv[cfg.stun_srv_cnt++]), 
								 "spokn.com");
			}
			else
			{
				pj_strdup2_with_null(ps->pjpool, 
									 &(cfg.stun_srv[cfg.stun_srv_cnt++]), 
									 "stun.spokn.com");
			
			}
		}
		else {
				pj_strdup2_with_null(ps->pjpool, 
								 &(cfg.stun_srv[cfg.stun_srv_cnt++]), 
								 "stun.spokn.com");
						
		}

		#else
				pj_strdup2_with_null(ps->pjpool, 
							 &(cfg.stun_srv[cfg.stun_srv_cnt++]), 
							 "stun.spokn.com");
				/*pj_strdup2_with_null(ps->pjpool, 
							 &(cfg.outbound_proxy[cfg.outbound_proxy_cnt++]), 
							 "sip:192.168.173.122:5060");*/
		
		
		/*pj_strdup2_with_null(ps->pjpool, 
							 &(cfg.stun_srv[cfg.stun_srv_cnt++]), 
							 "stun.ideasip.com");
		
		pj_strdup2_with_null(ps->pjpool, 
							 &(cfg.stun_srv[cfg.stun_srv_cnt++]), 
							 "stun.sipgate.net:10000");*/
		#endif
	}
	//sprintf(str,"spokn iphone version %s",CLIENT_VERSION);
	if(userAgentP)
	{	
		pj_strdup2_with_null(ps->pjpool, 
						 &(cfg.user_agent), 
						 userAgentP);
	}
	status = pjsua_init(&cfg, &log_cfg, &cfgmedia);
	if (status != PJ_SUCCESS){
		strcpy(errorstring, "Error in pjsua_init()");
		return 1;
	}
	
	
	if(sip_set_udp_transport(ps,ps->ltpUserid,errorstring,&ps->tranportID)==0)
	{
		strcpy(errorstring, "Error in sip_set_udp_transport");
		return 2;
	}	
#ifdef _SPEEX_CODEC_
	{
		//speex code
		/* Set codec priority
		 
		 Use only "speex/8000" or "speex/16000". Set zero priority for others.
		 
		 */
		//int x;
		pj_str_t tmp1;
		//pjsua_codec_set_priority(pj_cstr(&tmp1, "speex"), PJMEDIA_CODEC_PRIO_HIGHEST);
		
		pjsua_codec_set_priority(pj_cstr(&tmp1, "speex/8000"), PJMEDIA_CODEC_PRIO_HIGHEST );
		pjsua_codec_set_priority(pj_cstr(&tmp1, "speex/16000"), PJMEDIA_CODEC_PRIO_NEXT_HIGHER);
		//pjsua_codec_set_priority(pj_cstr(&tmp1, "ilbc"), 250);
		pjsua_codec_set_priority(pj_cstr(&tmp1, "speex/32000"), 0);
		
		pjsua_codec_set_priority(pj_cstr(&tmp1, "pcmu"), 0);
		
		pjsua_codec_set_priority(pj_cstr(&tmp1, "pcma"), 0);
		
		//pjsua_codec_set_priority(pj_cstr(&tmp1, "gsm"), 0);
	}
#endif	
	
    /* Initialization is done, now start pjsua */
    status = pjsua_start();
	if (status != PJ_SUCCESS){ 
		strcpy(errorstring, "Error starting pjsua");
		return 3;
	}
	
    pjsua_detect_nat_type();
	pjsua_acc_add_local(ps->tranportID,PJ_TRUE,0);
	ps->gotOpenPortB = 0;
	strcpy(sipOptionDataPort1.connectionUrl,"sip:spokn.com:8060");
	sipOptionDataPort1.dataP = ps;
	sipOptionDataPort2.dataP = ps;
	sipOptionDataPort1.errorCode = 0;
	sipOptionDataPort2.errorCode = 0;
	strcpy(sipOptionDataPort2.connectionUrl,"sip:spokn.com:5060");
    send_request("OPTIONS",sipOptionDataPort1.connectionUrl,&sipOptionDataPort1);    
    send_request("OPTIONS",sipOptionDataPort2.connectionUrl,&sipOptionDataPort2);
	while(ps->gotOpenPortB==0) 
	{
		if(sipOptionDataPort1.errorCode >0 && sipOptionDataPort2.errorCode>0)//mean both port are blocked error
		{
			strcpy(errorstring, "port blocked");

			return 4;
		}
		sleep(1);
	}
	return 0;
	
}
int sip_spokn_pj_init(struct ltpStack *ps,char *luserAgentP, char *errorstring)
{
	pj_status_t status;
	    /* Create pjsua first! */
  
	/* Init pjsua */
	pjsua_destroy();
	status = pjsua_create();
	
	if (status != PJ_SUCCESS){
		
		return 0;
	}
	
	return sip_spokn_pj_config(ps,luserAgentP,errorstring);
}

int setSoundDev(int input, int output)
{
	int in,out;
	pjsua_get_snd_dev(&in, &out);
	//pjsua_set_null_snd_dev();
	//pjmedia_snd_deinit();
	//pjmedia_snd_init(pjsua_get_pool_factory());
		
	pj_status_t status = pjsua_set_snd_dev(input, output);
	if(status==PJ_SUCCESS)
		printf("Success");
	else printf("failed");
	return (status == PJ_SUCCESS) ? 1 : 0;
}

void reInitAudio()
{
	// Stop sound device and disconnect it from the conference.
	pjsua_set_null_snd_dev();
	
	// Reinit sound device.
	pjmedia_snd_deinit();
	pjmedia_snd_init(pjsua_get_pool_factory());
}

int sip_mac_init(struct ltpStack *ps, char *errorstring)
{
	pjsua_config cfg;
	pjsua_logging_config log_cfg;
	pj_status_t status;
	pjsua_transport_config transcfg;
	pjsua_media_config cfgmedia;
	pj_str_t tmp;
	pjsua_transport_config rtp_cfg;
	if(ps->pjpool)
	{ 
		pj_pool_release(ps->pjpool);
		ps->pjpool = 0;
		
	}
	/* Create pjsua first! */
    status = pjsua_create();
	
	if (status != PJ_SUCCESS)
	{
		sprintf(errorstring, "Error in pjsua_create(). [status:%d]",status);
		return status;
	}
	
	/* Init pjsua */
	//init callback config
	pjsua_config_default(&cfg);
	//cfg.thread_cnt = 6; //MAX_SLOTS
	cfg.cb.on_incoming_call = &sip_on_incoming_call;
	cfg.cb.on_call_media_state = &sip_on_call_media_state;
	cfg.cb.on_call_state = &sip_on_call_state;
	cfg.cb.on_reg_state = &sip_on_reg_state;
	ps->pjpool = pjsua_pool_create("pjsua", 1000, 1000);
	
	pj_strdup2_with_null(ps->pjpool, 
                         &(cfg.stun_srv[cfg.stun_srv_cnt++]), 
                         "stun.spokn.com");
	
	pj_strdup2_with_null(ps->pjpool, 
                         &(cfg.stun_srv[cfg.stun_srv_cnt++]), 
                         "stun.spokn.com");
	
	pj_strdup2_with_null(ps->pjpool, 
                         &(cfg.stun_srv[cfg.stun_srv_cnt++]), 
                         "stun.ideasip.com");
	
	pj_strdup2_with_null(ps->pjpool, 
                         &(cfg.stun_srv[cfg.stun_srv_cnt++]), 
                         "stun.sipgate.net:10000");
	//init logging config
	pjsua_logging_config_default(&log_cfg);
	log_cfg.console_level = 6;
	
	//init media config
	pjsua_media_config_default(&cfgmedia);
	cfgmedia.clock_rate = 8000;
	cfgmedia.snd_clock_rate = 8000;
	cfgmedia.snd_auto_close_time = 2;
	cfgmedia.enable_ice=1;
#ifdef _SPEEX_CODEC_	
	//use speex as the AEC (Acoustic Echo Canceller)
	cfgmedia.ec_options = PJMEDIA_ECHO_SPEEX;
#endif
	
	//thus init pjsua
	status = pjsua_init(&cfg, &log_cfg, &cfgmedia);
	if (status != PJ_SUCCESS)
	{
		sprintf(errorstring, "Error in pjsua_init(). [status:%d]",status);
		return status;
	}
	
#ifdef _SPEEX_CODEC_
	//speex code
	/* Set codec priority 
	 Use only "speex/8000" or "speex/16000". Set zero priority for others.
	 */
	pjsua_codec_set_priority(pj_cstr(&tmp, "speex/8000"), PJMEDIA_CODEC_PRIO_HIGHEST);
	
	 pjsua_codec_set_priority(pj_cstr(&tmp, "speex/16000"), PJMEDIA_CODEC_PRIO_NEXT_HIGHER);
	 
	 pjsua_codec_set_priority(pj_cstr(&tmp, "speex/32000"), 0);
	 
	 pjsua_codec_set_priority(pj_cstr(&tmp, "pcmu"), 0);
	 
	 pjsua_codec_set_priority(pj_cstr(&tmp, "pcma"), 0);
	 
	 pjsua_codec_set_priority(pj_cstr(&tmp, "ilbc"), 0);
	
	//pjsua_codec_set_priority(pj_cstr(&tmp, "gsm"), 0);
#endif	
	
	
    /* Add UDP transport. */
	//Try port 8060 first. If it fails, then try with 5060 as a fallback.
	/*pjsua_transport_config_default(&transcfg);
	 transcfg.port = 8060;
	 status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transcfg, NULL);
	 if (status != PJ_SUCCESS){
	 transcfg.port = 5060;
	 status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transcfg, NULL);
	 if (status != PJ_SUCCESS)
	 {
	 sprintf(errorstring, "Error in pjsua_transport_create(). [status:%d]",status);
	 return status;
	 }
	 }*/
	pjsua_transport_config_default(&transcfg);
	status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transcfg, NULL);
	if (status != PJ_SUCCESS){
		strcpy(errorstring, "Error creating transport");
		return 0;
    }
	
	pjsua_transport_config_default(&rtp_cfg);
	{
		enum { START_PORT=4000 };
		unsigned range;
		
		range = (65535-START_PORT-PJSUA_MAX_CALLS*2);
		rtp_cfg.port = START_PORT + 
		((pj_rand() % range) & 0xFFFE);
		if(rtp_cfg.port==5060)//change to some other port
		{
			rtp_cfg.port = rtp_cfg.port + 102;
		}
	}
	
	status = pjsua_media_transports_create(&rtp_cfg);
	
    /* Initialization is done, now start pjsua */
    status = pjsua_start();
	if (status != PJ_SUCCESS){ 
		strcpy(errorstring, "Error starting pjsua");
		return status;
	}
	
    pjsua_detect_nat_type();
	return 1;
	
}
void sip_pj_DeInit(struct ltpStack *ps)
{
	if(ps==0)
		return;
	if(ps->sipOnB==0)
	{
		return;
	}
	if(ps->pjpool)
	{	
		pj_pool_release(ps->pjpool);
		ps->pjpool = 0;
		
	}
	pjsua_destroy();
	
	
}

/* 
ltpInit:
this is the big one. It calls malloc (from standard c library). If your system doesn't support
dynamic memory, no worry, just pass a pointer to a preallocated block of the size of ltpStack. It is
a one time allocation. The call structures inside are also allocated just once in the entire stack's
lifecycle. The memory is nevery dynamically allocated during the runtime.

We could have declared ltpStack and Calls as global data, however a number of systems do not allow
global writeable data (like the Symbian) hence, we request for memory. Most systems can simply return 
a pointer to preallocated memory */

struct ltpStack  *sip_ltpInit(int maxslots, int maxbitrate, int framesPerPacket)
{
	int	i;
	struct Call *p;
	struct ltpStack *ps;
	
	ps = (struct ltpStack *)malloc(sizeof(struct ltpStack));
	if (!ps)
		return NULL;

	zeroMemory(ps, sizeof(struct ltpStack));	
	
	ps->defaultCodec = (short16) maxbitrate;
	ps->loginCommand = CMD_LOGIN;
	ps->loginStatus = LOGIN_STATUS_OFFLINE;
	ps->ringTimeout  = 30;
	ps->myPriority = NORMAL_PRIORITY;
	strncpy(ps->userAgent,USERAGENT, MAX_USERID);
	ps->ltpPresence = NOTIFY_ONLINE;
	ps->updateTimingAdvance = 0;
	ps->stunB = 1;
	ps->tranportID = -1;
	strcpy(ps->registerUrl,"sip:spokn.com"); 
	ps->maxSlots = maxslots;
	ps->call = (struct Call *) malloc(sizeof(struct Call) * maxslots);
	if (!ps->call)
		return NULL;
	zeroMemory(ps->call, sizeof(struct Call) * maxslots);	

	ps->chat = (struct Message *) malloc(sizeof(struct Call) * MAX_CHATS);
	if (!ps->chat)
		return NULL;
	zeroMemory(ps->chat, sizeof(struct Message) * MAX_CHATS);
	ps->maxMessageRetries = 3;

	for (i=0; i<maxslots; i++)
	{
		p = ps->call + i;
		/* assign lineId */
		p->lineId = i;
		p->ltpState = CALL_IDLE;
	}


	/* determine if we are on a big-endian architecture */
	i = 1;
	if (*((char *)&i))
		ps->bigEndian = 0;
	else
		ps->bigEndian = 1;
	return ps;
}


/**
	pj sip wrappers
*/



void sip_ltpTick(struct ltpStack *ps, unsigned int timeNow)
{
	pstack->now = timeNow;
	//pjsua_handle_events(1);
}


/* these functions have to be wrapped around the pjsip stack */

void sip_ltpMessageDTMF(struct ltpStack *ps, int lineid, char *msg)
{
	pj_str_t digits;

	digits = pj_str(msg);
	if (ps->call[lineid].ltpState != CALL_IDLE)
		pjsua_call_dial_dtmf((pjsua_call_id)ps->call[lineid].ltpSession, &digits);
}


void sip_ltpLogin(struct ltpStack *ps, int command)
{
	pjsua_acc_id acc_id;
	//char	url[128];
	//char	url1[128];
	char errorStr[50]={0};
	//char	url[128];
    /* Register to SIP server by creating SIP account. */

	if (command == CMD_LOGIN){
		//check if an account already exists
		if (strlen(ps->ltpUserid) && strlen(ps->ltpPassword) && pjsua_acc_get_count() > 0){
			acc_id = pjsua_acc_get_default();
			//if (acc_id != PJSUA_INVALID_ID){

			//	//if the the account details are the same, then just re-register
			//	if (!strcmp(pstack->ltpUserid, acccfg.cred_info[0].username.ptr) &&
			//		!strcmp(pstack->ltpPassword, acccfg.cred_info[0].data.ptr))
			//	{
			//		pjsua_acc_set_registration(acc_id, PJ_TRUE);
			//		return;
			//	}
			//	
			//}


			//account details don't match, then delete this account and create a new default account
			pjsua_acc_del(acc_id);
		}
		
		if(sip_set_udp_transport(ps,ps->ltpUserid,errorStr,&ps->tranportID)==0)
		{
			alert(0,ATTEMPT_LOGIN_ERROR,strdup(errorStr));
			return;
		}
				
		pjsua_acc_config_default(&acccfg);

		if(ps->idBlock==0)
		{	
				ps->idBlock = pj_pool_alloc(/*app_config.*/ps->pjpool, PJSIP_MAX_URL_SIZE);
		}	
		acccfg.id.ptr = (char*) ps->idBlock;

		acccfg.id.slen = sprintf(acccfg.id.ptr, "sip:%s@%s", ps->ltpUserid, SIP_DOMAIN);
		//acccfg.id = pj_str(url);
		acccfg.reg_uri = pj_str(ps->registerUrl);
		//sprintf(url1, "testrelmstring%s%d", ps->ltpUserid, ps->lport);
		//acccfg.force_contact =pj_str(url1);
		acccfg.cred_count = 1;
		acccfg.cred_info[0].realm = pj_str(SIP_DOMAIN);
		acccfg.cred_info[0].scheme = pj_str("digest");
		acccfg.cred_info[0].username = pj_str(ps->ltpUserid);
		acccfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
		acccfg.cred_info[0].data = pj_str(ps->ltpPassword);
		acccfg.reg_timeout = ps->timeOut;
		//pjsip_cfg()->regc.check_contact = PJ_FALSE;
		//pjsip_cfg()->regc.add_xuid_param = PJ_TRUE;
		

		pjsua_acc_add(&acccfg, PJ_TRUE, &acc_id);
		/*pjsip_method method;
		pj_str_t target = pj_str("spokn.com:8060");
		pjsip_tx_data *p_tdataP;
		target.ptr = malloc(100);
		method.id = PJSIP_OPTIONS_METHOD;
		method.name = pj_str("OPTIONS");
		int er ;
		er = pjsua_acc_create_request(acc_id,&method,&target,&p_tdataP);
	*/	
	}

	if (command == CMD_LOGOUT) {
		
		if(pjsua_acc_get_count() > 0)
		{	
			acc_id = pjsua_acc_get_default();

			if (acc_id != PJSUA_INVALID_ID)
			{	
				pjsua_acc_set_registration(acc_id, PJ_FALSE);
				//pjsua_acc_del(acc_id);
			}
			
		}	
	}
}
/* at the moment this simply unregisters from the sip server, maybe something else is required here */
void sip_ltpLoginCancel(struct ltpStack *ps)
{
	pjsua_acc_id acc_id = pjsua_acc_get_default();

	if (acc_id != PJSUA_INVALID_ID)
		pjsua_acc_del(acc_id);
}

/* we reuse the ltpSession member of the call slot to hold the callid of pjsip in the ltpstack 
*/

void sip_ltpHangup(struct ltpStack *ps, int lineid)
{
	int	i;
	pjsua_call_id call_id;

	for (i = 0; i < ps->maxSlots; i++)
	{
		if (ps->call[i].lineId == lineid && ps->call[i].ltpState != CALL_IDLE){
			call_id = (pjsua_call_id) ps->call[i].ltpSession;
//bug#26267 - Check if its a missed call
			bMissedCallReported = 0;
			if ((ps->call[i].ltpState != CALL_CONNECTED) && (ps->call[i].kindOfCall& CALLTYPE_IN))
			{
				bMissedCallReported = 1;
				ps->call[i].kindOfCall |= CALLTYPE_MISSED;
			}
			//bug#26268, the incoming call has been rejected
			if (ps->call[i].ltpState == CALL_RING_RECEIVED)
			{
				pj_str_t pjStr;  
				pjStr.ptr = "Busy Here"; 
				pjStr.slen =  strlen(pjStr.ptr);
				ps->call[i].ltpState = CALL_HANGING;
				pjsua_call_answer(call_id, PJSIP_SC_BUSY_HERE, &pjStr, NULL);
			}
			//else this is a regular hangup
			else
			{
				ps->call[i].ltpState = CALL_HANGING;
				pjsua_call_hangup(call_id, 0, NULL, NULL);
			}
			return;
		}
	}
}

int sip_ltpRing(struct ltpStack *ps, char *remoteid, int command)
{
	int i;
	int err;
	
	char	struri[128];
	pj_str_t uri;
	struct Call *pc;
	pjsua_call_id call_id;
	err = 0;
	pc = sip_callFindIdle(pstack);
	if (!pc){
		alert(-1, ALERT_ERROR, "Too many calls");
		return 0;
	}

	if (strncmp(remoteid, "sip:", 4))
	{	
		if(strstr(remoteid,"+"))
		{
			sprintf(struri, "sip:%s@spokn.com", remoteid);
		}
		else
		{	
			if(strlen(remoteid)<6)//this changes is temp
			{
				sprintf(struri, "sip:%s@spokn.com", remoteid);
			}
			else
			{	
				sprintf(struri, "sip:+%s@spokn.com", remoteid);
			}	
		}	
	}
	else
		strcpy(struri, remoteid);

	uri = pj_str(struri);
	if (pjsua_call_make_call(pjsua_acc_get_default(), &uri, 0, NULL, NULL, &call_id) == PJ_SUCCESS){
		strcpy(pc->remoteUserid, remoteid);
		pc->ltpSession = (unsigned int) call_id;
		pc->ltpState = CALL_RING_SENT;
	}
	else
	{
		err = 1;
		strcpy(pc->remoteUserid, remoteid);
		pc->ltpSession = (unsigned int) call_id;
		pc->ltpState = CALL_RING_SENT;
	}
	pc->kindOfCall = CALLTYPE_OUT | CALLTYPE_CALL;
	#ifdef _MACOS_
		pstack->now = time(NULL);
	#endif  
	pc->timeEllapsed=-1;
	pc->timeStart = pstack->now;
	ps->activeLine =pc->lineId;

	//put all the other calls on hold
	for (i = 0; i < ps->maxSlots; i++)
		if (ps->call[i].lineId != pc->lineId && ps->call[i].ltpState != CALL_IDLE && !ps->call[i].InConference)
			pjsua_call_set_hold((pjsua_call_id)ps->call[i].ltpSession, NULL);

	if(err)
	{
		
		alert(pc->lineId, ALERT_CALL_NOT_START, 0);
		return -1;
	}
	
	return pc->lineId;
}

void sip_ltpAnswer(struct ltpStack *ps, int lineid)
{
	int	i;
	pjsua_call_id call_id;

	for (i = 0; i < ps->maxSlots; i++)
		if (ps->call[i].lineId == lineid && ps->call[i].ltpState != CALL_IDLE){
			call_id = (pjsua_call_id) ps->call[i].ltpSession;
			pjsua_call_answer(call_id, 200, NULL, NULL);
			ps->activeLine = lineid;
		}
}

void sip_switchReinvite(struct ltpStack *ps, int lineid)
{
	int	i, inConf=0;
	pjsua_call_id call_id;

	for (i = 0; i < ps->maxSlots; i++)
		if (lineid == ps->call[i].lineId && ps->call[i].ltpState == CALL_CONNECTED && ps->call[i].InConference)
			inConf = 1;

	for (i = 0; i < ps->maxSlots; i++)
		if (ps->call[i].ltpState != CALL_IDLE){
			call_id = (pjsua_call_id) ps->call[i].ltpSession;
			if (lineid == ps->call[i].lineId){
				pjsua_call_reinvite((pjsua_call_id)ps->call[i].ltpSession, PJ_TRUE, NULL);
				ps->activeLine = lineid;
			}
			else if (!ps->call[i].InConference && ps->call[i].ltpState == CALL_CONNECTED) //hold all the calls not in conference
				pjsua_call_set_hold((pjsua_call_id)ps->call[i].ltpSession, NULL);
		}
}

void sip_startConference ()
{
	int	i;

	for (i = 0; i < pstack->maxSlots; i++)
		if (pstack->call[i].ltpState != CALL_IDLE){
			pjsua_call_reinvite((pjsua_call_id)pstack->call[i].ltpSession, PJ_TRUE, NULL);
			pstack->call[i].InConference = 1;
		}
}
void sip_setMute(int enableB)
{
	//int	i;
	//pjsua_conf_port_id  port;
	if (enableB)
	{
		pjsua_conf_adjust_rx_level(0 , 0.0f);
	}
	else
	{
		pjsua_conf_adjust_rx_level(0 , 1.0f);
	}
}


void sip_setHold(struct ltpStack *ps,int enableB)
{
	int	i;
	if (enableB)
	{
		for (i = 0; i < pstack->maxSlots; i++)
			if (pstack->call[i].ltpState != CALL_IDLE && pstack->call[i].ltpSession!=PJSUA_INVALID_ID){
				pjsua_call_set_hold((pjsua_call_id)pstack->call[i].ltpSession, NULL);
				
			}
	}
	else
	{
		for (i = 0; i < pstack->maxSlots; i++)
			if (pstack->call[i].ltpState != CALL_IDLE && pstack->call[i].ltpSession!=PJSUA_INVALID_ID){
				pjsua_call_reinvite((pjsua_call_id)pstack->call[i].ltpSession, PJ_TRUE, NULL);
				
			}
	}
}

void setMute(struct ltpStack *ps,int enableB)
{
	if(ps->sipOnB)
	{
		sip_setMute(enableB);
	}
	else
	{
		//sip_setMute(ps,enableB);
	}
}

void setHold(struct ltpStack *ps,int enableB)
{

	if(ps->sipOnB)
	{
		sip_setHold(ps,enableB);
	}
	else
	{
		//sip_setMute(ps,enableB);
	}
}

struct ltpStack  *ltpInitNew(int siponB,int maxslots, int maxbitrate, int framesPerPacket)
{
	struct ltpStack  *tmpP=0;
	if(siponB)
	{
		tmpP = sip_ltpInit(maxslots,maxbitrate,framesPerPacket);
	}
	else
	{
		//sip_setMute(ps,enableB);
		tmpP = LTP_ltpInit(maxslots,maxbitrate,framesPerPacket);
	}
	if(tmpP)
	{
		tmpP->sipOnB = siponB;
		tmpP->timeOut = MAXTIMEOUT;
	}
	return tmpP;
}


int ltpTalk(struct ltpStack *ps, char *remoteid)
{
	
	if(ps->sipOnB)
	{
		//return sip_ltpTalk(ps,remoteid);
		return 0;
	}
	else
	{
		return 0;
		//return LTP_ltpTalk(ps,remoteid);
		//sip_setMute(ps,enableB);
	}
}

void ltpHangup(struct ltpStack *ps, int lineid)
{
	
	if(lineid>=0)
	{	
		if(ps->sipOnB)
		{
			sip_ltpHangup(ps,lineid);
		}
		else
		{
			LTP_ltpHangup(ps,lineid);
			//sip_setMute(ps,enableB);
		}
	}
	else {
		int i;
		
		for (i = 0; i < ps->maxSlots; i++)
		{
			if (ps->call[i].ltpState != CALL_IDLE){
				if(ps->sipOnB)
				{
					sip_ltpHangup(ps,ps->call[i].lineId );
				}
				else
				{
					LTP_ltpHangup(ps,ps->call[i].lineId );
					//sip_setMute(ps,enableB);
				}
				
			}
		}	
	}

}

void ltpRefuse(struct ltpStack *ps, int lineid, char *msg)
{
	if(ps->sipOnB)
	{
		sip_ltpHangup(ps,lineid);
		return;
	}
	else
	{
		LTP_ltpRefuse(ps,lineid,msg);
		//sip_setMute(ps,enableB);
	}
}

void ltpAnswer(struct ltpStack *ps, int lineid)
{
	if(ps->sipOnB)
	{
		sip_ltpAnswer(ps,lineid);
	}
	else
	{
		LTP_ltpAnswer(ps,lineid);
		//sip_setMute(ps,enableB);
	}
}

int ltpRing(struct ltpStack *ps, char *remoteid, int command)
{
	if(ps->sipOnB)
	{
		return sip_ltpRing(ps,remoteid,command);
	}
	else
	{
		//sip_setMute(ps,enableB);
		if(*remoteid=='+')//if plus is added
		{
			remoteid = remoteid + 1;
		}
		return LTP_ltpRing(ps,remoteid,command);
	}
}

int ltpMessage(struct ltpStack *ps, char *userid, char *msg)
{
	if(ps->sipOnB)
	{
		//sip_ltpMessage(ps,userid,msg);
		return 0;
	}
	else
	{
		//sip_setMute(ps,enableB);
		return 	LTP_ltpMessage(ps,userid,msg);
	}
}

void ltpChat(struct ltpStack *ps, char *userid, char *message)
{
	if(ps->sipOnB)
	{
		//sip_ltpChat(ps,userid,message);
		return ;
	}
	else
	{
	   //	LTP_ltpChat(ps,userid,message);
		//sip_setMute(ps,enableB);
	}
}

void ltpLogin(struct ltpStack *ps, int command)
{
	
	if(ps->sipOnB)
	{
		sip_ltpLogin(ps,command);
	}
	else
	{
		LTP_ltpLogin(ps,command);
	}
}

void ltpTick(struct ltpStack *ps, unsigned int timeNow)
{
	if(ps->sipOnB)
	{
		sip_ltpTick(ps,timeNow);
	}
	else
	{
		LTP_ltpTick(ps,timeNow);
	}
}

void ltpLoginCancel(struct ltpStack *ps)
{
	if(ps->sipOnB)
	{
		sip_ltpLoginCancel(ps);
	}
	else
	{
	//	LTP_ltpLoginCancel(ps);
		return;
	}
}
void ltpMessageDTMF(struct ltpStack *ps, int lineid, char *msg)
{
	if(ps->sipOnB)
	{
		 sip_ltpMessageDTMF(ps, lineid, msg);
	}
	else
	{
		LTP_ltpMessageDTMF(ps, lineid, msg);
	}
}
void startConference(struct ltpStack *ps)
{
	int	i;

	for (i = 0; i < ps->maxSlots; i++)
		if (ps->call[i].ltpState != CALL_IDLE && ps->call[i].ltpState != CALL_HANGING){
			if(ps->sipOnB)
			{
				pjsua_call_reinvite((pjsua_call_id)ps->call[i].ltpSession, PJ_TRUE, NULL);
			}
			ps->call[i].InConference = 1;
		}
}
void shiftToConferenceCall(struct ltpStack *ps,int oldLineId)
{
	int	i;
	int lactiveLineId = -1;
	for (i = 0; i < ps->maxSlots; i++)
	{	
		if (ps->call[i].ltpState != CALL_IDLE&& ps->call[i].ltpState != CALL_HANGING){
			
			if(ps->call[i].lineId!=oldLineId)
			{
				ps->call[i].InConference = 1;
			}
			else {
				ps->call[i].InConference = 0;
			}
			if(ps->sipOnB)
			{
				if(ps->call[i].lineId!=oldLineId)
				{	
					pjsua_call_reinvite((pjsua_call_id)ps->call[i].ltpSession, PJ_TRUE, NULL);
					if(lactiveLineId<0)
					{	
						lactiveLineId = ps->call[i].lineId;
					}	
					
				}
				else {
						pjsua_call_set_hold((pjsua_call_id)ps->call[i].ltpSession, NULL);
					//pjsua_call_reinvite((pjsua_call_id)ps->call[i].ltpSession, PJ_FALSE, NULL);
				}
				
			}
			else {
				if(ps->call[i].lineId!=oldLineId)
				{	
					if(lactiveLineId<0)
					{	
						lactiveLineId = ps->call[i].lineId;
					}
				}	
			}
		}
	}	
	if(lactiveLineId>=0)
	{
		ps->activeLine= lactiveLineId;
	}
}
void setPrivateCall(struct ltpStack *ps,int lineid)
{
	int	i;

	for (i = 0; i < ps->maxSlots; i++)
	{	
		if (ps->call[i].ltpState != CALL_IDLE && ps->call[i].ltpState != CALL_HANGING){
			if(ps->sipOnB)
			{
				if(ps->call[i].lineId !=lineid)
				{	
					//	pjsua_call_reinvite((pjsua_call_id)ps->call[i].ltpSession, PJ_FALSE, NULL);
					pjsua_call_set_hold((pjsua_call_id)ps->call[i].ltpSession, NULL);
				}
				else {
					pjsua_call_reinvite((pjsua_call_id)ps->call[i].ltpSession, PJ_TRUE, NULL);
				}
			}
			//if(ps->call[i].lineId ==lineid)
			{	
				ps->call[i].InConference = 0;
				
			}	
		}
	}	
	
	ps->activeLine= lineid;
}
void switchReinvite(struct ltpStack *ps, int lineid)
{
	int	i;
	pjsua_call_id call_id;
	
	for (i = 0; i < ps->maxSlots; i++)
		if (ps->call[i].ltpState != CALL_IDLE&& ps->call[i].ltpState != CALL_HANGING){
			call_id = (pjsua_call_id) ps->call[i].ltpSession;
			if (lineid == ps->call[i].lineId){
				if(ps->sipOnB)
				{
					pjsua_call_reinvite((pjsua_call_id)ps->call[i].ltpSession, PJ_TRUE, NULL);
				}
				ps->activeLine = lineid;
			}
			else if (!ps->call[i].InConference && ps->call[i].ltpState == CALL_CONNECTED) //hold all the calls not in conference
			{
				if(ps->sipOnB)
				{
					pjsua_call_set_hold((pjsua_call_id)ps->call[i].ltpSession, NULL);
				}
				else
				{
					ps->activeLine = lineid; //put on hold
				}
			}
		}
}

void Unconference(struct ltpStack *pstackP)
{
	int i;
	for (i=0; i < pstackP->maxSlots; ++i)
		pstackP->call[i].InConference = 0;
}	


