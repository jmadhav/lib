/*
 *  openvpninterface.c
 *  staticopenvpnssl
 *
 *  Created by Mukesh Sharma on 27/09/10.
 *  Copyright 2010 Geodesic Ltd. All rights reserved.
 *
 */


//#include "openvpn.h"
#include "openvpninterface.h"
extern int vpnInitAndCall(char *conf,void *uData,readwriteDataCallback readP,readwriteDataCallback writeP,statusCallback statusP);
extern unsigned char * readData(unsigned char *buf, int len,unsigned int*srchostP,unsigned short *srcportP,unsigned int*dsthostP,unsigned short *dstportP ,int *lenP);
extern void setTurnPath(unsigned char *pathP);
extern void genrateReadSignal();
int vpnInitAndCallInterface(char *conf,void *uData,readwriteDataCallback readP,readwriteDataCallback writeP, statusCallback statusCallP)
{
	return vpnInitAndCall(conf,uData,readP,writeP,statusCallP);
}
void genrateReadSignalInterface()
{
	genrateReadSignal();
}
unsigned char * readDataInterface(unsigned char *buf, int len,unsigned int*srchostP,unsigned short *srcportP ,unsigned int*dsthostP,unsigned short *dstportP ,int *lenP)
{
	return readData(buf,len,srchostP,srcportP,dsthostP,dstportP,lenP);
}
void setTurnPathInterface(unsigned char *pathP)
{
	return setTurnPath(pathP);

}

