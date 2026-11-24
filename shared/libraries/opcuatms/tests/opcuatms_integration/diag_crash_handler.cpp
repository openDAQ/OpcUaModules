#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#pragma comment(lib, "dbghelp.lib")

static LONG WINAPI diagHandler(EXCEPTION_POINTERS* info)
{
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW && code != 0xC0000374 && code != STATUS_HEAP_CORRUPTION)
        return EXCEPTION_CONTINUE_SEARCH;
    static volatile LONG once = 0;
    if (InterlockedExchange(&once, 1))
        return EXCEPTION_CONTINUE_SEARCH;
    fprintf(stderr, "DIAG exception 0x%08lX at %p thread %lu\n", code, info->ExceptionRecord->ExceptionAddress, GetCurrentThreadId());
    if (code == EXCEPTION_ACCESS_VIOLATION)
        fprintf(stderr, "DIAG access %s address %p\n", info->ExceptionRecord->ExceptionInformation[0] ? "write" : "read", (void*) info->ExceptionRecord->ExceptionInformation[1]);
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, nullptr, TRUE);
    CONTEXT ctx = *info->ContextRecord;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = ctx.Rip; frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp; frame.AddrStack.Mode = AddrModeFlat;
    char buf[sizeof(SYMBOL_INFO) + 512];
    for (int i = 0; i < 80; ++i)
    {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &frame, &ctx, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        auto sym = reinterpret_cast<SYMBOL_INFO*>(buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 511;
        DWORD64 disp = 0;
        DWORD disp32 = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        const char* name = SymFromAddr(proc, frame.AddrPC.Offset, &disp, sym) ? sym->Name : "?";
        if (SymGetLineFromAddr64(proc, frame.AddrPC.Offset, &disp32, &line))
            fprintf(stderr, "DIAG #%02d %s %s:%lu\n", i, name, line.FileName, line.LineNumber);
        else
            fprintf(stderr, "DIAG #%02d %s +0x%llx\n", i, name, disp);
    }
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

static const int diagInstalled = [] {
    ULONG guarantee = 64 * 1024;
    SetThreadStackGuarantee(&guarantee);
    AddVectoredExceptionHandler(1, diagHandler);
    return 0;
}();
#endif
