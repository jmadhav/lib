/**
 Copyright (c) 2009 Geodesic Limited <http://www.geodesic.com>.
 
 This file is part of Spokn software.
 
 Spokn is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, version 2 of the License.
 
 Spokn is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with Spokn.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _MACOS_
#include <windows.h>
#include <winsock.h>
#include <commctrl.h>

#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <blowfish.h>
#include <ltpmobile.h>
#include <ezxml.h>
#include <ua.h>
#ifdef _MACOS_
#include "ltpandsip.h"
#endif
#define MAX_SIZE_DATA 10000
int appTerminateB;
int forwardStartB;
//struct AddressBook addressBookG={0,"TestCall","","","","","","1234567"};
// this is the single object that is the instance of the ltp stack for the user agent
struct ltpStack *pstack;
struct CDR *listCDRs=NULL;
struct AddressBook *listContacts=NULL;
struct VMail *listVMails=NULL;

static unsigned long	lastUpdate = 0;
static int busy = 0;
int terminateB;
char	myFolder[MAX_PATH], vmFolder[MAX_PATH], outFolder[MAX_PATH];

char mailServer[100], serverName[100],myTitle[200], fwdnumber[32], oldForward[33], myDID[32], client_name[32],client_ver[32],client_os[32],client_osver[32],client_model[100],client_uid[200];

int	redirect = REDIRECT2ONLINE;
int creditBalance = 0;
int bandwidth;
int gnewMails;
int GthreadTerminate = 0;
int clearProfileB = 0;
//variable to set the type for incoming call termination setting
int settingType = -1;  //not assigned state yet
int oldSetting = -1;
int bkupsettingType = -1; //backup of last known settingType;
#ifdef _MACOS_
int uniqueIDContact,uniqueIDVmail,uniqueIDCalllog;

UACallBackType uaCallBackObject;

#endif

#ifdef _MAC_OSX_CLIENT_
UACallBackType uaCallBackObject;
#endif

//add by mukesh 20359
ThreadStatusEnum threadStatus;
char uaUserid[32];
static char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789"
"+/";

/*
 ** Translation Table as described in RFC1113
 */
static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 ** Translation Table to decode (created by author)
 */
static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";
//base64 encoder
void encodeblock( unsigned char in[3], unsigned char out[4], int len )
{
    out[0] = cb64[ in[0] >> 2 ];
    out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
    out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
    out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}

/*
 ** (base64 decoder) decode 4 '6-bit' characters into 3 8-bit binary bytes
 */
void decodeblock( unsigned char in[4], unsigned char out[3] )
{   
    out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
    out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
    out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
}

//strip white space on both ends of a string
void strTrim(char *szString)
{
	int i;
	
	if (strlen(szString) == 0)
		return;
	
	//trim the leading spaces
	while (szString[0] <= ' '  && strlen(szString))
		memmove(szString, szString+1, strlen(szString));
	
	if (!strlen(szString))
		return;
	
	//trim the trailing spaces
	for (i = strlen(szString) - 1; !isgraph(szString[i]) && i >=0; i--)
		szString[i] = 0;
}


//the digest system used in our web api authentiction
void digest2String(unsigned char *digest, char *string)
{
	int	i;
	
	for (i = 0; i < 16; i++){
		*string++ = (digest[i] & 0x0f) + 'A';
		*string++ = ((digest[i] & 0xf0)/16) + 'a';
	}
	*string = 0;
}

// the http cookie used to authenticate the user for all calls to the web apis
void httpCookie(char *cookie)
{
	struct	MD5Context	md5;
	unsigned char	digest[16];
	
	memset(&md5, 0, sizeof(md5));
	MD5Init(&md5);
	
	MD5Update(&md5, (char unsigned *)pstack->ltpUserid, strlen(pstack->ltpUserid), pstack->bigEndian);
	MD5Update(&md5, (char unsigned *)pstack->ltpPassword, strlen(pstack->ltpPassword), pstack->bigEndian);
	MD5Update(&md5, (char unsigned *)pstack->ltpNonce, strlen(pstack->ltpNonce), pstack->bigEndian);
	MD5Final(digest,&md5);
	
	digest2String(digest, cookie);
}

//encode a url to escape certain characters
void urlencode(char *s, char *t) {
	unsigned char *p;
	char *tp;
	
	if(t == NULL) {
		fprintf(stderr, "Serious memory error...\n");
		exit(1);
	}
	
	tp=t;
	
	for(p=(unsigned char*)s; *p; p++) {
		
		if((*p > 0x00 && *p < ',') ||
		   (*p > '9' && *p < 'A') ||
		   (*p > 'Z' && *p < '_') ||
		   (*p > '_' && *p < 'a') ||
		   (*p > 'z' && *p < 0xA1)) {
			sprintf(tp, "%%%02X", (unsigned char)*p);
			tp += 3;
		} else {
			*tp = *p;
			tp++;
		}
	}
	
	*tp='\0';
	return;
}

//encoding a value for generating xml 
void xmlEncode(char *s, char *t) {
	char *p;
	char *tp;
	
	if(t == NULL) {
		fprintf(stderr, "Serious memory error...\n");
		exit(1);
	}
	
	tp=t;
	
	for(p=s; *p; p++) {
		switch(*p){
			case '<':
				strcpy(tp, "&lt;");
				tp += 4;
				break;
			case '>':
				strcpy(tp, "&gt;");
				tp += 4;
				break;
			case '&':
				strcpy(tp, "&amp;");
				tp += 5;
				break;
			case '"':
				strcpy(tp, "&quot;");
				tp += 6;
				break;
			case '\'':
				strcpy(tp, "&apos;");
				tp += 5;
			default:
				*tp = *p;
				tp++;
		}
	}
	
	*tp='\0';
	return;
}

//this is intended to only read text or xml data, it stops at the end of the buffer, end of net
//or end of line
static int readNetLine(int sock, char *buff, int maxlength)
{
	int	i = 0;
	char	c;
	
	while (recv(sock, &c, 1, 0)==1){
		i++;
		*buff++ = c;
		//have we run out of buffer?
		if (i == maxlength)
			return i;
		//have we reached the end of the line?
		if (c == '\n'){
			//end the line gracefully
			*buff = 0;
			return i;
		}
	}
	
	//socket closed!
	return i;
}


static int restCall(char *requestfile, char *responsefile, char *host, char *url,int terminateThreadB)
{
	FILE	*pf;
	SOCKET	sock;
	
	struct	sockaddr_in	addr;
	int	byteCount = 0, isChunked = 1, contentLength = 0, ret, length;
#ifdef MAX_SIZE_DATA
	char	*data=0, header[1000];
	
#else
	char	data[10000], header[1000];
#endif
	//printf("\nrestCall");
	//host = "anurag.spokn.com";
	
	pf = fopen(requestfile, "rb");
	if (!pf)
		return 0;
	fseek(pf, 0, SEEK_END);
	contentLength = ftell(pf);
	fclose(pf);
	
	//prepare the http header
#ifdef _STAGING_SERVER_
	sprintf(header, 
			"POST %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Content-Length: %d\r\n"
			"Authorization: %s %s\r\n\r\n",
			url, host, contentLength,STAGING_AUTH_STRING1,STAGING_AUTH_STRING2);
#else
	
	sprintf(header, 
			"POST %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Content-Length: %d\r\n\r\n",url, host, contentLength);
	
#endif
	addr.sin_addr.s_addr = lookupDNS(host);
	if (addr.sin_addr.s_addr == INADDR_NONE)
		return 0;
	
	addr.sin_port = htons(80);	
	addr.sin_family = AF_INET;
	
	//try connecting the socket
	sock = (int)socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)))
	{
		closesocket(sock);
		return 0;
	}
	//send the http headers
	send(sock, header, strlen(header),0);
	byteCount += strlen(header);
	
	//send the xml data
	pf = fopen(requestfile, "rb");
	if (!pf)
	{	
		closesocket(sock);
		return 0;
	}	
#ifdef MAX_SIZE_DATA
	data = malloc(MAX_SIZE_DATA+10);
	memset(data,0,MAX_SIZE_DATA+10);
	while((ret = fread(data, 1, MAX_SIZE_DATA, pf)) > 0){
		send(sock, data, ret, 0);
		byteCount += ret;
	}
	

	#else
	while((ret = fread(data, 1, sizeof(data), pf)) > 0){
		send(sock, data, ret, 0);
		byteCount += ret;
	}
	#endif
	fclose(pf);
	
	//read the headers
	//Tasvir Rohila - bug#18352 & #18353 - Initialize isChunked to zero for profile to download & 
	//Add/Delete contact to work.
	isChunked = 0;
	while (1){
		#ifdef MAX_SIZE_DATA
		int length = readNetLine(sock, data, MAX_SIZE_DATA);

		#else
		int length = readNetLine(sock, data, sizeof(data));

		#endif
		byteCount += length;
		if (length <= 0)
			break;
		if (!strncmp(data, "Transfer-Encoding:", 18) && strstr(data, "chunked"))
			isChunked = 1;
		if (!strcmp(data, "\r\n"))
			break;
	}
	
	//prepare to download xml
	pf = fopen(responsefile, "w");
	
	if (!pf)
	{	
		#ifdef MAX_SIZE_DATA
			if(data)
				free(data);
		#endif	
		closesocket(sock);
		return 0;
	}
	//read the body 
	while (1) {
		if (isChunked){
			int count;
			
		#ifdef MAX_SIZE_DATA
					 length = readNetLine(sock, data, MAX_SIZE_DATA);
					
		#else
					 length = readNetLine(sock, data, sizeof(data));
					
		#endif
			
			if (length <= 0)
				break;
			byteCount += length;
			count = strtol(data, NULL, 16);
			if (count <= 0)
				break;
			
			//read the chunk in multiple calls to read
			while(count){
			#ifdef MAX_SIZE_DATA 
				length = recv(sock, data, (count > MAX_SIZE_DATA) ? MAX_SIZE_DATA : count, 0);
			#else
				length = recv(sock, data, count > sizeof(data) ? sizeof(data) : count, 0);
			#endif	
				if (length <= 0) //crap!! the socket closed
					goto end;
				byteCount += length;
				fwrite(data, length, 1, pf);
				count -= length;
			}
			
			//read the \r\n after the chunk (if any)
			length = readNetLine(sock, data, 2);
			if (length != 2)
				break;
			byteCount += length;
			//loop back to read the next chunk
		}
		else{ // read it in fixed blocks of data
			while (1){
				#ifdef MAX_SIZE_DATA 
				length = recv(sock, data, MAX_SIZE_DATA, 0);
				#else
				length = recv(sock, data, sizeof(data), 0);

				#endif	
				if (length > 0)
					fwrite(data, length, 1, pf);
				else 
					goto end;
				byteCount += length;
			}
		}
	}
end:
	fclose(pf); // close the download.xml handle
#ifdef MAX_SIZE_DATA
	if(data)
		free(data);
#endif	
	closesocket(sock);
	if(GthreadTerminate && terminateThreadB)
	{
		//printf("\nthread terminate");
		threadStatus = ThreadNotStart ;
	#ifdef _MAC_OSX_CLIENT_
		threadStopped();
	#endif
		busy = 0;
	//#ifdef _MACOS_
		UaThreadEnd();
		THREAD_EXIT(0);
	//#endif
		//GthreadTerminate = 0;
	}
	return byteCount;
}


/*
 CDR functions
 */

void cdrEmpty()
{
	struct CDR *p, *q;
	
	p = listCDRs;
	while (p){
		q = p->next;
		free(p);
		p = q;
	}
	listCDRs = NULL;
}

#ifdef _MAC_OSX_CLIENT_

void threadStopped()
{
	uaCallBackObject.alertNotifyP(UA_ALERT,0,THREAD_STOPPED,(unsigned long)uaCallBackObject.uData,0);	
}

#endif

static void cdrSave(){
	char pathname[MAX_PATH];
	struct	 CDR *q;
	char	line[1000];
	FILE	*pf;
#ifdef _MACOS_
	sprintf(pathname, "%s/calls.txt", myFolder);
	
#else
	sprintf(pathname, "%s\\calls.txt", myFolder);
	
#endif	
	
	pf = fopen(pathname, "w");
	if (!pf)
		return;
	
	for (q = listCDRs; q; q = q->next){
#ifdef _MACOS_
		///		int addressUId;
		//	int propertyID;	
	#ifdef _MAC_OSX_CLIENT_ 
		sprintf(line, "<cdr><date>%lu</date><duration>%d</duration><type>%d</type><userid>%s</userid> <abid>%d</abid><recordid>%d</recordid><uniqueId>%s</uniqueId></cdr>\r\n",
				(unsigned long)q->date, (int)q->duration, (int)q->direction, q->userid,q->addressUId,q->recordID,q->uniqueId);
	#else
		sprintf(line, "<cdr><date>%lu</date><duration>%d</duration><type>%d</type><userid>%s</userid> <abid>%d</abid><recordid>%d</recordid></cdr>\r\n",
				(unsigned long)q->date, (int)q->duration, (int)q->direction, q->userid,q->recordUId,q->isexistRecordID);
	#endif
		
#else
		sprintf(line, "<cdr><date>%lu</date><duration>%d</duration><type>%d</type><userid>%s</userid></cdr>\r\n",
				(unsigned long)q->date, (int)q->duration, (int)q->direction, q->userid);
		
#endif	
		
		fwrite(line, strlen(line), 1, pf);
	}
	fclose(pf);
}


static void cdrCompact() {
	struct CDR *p, *q;
	//check if the cdr count has exceeded 200
	int i = 0;
	for (q = listCDRs; q; q = q->next)
		i++;
	
	//high water point is 300
	if (i < 300)
		return;
	
	//remove all the cdrs past the 200th
	for (i = 0, p = listCDRs; p; p = p->next)
		if (i == 200)
			break;
	
	//place a null to truncate this list
	q = p->next;
	p->next = NULL;
	
	//remove the rest of the tail
	while (q){
		p = q->next;
		free(q);
		q = p;
	}
	cdrSave();
}

