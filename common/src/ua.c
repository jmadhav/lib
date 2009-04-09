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
char mailServer[100], myTitle[200], fwdnumber[32], myDID[32];
int	redirect = REDIRECT2ONLINE;
int creditBalance = 0;

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
	for (i = strlen(szString) - 1; !isprint(szString[i]) && i >=0; i--)
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

void cdrSave(){
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


void cdrCompact() {
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
	strcpy(p->userid, userid);
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
	
	cdrSave();
	cdrEmpty();
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


void updateContactPresence(int id, int presence)
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
	while (p){
		q = p->next;
		free(p);
		p = q;
	}
	listVMails = NULL;
}

//the vmails are saved in a very wierd format of one xml object per line. dont put a line-break within <vm> ... </vm>
void vmsSave(){
	char pathname[MAX_PATH];
	char	line[1000];
	struct	 VMail *q;
	FILE	*pf;

	sprintf(pathname, "%s\\vmails.txt", myFolder);
	pf = fopen(pathname, "w");
	if (!pf)
		return;
	
	for (q = listVMails; q; q = q->next){
		if (q->status == VMAIL_STATUS_DONE)
			continue;
		sprintf(line, "<vm><date>%u</date><userid>%s</userid><id>%s</id><direction>%s</direction>",
			(unsigned int)q->date, q->userid, q->vmsid, q->direction == VMAIL_OUT ? "out" : "in");
		fwrite(line, strlen(line), 1, pf);

		switch(q->status){
			case VMAIL_STATUS_NEW:
				strcpy(line, "<status>new</status>");
				break;
			case VMAIL_STATUS_DELETE:
				strcpy(line, "<status>delete</status>");
				break;

				//inbox
			case VMAIL_STATUS_DONE:
				strcpy(line, "<status>done</status>");
				break;
			case VMAIL_STATUS_READY:
				strcpy(line, "<status>ready</status>");
				break;
			case VMAIL_STATUS_OPENED:
				strcpy(line, "<status>opened</status>");
				break;
				//outbox
			case VMAIL_STATUS_UPLOADED:
				strcpy(line, "<status>done</status>");
				break;
			case VMAIL_STATUS_DELIVERED:
				strcpy(line, "<status>delivered</status>");
				break;				
		}
		strcat(line, "</vm>\n");
		fwrite(line, strlen(line), 1, pf);
	}
	fclose(pf);
}

void vmsLoad() 
{
	FILE	*pf;
	char	pathname[MAX_PATH];
	struct VMail *p, *tail;
	int		index;
	char	line[1000];
	ezxml_t	vmail, date, userid, vmsid, direction, status;

	sprintf(pathname, "%s\\vmails.txt", myFolder);
	pf = fopen(pathname, "r");
	if (!pf)
		return;

	index = 0;
	tail = NULL;
	while (fgets(line, 999, pf)){
		vmail = ezxml_parse_str(line, strlen(line));
		if (!vmail)
			continue;

		date = ezxml_child(vmail, "date");
		vmsid = ezxml_child(vmail, "id");
		userid = ezxml_child(vmail, "userid");
		direction = ezxml_child(vmail, "direction");
		status = ezxml_child(vmail, "status");

		if (!status)
			continue;
		if (!strcmp(status->txt, "done"))
			continue;

		p = (struct VMail *) malloc(sizeof(struct VMail));
		if (!p){
			fclose(pf);
			ezxml_free(vmail);
			return;
		}
		memset(p, 0, sizeof(struct VMail));
		if (date)
			p->date = (unsigned long)atol(date->txt);
		if (userid)
			strcpy(p->userid, userid->txt);
		if (vmsid)
			strcpy(p->vmsid, vmsid->txt);
	
		if (direction){
			if (!strcmp(direction->txt, "out"))
				p->direction = VMAIL_OUT;
			else
				p->direction = VMAIL_IN;
		}
		
		if (!strcmp(status->txt, "new"))
			p->status = VMAIL_STATUS_NEW;
		else if (!strcmp(status->txt, "delete"))
			p->status = VMAIL_STATUS_DELETE;
		else if (!strcmp(status->txt, "ready"))
			p->status = VMAIL_STATUS_READY;
		else if (!strcmp(status->txt, "opened"))
			p->status = VMAIL_STATUS_OPENED;
		else if (!strcmp(status->txt, "delivered"))
			p->status = VMAIL_STATUS_DELIVERED;

		//add to the tail of the list
		if (tail)
			tail->next = p;
		else
			listVMails = p;
		tail = p;
	}
	fclose(pf);
}

void vmsCompact() {
	struct VMail *p, *q;
	//check if the cdr count has exceeded 200
	int i = 0;
	for (q = listVMails; q; q = q->next)
		i++;
	
	//high water point is 300
	if (i < 300)
		return;

	//remove all the cdrs past the 200th
	for (i = 0, p = listVMails; p; p = p->next)
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
	vmsSave();
}

void vmsDelete(struct VMail *p)
{
	struct VMail *q;
	char	path[MAX_PATH];
	if (!p)
		return;

	//Tasvir Rohila - 05/03/2009 - bug#18256 - Find this node in listVmails and delete it, rather than marking it as VMAIL_STATUS_DELETE.
	if (p == listVMails) {
		listVMails = p->next;
	}
	else {
		//delink the Vmail
		for (q = listVMails; q->next; q = q->next){
			if (q->next == p){
				q->next = p->next;
				break;
			}
		}
	}

	//free the node, delete the file
	if (p){
		if (p->direction == VMAIL_IN)
			sprintf(path, "%s\\%s.gsm", vmFolder, p->vmsid);
		else
			sprintf(path, "%s\\%s.gsm", outFolder, p->vmsid);

		free(p);

		unlink(path);
	}
	vmsSave();
	profileResync();
}

struct VMail *vmsUpdate(char *userid, char *vmsid, time_t time, int status, int direction)
{
	struct	VMail	*p;
	int		isNew=1;

	//search if this is an update to an existing vms, if so, just update the status and return
	for (p = listVMails; p; p = p->next){
		if (!strcmp(p->vmsid, vmsid)){
			p->status = status;
			vmsSave();
			return p;
		}
	}

	p = (struct VMail *) malloc(sizeof(struct VMail));
	if (!p)
		return NULL;
	memset(p, 0, sizeof(struct VMail));

	strcpy(p->userid, userid);
	strcpy(p->vmsid, vmsid);
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

	vmsSave();
	return p;
}

static void vmsUpload(struct VMail *v)
{
	char buffer[3000], key[100], *strxml;
	unsigned char g[10], b64[10];
	SOCKET sock;
	struct sockaddr_in	addr;
	char	path[MAX_PATH];
	FILE	*pfIn, *pfOut;
	int		length, i;

	sprintf(path, "%s\\%s.gsm", outFolder, v->vmsid);
	pfIn = fopen(path, "rb");
	if (!pfIn){
		v->status = VMAIL_STATUS_DONE;
		return;
	}

	sprintf(path, "%s\\upvmail.txt", myFolder);
	pfOut = fopen(path, "wb");
	if (!pfOut){
		fclose(pfIn);
		return;
	}

	httpCookie(key);
	fprintf(pfOut, "<callout>\n <u>%s</u><key>%s</key>\n <dest>%s</dest>\n <gsm>", pstack->ltpUserid, key, v->userid);
	while (fread(g, 3, 1, pfIn)>0){
		encodeblock(g, b64, 3);
		fwrite(b64, 4, 1, pfOut);
	}
	fclose(pfIn);
	fprintf(pfOut, "</gsm>\n</callout>\n");
	length = ftell(pfOut);
	fclose(pfOut);

	sprintf(buffer, 
		"POST //cgi-bin//xcallout.cgi HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Content-Length: %d\r\n\r\n", 
		mailServer, length);

	addr.sin_addr.s_addr = lookupDNS(mailServer);
	if (addr.sin_addr.s_addr == INADDR_NONE)
		return;
	addr.sin_port = htons(80);	
	addr.sin_family = AF_INET;

	sock = (int)socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr))){
		closesocket(sock);
		return;
	}
	//send the http headers
	send(sock, buffer, strlen(buffer),0);
	pfOut = fopen(path, "rb");
	if (!pfOut){
		return;
	}

	while((i = fread(buffer, 1, sizeof(buffer), pfOut)) > 0)
		send(sock, buffer, i, 0);
	fclose(pfOut);

	i = recv(sock, buffer, sizeof(buffer), 0);
	buffer[i] =  0;
	closesocket(sock);
	
	strxml = strstr(buffer, "<?xml");

	//todo check that the response is not 'o' and store the returned value as the vmsid
	if (strxml){
		ezxml_t xml, status;

		if (xml = ezxml_parse_str(strxml, strlen(strxml))){
			if (status = ezxml_child(xml, "status")){
				if (!strcmp(status->txt, "ok")){
					sprintf(path, "%s\\%s.gsm", outFolder, v->vmsid);
					unlink(path);
					//farhan, 2008 feb 28, i am changing the status to VMAIL_STATUS_DONE instead of VMAIL_STATUS_UPLOADED
					// to force it to be removed from the voicemail list
					v->status = VMAIL_STATUS_DONE;
				}else 
					alert(-1, ALERT_VMAILERROR, "Voicemail server problem");
			}
			ezxml_free(xml);
		}
	}
	sprintf(path, "%s\\upvmail.txt", myFolder);
	unlink(path);
}

