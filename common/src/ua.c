// profile.cpp : Defines the entry point for the application.
//
#include <windows.h>
#include <winsock.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <blowfish.h>
#include <ltpmobile.h>
#include <ezxml.h>
#include <ua.h>

// this is the single object that is the instance of the ltp stack for the user agent
struct ltpStack *pstack;
struct CDR *listCDRs=NULL;
struct AddressBook *listContacts=NULL;
struct VMail *listVMails=NULL;

static unsigned long	lastUpdate = 0;
static int busy = 0;
char	myFolder[MAX_PATH], vmFolder[MAX_PATH], outFolder[MAX_PATH];
char mailServer[100], myTitle[200], fwdnumber[32], myDID[32], client_name[32],client_ver[32],client_os[32],client_osver[32],client_model[32],client_uid[32];
int	redirect = REDIRECT2ONLINE;
int creditBalance = 0;
int bandwidth;

//variable to set the type for incoming call termination setting
int settingType = -1;  //not assigned state yet
int oldSetting = -1;

//add by mukesh 20359
ThreadStatusEnum threadStatus;
char uaUserid[32];

/*
** Translation Table as described in RFC1113
*/
static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
** Translation Table to decode (created by author)
*/
static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

//base64 encoder
static void encodeblock( unsigned char in[3], unsigned char out[4], int len )
{
    out[0] = cb64[ in[0] >> 2 ];
    out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
    out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
    out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}

