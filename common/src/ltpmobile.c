#include <ltpmobile.h>
#include <stdlib.h>
#include <string.h>
#define USERAGENT  "desktop-windows-d2-1.0"
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


/* byte flipping functions:
These are required when we have to flip bytes within an int32 (4 bytes = 32bit integer)
to convert between big-endian and little-endian system.

The ltpStack structure has a flag called bigEndian. When we init the ltpStack, we
write '1' to an int and check which byte gets set and hence we detect if the system
is bigEndian. the flip16 (for 16-bit flips) and flip32 (for 32-bit flips are called
mostly upon checking the bigEndian property of the stack.

*/

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

/* callInit:
Initializes the call to be ready for the next call. Becareful in not
initializing by calling zeroMemory on this structure, that will free
up the code structures that are dynamically allocated in ltpInit()
*/
static void callInit(struct Call *p, short16 defaultCodec)
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

struct ltpStack  *ltpInit(int maxslots, int maxbitrate, int framesPerPacket)
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
		callInit(p, ps->defaultCodec);

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


/* ltpWrite:
Sends an ltp packet out, it might require some bits to be flipped over
if you are running on a big endian system */

int ltpWrite(struct ltpStack *ps, struct ltp *ppack, unsigned int length, unsigned int32 ip, unsigned short16  port)
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
/* callStartRequest:
A Call repeatedly sends an ltp message to a remote end-point (or the ltp server)
until a response is received or time-out */
static void callStartRequest(struct ltpStack *ps, struct Call *pc, struct ltp *pltp)
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
	ltpWrite(ps, (struct ltp *)pc->ltpRequest, pc->ltpRequestLength, pc->remoteIp, pc->remotePort);

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
static void callStopRequest(struct Call *pc)
{
	pc->retryCount = 0;
}

/*  
callFindIdle:
finds an empty slot to start a new call
returns NULL if not found (quite possible), always check for null */
struct Call *callFindIdle(struct ltpStack *ps)
{
	int     i;
	struct Call *p;