static void vmsUploadAll()
{
	struct VMail *p;

	for (p = listVMails; p; p = p->next){
		if (p->direction == VMAIL_OUT && p->status == VMAIL_STATUS_NEW)
			vmsUpload(p);
	}
	vmsSave();
}

static void vmsDownload()
{
	char	data[1000], key[64], header[2000];
	int		nMails = 0;
	char	pathname[MAX_PATH];
	struct VMail	*p;
	SOCKET	sock;
	struct sockaddr_in	addr;
	FILE	*pfIn;
	int		length, isChunked=0, count=0;


	for (p = listVMails; p; p = p->next) {
	
		if (p->direction == VMAIL_OUT)
			continue;
		if (p->status != VMAIL_STATUS_NEW)
			continue;

		sprintf(pathname, "%s\\%s.gsm", vmFolder, p->vmsid);
		pfIn = fopen(pathname, "r");
		if (pfIn){
			fclose(pfIn);
			continue;
		}
		httpCookie(key);

		//prepare the http header
		sprintf(header, 
			"GET /cgi-bin/playgsm.cgi?userid=%s&key=%s&id=%s HTTP/1.1\r\n"
			"Host: %s\r\n\r\n",
			pstack->ltpUserid, key, p->vmsid, mailServer);

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
	} //loop over for the next voice mail
}
//change for bug id 18641 
void ResetTime()
{
	lastUpdate = 0;//this function is call from logout
	
}
/** 
profile routines
*/

