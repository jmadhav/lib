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
		void Start(GThreadDataLocal * glP);
		void GetValue(GThreadDataLocal *glP,GThreadData *gThreadP);
		

	#ifdef __cplusplus
		}
	#endif

#endif 

#define THREAD_PROC DWORD WINAPI
