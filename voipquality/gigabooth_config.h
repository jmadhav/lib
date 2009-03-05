#ifndef _GIGABOOTH_CONFIG_
	#define _GIGABOOTH_CONFIG_
	//#define PALM_OS
	#define WIN_OS
	#ifdef PALM_OS
	#include <PalmOS.h>
	#else
	
	typedef signed char		Int8;
	typedef signed short	Int16;
	typedef signed long		Int32;
	typedef unsigned char	UInt8;
	typedef unsigned short  UInt16;
	typedef unsigned long   UInt32;
	typedef void*			MemPtr;
	typedef char			Char;
	typedef wchar_t			WChar;
	#endif
	//#define DEBUG_LIB
	#ifdef DEBUG_LIB
		#define CONST_MEMORY_WRITE_BEFORE_FREE  "Free"
	#endif
#endif