#ifdef _MACOS_
	#ifdef	_MAC_OSX_CLIENT_
	void cdrAdd(char *userid, time_t time, int duration, int direction ,int abid,int recordid,char *Uid)
	#else
		void cdrAdd(char *userid, time_t time, int duration, int direction ,int abid,int recordid)
	#endif
#else
		void cdrAdd(char *userid, time_t time, int duration, int direction)
#endif
{
	//write to the disk
	char	pathname[MAX_PATH];
	char	line[300];
	struct	CDR	*p;
	FILE	*pf;
	int i;
	
	if (listCDRs){
		// compare the last call with this 
		if (!strcmp(listCDRs->userid, userid) && listCDRs->direction == direction
			&& listCDRs->date == (time_t)time)
			return;
	}
	
	p = (struct CDR *) malloc(sizeof(struct CDR));
	if (!p)
		return;
	
	memset(p, 0, sizeof(struct CDR));
	#ifdef _MACOS_
		p->uniqueID = ++uniqueIDCalllog;
	#endif
	//Kaustubh Deshpande 21114 fixed-Junk characters are displayed in call list on calling a long number.
	//the userid has limit of 32, and we were not checking the limit earlier. Now a check is added for that.
	for (i=0;i<31;i++)
	{
		p->userid[i]=userid[i];
		if(userid[i]==0) break;
	}
	p->userid[32]=0;
	///
	
	p->date = (time_t)time;
	p->duration = duration;
	p->direction = direction;
	#ifdef _MACOS_
		p->recordUId=abid;
		p->isexistRecordID=0;
	#ifdef  _MAC_OSX_CLIENT_
		strcpy(p->uniqueId,Uid);	
	#endif
		if(recordid)
		{	
			p->isexistRecordID=1;
		}	
	#endif
	getTitleOf(p->userid, p->title);
	//printf("\n %s %ld",p->userid,(long)p->date);
	if(listCDRs)
	{	
		if(listCDRs->date>p->date)
		{
			p->next = listCDRs->next;
			listCDRs->next = p;
		
		}
		else {
				p->next = listCDRs;
				listCDRs = p;

		}
	}
	else
	{
		p->next = listCDRs;
		listCDRs = p;
	
	}

	
	//add to the linked list
	#ifdef _MACOS_
	sprintf(pathname, "%s/calls.txt", myFolder);
	
#else
	sprintf(pathname, "%s\\calls.txt", myFolder);
	
	
#endif	
#ifdef _MACOS_
	///		int addressUId;
	//	int propertyID;	
	
	#ifdef _MAC_OSX_CLIENT_
		sprintf(line, "<cdr><date>%lu</date><duration>%d</duration><type>%d</type><userid>%s</userid> <abid>%d</abid><recordid>%d</recordid><uniqueId>%s</uniqueId></cdr>\r\n",
			(unsigned long)p->date, (int)p->duration, (int)p->direction, p->userid,p->addressUId,p->recordID,p->uniqueId);
	#else
		sprintf(line, "<cdr><date>%lu</date><duration>%d</duration><type>%d</type><userid>%s</userid> <abid>%d</abid><recordid>%d</recordid></cdr>\r\n",
			(unsigned long)p->date, (int)p->duration, (int)p->direction, p->userid,p->recordUId,p->isexistRecordID);
	#endif
#else
	sprintf(line, "<cdr><date>%lu</date><duration>%d</duration><type>%d</type><userid>%s</userid></cdr>\r\n",
			(unsigned long)p->date, (int)p->duration, (int)p->direction, p->userid);
	
#endif	
	
	
	
	pf = fopen(pathname, "a");
	if(pf)
	{	
		fwrite(line, strlen(line), 1, pf);
		fclose(pf);
	}
	
}

void cdrRemove(struct CDR *p)
{
	struct CDR *q;
	if (!p)
		return;
	
	if (p == listCDRs) {
		listCDRs = p->next;
	}
	else {
		//delink the cdr
		for (q = listCDRs; q->next; q = q->next){
			if (q->next == p){
				q->next = p->next;
				break;
			}
		}
	}
	
	free(p);
	cdrSave();
}

void cdrLoad() {
	FILE	*pf;
	char	pathname[MAX_PATH];
	struct CDR *p;
	int		index;
	char	line[1000];
	ezxml_t	cdr, duration, date, userid, type, abidP,recordidP;
#ifdef _MAC_OSX_CLIENT_
	ezxml_t uId;
#endif	
#ifdef _MACOS_
	sprintf(pathname, "%s/calls.txt", myFolder);
	
#else
	sprintf(pathname, "%s\\calls.txt", myFolder);
	
#endif	
	
	pf = fopen(pathname, "r");
	if (!pf)
		return;
	
	index = 0;
	while (fgets(line, 999, pf)){
		
		cdr = ezxml_parse_str(line, strlen(line));
		if (!cdr)
			continue;
		
		date = ezxml_child(cdr, "date");
		duration = ezxml_child(cdr, "duration");
		type = ezxml_child(cdr, "type");
		userid = ezxml_child(cdr, "userid");
		abidP = ezxml_child(cdr, "abid");
		recordidP = ezxml_child(cdr, "recordid");
		
#ifdef	_MAC_OSX_CLIENT_
		uId=ezxml_child(cdr, "uniqueId");
#endif
		
		p = (struct CDR *) malloc(sizeof(struct CDR));
		if (!p){
			fclose(pf);
			
			ezxml_free(cdr);
			return;
		}
		memset(p, 0, sizeof(struct CDR));
		#ifdef _MACOS_
			p->uniqueID = ++uniqueIDCalllog;
		if (abidP)
			p->recordUId = (unsigned long)atol(abidP->txt);
		if (recordidP)
			p->isexistRecordID = (unsigned long)atol(recordidP->txt);
		#ifdef	_MAC_OSX_CLIENT_
			if(uId)
			{
				strcpy(p->uniqueId,uId->txt);
			}
		#endif	
		
		#endif
		if (date)
			p->date = (unsigned long)atol(date->txt);
		if (duration)
			p->duration = atoi(duration->txt);
		else
			p->duration = 0;
		if (userid)
			strcpy(p->userid, userid->txt);
		if (type)
			p->direction = atoi(type->txt);
		
		getTitleOf(p->userid, p->title);
		
		//add to the linked list
		//p->next = listCDRs;
		//listCDRs = p;
		if(listCDRs)
		{	
			if(listCDRs->date>p->date)
			{
				p->next = listCDRs->next;
				listCDRs->next = p;
				
			}
			else {
				p->next = listCDRs;
				listCDRs = p;
				
			}
		}
		else
		{
			p->next = listCDRs;
			listCDRs = p;
			
		}
		
		ezxml_free(cdr);
		
	}
	fclose(pf);
}

void cdrRemoveAll()
{	
	cdrEmpty();
	cdrSave();
}

/**
 Contacts functions
 */

struct AddressBook *getContactsList()
{
	return listContacts;
}

void resetContacts(){
	struct AddressBook	*p, *q;
	p = listContacts;
	while(p){
		q = p->next;
		free(p);
		p  = q;
	}
	listContacts = NULL;
}

static int strCompare(const char *p, const char *q) 
{
	char x, y;
	while(*p && *q) {
		x = tolower(*p);
		y = tolower(*q);
		if (x < y)
			return -1;
		if (x > y)
			return 1;
		p++;q++;
	}
	return 0;
}


void sortContacts()
{
	struct AddressBook *newList, *i, *p, *prev, *nextOld;
	
	newList = NULL;
	
	for (i = listContacts; i; i = nextOld){
		
		nextOld = i->next;
		
		//insert it in the begining if the list is empty
		if (!newList){
			i->next =NULL; //keep the pointers of the current list clean - multithreading requirement
			newList = i;
		}
		else {
			//forward to the object with title greater than the new object
			for (prev = NULL, p = newList; p; p = p->next){
				if (strCompare(p->title, i->title) > 0)
					break;
				prev = p;
			}
			
			//i has to be inserted before p
			if (prev)
				prev->next = i;
			else
				newList = i;
			i->next = p; //keep the pointers of the current list clean - multithreading requirement
		}
		
	} //take the next item of the old list
	
	//swap the lists
	listContacts = newList;
}

int countOfContacts()
{
	struct AddressBook	*p;
	int	i;
	for (i = 0, p = listContacts; p; p = p->next)
		if(!p->isDeleted)
			i++;
	return i;
}

struct AddressBook *getContact(int id)
{
	struct AddressBook	*p;
	
	for (p = listContacts; p; p = p->next)
	{	
		if (p->id == id && !p->isDeleted)
			return p;
	}	
	return NULL;
}

//used to match substrings against title
int isMatched(char *title, char *query)
{
	char *r;
	int qlen, last;
	
	if (!*query)
		return 1;
	
	qlen = strlen(query);
	
	//check if the query matches the starting
	r = title;
	last = ' ';
	for (r = title; *r; r++){
		//if the match occurs at a word boundary, select it
		if ((last == ' '  || last == '.') && *r > ' ')
#ifdef _MACOS_
			if (!strncasecmp(r, query, qlen))
				return 1;
		
#else
		if (!_strnicmp(r, query, qlen))
			return 1;
		
#endif
		last = *r;
	}
	
	return 0;
}

//used to highlight the matched parts of a contact's title against a search string
int indexOf(char *title, char *query)
{
	char lowertitle[100], *r;
	int i;
	
	if (!*query)
		return -1;
	
	for (i = 0; i < 99 && title[i]; i++)
		lowertitle[i] = tolower(title[i]);
	
	lowertitle[i] = 0;
	r = strstr(lowertitle, query);
	if (!r)
		return -1;
	else
		return r - lowertitle;
}


static void updateContactPresence(int id, int presence)
{
	struct AddressBook	*p;
	
	for (p = listContacts; p; p = p->next)
		if (p->id == id)
			p->presence = presence;
}

void deleteContactLocal(int id) 
{
	struct AddressBook	*p;
	
	for (p = listContacts; p; p = p->next)
		if (p->id == id){
			p->isDeleted = 1;
			p->dirty = 1;
		}
}

struct AddressBook *updateContact(unsigned long id, char *title, char *mobile, char *home, char *business, char *other, char *email, char *spoknid)
{
	struct AddressBook	*p, *q;
	
	for (p = listContacts; p; p = p->next)
		if (id == p->id){
			strcpy(p->title, title);
			strcpy(p->mobile, mobile);
			strcpy(p->home, home);
			strcpy(p->business, business);
			if (other)
				strcpy(p->other, other);
			if (email)
				strcpy(p->email, email);
			if (spoknid)
				strcpy(p->spoknid, spoknid);
			p->dirty = 1;
			return p;
		}
	
	q = (struct AddressBook *)malloc(sizeof(struct AddressBook));
	
	memset(q, 0, sizeof(struct AddressBook));
	#ifdef _MACOS_
		q->uniqueID = ++uniqueIDContact;
	#endif
	q->id = id;
	
	strcpy(q->title, title);
	strcpy(q->mobile, mobile);
	strcpy(q->home, home);
	strcpy(q->business, business);
	if (other)
		strcpy(q->other, other);
	if (email)
		strcpy(q->email, email);
	if (spoknid)
		strcpy(q->spoknid, spoknid);
	q->dirty = 1;
	//insert in at the head and sort
	q->next = listContacts;
	listContacts = q;
	return q;
}

struct AddressBook *addContact(char *title, char *mobile, char *home, char *business, char *other, char *email, char *spoknid)
{
	struct AddressBook	*q;
	
	q = (struct AddressBook *)malloc(sizeof(struct AddressBook));
	memset(q, 0, sizeof(struct AddressBook));
	#ifdef _MACOS_
		q->uniqueID = ++uniqueIDContact;
	#endif
	strcpy(q->title, title);
	strcpy(q->mobile, mobile);
	strcpy(q->home, home);
	strcpy(q->business, business);
	if (other)
		strcpy(q->other, other);
	if (email)
		strcpy(q->email, email);
	if (spoknid)
		strcpy(q->spoknid, spoknid);
	q->dirty = 1;
	
	//insert in at the head and sort
	q->next = listContacts;
	listContacts = q;
	return q;
}


struct AddressBook *getTitleOf(char *userid, char *title){
	struct AddressBook	*p;
	
	title[0] = 0;
	
	if (*userid == 0){
		strcpy(title, "(Unknown)");
		return NULL;
	}
	
	for (p = listContacts; p; p = p->next){
		
		if (p->isDeleted)
			continue;
		
		if (!strcmp(p->mobile, userid)){
			sprintf(title, "%s (m)", p->title);
			return p;
		}
		
		if (!strcmp(p->home, userid)){
			sprintf(title, "%s (h)", p->title);
			return p;
		}
		
		if (!strcmp(p->business, userid)){
			sprintf(title, "%s (w)", p->title);
			return p;
		}
		
		if (!strcmp(p->other, userid)){
			sprintf(title, "%s (o)", p->title);
			return p;
		}
		
		if (!strcmp(p->email, userid)){
			sprintf(title, "%s (mail)", p->title);
			return p;
		}
		if (!strcmp(p->spoknid, userid)){
			sprintf(title, "%s (s)", p->title);
			return p;
		}
	}
	
	sprintf(title, "%s", userid);
	return NULL;
}

struct AddressBook *getContactOf(char *userid)
{
	struct AddressBook	*p;
	
	for (p = listContacts; p; p = p->next){
		
		if (p->isDeleted)
			continue;
		
		
		if (!strcmp(p->spoknid, userid))
			return p;
		
		
		if (!strcmp(p->mobile, userid))
			return p;
		
		if (!strcmp(p->home, userid))
			return p;
		
		if (!strcmp(p->business, userid))
			return p;
		
		if (!strcmp(p->other, userid))
			return p;
		
		if (!strcmp(p->email, userid))
			return p;
	}
	
	return NULL;
}

