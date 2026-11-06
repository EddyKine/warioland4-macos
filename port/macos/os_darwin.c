#include "port/os.h"
#include "address.h"
#include "arm64_access.h"
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

static PortFaultCallback sFault;
static PortRedirectCallback sRedirect;
static PortTrapCallback sBefore,sAfter;
static uintptr_t sIoBase;
static size_t sIoSize;
static uint8_t *sIo;
static int sInstalled;
static uint64_t sAccesses;
struct Range {uintptr_t low;size_t size;int writable;};
static struct Range sRanges[16];static unsigned sRangeCount;
static int Flags(enum PortProtection p) {
 return p==PORT_NO_ACCESS?PROT_NONE:p==PORT_READ_ONLY?PROT_READ:PROT_READ|PROT_WRITE;
}
int PortOsReserveFixed(uintptr_t address,size_t size) {
 void *wanted=(void *)PortDarwinHost(address);
 /* Never replace an existing mapping, including an image loaded by dyld. */
 mach_vm_address_t fixed=(mach_vm_address_t)wanted;
 kern_return_t kr=mach_vm_allocate(mach_task_self(),&fixed,size,VM_FLAGS_FIXED);
 if(kr!=KERN_SUCCESS){fprintf(stderr,"mach_vm_allocate(%p, %zu): %s\n",wanted,size,mach_error_string(kr));errno=EADDRINUSE;return -1;}
 void *p=(void *)fixed;
 if(p!=wanted){mach_vm_deallocate(mach_task_self(),fixed,size);errno=EADDRINUSE;return -1;}
 if(mprotect(p,size,PROT_NONE)){mach_vm_deallocate(mach_task_self(),fixed,size);return -1;}
 return 0;
}
int PortOsProtect(uintptr_t address,size_t size,enum PortProtection p) {
 uintptr_t host=PortDarwinHost(address),page=(uintptr_t)getpagesize();
 /* GBA subregions share host pages; round without spilling into another reservation. */
 uintptr_t start=host&~(page-1),end=(host+size+page-1)&~(page-1);
 if(p==PORT_READ_WRITE_GUARD)p=PORT_NO_ACCESS;
 if(mprotect((void *)start,end-start,Flags(p)))return -1;
 if(p!=PORT_NO_ACCESS) {
  unsigned i;for(i=0;i<sRangeCount;i++)if(sRanges[i].low==address)break;
  if(i==sRangeCount){if(sRangeCount>=16)return -1;sRangeCount++;}
  sRanges[i]=(struct Range){address,size,p!=PORT_READ_ONLY};
 }
 return 0;
}
void PortOsDescribeAddress(uintptr_t address,char *buffer,size_t size) {
 snprintf(buffer,size,"Darwin backing address %p: %s",(void *)PortDarwinHost(address),strerror(errno));
}
void PortOsWriteError(const char *message){write(STDERR_FILENO,message,strlen(message));}
void PortOsShowErrors(int show){(void)show;}
uintptr_t PortOsExecutableOffset(uintptr_t pc){return pc-(uintptr_t)_dyld_get_image_header(0);}
static int Transfer(uintptr_t address,void *value,size_t size,int writeAccess) {
 uintptr_t low=PortDarwinGba(address),host=0;
 if(low>=sIoBase && low+size<=sIoBase+sIoSize) {
  /* The native instruction is emulated, so no single-step or writable game I/O page is needed. */
  uintptr_t pc=0; /* Set by the handler below. */
  extern uintptr_t gPortDarwinFaultPc;pc=gPortDarwinFaultPc;
  sBefore(low,writeAccess,pc);
  if(writeAccess)memcpy(sIo+low-sIoBase,value,size);else memcpy(value,sIo+low-sIoBase,size);
  sAfter(low,writeAccess,pc);return 1;
 }
 for(unsigned i=0;i<sRangeCount;i++) {
  struct Range *r=&sRanges[i];
  if(low>=r->low && low+size<=r->low+r->size) {
   if(writeAccess && !r->writable)return 0;
   host=PortDarwinHost(low);break;
  }
 }
 if(!host && low<=UINT32_MAX && sRedirect) {
  host=sRedirect(low);if(host)host=PortDarwinHost(host);
 }
 if(!host)return 0;
 if(writeAccess)memcpy((void *)host,value,size);else memcpy(value,(void *)host,size);
 return 1;
}
uintptr_t gPortDarwinFaultPc;
static void Signal(int sig,siginfo_t *info,void *context) {
 ucontext_t *uc=context; mcontext_t m=uc->uc_mcontext;
 uintptr_t pc=m->__ss.__pc,address=(uintptr_t)info->si_addr;
 uint32_t instruction=*(const uint32_t *)pc;
 uintptr_t previous=gPortDarwinFaultPc;gPortDarwinFaultPc=pc;
 if(PortDarwinGba(address)<=UINT32_MAX && PortArm64Emulate(instruction,m,Transfer)) {
  gPortDarwinFaultPc=previous;sAccesses++;return;
 }
 gPortDarwinFaultPc=previous;
 char message[256];
 snprintf(message,sizeof(message),"Darwin unresolved signal %d address=%p pc=%p offset=0x%llx instruction=%08x after %llu translated accesses\n",sig,(void *)address,(void *)pc,(unsigned long long)PortOsExecutableOffset(pc),instruction,(unsigned long long)sAccesses);
 PortOsWriteError(message);
 if(sFault)sFault(PORT_FAULT_READ,PortDarwinGba(address),pc,m->__ss.__sp);
 _exit(128+sig);
}
static void Install(void) {
 if(sInstalled)return;sInstalled=1;
 stack_t stack={0};stack.ss_size=1024*1024;
 stack.ss_sp=mmap(NULL,stack.ss_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
 if(stack.ss_sp==MAP_FAILED || sigaltstack(&stack,NULL))abort();
 struct sigaction action={0};action.sa_sigaction=Signal;sigemptyset(&action.sa_mask);
 action.sa_flags=SA_SIGINFO|SA_ONSTACK|SA_NODEFER;
 if(sigaction(SIGSEGV,&action,NULL)||sigaction(SIGBUS,&action,NULL)||sigaction(SIGILL,&action,NULL))abort();
}
void PortOsInstallFaultHandler(PortFaultCallback callback){sFault=callback;Install();}
void PortOsRedirectFaults(PortRedirectCallback callback){sRedirect=callback;Install();}
int PortOsMapTrappedRegion(uintptr_t address,size_t reserve,size_t size,uint8_t **view,PortTrapCallback before,PortTrapCallback after) {
 (void)reserve;sIoBase=address;sIoSize=size;
 sIo=mmap(NULL,getpagesize(),PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
 if(sIo==MAP_FAILED)return -1;
 *view=sIo;sBefore=before;sAfter=after;Install();return 0;
}
