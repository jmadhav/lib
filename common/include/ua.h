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
#pragma pack(4)

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
	/* some architecture specifics */
#ifdef _MACOS_	
#include "netlayer.h"
#include <pthread.h>
#define MAX_PATH 256
#define SOCKET int
#ifndef TRUE
#define TRUE 1
#endif	
#ifndef FALSE
#define FALSE 0
#endif	
//#define _STAGING_SERVER_	
#define closesocket close
	
	//pthread_create(&ltpInterfaceP->pthObj, 0,PollThread,ltpInterfaceP);
	
#define THREAD_PROC void * 
	#define START_THREAD(a) {  pthread_t pt; pthread_create(&pt, 0,a,0);	 }
#define THREAD_EXIT(X)        pthread_exit(X)
#else	

#define THREAD_PROC DWORD WINAPI
#define START_THREAD(a) CreateThread(NULL, 0, (a), NULL, 0, NULL)
#define THREAD_EXIT(X)        ExitThread(X)
	
#endif	
	typedef struct LogoutStructType
	{
		char	 ltpUserid[40];
		char  ltpPassword[40]; 
		char  ltpServerName[40];
		char  ltpNonce[20];
		int   bigEndian;
		
	} LogoutStructType;	
	void loggedOut();
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
#ifdef _MACOS_
		int recordUId;
		int recordID;
		int addressUId;
		int isexistRecordID;
		int uniqueID;
	#ifdef  _MAC_OSX_CLIENT_
		char uniqueId[64];
	#endif
#endif	
	};
	extern struct CDR *listCDRs;
	
	void cdrLoad();
	#ifdef _MACOS_
		#ifdef _MAC_OSX_CLIENT_
			void cdrAdd(char *userid, time_t time, int duration, int direction ,int abid,int recordid,char *Uid);
		#else
			void cdrAdd(char *userid, time_t time, int duration, int direction ,int abid,int recordid);
		#endif
	#else
		void cdrAdd(char *userid, time_t time, int duration, int direction);
	#endif
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
#define UA_LOGIN_SUCCESSFULL 22
#define SWITCH_TO_SIP        23
	
	struct AddressBook{
		unsigned long		id;
		char	title[100];
		char	home[32];
		char	business[32];
		char	mobile[32];
		char	other[32];
		char	email[128];
		char    spoknid[9];
		int		presence;
		int		isFavourite;
		int		dirty;
		int		isDeleted;
		struct AddressBook *next;
	#ifdef _MACOS_
		int uniqueID;
	#endif	
		
		
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
	
	int encode(unsigned s_len, char *src, unsigned d_len, char *dst);
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
#pragma pack(4)	
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
#ifdef _MACOS_
		int recordUId;
		int isexistRecordID;
		int uniqueID;
	#ifdef  _MAC_OSX_CLIENT_
		char uniqueId[40];
	#endif
#endif	
		
	};
	extern struct VMail *listVMails;
	
	
	void vmsLoad();
#ifdef _MACOS_

#ifdef  _MAC_OSX_CLIENT_ //If Mac OSx Client
		struct VMail *vmsUpdate(char *userid, char *hashid, char *vmsid, time_t time, int status, int direction,int laddressUId,int lrecordID,char *uId);
#else	// else iPhone client
	struct VMail *vmsUpdate(char *userid, char *hashid, char *vmsid, time_t time, int status, int direction,int laddressUId,int lrecordID);
#endif

#else
	struct VMail *vmsUpdate(char *userid, char *hashid, char *vmsid, time_t time, int status, int direction);
#endif
	//struct VMail *vmsUpdate(char *userid, char *hashid, char *vmsid, time_t time, int status, int direction);
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
//#define ALERT_SERVERMSG	102
#define ALERT_HOSTNOTFOUND 110
	
#define ALERT_THREADTERMINATED 111
#define ALERT_USERID_TAG_FOUND    112
#define ALERT_USERID_TAG_NOTFOUND 113
	//Seperator for href & message, sent by server.
#define SEPARATOR '|'

	/**
	 profile functions
	 */
	
	void uaInit();
	extern int redirect, creditBalance, bandwidth, settingType, oldSetting, bkupsettingType;
	extern char fwdnumber[], oldForward[], myFolder[], vmFolder[], outFolder[], mailServer[], myTitle[], myDID[],client_name[],client_ver[],client_os[],client_osver[],client_model[],client_uid[];
	void profileResync();
	void profileClear();
	void profileSave();
	void profileSetRedirection(int redirectTo);
	THREAD_PROC profileReloadEverything(void *something);
	void UaThreadEnd();
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
#ifdef _MAC_OSX_CLIENT_
	extern void threadStopped();
#endif
	void cdrEmpty();
	//for voip quality
	
	void setBandwidth(unsigned long timeTaken,int byteCount);

	THREAD_PROC sendLogOutPacket(void *lDataP);
	void TerminateUAThread();
	void ReStartUAThread();
	int getThreadState();

#ifdef _MACOS_
#define TEST_CALL_ID -2
#define _T(X) X
#define UA_ALERT     2000	
#define UA_ERROR_ALERT 2001

