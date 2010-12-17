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
#define ATTEMPT_VPN_CONNECT_QUIT 6020

#define GOOGLE_PAGE_FOUND 6021
#define GOOGLE_PAGE_NOT_FOUND 6022
#define NET_NOT_AVAILABLE     6023
#define ATTEMPT_VPN_END_CALL 6024
#define MAX_VPN_FAILED 3
#define MAXTIMEOUT 300
#include "ltpmobile.h"
//#define _UI_LOG_
#define _OPEN_VPN_
#ifdef _OPEN_VPN_
#include "openvpninterface.h"
#endif
#ifdef _MACOS_
#define THREAD_PROC void*
#else
#define THREAD_PROC DWORD WINAPI 
#endif
#define SPOKN_SERVER "www.spokn.com"
//#define _ENCRIPTION_
#ifdef _ENCRIPTION_
#define DEFAULT_SIP_PORT  "9065"
#define SIP_PORT1   DEFAULT_SIP_PORT
#define SIP_DOMAIN	"174.143.168.31"
#else
//#define _ENCRIPTION_SUPPORT_IN_MAIN 

#define DEFAULT_SIP_PORT  "5060"
#define SIP_ENCRIPTION_PORT "9065"
#define SIP_PORT1   "8060"
#define SIP_PORT2   "9060"
#define SIP_PORT3   "5062"
#define SIP_PORT4   "5060"
#define SIP_DOMAIN	"spokn.com"
#endif
#define SIP_DEFAULT_PORT 5060
#define GOOGLEPAGE "https://spreadsheets.google.com/pub?key=0Ao-_UbC4VxyDdHdyaDNQQjhKTUpsT1Axc2hQVlhMMnc&hl=en&single=true&gid=0&output=txt"
//#define GOOGLEPAGE "http://www.google.com/notebook/public/08332985133275968837/BDQ8nSgoQobyBnpEj?alt=xml"
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
void sip_setCallIdle(struct ltpStack *ps,int llineID);
int readSipDataCallback(unsigned int*srchostP,unsigned short *srcportP,unsigned int*dsthostP,unsigned short *dstportP  ,unsigned char *data,int *lenP);
void setVpnCallback(struct ltpStack *pstackP,char *pathP,char *rscPath);
int writeSipDataCallback(unsigned int*srchostP,unsigned short *srcportP,unsigned int*dsthostP,unsigned short *dstportP  ,unsigned char *data,int *lenP);
	void setDevPath(unsigned char *pathP);
void setVpnStatus(struct ltpStack *pstackP,OpenVpnStatusType status);
OpenVpnStatusType getVpnStatus(struct ltpStack *pstackP);

//for following hangup path and writting it to a file
void setFolderPath(struct ltpStack *pstackP,char* lpath);
void writeUIlog(struct ltpStack *pstackP,char * data,char *extraInfoP);
void vpnExitNormal(struct ltpStack *pstackP);
void resetFailCount(struct ltpStack *pstackP);
int statusCallbackL(void *udata,int status,int vpnIP);
char* getVpnIP(struct ltpStack *pstackP);
int  getSpoknHost(struct ltpStack *pstackP,char *vpnserverP,char *sipServer,char *spoknServerP,int *vpnportP,char *pageP,char *path);
int  setSpoknHost(struct ltpStack *pstackP,char *vpnserverP,char *sipServer,char *spoknServerP,int vpnport,char *pageP,char *path);
int getGooglePage(struct ltpStack *pstackP);
int getNewServerForSpokn(struct ltpStack *pstackP);
int getServerList(char *data,SipVpnServer *sipVpnServerP);
void setVpnPjsipCallBack();
int closeLocalsocket();
//THREAD_PROC vpnThreadProc(void *uDataP);
unsigned int  resolveLocalDNS(char *host);
#define CACRTDEF "ca.crt"
#define CERTDEF "spokn.crt"
#define KEY     "spokn.key"
//#define OVPN_SERVER "luke.stage.spokn.com"
#define OVPN_SERVER "nkops.com"
#define OVPN_PORT 1935
#define MYXML "<?xml version=\"1.0\"?>" "\r\n"\
"<server>" "\r\n"\
"<host type =\"sip\" name=\"%s\" port=\"%d\"\"/>" "\r\n"\
"<host type =\"spokn\" name=\"%s\" port=\"%d\"/>" "\r\n"\
"<host type =\"vpn\" name=\"%s\" port=\"%d\"/>" "\r\n"\
"</server>""\r\n"
#ifdef _MACOS_

#define OPVNFILE "client\r\ndev tun\r\nproto tcp\r\n<connection>\r\nremote %s %d\r\nconnect-retry-max 3\r\n</connection>\r\nns-cert-type server\r\n\r\nnobind\r\npersist-key\r\npersist-tun\r\nca \"%s/sandbox-ca.crt\"\r\ncert \"%s/sandbox.crt\"\r\nkey \"%s/sandbox.key\""

#else
#define OPVNFILE "client\r\ndev tun\r\nproto tcp\r\n<connection>\r\nremote %s %d\r\nconnect-retry-max 3\r\n</connection>\r\nns-cert-type server\r\n\r\nnobind\r\npersist-key\r\npersist-tun\r\nca \"%s\\\\%s\"\r\ncert \"%s\\\\%s\"\r\nkey \"%s\\\\%s\"\r\ncomp-lzo\r\n"

#endif

#ifdef __cplusplus
}
#endif 

#endif