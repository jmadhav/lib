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
#include "stdafx.h"
#include "VoipQuality.h"
#include <winsock.h>
#include "stdio.h"
#include "spokn_config.h"

#define DEFAULT_BUFLEN 100
#define TOTALPACKETS 50
#define PADDING "  packet:"
#define SERVERIPADDRESS "122.170.123.15"
#define SERVERNAME "speak.mumbai.geodesic.com"
#define PORT 7


//static GThreadDataLocal gData;

// ameya waingankar
// 24/2/2009
// to ser transport address structure
int SetSckAdd(Char *Servername, UInt16 port,struct  sockaddr_in *sockAddrP)
{
	
	DWORD error;
	struct hostent *pHostEnt;
	char hostname[35];
	if(sockAddrP==0)
	{
		return 1;
	}
	memset(sockAddrP,0,sizeof(struct  sockaddr_in));
	strcpy(hostname,Servername);
	pHostEnt = gethostbyname(hostname);
	if(!pHostEnt)
	{
		error = GetLastError();
		return error;
	}
    sockAddrP->sin_port = htons(port);
    sockAddrP->sin_family = AF_INET;
	sockAddrP->sin_addr = *(struct in_addr *)(pHostEnt->h_addr_list[0]);
	return 0;
}

// ameya waingankar
// 24/2/2009
// to check if there is any data to recieve
int Data_Recieve(SOCKET soc,int timelong, SelectType selectEnum )
{
	fd_set  fdread;
	int     ret;
	struct timeval timeout={0};
	if(timelong<1200)
	{
		timelong = 1200;
	}
	timeout.tv_usec = timelong*100;
	timeout.tv_sec = timelong/100;//give in second
// Create a socket, and accept a connection

// Manage I/O on the socket

    // Always clear the read set before calling 
    // select()
    FD_ZERO(&fdread);

    // Add socket s to the read set
    FD_SET(soc, &fdread);

	if(selectEnum ==WriteEnum)  
	{
		if ((ret = select(0, NULL, &fdread, NULL, &timeout))   == SOCKET_ERROR) 
		{
			return 1;//main error
			// Error condition
		}
	}
	else
	{
		if ((ret = select(0, &fdread, NULL, NULL, &timeout)) == SOCKET_ERROR) 
		{
			return 1;//main error
        // Error condition
		}
	}
	
	if(ret==0) return 2;
	return 0;
	/*
    if (ret > 0)
    {
        // For this simple case, select() should return
        // the value 1. An application dealing with 
        // more than one socket could get a value 
        // greater than 1. At this point, your 
        // application should check to see whether the 
        // socket is part of a set.

        if (FD_ISSET(soc, &fdread))
        {
            // A read event has occurred on socket s
			return 0;//mean read
        }
    }

	return 1;//mean continue
*/
}

// ameya waingankar
// 24/2/2009
// to send data to the server
void SendPackets(SOCKET sck,struct  sockaddr_in  SockAddr,Char *Servername, UInt16 port)
{
	//DWORD error;
	//struct  sockaddr_in  SockAddr;
	//struct hostent *pHostEnt;
	//char hostname[35];
	//strcpy(hostname,Servername);
	//pHostEnt = gethostbyname(hostname);
	//if(pHostEnt == NULL)
	//{
	//	error = GetLastError();
	//}
 //   SockAddr.sin_port = htons(port);
 //   SockAddr.sin_family = AF_INET;
	//SockAddr.sin_addr = *(struct in_addr *)(pHostEnt->h_addr_list[0]);
	////SockAddr.sin_addr.s_addr = inet_addr(SERVERIPADDRESS);

	char lData1[DEFAULT_BUFLEN]= " 1 " ;
	
	UInt32 x = 1;
	Char *tempP;
	Int16 len = 0;

	for(x=1; x <= TOTALPACKETS; ++x)
	{
		memmove(lData1,(void*)&x,sizeof(UInt32));
		tempP = lData1 +sizeof(UInt32);
        strcpy(tempP,PADDING);
		len = sendto(sck,lData1,DEFAULT_BUFLEN,0,(struct sockaddr *)&SockAddr,sizeof(struct sockaddr_in));
		::Sleep(10);
	}
	
}

// ameya waingankar
// 24/2/2009
// to recieve data from the server
int recieve(SOCKET sck, Char *buffer,struct  sockaddr_in  SockAddr,Char *Servername, UInt16 port)
{
	//DWORD error;
	//struct  sockaddr_in  SockAddr;
	//struct hostent *pHostEnt;
	//char hostname[35];
	//strcpy(hostname,Servername);
	//pHostEnt = gethostbyname(hostname);
	//if(pHostEnt == NULL)
	//{
	//	error = GetLastError();
	//}
 //   SockAddr.sin_port = htons(port);
 //   SockAddr.sin_family = AF_INET;
	//SockAddr.sin_addr = *(struct in_addr *)(pHostEnt->h_addr_list[0]);
	////SockAddr.sin_addr.s_addr = inet_addr(SERVERIPADDRESS);
    int SenderAddrSize = sizeof(struct sockaddr_in);
    
	int timelong = 0;
	Char recvbuf[DEFAULT_BUFLEN];
    Int16 len = 0;
	Int16 er;
	er = Data_Recieve(sck,timelong,ReadEnum); 
    if( er == 0)
	{
	    len = recvfrom(sck, recvbuf, DEFAULT_BUFLEN, 0,(struct sockaddr *)&SockAddr, &SenderAddrSize);
	    if(len > 0)
	    {
		    memmove(buffer,recvbuf,len);
			return 0;
	    }
	    else
	    {
		    return 1;
	    }
	}
	else
	{
		return er;
	}

	return 0;
}

