#pragma once

// Function-like on purpose: `#if ARCH_X64()` is a compile error when this
// header was not included, instead of silently evaluating to 0.
#if defined(_M_IX86) || defined(__i386__)
#define ARCH_X86() 1
#define ARCH_X64() 0
#define ARCH_THISCALL __thiscall
#elif defined(_M_X64) || defined(__x86_64__)
#define ARCH_X86() 0
#define ARCH_X64() 1
#define ARCH_THISCALL
#else
#error "Unsupported architecture: build the x86 or x64 profile"
#endif