#define REFRESH_CONTACT 1	
#define REFRESH_VMAIL   2	
	
#define REFRESH_CALLLOG    3	
#define REFRESH_ALL       4	
#define LOAD_ADDRESS_BOOK 5	
#define STOP_ANIMATION 6
#define BEGIN_THREAD 7	
#define END_THREAD 8	
#define USERNAME_RANGE  30
#define PASSWORD_RANGE  30	

#define REFRESH_DIALER	11

#ifdef _MAC_OSX_CLIENT_
	#define THREAD_STARTED  12
	#define	THREAD_STOPPED  13
#endif
	
#define EMAIL_RANGE  127
#define NUMBER_RANGE  31	
#define CONTACT_RANGE 98	
#define SPOKN_ID_RANGE 7	
	
	
	typedef enum 
		{
			GETCONTACTLIST=1,
			GETVMAILLIST = 2,
			GETCALLLOGLIST = 3,
			GETCALLLOGMISSEDLIST = 4,
			GETVMAILUNDILEVERD = 5
			
		}UAObjectType;
	
	typedef char * (*GetPathFunPtr)(void *uData);
	typedef void(*CreateDirectoryFunPtr)(void *uData,char *pathCharP);
	typedef void (*RelistVMailsFunPtr)(void *uData);
	typedef void (*RelistContactsFunPtr)(void *uData);
	typedef void (*RelistCDRsFunPtr)(void *uData);
	typedef void (*RefreshDisplayFunPtr)(void *uData);
	typedef void (*CdrRemoveAllFunPtr)(void *uData);
	
	typedef int (*AlertNotificationCallbackUAP)(int type,unsigned int valLong,int valSubLong, unsigned long userData,void *otherInfoP);
	
	
	typedef struct UACallBackType
		{
			void *uData;
			AlertNotificationCallbackUAP alertNotifyP;
			GetPathFunPtr		pathFunPtr;
			
			CreateDirectoryFunPtr	   creatorDirectoryFunPtr;
			RelistVMailsFunPtr relistVMailsFunPtr;
			RelistContactsFunPtr relistContactsFunPtr;
			RelistCDRsFunPtr relistCDRsFunPtr;
			RefreshDisplayFunPtr refreshDisplayFunPtr;
			CdrRemoveAllFunPtr cdrRemoveAllFunPtr;
			
			
		}UACallBackType,*UACallBackPtr;
 	void UACallBackInit(UACallBackPtr uaCallbackP,struct ltpStack *pstackP);
	int GetTotalCount(UAObjectType uaObj);
	void * GetObjectAtIndex(UAObjectType uaObj,int index);
	int GetVmsFileName(struct VMail *vmailP,char **fnameWithPathP);
	int makeVmsFileName(char *fnameP,char **fnameWithPathP);
#ifdef _MAC_OSX_CLIENT_
	int sendVms(char *remoteParty,char *vmsfileNameP,int laddressUId,int lrecordID,char *uId);
	
#else
	int sendVms(char *remoteParty,char *vmsfileNameP,int laddressUId,int lrecordID);
#endif
	//int sendVms(char *remoteParty,char *vmsfileNameP);
	int getBalance();
	void SetDeviceDetail(char *lclientName,char *clientVer,char *lclientOs,char *lclientOsVer,char *lclientModel,char *clientUId);
	char *getForwardNo(int *forwarddP);
	char *getDidNo();
	int newVMailCount();
	void newVMailCountdecrease();
	struct AddressBook * getContactAndTypeCall(char *noCharP,/*out*/char *ltypeCharP);
	char *getTitle();
	int vmsDeleteByID(char *idCharP);
	char *getAccountPage();
	char *getCreditsPage();
	char *getPayPalPage();
	void SetOrReSetForwardNo(int forwardB, char *forwardNoCharP);
	char *getOldForwardNo();
	int resetMissCallCount();
	void saveMissCall();
	void loadMissCall();
	int getMissCount();
	int incriseMissCallCount();
	void * GetObjectByUniqueID(UAObjectType uaObj ,int luniqueId);
	
#endif		
	
	extern void relistVMails();
	extern void relistContacts();
	extern void relistCDRs();
	extern void refreshDisplay();
	extern void createFolders();
	extern void cdrRemoveAll();
	extern unsigned long ticks();
	void cdrEmpty();
	char *NormalizeNumber(char *lnoCharP,int type);
	char *NormalizeBoth(char *lnoCharP);
	int validateNo(char *numberP);
	
	//for voip quality
	void setBandwidth(unsigned long timeTaken,int byteCount);
	int validName(char*inP);
	int terminateThread();
	void relistAll();

#ifdef _MAC_OSX_CLIENT_
	struct AddressBook * getContactList();
	int getCreditBalance();
	struct CDR *getCallList();
	int getVmailCount();
	struct VMail *getVMailList();	
#endif
	
	void stopAnimation();
	char *getSupportPage();
	void UaThreadBegin();
	void applicationEnd();
	char* getencryptedPassword();
#define	IDS_LTP_SERVERIP	"www.spokn.com"
#define _FORWARD_VMS_
#ifdef __cplusplus
}



#endif
#endif
