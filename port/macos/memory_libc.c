#include "compat.h"
#undef memcpy
#undef memset
#undef memcmp
#undef fwrite
/* Only RAM/ROM aliases. I/O must still go through its emulated access callbacks. */
static void *Alias(const void *p) {uintptr_t a=(uintptr_t)p;if(a>=0x01000000 && a<0x10000000 && a>>24!=4)a=PortDarwinHost(a);return (void *)a;}
void *PortDarwinMemcpy(void *d,const void *s,size_t n){memcpy(Alias(d),Alias(s),n);return d;}
void *PortDarwinMemset(void *d,int v,size_t n){memset(Alias(d),v,n);return d;}
int PortDarwinMemcmp(const void *a,const void *b,size_t n){return memcmp(Alias(a),Alias(b),n);}
size_t PortDarwinFwrite(const void *p,size_t n,size_t c,FILE *f){return fwrite(Alias(p),n,c,f);}
