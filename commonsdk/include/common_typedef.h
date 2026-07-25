#ifndef COMMON_TYPEDEF
#define COMMON_TYPEDEF

#ifndef TYPEDEF_I64
#define TYPEDEF_I64
typedef long long I64;
#endif

#ifndef TYPEDEF_PI64
#define TYPEDEF_PI64
typedef long long* PI64;
#endif

#ifndef TYPEDEF_CPI64
#define TYPEDEF_CPI64
typedef const long long* CPI64;
#endif

#ifndef TYPEDEF_U64
#define TYPEDEF_U64
typedef unsigned long long U64;
#endif

#ifndef TYPEDEF_PU64
#define TYPEDEF_PU64
typedef unsigned long long* PU64;
#endif

#ifndef TYPEDEF_CPU64
#define TYPEDEF_CPU64
typedef const unsigned long long* CPU64;
#endif

#ifndef TYPEDEF_I2
#define TYPEDEF_I2
typedef short I2;
#endif

#ifndef TYPEDEF_CI2
#define TYPEDEF_CI2
typedef const short CI2;
#endif

#ifndef TYPEDEF_PI2
#define TYPEDEF_PI2
typedef short* PI2;
#endif

#ifndef TYPEDEF_CPI2
#define TYPEDEF_CPI2
typedef const short* CPI2;
#endif

#ifndef TYPEDEF_U2
#define TYPEDEF_U2
typedef unsigned short U2;
#endif

#ifndef TYPEDEF_CU2
#define TYPEDEF_CU2
typedef const unsigned short CU2;
#endif

#ifndef TYPEDEF_PU2
#define TYPEDEF_PU2
typedef unsigned short* PU2;
#endif

#ifndef TYPEDEF_CPU2
#define TYPEDEF_CPU2
typedef const unsigned short* CPU2;
#endif

#ifndef TYPEDEF_I4
#define TYPEDEF_I4
typedef int I4;
#endif

#ifndef TYPEDEF_CI4
#define TYPEDEF_CI4
typedef const int CI4;
#endif

#ifndef TYPEDEF_PI4
#define TYPEDEF_PI4
typedef int* PI4;
#endif

#ifndef TYPEDEF_CPI4
#define TYPEDEF_CPI4
typedef const int* CPI4;
#endif

#ifndef TYPEDEF_U4
#define TYPEDEF_U4
typedef unsigned int U4;
#endif

#ifndef TYPEDEF_CU4
#define TYPEDEF_CU4
typedef const unsigned int CU4;
#endif

#ifndef TYPEDEF_PU4
#define TYPEDEF_PU4
typedef unsigned int* PU4;
#endif

#ifndef TYPEDEF_CPU4
#define TYPEDEF_CPU4
typedef const unsigned int* CPU4;
#endif

#ifndef TYPEDEF_I1
#define TYPEDEF_I1
typedef char I1;
#endif

#ifndef TYPEDEF_CI1
#define TYPEDEF_CI1
typedef const char CI1;
#endif

#ifndef TYPEDEF_PI1
#define TYPEDEF_PI1
typedef char* PI1;
#endif

#ifndef TYPEDEF_CPI1
#define TYPEDEF_CPI1
typedef const char* CPI1;
#endif

#ifndef TYPEDEF_U1
#define TYPEDEF_U1
typedef unsigned char U1;
#endif

#ifndef TYPEDEF_CU1
#define TYPEDEF_CU1
typedef const unsigned char CU1;
#endif

#ifndef TYPEDEF_PU1
#define TYPEDEF_PU1
typedef unsigned char* PU1;
#endif

#ifndef TYPEDEF_CPU1
#define TYPEDEF_CPU1
typedef const unsigned char* CPU1;
#endif

#ifndef TYPEDEF_PV
#define TYPEDEF_PV
typedef void* PV;
#endif

#ifndef TYPEDEF_CPV
#define TYPEDEF_CPV
typedef const void* CPV;
#endif

#ifndef TYPEDEF_PVOID
#define TYPEDEF_PVOID
typedef void* PVoid;
#endif

#ifndef TYPEDEF_BOOL
#define TYPEDEF_BOOL
#ifndef __OBJC_BOOL
#define __OBJC_BOOL
typedef U1 BOOL;
#endif
#endif

#ifndef TYPEDEF_SIZE_T
#define TYPEDEF_SIZE_T
typedef U64 size_t;
#endif

#ifndef COMMON_LIST
#define COMMON_LIST(x, name) \
    U64 name##Size; \
    x* name;
#endif

#endif