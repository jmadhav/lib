/*
 devloped by mukesh sharma
 
 
 */
//#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "portaudio.h"
#include "pa_trace.h"
#include "pa_util.h"
#include "pa_allocation.h"
#include "pa_hostapi.h"
#include "pa_stream.h"
#include "pa_cpuload.h"
#include "pa_process.h"
#include "myaudio.h"
#define BUFF_SIZE 320
#define BUFFLOOP 3
#define NEWSZ BUFF_SIZE*BUFFLOOP

// =====  constants  =====

// =====  structs  =====
#pragma mark structs

// PaMacCoreHostApiRepresentation - host api datastructure specific to this implementation
typedef struct PaMacCore_HAR
	{
		PaUtilHostApiRepresentation inheritedHostApiRep;
		PaUtilStreamInterface callbackStreamInterface;
		PaUtilStreamInterface blockingStreamInterface;
		
		PaUtilAllocationGroup *allocations;
		AudioDeviceID *macCoreDeviceIds;
	}
	PaMacCoreHostApiRepresentation;

typedef struct PaMacCore_DI
	{
		PaDeviceInfo inheritedDeviceInfo;
	}
	PaMacCoreDeviceInfo;
#define AUDIO_BUFFERS 3
typedef struct AudioQueueStructType 
	{
		AudioQueueRef queue;
		int startB;
		void *clientDataP;
		AudioQueueBufferRef mBuffers[AUDIO_BUFFERS];
		AudioStreamBasicDescription mDataFormat;
	} AudioQueueStructType;
// PaMacCoreStream - a stream data structure specifically for this implementation
typedef struct PaMacCore_S
	{
		PaUtilStreamRepresentation streamRepresentation;
		PaUtilCpuLoadMeasurer cpuLoadMeasurer;
		PaUtilBufferProcessor bufferProcessor;
		
		int primeStreamUsingCallback;
		
		AudioDeviceID inputDevice;
		AudioDeviceID outputDevice;
		
		// Processing thread management --------------
		//    HANDLE abortEvent;
		//    HANDLE processingThread;
		//    DWORD processingThreadId;
		
		char throttleProcessingThreadOnOverload; // 0 -> don't throtte, non-0 -> throttle
		int processingThreadPriority;
		int highThreadPriority;
		int throttledThreadPriority;
		unsigned long throttledSleepMsecs;
		
		int isStopped;
		volatile int isActive;
		volatile int stopProcessing; // stop thread once existing buffers have been returned
		volatile int abortProcessing; // stop thread immediately
		AudioQueueStructType aqPlay;
		AudioQueueStructType aqRecord;
		void *clientDataP;
		
		
		//    DWORD allBuffersDurationMs; // used to calculate timeouts
	}
	PaMacCoreStream;

// Data needed by the CoreAudio callback functions
typedef struct PaMacCore_CD
	{
		PaMacCoreStream *stream;
		PaStreamCallback *callback;
		void *userData;
		PaUtilConverter *inputConverter;
		PaUtilConverter *outputConverter;
		void *inputBuffer;
		void *outputBuffer;
		int inputChannelCount;
		int outputChannelCount;
		PaSampleFormat inputSampleFormat;
		PaSampleFormat outputSampleFormat;
		PaUtilTriangularDitherGenerator *ditherGenerator;
	}
	PaMacClientData;




void SetAudioType(void *uData,int type)
{
	UInt32 sessionCategory = kAudioSessionCategory_PlayAndRecord;//kAudioSessionCategory_MediaPlayback
	
	
	switch(type)
	{
		case 1://play back
			sessionCategory = kAudioSessionCategory_MediaPlayback;
			break;
		case 2:// record
			sessionCategory = kAudioSessionCategory_RecordAudio;
			
			break;
		case 0: //play and record
			sessionCategory = kAudioSessionCategory_PlayAndRecord;
			break;
			
			
			
	}
	//this is added for iphone 3.0
	//AudioSessionInitialize(0,0,0,uData);
	//AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(sessionCategory), &sessionCategory);		//AudioQueueAddPropertyListener(aqcP->queue,kAudioQueueProperty_IsRunning,AudioQueuePropertyListenerFunction,aqcP);
}	




// =====  CoreAudio-PortAudio bridge functions =====
#pragma mark CoreAudio-PortAudio bridge functions

