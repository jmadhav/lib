/**
 Copyright (c) 2009 Geodesic Limited <http://www.geodesic.com>.
 
 This file is part of Spokn software.
 
 Spokn is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 Spokn is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with Spokn.  If not, see <http://www.gnu.org/licenses/>.
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