#pragma once
#include <Windows.h>
#include <vector>
#include <iostream>
#include <Psapi.h>

class MF
{
public:
    template <typename T>
    static T Read(uintptr_t address)
    {
        return *((T*)address);
    }

    template<typename T>
    static void Write(uintptr_t address, T value)
    {
        *((T*)address) = value;
    }

    template<typename T>
    static uintptr_t Protect(uintptr_t address, uintptr_t protect)
    {
        DWORD oldProt;
        VirtualProtect((LPVOID)address, sizeof(T), protect, &oldProt);
        return oldProt;
    }

    static void ReplaceByte(uintptr_t address, BYTE byteToReplaceWith)
    {
        uintptr_t oldProtect = Protect<BYTE>(address, PAGE_EXECUTE_READWRITE);
        Write<BYTE>(address, byteToReplaceWith);
        Protect<BYTE>(address, oldProtect);
    }

    static void ReplaceByte(uintptr_t address, BYTE byteToReplaceWith, int count)
    {
        for (int i = 0; i < count; i++)
        {
            uintptr_t oldProtect = Protect<BYTE>(address, PAGE_EXECUTE_READWRITE);
            Write<BYTE>(address + 0x1 * i, (BYTE)byteToReplaceWith);
            Protect<BYTE>(address, oldProtect);
        }
    }

    static void ReplaceByte(uintptr_t address, std::vector<BYTE> byteArray)
    {
        for (int i = 0; i < byteArray.size(); i++)
        {
            uintptr_t oldProtect = Protect<BYTE>(address, PAGE_EXECUTE_READWRITE);
            Write<BYTE>(address + 0x1 * i, (BYTE)byteArray[i]);
            Protect<BYTE>(address, oldProtect);
        }
    }

    static void PrintBytes(uintptr_t address, int count)
    {
        for (int i = 0; i < count; i++)
        {
            printf("%02hhX ", Read<BYTE>(address + 0x1 * i));
        }
    }

    static unsigned char* hookWithJump(uintptr_t hookAt, uintptr_t newFunc)
    {
        uintptr_t newOffset = newFunc - hookAt - 5;

        auto oldProtection = Protect<BYTE[5]>(hookAt, PAGE_EXECUTE_READWRITE);

        unsigned char* originals = new unsigned char[5];
        for (unsigned int i = 0; i < 5; i++)
            originals[i] = Read<unsigned char>(hookAt + i);

        Write<BYTE>(hookAt, 0xE9);
        Write<uintptr_t>(hookAt + 1, newOffset);

        Protect<BYTE[5]>(hookAt + 1, oldProtection);
        return originals;
    }

    static void unhookWithJump(uintptr_t hookAt, unsigned char* originals)
    {
        auto oldProtection = Protect<BYTE[5]>(hookAt, PAGE_EXECUTE_READWRITE);
        for (unsigned int i = 0; i < 5; i++)
            Write<BYTE>(hookAt + i, originals[i]);
        Protect<BYTE[5]>(hookAt + 1, oldProtection);

        delete[] originals;
    }

    static uintptr_t GetModuleAddress(const char* moduleName)
    {
        HMODULE hModule = GetModuleHandleA(moduleName);
        if (hModule == NULL)
            return NULL;
        return (uintptr_t)hModule;
    }

    static uintptr_t FindDMAAddy(uintptr_t baseAddress, std::vector<unsigned int> offsets)
    {
        for (int i = 0; i < offsets.size(); i++)
            baseAddress = *(uintptr_t*)baseAddress + offsets[i];
        return baseAddress;
    }

    static uintptr_t GetPointerAddress(uintptr_t baseAddress, std::vector<unsigned int> offsets)
    {
        uintptr_t addr = baseAddress;
        for (int i = 0; i < offsets.size(); i++)
        {
            addr = *(uintptr_t*)addr;
            addr += offsets[i];
        }
        return addr;
    }
};