// Maps CoreAudio OSStatus codes to PortAudio PaError codes
static PaError conv_err(OSStatus error)
{
    PaError result;
    
    switch (error) {
        case kAudioHardwareNoError:
            result = paNoError; break;
        case kAudioHardwareNotRunningError:
            result = paInternalError; break;
        case kAudioHardwareUnspecifiedError:
            result = paInternalError; break;
        case kAudioHardwareUnknownPropertyError:
            result = paInternalError; break;
        case kAudioHardwareBadPropertySizeError:
            result = paInternalError; break;
        case kAudioHardwareIllegalOperationError:
            result = paInternalError; break;
        case kAudioHardwareBadDeviceError:
            result = paInvalidDevice; break;
        case kAudioHardwareBadStreamError:
            result = paBadStreamPtr; break;
        case kAudioHardwareUnsupportedOperationError:
            result = paInternalError; break;
        case kAudioDeviceUnsupportedFormatError:
            result = paSampleFormatNotSupported; break;
        case kAudioDevicePermissionsError:
            result = paDeviceUnavailable; break;
        default:
            result = paInternalError;
    }
    
    return result;
}
void * PaUtil_AllocateMemoryLocal(int sizeInt)
{
	void * lmemeoryP=0;
	lmemeoryP = PaUtil_AllocateMemory(sizeInt);
	memset(lmemeoryP, 0, sizeInt);
	return lmemeoryP;
	
}
/* This function is unused
 static AudioStreamBasicDescription *InitializeStreamDescription(const PaStreamParameters *parameters, double sampleRate)
 {
 struct AudioStreamBasicDescription *streamDescription = PaUtil_AllocateMemory(sizeof(AudioStreamBasicDescription));
 streamDescription->mSampleRate = sampleRate;
 streamDescription->mFormatID = kAudioFormatLinearPCM;
 streamDescription->mFormatFlags = 0;
 streamDescription->mFramesPerPacket = 1;
 
 if (parameters->sampleFormat & paNonInterleaved) {
 streamDescription->mFormatFlags |= kLinearPCMFormatFlagIsNonInterleaved;
 streamDescription->mChannelsPerFrame = 1;
 streamDescription->mBytesPerFrame = Pa_GetSampleSize(parameters->sampleFormat);
 streamDescription->mBytesPerPacket = Pa_GetSampleSize(parameters->sampleFormat);
 }
 else {
 streamDescription->mChannelsPerFrame = parameters->channelCount;
 }
 
 streamDescription->mBytesPerFrame = Pa_GetSampleSize(parameters->sampleFormat) * streamDescription->mChannelsPerFrame;
 streamDescription->mBytesPerPacket = streamDescription->mBytesPerFrame * streamDescription->mFramesPerPacket;
 
 if (parameters->sampleFormat & paFloat32) {
 streamDescription->mFormatFlags |= kLinearPCMFormatFlagIsFloat;
 streamDescription->mBitsPerChannel = 32;
 }
 else if (parameters->sampleFormat & paInt32) {
 streamDescription->mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
 streamDescription->mBitsPerChannel = 32;
 }
 else if (parameters->sampleFormat & paInt24) {
 streamDescription->mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
 streamDescription->mBitsPerChannel = 24;
 }
 else if (parameters->sampleFormat & paInt16) {
 streamDescription->mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
 streamDescription->mBitsPerChannel = 16;
 }
 else if (parameters->sampleFormat & paInt8) {
 streamDescription->mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
 streamDescription->mBitsPerChannel = 8;
 }    
 else if (parameters->sampleFormat & paInt32) {
 streamDescription->mBitsPerChannel = 8;
 }
 
 return streamDescription;
 }
 */


// =====  support functions  =====
#pragma mark support functions

static void CleanUp(PaMacCoreHostApiRepresentation *macCoreHostApi)
{
    if( macCoreHostApi->allocations )
    {
        PaUtil_FreeAllAllocations( macCoreHostApi->allocations );
        PaUtil_DestroyAllocationGroup( macCoreHostApi->allocations );
    }
    
    PaUtil_FreeMemory( macCoreHostApi );    
}

static PaError GetChannelInfo(PaDeviceInfo *deviceInfo, AudioDeviceID macCoreDeviceId, int isInput)
{
    UInt32 propSize;
    PaError err = paNoError;
    UInt32 i;
    int numChannels = 1;
    AudioBufferList *buflist;
	if (isInput)
		deviceInfo->maxInputChannels = numChannels;
	else
		deviceInfo->maxOutputChannels = numChannels;
	
    return err;
}