//save only if the login was valid, as a result save is called from alert() upon getting ALERT_ONLINE
void profileSave(){
	char	pathname[MAX_PATH];
	struct AddressBook *p;
	FILE	*pf;
	unsigned char szData[32], szEncPass[64], szBuffIn[10], szBuffOut[10];
	BLOWFISH_CTX ctx;
	int i, j, len;

	sprintf(pathname, "%s\\hfprofile.xml", myFolder);
	pf = fopen(pathname, "w");
	if (!pf)
		return;

	//Tasvir Rohila - 25/02/2009 - bug#17212.
	//Encrypt the password before saving to hfprofile.xml
	memset(szData, '\0', sizeof(szData));
	strcpy(szData,pstack->ltpPassword);

	Blowfish_Init (&ctx, (unsigned char*)HASHKEY, HASHKEY_LENGTH);

	for (i = 0; i < sizeof(szData); i+=8)
		   Blowfish_Encrypt(&ctx, (unsigned long *) (szData+i), (unsigned long*)(szData + i + 4));
	szData[31] = '\0'; //important to NULL terminate;

	//Output of Blowfish_Encrypt() is binary, which needs to be stored in plain-text in hfprofile.xml for ezxml to understand and parse.
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
	//now output the contacts
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
	ezxml_t userid, password, server, lastupdate, dirty, status, vms, dated, fwd, email;
	char empty[] = "";
	struct AddressBook *p;
	unsigned char v, szData[32], szBuffIn[10], szBuffOut[10];
    BLOWFISH_CTX ctx;
	int i, j, len;

	strcpy(pstack->ltpServerName, "64.49.236.88");
	sprintf(pathname, "%s\\hfprofile.xml", myFolder);

	pf = fopen(pathname, "r");
	if (!pf)
	{
		
		return;
	}	
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
	//Decrypt the password read from hfprofile.xml
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
	{
		id = ezxml_child(vms, "id");
		if (!id)
			continue;
		contact = ezxml_child(vms, "u");
		dated = ezxml_child(vms, "dt");
		vmsUpdate(
			contact? contact->txt : "Unknown", 
			id->txt, 
			dated ? (unsigned long)atol(dated->txt) : 0,
			VMAIL_STATUS_NEW,
			VMAIL_IN
			);
	}

	ezxml_free(xml);
	free(strxml);
}