/*
 VMS routines
 
 voice mails are held in memory as a linked list with the latest voice mail at the head of the list
 voice mails are stored on the disk as individual xml objects with one line for each object in the same orde
 as the linked list.
 
 note: while loading the vmails back, each successive object read from the disk is added to the tail of the list
 to preserve the same order as saved.
 */

void vmsEmpty()
{
	struct VMail *p, *q;
	
	p = listVMails;
	while (p)
	{
		
		q = p->next;
		free(p);
		p = q;
	}
	listVMails = NULL;
}

struct VMail *vmsById(char *id)
{
	struct VMail *p;
	
	for (p = listVMails; p; p = p->next)
		if (!strcmp(p->vmsid, id))
			return p;
	return NULL;
}

static void vmsSort()
{
	struct VMail *newList, *i, *p, *prev, *nextOld;
	
	newList = NULL;
	
	for (i = listVMails; i; i = nextOld){
		
		nextOld = i->next;
		
		//insert it in the begining if the list is empty
		if (!newList){
			i->next =NULL; //keep the pointers of the current list clean - multithreading requirement
			newList = i;
		}
		else {
			//forward to the object with id greater than the new object
			for (prev = NULL, p = newList; p; p = p->next){
				if (p->date < i->date)
					break;
				prev = p;
			}
			
			//i has to be inserted before p
			if (prev)
				prev->next = i;
			else
				newList = i;
			i->next = p; //keep the pointers of the current list clean - multithreading requirement
		}
		
	} //take the next item of the old list
	
	//swap the lists
	listVMails = newList;
}

static struct VMail *vmsRead(ezxml_t vmail)
{
	struct VMail *p;
	ezxml_t	date, userid, vmsid, direction, status, deleted, hashid, toDelete,abidP,recordidP;//,uId;
	#ifdef _MAC_OSX_CLIENT_
		ezxml_t uId;
	#endif	
	
	
	date = ezxml_child(vmail, "dt");
	vmsid = ezxml_child(vmail, "id");
	userid = ezxml_child(vmail, "u");
	direction = ezxml_child(vmail, "dir");
	status = ezxml_child(vmail, "status");
	hashid = ezxml_child(vmail, "hashid");
	deleted = ezxml_child(vmail, "deleted");
	toDelete = ezxml_child(vmail, "todelete");
	abidP = ezxml_child(vmail, "abid");
	recordidP = ezxml_child(vmail, "recordid");
#ifdef	_MAC_OSX_CLIENT_
	uId=ezxml_child(vmail, "uniqueId");
#endif
	//check for all the required tags within <vm>
	if (!status || !date || !vmsid || !userid || !direction 
		|| !status || !hashid)
		return NULL;
	
	//if no vmail exists, then create a new one at the head of listVMails
	p = vmsById(vmsid->txt);
	
	if (!p){
		p = (struct VMail *) malloc(sizeof(struct VMail));
		if (!p)
			return NULL;
		memset(p, 0, sizeof(struct VMail));
		#ifdef _MACOS_
			p->uniqueID = ++uniqueIDVmail;
		#endif
				
		strcpy(p->hashid, hashid->txt);
		strcpy(p->vmsid, vmsid->txt);
		#ifdef _MACOS_
			if (abidP)
			{
				p->recordUId = (unsigned long)atol(abidP->txt);
			} 
			if (recordidP)
			{
				p->isexistRecordID = (unsigned long)atol(recordidP->txt);
			}
		
		#ifdef	_MAC_OSX_CLIENT_
		if(uId)
			strcpy(p->uniqueId,uId->txt);
		#endif
		#endif
		//make this 'starred', this is fresh mail
		
		if(listVMails)
			p->next = listVMails;
		listVMails = p;
	}
	
	//update the vmail
	p->date = (unsigned long)atol(date->txt);
	strcpy(p->userid, userid->txt);
	strcpy(p->vmsid, vmsid->txt);
	
	if (deleted){
		if (!strcmp(deleted->txt, "1"))
			p->deleted = 1;
		else
			p->deleted = 0;
	}
	
	if (toDelete)
		p->toDelete = 1;
	
	if (!strcmp(direction->txt, "out"))
		p->direction = VMAIL_OUT;
	else
		p->direction = VMAIL_IN;
	
	if (!strcmp(status->txt, "new"))
		p->status = VMAIL_NEW;
	else if (!strcmp(status->txt, "active"))
		p->status = VMAIL_ACTIVE;
	else if (!strcmp(status->txt, "delivered"))
	{
		p->status = VMAIL_DELIVERED;
		if(p->direction==VMAIL_OUT)
			p->dirty=FALSE;
	}
	else
		p->status = VMAIL_FAILED;
	
	return p;
}

static int vmsWrite(FILE *pf, struct VMail *p)
{
	//don't write those that are deleted already
	if (p->deleted)
		return 0;
	#ifdef _MACOS_
	
		#ifdef _MAC_OSX_CLIENT_
				
				fprintf(pf, "<vm><dt>%u</dt><u>%s</u><id>%s</id><hashid>%s</hashid><dir>%s</dir><abid>%d</abid><recordid>%d</recordid><uniqueId>%s</uniqueId>",
					(unsigned int)p->date, p->userid, p->vmsid, p->hashid, p->direction == VMAIL_OUT ? "out" : "in",p->recordUId,p->isexistRecordID,p->uniqueId);
		#else
			fprintf(pf, "<vm><dt>%u</dt><u>%s</u><id>%s</id><hashid>%s</hashid><dir>%s</dir><abid>%d</abid><recordid>%d</recordid>",
					(unsigned int)p->date, p->userid, p->vmsid, p->hashid, p->direction == VMAIL_OUT ? "out" : "in",p->recordUId,p->isexistRecordID);	
		#endif	
#else
	fprintf(pf, "<vm><dt>%u</dt><u>%s</u><id>%s</id><hashid>%s</hashid><dir>%s</dir>",
			(unsigned int)p->date, p->userid, p->vmsid, p->hashid, p->direction == VMAIL_OUT ? "out" : "in");
	#endif
	if (p->deleted)
		fprintf(pf, "<deleted>1</deleted>");
	switch(p->status){
		case VMAIL_NEW:
			fprintf(pf, "<status>new</status>");
			break;
		case VMAIL_DELIVERED:
			fprintf(pf, "<status>delivered</status>");
			break;
		case VMAIL_ACTIVE:
			fprintf(pf, "<status>active</status>");
			break;
		default:
			fprintf(pf, "<status>failed</status>");
	}
	
	if (p->toDelete)
		fprintf(pf, "<todelete>1</todelete>");
	fprintf(pf, "</vm>\n");
	
	return 1;
}

static void vmsCompact(){
	char path[MAX_PATH];
	struct VMail *p, *q;
	int i = 0;
	
	for (p = listVMails; p && i < VMAIL_MAXCOUNT; p = p->next)
		i++;
	
	if (!p)
		return;
	
	//remove all the cdrs past this point
	q = p->next;
	p->next = NULL;
	
	while (q){
#ifdef _MACOS_
		sprintf(path, "%s/%s.gsm", vmFolder, p->hashid);
#else
		sprintf(path, "%s\\%s.gsm", vmFolder, p->hashid);
#endif
		
		
		
		unlink(path);
		p = q->next;
		free(q);
		q = p;
	}
}

void vmsDelete(struct VMail *p)
{
	char	path[MAX_PATH];
	if (!p)
		return;
	
	//keep the vmail in the log file
	p->toDelete = 1;
#ifdef _MACOS_
	sprintf(path, "%s/%s.gsm", vmFolder, p->hashid);
#else
	sprintf(path, "%s\\%s.gsm", vmFolder, p->hashid);
#endif
	
	
	unlink(path);
	
	//profileResync();
}

#ifdef _MACOS_
	#ifdef  _MAC_OSX_CLIENT_ //If Mac OSx Client
		struct VMail *vmsUpdate(char *userid, char *hashid, char *vmsid, time_t time, int status, int direction,int laddressUId,int lrecordID,char *uId)
	#else	// else iPhone client
		struct VMail *vmsUpdate(char *userid, char *hashid, char *vmsid, time_t time, int status, int direction,int laddressUId,int lrecordID)
	#endif
#else
	struct VMail *vmsUpdate(char *userid, char *hashid, char *vmsid, time_t time, int status, int direction)
#endif
//struct VMail *vmsUpdate(char *userid, char *hashid, char *vmsid, time_t time, int status, int direction)
{
	struct	VMail	*p=NULL;
	#ifndef _FORWARD_VMS_
	if (vmsid)
		p = vmsById(vmsid);
	
	//if it already exists, then update the status and return
	if (p){
		p->status = status;
		return p;
	}
#endif	
	//hmm this looks like a new one
	p = (struct VMail *) malloc(sizeof(struct VMail));
	if (!p)
		return NULL;
	memset(p, 0, sizeof(struct VMail));
	#ifdef _MACOS_
		#ifdef _MAC_OSX_CLIENT_
			strcpy(p->uniqueId,uId);
		#else
				p->uniqueID = ++uniqueIDVmail;	
		#endif
	#endif
	strcpy(p->userid, userid);
	if (vmsid)
		strcpy(p->vmsid, vmsid);
	strcpy(p->hashid, hashid);
	p->date = (time_t)time;
	p->direction = direction;
	p->status = status;
	#ifdef _MACOS_
		p->recordUId = laddressUId;
	
	p->isexistRecordID = 0;
	if(lrecordID)
	{	
		p->isexistRecordID = 1;
	}	
	#endif
	//add to the head of the list
	if (!listVMails)
		listVMails = p;
	else {
		p->next = listVMails;
		listVMails = p;
	}
	
	profileSave();
	return p;
}

static void vmsUpload(struct VMail *v)
{
	char	key[100], *strxml;
#ifdef MAX_SIZE_DATA
	unsigned char g[10], b64[10], *buffer=0;
#else
	unsigned char g[10], b64[10], buffer[10000];
#endif
	
	char	requestfile[MAX_PATH], responsefile[MAX_PATH], path[MAX_PATH];
	FILE	*pfIn, *pfOut;
	int		length, byteCount;
	
	if (v->direction != VMAIL_OUT || v->status != VMAIL_NEW)
		return;
#ifdef _MACOS_
	sprintf(path, "%s/%s.gsm", vmFolder, v->hashid);
#else
	sprintf(path, "%s\\%s.gsm", vmFolder, v->hashid);
#endif
	
	
	pfIn = fopen(path, "rb");
	if (!pfIn){
		v->toDelete = 1;
		return;
	}
#ifdef _MACOS_
	sprintf(requestfile, "%s/vmaireq.txt", myFolder);
	sprintf(responsefile, "%s/vmailresp.txt", myFolder);
	
#else
	sprintf(requestfile, "%s\\vmaireq.txt", myFolder);
	sprintf(responsefile, "%s\\vmailresp.txt", myFolder);
	
#endif
	
	
	pfOut = fopen(requestfile, "wb");
	if (!pfOut){
		fclose(pfIn);
		return;
	}
	
	httpCookie(key);
	fprintf(pfOut, "<?xml version=\"1.0\"?><vms>\n <u>%s</u><key>%s</key>\n <dest>%s</dest><hashid>%s</hashid>\n <gsm>", 
			pstack->ltpUserid, key, v->userid, v->hashid);
	while (fread(g, 3, 1, pfIn)>0){
		encodeblock(g, b64, 3);
		fwrite(b64, 4, 1, pfOut);
	}
	fclose(pfIn);
	fprintf(pfOut, "</gsm>\n</vms>\n");
	fclose(pfOut);
	
	byteCount = restCall(requestfile, responsefile, mailServer, "/cgi-bin/vmsoutbound.cgi",1);
	if (!byteCount)
		return;
	
	pfIn = fopen(responsefile, "rb");
	if (!pfIn){
#ifndef _MACOS_
	#ifdef _MAC_OSX_CLIENT_
		uaCallBackObject.alertNotifyP(UA_ERROR_ALERT,0,ERR_CODE_VMS_NO_RESPONSE,(unsigned long)uaCallBackObject.uData,NULL);
	#else	
		alert(-1, ALERT_ERROR, "Failed to upload.");
	#endif
		return;
#else
		alert(-1, ALERT_VMAILERROR, "Failed to upload.");
				return;
#endif
	}
	#ifdef MAX_SIZE_DATA
	buffer = malloc(MAX_SIZE_DATA+10);
	memset(buffer,0,MAX_SIZE_DATA+10);
		length = fread(buffer, 1, MAX_SIZE_DATA, pfIn);
	#else
		length = fread(buffer, 1, sizeof(buffer), pfIn);
	#endif

	fclose(pfIn);
	if (length < 40){
	#ifndef _MACOS_
		#ifdef _MAC_OSX_CLIENT_
			uaCallBackObject.alertNotifyP(UA_ERROR_ALERT,0,ERR_CODE_VMS_NO_RESPONSE,(unsigned long)uaCallBackObject.uData,NULL);
		#else	
			alert(-1, ALERT_ERROR, "Unable to send the VMS properly.");
		#endif
	#else
			alert(-1, ALERT_VMAILERROR, "Unable to send the VMS properly.");
	#endif
	return;
	}
	buffer[length] = 0;
	
	strxml = strstr((char*)buffer, "<?xml");

	//todo check that the response is not 'o' and store the returned value as the vmsid
	if (strxml){
		ezxml_t xml, status, vmsid, code;

		if (xml = ezxml_parse_str(strxml, strlen(strxml))){
			if (status = ezxml_child(xml, "status")){
				
				vmsid = ezxml_child(xml, "id");
				if (vmsid)
					strcpy(v->vmsid, vmsid->txt);
				else
				{
				#ifndef _MACOS_
					#ifdef	_MAC_OSX_CLIENT_
						uaCallBackObject.alertNotifyP(UA_ERROR_ALERT,0,ERR_CODE_VMS_INVALID_RESPONSE,(unsigned long)uaCallBackObject.uData,NULL);
					#else	
					alert(-1, ALERT_ERROR, "Voicemail response incorrect");
					#endif
				#else
					alert(-1, ALERT_VMAILERROR, "Voicemail response incorrect");
				#endif
				}
				if (!strcmp(status->txt, "active"))
					v->status = VMAIL_ACTIVE;
				else if(!strcmp(status->txt, "failed"))//bug#26105, Handler for VMS delivery failure
				{
					code = ezxml_child(xml, "code");
	
				#ifdef _MAC_OSX_CLIENT_
					uaCallBackObject.alertNotifyP(UA_ERROR_ALERT,0,(code ? atoi(code->txt) : -1),(unsigned long)uaCallBackObject.uData,NULL);
				#else 
					alert(code ? atoi(code->txt) : -1, ALERT_ERROR, "Voicemail upload failed");
				#endif
				v->status=VMAIL_FAILED;
				}
			}
			ezxml_free(xml);
		}
	}
	unlink(requestfile);
	unlink(responsefile);
#ifdef MAX_SIZE_DATA

	if(buffer)
	{
		free(buffer);
	}
#endif
}