static PaError InitializeDeviceInfo(PaMacCoreDeviceInfo *macCoreDeviceInfo,  AudioDeviceID macCoreDeviceId, PaHostApiIndex hostApiIndex )
{
    PaDeviceInfo *deviceInfo = &macCoreDeviceInfo->inheritedDeviceInfo;
    deviceInfo->structVersion = 2;
    deviceInfo->hostApi = hostApiIndex;
    
    PaError err = paNoError;
    UInt32 propSize=100;
	// FIXME: this allocation should be part of the allocations group
    char *name = PaUtil_AllocateMemoryLocal(propSize);
	
	//sprintf(name,"iphoneipod%d",(int)hostApiIndex);
	strcpy(name,"spokniphone");
	deviceInfo->name = name;
	Float64 sampleRate = 8000;
    propSize = sizeof(Float64);
	err = 0;
	// err = conv_err(AudioDeviceGetProperty(macCoreDeviceId, 0, 0, kAudioDevicePropertyNominalSampleRate, &propSize, &sampleRate));
    if (!err) {
        deviceInfo->defaultSampleRate = sampleRate;
    }
	// Get channel info
    err = GetChannelInfo(deviceInfo, macCoreDeviceId, 1);
    err = GetChannelInfo(deviceInfo, macCoreDeviceId, 0);
	
    return err;
}

static PaError InitializeDeviceInfos( PaMacCoreHostApiRepresentation *macCoreHostApi, PaHostApiIndex hostApiIndex )
{
    PaError result = paNoError;
    PaUtilHostApiRepresentation *hostApi;
    PaMacCoreDeviceInfo *deviceInfoArray;
	
    // initialise device counts and default devices under the assumption that there are no devices. These values are incremented below if and when devices are successfully initialized.
    hostApi = &macCoreHostApi->inheritedHostApiRep;
    hostApi->info.deviceCount = 0;
    hostApi->info.defaultInputDevice = paNoDevice;
    hostApi->info.defaultOutputDevice = paNoDevice;
	UInt32 propsize=0;
	
    int numDevices = 1;
	
	
	hostApi->info.deviceCount = numDevices;
    if (numDevices > 0) {
        hostApi->deviceInfos = (PaDeviceInfo**)PaUtil_GroupAllocateMemory(
																		  macCoreHostApi->allocations, sizeof(PaDeviceInfo*) * numDevices );
        if( !hostApi->deviceInfos )
        {
            return paInsufficientMemory;
        }
        // allocate all device info structs in a contiguous block
        deviceInfoArray = (PaMacCoreDeviceInfo*)PaUtil_GroupAllocateMemory(
																		   macCoreHostApi->allocations, sizeof(PaMacCoreDeviceInfo) * numDevices );
        if( !deviceInfoArray )
        {
            return paInsufficientMemory;
        }
		
        macCoreHostApi->macCoreDeviceIds = PaUtil_GroupAllocateMemory(macCoreHostApi->allocations, propsize);
		
        AudioDeviceID defaultInputDevice, defaultOutputDevice;
        propsize = sizeof(AudioDeviceID);
		
		
		UInt32 i;
		
		for (i = 0; i < numDevices; ++i) {
            macCoreHostApi->macCoreDeviceIds[i] = i+1;
			
			InitializeDeviceInfo(&deviceInfoArray[i], macCoreHostApi->macCoreDeviceIds[i], hostApiIndex);
            hostApi->deviceInfos[i] = &(deviceInfoArray[i].inheritedDeviceInfo);      
        }
		
		hostApi->info.defaultOutputDevice = 0;
		hostApi->info.defaultInputDevice = 0;
		
    }
	
    return result;
}

static OSStatus CheckFormat(AudioDeviceID macCoreDeviceId, const PaStreamParameters *parameters, double sampleRate, int isInput)
{
    UInt32 propSize = sizeof(AudioStreamBasicDescription);
    AudioStreamBasicDescription *streamDescription = PaUtil_AllocateMemoryLocal(propSize);
	
    streamDescription->mSampleRate = sampleRate;
    streamDescription->mFormatID = 0;
    streamDescription->mFormatFlags = 0;
    streamDescription->mBytesPerPacket = 0;
    streamDescription->mFramesPerPacket = 0;
    streamDescription->mBytesPerFrame = 0;
    streamDescription->mChannelsPerFrame = 0;
    streamDescription->mBitsPerChannel = 0;
    streamDescription->mReserved = 0;
	
    OSStatus result = 0;// AudioDeviceGetProperty(macCoreDeviceId, 0, isInput, kAudioDevicePropertyStreamFormatSupported, &propSize, streamDescription);
    PaUtil_FreeMemory(streamDescription);
    return result;
}

