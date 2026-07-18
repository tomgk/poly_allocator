#include "exception_handling.h"

#include <string>
#include<iostream>
#include<unordered_map>

#ifdef _WIN32
// ==========================================
// WINDOWS IMPLEMENTATION
// ==========================================
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
#elif defined(__linux__)
// ==========================================
// LINUX / POSIX IMPLEMENTATION
// ==========================================
#include <csignal>
#include <cstdlib>

void linuxSignalHandler(int signalNumber)
{
    std::cerr << "\n=========================================\n";
    std::cerr << "CRASH DETECTED BY LINUX SIGNAL HANDLER!\n";
    std::cerr << "Signal Number: " << signalNumber << "\n";

    switch (signalNumber)
    {
    case SIGSEGV:
        std::cerr << "Meaning: Segmentation Fault (Access Violation / Bad pointer / Nullptr)\n";
        break;
    case SIGFPE:
        std::cerr << "Meaning: Floating Point Exception (e.g., Integer Divide by Zero)\n";
        break;
    case SIGILL:
        std::cerr << "Meaning: Illegal Instruction (Corrupted binary or invalid execution path)\n";
        break;
    case SIGABRT:
        std::cerr << "Meaning: Abort Signal (std::terminate or failed assert)\n";
        break;
    default:
        std::cerr << "Meaning: Other unhandled POSIX signal.\n";
        break;
    }
    std::cerr << "=========================================\n";

    // Exit immediately to mimic standard crash behavior
    std::exit(signalNumber);
}

void installExceptionHandlers()
{
    // Register the handler for the most common crash signals on Linux
    std::signal(SIGSEGV, linuxSignalHandler);
    std::signal(SIGFPE, linuxSignalHandler);
    std::signal(SIGILL, linuxSignalHandler);
    std::signal(SIGABRT, linuxSignalHandler);
}
#else
// ==========================================
// UNSUPPORTED PLATFORM ERROR
// ==========================================
#error "Unsupported platform! This project currently only supports Windows and Linux."
#endif
