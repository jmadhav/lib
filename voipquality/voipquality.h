/**
    Copyright (c) 2009 Geodesic Limited <http://www.geodesic.com>.

    This file is part of Spokn software for Windows.

    Spokn for Windows is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Spokn for Windows is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Spokn for Windows.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "gigabooth_config.h"
#ifndef _VOIP_QUALITY_H_
#define _VOIP_QUALITY_H_

    //structure which will be accessed from outside
	typedef struct GThreadData
	{
		float RunningAvg;
		Int16 MatchedPackets;
	}GThreadData;

	//local structure
	typedef BOOL (*VoipIndicatorCallBack)(int reasonCallInt,long uData,GThreadData *GThreadDataP,int error);
	typedef struct GThreadDataLocal
	{
		GThreadData gThread;
		long uDataLong;
		VoipIndicatorCallBack voipIndCallbackP;
		Int16 threadStartInt;
	}GThreadDataLocal;	

	typedef enum SelectType
    {
	 WriteEnum = 0,
	 ReadEnum = 1
    }SelectType;

	typedef enum ReasonCall
	{
		Success = 0,
		Error = 1,
		InProcess = 2
	}ReasonCall;

	#ifdef __cplusplus
			extern "C" {
		#endif
		GThreadDataLocal * VoipQualityInit(VoipIndicatorCallBack voipIndCallbackP,long uDataLong);
		void VoipQualityDeInit(GThreadDataLocal **glP);
		void VoipStart(GThreadDataLocal * glP);
		void VoipGetValue(GThreadDataLocal *glP,GThreadData *gThreadP);
		

	#ifdef __cplusplus
		}
	#endif

#endif 

#define THREAD_PROC DWORD WINAPI