	for (i = 0; i < ps->maxSlots; i++)
	{
		p = ps->call + i;
		if(p->ltpState == CALL_IDLE) 
		{
			callInit(p, ps->defaultCodec);
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
struct Call *callSearchByRtp(struct ltpStack *ps, unsigned int32 ltpIp, unsigned short16 ltpPort, unsigned int32 fwdip, unsigned short16 fwdport, unsigned int session)
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
static struct Call *callMatch(struct ltpStack *ps, struct ltp *p, unsigned int32 ip, unsigned short16 port, 
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
			callInit(pc, ps->defaultCodec);
}
*/

/*
 first try locating a chat session with userd,
 if found, check if it has an active session and return the session.
 if not found, search for a free chat session and initialize it.
*/
struct Message *getMessage(struct ltpStack *ps, char *userid)
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
static void redirect(struct ltpStack *ps, struct ltp *response, unsigned int fromip, unsigned short fromport)
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

		ltpWrite(ps, response, sizeof(struct ltp) + strlen(response->data), ip, port);
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
void ltpLogin(struct ltpStack *ps, int command)
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
					ltpHangup(ps, i);
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

			ps->myPriority = NORMAL_PRIORITY;
			/* let's rest and try after sometime */
			ps->loginNextDate = (unsigned int)ps->now + LTP_LOGIN_INTERVAL;
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
	
	ltpWrite(ps, ppack, sizeof(struct ltp) + strlen(ppack->data), ps->ltpServer, (short16)(ps->bigEndian ? RTP_PORT : flip16(RTP_PORT)));
#if DEBUG
		printf("lgn %02d %03d %s %s contact:%u to %s %d\n", ppack->command, ppack->response, ppack->from, ppack->to, 
		ppack->contactIp, netGetIPString(ps->ltpServer), RTP_PORT);
#endif
}


/*loginCancel:
cancels any ongoing login or logout */
void ltpLoginCancel(struct ltpStack *ps)
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

static void callTick(struct ltpStack *ps, struct Call *pc)
{
	struct ltp *p = (struct ltp *)pc->ltpRequest;

	/** farhan, aug 20 2009, bug id 23023
	the call on hold was getting dropped by the asterisk side as there were no media packets received at all.
	instead, 
	*/
	if (pc->ltpState == CALL_CONNECTED && ps->activeLine != pc->lineId){
		short silence[160] = {0};
		rtpOut(ps, pc, 160, silence, 0);
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
	ltpWrite(ps, (struct ltp *)pc->ltpRequest, pc->ltpRequestLength, pc->remoteIp, pc->remotePort);

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
void ltpTick(struct ltpStack *ps, unsigned int timeNow)
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
		ltpWrite(ps, (struct ltp *)&ps->loginSession, 4, ps->ltpServer, (short16)(ps->bigEndian ? RTP_PORT : flip16(RTP_PORT)));
		ps->dateNextEcho = timeNow + 60;
	}
	


	/* give each non-idle call a slice of time */
	soundRequired = 0;
	for (i =0; i < ps->maxSlots; i++)
	{
		p = ps->call + i;
		if (p->ltpState == CALL_IDLE)
			continue;
		
		callTick(ps, p);

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
			ltpWrite(ps, (struct ltp *)pmsg->outBuff, pmsg->length, pmsg->ip, pmsg->port);
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
static void onChallenge(struct ltpStack *ps, struct ltp *response, unsigned int fromip, unsigned short fromport)
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

	ltpWrite(ps, response, sizeof(struct ltp) + strlen(response->data), fromip, fromport);

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
void loginResponse(struct ltpStack *ps, struct ltp *msg)
{
    int i;
//	char	*p, *q;

	/* server redirection?
	the server might ask us to retry login in through a proxy or a different server (different domain) */
	if (msg->response == RESPONSE_REDIRECT)
	{
		ps->ltpServer = msg->contactIp;
		msg->response = 0;
		ltpWrite(ps, msg, sizeof(struct ltp) + strlen(msg->data), ps->ltpServer, (short16)(ps->bigEndian? RTP_PORT : flip16(RTP_PORT)));
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
static void logoutResponse(struct ltpStack *ps, struct ltp *msg)
{
	/* server redirection?
	the server might ask us to retry logout in through a proxy or a different server (different domain) */
	if (msg->response == RESPONSE_REDIRECT)
	{
		ps->ltpServer = msg->contactIp;
		msg->response = 0;
		ltpWrite(ps, msg, sizeof(struct ltp) + strlen(msg->data), ps->ltpServer, (short16)(ps->bigEndian? RTP_PORT : flip16(RTP_PORT)));
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



static void  messageResponse(struct ltpStack *ps, struct ltp *pack, unsigned int32 ip, unsigned short16 port)
{
	struct Message *pchat=NULL;

	pchat = getMessage(ps, pack->to);
	if (!pchat)
		return;

	//if this is a confirmation of some other message, skip it.
	if (pchat->lastMsgSent != pack->msgid)
		return;

	//if we are redirected directly to the peer, so be it.
	if (pack->response == RESPONSE_REDIRECT){
		pchat->ip = pack->contactIp;
		pchat->port = pack->contactPort;
		ltpWrite(ps, pack, pchat->length, pchat->ip, pchat->port);
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
static void  ringResponse(struct ltpStack *ps, struct ltp *pack, unsigned int ip, unsigned short port)
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
		callStopRequest(pc);

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
static void hangupResponse(struct ltpStack *ps, struct Call *pc /*, struct ltp *pack, int ip, short port*/)
{
	/* reset the request state */
	callStopRequest(pc);

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
static void onResponse(struct ltpStack *ps, struct ltp *pack, unsigned int ip, unsigned short port)
{
	struct Call	*pc;

	if (pack->response == RESPONSE_AUTHENTICATION_REQUIRED)
	{
		onChallenge(ps, pack, ip, port);
		return;
	}

	if (pack->command == CMD_LOGIN)
	{
		loginResponse(ps, pack);
		return;
	}

	if (pack->command == CMD_LOGOUT)
	{
		logoutResponse(ps, pack);
		return;
	}

	if (pack->command == CMD_MSG){
		messageResponse(ps, pack, ip, port);
		return;
	}

	if(pack->response == RESPONSE_REDIRECT)
	{
		redirect(ps, pack, ip, port);
		return;
	}

	/* ring response will not match ordinarily */ 
	if(pack->command == CMD_RING || pack->command == CMD_TALK)
	{
		ringResponse(ps, pack, ip, port);
		return;
	}

	/* the rest of the messages are call slot specific, hence we need to match this
	with specific calls*/

	pc = callMatch(ps, pack, ip, port, pack->session);
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
		hangupResponse(ps, pc/*, pack, ip, port*/);
		break;		
	}

	callStopRequest(pc);
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
	ltpWrite(ps, p, sizeof(struct ltp) + strlen(p->data), ip, port);
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
static struct Call *onRing(struct ltpStack *ps, struct ltp *ppack, unsigned int fromip, unsigned short fromport)
{
	struct Contact *pcon = NULL;
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
		pc = callFindIdle(ps);
		if (!pc)
		{
			sendResponse(ps, ppack, RESPONSE_BUSY,"Busy", fromip, fromport);
			return NULL;
		}

		callInit(pc, ps->defaultCodec);

		strncpy(pc->remoteUserid, ppack->from, MAX_USERID);
		pc->ltpSession = ppack->session;
		pc->ltpSeq = 0;
		pc->ltpState = CALL_RING_RECEIVED;
		pc->kindOfCall = CALLTYPE_IN | CALLTYPE_CALL;	
		strncpy(pc->remoteUserid, ppack->from, MAX_USERID);
		strncpy(pc->title, ppack->data, MAX_TITLE);
		pc->timeStart = ps->now;

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
static void onAnswer(struct ltpStack *ps, struct ltp *ppack, unsigned int fromip, unsigned short fromport)
{
	struct Call	*pc;

	pc = callMatch(ps, ppack, fromip, fromport, ppack->session);

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
		callStopRequest(pc);
		pc->ltpState = CALL_CONNECTED;
		pc->timeStart = ps->now;
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
static void onRefuse(struct ltpStack *ps, struct ltp *ppack, unsigned int fromip, unsigned short fromport)
{
	struct Call	*pc;

	pc = callMatch(ps, ppack, fromip, fromport, ppack->session);
	
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

static void onHangup(struct ltpStack *ps, struct ltp *ppack, unsigned int fromip, unsigned short fromport)
{
	struct Call	*pc;

	pc = callMatch(ps, ppack, fromip, fromport, ppack->session);
	
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

static void onMessage(struct ltpStack *ps, struct ltp *ppack, unsigned int fromip, unsigned short fromport)
{
	struct Message *pmsg;
	int iContact=-1;
	unsigned short16 prevport;
	unsigned int32	previp;

	if (strcmp(ps->ltpUserid, ppack->to))
		return;

	pmsg = getMessage(ps, ppack->from);
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
void ltpTrace(struct ltp *msg)
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
static void ltpIn(struct ltpStack *ps, unsigned int fromip, short unsigned fromport, char *buffin, int len)
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
		onResponse(ps, ppack, fromip, fromport);
		return;
	}


	/* handle incoming requests only when logged in. */
	if (ps->loginStatus != LOGIN_STATUS_ONLINE)
		return;

    /* handle the incoming message, based on the message type */ 
	switch(ppack->command)
	{
	case CMD_RING:
		onRing(ps, ppack, fromip, fromport);
		break;
	case CMD_TALK:
		pc = onRing(ps, ppack, fromip, fromport);
		if (pc)
			pc->inTalk;
		break;
	case CMD_ANSWER:
		onAnswer(ps, ppack, fromip, fromport);
		break;
	case CMD_REFUSE:
		onRefuse(ps, ppack, fromip, fromport);
		break; 
	case CMD_HANGUP:
		onHangup(ps, ppack, fromip, fromport);
		break;
	case CMD_MSG:
		onMessage(ps, ppack, fromip, fromport);
		break;
	}
}


/* ltpMessage:
A simple request to send text in the data field of the ltp message 
to the remote end-point in a call
*/
int ltpMessage(struct ltpStack *ps, char *userid, char *msg)
{
	struct Message *pmsg;
	struct ltp *ppack;

	if (strlen(msg) > 999)
		return 0;

	pmsg = getMessage(ps, userid);
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

	ltpWrite(ps, (struct ltp *)pmsg->outBuff, pmsg->length, pmsg->ip, pmsg->port);
	return 1;
}

/* ltpRing:
sends a ring reqest
It can get quite complex in the way this is received at the other
end. read the comments on onRing()
*/
int ltpRing(struct ltpStack *ps, char *remoteid, int mode)
{
	struct Call *pc;
	struct ltp *ppack;

	pc = callFindIdle(ps);
	if (!pc)
	{
		alert(-1, ALERT_ERROR, "Disconnect before calling");
		return 0;
	}
	callInit(pc, ps->defaultCodec);
	
	strncpy(pc->remoteUserid, remoteid, MAX_USERID);
	pc->ltpSession = ++ps->nextCallSession;
	pc->ltpSeq = 0;
	pc->ltpState = CALL_RING_SENT;
	pc->remoteIp = ps->ltpServer;
	pc->remotePort = ps->bigEndian ? RTP_PORT : flip16(RTP_PORT);
	pc->kindOfCall = CALLTYPE_OUT | CALLTYPE_CALL;
	pc->timeStart = ps->now;
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

	callStartRequest(ps, pc, NULL);
	
	return pc->lineId;
}

/* ltpAnswer, ltpRefuse and ltpHangup assume
that the route to the remote end-point is already established
through the CMD_RING's negotiations
*/
void ltpAnswer(struct ltpStack *ps, int lineid)
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
    pc->ltpState = CALL_CONNECTED;
	alert(pc->lineId, ALERT_CONNECTED, pc->title);
	callStartRequest(ps, pc, NULL);

	// the app opens sound on alert_connected
//	openSound(1);
}

void ltpRefuse(struct ltpStack *ps, int lineid, char *msg)
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

	callStartRequest(ps, pc, NULL);
}

void ltpHangup(struct ltpStack *ps, int lineid)
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


	callStartRequest(ps, pc, NULL);
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
void rtpIn(struct ltpStack *ps, unsigned int ip, unsigned short port, char *buff, int length)
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
		pc = callSearchByRtp(ps, ip, port, fwdip, fwdport, prtp->ssrc);
		data = prtp->data + RTP_EXT_SIZE;
		length -= RTP_EXT_SIZE;	
	}
	else
	{
		pc = callSearchByRtp(ps, ip, port,0,0,prtp->ssrc);
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
static int rtpOut(struct ltpStack *ps, struct Call *pc, int nsamples, short *pcm, int isSpeaking)
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

If there is no conference on, then it is easy, just pass it on to the rtpOut.

The conference is done very simply, one sample is read from each of the participant's
input pcm queue. All the samples are added up and then written back to each participant's
output pcm queue (after subtracting that paricipant's own sample: to prevent echo).

When all the participant queues are processed, the output queues are read back one 
by one and output through rtpOut()
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
				rtpOut(ps, pc, nsamples, pcm, isSpeaking);
			return;
		}


		if (!pc->InConference && pc->ltpState == CALL_CONNECTED)
				rtpOut(ps, pc, nsamples, pcm, isSpeaking);
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
		rtpOut(ps, pc, nsamples, pcmBuff, 0);
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
		ltpIn(ps, address, port, msg, length);
	else
		rtpIn(ps, address, port, msg, length);
}
void ltpMessageDTMF(struct ltpStack *ps, int lineid, char *msg)
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

	callStartRequest(ps, pc, ppack);
	free(buff);	
}