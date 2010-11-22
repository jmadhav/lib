/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2009 OpenVPN Technologies, Inc. <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "syshead.h"

#include "init.h"
#include "forward.h"
#include "multi.h"
#include "win32.h"
#include "openvpn.h"
#include "memdbg.h"

#include "forward-inline.h"
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
int normalExit = 1;
#define P2P_CHECK_SIG() EVENT_LOOP_CHECK_SIGNAL (c, process_signal_p2p, c);
struct context c;
static bool process_signal_p2p (struct context *c)
{
  remap_signal (c);
  return process_signal (c);
}
int breakLoop = 0;
int ThreadStartB = 0;
void normalExitFunction()
{
	
	int count = 0;
	if(ThreadStartB==0)
		return;
	sleep(1);
	breakLoop = 1;
	while(normalExit==0 || ThreadStartB==1)
	{
		printf("\n wait for thread exit");
		count++;
		if(count>20)break;
		sleep(1);
	}
}
static void
tunnel_point_to_point (struct context *c)
{
  context_clear_2 (c);

  /* set point-to-point mode */
  c->mode = CM_P2P;
	normalExit = 1;
  /* initialize tunnel instance */
  init_instance_handle_signals (c, c->es, CC_HARD_USR1_TO_HUP);
  if (IS_SIG (c))
    return;

  init_management_callback_p2p (c);
	breakLoop = 0;
	normalExit = 0;
  /* main event loop */
  while (true)
    {
      perf_push (PERF_EVENT_LOOP);
	  if(breakLoop)
	  {
		//breakLoop = 1;
		  normalExit = 1;
		  break;
	  }
      /* process timers, TLS, etc. */
      pre_select (c);
      P2P_CHECK_SIG();

      /* set up and do the I/O wait */
      io_wait (c, p2p_iow_flags (c));
      P2P_CHECK_SIG();

      /* timeout? */
      if (c->c2.event_set_status == ES_TIMEOUT)
	{
	  perf_pop ();
	  continue;
	}

      /* process the I/O which triggered select */
      process_io (c);
      P2P_CHECK_SIG();

      perf_pop ();
    }
  normalExit = 1;	
  uninit_management_callback ();

  /* tear down tunnel instance (unless --persist-tun) */
  close_instance (c);
}
void genrateReadSignal()
{
	 //readSignal(&c);
	
}
#undef PROCESS_SIGNAL_P2P
unsigned int writeDataCallbackL (void *uData,unsigned int *shost,unsigned short  *sportP,unsigned int *dhost,unsigned short  *dportP ,unsigned char *data,int *lenP)
{
	printf("\ndata= %s\n\n",data);
	return *lenP;
}
char 	datasend1[] ={69,0,1,157,172,154,0,0,128,17,242,158,192,168,150,6,64,39,3,65,31,124,19,196,1,137,65,101,82,69,71,73,83,84,69,82,32,115,105,112,58,115,112,111,107,110,46,99,111,109,32,83,73,80,47,50,46,48,13,10,86,105,97,58,32,83,73,80,47,50,46,48,47
	,85,68,80,32,49,57,50,46,49,54,56,46,49,55,51,46,49,51,58,56,48,54,48,59,114,112,111,114,116,59,98,114,97,110,99,104,61,122,57,104,71,52,98,75,80,106,101,57,56,53,48,49,97,97,101,99,55,57,52,55,55,49,97,49,99,56,48,50,53,100,55,55,56,101
	,57,97,99,51,13,10,77,97,120,45,70,111,114,119,97,114,100,115,58,32,55,48,13,10,70,114,111,109,58,32,60,115,105,112,58,49,49,50,50,51,51,52,64,115,112,111,107,110,46,99,111,109,62,59,116,97,103,61,98,51,54,97,49,52,99,55,52,101,50,56
	,52,50,100,99,57,99,48,53,99,100,51,97,51,54,55,54,56,99,49,100,13,10,84,111,58,32,60,115,105,112,58,49,49,50,50,51,51,52,64,115,112,111,107,110,46,99,111,109,62,13,10,67,97,108,108,45,73,68,58,32,50,55,49,48,50,48,102,48,53,98,100,97,
	52,51,49,50,98,50,100,100,52,49,53,57,57,101,56,52,101,50,100,101,13,10,67,83,101,113,58,32,51,51,55,54,53,32,82,69,71,73,83,84,69,82,13,10,67,111,110,116,97,99,116,58,32,60,115,105,112,58,49,49,50,50,51,51,52,64,49,57,50,46,49,54,56,46,
	49,55,51,46,49,51,58,56,48,54,48,62,13,10,69,120,112,105,114,101,115,58,32,51,48,48,13,10,67,111,110,116,101,110,116,45,76,101,110,103,116,104,58,32,32,48,13,10,13,10
};				
unsigned int readDataCallbackL (void *uData,unsigned int*shost,unsigned short *sportP,unsigned int*host,unsigned short *portP ,unsigned char *data,int *lenP)
{
	char *dataP;
	*host = inet_addr("64.39.3.65");
	
	*portP = 8060;
	dataP = datasend1 + sizeof(struct openvpn_iphdr)+sizeof(struct openvpn_udphdr);
	*lenP = sizeof(datasend1) - (sizeof(struct openvpn_iphdr)+sizeof(struct openvpn_udphdr));
	memmove(data,dataP,*lenP);
	
	return *lenP;
}
int vpnInitAndCall(char *conf,void *uData,readwriteDataCallback readP,readwriteDataCallback writeP,statusCallback statusP)
{
	//FILE *fp;
	OpenvpnInterfaceType openVpn;
	strcpy(openVpn.confFile,conf);
	openVpn.readCallP = readP;
	openVpn.writeCallP = writeP;
	openVpn.uData = uData;
	openVpn.statusCallP = statusP;
	setStatusCallbackforExit(uData,statusP);
	normalExit = 1;
	initAndStartOpenVpn(&openVpn,0,0);
	normalExit = 1;
	/*fp = fopen("test.txt","a");
	if(fp)
	{
		fprintf(fp,"\n openvpn exit successfull\n");
		fclose(fp);
	}*/
	printf("\n openvpn exit successfull");
	//sleep(10);
	return 0;
	

}
int testVPN()
{
	OpenvpnInterfaceType openVpn;
	strcpy(openVpn.confFile,"c:\\staticopenvpnssl\\sandbox.ovpn");
	openVpn.readCallP = readDataCallbackL;
	openVpn.writeCallP = writeDataCallbackL;
	initAndStartOpenVpn(&openVpn,0,0);
	return 0;


}
//data pointer
unsigned char * readData(unsigned char *buf, int len,unsigned int*srchostP,unsigned short *srcportP,unsigned int*dsthostP,unsigned short *dstportP ,int *lenP)
{
	struct openvpn_iphdr *ih;
	unsigned char *dataP,*newP;
	char *p;
	*lenP = 0;
	ih = ( struct openvpn_iphdr *) buf;
	p = (char*)(buf + sizeof(struct openvpn_iphdr));
	if (OPENVPN_IPH_GET_VER (ih->version_len) == 4)
	{	
	//	struct	in_addr srcHost,dstHost;
		*srchostP = ih->saddr;
		*dsthostP = ih->daddr;
		if(ih->protocol == OPENVPN_IPPROTO_UDP)
		{
			struct openvpn_udphdr *opP;
			opP = (struct openvpn_udphdr *)p;
			*srcportP = opP->source;
			*dstportP = opP->dest;//ntohs(opP->dest);
			*lenP = len -(sizeof(struct openvpn_iphdr)+sizeof(struct openvpn_udphdr));
			dataP = buf + sizeof(struct openvpn_iphdr)+sizeof(struct openvpn_udphdr);
			newP = malloc(len);
			memmove(newP,dataP,*lenP);
			
			return newP;
		}	
		
		//printf("\n nsrc=%s,ndest=%s", inet_ntoa(x),inet_ntoa(y));
		//return buf_advance (buf, offset);
		return 0;
	}  
	return 0;
}
int initAndStartOpenVpn(OpenvpnInterfaceType *openVpnP,int argc, char *argv[])
{
  
	int breakB = 0;
	char *myargv[2]={0};
#if PEDANTIC
  fprintf (stderr, "Sorry, I was built with --enable-pedantic and I am incapable of doing any real work!\n");
  return 1;
#endif
  initSig();
  memset(&c,0,sizeof(struct context));
	ThreadStartB = 1;
  CLEAR (c);
	breakLoop = 0;
  /* signify first time for components which can
     only be initialized once per program instantiation. */
  c.first_time = true;
	
  /* initialize program-wide statics */
  if (init_static ())
    {
      /*
       * This loop is initially executed on startup and then
       * once per SIGHUP.
       */
      do
	{
	  /* enter pre-initialization mode with regard to signal handling */
	  pre_init_signal_catch ();
		//printf("\n shankar jaikishan");
	  /* zero context struct but leave first_time member alone */
	  context_clear_all_except_first_time (&c);

	  /* static signal info object */
	  CLEAR (siginfo_static);
	  c.sig = &siginfo_static;

	  /* initialize garbage collector scoped to context object */
	  gc_init (&c.gc);

	  /* initialize environmental variable store */
	  c.es = env_set_create (NULL);
#ifdef MYWIN32
	  //env_set_add_win32 (c.es);
#endif

#ifdef ENABLE_MANAGEMENT
	  /* initialize management subsystem */
	  init_management (&c);
#endif

	  /* initialize options to default state */
	  init_options (&c.options, true);

	  /* parse command line options, and read configuration file */
		argc = 2;
		myargv[1] = openVpnP->confFile;//"/Users/mukesh/mygit/myvpnlibrary/staticopenvpnssl/sandbox.ovpn";
		
		//myargv[1] = TUNPATH"/spokn.ovpn";
		//myargv[1] = TUNPATH"/sandbox.opvn";
	  parse_argv (&c.options, argc, myargv, M_USAGE, OPT_P_DEFAULT, NULL, c.es);

#ifdef ENABLE_PLUGIN
	  /* plugins may contribute options configuration */
	  init_verb_mute (&c, IVM_LEVEL_1);
	  init_plugins (&c);
	  open_plugins (&c, true, OPENVPN_PLUGIN_INIT_PRE_CONFIG_PARSE);
#endif

	  /* init verbosity and mute levels */
	  init_verb_mute (&c, IVM_LEVEL_1);

	  /* set dev options */
	  init_options_dev (&c.options);
	  c.readCallbackP = openVpnP->readCallP;
	  c.writeCallbackP = openVpnP->writeCallP;
		c.statusCallP = 	openVpnP->statusCallP;
		c.uData = 	openVpnP->uData;
	  /* openssl print info? */
	  if (print_openssl_info (&c.options))
	    break;

	  /* --genkey mode? */
	  if (do_genkey (&c.options))
	    break;

	  /* tun/tap persist command? */
	  if (do_persist_tuntap (&c.options))
	    break;

	  /* sanity check on options */
	  options_postprocess (&c.options);

	  /* show all option settings */
	  show_settings (&c.options);

	  /* print version number */
	  msg (M_INFO, "%s", title_string);

	  /* misc stuff */
	  pre_setup (&c.options);

	  /* test crypto? */
	  if (do_test_crypto (&c.options))
	    break;
	  
#ifdef ENABLE_MANAGEMENT
	  /* open management subsystem */
	  if (!open_management (&c))
	    break;
#endif

	  /* set certain options as environmental variables */
	  setenv_settings (c.es, &c.options);

	  /* finish context init */
	  context_init_1 (&c);

	  do
	    {
	      /* run tunnel depending on mode */
	      switch (c.options.mode)
		{
		case MODE_POINT_TO_POINT:
			//printf("\n madan mohan");	
		  tunnel_point_to_point (&c);
				breakB = 1;
				if(breakLoop)
				{
					openVpnP->statusCallP(openVpnP->uData,3,0);//normal exit	
				}
				else
				{
					openVpnP->statusCallP(openVpnP->uData,2,0);//failed	
				}
				normalExit = 1;
		  break;
#if P2MP_SERVER
		case MODE_SERVER:
		  tunnel_server (&c);
		  break;
#endif
		default:
		  ASSERT (0);
		}

	      /* indicates first iteration -- has program-wide scope */
	      c.first_time = false;

	      /* any signals received? */
	      if (IS_SIG (&c))
		print_signal (c.sig, NULL, M_INFO);

	      /* pass restart status to management subsystem */
	      signal_restart_status (c.sig);
			if(breakB)
				break;
	    }
	  while (c.sig->signal_received == SIGUSR1);

	  uninit_options (&c.options);
	  gc_reset (&c.gc);
		if(breakB)
			break;
	}
		
      while (c.sig->signal_received == SIGHUP);
    }

  context_gc_free (&c);

  env_set_destroy (c.es);

#ifdef ENABLE_MANAGEMENT
  /* close management interface */
  close_management ();
#endif

  /* uninitialize program-wide statics */
  uninit_static ();

  openvpn_exit (OPENVPN_EXIT_STATUS_GOOD);  /* exit point */
  ThreadStartB = 0;
  return 0;			            /* NOTREACHED */

}
