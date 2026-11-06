#ifndef WL4_MACOS_COMPAT_H
#define WL4_MACOS_COMPAT_H
#include <string.h>
#include <stdio.h>
#include "address.h"
void *PortDarwinMemcpy(void *,const void *,size_t);
void *PortDarwinMemset(void *,int,size_t);
int PortDarwinMemcmp(const void *,const void *,size_t);
size_t PortDarwinFwrite(const void *,size_t,size_t,FILE *);
#undef memcpy
#define memcpy PortDarwinMemcpy
#undef memset
#define memset PortDarwinMemset
#define memcmp PortDarwinMemcmp
#define fwrite PortDarwinFwrite
#endif
