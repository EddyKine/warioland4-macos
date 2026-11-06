#ifndef WL4_ARM64_ACCESS_H
#define WL4_ARM64_ACCESS_H
#include <stdint.h>
#include <stddef.h>
#include <sys/ucontext.h>
typedef int (*PortArmTransfer)(uintptr_t address, void *value, size_t size, int write);
int PortArm64Emulate(uint32_t instruction, mcontext_t state, PortArmTransfer transfer);
unsigned PortArm64AccessSize(uint32_t instruction);
#endif