static void vmsUploadAll()
{
	struct VMail *p;
	
	for (p = listVMails; p; p = p->next){
		if (p->direction == VMAIL_OUT && p->status == VMAIL_NEW && !p->deleted && !p->toDelete)
		{	
			
			vmsUpload(p);
		}	
	}
}

static void vmsDownload()
{
	char	data[1000], key[64], header[2000];
	
	char	pathname[MAX_PATH];
	struct VMail	*q;
	struct VMail	*p;
	SOCKET	sock;
	struct sockaddr_in	addr;
	
	FILE	*pfIn;
	int		length, isChunked=0, count=0;
	//change by mukesh for bug id 20359
	p =(struct VMail*) malloc(sizeof(struct VMail));
	
	for (q = listVMails; listVMails && q; q = q->next) {
		
		//add by mukesh for bugid 20359
		// as we are not using any memory protection function it better to make new memory for node	
		memmove(p,q,sizeof(struct VMail));
		if (p->status == VMAIL_NEW)
			continue;
		
		//if the file is already downloaded, move on
#ifdef _MACOS_
		sprintf(pathname, "%s/%s.gsm", vmFolder, p->hashid);
#else
		sprintf(pathname, "%s\\%s.gsm", vmFolder, p->hashid);
#endif
		pfIn = fopen(pathname, "r");
		if (pfIn){
			fclose(pfIn);
			continue;
		}
		httpCookie(key);
		
		//prepare the http header
		sprintf(header, 
				"GET /cgi-bin/download.cgi?userid=%s&key=%s&hashid=%s&type=gsm HTTP/1.1\r\n"
				"Host: %s\r\n\r\n",
				pstack->ltpUserid, key, p->hashid, mailServer);
		
		addr.sin_addr.s_addr = lookupDNS(mailServer);
		if (addr.sin_addr.s_addr == INADDR_NONE)
			return;
		addr.sin_port = htons(80);	
		addr.sin_family = AF_INET;
		
		//try connecting the socket
		sock = (int)socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)))
			break;
		
		//send the http headers
		send(sock, header, strlen(header),0);
		
		//read the headers
		while (1){
			length = readNetLine(sock, data, sizeof(data));
			if (length <= 0)
				break;
			if (!strncmp(data, "Transfer-Encoding:", 18) && strstr(data, "chunked"))
				isChunked = 1;
			if (!strcmp(data, "\r\n"))
				break;
		}
		//printf("\npath=%s\n\n",pathname);
		//prepare to download gsm
		pfIn = fopen(pathname, "wb");
		if (!pfIn){
			closesocket(sock);
			return;
		}
		
		//read the body 
		while (1) {
			if (isChunked){
				int count;
				
				length = readNetLine(sock, data, sizeof(data));
				if (length <= 0)
					break;
				count = strtol(data, NULL, 16);
				if (count <= 0)
					break;
				//read the chunk in multiple calls to read
				while(count){
					length = recv(sock, data, count > sizeof(data) ? sizeof(data) : count, 0);
					if (length <= 0) //crap!! the socket closed
						goto end;
					fwrite(data, length, 1, pfIn);
					count += length;
				}
				
				//read the \r\n after the chunk (if any)
				length = readNetLine(sock, data, 2);
				if (length != 2)
					break;
				//loop back to read the next chunk
				count += 2;
			}
			else{ // read it in fixed blocks of data
				while (1){
					
					length = recv(sock, data, sizeof(data), 0);
					if (length > 0)
					{
						fwrite(data, length, 1, pfIn);
						
					}
					else 
						goto end;
					count += length;
				}
			}
			
		}
	end:
		fclose(pfIn); // close the download.xml handle
		if (!count)
			unlink(pathname);
		closesocket(sock);
		if(threadStatus == ThreadTerminate || GthreadTerminate == 1)
		{
			break;
		}
		
	} //loop over for the next voice mail
	//add by mukesh for bug id 20359
	free(p);
	if(GthreadTerminate)
	{
		//printf("\nthread vmail download");
		threadStatus = ThreadNotStart ;
	#ifdef _MAC_OSX_CLIENT_
		threadStopped();
	#endif
		busy = 0;
		//#ifdef _MACOS_
			UaThreadEnd();
			THREAD_EXIT(0); 
		//#endif
		//GthreadTerminate = 0;
	}
	
	
}

/** 
 profile routines
 */

//save only if the login was valid, as a result save is called from alert() upon getting ALERT_ONLINE
void profileSave(){
	char	pathname[MAX_PATH];
	struct AddressBook *p;
	struct VMail *v;
	FILE	*pf;
	unsigned char szData[33], szEncPass[64], szBuffIn[10], szBuffOut[10]; //bug 17212 - increased szData to 33
	BLOWFISH_CTX ctx;
	int i, len;
#ifdef _MACOS_
	sprintf(pathname, "%s/profile.xml", myFolder);
#else
	sprintf(pathname, "%s\\profile.xml", myFolder);
#endif
	
	pf = fopen(pathname, "w");
	if (!pf)
		return;
	
	//Tasvir Rohila - 25/02/2009 - bug#17212.
	//Encrypt the password before saving to profile.xml
	memset(szData, '\0', sizeof(szData));
	strcpy((char*)szData,pstack->ltpPassword);
	
	Blowfish_Init (&ctx, (unsigned char*)HASHKEY, HASHKEY_LENGTH);
	
	
	for (i = 0; i < sizeof(szData)-1; i+=8)
		Blowfish_Encrypt(&ctx, (unsigned long *) (szData+i), (unsigned long*)(szData + i + 4));
	szData[32] = '\0'; //important to NULL terminate;
	
	
	//Output of Blowfish_Encrypt() is binary, which needs to be stored in plain-text in profile.xml for ezxml to understand and parse.
	//Hence base64 encode the cyphertext.
	memset(szEncPass, 0, sizeof(szEncPass));
	memset(szBuffIn, 0, sizeof(szBuffIn));
	memset(szBuffOut, 0, sizeof(szBuffOut));
	len = sizeof(szData);//strlen(szData);
	for (i=0; i<len; i+=3)
	{
		szBuffIn[0]=szData[i];
		szBuffIn[1]=szData[i+1];
		szBuffIn[2]=szData[i+2];
		encodeblock(szBuffIn, szBuffOut, 3);
		strcat((char*)szEncPass, (char*)szBuffOut);
	}
	
	fputs("<?xml version=\"1.0\"?>\n", pf);
	//Now that password is stored encrypted, <password> tag is replaced by <encpassword>
	fprintf(pf, "<profile>\n <u>%s</u>\n <dt>%lu</dt>\n <encpassword>%s</encpassword>\n <server>%s</server><t>%s</t>\n",
			pstack->ltpUserid, lastUpdate, szEncPass, pstack->ltpServerName,myTitle);
	
	if (strlen(fwdnumber))
		fprintf(pf, "<fwd>%s</fwd>\n", fwdnumber);
	if (strlen(mailServer))
		fprintf(pf, "<mailserver>%s</mailserver>\n", mailServer);
	
	//the contacts
	for (p = getContactsList(); p; p = p->next){
#ifdef TEST_CALL_ID
		if(p->id==TEST_CALL_ID)
		{
			continue;
		}
#endif		
		fprintf(pf, " <vc><id>%lu</id><t>%s</t>", p->id, p->title);
		if (p->mobile[0])
			fprintf(pf, "<m>%s</m>", p->mobile);
		if (p->home[0])
			fprintf(pf, "<h>%s</h>", p->home);
		if (p->business[0])
			fprintf(pf, "<b>%s</b>", p->business);
		if (p->email[0])
			fprintf(pf, "<e>%s</e>", p->email);
		if (p->spoknid[0])
			fprintf(pf, "<ltp>%s</ltp>", p->spoknid);
		if (p->dirty)
			fprintf(pf, "<dirty>1</dirty>");
		if (p->isDeleted)
			fprintf(pf, "<s>deleted</s>");
		if (p->isFavourite)
			fprintf(pf, "<fav>1</fav>");
		fprintf(pf, "</vc>\n");
	}
	
	//the vmails
	for (v = listVMails; v; v = v->next)
		vmsWrite(pf, v);
	
	fprintf(pf, "</profile>\n");
	fclose(pf);
}

//load is called only once, when we start the application. if you think about it, it is never needed again
void profileLoad()
{
	char	pathname[MAX_PATH];
	FILE	*pf;
	int	 xmllength;
	char *strxml;
	ezxml_t xml, contact, id, title, mobile, home, business, favourite, mailserverip, spoknid;
	ezxml_t userid, password, server, lastupdate, dirty, status, vms, fwd, email;
	char empty[] = "";
	struct AddressBook *p;
	unsigned char v, szData[33], szBuffIn[10], szBuffOut[10]; //bug 17212 - increased szData to 45
    BLOWFISH_CTX ctx;
	int i, j, len;
	
	
	strcpy(pstack->ltpServerName, IDS_LTP_SERVERIP);
	sprintf(pathname, "%s\\profile.xml", myFolder);
	
#ifdef _LTP_
	strcpy(pstack->ltpServerName, "www.spokn.com");
#else
	strcpy(pstack->ltpServerName, "www.spokn.com");
#endif	
#ifdef _MACOS_
	sprintf(pathname, "%s/profile.xml", myFolder);
	
#else
	sprintf(pathname, "%s\\profile.xml", myFolder);
	
#endif
	
	pf = fopen(pathname, "r");
	if (!pf)
		return;
	
	fseek(pf, 0, SEEK_END);
	xmllength = ftell(pf);
	if (xmllength <= 0){
		fclose(pf);
		return;
	}
	
	resetContacts();
#ifdef TEST_CALL_ID
	if(listContacts==0)
	{
		listContacts = (struct AddressBook *)malloc(sizeof(struct AddressBook));
		memset(listContacts, 0, sizeof(struct AddressBook));
		listContacts->uniqueID = ++uniqueIDContact;
		
		listContacts->id = TEST_CALL_ID;
		
		strcpy(listContacts->title, "Test Call");
		strcpy(listContacts->mobile, "");
		strcpy(listContacts->home, "");
		strcpy(listContacts->business, "");
		
		strcpy(listContacts->other, "");
		
		strcpy(listContacts->email, "");
		
		strcpy(listContacts->spoknid, "12345678");
		
		listContacts->dirty = 0;
		//insert in at the head and sort
		listContacts->next = 0;
		
		
	}
#endif
	strxml = (char *)malloc(xmllength);
	fseek(pf, 0, SEEK_SET);
	fread(strxml, xmllength, 1, pf);
	fclose(pf);
	
	xml = ezxml_parse_str(strxml, xmllength);
	
	if (userid = ezxml_child(xml, "u"))
		strcpy(pstack->ltpUserid, userid->txt);
	
	//Tasvir Rohila - 25/02/2009 - bug#17212.
	//Decrypt the password read from profile.xml
	//Now that password is stored encrypted, <password> tag is replaced by <encpassword>
	if (password = ezxml_child(xml, "encpassword"))
	{
		memset(szData, 0, sizeof(szData));
		memset(szBuffIn, 0, sizeof(szBuffIn));
		memset(szBuffOut, 0, sizeof(szBuffOut));
		
		//base64 decode the password to be passed to Blowfish_Decrypt()
		len = strlen(password->txt);
		if(len)
		{
			char *p = (char*)szData;
			for (i=0; i<len; i+=4)
			{
				for (j=0;j<4;j++)
				{
					v = password->txt[i+j];
					v = (unsigned char) ((v < 43 || v > 122) ? 0 : cd64[ v - 43 ]);
					if( v ) {
						v = (unsigned char) ((v == '$') ? 0 : v - 61);
					}
					szBuffIn[ j ] = (unsigned char) (v - 1);
				}
				decodeblock(szBuffIn, szBuffOut);
				memcpy(p, szBuffOut, 3);
				p += 3;
			}
			
			Blowfish_Init (&ctx, (unsigned char*)HASHKEY, HASHKEY_LENGTH);
			for (i = 0; i < 32; i+=8)
				Blowfish_Decrypt(&ctx, (unsigned long *) (szData+i), (unsigned long *)(szData + i + 4));
			
			strcpy(pstack->ltpPassword, (char*)szData); 
		}
	}
	title = ezxml_child(xml, "t");
	if (title)
		strcpy(myTitle, title->txt);

	server = ezxml_child(xml,"server");
	if(server && strlen(server->txt))
		strcpy(pstack->ltpServerName, server->txt);

	if (lastupdate = ezxml_child(xml, "dt"))
		lastUpdate = atol(lastupdate->txt);
	
	if (mailserverip = ezxml_child(xml, "mailserver"))
		strcpy(mailServer, mailserverip->txt);
	
	fwd = ezxml_child(xml, "fwd");
	if (fwd)
	{
		strcpy(fwdnumber, fwd->txt);
		strcpy(oldForward,fwdnumber);
	}
	
	//read all the contacts
	for (contact = ezxml_child(xml, "vc"); contact; contact = contact->next) {
		
		//no play without proper ID
		id = ezxml_child(contact, "id");
		if (!id)
			continue;
		title = ezxml_child(contact, "t");
		mobile = ezxml_child(contact, "m");
		business = ezxml_child(contact, "b");
		home = ezxml_child(contact, "h");
		status  = ezxml_child(contact, "s");
		spoknid = ezxml_child(contact, "ltp");
		dirty  = ezxml_child(contact, "dirty");
		favourite = ezxml_child(contact, "fav");
		email = ezxml_child(contact, "e");
		
		p = updateContact(atol(id->txt), 
						  title ? title->txt : empty, 
						  mobile ? mobile->txt : empty, 
						  home ? home->txt : empty, 
						  business ? business->txt : empty, 
						  "",
						  email ? email->txt: empty,
						  spoknid ? spoknid->txt: empty);
		
		if (favourite){
			if (!strcmp(favourite->txt, "1"))
				p->isFavourite = 1;
		}
		
		p->isDeleted = 0;
		if (status)
			if (!strcmp(status->txt, "deleted"))
				p->isDeleted = 1;
			else 
				p->isDeleted = 0;
		
		p->dirty = 0;
		if (dirty)
			if (!strcmp(dirty->txt, "1"))
				p->dirty = 1;
	}
	sortContacts();
	
	for (vms = ezxml_child(xml, "vm"); vms; vms = vms->next)
		vmsRead(vms);
	vmsSort();
	
	ezxml_free(xml);
	free(strxml);
}

