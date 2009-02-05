#include <windows.h>
#include "mystring.h"

/*
void *memcpy(void *to, void *from, size_t count)
{
	char *q, *p;
	int	i;
	q = (char *) from;
	p = (char *) to;

	for (i = 0; i < count; i++)
		*p++ = *q++;

	return to;
}

void *memset(void *mem, int byte, size_t count)
{
	int i;
	char *p = (char *)mem;

	for (i = 0; i < count; i++, p++)
		*p = byte;
	return mem;
}
*/
long StringToNumber(char *string)
{
	long number = 0;
	while (*string >= '0' && *string <= '9')
		number = (number * 10) + (*string++ - '0');
	return number;
}

char toLower(char c)
{
	if (c >= 'A' && c <= 'Z')
		c += 32;
	return c;
}

/*
void toLower(char *string)
{
	for (;*string;string++)
		*string = toLower(*string);
}
*/
int isDigit(char c)
{
	if (c >= '0' && c <= '9')
		return 1;
	else
		return 0;
}

int isDigitLetter(char c)
{
	if ((c >= 0 && c <= '9')
		|| (c >= 'A' && c <= 'Z')
		|| (c >= 'a' && c <= 'z'))
		return 1;
	else
		return 0;
}
/*
char * strstr (const char * str1,const char * str2)
{
	char *cp = (char *) str1;
    char *s1, *s2;

	if(!*str2)
		return((char *)str1);

	while (*cp)
	{
		s1 = cp;
		s2 = (char *) str2;

			while ( *s1 && *s2 && !(*s1-*s2) )
                        s1++, s2++;

		if (!*s2)
			return(cp);

		cp++;
	}

    return(NULL);
}

int  strncmp (const char * first, const char * last, size_t count)
{
	if (!count)
		return(0);

	while (--count && *first && *first == *last)
	{
		first++;
		last++;
    }

    return( *(unsigned char *)first - *(unsigned char *)last );
}
*/