static OSStatus CopyInputData(PaMacClientData* destination, const AudioBufferList *source, unsigned long frameCount)
{
    int frameSpacing, channelSpacing;
    if (destination->inputSampleFormat & paNonInterleaved) {
        frameSpacing = 1;
        channelSpacing = destination->inputChannelCount;
    }
    else {
        frameSpacing = destination->inputChannelCount;
        channelSpacing = 1;
    }
    
    AudioBuffer const *inputBuffer = &source->mBuffers[0];
    void *coreAudioBuffer = inputBuffer->mData;
    void *portAudioBuffer = destination->inputBuffer;
    UInt32 i, streamNumber, streamChannel;
    for (i = streamNumber = streamChannel = 0; i < destination->inputChannelCount; ++i, ++streamChannel) {
        if (streamChannel >= inputBuffer->mNumberChannels) {
            ++streamNumber;
            inputBuffer = &source->mBuffers[streamNumber];
            coreAudioBuffer = inputBuffer->mData;
            streamChannel = 0;
        }
        destination->inputConverter(portAudioBuffer, frameSpacing, coreAudioBuffer, inputBuffer->mNumberChannels, frameCount, destination->ditherGenerator);
        coreAudioBuffer += sizeof(Float32);
        portAudioBuffer += Pa_GetSampleSize(destination->inputSampleFormat) * channelSpacing;
    }
    return noErr;
}

static OSStatus CopyOutputData(AudioBufferList* destination, PaMacClientData *source, unsigned long frameCount)
{
    int frameSpacing, channelSpacing;
    if (source->outputSampleFormat & paNonInterleaved) {
        frameSpacing = 1;
        channelSpacing = source->outputChannelCount;
    }
    else {
        frameSpacing = source->outputChannelCount;
        channelSpacing = 1;
    }
	
    AudioBuffer *outputBuffer = &destination->mBuffers[0];
    void *coreAudioBuffer = outputBuffer->mData;
    void *portAudioBuffer = source->outputBuffer;
    UInt32 i, streamNumber, streamChannel;
    for (i = streamNumber = streamChannel = 0; i < source->outputChannelCount; ++i, ++streamChannel) {
        if (streamChannel >= outputBuffer->mNumberChannels) {
            ++streamNumber;
            outputBuffer = &destination->mBuffers[streamNumber];
            coreAudioBuffer = outputBuffer->mData;
            streamChannel = 0;
        }
        source->outputConverter(coreAudioBuffer, outputBuffer->mNumberChannels, portAudioBuffer, frameSpacing, frameCount, NULL);
        coreAudioBuffer += sizeof(Float32);
        portAudioBuffer += Pa_GetSampleSize(source->outputSampleFormat) * channelSpacing;
    }
    return noErr;
}

#include "stdio.h"


static void playCallback(
						 void *userData,
						 AudioQueueRef outQ,
						 AudioQueueBufferRef outQB)
{
	if(outQB)
	{	
		memset(outQB->mAudioData,0,NEWSZ);
	}
	if(userData)
	{	
		int i;
		AudioBufferList outOutputData;
		PaMacClientData *clientData = (PaMacClientData *)userData;
		outOutputData.mBuffers[0].mNumberChannels = 1;
		outOutputData.mBuffers[0].mDataByteSize = outQB->mAudioDataByteSize;
		outOutputData.mBuffers[0].mData = outQB->mAudioData;
		
		//	printf("\nplay");
		AudioBuffer *outputBuffer = &outOutputData.mBuffers[0];
		unsigned long frameCount = 160;
		
		//added by mukesh for 2g device
		for(i=0;i<BUFFLOOP;++i)
		{	
			outOutputData.mBuffers[0].mNumberChannels = 1;
			outOutputData.mBuffers[0].mDataByteSize = BUFF_SIZE;
			outOutputData.mBuffers[0].mData = outQB->mAudioData + i*BUFF_SIZE;
			
			clientData->callback(NULL, outQB->mAudioData+ i*BUFF_SIZE, frameCount, 0, paNoFlag, clientData->userData);			
			
			
		}	
		
		
		
		
		outputBuffer->mDataByteSize =NEWSZ;
		
    }
	if(outQB)
	{	
		outQB->mAudioDataByteSize = NEWSZ;
		AudioQueueEnqueueBuffer(outQ, outQB, 0, NULL);
	}
	
	return ;
	
	
	
}

static void recordCallback (
							void                                *userData,
							AudioQueueRef                       inQ,
							AudioQueueBufferRef                 inQB,
							const AudioTimeStamp                *inStartTime,
							UInt32                              inNumPackets,
							const AudioStreamPacketDescription  *inPacketDesc)
{
   	
	
	
	
	
	
	
	AudioBufferList inInputData;
	
	inInputData.mBuffers[0].mNumberChannels = 1;
	inInputData.mBuffers[0].mDataByteSize = inQB->mAudioDataByteSize;
	inInputData.mBuffers[0].mData = inQB->mAudioData;
	
	PaMacClientData *clientData = (PaMacClientData *)userData;
	//printf("\n			record");
	
	AudioBuffer const *inputBuffer = &inInputData.mBuffers[0];
	unsigned long frameCount = inQB->mAudioDataByteSize / 2;//(inputBuffer->mNumberChannels * sizeof(Float32));
	PaStreamCallbackResult result = clientData->callback(inQB->mAudioData, 0, frameCount, 0, paNoFlag, clientData->userData);
  	
	AudioQueueEnqueueBuffer(/*pAqData->mQueue*/inQ, inQB, 0, NULL);
    return ;
	
	
	
	
	
}