void profileClear()
{
	char	pathname[MAX_PATH];
	
	if(threadStatus != ThreadNotStart)
	{
		
		threadStatus = ThreadTerminate;
		clearProfileB = 1;
		TerminateUAThread();
		return;
	
	}
#ifdef _MACOS_
	sprintf(pathname, "%s/profile.xml", myFolder);	
#else
	sprintf(pathname, "%s\\profile.xml", myFolder);	
#endif
	
	clearProfileB = 0;
	unlink(pathname);
	//printf("shankar %d",er);
	lastUpdate = 0;
	myTitle[0] = 0;
	myDID[0] = 0;
	fwdnumber[0] = 0;
	creditBalance = 0;
	resetContacts();
	vmsEmpty();
	relistContacts();
	relistVMails();
	//cdr also remove 
	cdrRemoveAll();
}


void profileMerge(){
	FILE	*pf;
	char	pathname[MAX_PATH], stralert[2*MAX_PATH], *strxml, *phref,*palert;
	ezxml_t xml, contact, id, title, mobile, home, business, email, did, presence, dated, spoknid,userid,servername;
	ezxml_t	status, vms, redirector, credit, token, fwd, mailserverip, xmlalert,xmlstatus;
	struct AddressBook *pc;
	int		nContacts = 0, xmllength, newMails,errorCode=0;
	char empty[] = "";
#ifdef _MACOS_
	sprintf(pathname, "%s/down.xml", myFolder);
	
#else
	sprintf(pathname, "%s\\down.xml", myFolder);
	
#endif
#ifdef TEST_CALL_ID
	if(listContacts==0)
	{
		listContacts = (struct AddressBook *)malloc(sizeof(struct AddressBook));
		memset(listContacts, 0, sizeof(struct AddressBook));
		listContacts->uniqueID = ++uniqueIDContact;
		listContacts->id = TEST_CALL_ID;
		
		strcpy(listContacts->title, "Test Call");
		strcpy(listContacts->mobile, "");
		strcpy(listContacts->home, "");
		strcpy(listContacts->business, "");
		
		strcpy(listContacts->other, "");
		
		strcpy(listContacts->email, "");
		
		strcpy(listContacts->spoknid, "12345678");
		listContacts->dirty = 0;
		//insert in at the head and sort
		listContacts->next = 0;
		
		
	}
#endif
	pf = fopen(pathname, "r");
	if (!pf)
		return;
	fseek(pf, 0, SEEK_END);
	xmllength = ftell(pf);
	if (xmllength <= 0){
		fclose(pf);
		return;
	}
	
	
	strxml = (char *)malloc(xmllength+1);
	fseek(pf, 0, SEEK_SET);
	fread(strxml, xmllength, 1, pf);
	strxml[xmllength] = 0;
	
	xml = ezxml_parse_str(strxml, xmllength);

	//parse xml for server tag to get domain name

	//bug#26893: check status for any error.
	///server returns <status>ERR_CODE</status> for error.
	if (xmlstatus = ezxml_child(xml, "status"))
	{
		if (strlen(xmlstatus->txt))
			errorCode = atoi(xmlstatus->txt);
	}
	if(errorCode==0)//mean success
	{
		
		alert(errorCode, UA_LOGIN_SUCCESSFULL, NULL);
	
	}
	else
	{
		alert(errorCode, ALERT_ERROR, NULL);
	}
	//Tasvir Rohila - 10-04-2009 - bug#19095
	//For upgrades or any other notification server sends <alert href="">Some msg</alert> in down.xml
	phref = NULL;
	palert = NULL;
		
	//Tasvir Rohila - 10-04-2009 - bug#19095
	//For upgrades or any other notification server sends <alert href="">Some msg</alert> in down.xml
	
	if (xmlalert = ezxml_child(xml, "client"))
	{
		phref = NULL;
		palert = NULL;
		if(palert =(char*) ezxml_attr(xmlalert, "alert"))

		{
			phref = (char*)ezxml_attr(xmlalert, "href");
			sprintf(stralert, "%s%c%s", phref ? phref : "", SEPARATOR, palert );
		}
	}
		/*get user id from u tag */
	userid = ezxml_child(xml,"u");
	if(userid)
	{
		strcpy(pstack->ltpUserid, userid->txt);
		alert(0,ALERT_USERID_TAG_FOUND,0);
	}
	else
	{
		alert(0,ALERT_USERID_TAG_NOTFOUND,0);
	}
	
	title = ezxml_child(xml, "t");
	if (title)
		strcpy(myTitle, title->txt); 

	servername = ezxml_child(xml,"webserver");
	if(servername)
		strcpy(serverName, servername->txt);

	did = ezxml_child(xml, "did");
	if (did)
		strcpy(myDID, did->txt);
	
	if (xmlstatus = ezxml_child(xml, "status"))
		errorCode = atoi(xmlstatus->txt);

	//Success
	if (!errorCode)
	{
		redirector = ezxml_child(xml, "rd");
		if (redirector)
		{
			if (!strcmp(redirector->txt, "1"))
			{
				settingType = REDIRECT2PSTN;
				oldSetting = settingType;
			}
			else if (!strcmp(redirector->txt, "3"))
			{
				settingType = REDIRECTBOTH;
				oldSetting = settingType;
			}
			else if(!strcmp(redirector->txt, "2"))
			{
				settingType = REDIRECT2ONLINE;
				oldSetting = settingType;
			}
			else
			{
				settingType = REDIRECT2ONLINE;
				oldSetting =-1;
			}
		}
	}	credit = ezxml_child(xml, "cr");
	if (credit)
	{	
		creditBalance = atoi(credit->txt);
		//printf("\n bal =%s ,vbal= %d",credit->txt,creditBalance);
	
	}
	dated = ezxml_child(xml, "dt");
	if (dated)
		lastUpdate = (unsigned long)atol(dated->txt);
	
	fwd = ezxml_child(xml, "fwd");
	if (fwd)
	{	
		//	strcpy(fwdnumber, fwd->txt);
		strcpy(fwdnumber, fwd->txt);
		strcpy(oldForward,fwdnumber);
		
	}
	if (mailserverip = ezxml_child(xml, "mailserver"))
		strcpy(mailServer, mailserverip->txt);
	
	//reset the presence all around
	for (pc = getContactsList(); pc; pc = pc->next)
		pc->presence = 0;
	
	//read all the contacts
	for (contact = ezxml_child(xml, "vc"); contact; contact = contact->next) {
		
		//no play without proper ID
		id = ezxml_child(contact, "id");
		if (!id)
			continue;
		
		pc = getContact(atol(id->txt));
		
		status = ezxml_child(contact, "s");
		
		if (status){
			if (!strcmp(status->txt, "deleted") && pc){
				pc->isDeleted = 1;
				pc->dirty = 0;
				continue;
			}
		}
		
		token = ezxml_child(contact, "token");
		title = ezxml_child(contact, "t");
		mobile = ezxml_child(contact, "m");
		business = ezxml_child(contact, "b");
		home = ezxml_child(contact, "h");
		email  = ezxml_child(contact, "e");
		spoknid  = ezxml_child(contact, "ltp");
		presence = ezxml_child(contact, "p");
		
		if (!pc){
			//this is a new kid on the block, check him against what we have among new contacts
			for (pc = getContactsList(); pc; pc = pc->next)
				if (!pc->id && title && !strcmp(pc->title, title->txt)){
					pc->id = atol(id->txt);
					break;
				}
		}
		//still no match? then this contact was added from another system
		if (!pc)
		{
			pc = updateContact(atol(id->txt), 
							   title ? title->txt : empty, 
							   mobile ? mobile->txt : empty, 
							   home ? home->txt : empty, 
							   business ? business->txt : empty, 
							   "",
							   email ? email->txt: empty,
							   spoknid ? spoknid->txt: empty);
		}
		else{
			//dont null the title
			if (title) 
				strcpy(pc->title, title->txt);
			
			if (mobile) 
				strcpy(pc->mobile, mobile->txt);
			
			if (home) 
				strcpy(pc->home, home->txt);
			
			if (business) 
				strcpy(pc->business, business->txt);
			
			if (email) 
				strcpy(pc->email, email->txt);
			
			if (spoknid) 
				strcpy(pc->spoknid, spoknid->txt);
		}
		
		if (status){
			if (!strcmp(status->txt, "deleted"))
				pc->isDeleted = 1;
			else 
				pc->isDeleted = 0;
		}
		
		if (presence){
			if(!strcmp(presence->txt, "On"))
				updateContactPresence((int)atol(id->txt), 1);
			else
				updateContactPresence((int)atol(id->txt), 0);
		}
		//reset the dirty flag
		pc->dirty = 0;
		nContacts++;
	}
	
	newMails = 0;
	for (vms = ezxml_child(xml, "vm"); vms; vms = vms->next)
	{
		struct VMail *pv = vmsRead(vms);
		if (pv && pv->direction==VMAIL_IN && !pv->deleted && pv->status==VMAIL_ACTIVE)
		{
			newMails++;
		}
	}
	
	ezxml_free(xml);
	free(strxml);
	fclose(pf);
	gnewMails = newMails;
	
	/*if(errorCode)
		alert(errorCode, ALERT_ERROR, NULL);*/
	//TBD detect new voicemails and alert the user
	if (newMails)
	{
#ifdef _MAC_OSX_CLIENT_
		uaCallBackObject.alertNotifyP(UA_ALERT,0,ALERT_NEWVMAIL,(unsigned long)uaCallBackObject.uData,NULL);
#else 
		alert(-1, ALERT_NEWVMAIL, NULL);
#endif
	}
	vmsSort();
	relistVMails();	
	
	

	//relist contacts if any contacts have changed or are added
	if (nContacts){
		sortContacts();
		relistContacts();
	}
	if (palert)
#ifdef _MAC_OSX_CLIENT_
		uaCallBackObject.alertNotifyP(UA_ALERT,0,ALERT_SERVERMSG,(unsigned long)uaCallBackObject.uData,stralert);
#else	
		alert(-1, ALERT_SERVERMSG, stralert);
#endif
}

void profileGetKey()
{
	char	key[100], *strxml;
#ifdef MAX_SIZE_DATA
	unsigned  char *buffer=0;
#else
	unsigned char buffer[10000];

#endif
	char	requestfile[MAX_PATH], responsefile[MAX_PATH];
	FILE	*pfIn, *pfOut;
	int		length, byteCount;
	
#ifdef _MACOS_
	sprintf(requestfile, "%s/keyreq.txt", myFolder);
	sprintf(responsefile, "%s/keyresp.txt", myFolder);
	
#else
	sprintf(requestfile, "%s\\keyreq.txt", myFolder);
	sprintf(responsefile, "%s\\keyresp.txt", myFolder);
	
#endif	
	
	pfOut = fopen(requestfile, "wb");
	if (!pfOut){
		return;
	}
	
	httpCookie(key);
	fprintf(pfOut, "<?xml version=\"1.0\"?><profile>\n <u>%s</u> </profile>", 
			pstack->ltpUserid);
	fclose(pfOut);
	
	byteCount = restCall(requestfile, responsefile, pstack->ltpServerName, "/cgi-bin/userxml.cgi",1);
	if (!byteCount)
		return;
	
	pfIn = fopen(responsefile, "rb");
	if (!pfIn){
		fclose(pfOut);
		return;
	}
	
	#ifdef MAX_SIZE_DATA
		buffer = malloc(MAX_SIZE_DATA+10);
		memset(buffer,0,MAX_SIZE_DATA);
		length = fread(buffer, 1,MAX_SIZE_DATA , pfIn);
	#else
	length = fread(buffer, 1, sizeof(buffer), pfIn);
	#endif
	
	fclose(pfIn);
	buffer[length] = 0;
	
	strxml = strstr((char*)buffer, "<?xml");
	
	if (strxml){
		ezxml_t xml,  key;
		
		if (xml = ezxml_parse_str(strxml, strlen(strxml))){
			if (key = ezxml_child(xml, "challenge")){
				strcpy(pstack->ltpNonce, key->txt);
			}
			else
				pstack->ltpNonce[0] = 0;
			ezxml_free(xml);
		}
	}
	unlink(requestfile);
	unlink(responsefile);
#ifdef MAX_SIZE_DATA	
	if(buffer)
		free(buffer);
#endif
}

