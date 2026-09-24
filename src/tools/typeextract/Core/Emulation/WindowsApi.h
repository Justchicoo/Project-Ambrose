/*
 * Project Ambrose by Imjustchico
 * The Windows layer under the guest's C and C++ runtime: registers handlers for the kernel functions the runtime and the client's startup call, covering the heap, TLS and FLS, critical sections, SRW locks, events and one-time initialization, code page conversion and character types, SLists, module and export lookup, time and process information, each behaving like Windows for a single-threaded process.
 */

#ifndef AMBROSE_WINDOWSAPI_H
#define AMBROSE_WINDOWSAPI_H

class GuestProcess;

namespace WindowsApi
{
    void Register(GuestProcess& process);
}

#endif