void profileClear()
{
	char	pathname[MAX_PATH];

	sprintf(pathname, "%s\\hfprofile.xml", myFolder);
	unlink(pathname);
	lastUpdate = 0;
	myTitle[0] = 0;
	myDID[0] = 0;
	fwdnumber[0] = 0;
	resetContacts();
	vmsEmpty();
	vmsSave();
	relistContacts();
	relistVMails();
}


void profileMerge(){
	FILE	*pf;
	char	pathname[MAX_PATH], *strxml;
	ezxml_t xml, contact, id, title, mobile, home, business, email, did, presence, dated;
	ezxml_t	status, vms, redirector, credit, token, fwd, mailserverip;
	struct AddressBook *pc;
	int		nContacts = 0, xmllength, newVmails;

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

	title = ezxml_child(xml, "t");
	if (title)
		strcpy(myTitle, title->txt);

	did = ezxml_child(xml, "did");
	if (did)
		strcpy(myDID, did->txt);

	redirector = ezxml_child(xml, "rd");
	if (redirector){
		if (!strcmp(redirector->txt, "online"))
			redirect = REDIRECT2ONLINE;
		else if (!strcmp(redirector->txt, "vms"))
			redirect = REDIRECT2VMS;
		else
			redirect = REDIRECT2PSTN;
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

	newVmails = 0;
	for (vms = ezxml_child(xml, "vm"); vms; vms = vms->next)
	{
		id = ezxml_child(vms, "id");
		if (!id)
			continue;
		contact = ezxml_child(vms, "u");
		dated = ezxml_child(vms, "dt");
		vmsUpdate(
			contact? contact->txt : "Unknown", 
			id->txt, 
			dated ? (unsigned long)atol(dated->txt) : 0,
			VMAIL_STATUS_NEW,
			VMAIL_IN);
		newVmails++;
	}

	ezxml_free(xml);
	free(strxml);
	fclose(pf);

	//relist vmails if any new ones have arrived
	if (newVmails){
		alert(-1, ALERT_NEWVMAIL, NULL);
		relistVMails();	
	}

	//relist contacts if any contacts have changed or are added
	if (nContacts){
		sortContacts();
		relistContacts();
	}

}




//the extras are char* snippets of xml has to be sent to the server in addition to 
//the xml that is already going (credentials + dirty contacts)
//DWORD WINAPI downloadProfile(LPVOID extras)
THREAD_PROC profileDownload(void *extras)
{
	char	data[10000], key[64], header[1000];
	int		length, ret, isChunked, contentLength, ndirty;
	char	pathUpload[MAX_PATH], pathDown[MAX_PATH];
	SOCKET sock;
	struct sockaddr_in	addr;
	FILE	*pfOut, *pfIn;
	struct AddressBook *pc;
	struct VMail *vm;

	if (busy > 0)
		return 0;
	else 
		busy = 1;

	httpCookie(key);

	//prepare the xml upload
	sprintf(pathUpload, "%s\\upload.xml", myFolder);
	pfOut = fopen(pathUpload, "wb");
	if (!pfOut){
		busy = 0;
		return 0;
	}
	fprintf(pfOut,  
	"<?xml version=\"1.0\"?>\n"
	"<profile>\n"
	" <u>%s</u>\n"
	" <key>%s</key> \n"
	" <since>%u</since> \n", 
	pstack->ltpUserid, key, lastUpdate);

	if (extras){
		int	param = (int)extras;
		switch(param){
			case REDIRECT2PSTN:
				fprintf(pfOut, " <rd>%s</rd>\n", fwdnumber);
				break;
			case REDIRECT2VMS:
				fprintf(pfOut, " <rd>vms</rd>\n");
				break;
			case REDIRECT2ONLINE:
				fprintf(pfOut, " <rd>online</rd>\n");
				break;
		}
	}

	//open this when you add conversations and msgeditor.cpp
/*
	//yay!! no blocks.
	for (pcon = conversations; pcon; pcon = pcon->next)
		for (psms = pcon->list; psms; psms =psms->next)
			if (psms->unsent && psms->direction == MSG_OUT){
				for (n = 0; n < pcon->npeers; n++){ //userid, content
					struct Peer *peer = pcon->peer + n;
					fprintf(pfOut, "<sms><dest>%s</dest><body>%s</body></sms>\n", peer->userid, psms->utfBody);
				}
				psms->unsent = 0;
			}
*/
	//check for new contacts
	ndirty = 0;
	for (pc = getContactsList(); pc; pc = pc->next)
		if (pc->dirty && !pc->id)
			ndirty++;
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
	for (pc = getContactsList(); pc; pc = pc->next)
		if (pc->dirty && pc->id && !pc->isDeleted)
			ndirty++;
	if (ndirty){
		fprintf(pfOut, "<mod>\n");
		for (pc = getContactsList(); pc; pc = pc->next)
			if (pc->dirty && pc->id && !pc->isDeleted)
				fprintf(pfOut,
				"<vc><id>%u</id><t>%s</t><m>%s</m><b>%s</b><h>%s</h><e>%s</e></vc>\n",
				(unsigned long)pc->id, pc->title, pc->mobile, pc->business, pc->home, pc->email);
		fprintf(pfOut, "</mod>\n");
	}

	//check for deleted contacts
	//these contacts have an id and isDeleted is 1
	ndirty = 0;
	for (pc = getContactsList(); pc; pc = pc->next)
		if (pc->dirty && pc->id && pc->isDeleted)
			ndirty++;

	for (vm = listVMails; vm; vm = vm->next)
		if (vm->status == VMAIL_STATUS_DELETE)
			ndirty++;

	if (ndirty){
		fprintf(pfOut, "<del>\n");
		for (pc = getContactsList(); pc; pc = pc->next)
			if (pc->dirty && pc->id && pc->isDeleted)
				fprintf(pfOut,
					" <vc><id>%u</id></vc>\n",
					(unsigned long)pc->id);

		for (vm = listVMails; vm; vm = vm->next)
			if (vm->status == VMAIL_STATUS_DELETE)
				fprintf(pfOut, " <vm><id>%s</id></vm>\n", vm->vmsid);
		fprintf(pfOut, "</del>\n");
	}

	fprintf(pfOut, "</profile>\n");
	contentLength = ftell(pfOut);
	fclose(pfOut);

	//prepare the http header
	sprintf(header, 
		"POST /cgi-bin/userxml.cgi HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Content-Length: %d\r\n\r\n",
		pstack->ltpServerName, contentLength);

	addr.sin_addr.s_addr = lookupDNS(pstack->ltpServerName);
	if (addr.sin_addr.s_addr == INADDR_NONE){
		busy = 0;
		return 0;
	}
	addr.sin_port = htons(80);	
	addr.sin_family = AF_INET;

	//try connecting the socket
	sock = (int)socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr))){
		busy = 0;
		return 0;
	}

	//send the http headers
	send(sock, header, strlen(header),0);
	
	//send the xml data
	sprintf(pathUpload, "%s\\upload.xml", myFolder);
	pfOut = fopen(pathUpload, "rb");
	if (!pfOut){
		busy = 0;
		return 0;
	}
	while((ret = fread(data, 1, sizeof(data), pfOut)) > 0)
		send(sock, data, ret, 0);
	fclose(pfOut);

	//read the headers
	//Tasvir Rohila - bug#18352 & #18353 - Initialize isChunked to zero for profile to download & 
	//Add/Delete contact to work.
	isChunked = 0;
	while (1){
		length = readNetLine(sock, data, sizeof(data));
		if (length <= 0)
			break;
		if (!strncmp(data, "Transfer-Encoding:", 18) && strstr(data, "chunked"))
			isChunked = 1;
		if (!strcmp(data, "\r\n"))
			break;
	}

	//prepare to download xml
	sprintf(pathDown, "%s\\down.xml", myFolder);
	pfIn = fopen(pathDown, "w");
	if (!pfIn){
		busy = 0;
		return 0;
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
				count -= length;
			}

			//read the \r\n after the chunk (if any)
			length = readNetLine(sock, data, 2);
			if (length != 2)
				break;
			//loop back to read the next chunk
		}
		else{ // read it in fixed blocks of data
			while (1){
				length = recv(sock, data, sizeof(data), 0);
				if (length > 0)
					fwrite(data, length, 1, pfIn);
				else 
					goto end;
			}
		}
	}
end:
	fclose(pfIn); // close the download.xml handle

	closesocket(sock);
	profileMerge();
	profileSave();
	relistContacts();
	refreshDisplay();
	vmsDownload();
	vmsUploadAll();
	relistVMails();
	busy = 0;
	return 0;
}

void profileResync()
{
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
}