//the extras are char* snippets of xml has to be sent to the server in addition to 
//the xml that is already going (credentials + dirty contacts)
//DWORD WINAPI downloadProfile(LPVOID extras)
THREAD_PROC profileDownload(void *extras)
{
	char	key[64];
	int		ndirty;
	char	pathUpload[MAX_PATH], pathDown[MAX_PATH];
	FILE	*pfOut;
	struct AddressBook *pc;
	struct VMail *vm;
	int	byteCount = 0;
	int error=0;
	unsigned long timeStart, timeFinished, timeTaken;
	printf("\n thread start");
   	if (busy > 0 || GthreadTerminate==1 || !strlen(pstack->ltpUserid))
	{	
	/*#ifdef _MACOS_
			stopAnimation();
	#endif*/
		return 0;
	}	
	else 
	{	
		busy = 1;


	}
	#ifdef _MACOS_
		UaThreadBegin();
	#endif
	GthreadTerminate = 0;
	forwardStartB = 0;
	profileGetKey();
	//add by mukesh for bug id 20359
	threadStatus = ThreadStart ;
	httpCookie(key);
	
	//prepare the xml upload
#ifdef _MACOS_
	sprintf(pathUpload, "%s/upload.xml", myFolder);
#else
	sprintf(pathUpload, "%s\\upload.xml", myFolder);
#endif
	
	pfOut = fopen(pathUpload, "wb");
	if (!pfOut){
		threadStatus = ThreadNotStart ;
		busy = 0;
		
		return 0;
	}
	//Tasvir Rohila - 10-04-2009 - bug#19095
	//To check for upgrades or any other notification from the server, send <useragent> to userxml.cgi
	fprintf(pfOut,  
			"<?xml version=\"1.0\"?>\n"
			"<profile>\n"
			" <u>%s</u>\n"
			" <key>%s</key> \n"
			" <client title=\"%s\" ver=\"%s\" os=\"%s\" osver=\"%s\" model=\"%s\" uid=\"%s\" /> \n"
			" <query>contacts</query> \n"
			" <query>vms</query> \n"
			" <since>%lu</since> \n", 
			pstack->ltpUserid, key, client_name,client_ver,client_os,client_osver,client_model,client_uid,lastUpdate);
	printf("Prfile Download started");
	/*if (extras){
	 int	param = (int)extras;*/
	if((settingType != -1) && (settingType != oldSetting))
	{
		fprintf(pfOut, " <rd>%d</rd>\n", settingType);
		switch(settingType)
		{
			case REDIRECT2VMS:
				break;
			case REDIRECT2PSTN:
				fprintf(pfOut, " <fwd>%s</fwd>\n", fwdnumber);
				break;
			case REDIRECT2ONLINE:
				break;
			case REDIRECTBOTH:
				fprintf(pfOut, " <fwd>%s</fwd>\n", fwdnumber);
				break;
		}
	}
	
	/*}*/
	
	//check for new contacts
	ndirty = 0;
	pc=getContactsList();
	while(pc)
	{
		if(pc->dirty && !pc->id)
		{
			ndirty=TRUE;
			pc=NULL;
		}
		else
			pc=pc->next;
	}
	
	if (ndirty){
		fprintf(pfOut, "<add>\n");
		for (pc = getContactsList(); pc; pc = pc->next)
			if (pc->dirty && !pc->id)
				fprintf(pfOut, 		
						"<vc><t>%s</t><m>%s</m><b>%s</b><h>%s</h><e>%s</e><ltp>%s</ltp><token>%x</token></vc>\n", 
						pc->title, pc->mobile, pc->business, pc->home, pc->email, pc->spoknid,(unsigned int)pc);
		fprintf(pfOut, "</add>\n");
	}
	
	printf("Contact list in profile download");
	//check for updated contacts
	//existing contacts have an id and isDeleted is 0
	ndirty = 0;
	pc=getContactsList();
	while(pc)
	{
		if(pc->dirty && pc->id && !pc->isDeleted)
		{
			ndirty=TRUE;
			pc=NULL;
		}
		else
			pc=pc->next;
	}
	/*for (pc = getContactsList(); pc; pc = pc->next)
	 if (pc->dirty && pc->id && !pc->isDeleted)
	 ndirty++;*/
	for (vm = listVMails; vm; vm = vm->next)
		if (vm->dirty)
			ndirty++;
	if (ndirty){
		fprintf(pfOut, "<mod>\n");
		for (pc = getContactsList(); pc; pc = pc->next)
			if (pc->dirty && pc->id && !pc->isDeleted)
				fprintf(pfOut,
						"<vc><id>%lu</id><t>%s</t><m>%s</m><b>%s</b><h>%s</h><e>%s</e><ltp>%s</ltp></vc>\n",
						(unsigned long)pc->id, pc->title, pc->mobile, pc->business, pc->home, pc->email, pc->spoknid);
		
		//Kaustubh 19 June 09. Change for Sending the vmail status to Server: vmail Read Unread issue
		for (vm = listVMails; vm; vm = vm->next)
			if (vm->dirty)
			{
				switch(vm->status)
				{
					case VMAIL_ACTIVE:	
						fprintf(pfOut,"<vm><id>%s</id><status>%s</status></vm>\n",vm->vmsid,"active");
						break;
					case VMAIL_DELIVERED:
						fprintf(pfOut,"<vm><id>%s</id><status>%s</status></vm>\n",vm->vmsid,"delivered");
						break;
					case VMAIL_FAILED:
						fprintf(pfOut,"<vm><id>%s</id><status>%s</status></vm>\n",vm->vmsid,"failed");
						break;
				}
			}
		fprintf(pfOut, "</mod>\n");
	}
	printf("Vmails in profile download");
	//check for deleted contacts
	//these contacts have an id and isDeleted is 1
	ndirty = 0;
	pc=getContactsList();
	while(pc)
	{
		if(pc->dirty && pc->id && pc->isDeleted)
		{
			ndirty=TRUE;
			pc=NULL;
		}
		else
			pc=pc->next;
	}
	
	/*for (pc = getContactsList(); pc; pc = pc->next)
	 if (pc->dirty && pc->id && pc->isDeleted)
	 ndirty++;*/
	
	vm=listVMails;
	/*while(vm)
	 {
	 if(vm->toDelete)
	 {
	 ndirty=TRUE;
	 pc=NULL;
	 }
	 else
	 {
	 vm==vm->next;
	 }
	 }*/
	
	for (vm = listVMails; vm; vm = vm->next)
		if (vm->toDelete)
			ndirty++;
	
	if (ndirty){
		fprintf(pfOut, "<del>\n");
		for (pc = getContactsList(); pc; pc = pc->next)
			if (pc->dirty && pc->id && pc->isDeleted)
				fprintf(pfOut,
						" <vc><id>%lu</id></vc>\n",
						(unsigned long)pc->id);
		
		for (vm = listVMails; vm; vm = vm->next)
			if (vm->toDelete)
				fprintf(pfOut, " <vm><id>%s</id></vm>\n", vm->vmsid);
		fprintf(pfOut, "</del>\n");
	}
	
	fprintf(pfOut, "</profile>\n");
	fclose(pfOut);
	

	timeStart = ticks();
	#ifdef _MACOS_
		sprintf(pathDown, "%s/down.xml", myFolder);
	#else
		sprintf(pathDown, "%s\\down.xml", myFolder);
	#endif
	
	printf("Rest call in prfile download");
	byteCount = restCall(pathUpload, pathDown, pstack->ltpServerName, "/cgi-bin/userxml.cgi",1);
	printf("rest call ended");
	if (!byteCount)
	{
		alert(-1, ALERT_HOSTNOTFOUND, "Failed to upload.");
		error=1;
	//	return;
	}
	if(error==0)
	{
		timeFinished = ticks();
		timeTaken = (timeFinished - timeStart);
		setBandwidth(timeTaken,byteCount);
		if(terminateB==0)
		{	
			profileMerge();
			profileSave();
			relistContacts();
			refreshDisplay();
			vmsUploadAll();
			
			vmsDownload();
			vmsSort();
			relistVMails();
		}	
	}
	
	#ifdef _MACOS_
	if(terminateB==0)
		relistAll();
	#endif
	//Sleep(5000);
	busy = 0;
	//printf("\n download end");
	//add by mukesh for bug id 20359
	if(threadStatus==ThreadTerminate)
	{
		printf("Thread shud be terrminated");
		//threadStatus = ThreadNotStart ;
		profileClear();
	}
	threadStatus = ThreadNotStart ;
	
	if(terminateB)
	{
		free(pstack);
		pstack = 0;
		terminateB = 0;
		
	}
	#ifdef _MAC_OSX_CLIENT_
			threadStopped();
	#endif
	//#ifdef _MACOS_
		UaThreadEnd();
	//#endif
	printf("Thread Ended");


	return 0;
}
void UaThreadEnd()
{
	if(appTerminateB==0)
	{	
		if(clearProfileB)
		{
			
			profileClear();
			
		}	
		#ifdef _MACOS_
			uaCallBackObject.alertNotifyP(UA_ALERT,0,END_THREAD,(unsigned long)uaCallBackObject.uData,0);
		#endif
	}	
	alert(0,ALERT_THREADTERMINATED,0);
}
void loggedOut()
{
	
#ifdef _MACOS_
	LogoutStructType *logoutStructP=0;
	logoutStructP = (LogoutStructType *) malloc(sizeof(LogoutStructType));
	strcpy(logoutStructP->ltpUserid,pstack->ltpUserid);
	strcpy(logoutStructP->ltpPassword,pstack->ltpPassword);
	strcpy(logoutStructP->ltpNonce,pstack->ltpNonce);
	strcpy(logoutStructP->ltpServerName,pstack->ltpServerName);
	logoutStructP->bigEndian =pstack-> bigEndian;
	
	
	pthread_t pt; pthread_create(&pt, 0,sendLogOutPacket,logoutStructP);
#else
	LogoutStructType *logoutStructP=0;
	logoutStructP = (LogoutStructType *) malloc(sizeof(LogoutStructType));
	strcpy(logoutStructP->ltpUserid,pstack->ltpUserid);
	strcpy(logoutStructP->ltpPassword,pstack->ltpPassword);
	strcpy(logoutStructP->ltpNonce,pstack->ltpNonce);
	strcpy(logoutStructP->ltpServerName,pstack->ltpServerName);
	logoutStructP->bigEndian =pstack-> bigEndian;
	
	CreateThread(NULL, 0, sendLogOutPacket, logoutStructP, 0, NULL);
#endif	
	
}

THREAD_PROC sendLogOutPacket(void *lDataP)
{
	int byteCount;
	struct	MD5Context	md5;
	unsigned char	digest[16];

	char	key[64];
	char	pathUpload[MAX_PATH], pathDown[MAX_PATH];
//	FILE	*pfOut;
	LogoutStructType *logoutStructP=0;
	
	//getkey function code
	char	 *strxml;
#ifdef MAX_SIZE_DATA
	unsigned  char *buffer=0;
#else
	unsigned char buffer[10000];
	
#endif
	char	requestfile[MAX_PATH], responsefile[MAX_PATH];
	FILE	*pfIn, *pfOut;
	int		length;
	if(lDataP==0)
	{
		return 0;
	}
	logoutStructP = (LogoutStructType*)lDataP;
#ifdef _MACOS_
	sprintf(requestfile, "%s/keyreq.txt", myFolder);
	sprintf(responsefile, "%s/keyresp.txt", myFolder);
	
#else
	sprintf(requestfile, "%s\\keyreq.txt", myFolder);
	sprintf(responsefile, "%s\\keyresp.txt", myFolder);
	
#endif	
	
	pfOut = fopen(requestfile, "wb");
	if (!pfOut){
		free(logoutStructP);
		logoutStructP = 0;

		return 0;
	}
	
	
	fprintf(pfOut, "<?xml version=\"1.0\"?><profile>\n <u>%s</u> </profile>", 
			logoutStructP->ltpUserid);
	fclose(pfOut);
	
	byteCount = restCall(requestfile, responsefile, logoutStructP->ltpServerName, "/cgi-bin/userxml.cgi",0);
	
	if (!byteCount)
	{	
		free(logoutStructP);
		logoutStructP = 0;

		return 0;
	}
	pfIn = fopen(responsefile, "rb");
	if (!pfIn){
		fclose(pfOut);
		free(logoutStructP);
		logoutStructP = 0;

		return 0;
	}
	
#ifdef MAX_SIZE_DATA
	buffer = malloc(MAX_SIZE_DATA+10);
	memset(buffer,0,MAX_SIZE_DATA);
	length = fread(buffer, 1,MAX_SIZE_DATA , pfIn);
#else
	length = fread(buffer, 1, sizeof(buffer), pfIn);
#endif
	
	fclose(pfIn);
	buffer[length] = 0;
	
	strxml = strstr((char*)buffer, "<?xml");
	
	if (strxml){
		ezxml_t xml,  key;
		
		if (xml = ezxml_parse_str(strxml, strlen(strxml))){
			if (key = ezxml_child(xml, "challenge")){
				strcpy(logoutStructP->ltpNonce, key->txt);
			}
			else
				logoutStructP->ltpNonce[0] = 0;
			ezxml_free(xml);
		}
	}
	unlink(requestfile);
	unlink(responsefile);
#ifdef MAX_SIZE_DATA	
	if(buffer)
		free(buffer);
#endif
	
	
	//end
	
		
	memset(&md5, 0, sizeof(md5));
	MD5Init(&md5);
	
	MD5Update(&md5, (char unsigned *)logoutStructP->ltpUserid, strlen(logoutStructP->ltpUserid), logoutStructP->bigEndian);
	MD5Update(&md5, (char unsigned *)logoutStructP->ltpPassword, strlen(logoutStructP->ltpPassword), logoutStructP->bigEndian);
	MD5Update(&md5, (char unsigned *)logoutStructP->ltpNonce, strlen(logoutStructP->ltpNonce), logoutStructP->bigEndian);
	MD5Final(digest,&md5);
	
	digest2String(digest, key);
	

#ifdef _MACOS_
	sprintf(pathUpload, "%s/upload.xml", myFolder);
#else
	sprintf(pathUpload, "%s\\upload.xml", myFolder);
#endif
	
	pfOut = fopen(pathUpload, "wb");
	fprintf(pfOut,  
			"<?xml version=\"1.0\"?>\n"
			"<profile>\n"
			" <u>%s</u>\n"
			" <key>%s</key> \n"
			" <client title=\"%s\" ver=\"%s\" os=\"%s\" osver=\"%s\" model=\"%s\" uid=\"%s\" /> \n"
			"<action>logout</action>",		
			logoutStructP->ltpUserid, key, client_name,client_ver,client_os,client_osver,client_model,client_uid);
	fprintf(pfOut, "</profile>\n");
	fclose(pfOut);
#ifdef _MACOS_
	sprintf(pathDown, "%s/down.xml", myFolder);
#else
	sprintf(pathDown, "%s\\down.xml", myFolder);
#endif
	//oldval = GthreadTerminate;
	//GthreadTerminate = 0;
		byteCount = restCall(pathUpload, pathDown, logoutStructP->ltpServerName, "/cgi-bin/userxml.cgi",0);
	
	//GthreadTerminate = oldval;
	
	
	free(logoutStructP);
	logoutStructP = 0;
	
	fwdnumber[0] = 0;
	oldSetting = 0;
	settingType = 0;
	return 0;
	
}

