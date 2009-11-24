/* This is not just a header, it explains a lot of LTP implementation.
Read this along with the LTP Protocol Draft 1.0
- Farhan
*/


/*	the LTPSTACK_DEFINED preprocessor variable will
	avoid the definitions from being included twice
*/
#ifndef LTPSTACK_DEFINED
#define LTPSTACK_DEFINED
#ifdef __cplusplus
extern "C" {
#endif 

/* gsm is mandatory in all ltp implementations */
#include <gsm.h>

/* SUPPORT_SPEEX is a preprocessor directive that should be passed from compiler options */

#define SUPPORT_SPEEX 1

#define short16 short
#define int32 int


#ifdef SUPPORT_SPEEX
#include <speex/speex.h>
#endif

/*	Queue:
	Before we dip into LTP proper, I have used a very simple queue of words for much of
	my work. This has a very simple API. Lets get over with it here.

	It is a non-blocking, in-memory queuing system for sound sample pipes
*/
#define MAX_Q	4800
struct Queue
{
	int head;
	int	tail;
	int	count;
	short16	data[MAX_Q];
	int  stall;
};
void queueInit(struct Queue *p);
void queueEnque(struct Queue *p, short16 w);
short16 queueDeque(struct Queue *p);




/*
The LTP data types:

The LTP messages have three data types.
The character string is considered to be a UTF-8, 8-bit
character array terminated by a null.

There are two integer types defined: a 32-bit integer defined as int32 
and a 16-bit short defined as short16. depending on the compiler
and platform type, these two integer types will have to be defined.


The LTP messages use little endian number format for
all numbers except for the contact fields that contain the
ip address and port. these are stored in the big-endian format
so that the standard bsd socket api works without having to resort
to byte flipping.

Tip: this format will work on all ARM and x86 platforms that provide
socket API for TCP/IP without any worries. Only the Macintosh PPC 
developers will have to keep any eye on the LTP messages.

The three function zeroMemory, flipShort and flipInt32 are utility functions
that provide platform independent implementations to handle LTP packet data. 
They also eliminate the need to include headers and libraries 
that are not required otherwise.
*/


#pragma pack(1)

#define MAX_USERID 64
#define RTP_PORT (5004)

/*	All LTP packets contain exactly the same fields.
	Every LTP packet is either a request or a response. 
	Every LTP request is retransmitted until a response is received or timeout.
	A request has response field set to 0.
	The requests are matched to reponses with to, from, session and msgid.
*/

/* the symbian gcc compiler does not align the fields on bytes hence, this will force it to do so */

#if defined(__GCC32__)
	 __attribute__((packed,aligned(1)))
#endif
;


/* the following commands ids (values in command field) are defined in this implementation */
#define CMD_LOGIN		1
#define CMD_NEWUSER		2
#define CMD_RING		3
#define CMD_ANSWER		4
#define CMD_MSG			5
#define CMD_HANGUP		6
#define CMD_CANCEL		7
#define CMD_LOGOUT		8
#define CMD_REFUSE		9
#define CMD_NOTIFY		10
#define CMD_SUBSCRIBE	11
#define CMD_TALK		12

/* these are the possible valid values of the response field */
#define RESPONSE_OK 200
#define RESPONSE_OK_RELAYED 202
#define RESPONSE_REDIRECT 302
#define RESPONSE_AUTHENTICATION_REQUIRED 407
#define RESPONSE_NOT_FOUND 404
#define RESPONSE_OFFLINE 480
#define RESPONSE_ERROR 401
#define RESPONSE_BUSY 408

/* for login packets, the wparam is set to one of these values, set it to
NORMAL_PRIORITY for clients, GATEWAY_PRIORITY is used only for end-points with
fixed public IP addresses, MAIL_PRIORITY is a low priority login. */
#define NORMAL_PRIORITY 5
#define MAIL_PRIORITY 1
#define GATEWAY_PRIORITY 3

struct ltp
{
	/* always set to 1 in this version of LTP */
	short16	version;	

	/* see the #define CMD_xxx to know different commands */
	short16	command;	
	
	/* see the #define RESPONSE_xxx to know different responses */
	short16	response;   
	
	/* zero terminated, regular C string */
	char	to[MAX_USERID]; 

	/* zero termianted, regular C string */
	char	from[MAX_USERID]; 
	
	/* each individual call, login session has a different session number */
	unsigned int32		session; 

	/* each request within a session has a different message id */
	unsigned int32		msgid;
	
	/* a RESPONSE_AUTHENTICATE will carry a challange and a subsequent request will carry an MD5 checksum (read Sec 6 of the draft) */
	char	authenticate[16]; 
	
	/* When a received request has these fields set to non-zero, you should
	transmit back the response to the same end-point that you received it from and
	not touch the contact fields (which maybe used for forwarding the response back).
	
	When a received response with response=RESPONSE_REDIRECT has these fields set to non-zero, 
	the request should be re-send to the end-point specified in these fields.
	*/
	int32		contactIp;
	short16	contactPort;

	/* general purpose fields used for different things in different types of requests */
	short16	wparam;
	int32		lparam;

	/* this field actually extends to the end of the packet.
	It contains UTF-8 encoded text that is not interpreted by the stack but passed onto
	the application for possible display to the human user. For ex: in a chat message
	this field will carry the actual text */
	char	data[1];
};




#pragma pack(1)


/* 
	The Call structure:

	The stack represents each call as an instance of this structure.
	The stack initializes a number of call structure in the begining as an array (see ltpInit())


	An application can read any of the fields. the important ones are:

	remoteUserid : holds the LTP id of the remote party.
	ltpState: the state of the call (see the #define CALL_xx)
	title:	if the title of teh caller is available, then it is displayed (from the contacts list)
	lineid: specifies which 'virtual line' the call belongs to.
	inTalk: set if the call is a PTT call, 0 if the call is a regular voice call
	
	You can modify just one field directly:
	inConference: setting this to 1 will put the call in conference. 0 will take it out.

	To change anything about a call use the ltp api functions.
*/

/* LTP call state enumeration. Some of them are not used in the implementation */

#define CALL_IDLE 0
#define CALL_RING_SENT 1
#define CALL_RING_ACCEPTED 2
#define CALL_RING_RECEIVED 3
#define CALL_CONNECTED 11
#define CALL_HANGING 12


struct Call
{
		/* at times a call might be passed to a function as an argument, 
		we need to identify it in the call table by it's lineid. lineId
		is identified by this member */ 
		int	lineId;
		
		/* if this call is in conference, then set this to non-zero (not necessarily 1) */
		int	InConference;
		
		/* talk is actually push-to-talk (one-way audio). inTalk signals that 
		the call has the local mic muted */
		int	inTalk;
				
		/* the end-point to which all the packets of this call are to be streamed.
		Often, if the call is being relayed, then the end-point to which you have
		to send all the packet is a relay, hence the remoteIp/Port maybe either a 
		a relay or an end-point, in case the remoteIp/Port is a relay, then the actual
		end-point to which the relay has to foward the packets is stored in fwdIP/port.
		Note: If fwdIP is set to zero, then the call is not being relayed 
		(Read Section 4 of the the LTP draft.)
		*/
		 
		unsigned int32		remoteIp;
 		unsigned short16	remotePort;
		unsigned int32		fwdIP;
		unsigned short16	fwdPort;

		/* Set if the remote media stream is not silent (but has active speech). */
		short remoteVoice;

		/*  a number of incoming ring messages are directly responded to, by sending their
		response directly to the calling user's end-point. If the ring messages still 
		keep coming, it means that the direct path to the caller is not available and you 
		need to get the packets relayed */ 
		short	nRingMessages;


		/* set when we were able to establish direct contact with the remote without
		requiring a relay */
		short	directMedia;

		/* the remote peer (the one this call is connected to) */
        char	remoteUserid[64];

		/* a human friendly title for the remote userid (the userid can be a number, title is a name) */
		char	title[1000];

		/* nonce is required when you need to challenge the other side to show credentials */
        char	nonce[64];
	
		/* shows the current state of the call, look at the call states just above this structure */
		int		ltpState;

		/* if there is a trunck route, more than one calls maybe going on between
		the same two end-points, in which case, we cannot individual calls with their 
		remoteUserid alone, we will also require a session that is unique between two end-points */
        unsigned int ltpSession;

		/* each message within a call is identified by a unique message id. Once a message is received,
		the lastMsgReceived is set to that msgid so that if we get a repeat message (if the response was
		lost), then we don't have to process it all over again. Similarly, ltpSeq is the 
		message id to be used for the next outgoing message */
		unsigned int		ltpSeq;
		unsigned int		ltpLastMsgReceived;

		/* this is a buffer to store the message that being sent repeatedly to the remote end
		but we haven't yet gotten a response for */
		char	ltpRequest[1000];
		int		ltpRequestLength;

		/* when do we next retransmit the above message? */
		unsigned int	retryNext;
		/* how many times more should we transmit the above message before giving up? */
		int		retryCount;

		/* this is the time a call starts and ends */
        unsigned int timeStart;
        unsigned int  timeStop;

		/* max call time for this call in seconds. -1 sets no time limit */
        int      maxTime;

		/* at the end of a call, this specifies what kind of a call it was, check the calltype definitions
		above */
		int		kindOfCall;

		/* gsm codec being used for this call, this is defined in the gsm includes above */
		gsm gsmIn;
		gsm gsmOut;

		/* prevSample is used when the media is being resampled at a lower bitrate */
		short prevSample;

		/* media input and output stream pipes */
        struct Queue   pcmOut, pcmIn;

#ifdef SUPPORT_SPEEX

		/* speex narrow-band codec */
		void *speex_enc;
		void *speex_dec;
		struct SpeexBits	speexBitEnc;
		struct SpeexBits	speexBitDec;

		/* speex wide-band codec */
		void *speex_wb_enc;
		void *speex_wb_dec;
		struct SpeexBits	speexWideBitEnc;
		struct SpeexBits	speexWideBitDec;
#endif
		/* rtp state : a trivial rtp implementation without RTCP */
        unsigned short16    rtpSequence;
        unsigned int32 rtpTimestamp;
		short16		codec;

		int		coef[12];
		int		delay[12];

		/* often, the call is associated  with a window or a channel etc. this int can be cast to stor
		this */
		int		uiHandle;

		/* though, not implemented yet, this will store a private encryption key
		for encrypting voice between end-points */
		char	key[128];
};

/*
#pragma pack(1)
#define MAX_USERID 64
#define RTP_PORT (5004)

struct ltp
{
	short16	version;
	short16	command;
	short16	response;
	char	to[MAX_USERID];
	char	from[MAX_USERID];
	unsigned int32		session;
	unsigned int32		msgid;
	char	authenticate[16];
	int32		contactIp;
	short16	contactPort;
	short16	wparam;
	int32		lparam;
	char	data[1];
}
#if defined(__GCC32__)
	 __attribute__((packed,aligned(1)))
#endif
;
*/
/* Each RTP packet is cast as a struct rtp. Those rtp packets
that are being relayed will use an rtp extenion struct rtpExt.
see RFC 1889.
*/

/* the following payload codecs are defined.*/
#define RTP_FLAG 0x80
#define PCMU	0
#define PCMA	8
#define GSM		3
#define LGSM	98
#define SPEEX 97

#define RTP_HEADER_SIZE 12
#define RTP_EXT_SIZE 12
#define RTP_VERSION 0x80
#define RTP_FLAG_EXT 0x10

struct rtp
{
	unsigned char	flags;
	unsigned char	payload;
	short			sequence;
	unsigned int	timestamp;
	unsigned int	ssrc;
	unsigned char	data[1];
}
#if defined(__GCC32__)
	 __attribute__((packed,aligned(1)))
#endif
;

struct rtpExt
{
	short	extID;
	short	intCount;
	int		fwdIP;
	short	fwdPort;
	short	padding;
}
#if defined(__GCC32__)
	 __attribute__((packed,aligned(1)))
#endif
;

#pragma pack()

/*
	Contacts:
	All contacts are maintained in an array called 'book' within the ltpstack.

	each contact is uniquely identified by its ltp id.
	the most important fields in the contact field are:

	userid	(the ltp userid)
	title	(the displayed name of the userid)
	presence (a numeric value that shows the icon against each user)
	label	(a message set by the contact to show to all the contacts)

	as an application developer, you should not change any of these fields in this
	structure. use the ltp API to manipulate them (just sorting for now).
*/

#define MAX_CONTACTS 200
#define MAX_TITLE 100

/* ltp presence states */

#define NOTIFY_OFFLINE 0
#define NOTIFY_ONLINE 1
#define NOTIFY_BUSY    2
#define NOTIFY_AWAY  3
#define NOTIFY_SILENT  4
#define NOTIFY_CHAT 5
#define NOTIFY_ROAMING 6
#define NOTIFY_DRIVING 7
#define NOTIFY_SLEEPING 8
#define NOTIFY_ONLINE_MOBILE 9
#define NOTIFY_BUSY_MOBILE    10
#define NOTIFY_SILENT_MOBILE  11
#define NOTIFY_FORWARD 13

/*
struct Contact{
	char		userid[MAX_USERID];
	char		title[100];
	char		label[128];
	char		group[40];
	char		device[MAX_USERID];
	unsigned int32		ip;
	unsigned short16	port;
	unsigned int32		fwdip;
	unsigned short16	fwdport;
	unsigned short16	presence;	
	unsigned int32		session;
	unsigned int32		msgid;
	unsigned int32		lastMsgReceived;
	int		lastMsgCount;
	int		blocked;
	short	directMedia;
	short	chatSeq;

	unsigned int	nextDate;
	int		retryCount;
	int	ui;

	char	notifyOut[1000];
	char	scratch[1000];
	int lastChatChecksum;
	struct Queue chatq;
};

*/
/* define the type of call routing */
#define USE_DIRECT 0
#define USE_PROXY 1


#define LTP_CODEC_GSM 0
#define LTP_CODEC_ULAW 1
#define LTP_CODEC_ALAW 2
/* 2009/06/03 added to work with individual speex packets */
#define LTP_CODEC_SPEEX 3
#define LTP_CODEC_LGSM 98

#define LOGIN_STATUS_OFFLINE 0
#define LOGIN_STATUS_NO_ACCESS 1
#define LOGIN_STATUS_FAILED 2
#define LOGIN_STATUS_TRYING_LOGIN 3
#define LOGIN_STATUS_ONLINE 4
#define LOGIN_STATUS_TRYING_LOGOUT 5
#define LOGIN_STATUS_BUSY_OTHERDEVICE 6
#define LOGIN_STATUS_TIMEDOUT	7

#define LTP_MSG_REMOTE 1
#define LTP_MSG_YOUR 2
#define LTP_MSG_ALERT 3

#define LTP_MAX_MESSAGE 300

#define CALLTYPE_IN 1
#define CALLTYPE_OUT 2

#define CALLTYPE_CALL 4
#define CALLTYPE_TALK 8

#define CALLTYPE_MISSED 16
#define CALLTYPE_BUSY 32
#define CALLTYPE_OFFLINE 64
#define CALLTYPE_NETWORKERROR 128
#define CALLTYPE_BADADDRESS 256



/* this is the big ltp structure that in turn holds call and contact arrays.
The important field for you to set are ltpUserid, ltpPassword and ltpServer.
You can keep checking on loginStatus to see your current state */
struct ltpStack
{
	int incount, outcount;
	int msgCount;
	int loginCommand;
	unsigned int	dateNextEcho;
	int	bigEndian;
	int maxMessageRetries;

	/*holds the incoming or outgoing message
	int	nextMsgId;
	int	prevMsgChecksum; */

	/* ltpLogin */ 
	char    ltpNoncePrevious[20];
	char    cookie[100];
	unsigned int32	ltpServer;
	unsigned short16	ltpServerPort;
	int     nextCallSession;
	int     nat;
	unsigned	int loginNextDate;
	int		loginRetryCount;
	unsigned int	loginRetryDate;
	int		loginSession;

	char	scratch[4000];
	short	pcmBuff[4000];
	int		nframesPerPacket;
	char	inbuff[2000];
	short	rtpInBuff[6400];

	int doDebug;
	int	forceProxy;
	int	doPreprocess;
	char	ltpUserid[40], ltpPassword[40], ltpServerName[40], motd[1000];
	char    ltpNonce[20];
	char	ltpTitle[1000];
	char	ltpLabel[128];
	char	userAgent[MAX_USERID];
	int		myPriority;
	int		soundBlockSize;
	int		nticks;
	short16		ltpPresence;
	
	/* above 100, it is the speed of speex codec */
	short16		defaultCodec;
	int		activeLine;
	struct Call	*call;
	int		maxSlots;
	unsigned int	now;
	int		loginStatus;
	int		muteAll;
	int		rememberLogin;
	int		ringTimeout;
	int		forceTx;
	int		wallClock;

	/* contacts management */
	struct Contact	*book;
	unsigned int	dateContactUpdated;
	unsigned int	updateTimingAdvance;
	int nContacts;

	struct Message	*chat;
	int		maxChatRetries;
	int		nextMsgID;

	int debugCount;
};


/* chat structure */
#define MAX_CHATS 10
struct Message{

	char		userid[MAX_USERID];
	unsigned int32		ip;
	unsigned short16	port;
	unsigned int32		fwdip;
	unsigned short16	fwdport;

	unsigned int32		lastMsgRecvd;
	unsigned int32		lastMsgSent;
	unsigned int32		msgid;
	unsigned int32		dateLastMsg;
	unsigned int32		session;

	int		blocked;
	int		retryCount;
	int	ui;

	char	outBuff[1200];
	int  length;
};



#define ALERT_INCOMING_CALL 1
#define ALERT_INCOMING_TALK 2
#define ALERT_CONTACT_ONLINE 3
#define ALERT_CONNECTED 4
#define	ALERT_DISCONNECTED 5
#define ALERT_ONLINE 6
#define ALERT_OFFLINE 7
//#define ALERT_CALL_END 8
#define ALERT_MESSAGE 9
#define ALERT_ERROR 10
#define ALERT_WARNING 11
#define ALERT_CHAT 12
#define ALERT_RINGING 13
#define ALERT_CHAT_FAILED 14
#define ALERT_CONTACT_OFFLINE 15
#define ALERT_MESSAGE_ERROR 16
#define ALERT_MESSAGE_DELIVERED 17

/* sub versions by date */
#define UA_SUBVERSION 010000x
#define LTP_MAX_RETRY 5
#define LTP_RETRY_INTERVAL 10
#define LTP_LOGIN_INTERVAL 180
#define LTP_NOTIFY_INTERVAL 180
#define LTP_NOTIFY_MAX_RETRY 10
#define LTP_NOTIFY_RETRY_INTERVAL 15
#define LTP_CHAT_MAX_RETRY 5
#define LTP_CHAT_RETRY_INTERVAL 3


unsigned int32 lookupDNS(char *host);
void alert(int lineid, int alertcode, void *data);
int netWrite(void *msg, int length, unsigned int32 address, unsigned short16 port);
void outputSound(struct ltpStack *ps, struct Call *pc, short *pcm, int length);
int openSound(int isFullDuplex);
void closeSound();
char *netGetIPString(long address);

/* public APIs */
struct ltpStack  *ltpInit(int maxslots, int maxbitrate, int framesPerPacket);
void ltpRefuse(struct ltpStack *ps, int lineid, char *msg);
void ltpHangup(struct ltpStack *ps, int lineid);
int ltpTalk(struct ltpStack *ps, char *remoteid);
void ltpHangup(struct ltpStack *ps, int lineid); 
void ltpRefuse(struct ltpStack *ps, int lineid, char *msg);
void ltpAnswer(struct ltpStack *ps, int lineid);
int ltpRing(struct ltpStack *ps, char *remoteid, int command);
int ltpMessage(struct ltpStack *ps, char *userid, char *msg);
void ltpChat(struct ltpStack *ps, char *userid, char *message);
void ltpLogin(struct ltpStack *ps, int command);
void ltpTick(struct ltpStack *ps, unsigned int timeNow);
void ltpLoginCancel(struct ltpStack *ps);


 #if DEBUG
void ltpTrace(struct ltp *msg);
#endif


/* call onLTPPacket whenever a packet is received on the udp socket dedicated
to the LTP stack */
void ltpOnPacket(struct ltpStack *ps, char *msg, int length, unsigned int32 address, unsigned short16 port);
void ltpSoundInput(struct ltpStack *ps, short *pcm, int nsamples, int isSpeaking);
int ltpUpdateContact(struct ltpStack *ps, char *userid, char *title, char *group, unsigned short16 presence, 
		unsigned int32 ip, unsigned short16 port, unsigned int32 fwdip, unsigned short16 fwdport,
		char *device, char *label);
void ltpMessageDTMF(struct ltpStack *ps, int lineid, char *msg);
/*
void ltpRemoveAllContacts(struct ltpStack *ps);
struct Contact *getContact(struct ltpStack *ps, char *userid);
void ltpSortContacts(struct ltpStack *ps, int byGroups);
*/
void ltpUpdatePresence(struct ltpStack *ps, unsigned short16 state, char *label);

/* we use a slighlty modified md5 algorithm that can use a runtime flag to determine if it is
being used on a big endian system */
typedef unsigned long uint32;
struct MD5Context {
		uint32 buf[4];
		uint32 bits[2];
		unsigned char in[64];
};

void MD5Init(struct MD5Context *ctx);
void MD5Update(struct MD5Context *ctx, unsigned char const  *buf, unsigned len, int isBigEndian);
void MD5Final(unsigned char *digest, struct MD5Context *ctx);


#ifdef __cplusplus
}
#endif
 
#endif