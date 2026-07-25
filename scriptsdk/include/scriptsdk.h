#ifndef SCRIPTSDK
#define SCRIPTSDK

#include "common_typedef.h"

#ifdef SELF_BUILD
#define IMPORT_AND_EXPORT __declspec(dllexport)
#else
#define IMPORT_AND_EXPORT __declspec(dllimport)
#endif

#define CHECKING_TYPE_HEADER 100
#define CHECKING_TYPE_NONE   0

extern "C"
{
    typedef unsigned short CheckingType;

    typedef struct {
        I64 ring;
        CPI1 name;
        CPI1 source;
        CheckingType type;
    }TargetScript, *PTargetScript;

    typedef struct {
        CPI1 Name;
        CPI1 Signature;
    }TargetMethodHeader;

    typedef struct {
        CPI1 Name;
        CPI1 Signature;
    }TargetFieldHeader;

    typedef struct {
        CPI1 Name;
        U64 Size;
        COMMON_LIST(TargetFieldHeader, Fields)
        COMMON_LIST(TargetMethodHeader, Methods)
    }TargetClassHeader;

    IMPORT_AND_EXPORT PTargetScript CreateTargetScript(I64 Ring, CPI1 Name, CPI1 Source, CheckingType Type);
    IMPORT_AND_EXPORT void DestroyTargetScript(PTargetScript Script);
    IMPORT_AND_EXPORT BOOL ExecuteTargetScript(TargetScript Script, TargetClassHeader Header);
}
#endif
