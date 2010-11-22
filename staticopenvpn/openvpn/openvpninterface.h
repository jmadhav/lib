/*
 *  openvpninterface.h
 *  staticopenvpnssl
 *
 *  Created by Mukesh Sharma on 27/09/10.
 *  Copyright 2010 Geodesic Ltd. All rights reserved.
 *
 */
#ifndef _OPEN_VPN_INTERFACE_H_
#define _OPEN_VPN_INTERFACE_H_
#define VPNMAXPATH 355 
typedef unsigned int (*readwriteDataCallback )(void *udata, unsigned int*srchostP,unsigned short *srcportP,unsigned int*dsthostP,unsigned short *dstportP ,unsigned char *data,int *lenP);
typedef unsigned int (*statusCallback )(void *udata,int status,int vpnIpAddress);
typedef struct OpenvpnInterfaceType
{
	//char caFile[VPNMAXPATH];
	//char certFile[VPNMAXPATH];
	//char keyFile[VPNMAXPATH];
	char confFile[VPNMAXPATH];
	readwriteDataCallback readCallP;
	readwriteDataCallback writeCallP;
	void *uData;
	statusCallback   statusCallP;
	
}OpenvpnInterfaceType;
#ifdef __cplusplus
extern "C" {
#endif 
 int vpnInitAndCallInterface(char *conf,void *uData,readwriteDataCallback readP,readwriteDataCallback writeP, statusCallback statusCallP);

 unsigned char * readDataInterface(unsigned char *buf, int len,unsigned int*srchostP,unsigned short *srcportP,unsigned int*dsthostP,unsigned short *dstportP ,int *lenP);

 void setTurnPathInterface(unsigned char *pathP);
void genrateReadSignalInterface();
void normalExitInterface();
#ifdef __cplusplus
}
#endif 
#endif
