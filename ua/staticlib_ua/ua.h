#ifndef UAREST_DEFINED
#define UAREST_DEFINED
#ifdef __cplusplus
extern "C" {
#endif 


/* some architecture specifics */
#define THREAD_PROC DWORD WINAPI
#define START_THREAD(a) CreateThread(NULL, 0, (a), NULL, 0, NULL)

/* user agent's utility functions */
void httpCookie(char *cookie);
void cleanupNumber(char *number);
void urlencode(char *s, char *t) ;
void xmlEncode(char *s, char *t);
void strTrim(char *szString);

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

struct AddressBook{
	unsigned long		id;
	char	title[100];
	char	home[32];
	char	business[32];
	char	mobile[32];
	char	other[32];
	char	email[128];
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
struct AddressBook *addContact(char *title, char *mobile, char *home, char *business, char *other, char *email);
struct AddressBook *updateContact(unsigned long id, char *title, char *mobile, char *home, char *business, char *other, char *email);
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
#define VMAIL_NOTDOWNLOADED 1
#define VMAIL_DOWNLOADED 2
#define VMAIL_OPENED 3

#define VMAIL_OUT 1
#define VMAIL_IN 0
#define VMAIL_MAX 33000

#define VMAIL_STATUS_NEW 0
#define VMAIL_STATUS_DELETE 3
#define VMAIL_STATUS_DONE 4

//inbound vmail states
#define VMAIL_STATUS_READY 5
#define VMAIL_STATUS_OPENED 6

//outbound vmail states
#define VMAIL_STATUS_UPLOADED 7
#define VMAIL_STATUS_DELIVERED 8

struct VMail {
	char	userid[32];
	time_t	date;
	char	vmsid[100];
	int		status;
	int		direction;
	int		length; //in bytes
	struct VMail *next;
};
extern struct VMail *listVMails;


void vmsLoad();
struct VMail *vmsUpdate(char *userid, char *vmsid, time_t time, int status, int direction);
void vmsDelete(struct VMail *p);
void vmsSave();

/*
profile related definitions
*/
#define REDIRECT2ONLINE 1
#define REDIRECT2VMS 2
#define REDIRECT2PSTN 3

#define ALERT_NEWVMAIL 100
#define ALERT_VMAILERROR 101

/**
	profile functions
*/
void uaInit();
extern int redirect, creditBalance;
extern char fwdnumber[], myFolder[], vmFolder[], outFolder[], mailServer[], myTitle[], myDID[];
void profileResync();
void profileSave();
void profileSetRedirection(int redirectTo);
THREAD_PROC profileReloadEverything(void *something);
            
//to be implemented for every platform
#ifdef WINCE
extern int unlink(char *filename);
#endif
extern void relistVMails();
extern void relistContacts();
extern void relistCDRs();
extern void refreshDisplay();
extern void createFolders();
extern unsigned long ticks();


#ifdef __cplusplus
}
#endif
#endif