// ameya waingankar
// 24/2/2009
// to create a UDPsocket
SOCKET CreateSocket()
{
	DWORD error;
	SOCKET sock;
	WSADATA wsaData;

	if(WSAStartup(MAKEWORD(2,0),&wsaData) != 0)
	{
		error = GetLastError();
	}

    if ( (sock = socket(AF_INET, SOCK_DGRAM , IPPROTO_UDP)) == INVALID_SOCKET )
	{
		error = GetLastError();
	}
	else
	{
	    return sock;
	}

	return sock;
}	

// ameya waingankar
// 24/2/2009
// to check ideal server response
Int16 EvalResponse(int x, int count)
{
	Int16 increment = 0;
	//check for correct response

	if(x == count)
	{
		increment = 1;
	}
   
	return increment;
}

// ameya waingankar
// 24/2/2009
// to send packets to echo server and recieve the same to check the voip quality
THREAD_PROC SendRecieve(void *extras)
{
	Int16 count = 0;
	Int16 matched = 0;
	Int16 result = 0;
	UInt32 x = 0;
	Int16 err = 0;
	Int16 quality = 0;
	Int16 correctRespose = 0;
	int er;
	BOOL breakLoopB = TRUE ;
	Char *tmpP;
	GThreadDataLocal *gThreadP;
    Char recieved[DEFAULT_BUFLEN];

	struct  sockaddr_in SockAddr;

	gThreadP = (GThreadDataLocal*)extras;

	if(gThreadP==0)
	{
		return 0;
	}
	gThreadP->threadStartInt = 1;
	gThreadP->gThread.RunningAvg = 100;

    SOCKET sck = CreateSocket();
	if(sck <= 0)
	{
		if(gThreadP->voipIndCallbackP)
		{
			gThreadP->voipIndCallbackP(1,gThreadP->uDataLong,&gThreadP->gThread,1);
		}
		return -1;
	}
    
    err=  SetSckAdd(SERVERNAME, PORT,&SockAddr);
	if(err)
	{
		closesocket(sck);
		gThreadP->threadStartInt = 0;
		if(gThreadP->voipIndCallbackP)
		{
			gThreadP->gThread.RunningAvg = 0;
			gThreadP->gThread.MatchedPackets = 0;
			gThreadP->voipIndCallbackP(InProcess,gThreadP->uDataLong,&gThreadP->gThread,0);//only data is return
			gThreadP->voipIndCallbackP(Success,gThreadP->uDataLong,&gThreadP->gThread,0);//call deinit
		}
	
		return -1;
	}
    SendPackets(sck,SockAddr,SERVERNAME, PORT);

	for(count = 1; count <= TOTALPACKETS; count ++)
	{
		er = recieve(sck,recieved,SockAddr,SERVERNAME, PORT);
		if(er==0)//no error
		{
			breakLoopB = FALSE;		  
			memmove((void*)&x,recieved,sizeof(UInt32));
		    tmpP = recieved + sizeof(UInt32);

		    if((x > 0) && (x < TOTALPACKETS))
	        {
		        matched ++;
				correctRespose = correctRespose + EvalResponse(x,count);
	        }
			quality = count - (count - correctRespose);
			gThreadP->gThread.RunningAvg = ((float)quality/(float)count)*100;
			gThreadP->gThread.MatchedPackets = matched;
		    //gThreadP->gThread.RunningAvg  = gThreadP->gThread.RunningAvg  + EvalResponse(x,count);
		}
		else
		{
			if(breakLoopB==TRUE)//request time out
			{
				if(count>2)
				{
					break;
				}
			}
		}
	}
	   

	gThreadP->threadStartInt = 0;
	if(gThreadP->voipIndCallbackP)
	{
		gThreadP->voipIndCallbackP(InProcess,gThreadP->uDataLong,&gThreadP->gThread,0);//only data is return
		gThreadP->voipIndCallbackP(Success,gThreadP->uDataLong,&gThreadP->gThread,0);//call deinit
	}
	closesocket(sck);
	return 0;
}
GThreadDataLocal * VoipQualityInit(VoipIndicatorCallBack voipIndCallbackP,long uDataLong)
{
	GThreadDataLocal *gThP = 0;
	gThP =(GThreadDataLocal *) malloc(sizeof(GThreadDataLocal));
	memset(gThP,0,sizeof(GThreadDataLocal));
	gThP->voipIndCallbackP = voipIndCallbackP;
	gThP->uDataLong = uDataLong;
	return gThP;

}
void VoipQualityDeInit(GThreadDataLocal **glP)
{
	if(glP)
	{
		if(*glP)
		{
			free(*glP);
			*glP = 0;
		}
	}

}
// ameya waingankar
// 24/2/2009
// to start the process by creating a thread
void VoipStart(GThreadDataLocal *gThP)
{
	
	if(gThP->threadStartInt==0)
	{
		gThP->threadStartInt = 1;
		CreateThread(NULL,0,SendRecieve,gThP,0,NULL);
	}
}

// ameya waingankar
// 24/2/2009
// to get values of matched packets and running average
void VoipGetValue(GThreadDataLocal *glP,GThreadData *gThreadP)
{
	if(gThreadP)
	{
		*gThreadP = glP->gThread;
	}
}