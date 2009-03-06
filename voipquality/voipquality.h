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
	typedef struct GThreadDataLocal
	{
		GThreadData gThread;
		Int16 threadStartInt;
	}GThreadDataLocal;	

	typedef enum SelectType
    {
	 WriteEnum=0,
	 ReadEnum = 1
    }SelectType;

	#ifdef __cplusplus
			extern "C" {
		#endif
		void Start();
		void GetValue(GThreadData *gThreadP);

	#ifdef __cplusplus
		}
	#endif

#endif 

#define THREAD_PROC DWORD WINAPI