static PaError SetSampleRate(AudioDeviceID device, double sampleRate, int isInput)
{
    PaError result = paNoError;
    
	
    return result;    
}

static PaError SetFramesPerBuffer(AudioDeviceID device, unsigned long framesPerBuffer, int isInput)
{
    PaError result = paNoError;
	
    return result;    
}

static PaError SetUpUnidirectionalStream(AudioDeviceID device, double sampleRate, unsigned long framesPerBuffer, int isInput)
{
    PaError err = paNoError;
	return err;
}

// =====  PortAudio functions  =====
#pragma mark PortAudio functions

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
    
    PaError PaMacCore_Initialize( PaUtilHostApiRepresentation **hostApi, PaHostApiIndex index );
    
#ifdef __cplusplus
}
#endif // __cplusplus

static void Terminate( struct PaUtilHostApiRepresentation *hostApi )
{
    PaMacCoreHostApiRepresentation *macCoreHostApi = (PaMacCoreHostApiRepresentation*)hostApi;
    CleanUp(macCoreHostApi);
}

static PaError IsFormatSupported( struct PaUtilHostApiRepresentation *hostApi,
								 const PaStreamParameters *inputParameters,
								 const PaStreamParameters *outputParameters,
								 double sampleRate )
{
    PaMacCoreHostApiRepresentation *macCoreHostApi = (PaMacCoreHostApiRepresentation*)hostApi;
    PaDeviceInfo *deviceInfo;
    
    PaError result = paNoError;
    if (inputParameters) {
        deviceInfo = macCoreHostApi->inheritedHostApiRep.deviceInfos[inputParameters->device];
        if (inputParameters->channelCount > deviceInfo->maxInputChannels)
            result = paInvalidChannelCount;
        else if (CheckFormat(macCoreHostApi->macCoreDeviceIds[inputParameters->device], inputParameters, sampleRate, 1) != kAudioHardwareNoError) {
            result = paInvalidSampleRate;
        }
    }
    if (outputParameters && result == paNoError) {
        deviceInfo = macCoreHostApi->inheritedHostApiRep.deviceInfos[outputParameters->device];
        if (outputParameters->channelCount > deviceInfo->maxOutputChannels)
            result = paInvalidChannelCount;
        else if (CheckFormat(macCoreHostApi->macCoreDeviceIds[outputParameters->device], outputParameters, sampleRate, 0) != kAudioHardwareNoError) {
            result = paInvalidSampleRate;
        }
    }
	
    return result;
}

