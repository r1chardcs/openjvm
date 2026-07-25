#ifndef JNI_HELPER
#define JNI_HELPER

#include "common_typedef.h"
#include "jni/jni.h"
#include "jni/jvmti.h"

typedef unsigned short MethodGetJvm;
#define MethodGetJvm_NONE 0
#define MethodGetJvm_JNI 100

typedef struct {
    JNIEnv* env;
    jvmtiEnv* jvmti;
    JavaVM* vm;
}JVM, *PJVM;

PJVM CreateJVM(MethodGetJvm);
BOOL DestroyJVM(PJVM JvmPointer);

#endif
