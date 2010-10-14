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

#ifndef _SIP_AND_LTP_WRAPPER_H
	#define _SIP_AND_LTP_WRAPPER_H
#define _SPEEX_CODEC_
//#define SRV_RECORD
#define ATTEMPT_LOGIN_ERROR      6015 
#define ATTEMPT_LOGIN_ON_OPEN_PORT 6016
#define ATTEMPT_VPN_CONNECT_SUCCESS 6017
#define ATTEMPT_VPN_CONNECT_UNSUCCESS 6018
#define ATTEMPT_VPN_CONNECT_EXIT 6019

#define MAXTIMEOUT 300
#include "ltpmobile.h"
#ifdef _OPEN_VPN_
#include "openvpninterface.h"
#endif
//#define _ENCRIPTION_
#ifdef _ENCRIPTION_
#define DEFAULT_SIP_PORT  "9065"
#define SIP_PORT1   DEFAULT_SIP_PORT
#define SIP_DOMAIN	"174.143.168.31"
#else

#define DEFAULT_SIP_PORT  "5060"
#define SIP_PORT1   "8060"
#define SIP_PORT2   "9060"
#define SIP_PORT3   "5062"
#define SIP_PORT4   "5060"
#define SIP_DOMAIN	"spokn.com"
#endif

#ifdef __cplusplus
extern "C" {
#endif 
typedef struct vpnDataStructureType
	{
		unsigned char data[1500];
		int  length;
		int ipAddressSrc;
		int portSrc;

		int ipAddressDst;
		int portDst;
		struct vpnDataStructureType *next;
		
	}vpnDataStructureType;
typedef struct SipOptionDataType
	{
		char connectionUrl[100];
		void *dataP;
		int errorCode;
	}SipOptionDataType;
int sip_spokn_pj_init(struct ltpStack *ps,char* luserAgentP,char *errorstring);
void LTP_ltpHangup(struct ltpStack *ps, int lineid);
void LTP_ltpRefuse(struct ltpStack *ps, int lineid, char *msg);

void LTP_ltpAnswer(struct ltpStack *ps, int lineid);

int LTP_ltpRing(struct ltpStack *ps, char *remoteid, int mode);

int LTP_ltpMessage(struct ltpStack *ps, char *userid, char *msg);
static int LTP_rtpOut(struct ltpStack *ps, struct Call *pc, int nsamples, short *pcm, int isSpeaking);
struct ltpStack  *LTP_ltpInit(int maxslots, int maxbitrate, int framesPerPacket);
void LTP_ltpLogin(struct ltpStack *ps, int command);
void LTP_ltpTick(struct ltpStack *ps, unsigned int timeNow);
void LTP_ltpMessageDTMF(struct ltpStack *ps, int lineid, char *msg);
struct ltpStack  *ltpInitNew(int siponB,int maxslots, int maxbitrate, int framesPerPacket);
void startConference(struct ltpStack *ps);
void switchReinvite(struct ltpStack *ps, int lineid);
void Unconference(struct ltpStack *pstackP);	
void shiftToConferenceCall(struct ltpStack *ps,int oldLineId);
void setPrivateCall(struct ltpStack *ps,int lineid);
void sip_pj_DeInit(struct ltpStack *ps);
int sip_spokn_pj_Create(struct ltpStack *ps);	
int sip_set_udp_transport(struct ltpStack *ps,char *userId,char *errorstring,int *p_id);	
int sip_destroy_transation(struct ltpStack *ps);
int sip_set_randomVariable(struct ltpStack *ps,int randVariable);	
int sip_spokn_pj_config(struct ltpStack *ps, char *userAgentP,char *errorstring);	
void setLog(struct ltpStack *ps, int onB,char *pathP);	
char *getLogFile(struct ltpStack *ps);
int setSoundDev(int input,  int output,int bVal);
void reInitAudio();
int sip_IsPortOpen(struct ltpStack *ps, char *errorstring,int blockB);
int send_request(int acc_id,char *cstr_method, char *ldst_uriP,void *uDataP);	
typedef unsigned int (*readwriteSipDataCallback1 )(unsigned int*srchostP,unsigned short *srcportP,unsigned int*dstHostP,unsigned short *dstPortP ,unsigned char *data,int *lenP);
extern void setReadWriteCallback(readwriteSipDataCallback1 readSipDataCallbackP,readwriteSipDataCallback1 writeDataCallP );

int readSipDataCallback(unsigned int*srchostP,unsigned short *srcportP,unsigned int*dsthostP,unsigned short *dstportP  ,unsigned char *data,int *lenP);
void setVpnCallback(struct ltpStack *pstackP,char *pathP,char *rscPath);
int writeSipDataCallback(unsigned int*srchostP,unsigned short *srcportP,unsigned int*dsthostP,unsigned short *dstportP  ,unsigned char *data,int *lenP);
	void setDevPath(unsigned char *pathP);
	
#ifdef __cplusplus
}
#endif 

#endif