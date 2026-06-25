#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"
#include <gtest/gtest.h>

static void dump0(const ArenaAllocator<StoreTypeInfoType::yes> &a)
{
    for(auto iter=a.begin();iter!=a.end();++iter)
    {
        const std::type_info &type = *iter.get_header().type_info;
        std::cout << type.name();

        if(typeid(std::string) == type)
        {
            std::cout << "STR " << iter.get<std::string>();
        }
        else if(typeid(int) == type)
        {
            std::cout << "INT " << iter.get<int>();
        }

        std::cout << std::endl;
    }

    std::cout << "capacity: " << a.get_capacity() << std::endl;
}

static void dump(const TypeAwareArenaAllocator &a)
{
    if(false)
    {
        TypeAwareArenaAllocator::ConstIterator iter = a.begin();
        const auto &val = *iter;
        //const TypeAwareArenaAllocator::Entry<true> &val = *iter;
    }

    //INFO: keep type non-auto to catch compiler errors after changes
    for(const TypeAwareArenaAllocator::Entry<EntryConstness::yes> &val : a)
    {
        const std::type_info &type = *val.get_header().type_info;
        std::cout << type.name();

        if(typeid(std::string) == type)
        {
            std::cout << "STR " << val.get<std::string>();
        }
        else if(typeid(int) == type)
        {
            std::cout << "INT " << val.get<int>();
        }

        std::cout << std::endl;
    }

    std::cout << "capacity: " << a.get_capacity() << std::endl;
}

static std::string LONG_STRING = []{
    std::string str;

    for(size_t i=0;i<sizeof(std::string)+1;++i)
        str += ('A'+i);

    return str;
}();

#include "data.h"

TEST(ArenaAllocator_deallocate, Data)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<Data>(12);
    a.deallocate(ptr);
}

TEST(ArenaAllocator_deallocate, string_short)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<std::string>("12");
    a.deallocate(ptr);
}

TEST(ArenaAllocator_deallocate, string_long)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<std::string>(LONG_STRING);
    a.deallocate(ptr);
}

TEST(ArenaAllocator_clear, Data)
{
    TypeAwareArenaAllocator a;

    a.allocate<Data>(12);
    a.clear();
}

TEST(ArenaAllocator_clear, string_short)
{
    TypeAwareArenaAllocator a;

    a.allocate<std::string>("12");
    a.clear();
}

TEST(ArenaAllocator_clear, string_long)
{
    TypeAwareArenaAllocator a;

    a.allocate<std::string>(LONG_STRING);
    a.clear();
}

TEST(ArenaAllocator_allocate, OneTypeWithoutResizeAndClear_Complex_Once)
{
    TypeAwareArenaAllocator a;

    a.allocate<std::string>("#0");

    dump(a);
}

TEST(ArenaAllocator_allocate, DISABLED_OneTypeWithoutResizeAndClear_Complex_MultipleFull)
{
    TypeAwareArenaAllocator a;

    //TODO: i<8 (which requires reallocation) causes a crash for some reason
    for(int i=0;i<7;++i)
    {
        a.allocate<std::string>("#"+std::to_string(i));
        std::cout << i << std::endl;
    }

    dump(a);
}

TEST(ArenaAllocator, DISABLED_MixedWithoutResizeAndClear)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<int>(12);
    a.deallocate(ptr);
    a.get_type_info(ptr);
    std::cout << "capacity: " << a.get_capacity() << std::endl;
    a.get_used_bytes();

    //TODO: i<8 (which requires reallocation) causes a crash for some reason
    for(int i=0;i<7/*256*/;++i)
    {
        a.allocate<std::string>("#"+std::to_string(i));
        std::cout << i << std::endl;
    }

    dump(a);
}

TEST(ArenaAllocator, DISABLED_MixedWithoutResize)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<int>(12);
    a.deallocate(ptr);
    a.get_type_info(ptr);
    std::cout << "capacity: " << a.get_capacity() << std::endl;
    a.get_used_bytes();
    a.clear();

    //TODO: i<8 (which requires reallocation) causes a crash for some reason
    for(int i=0;i<7/*256*/;++i)
    {
        a.allocate<std::string>("#"+std::to_string(i));
        std::cout << i << std::endl;
    }

    dump(a);
}

TEST(ArenaAllocator, DISABLED_MixedWithResize)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<int>(12);
    a.deallocate(ptr);
    a.get_type_info(ptr);
    std::cout << "capacity: " << a.get_capacity() << std::endl;
    a.get_used_bytes();
    //TODO: add test for that
    //TODO: removing this line makes it crash for some reason
    a.clear();

    for(int i=0;i<8;++i)
    {
        a.allocate<std::string>("#"+std::to_string(i));
        std::cout << i << std::endl;
    }

    dump(a);

    std::cout << "Starting deallocation..." << std::endl;
}

void test()
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<int>(12);
    a.deallocate(ptr);
    a.get_type_info(ptr);
    a.get_capacity();
    a.get_used_bytes();
    a.clear();

    for(auto iter = a.begin();iter != a.end(); ++iter)
    {
        iter.get_header();
        iter.get_size();
        iter.get<int>();
    }
}

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

    return "Unknown Windows OS Signal / Unhandled Exception";
}

//THIS WORKS
// The Vectored Exception callback signature
LONG WINAPI vectoredCrashCallback(EXCEPTION_POINTERS* exceptionInfo)
{
    DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;

    // Ignore harmless background debugger signals (like thread names being set)
    if (code == 0x406D1388) return EXCEPTION_CONTINUE_SEARCH;

    std::cerr << "\n=========================================\n";
    std::cerr << "VEH CALLBACK CAUGHT CRASH!\n";
    std::cerr << "Windows Signal Hex Code: 0x" << std::hex << code << "\n";
    std::cerr << getWindowsExceptionString(exceptionInfo->ExceptionRecord->ExceptionCode) << std::endl;
    std::cerr << "=========================================\n";

    // EXCEPTION_CONTINUE_SEARCH lets the debugger catch it next so Qt Creator still shows the line
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv)
{
    // 3. Register the callback function at the very start of the application
    SetUnhandledExceptionFilter(crashCallback);

    AddVectoredExceptionHandler(1, vectoredCrashCallback);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