static PaError OpenStream( struct PaUtilHostApiRepresentation *hostApi,
						  PaStream** s,
						  const PaStreamParameters *inputParameters,
						  const PaStreamParameters *outputParameters,
						  double sampleRate,
						  unsigned long framesPerBuffer,
						  PaStreamFlags streamFlags,
						  PaStreamCallback *streamCallback,
						  void *userData )
{
    
	
	
	PaError err = paNoError;
	int i=0;
    PaMacCoreHostApiRepresentation *macCoreHostApi = (PaMacCoreHostApiRepresentation *)hostApi;
    PaMacCoreStream *stream = PaUtil_AllocateMemoryLocal(sizeof(PaMacCoreStream));
    stream->isActive = 0;
    stream->isStopped = 1;
    stream->inputDevice = kAudioDeviceUnknown;
    stream->outputDevice = kAudioDeviceUnknown;
    
    PaUtil_InitializeStreamRepresentation( &stream->streamRepresentation,
										  ( (streamCallback)
										   ? &macCoreHostApi->callbackStreamInterface
										   : &macCoreHostApi->blockingStreamInterface ),
										  streamCallback, userData );
    PaUtil_InitializeCpuLoadMeasurer( &stream->cpuLoadMeasurer, sampleRate );
	//  sampleRate = 8000;
    *s = (PaStream*)stream;
    PaMacClientData *clientData = PaUtil_AllocateMemoryLocal(sizeof(PaMacClientData));
	stream->clientDataP = clientData;///store client data
    clientData->stream = stream;
    clientData->callback = streamCallback;
    clientData->userData = userData;
    clientData->inputBuffer = 0;
    clientData->outputBuffer = 0;
    clientData->ditherGenerator = PaUtil_AllocateMemoryLocal(sizeof(PaUtilTriangularDitherGenerator));
    PaUtil_InitializeTriangularDitherState(clientData->ditherGenerator);
    
    if (inputParameters != NULL) {
        stream->inputDevice = macCoreHostApi->macCoreDeviceIds[inputParameters->device];
        clientData->inputConverter = PaUtil_SelectConverter(paFloat32, inputParameters->sampleFormat, streamFlags);
        clientData->inputBuffer = PaUtil_AllocateMemoryLocal(Pa_GetSampleSize(inputParameters->sampleFormat) * framesPerBuffer * inputParameters->channelCount);
        clientData->inputChannelCount = inputParameters->channelCount;
        clientData->inputSampleFormat = inputParameters->sampleFormat;
        err = SetUpUnidirectionalStream(stream->inputDevice, sampleRate, framesPerBuffer, 1);
		err = 0;
		AudioQueueStructType *aqcP;
		aqcP = &stream->aqRecord;
		aqcP->clientDataP = clientData;
		aqcP->mDataFormat.mSampleRate = 8000;
		aqcP->mDataFormat.mFormatID = kAudioFormatLinearPCM;
		aqcP->mDataFormat.mFormatFlags =
		kLinearPCMFormatFlagIsSignedInteger
		| kAudioFormatFlagIsPacked;
		aqcP->mDataFormat.mBytesPerPacket = 2;
		aqcP->mDataFormat.mFramesPerPacket = 1;
		aqcP->mDataFormat.mBytesPerFrame = 2;
		aqcP->mDataFormat.mChannelsPerFrame = 1;
		aqcP->mDataFormat.mBitsPerChannel = 16;
		aqcP->mDataFormat.mFramesPerPacket = 1;
		aqcP->mDataFormat.mBytesPerFrame = 2;
		aqcP->queue = 0;
		for (i=0; i<AUDIO_BUFFERS; i++) 
        {
            
			aqcP->mBuffers[i] = 0;
		}	
		
    }
    
    if (err == paNoError && outputParameters != NULL) {
		AudioQueueStructType *aqcP;
		aqcP = &stream->aqPlay;
		aqcP->clientDataP = clientData;
		aqcP->mDataFormat.mSampleRate = sampleRate;
		aqcP->mDataFormat.mFormatID = kAudioFormatLinearPCM;
		aqcP->mDataFormat.mFormatFlags =
		kLinearPCMFormatFlagIsSignedInteger
		| kAudioFormatFlagIsPacked;
		aqcP->mDataFormat.mBytesPerPacket = 2;
		aqcP->mDataFormat.mFramesPerPacket = 1;
		aqcP->mDataFormat.mBytesPerFrame = 2;
		aqcP->mDataFormat.mChannelsPerFrame = 1;
		aqcP->mDataFormat.mBitsPerChannel = 16;
		aqcP->mDataFormat.mFramesPerPacket = 1;
		aqcP->mDataFormat.mBytesPerFrame = 2;
		aqcP->queue = 0;
		for (i=0; i<AUDIO_BUFFERS; i++) 
        {
            
			aqcP->mBuffers[i] = 0;
		}	
		
		
		stream->outputDevice = macCoreHostApi->macCoreDeviceIds[outputParameters->device];
        clientData->outputConverter = PaUtil_SelectConverter(outputParameters->sampleFormat, paFloat32, streamFlags);
        clientData->outputBuffer = PaUtil_AllocateMemoryLocal(Pa_GetSampleSize(outputParameters->sampleFormat) * framesPerBuffer * outputParameters->channelCount);
        clientData->outputChannelCount = outputParameters->channelCount;
        clientData->outputSampleFormat = outputParameters->sampleFormat;
        err = SetUpUnidirectionalStream(stream->outputDevice, sampleRate, framesPerBuffer, 0);
		err = 0;
		
		
		
    }
	
	SetAudioType(0,0);
	return err;
}

