#include <windows.h>
#include "ltpmobile.h"
#include <winuser.h>
#include <winsock.h>
#include <commctrl.h>
#include <ntddndis.h>
//#include <nuiouser.h>
#include <winioctl.h>
#include <iphlpapi.h>

#include "gigabooth.h"


extern ltpStack *pstack;
extern HWND	wndMain;

/* networking */
int	isock=0;
int lastCount=0;


bool IsWireless(LPCTSTR pszAdapter);
int wifiGetIp(TCHAR *strAdapter, TCHAR *strIP);
bool wifiGetSsid(LPCTSTR pszAdapter, NDIS_802_11_SSID* pSsid);
int wifiAdapter(TCHAR *strAdapter);
bool	wifiGetRssi(TCHAR *strAdapter, NDIS_802_11_RSSI* pRssi);



int getWifiStatus(TCHAR *strssid, int *strength){
/*	NDIS_802_11_SSID ssid;
	NDIS_802_11_RSSI rssi;
	TCHAR wifi[100];
	int	i;

	wifiAdapter(wifi);
	if (IsWireless(wifi)){
		wifiGetSsid(wifi, &ssid);
		for (i= 0; i < ssid.SsidLength; i++)
			strssid[i] = ssid.Ssid[i];
		strssid[i] = 0;
		wifiGetRssi(wifi,&rssi);
		*strength = 200 + rssi;
		return 1;
	}
	else{
		*strength = 0;
		strssid[0] = 0;
	}

	return 0;*/
	return 0;
}


static int netRead(void *msg, int maxlength, unsigned long *address, short unsigned *port)
{	
	fd_set	readfs;
	struct sockaddr_in	addr;
	int	 ret;
	struct timeval	tv;
#ifdef WIN32
	int dummylen;
#else
	socklen_t	dummylen;
#endif

	tv.tv_sec = 1;
	tv.tv_usec = 0; //10msec time-out
	FD_ZERO(&readfs);
	FD_SET(isock, &readfs);


	ret = select(32, &readfs, NULL, NULL, &tv);
	if (ret <= 0)
		return 0;

	if (FD_ISSET(isock, &readfs))
	{
		dummylen = sizeof(addr);
		ret = recvfrom(isock, (char *)msg, maxlength, 0, (struct sockaddr *)&addr, &dummylen);

		*address	= (unsigned long)addr.sin_addr.s_addr;
		*port	= (int)addr.sin_port;
		return ret;
	}

	return 0;
}


DWORD WINAPI monitorSocket(LPVOID whyatall){
	int	 ret, length;
	struct sockaddr_in	addr;

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	while(1)
	{
		length = sizeof(addr);
		struct packet *p = (struct packet *)malloc(sizeof(packet));
		if(!p)
			break;
		ret = netRead(p->data, 2000, &(p->address), &(p->port));

		if(ret > 0)
			//get current window handle
			PostMessage(wndMain, WM_UDP_AVAILABLE, ret, (LPARAM)p);
		else 
			free(p);
		if(ret <= 0){
			int error = WSAGetLastError();
			WSASetLastError(0);
			//take a deep breath
			Sleep(100);
		}
	}
	return 0;
}


DWORD WINAPI monitorSocketOld(LPVOID whyatall){
	int	 ret, length;
	struct sockaddr_in	addr;

//	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	while(1){
		
		length = sizeof(addr);
		struct packet *p = (struct packet *)malloc(sizeof(packet));
		if (!p)
			break;
		ret = recvfrom(isock, (char *)p->data, 1000, 0, (struct sockaddr *)&addr, &length);
		p->address	= (unsigned long)addr.sin_addr.s_addr;
		p->port	= (unsigned short)addr.sin_port;

		if (ret > 0)
			PostMessage(wndMain, WM_UDP_AVAILABLE, ret, (LPARAM)p);
		else 
			free(p);
		if (ret <= 0){
			int error = WSAGetLastError();
			//take a deep breath
			Sleep(10);
		}
	}
	return 0;
}

/* some utils, specific to pocket pc */

int restartSocket(){

	if (isock)
		closesocket(isock);

	isock = (int)socket(AF_INET, SOCK_DGRAM, 0);

	if (isock == -1)
		return -1;
	return 0;
}

int __cdecl initNet(){	
	WSADATA	wsadata;
	int sock;

	int err = WSAStartup(0x0101, &wsadata);
	if (err != 0)
		return -1;

	sock = restartSocket();
//	CreateThread(NULL, 0, monitorSocket, NULL, 0, NULL);
	return sock;
}

int _cdecl netWrite(void *msg, int length, unsigned int32 address, unsigned short16 port){
	struct sockaddr_in	addr;

	addr.sin_addr.s_addr = address;
	addr.sin_port = (short)port;
	addr.sin_family = AF_INET;

	int ret = sendto(isock, (char *)msg, length, 0, (struct sockaddr *)&addr, sizeof(addr));
	return ret;
}

unsigned int __cdecl lookupDNS(char *host){
	int i, count=0;
	char *p = host;
	struct sockaddr_in	addr;
	struct hostent		*pent;

	if (!strcmp(host, "0"))
		return 0;

	while (*p){
		for (i = 0; i < 3; i++, p++)
			if (!isdigit(*p))
				break;
		if (*p != '.')
			break;
		p++;
		count++;
	}

	if (count == 3 && i > 0 && i <= 3)
		return inet_addr(host);

	pent = gethostbyname(host);
	if (!pent)
		return INADDR_NONE;

	addr.sin_addr = *((struct in_addr *) *pent->h_addr_list);

	return addr.sin_addr.s_addr;
}



/****************************************************************************
Wifi Handlers
*****************************************************************************/