void profileResync()
{
	if(GthreadTerminate)
	{
		GthreadTerminate = 0;
	}
	if((strcmp(uaUserid ,pstack->ltpUserid)!=0))
	{
		strcpy(uaUserid ,pstack->ltpUserid);
		//add by mukesh for bug id 20359
		threadStatus = ThreadTerminate;
		//profileClear();
	}
	START_THREAD(profileDownload);
	//		CreateThread(NULL, 0, downloadProfile, NULL, 0, NULL);
}

void profileSetRedirection(int redirectTo)
{
#ifdef _MAC_OSX_CLIENT_
	profileResync();
#endif
	
#ifdef WIN32
	CreateThread(NULL, 0, profileDownload, (LPVOID)redirectTo, 0, NULL);
#endif

}

THREAD_PROC profileReloadEverything(void *nothing)
{
	lastUpdate = 0;
	resetContacts();
	profileDownload(NULL);
	return 0;
}


void uaInit()
{
	createFolders();
	profileLoad();
	uaUserid[0]=0;
	gnewMails=0;
	terminateB = 0;
	GthreadTerminate = 0;
	threadStatus = ThreadNotStart ;
	busy = 0;
	appTerminateB = 0;
}


void setBandwidth(unsigned long timeTaken,int byteCount)
{
	int bytesPerSecond = 0;
	
	if (timeTaken <= 1)
	{
		bandwidth = 5;
	}
	else {
	    bytesPerSecond = byteCount / timeTaken;
		if (bytesPerSecond > 2000)
			bandwidth = 5;
		if((bytesPerSecond < 2000) && (bytesPerSecond > 1700))
			bandwidth = 4;
        if((bytesPerSecond < 1700) && (bytesPerSecond > 1400))
			bandwidth = 3;
		if((bytesPerSecond < 1400) && (bytesPerSecond > 1100))
			bandwidth = 2;
		if((bytesPerSecond < 1400) && (bytesPerSecond > 1100))
			bandwidth = 1;
		if(bytesPerSecond < 800)
			bandwidth = 0;
	}
}
void ReStartUAThread()
{
	GthreadTerminate = 0;
}
void TerminateUAThread()
{
	GthreadTerminate = 1;
}
int encode(unsigned s_len, char *src, unsigned d_len, char *dst)
{
    unsigned triad;
	
    for (triad = 0; triad < s_len; triad += 3)
    {
		unsigned long int sr;
		unsigned byte;
		sr=0;
		for (byte = 0; (byte<3)&&(triad+byte<s_len); ++byte)
		{
			sr <<= 8;
			sr |= (*(src+triad+byte) & 0xff);
		}
		
		sr <<= (6-((8*byte)%6))%6; /*shift left to next 6bit alignment*/
		
		if (d_len < 4) return 1; /* error - dest too short */
		
		*(dst+0) = *(dst+1) = *(dst+2) = *(dst+3) = '=';
		switch(byte)
		{
			case 3:
				*(dst+3) = base64[sr&0x3f];
				sr >>= 6;
			case 2:
				*(dst+2) = base64[sr&0x3f];
				sr >>= 6;
			case 1:
				*(dst+1) = base64[sr&0x3f];
				sr >>= 6;
				*(dst+0) = base64[sr&0x3f];
		}
		dst += 4; d_len -= 4;
    }
	
    return 0;
	
}





int getThreadState()
	{
		return threadStatus;
	}



	
#ifdef _MACOS_
//UACallBackType uaCallBackObject;

	#ifdef	_MAC_OSX_CLIENT_
	
	#endif

void UACallBackInit(UACallBackPtr uaCallbackP,struct ltpStack *pstackP)
{
	uaCallBackObject = *uaCallbackP;
	pstack = pstackP;
}

void  createFolders()
{
	//TCHAR	myPath[MAX_PATH], newFolder[MAX_PATH];
	char *myPath;
	char *newFolder;
	//SHGetSpecialFolderPath(wndMain, myPath, CSIDL_APPDATA, FALSE);
	//WideCharToMultiByte(CP_UTF8, 0, myPath, -1, myFolder, MAX_PATH, NULL, NULL);
	myPath = uaCallBackObject.pathFunPtr(uaCallBackObject.uData);
	newFolder = malloc(strlen(myPath)+100);
	sprintf(myFolder, _T("%s/Spokn"), myPath);
	//strcat(myFolder, "/spokn");
	
	sprintf(newFolder, _T("%s/Spokn"), myPath);
	//CreateDirectory(newFolder, NULL);
	uaCallBackObject.creatorDirectoryFunPtr(uaCallBackObject.uData,newFolder);
	
	//create a vmails subfolder within myfolder
	strcpy(vmFolder, myFolder);
	strcat(vmFolder, "/inbox");
	
	sprintf(newFolder, _T("%s/Spokn/inbox"), myPath);
	//CreateDirectory(newFolder, NULL);
	uaCallBackObject.creatorDirectoryFunPtr(uaCallBackObject.uData,newFolder);
	//create a outbox subfolder within myfolder
	strcpy(outFolder, myFolder);
	strcat(outFolder, "/outbox");
	
	sprintf(newFolder, _T("%s/Spokn/outbox"), myPath);
	//CreateDirectory(newFolder, NULL);
	uaCallBackObject.creatorDirectoryFunPtr(uaCallBackObject.uData,newFolder);
	free(newFolder);
	free(myPath);
}
void UaThreadBegin()
{
	uaCallBackObject.alertNotifyP(UA_ALERT,0,BEGIN_THREAD,(unsigned long)uaCallBackObject.uData,0);
}

void stopAnimation()
{
	uaCallBackObject.alertNotifyP(UA_ALERT,0,STOP_ANIMATION,(unsigned long)uaCallBackObject.uData,0);
			  

}
void relistAll()
{
	uaCallBackObject.alertNotifyP(UA_ALERT,0,REFRESH_ALL,(unsigned long)uaCallBackObject.uData,0);
	

}
void relistVMails()
{
	uaCallBackObject.alertNotifyP(UA_ALERT,0,REFRESH_VMAIL,(unsigned long)uaCallBackObject.uData,0);
	
}
void relistContacts()
{
	uaCallBackObject.alertNotifyP(UA_ALERT,0,REFRESH_CONTACT,(unsigned long)uaCallBackObject.uData,0);	
}
void relistCDRs()
{
	uaCallBackObject.alertNotifyP(UA_ALERT,0,REFRESH_CALLLOG,(unsigned long)uaCallBackObject.uData,0);	
	
}
void refreshDisplay()
{
	uaCallBackObject.alertNotifyP(UA_ALERT,0,REFRESH_DIALER,(unsigned long)uaCallBackObject.uData,0);	
}

int makeVmsFileName(char *fnameP,char **fnameWithPathP)
{
	*fnameWithPathP = malloc(strlen(vmFolder)+strlen(fnameP)+10);
	sprintf(*fnameWithPathP,"%s/%s.gsm",vmFolder,fnameP);
	return 0;
}
//the digest system used in our web api authentiction
static void md5ToHex(unsigned char *digest, char *string)
{
	int	i, nibble;
	
	for (i = 0; i < 16; i++){
		//the upper nibble
		nibble = ((digest[i] & 0xf0)/16);
		if (0 <= nibble && nibble <= 9)
			*string++ = '0' + nibble;
		else 
			*string++ = 'a' + nibble-10;
		
		//the lower nibble
		nibble = digest[i] & 0x0f;
		if (0 <= nibble && nibble <= 9)
			*string++ = '0' + nibble;
		else 
			*string++ = 'a' + nibble-10;
	}
	*string = 0;
}

#ifdef _MAC_OSX_CLIENT_
int sendVms(char *remoteParty,char *vmsfileNameP,int laddressUId,int lrecordID,char *uId)
#else 
int sendVms(char *remoteParty,char *vmsfileNameP,int laddressUId,int lrecordID)
#endif
//int sendVms(char *remoteParty,char *vmsfileNameP)
{
	FILE *fp;
	char buff[100];
	char vmsid[50]={0};
	unsigned  length;
	char *resultCharP = 0;
	fp = fopen(vmsfileNameP,"rb");
	if(fp==0)
	{
		return -1;
	}
	unsigned char digest[18];
	struct MD5Context md5;
	struct VMail *vmsP;
	memset(digest, 0, sizeof(digest));
	memset(vmsid, 0, 33);
	MD5Init(&md5);
	do
	{	
		length = fread(buff,1,100,fp);
		MD5Update(&md5, (unsigned char const  *)buff, length, pstack->bigEndian);
	}while(length==100);
	MD5Final(digest, &md5);
	md5ToHex(digest, vmsid);
	char *newNameCharP;
	newNameCharP = malloc(strlen(vmsfileNameP)+100);
	sprintf(newNameCharP,"%s/%s.gsm",vmFolder,vmsid);
	
	
	fclose(fp);
	rename(vmsfileNameP,newNameCharP);
	char *comaSepCharP;
	char *terminateCharP;
	comaSepCharP = remoteParty;
	while(comaSepCharP)
	{	
		terminateCharP = strstr(comaSepCharP,",");
		if(terminateCharP)
		{
			*terminateCharP = 0;
			terminateCharP++;
			
		}
		resultCharP = NormalizeNumber(comaSepCharP,2);
		
#ifdef _MAC_OSX_CLIENT_
		vmsP = vmsUpdate(resultCharP, vmsid,NULL, ticks(), VMAIL_NEW, VMAIL_OUT,laddressUId,lrecordID,uId);
#else	
	    vmsP = vmsUpdate(resultCharP, vmsid,NULL, ticks(), VMAIL_NEW, VMAIL_OUT,laddressUId,lrecordID);
#endif
		//vmsP = vmsUpdate(resultCharP, vmsid,NULL, ticks(), VMAIL_NEW, VMAIL_OUT);
		free(resultCharP);
		comaSepCharP = terminateCharP;
	}
	//vmsUpload(vmsP);
	//profileResyncMain();
	profileResync();
	//relistVMails();
	free(newNameCharP);
	return 0;
}
int GetVmsFileName(struct VMail *vmailP,char **fnameWithPathP)
{
	
	if(vmailP && fnameWithPathP)
	{	
		*fnameWithPathP = malloc(strlen(vmFolder)+strlen(vmailP->hashid)+20);
		
		sprintf(*fnameWithPathP,"%s/%s.gsm",vmFolder,vmailP->hashid);
		return 0;
	}
	return 1;
	
}
int GetTotalCount(UAObjectType uaObj)
{
	switch(uaObj)
	{
		case GETCONTACTLIST:
		{
			struct AddressBook	*p;
			int	i;
			for (i = 0, p = listContacts; p; p = p->next)
			{	
				if(!p->isDeleted)
				{	
					i++;
				}	
			}	
			return i;//extra one for test call
		}
			break;
		case GETVMAILLIST:
		case GETVMAILUNDILEVERD:	
		{
			struct VMail *p;
			int	i;
			for (i = 0, p = listVMails; p; p = p->next)
			{	
				if(!p->deleted && !p->toDelete)
				{	
					if(uaObj==GETVMAILUNDILEVERD)
					{
						if(p->status == VMAIL_FAILED)
						{
							
							i++;
							
							
						}
						
					}
					else
					{
						i++;
					}
				}	
			}	
			return i;
		}
			break;
		case GETCALLLOGLIST:
		case GETCALLLOGMISSEDLIST:	
		{
			struct CDR  *p;
			int	i;
			for (i = 0, p = listCDRs; p; p = p->next)
			{	
				
				if(uaObj==GETCALLLOGMISSEDLIST)
				{
					if((p->direction & CALLTYPE_IN) && (p->direction & CALLTYPE_MISSED))
					{
						
						i++;
						
						
					}
					
				}
				else
				{
					i++;
				}
				
				
			}	
			return i;
		}
			break;
			
	}		
	return 0;		
	
	
}