// Note: When CloseStream() is called, the multi-api layer ensures that the stream has already been stopped or aborted.
static PaError CloseStream( PaStream* s )
{
    PaError err = paNoError;
	int i;
    PaMacCoreStream *stream = (PaMacCoreStream*)s;
	PaMacClientData *clientP;
	
	if(stream==0)
	{
		return 1;
	}
	//printf("\stop");
    PaUtil_ResetCpuLoadMeasurer( &stream->cpuLoadMeasurer );
	AudioQueueStructType *aqcP;
	aqcP = &stream->aqPlay;
	if(aqcP->queue)
	{	
		AudioQueueStop (aqcP->queue, true);
		for (i=0; i<AUDIO_BUFFERS; i++) 
		{
			if(aqcP->mBuffers[i])
			{	
				AudioQueueFreeBuffer(aqcP->queue,aqcP->mBuffers[i]);
				aqcP->mBuffers[i] = 0;
			}
		} 
		AudioQueueDispose (aqcP->queue, true);
		aqcP->queue = 0;
	}
	aqcP = &stream->aqRecord;
	if(aqcP->queue)
	{	
		AudioQueueStop (aqcP->queue, true);
		for (i=0; i<AUDIO_BUFFERS; i++) 
		{
			if(aqcP->mBuffers[i])
			{	
				AudioQueueFreeBuffer(aqcP->queue,aqcP->mBuffers[i]);
				aqcP->mBuffers[i] = 0;
			}
			
		} 
		AudioQueueDispose (aqcP->queue, true);
		aqcP->queue = 0;
	}
	PaUtil_TerminateStreamRepresentation( &stream->streamRepresentation );
	
	clientP = stream->clientDataP;
	if(clientP)
	{
		PaUtil_FreeMemory(clientP->ditherGenerator);
		PaUtil_FreeMemory(clientP->outputBuffer);
		PaUtil_FreeMemory(clientP->inputBuffer);
		
		PaUtil_FreeMemory(clientP);
	}
	PaUtil_FreeMemory( stream );
	
	
	return err;
}
//#define _THREAD_
static PaError StartStream( PaStream *s )
{
    PaError err = paNoError;
	OSStatus status;
	
    PaMacCoreStream *stream = (PaMacCoreStream*)s;
	
    if (stream->inputDevice != kAudioDeviceUnknown) {
		int i;
		/* if (stream->outputDevice == kAudioDeviceUnknown || stream->outputDevice == stream->inputDevice) {
		 err = conv_err(AudioDeviceStart(stream->inputDevice, AudioIOProc));
		 }
		 else {
		 err = conv_err(AudioDeviceStart(stream->inputDevice, AudioInputProc));
		 err = conv_err(AudioDeviceStart(stream->outputDevice, AudioOutputProc));
		 }*/
		
		
		AudioQueueStructType *aqcP;
		aqcP = &stream->aqPlay;
		//aqcP->frameCount = 3200;
		status = AudioQueueNewOutput(&(aqcP->mDataFormat),
									 playCallback,
									 aqcP->clientDataP,
									 NULL,
									 0,
									 0,
									 &aqcP->queue);
        if (status)
        {
			return 1; // FIXME
        }
		
        UInt32 bufferBytes = NEWSZ;
		//stream->bits_per_sample / 8 * aq->mDataFormat.mBytesPerFrame;
        for (i=0; i<AUDIO_BUFFERS; i++) 
        {
            
			aqcP->mBuffers[i] = 0;
			status = AudioQueueAllocateBuffer(aqcP->queue, bufferBytes, 
											  &(aqcP->mBuffers[i]));
			if(aqcP->mBuffers[i]==0 ||status)
			{
				//AudioQueueDispose (aqcP->queue, true);
				//printf("\n buff not created out %ld",(long)status);
				aqcP->mBuffers[i] = 0;
				return 1; // FIXME
			}
            playCallback (0, aqcP->queue, aqcP->mBuffers[i]);
			
        }
        status = AudioQueueStart(aqcP->queue, NULL); 
		aqcP->startB = true;
		aqcP = &stream->aqRecord;
		
		//pthread_t pt; pthread_create(&pt, 0,profileDownload,0);
		
		
		status = AudioQueueNewInput (&(aqcP->mDataFormat),
									 recordCallback,
									 aqcP->clientDataP,
									 0,
									 0,
									 0,
									 &(aqcP->queue));
        if (status)
        {
            return 220001; // FIXME
        }
        
		//  UInt32 bufferBytes;
        bufferBytes = BUFF_SIZE;
		//	bufferBytes = 640;
		for (i = 0; i < AUDIO_BUFFERS; ++i) 
        {
            aqcP->mBuffers[i] = 0;
			status = AudioQueueAllocateBuffer (aqcP->queue, bufferBytes,
											   &(aqcP->mBuffers[i]));
			
            if (status)          
            {
                //PJ_LOG(1, (THIS_FILE, 
				//	   "AudioQueueAllocateBuffer[%d] err %d\n",i, status));
                // return PJMEDIA_ERROR; // FIXME return ???     
				aqcP->mBuffers[i] = 0;
				return 1;
            }
            AudioQueueEnqueueBuffer (aqcP->queue, aqcP->mBuffers[i], 0, NULL);
        }
        
        // FIXME : END
        
        status = AudioQueueStart (aqcP->queue, NULL);
        if (status)
        {
            return 220200;   
        }
        aqcP->startB = true;
        UInt32 level = 1;
		//     status = AudioQueueSetProperty(aqcP->queue, 
		//								   kAudioQueueProperty_EnableLevelMetering, &level, sizeof(level));
        if (status)
        {
			//   PJ_LOG(1, (THIS_FILE, "AudioQueueSetProperty err %d", status));
        }	
		
		
		
		
		
		
		
		
		
    }
	
    stream->isActive = 1;
    stream->isStopped = 0;
    return 0;
}

