/*
 *  sipandltpwrapper.h
 *  spokn
 *
 *  Created by Mukesh Sharma on 25/03/10.
 *  Copyright 2010 Geodesic Ltd.. All rights reserved.
 *
 */

#ifndef _SIP_AND_LTP_WRAPPER_H
	#define _SIP_AND_LTP_WRAPPER_H
#include "ltpmobile.h"
int sip_spokn_pj_init(char *errorstring);
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



#endif