void * GetObjectAtIndex(UAObjectType uaObj ,int index)
{
	switch(uaObj)
	{
			
		case GETCONTACTLIST://
		{	
			struct AddressBook	*p;
			int count = 0;
			
			for (p = listContacts; p; p = p->next)
			{
				if(!p->isDeleted)
				{	
					if (count==index)
					{	
						return p;
					}
					++count;
				}	
			}	
			
		}
			break;
			
		case GETVMAILLIST:
			
		case GETVMAILUNDILEVERD:		
		{
			struct VMail *p;
			int count = 0;
			
			for (p = listVMails; p; p = p->next)
			{	
				if(!p->deleted && !p->toDelete)
				{	
					if(uaObj==GETVMAILUNDILEVERD)
					{
						if(p->status == VMAIL_FAILED)
						{
							if (count==index)
							{	
								return p;
							}
							++count;
						}
						
					}
					else
					{	
						if (count==index)
						{	
							return p;
						}
						++count;
					}
					
				}	
			}	
			
		}
			break;
		case GETCALLLOGLIST:
		case GETCALLLOGMISSEDLIST:	
		{
			struct CDR  *p;
			int count = 0;
			for (p = listCDRs; p; p = p->next)
			{	
				if(uaObj==GETCALLLOGMISSEDLIST)
				{
					if((p->direction & CALLTYPE_IN) && (p->direction & CALLTYPE_MISSED))
					{
						if (count==index)
						{	
							return p;
						}
						++count;
					}
				}
				else
				{	
					if (count==index)
					{	
						return p;
					}
					++count;
				}
			}	
			
		}
			
			break;	
			
			
	}		
	return NULL;
	
}
void SetDeviceDetail(char *lclientName,char *clientVer,char *lclientOs,char *lclientOsVer,char *lclientModel,char *clientUId)
{
	
	
	strcpy(client_name,lclientName);
	strcpy(client_ver,clientVer);
	strcpy(client_os,lclientOs);
	strcpy(client_osver,lclientOsVer);
	strcpy(client_model,lclientModel);
	strcpy(client_uid,clientUId);
	
	
	
	
}
int getBalance()
{
	return creditBalance;
}
//dont free this memory
char *getForwardNo( int *forwardP)
{
	if(forwardP)
	{
		if(settingType==2)
		{
			*forwardP = 0;
			
		}
		else
		{
			*forwardP = 1;
		}
	}
	
	
	return fwdnumber;
}
char *getOldForwardNo()
{
	return oldForward;
}
char *getDidNo()
{
	return myDID;
}
char *getTitle()
{
	return myTitle;
}
int newVMailCount()
{
	struct VMail *pv;
	int	newMails = 0;
	for ( pv = listVMails;pv;  pv = pv->next)
	{	
		
		
		if ( pv->direction==VMAIL_IN && !pv->deleted && pv->status==VMAIL_ACTIVE)
		{
			newMails++;
		}
	}	
	
	return newMails;
	//return gnewMails;
}

void newVMailCountdecrease()
{
	if(gnewMails)
	{
		gnewMails --;
	}
}
void SetOrReSetForwardNo(int forwardB, char *forwardNoCharP)
{
	forwardStartB = 1;
	if(forwardB==1)
	{	
		if(forwardNoCharP)
		{	
			
			char *newNoP;
			newNoP = NormalizeNumber(forwardNoCharP,1);
			strncpy(fwdnumber,newNoP,sizeof(fwdnumber)-2);
			free(newNoP);
			oldSetting = -1;
			settingType = 3;
		}	
	}
	else
	{
		if(forwardB==2)//mean delete previous forward no
		fwdnumber[0]=0;
		settingType = 2;
	}
}
struct AddressBook * getContactAndTypeCall(char *objStrP,/*out*/char *ltypeCharP)
{
	struct AddressBook *addressP;
	char *typeCallP = "Unkonwn";
	addressP = getContactOf(objStrP);
	//
	if(addressP)
	{
		if (!strcmp(addressP->mobile, objStrP))
		{
			typeCallP = "mobile";
		}
		
		if (!strcmp(addressP->home, objStrP))
		{
			typeCallP = "home";
		}
		
		if (!strcmp(addressP->business, objStrP))
		{
			typeCallP = "business";
		}	
		if (!strcmp(addressP->spoknid, objStrP))
		{
			typeCallP = "spokn";
		}	
		if (!strcmp(addressP->other, objStrP))
		{
			typeCallP = "other";
		}
		
		if (!strcmp(addressP->email, objStrP))
		{
			typeCallP = "email";
		}
		
		
		
	}
	else
	{
		if(strstr(objStrP,"@"))
		{
			typeCallP = "email";
		}
	}
	if(ltypeCharP)
	{
		strcpy(ltypeCharP,typeCallP);
	}
	return addressP;
	
}
int vmsDeleteByID(char *idCharP)
{
	struct VMail *tmpVarVms;
	tmpVarVms = vmsById(idCharP);
	if(tmpVarVms)
	{	
		vmsDelete(tmpVarVms);
		return 0;
	}
	return 1;
}
char *getAccountPage()
{
	char cookieCharP[200];
	char *returnCharP;
	httpCookie(cookieCharP);
	returnCharP = malloc(500);
#ifdef _LTP_
	
	sprintf(returnCharP,"http://%s/services/iphone/accounts?userid=%s&session=%s",pstack->ltpServerName,pstack->ltpUserid,cookieCharP);

#else
	
	sprintf(returnCharP,"http://%s/services/iphone/accounts?userid=%s&session=%s",pstack->ltpServerName,pstack->ltpUserid,cookieCharP);

	
#endif	
	
	return returnCharP;
}
char *getCreditsPage()
{
	char cookieCharP[200];
	char *returnCharP;
	profileGetKey();
	httpCookie(cookieCharP);
	returnCharP = malloc(500);
#ifdef _LTP_ 
	
	sprintf(returnCharP,"http://%s/services/iphone/payment?userid=%s&session=%s&mode=cc",pstack->ltpServerName,pstack->ltpUserid,cookieCharP);
	
#else
	sprintf(returnCharP,"http://%s/services/iphone/payment?userid=%s&session=%s&mode=cc",pstack->ltpServerName,pstack->ltpUserid,cookieCharP);
	
#endif	
	
	
	return returnCharP;
}
char *getSupportPage()
{
	char cookieCharP[200];
	char *returnCharP;
	profileGetKey();
	httpCookie(cookieCharP);
	returnCharP = malloc(500);
#ifdef _LTP_ 
	
	sprintf(returnCharP,"http://%s/services/iphone/support?userid=%s&session=%s",pstack->ltpServerName,pstack->ltpUserid,cookieCharP);

#else
	sprintf(returnCharP,"http://%s/services/iphone/support?userid=%s&session=%s",pstack->ltpServerName,pstack->ltpUserid,cookieCharP);
	
#endif	
	
	
	return returnCharP;
}

char *getPayPalPage()
{
	char cookieCharP[200];
	char *returnCharP;
	profileGetKey();
	httpCookie(cookieCharP);
	returnCharP = malloc(500);
#ifdef _LTP_ 
	
	sprintf(returnCharP,"http://%s/services/iphone/payment?userid=%s&session=%s&mode=pp",pstack->ltpServerName,pstack->ltpUserid,cookieCharP);
	
#else
	sprintf(returnCharP,"http://%s/services/iphone/payment?userid=%s&session=%s&mode=pp",pstack->ltpServerName,pstack->ltpUserid,cookieCharP);
	
#endif	
	
	
	return returnCharP;
}
void vmailDeleteAll()
{
	
	struct VMail *p;
	
	p = listVMails;
	while (p)
	{
		p->toDelete = 1;
		
		
		p = p->next;
	}
	
}
int validateNo(char *numberP)
{
	char *tmpCharP;
	int checkB=0;
	tmpCharP = numberP;
	if(tmpCharP==0)
	{
		return 1;
	}
	while(*tmpCharP)
	{
		if(checkB==0)
		{	
			if(*tmpCharP>='0' &&*tmpCharP<='9')
			{
				checkB = 1;
			}
		}
		if(checkB)
		{	
			if(*tmpCharP=='+' || *tmpCharP=='*' || *tmpCharP=='#' )
			{
				tmpCharP++;
				if(*tmpCharP)
				{
					checkB = 0;
				}
				
				break;
			}
		}
		tmpCharP++;
	}
	return !checkB;//if 1 mean number is valid
	
}
char *NormalizeNumber(char *lnoCharP,int type)
{
	char *resultCharP = 0;
	if(lnoCharP)
	{
		char *tmpCharP;
		
		int i = 0;
		tmpCharP = lnoCharP;
		if(type==2)
		{
			if(strstr(lnoCharP,"@")==0)//if not email
			{
				type = 1;
			}
		}
		resultCharP = malloc(strlen(tmpCharP)+2);
		while(*tmpCharP)
		{
			
			
			
			if(type==2)//vmail
			{
				if (*tmpCharP == ' ' || *tmpCharP == '(' || *tmpCharP == ')' || *tmpCharP == '/' || *tmpCharP == '+'|| *tmpCharP == ',' )
				{
					tmpCharP++;
					continue;
				}
				
				
				
			}
			if(type==1)//number for forword
			{
				if (*tmpCharP == ' ' || *tmpCharP == '(' || *tmpCharP == ')' || *tmpCharP == '/' || *tmpCharP == '-' || *tmpCharP == '+'|| *tmpCharP == '.' || *tmpCharP == '*'|| *tmpCharP == '#'|| *tmpCharP == ',' )
				{
					tmpCharP++;
					continue;
				}
				
				
			}
			if(type==0)//number for call
			{
				
				if (*tmpCharP == ' ' || *tmpCharP == '(' || *tmpCharP == ')' || *tmpCharP == '/' || *tmpCharP == '.' || *tmpCharP == ',' || *tmpCharP == '-'  )
				{
					tmpCharP++;
					continue;
				}
				
				
				
			}
			resultCharP[i++]=	*tmpCharP;
			tmpCharP++;
		}
		resultCharP[i++] = 0;
		
	}
	
	return resultCharP;
	
	
}
int validName(char*inP)
{
	int validB = 0;
	if(inP==0)
	{
		return 0;
	}
	while(*inP)
	{
		if(*inP!=32 && *inP!='\t')
		{
			
			validB = 1;
		}
		inP++;
	}
	return validB;
	
	


}
long missCallCount;
int resetMissCallCount()
{
	if(missCallCount==0)
	{
		return 1;
	}
		
	missCallCount = 0;
	return 0;

}
int getMissCount()
{
	return missCallCount;
}
int incriseMissCallCount()
{
	
	missCallCount++;
	return missCallCount;

}
void loadMissCall()
{
	char pathname[MAX_PATH];
	FILE *fp;
#ifdef _MACOS_
	sprintf(pathname, "%s/misscallcount.txt", myFolder);
	
#else
	sprintf(pathname, "%s\\misscallcount.txt", myFolder);
	
#endif
	missCallCount = 0;
	fp = fopen(pathname,"rb");
	if(fp)
	{
		fread(&missCallCount,1,sizeof(long),fp);
		fclose(fp);
	}
	
}
void saveMissCall()
{
	char pathname[MAX_PATH];
	FILE *fp;
#ifdef _MACOS_
	sprintf(pathname, "%s/misscallcount.txt", myFolder);
	
#else
	sprintf(pathname, "%s\\misscallcount.txt", myFolder);
	
#endif
	
	fp = fopen(pathname,"wb");
	if(fp)
	{
		fwrite(&missCallCount,1,sizeof(long),fp);
		fclose(fp);
	}
	
}
int terminateThread()
{
	terminateB = 1;
	return busy;
}
void * GetObjectByUniqueID(UAObjectType uaObj ,int luniqueId)
{
	switch(uaObj)
	{
			
		case GETCONTACTLIST://
		{	
			struct AddressBook	*p;
						
			for (p = listContacts; p; p = p->next)
			{
				if(p->uniqueID == luniqueId)
				{
					return p;
				}
			}	
			
		}
			break;
			
		case GETVMAILLIST:
		case GETVMAILUNDILEVERD:		
		{
			struct VMail *p;
			
			
			for (p = listVMails; p; p = p->next)
			{	
				if(p->uniqueID==luniqueId)
				{
					return p;
				}
			}	
			
		}
			break;
		case GETCALLLOGLIST:
		case GETCALLLOGMISSEDLIST:	
		{
			struct CDR  *p;
			
			for (p = listCDRs; p; p = p->next)
			{	
				
				if(p->uniqueID==luniqueId)
				{
					return p;
				}
			}	
			
		}
			
			break;	
			
			
	}		
	return NULL;
	
}

struct AddressBook * getContactList()
{
	return listContacts;
}
struct CDR * getCallList()
{
	return listCDRs;
}

int getCreditBalance()
{
	//printf("\n credit %d",creditBalance);
	return creditBalance;
}

struct VMail *getVMailList()
{
	return listVMails;
}

int getVmailCount()
{
	int x=0;
	if(!listVMails)
		return 0;
	for (struct VMail *q = listVMails; q; q = q->next)
		if (!q->deleted && !q->toDelete)
			x++;
	return x;
}
void applicationEnd()
{
	appTerminateB = 1;
	TerminateUAThread();
	while(threadStatus==ThreadStart)
	{
		sleep(1);
	}
}
char* getencryptedPassword()
{
	/*
	 spoknid*pin*bpartyno.
	 where
	 spoknid : seven digit spokn id
	 e.g. 1234567
	 pin : 1st 5 digit(md5(pwd))
	 e.g. md5(vel) = d41d8cd98f00b204e9800998ecf8427e
	 1st 5 digit of md5 = d41d8
	 replace a = 6, b = 5, c = 4, d = 3, e = 2, f = 1(digit more than 9 subtract it with 16)
	 so the pin = 34138
	 bpartyno = 919821988975
	 */ 
	struct	MD5Context	md5;
	unsigned char	digest[16];
	char password[40];
	char hex[33];
	char newpass[6];
	int ch;
	memset(newpass, 0, sizeof(newpass));
	memset(hex, 0, sizeof(hex));
	memset(digest, 0, sizeof(digest));
	memset(password, '\0', sizeof(password));
	memset(&md5, 0, sizeof(md5));
	strcpy((char*)password,pstack->ltpPassword);
	MD5Init(&md5);
	MD5Update(&md5, (char unsigned *)password, strlen(password), 0);
	MD5Final(digest,&md5);
	md5ToHex(digest,hex);
	//printf("\n%s\n\n",hex);
	int i;	
	for(i=0;i<5;i++)
	{
		newpass[i]=hex[i];
				if( newpass[i] > '9' )
		 {
			 if( newpass[i]<'a')
			 {	 
				 ch = 6 - (newpass[i]-65);//ascii value of A
			 }
			 else {
				  ch = 6 - (newpass[i]-97);//ascii value of a
			 }
			 newpass[i] = ch + 48;

		 }									
	}
	return strdup(newpass);
	// printf("\n%s\n",newpass);
	//char *retVal;
	//retVal=malloc(sizeof(newpass));
	//strcpy(retVal,newpass);
	//return retVal;
}

#endif