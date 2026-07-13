#include "exception_handling.h"

#include<iostream>
#include<unordered_map>
#include<windows.h>

//THIS DIDN'T DO ANYTHING SO FAR
// 1. Define the Callback Function
// This gets triggered when the "Unknown Signal" operating system exception happens
LONG WINAPI crashCallback(EXCEPTION_POINTERS* exceptionInfo)
{
    std::cerr << "=========================================\n";
    std::cerr << "💥 CRASH DETECTED BY CUSTOM CALLBACK! 💥\n";

    // Extract the raw hexadecimal code that Qt Creator's debugger hides
    DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;
    std::cerr << "Windows Exception Code: 0x" << std::hex << exceptionCode << "\n";

    // 2. Identify the exact underlying cause
    switch (exceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:     // 0xC0000005
        std::cerr << "Meaning: Access Violation (Dangling pointer / Nullptr read/write)\n";
        break;
    case STATUS_HEAP_CORRUPTION:         // 0xC0000374
        std::cerr << "Meaning: Heap Corruption (Double delete or buffer overflow)\n";
        break;
    case EXCEPTION_STACK_OVERFLOW:       // 0xC00000FD
        std::cerr << "Meaning: Stack Overflow (Infinite recursion loops)\n";
        break;
    default:
        std::cerr << "Meaning: Other unhandled low-level OS exception.\n";
        break;
    }
    std::cerr << "=========================================\n";

    // EXCEPTION_EXECUTE_HANDLER tells the OS to run standard crash behavior now
    return EXCEPTION_EXECUTE_HANDLER;
}

static std::string getWindowsExceptionString(DWORD code)
{
    // A clean lookup table mapping the official Windows headers to strings
    static const std::unordered_map<DWORD, std::string> exceptionMap = {
        { EXCEPTION_ACCESS_VIOLATION, "Access Violation (Dangling pointer / Nullptr read-write)" },
        { 0xC0000374,                 "Heap Corruption (Double-delete or buffer overflow)" },
        { EXCEPTION_STACK_OVERFLOW,    "Stack Overflow (Infinite recursion loop)" },
        { 0x4000001F,                 "STATUS_WX86_BREAKPOINT (32-bit / 64-bit Architecture Kit Mismatch)" },
        { EXCEPTION_INT_DIVIDE_BY_ZERO,"Integer Divide by Zero" },
        { EXCEPTION_ILLEGAL_INSTRUCTION,"Illegal Instruction (Corrupted binary path)" },
        { 0xE06D7363,                 "Unhandled C++ Exception (std::bad_function_call / throw)" }
    };

    auto it = exceptionMap.find(code);
    if (it != exceptionMap.end()) {
        return it->second;
    }

    return "Unknown Windows OS Signal / Unhandled Exception, with code "+std::to_string(code);
}

//THIS WORKS
// The Vectored Exception callback signature
LONG WINAPI vectoredCrashCallback(EXCEPTION_POINTERS* exceptionInfo)
{
    DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;

    if(code < 0x80000000)
        return EXCEPTION_CONTINUE_SEARCH;

    // Ignore harmless background debugger signals (like thread names being set)
    if (code == 0x406D1388) return EXCEPTION_CONTINUE_SEARCH;

    std::cerr << "\n=========================================\n";
    std::cerr << "VEH CALLBACK CAUGHT CRASH!\n";
    std::cerr << "Windows Signal Hex Code: 0x" << std::hex << code << "\n";
    std::cerr << getWindowsExceptionString(exceptionInfo->ExceptionRecord->ExceptionCode) << std::endl;
    std::cerr << "=========================================\n";

    // EXCEPTION_CONTINUE_SEARCH lets the debugger catch it next so Qt Creator still shows the line
    if(IsDebuggerPresent())
        return EXCEPTION_CONTINUE_SEARCH;
    else
    {
        std::terminate();
        return EXCEPTION_EXECUTE_HANDLER;//EXCEPTION_CONTINUE_EXECUTION;
    }
}

void installExceptionHandlers()
{
    // 3. Register the callback function at the very start of the application
    SetUnhandledExceptionFilter(crashCallback);

    AddVectoredExceptionHandler(1, vectoredCrashCallback);
}