/*
** (base64 decoder) decode 4 '6-bit' characters into 3 8-bit binary bytes
*/
static void decodeblock( unsigned char in[4], unsigned char out[3] )
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
	char *p;
	char *tp;

	if(t == NULL) {
		fprintf(stderr, "Serious memory error...\n");
		exit(1);
	}

	tp=t;

	for(p=s; *p; p++) {

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


static int restCall(char *requestfile, char *responsefile, char *host, char *url)
{
	FILE	*pf;
	SOCKET	sock;
	char	data[10000], header[1000];
	struct	sockaddr_in	addr;
	int	byteCount = 0, isChunked = 1, contentLength = 0, ret, length;

	pf = fopen(requestfile, "rb");
	if (!pf)
		return 0;
	fseek(pf, 0, SEEK_END);
	contentLength = ftell(pf);
	fclose(pf);

	//prepare the http header
	sprintf(header, 
		"POST %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Content-Length: %d\r\n\r\n",
		url, host, contentLength);

	addr.sin_addr.s_addr = lookupDNS(host);
	if (addr.sin_addr.s_addr == INADDR_NONE)
		return 0;
	
	addr.sin_port = htons(80);	
	addr.sin_family = AF_INET;

	//try connecting the socket
	sock = (int)socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)))
		return 0;

	//send the http headers
	send(sock, header, strlen(header),0);
	byteCount += strlen(header);
	
	//send the xml data
	pf = fopen(requestfile, "rb");
	if (!pf)
		return 0;
	
	while((ret = fread(data, 1, sizeof(data), pf)) > 0){
		send(sock, data, ret, 0);
		byteCount += ret;
	}
	fclose(pf);

	//read the headers
	//Tasvir Rohila - bug#18352 & #18353 - Initialize isChunked to zero for profile to download & 
	//Add/Delete contact to work.
	isChunked = 0;
	while (1){
		int length = readNetLine(sock, data, sizeof(data));
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
		return 0;

	//read the body 
	while (1) {
		if (isChunked){
			int count;

			length = readNetLine(sock, data, sizeof(data));
			if (length <= 0)
				break;
			byteCount += length;
			count = strtol(data, NULL, 16);
			if (count <= 0)
				break;

			//read the chunk in multiple calls to read
			while(count){
				length = recv(sock, data, count > sizeof(data) ? sizeof(data) : count, 0);
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
				length = recv(sock, data, sizeof(data), 0);
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

static void cdrSave(){
	char pathname[MAX_PATH];
	struct	 CDR *q;
	char	line[1000];
	FILE	*pf;

	sprintf(pathname, "%s\\calls.txt", myFolder);
	pf = fopen(pathname, "w");
	if (!pf)
		return;
	
	for (q = listCDRs; q; q = q->next){
		sprintf(line, "<cdr><date>%lu</date><duration>%d</duration><type>%d</type><userid>%s</userid></cdr>\r\n",
		(unsigned long)q->date, (int)q->duration, (int)q->direction, q->userid);
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

void cdrAdd(char *userid, time_t time, int duration, int direction)
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
	//Kaustubh Deshpande 21114 fixed-Junk characters are displayed in call list on calling a long number.
	//the userid has limit of 32, and we were not checking the limit earlier. Now a check is added for that.
	for (i=0;i<31;i++)
	{
		p->userid[i]=userid[i];
	}
	p->userid[32]=0;
	///
	
	p->date = (time_t)time;
	p->duration = duration;
	p->direction = direction;
	getTitleOf(p->userid, p->title);

	//add to the linked list
	p->next = listCDRs;
	listCDRs = p;

	sprintf(pathname, "%s\\calls.txt", myFolder);
	sprintf(line, "<cdr><date>%ul</date><duration>%d</duration><type>%d</type><userid>%s</userid></cdr>\r\n",
		(unsigned long)p->date, (int)p->duration, (int)p->direction, p->userid);

	pf = fopen(pathname, "a");
	fwrite(line, strlen(line), 1, pf);
	fclose(pf);
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
	ezxml_t	cdr, duration, date, userid, type;

	sprintf(pathname, "%s\\calls.txt", myFolder);
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


		p = (struct CDR *) malloc(sizeof(struct CDR));
		if (!p){
			fclose(pf);
			ezxml_free(cdr);
			return;
		}
		memset(p, 0, sizeof(struct CDR));
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
		p->next = listCDRs;
		listCDRs = p;

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
		i++;

	return i;
}

struct AddressBook *getContact(int id)
{
	struct AddressBook	*p;

	for (p = listContacts; p; p = p->next)
		if (p->id == id && !p->isDeleted)
			return p;
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
			if (!_strnicmp(r, query, qlen))
				return 1;
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

struct AddressBook *updateContact(unsigned long id, char *title, char *mobile, char *home, char *business, char *other, char *email)
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
			p->dirty = 1;
			return p;
		}

	q = (struct AddressBook *)malloc(sizeof(struct AddressBook));
	memset(q, 0, sizeof(struct AddressBook));
	
	q->id = id;
	
	strcpy(q->title, title);
	strcpy(q->mobile, mobile);
	strcpy(q->home, home);
	strcpy(q->business, business);
	if (other)
		strcpy(q->other, other);
	if (email)
		strcpy(q->email, email);

	q->dirty = 1;
	//insert in at the head and sort
	q->next = listContacts;
	listContacts = q;
	return q;
}

struct AddressBook *addContact(char *title, char *mobile, char *home, char *business, char *other, char *email)
{
	struct AddressBook	*q;

	q = (struct AddressBook *)malloc(sizeof(struct AddressBook));
	memset(q, 0, sizeof(struct AddressBook));
	
	strcpy(q->title, title);
	strcpy(q->mobile, mobile);
	strcpy(q->home, home);
	strcpy(q->business, business);
	if (other)
		strcpy(q->other, other);
	if (email)
		strcpy(q->email, email);
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
			sprintf(title, "%s(m)", p->title);
			return p;
		}

		if (!strcmp(p->home, userid)){
			sprintf(title, "%s(h)", p->title);
			return p;
		}

		if (!strcmp(p->business, userid)){
			sprintf(title, "%s(b)", p->title);
			return p;
		}

		if (!strcmp(p->other, userid)){
			sprintf(title, "%s(o)", p->title);
			return p;
		}

		if (!strcmp(p->email, userid)){
			sprintf(title, "%s(mail)", p->title);
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
				if (atol(p->vmsid) < atol(i->vmsid))
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
	ezxml_t	date, userid, vmsid, direction, status, deleted, hashid, toDelete;

	date = ezxml_child(vmail, "dt");
	vmsid = ezxml_child(vmail, "id");
	userid = ezxml_child(vmail, "u");
	direction = ezxml_child(vmail, "dir");
	status = ezxml_child(vmail, "status");
	hashid = ezxml_child(vmail, "hashid");
	deleted = ezxml_child(vmail, "deleted");
	toDelete = ezxml_child(vmail, "todelete");

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
		strcpy(p->hashid, hashid->txt);
		strcpy(p->vmsid, vmsid->txt);
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

	fprintf(pf, "<vm><dt>%u</dt><u>%s</u><id>%s</id><hashid>%s</hashid><dir>%s</dir>",
		(unsigned int)p->date, p->userid, p->vmsid, p->hashid, p->direction == VMAIL_OUT ? "out" : "in");
	
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
		sprintf(path, "%s\\%s.gsm", vmFolder, p->hashid);
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
	sprintf(path, "%s\\%s.gsm", vmFolder, p->hashid);
	unlink(path);

	//profileResync();
}

struct VMail *vmsUpdate(char *userid, char *hashid, char *vmsid, time_t time, int status, int direction)
{
	struct	VMail	*p=NULL;
	int		isNew=1;

	if (vmsid)
		p = vmsById(vmsid);

	//if it already exists, then update the status and return
	if (p){
		p->status = status;
		return p;
	}

	//hmm this looks like a new one
	p = (struct VMail *) malloc(sizeof(struct VMail));
	if (!p)
		return NULL;
	memset(p, 0, sizeof(struct VMail));

	strcpy(p->userid, userid);
	if (vmsid)
		strcpy(p->vmsid, vmsid);
	strcpy(p->hashid, hashid);
	p->date = (time_t)time;
	p->direction = direction;
	p->status = status;

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
	unsigned char g[10], b64[10], buffer[10000];
	char	requestfile[MAX_PATH], responsefile[MAX_PATH], path[MAX_PATH];
	FILE	*pfIn, *pfOut;
	int		length, byteCount;

	if (v->direction != VMAIL_OUT || v->status != VMAIL_NEW)
		return;

	sprintf(path, "%s\\%s.gsm", vmFolder, v->hashid);
	pfIn = fopen(path, "rb");
	if (!pfIn){
		v->toDelete = 1;
		return;
	}

	sprintf(requestfile, "%s\\vmaireq.txt", myFolder);
	sprintf(responsefile, "%s\\vmailresp.txt", myFolder);

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

	byteCount = restCall(requestfile, responsefile, mailServer, "/cgi-bin/vmsoutbound.cgi");
	if (!byteCount)
		return;

	pfIn = fopen(responsefile, "rb");
	if (!pfIn){
		alert(-1, ALERT_VMAILERROR, "Failed to upload.");
		return;
	}

	length = fread(buffer, 1, sizeof(buffer), pfIn);
	fclose(pfIn);
	if (length < 40){
		alert(-1, ALERT_VMAILERROR, "Unable to send the VMS properly.");
		return;
	}
	buffer[length] = 0;

	strxml = strstr(buffer, "<?xml");

	//todo check that the response is not 'o' and store the returned value as the vmsid
	if (strxml){
		ezxml_t xml, status, vmsid;

		if (xml = ezxml_parse_str(strxml, strlen(strxml))){
			if (status = ezxml_child(xml, "status")){

				vmsid = ezxml_child(xml, "id");
				if (vmsid)
					strcpy(v->vmsid, vmsid->txt);
				else
					alert(-1, ALERT_VMAILERROR, "Voicemail response incorrect");

				if (!strcmp(status->txt, "active"))
					v->status = VMAIL_ACTIVE;
				else
					alert(-1, ALERT_VMAILERROR, "Voicemail upload failed");
			}
			ezxml_free(xml);
		}
	}
	unlink(requestfile);
	unlink(responsefile);
}

static void vmsUploadAll()
{
	struct VMail *p;

	for (p = listVMails; p; p = p->next){
		if (p->direction == VMAIL_OUT && p->status == VMAIL_NEW)
			vmsUpload(p);
	}
}

static void vmsDownload()
{
	char	data[1000], key[64], header[2000];
	int		nMails = 0;
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
		sprintf(pathname, "%s\\%s.gsm", vmFolder, p->hashid);
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
						fwrite(data, length, 1, pfIn);
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
		if(threadStatus == ThreadTerminate)
		{
			break;
		}
		
	} //loop over for the next voice mail
	//add by mukesh for bug id 20359
	free(p);
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
	unsigned char szData[32], szEncPass[64], szBuffIn[10], szBuffOut[10];
	BLOWFISH_CTX ctx;
	int i, len;

	sprintf(pathname, "%s\\profile.xml", myFolder);
	pf = fopen(pathname, "w");
	if (!pf)
		return;

	//Tasvir Rohila - 25/02/2009 - bug#17212.
	//Encrypt the password before saving to profile.xml
	memset(szData, '\0', sizeof(szData));
	strcpy(szData,pstack->ltpPassword);

	Blowfish_Init (&ctx, (unsigned char*)HASHKEY, HASHKEY_LENGTH);

	for (i = 0; i < sizeof(szData); i+=8)
		   Blowfish_Encrypt(&ctx, (unsigned long *) (szData+i), (unsigned long*)(szData + i + 4));
	szData[31] = '\0'; //important to NULL terminate;

	//Output of Blowfish_Encrypt() is binary, which needs to be stored in plain-text in profile.xml for ezxml to understand and parse.
	//Hence base64 encode the cyphertext.
	memset(szEncPass, 0, sizeof(szEncPass));
	memset(szBuffIn, 0, sizeof(szBuffIn));
	memset(szBuffOut, 0, sizeof(szBuffOut));
	len = strlen(szData);
	for (i=0; i<len; i+=3)
	{
		szBuffIn[0]=szData[i];
		szBuffIn[1]=szData[i+1];
		szBuffIn[2]=szData[i+2];
		encodeblock(szBuffIn, szBuffOut, 3);
		strcat(szEncPass, szBuffOut);
	}

	fputs("<?xml version=\"1.0\"?>\n", pf);
	//Now that password is stored encrypted, <password> tag is replaced by <encpassword>
	fprintf(pf, "<profile>\n <u>%s</u>\n <dt>%lu</dt>\n <encpassword>%s</encpassword>\n <server>%s</server>\n",
		pstack->ltpUserid, lastUpdate, szEncPass, pstack->ltpServerName);

	if (strlen(fwdnumber))
		fprintf(pf, "<fwd>%s</fwd>\n", fwdnumber);
	if (strlen(mailServer))
		fprintf(pf, "<mailserver>%s</mailserver>\n", mailServer);
	
	//the contacts
	for (p = getContactsList(); p; p = p->next){
		fprintf(pf, " <vc><id>%u</id><t>%s</t>", p->id, p->title);
		if (p->mobile[0])
			fprintf(pf, "<m>%s</m>", p->mobile);
		if (p->home[0])
			fprintf(pf, "<h>%s</h>", p->home);
		if (p->business[0])
			fprintf(pf, "<b>%s</b>", p->business);
		if (p->email[0])
			fprintf(pf, "<e>%s</e>", p->email);
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
	ezxml_t xml, contact, id, title, mobile, home, business, favourite, mailserverip;
	ezxml_t userid, password, server, lastupdate, dirty, status, vms, fwd, email;
	char empty[] = "";
	struct AddressBook *p;
	unsigned char v, szData[32], szBuffIn[10], szBuffOut[10];
    BLOWFISH_CTX ctx;
	int i, j, len;

	strcpy(pstack->ltpServerName, "64.49.236.88");
	sprintf(pathname, "%s\\profile.xml", myFolder);

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
				strncat(szData, szBuffOut, 3);
			}

			Blowfish_Init (&ctx, (unsigned char*)HASHKEY, HASHKEY_LENGTH);
			for (i = 0; i < sizeof(szData); i+=8)
				   Blowfish_Decrypt(&ctx, (unsigned long *) (szData+i), (unsigned long *)(szData + i + 4));
			strcpy(pstack->ltpPassword, szData); 
		}
	}

	if (server = ezxml_child(xml, "server"))
		strcpy(pstack->ltpServerName, server->txt);
	
	if (lastupdate = ezxml_child(xml, "dt"))
		lastUpdate = atol(lastupdate->txt);

	if (mailserverip = ezxml_child(xml, "mailserver"))
		strcpy(mailServer, mailserverip->txt);

	fwd = ezxml_child(xml, "fwd");
	if (fwd)
		strcpy(fwdnumber, fwd->txt);

	
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
		dirty  = ezxml_child(contact, "dirty");
		favourite = ezxml_child(contact, "fav");
		email = ezxml_child(contact, "e");

		p = updateContact(atol(id->txt), 
			title ? title->txt : empty, 
			mobile ? mobile->txt : empty, 
			home ? home->txt : empty, 
			business ? business->txt : empty, 
			"",
			email ? email->txt: empty);
	
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

	sprintf(pathname, "%s\\profile.xml", myFolder);
	unlink(pathname);
	lastUpdate = 0;
	myTitle[0] = 0;
	myDID[0] = 0;
	fwdnumber[0] = 0;
	resetContacts();
	vmsEmpty();
	relistContacts();
	relistVMails();
}


void profileMerge(){
	FILE	*pf;
	char	pathname[MAX_PATH], stralert[2*MAX_PATH], *strxml, *phref,*palert;
	ezxml_t xml, contact, id, title, mobile, home, business, email, did, presence, dated;
	ezxml_t	status, vms, redirector, credit, token, fwd, mailserverip, xmlalert;
	struct AddressBook *pc;
	int		nContacts = 0, xmllength, newMails;

	char empty[] = "";
	
	sprintf(pathname, "%s\\down.xml", myFolder);
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

	//Tasvir Rohila - 10-04-2009 - bug#19095
	//For upgrades or any other notification server sends <alert href="">Some msg</alert> in down.xml

	if (xmlalert = ezxml_child(xml, "client"))
	{
		phref = NULL;
		palert = NULL;
		if(palert = ezxml_attr(xmlalert, "alert"))
		{
			phref = ezxml_attr(xmlalert, "href");
			sprintf(stralert, "%s%c%s", phref ? phref : "", SEPARATOR, palert );
		}
	}

	title = ezxml_child(xml, "t");
	if (title)
		strcpy(myTitle, title->txt);

	did = ezxml_child(xml, "did");
	if (did)
		strcpy(myDID, did->txt);

	redirector = ezxml_child(xml, "rd");
	if (redirector){
		if (!strcmp(redirector->txt, "1"))
			settingType = REDIRECT2PSTN;
		else if (!strcmp(redirector->txt, "0"))
			settingType = REDIRECT2VMS;
		else if (!strcmp(redirector->txt, "2"))
			settingType = REDIRECT2ONLINE;
		else
			settingType = REDIRECTBOTH;
		oldSetting = settingType;
	}
	credit = ezxml_child(xml, "cr");
	if (credit)
		creditBalance = atoi(credit->txt);

	dated = ezxml_child(xml, "dt");
	if (dated)
		lastUpdate = (unsigned long)atol(dated->txt);

	fwd = ezxml_child(xml, "fwd");
	if (fwd)
		strcpy(fwdnumber, fwd->txt);

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
			email ? email->txt: empty);
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

	//TBD detect new voicemails and alert the user
	if (newMails){
		alert(-1, ALERT_NEWVMAIL, NULL);
		vmsSort();
		relistVMails();	
	} 

	//relist contacts if any contacts have changed or are added
	if (nContacts){
		sortContacts();
		relistContacts();
	}

	if (palert)
		alert(-1, ALERT_SERVERMSG, stralert);
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
	unsigned long timeStart, timeFinished, timeTaken;
    
	if (busy > 0|| !strlen(pstack->ltpUserid))
		return 0;
	else 
		busy = 1;
	//add by mukesh for bug id 20359
	threadStatus = ThreadStart ;
	httpCookie(key);

	//prepare the xml upload
	sprintf(pathUpload, "%s\\upload.xml", myFolder);
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
	" <since>%u</since> \n", 
	pstack->ltpUserid, key, client_name,client_ver,client_os,client_osver,client_model,client_uid,lastUpdate);

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
				"<vc><t>%s</t><m>%s</m><b>%s</b><h>%s</h><e>%s</e><token>%x</token></vc>\n", 
				pc->title, pc->mobile, pc->business, pc->home, pc->email, (unsigned int)pc);
		fprintf(pfOut, "</add>\n");
	}


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
				"<vc><id>%u</id><t>%s</t><m>%s</m><b>%s</b><h>%s</h><e>%s</e></vc>\n",
				(unsigned long)pc->id, pc->title, pc->mobile, pc->business, pc->home, pc->email);

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
					" <vc><id>%u</id></vc>\n",
					(unsigned long)pc->id);

		for (vm = listVMails; vm; vm = vm->next)
			if (vm->toDelete)
				fprintf(pfOut, " <vm><id>%s</id></vm>\n", vm->vmsid);
		fprintf(pfOut, "</del>\n");
	}
	
	fprintf(pfOut, "</profile>\n");
	fclose(pfOut);

    timeStart = ticks();
	sprintf(pathDown, "%s\\down.xml", myFolder);

	byteCount = restCall(pathUpload, pathDown, pstack->ltpServerName, "/cgi-bin/userxml.cgi");
    
	timeFinished = ticks();
	timeTaken = (timeFinished - timeStart);
	setBandwidth(timeTaken,byteCount);

	profileMerge();
	profileSave();
	relistContacts();
	refreshDisplay();
	vmsUploadAll();
	relistVMails();
	vmsDownload();

	busy = 0;
	//add by mukesh for bug id 20359
	threadStatus = ThreadNotStart ;
	return 0;
}

void profileResync()
{
	if((uaUserid  && strcmp(uaUserid ,pstack->ltpUserid)!=0))
	{
		strcpy(uaUserid ,pstack->ltpUserid);
		//add by mukesh for bug id 20359
		threadStatus = ThreadTerminate;
		profileClear();
	}
	START_THREAD(profileDownload);
//		CreateThread(NULL, 0, downloadProfile, NULL, 0, NULL);
}

void profileSetRedirection(int redirectTo)
{
	CreateThread(NULL, 0, profileDownload, (LPVOID)redirectTo, 0, NULL);
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