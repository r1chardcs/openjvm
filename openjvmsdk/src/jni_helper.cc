#include <jni_helper.h>
#include <common_memory.h>
#include <common_exception.h>

PJVM InternalCreateJvmJNI() {
    JavaVM* vms[1] = { nullptr };
    jsize vmCount = 0;

    const jint findResult = JNI_GetCreatedJavaVMs(vms, 1, &vmCount);
    if (findResult != JNI_OK) {
        CommonThrow("JNI_GetCreatedJavaVMs failed.");
        return nullptr;
    }

    if (vmCount == 0 || vms[0] == nullptr) {
        CommonThrow("No created JavaVM found.");
        return nullptr;
    }

    const auto jvm =
        static_cast<PJVM>(CommonCalloc(1, sizeof(JVM)));

    if (!jvm) {
        CommonThrow("Failed to allocate JVM struct.");
        return nullptr;
    }

    jvm->vm = vms[0];

    void* envPtr = nullptr;
    const jint envResult = jvm->vm->GetEnv(&envPtr, JNI_VERSION_1_6);
    if (envResult != JNI_OK || envPtr == nullptr) {
        CommonThrow("Failed to get JNIEnv.");
        CommonFree(jvm);
        return nullptr;
    }
    jvm->env = static_cast<JNIEnv*>(envPtr);

    void* jvmtiPtr = nullptr;
    const jint jvmtiResult = jvm->vm->GetEnv(&jvmtiPtr, JVMTI_VERSION_1_2);
    if (jvmtiResult != JNI_OK || jvmtiPtr == nullptr) {
        CommonThrow("Failed to get Jvmti.");
        CommonFree(jvm);
        return nullptr;
    }
    jvm->jvmti = static_cast<jvmtiEnv*>(jvmtiPtr);

    return jvm;
}

PJVM CreateJVM(MethodGetJvm Method) {
    if (Method == MethodGetJvm_JNI)
        return InternalCreateJvmJNI();

    return nullptr;
}

BOOL DestroyJVM(PJVM JvmPointer) {
    if (!JvmPointer)
        return 0;

    Deallocate(JvmPointer);
    return 1;
}
