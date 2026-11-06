#ifndef WL4_DARWIN_ADDRESS_H
#define WL4_DARWIN_ADDRESS_H
#include <stdint.h>
#define WL4_DARWIN_BANK UINT64_C(0x8000000000)
static inline uintptr_t PortDarwinHost(uintptr_t address) {
    return address >= 0x01000000 && address < 0x10000000 ? address + WL4_DARWIN_BANK : address;
}
static inline uintptr_t PortDarwinGba(uintptr_t address) {
    return address >= WL4_DARWIN_BANK && address < WL4_DARWIN_BANK + 0x10000000 ? address - WL4_DARWIN_BANK : address;
}
#endif
