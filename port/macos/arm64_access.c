/* Decode only native AArch64 memory instructions; game logic remains native C.
 * Unknown encodings are rejected without changing registers or memory. */
#include "arm64_access.h"
#include <string.h>
static uint64_t ReadReg(mcontext_t m,unsigned r,int sp) {
 if(r<29)return m->__ss.__x[r];
 if(r==29)return m->__ss.__fp;
 if(r==30)return m->__ss.__lr;
 return sp?m->__ss.__sp:0;
}
static void WriteReg(mcontext_t m,unsigned r,uint64_t x,int sp) {
 if(r<29)m->__ss.__x[r]=x;
 else if(r==29)m->__ss.__fp=x;
 else if(r==30)m->__ss.__lr=x;
 else if(sp)m->__ss.__sp=x;
}
static int64_t Signed(uint64_t n,unsigned bits) {return (int64_t)(n<<(64-bits))>>(64-bits);}
unsigned PortArm64AccessSize(uint32_t i) {
 if((i&0x3ffffc00)==0x38bfc000)return 1u<<(i>>30);
 if((i&0x3fbffc00)==0x089ffc00)return 1u<<(i>>30);
 if((i&0x3b000000)==0x39000000 || (i&0x3b000000)==0x38000000) {
  unsigned size=i>>30,opc=(i>>22)&3,v=(i>>26)&1;
  return v?(opc&2?16:1u<<size):1u<<size;
 }
 if((i&0x3a000000)==0x28000000)return (i&(1u<<26))?(4u<<(i>>30)):((i>>31)?8:4);
 return 0;
}
static int Transfer(mcontext_t m,PortArmTransfer fn,uintptr_t a,unsigned r,unsigned n,int load,int vector,unsigned sign,unsigned dest64) {
 unsigned char data[16]={0};
 if(!load) {
  if(vector)memcpy(data,&m->__ns.__v[r],n);
  else {uint64_t value=ReadReg(m,r,0);memcpy(data,&value,n);}
 }
 if(!fn(a,data,n,!load))return 0;
 if(load) {
  if(vector){m->__ns.__v[r]=0;memcpy(&m->__ns.__v[r],data,n);}
  else {uint64_t value=0;memcpy(&value,data,n);if(sign)value=Signed(value,n*8);if(!dest64)value=(uint32_t)value;WriteReg(m,r,value,0);}
 }
 return 1;
}
int PortArm64Emulate(uint32_t i,mcontext_t m,PortArmTransfer fn) {
 unsigned rn=(i>>5)&31,rt=i&31,v=(i>>26)&1,n,load,sign=0,dest64=0;
 uint64_t base=ReadReg(m,rn,1),address;int64_t off=0;int wb=0;
 if((i&0xbf200000)==0x0d000000) {
  unsigned opcode=(i>>13)&7,S=(i>>12)&1,size=(i>>10)&3,Q=(i>>30)&1,lane;
  if(opcode==0){n=1;lane=(Q<<3)|(S<<2)|size;}
  else if(opcode==2 && !(size&1)){n=2;lane=(Q<<2)|(S<<1)|(size>>1);}
  else if(opcode==4 && size==0){n=4;lane=(Q<<1)|S;}
  else if(opcode==4 && size==1 && !S){n=8;lane=Q;}
  else return 0;
  unsigned char value[8];load=(i>>22)&1;
  memcpy(value,(unsigned char *)&m->__ns.__v[rt]+lane*n,n);
  if(!fn(base,value,n,!load))return 0;
  if(load)memcpy((unsigned char *)&m->__ns.__v[rt]+lane*n,value,n);
  if(i&(1u<<23)){unsigned rm=(i>>16)&31;WriteReg(m,rn,base+(rm==31?n:ReadReg(m,rm,0)),1);}
 } else if((i&0x3ffffc00)==0x38bfc000) {
  n=1u<<(i>>30);
  if(!Transfer(m,fn,base,rt,n,1,0,0,n==8))return 0;
 } else if((i&0x3fbffc00)==0x089ffc00) {
  n=1u<<(i>>30);load=(i>>22)&1;
  if(!Transfer(m,fn,base,rt,n,load,0,0,n==8))return 0;
 } else if((i&0x3b000000)==0x39000000 || (i&0x3b000000)==0x38000000) {
  unsigned size=i>>30,opc=(i>>22)&3;
  n=PortArm64AccessSize(i);load=opc&1;
  if(!v){if(opc>=2){load=1;sign=1;}dest64=(size==3 || opc==2);if(size==3 && opc>=2)return 0;}
  if((i&0x3b000000)==0x39000000)off=((i>>10)&4095)*n;
  else if(i&(1u<<21)) {
   if((i&0xc00)!=0x800)return 0;
   uint64_t index=ReadReg(m,(i>>16)&31,0);unsigned option=(i>>13)&7;
   if(option==2)index=(uint32_t)index;
   else if(option==6)index=(int64_t)(int32_t)index;
   else if(option!=3 && option!=7)return 0;
   off=(int64_t)index;if(i&(1u<<12))off*=n;
  } else {
   unsigned mode=(i>>10)&3;
   if(mode==2)return 0;
   off=Signed((i>>12)&511,9);wb=(mode==1 || mode==3);
   if(mode==1){address=base;goto single;}
  }
  address=base+off;
 single:
  if(!Transfer(m,fn,address,rt,n,load,v,sign,dest64))return 0;
  if(wb)WriteReg(m,rn,base+off,1);
 } else if((i&0x3a000000)==0x28000000) {
  unsigned opc=i>>30,mode=(i>>23)&3,rt2=(i>>10)&31;
  load=(i>>22)&1;
  if(v){if(opc==3)return 0;n=4u<<opc;}
  else {if(opc==3 || (opc==1 && !load))return 0;n=opc==2?8:4;sign=opc==1;dest64=opc!=0;}
  off=Signed((i>>15)&127,7)*n;address=base+(mode==1?0:off);
  if(!Transfer(m,fn,address,rt,n,load,v,sign,dest64))return 0;
  if(!Transfer(m,fn,address+n,rt2,n,load,v,sign,dest64))return 0;
  if(mode==1 || mode==3)WriteReg(m,rn,base+off,1);
 } else return 0;
 m->__ss.__pc+=4;return 1;
}
/* Existing I/O frontend calls this architecture-neutral role by its old name. */
unsigned PortX86AccessSize(const uint8_t *code) {uint32_t i;memcpy(&i,code,4);return PortArm64AccessSize(i);}