bool IsWireless(LPCTSTR pszAdapter)
{
/*    HANDLE hDevice = CreateFile(
        NDISUIO_DEVICE_NAME, GENERIC_READ, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL);

    if (hDevice == INVALID_HANDLE_VALUE)
        return false;

    DWORD dwBytesReturned = 0;

    NDISUIO_QUERY_OID NDISUIOQueryOid = {0};
    NDISUIOQueryOid.ptcDeviceName = (PTCHAR) pszAdapter;
    NDISUIOQueryOid.Oid = OID_GEN_PHYSICAL_MEDIUM;

    if (!DeviceIoControl(
        hDevice, IOCTL_NDISUIO_QUERY_OID_VALUE,
        &NDISUIOQueryOid, sizeof(NDISUIOQueryOid),
        &NDISUIOQueryOid, sizeof(NDISUIOQueryOid),
        &dwBytesReturned, NULL))
    {
        CloseHandle(hDevice);
        return false;
    }

    CloseHandle(hDevice);
    return (*((PNDIS_PHYSICAL_MEDIUM) NDISUIOQueryOid.Data) == NdisPhysicalMediumWirelessLan);*/
	return 0;
}

int wifiAdapter(TCHAR *strAdapter)
{
    DWORD ret;
    ULONG buffLen = 0;

    if ((ret = GetAdaptersInfo(NULL, &buffLen)) != ERROR_BUFFER_OVERFLOW)
        return 0;

    IP_ADAPTER_INFO* pInfo = (IP_ADAPTER_INFO*) malloc(buffLen);

    if ((ret = GetAdaptersInfo(pInfo, &buffLen)) != ERROR_SUCCESS){
        free(pInfo);
        return 0;
    }

	ret = 0;
    while (pInfo){
		MultiByteToWideChar(CP_UTF8, 0, pInfo->AdapterName, -1, (LPWSTR) strAdapter, 200);
        if (IsWireless(strAdapter)){
			ret = 1;
            break;
        }

        pInfo = pInfo->Next;
    }

    free(pInfo);
    return ret;
}

bool wifiGetRssi(TCHAR *strAdapter, NDIS_802_11_RSSI* pRssi)
{
/*    HANDLE hDevice = CreateFile(
        NDISUIO_DEVICE_NAME, GENERIC_READ, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL);

    if (hDevice == INVALID_HANDLE_VALUE)
        return false;

    BYTE pBuffer[sizeof(NDISUIO_QUERY_OID) + sizeof(NDIS_802_11_RSSI)];
    PNDISUIO_QUERY_OID pNDISUIOQueryOid = (PNDISUIO_QUERY_OID) pBuffer;

    pNDISUIOQueryOid->ptcDeviceName = (PTCHAR) strAdapter;
    pNDISUIOQueryOid->Oid = OID_802_11_RSSI;

    DWORD dwBytesReturned = 0;

    if (!DeviceIoControl(
        hDevice, IOCTL_NDISUIO_QUERY_OID_VALUE, 
        pBuffer, sizeof(pBuffer),
        pBuffer, sizeof(pBuffer),
        &dwBytesReturned, NULL))
    {
        CloseHandle(hDevice);
        return false;
    }

    CloseHandle(hDevice);
    memcpy(pRssi, pNDISUIOQueryOid->Data, sizeof(NDIS_802_11_RSSI));
    return true;*/
	return true;
}

bool wifiGetSsid(LPCTSTR pszAdapter, NDIS_802_11_SSID* pSsid)
{
/*    HANDLE hDevice = CreateFile(
        NDISUIO_DEVICE_NAME, GENERIC_READ, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL);

    if (hDevice == INVALID_HANDLE_VALUE)
        return false;

    BYTE pBuffer[sizeof(NDISUIO_QUERY_OID) + sizeof(NDIS_802_11_SSID)];
    PNDISUIO_QUERY_OID pNDISUIOQueryOid = (PNDISUIO_QUERY_OID) pBuffer;

    pNDISUIOQueryOid->ptcDeviceName = (PTCHAR) pszAdapter;
    pNDISUIOQueryOid->Oid = OID_802_11_SSID;

    DWORD dwBytesReturned = 0;

    if (!DeviceIoControl(
        hDevice, IOCTL_NDISUIO_QUERY_OID_VALUE, 
        pBuffer, sizeof(pBuffer),
        pBuffer, sizeof(pBuffer),
        &dwBytesReturned, NULL))
    {
        CloseHandle(hDevice);
        return false;
    }

    CloseHandle(hDevice);
    memcpy(pSsid, pNDISUIOQueryOid->Data, sizeof(NDIS_802_11_SSID));
    return true;*/
	return true;
}


int wifiGetIp(TCHAR *strAdapter, TCHAR *strIP)
{

    DWORD ret;
    ULONG buffLen = 0;

    if ((ret = GetAdaptersInfo(NULL, &buffLen)) != ERROR_BUFFER_OVERFLOW)
        return 0;

    IP_ADAPTER_INFO* pInfo = (IP_ADAPTER_INFO*) malloc(buffLen);

    if ((ret = GetAdaptersInfo(pInfo, &buffLen)) != ERROR_SUCCESS){
        free(pInfo);
        return 0;
    }

    while (pInfo != NULL){
		TCHAR	wname[200];
		MultiByteToWideChar(CP_UTF8, 0, pInfo->AdapterName, -1, (LPWSTR) wname, 200);

		if (!lstrcmp(wname, strAdapter) == 0){
			MultiByteToWideChar(CP_UTF8, 0, pInfo->CurrentIpAddress->IpAddress.String, 16, 
					(LPWSTR)strIP, 20);
			break;
        }

        pInfo = pInfo->Next;
    }

    free(pInfo);
    return 1;
}
