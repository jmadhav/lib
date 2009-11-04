#ifndef UAREST_DEFINED
#define UAREST_DEFINED
#ifdef __cplusplus
extern "C" {
#endif 
/*
	Author Mukesh Sharma
	Date   07/05/2009
	Bug Id : ->20359
	This enum used for thread status.This show status of thread
*/
typedef enum 
{	ThreadNotStart,
	ThreadStart,
	ThreadTerminate,

}ThreadStatusEnum;

/* some architecture specifics */
#define THREAD_PROC DWORD WINAPI
#define START_THREAD(a) CreateThread(NULL, 0, (a), NULL, 0, NULL)

/* user agent's utility functions */
void httpCookie(char *cookie);
void cleanupNumber(char *number);
void urlencode(char *s, char *t) ;
void xmlEncode(char *s, char *t);
void strTrim(char *szString);

/* encoders and decoders for base64 */
void encodeblock( unsigned char in[3], unsigned char out[4], int len );
void decodeblock( unsigned char in[4], unsigned char out[3] );
extern char uaUserid[32];
/**
	call history - list of CDRs
*/

#define CDR_V1		0x00010001
struct CDR {
	char	title[128];
	char	userid[32];
	time_t	date;
	int		duration;
	int		direction;
	struct CDR *next;
};
extern struct CDR *listCDRs;

void cdrLoad();
void cdrAdd(char *userid, time_t time, int duration, int direction);
void cdrRemove(struct CDR *p);
void cdrRemoveAll();
/**
Contacts

All contacts are stored in a linked list.
This is the implementation of managing the contacts
*/

#define DIRTY_UPDATE 2
#define DIRTY_DELETE 3

#define VCARD_HOME 1
#define VCARD_BUSINESS 2
#define VCARD_MOBILE 3
#define VCARD_OTHER 4
#define VCARD_EMAIL 5
#define VCARD_SPOKNID 6

struct AddressBook{
	unsigned long		id;
	char	title[101];
	char	home[33];
	char	business[33];
	char	mobile[33];
	char	other[33];
	char	email[128];
	char    spoknid[8];
	int		presence;
	int		isFavourite;
	int		dirty;
	int		isDeleted;
	struct AddressBook *next;
};

extern struct ltpStack *pstack;
int isMatched(char *title, char *query);
int indexOf(char *title, char *query);
extern struct AddressBook *listContacts;
void resetContacts();
struct AddressBook *addContact(char *title, char *mobile, char *home, char *business, char *other, char *email, char *spoknid);
struct AddressBook *updateContact(unsigned long id, char *title, char *mobile, char *home, char *business, char *other, char *email, char *spoknid);
//void updateContactPresence(int id, int presence);
void deleteContactLocal(int index);
//void deleteContactOnServer(int id);

struct AddressBook *getTitleOf(char *userid, char *title);
struct AddressBook *getContactOf(char *userid);
//void httpCookie(char *cookie);
//int countOfContacts();
//struct AddressBook *getContactsList();
struct AddressBook *getContact(int id);
void sortContacts();

/**
Voicemails

All the voicemails are in a linked list too
*/

#define VMAIL_OUT 1
#define VMAIL_IN 0
#define VMAIL_MAX 33000

#define VMAIL_NEW 0
#define VMAIL_ACTIVE 1
#define VMAIL_DELIVERED 2
#define VMAIL_FAILED 3

#define VMAIL_MAXCOUNT 100 //no more than VMAIL_MAXCOUNT mails to be stored on the user agent

struct VMail {
	char	userid[128]; //bug#26028, increased size to match AddressBook->email;
	time_t	date;
	char	vmsid[100];
	char	hashid[33];
	short	status;
	short	direction;
	short	deleted; //those deleted at the server
	short	toDelete; //those marked by the user to be deleted from the client as well as the server
	short	isNew; //not to be saved
	struct VMail *next;
	int dirty; 
};
extern struct VMail *listVMails;


void vmsLoad();
struct VMail *vmsUpdate(char *userid, char *hashid, char *vmsid, time_t time, int status, int direction);
void vmsDelete(struct VMail *p);
void vmsSave();
void vmsEmpty();
/*
profile related definitions
*/
#define REDIRECT2ONLINE 2
#define REDIRECT2VMS 0
#define REDIRECT2PSTN 1
#define REDIRECTBOTH 3 //both online and pstn

#define ALERT_NEWVMAIL 100
#define ALERT_VMAILERROR 101
//Handler for server-sent messages
#define ALERT_SERVERMSG	102
//Seperator for href & message, sent by server.
#define SEPARATOR '|'
/**
	profile functions
*/

void uaInit();
extern int redirect, creditBalance, bandwidth, settingType, oldSetting;
extern char fwdnumber[], myFolder[], vmFolder[], outFolder[], mailServer[], myTitle[], myDID[],client_name[],client_ver[],client_os[],client_osver[],client_model[],client_uid[];
void profileResync();
void profileClear();
void profileSave();
void profileGetKey();
void profileSetRedirection(int redirectTo);
THREAD_PROC profileReloadEverything(void *something);

//
//	Hash Key & length used by Encryption / Decryption functions
//
#define HASHKEY				"{E5FD9B84-93DC-451d-AE10-FEEDD18F445D}"
#define HASHKEY_LENGTH		38
//to be implemented for every platform
#ifdef WINCE
extern int unlink(char *filename);
#endif
extern void relistVMails();
extern void relistContacts();
extern void relistCDRs();
extern void refreshDisplay();
extern void createFolders();
extern void cdrRemoveAll();
extern unsigned long ticks();
void cdrEmpty();
//for voip quality
void setBandwidth(unsigned long timeTaken,int byteCount);
#define	IDS_LTP_SERVERIP	"www.spokn.com"
#ifdef __cplusplus
}



#endif
#endif