static PaError AbortStream( PaStream *s )
{
    PaError err = paNoError;
    PaMacCoreStream *stream = (PaMacCoreStream*)s;
	
	if(stream)
	{	
		if(stream->aqRecord.startB)
		{	
			if(stream->aqRecord.queue)
				AudioQueuePause (stream->aqRecord.queue);
		}
		if(stream->aqPlay.startB)
		{
			if(stream->aqPlay.queue)
				AudioQueuePause (stream->aqPlay.queue);
		}
		stream->aqRecord.startB = 0;
		stream->aqPlay.startB = 0;
    }
	return err;
}    

static PaError StopStream( PaStream *s )
{
    // TODO: this should be nicer than abort
    return AbortStream(s);
}

static PaError IsStreamStopped( PaStream *s )
{
    PaMacCoreStream *stream = (PaMacCoreStream*)s;
    
    return stream->isStopped;
}


static PaError IsStreamActive( PaStream *s )
{
    PaMacCoreStream *stream = (PaMacCoreStream*)s;
	
    return stream->isActive;
}


static PaTime GetStreamTime( PaStream *s )
{
    OSStatus err;
    PaTime result=0;
	
    return result;
}


static double GetStreamCpuLoad( PaStream* s )
{
    PaMacCoreStream *stream = (PaMacCoreStream*)s;
    
    return PaUtil_GetCpuLoad( &stream->cpuLoadMeasurer );
}


// As separate stream interfaces are used for blocking and callback streams, the following functions can be guaranteed to only be called for blocking streams.

static PaError ReadStream( PaStream* s,
						  void *buffer,
						  unsigned long frames )
{
    return paInternalError;
}


static PaError WriteStream( PaStream* s,
						   const void *buffer,
						   unsigned long frames )
{
    return paInternalError;
}


static signed long GetStreamReadAvailable( PaStream* s )
{
    return paInternalError;
}


static signed long GetStreamWriteAvailable( PaStream* s )
{
    return paInternalError;
}

// HostAPI-specific initialization function
PaError PaMacCore_Initialize( PaUtilHostApiRepresentation **hostApi, PaHostApiIndex hostApiIndex )
{
    PaError result = paNoError;
    PaMacCoreHostApiRepresentation *macCoreHostApi = (PaMacCoreHostApiRepresentation *)PaUtil_AllocateMemoryLocal( sizeof(PaMacCoreHostApiRepresentation) );
    if( !macCoreHostApi )
    {
        result = paInsufficientMemory;
        goto error;
    }
	
    macCoreHostApi->allocations = PaUtil_CreateAllocationGroup();
    if( !macCoreHostApi->allocations )
    {
        result = paInsufficientMemory;
        goto error;
    }
	
    *hostApi = &macCoreHostApi->inheritedHostApiRep;
    (*hostApi)->info.structVersion = 1;
    (*hostApi)->info.type = paCoreAudio;
    (*hostApi)->info.name = "iphoneAudio";
	
    result = InitializeDeviceInfos(macCoreHostApi, hostApiIndex);
    if (result != paNoError) {
        goto error;
    }
	
    // Set up the proper callbacks to this HostApi's functions
    (*hostApi)->Terminate = Terminate;
    (*hostApi)->OpenStream = OpenStream;
    (*hostApi)->IsFormatSupported = IsFormatSupported;
    
    PaUtil_InitializeStreamInterface( &macCoreHostApi->callbackStreamInterface, CloseStream, StartStream,
									 StopStream, AbortStream, IsStreamStopped, IsStreamActive,
									 GetStreamTime, GetStreamCpuLoad,
									 PaUtil_DummyRead, PaUtil_DummyWrite,
									 PaUtil_DummyGetReadAvailable, PaUtil_DummyGetWriteAvailable );
    
    PaUtil_InitializeStreamInterface( &macCoreHostApi->blockingStreamInterface, CloseStream, StartStream,
									 StopStream, AbortStream, IsStreamStopped, IsStreamActive,
									 GetStreamTime, PaUtil_DummyGetCpuLoad,
									 ReadStream, WriteStream, GetStreamReadAvailable, GetStreamWriteAvailable );
	
    return paNoError;
    
error:
	if( macCoreHostApi ) {
		CleanUp(macCoreHostApi);
	}
	
    return result;
}

#pragma mark NEWAPI
