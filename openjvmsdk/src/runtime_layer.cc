#include "runtime_layer.h"

#include <cstring>
#include <iostream>

#include "common_exception.h"
#include "common_memory.h"

void GlobalErrorCallback(const char *Msg) {
    printf("FATAL ERROR\n");
    printf("%s\n", Msg);
    while (1) {}
}

RuntimeLayer RuntimeInstance;

BOOL SetActionRuntimeLayer(const RuntimeLayer::TargetAction action) {
    if (RuntimeInstance.Action != RuntimeLayer::TargetAction::NONE) {
        RuntimeInstance.PreAction2 = RuntimeInstance.PreAction1;
        RuntimeInstance.PreAction1 = action;
        return true;
    }

    RuntimeInstance.Action = action;
    return true;
}

void UpdateRuntimeLayer() {
    if (RuntimeInstance.Action == RuntimeLayer::TargetAction::NONE &&
        RuntimeInstance.PreAction1 == RuntimeLayer::TargetAction::NONE &&
        RuntimeInstance.PreAction2 == RuntimeLayer::TargetAction::NONE) {
        return;
        }

    RuntimeLayer::TargetAction actions[3] = {
        RuntimeInstance.Action,
        RuntimeInstance.PreAction1,
        RuntimeInstance.PreAction2
    };

    for (int i = 0; i < 3; i++) {
        if (actions[i] == RuntimeLayer::TargetAction::NONE) continue;

        switch (actions[i]) {
            case RuntimeLayer::TargetAction::INITIALIZE:
                InitializeRuntimeLayer();
                break;
            case RuntimeLayer::TargetAction::DATA_COLLECTED:
                CollectDataRuntimeLayer();
                break;
            case RuntimeLayer::TargetAction::DEALLOCATE_DATA:
                DeallocateRuntimeLayer();
                break;
            case RuntimeLayer::TargetAction::NONE:
                break;
        }
    }

    RuntimeInstance.Action = RuntimeLayer::TargetAction::NONE;
    RuntimeInstance.PreAction1 = RuntimeLayer::TargetAction::NONE;
    RuntimeInstance.PreAction2 = RuntimeLayer::TargetAction::NONE;
}

#ifdef Deallocate
    #undef Deallocate // Для jvmti->Deallocate
#endif

void InitializeRuntimeLayer() {
    RuntimeInstance.JVM = CreateJVM(MethodGetJvm_JNI);

    if (const auto errBuf = CommonCheckError(); errBuf) {
        Throw(errBuf);
    }
}

void CollectDataRuntimeLayer() {
    auto jvm = RuntimeInstance.JVM;
    if (!jvm || !jvm->jvmti || !jvm->env) {
        Throw("Invalid JVM");
    }

    jint class_count = 0;
    jclass* classes = nullptr;
    jvmtiError err = jvm->jvmti->GetLoadedClasses(&class_count, &classes);
    if (err != JVMTI_ERROR_NONE || class_count == 0 || classes == nullptr) {
        Throw("Invalid Getting Loaded Classes");
    }

    RuntimeInstance.ClassesSize = class_count;
    RuntimeInstance.Classes = static_cast<TransitionalClass*>(CommonCalloc(class_count, sizeof(TransitionalClass)));

    JNIEnv* env = jvm->env;
    jclass classClass = env->FindClass("java/lang/Class");
    jmethodID getNameMethod = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");

    for (jint i = 0; i < class_count; i++) {
        jclass klass = classes[i];
        if (!klass) continue;

        TransitionalClass transitional_class = {};

        jstring nameStr = static_cast<jstring>(env->CallObjectMethod(klass, getNameMethod));
        if (!nameStr) continue;
        const char* name = env->GetStringUTFChars(nameStr, nullptr);
        transitional_class.name = CommonStrDup(name);
        env->ReleaseStringUTFChars(nameStr, name);

        jint field_count = 0;
        jfieldID* fields = nullptr;
        err = jvm->jvmti->GetClassFields(klass, &field_count, &fields);
        if (err == JVMTI_ERROR_NONE && field_count > 0 && fields) {
            transitional_class.FieldsSize = field_count;
            transitional_class.Fields = static_cast<TransitionalField*>(CommonCalloc(field_count, sizeof(TransitionalField)));

            for (jint j = 0; j < field_count; j++) {
                char* fieldName = nullptr;
                char* fieldSig = nullptr;
                char* fieldGeneric = nullptr;
                err = jvm->jvmti->GetFieldName(klass, fields[j], &fieldName, &fieldSig, &fieldGeneric);
                if (err == JVMTI_ERROR_NONE) {
                    transitional_class.Fields[j].name = CommonStrDup(fieldName);
                    transitional_class.Fields[j].signature = CommonStrDup(fieldSig);
                    jvm->jvmti->Deallocate((unsigned char*)fieldName);
                    jvm->jvmti->Deallocate((unsigned char*)fieldSig);
                    if (fieldGeneric) jvm->jvmti->Deallocate((unsigned char*)fieldGeneric);
                }
            }
            jvm->jvmti->Deallocate((unsigned char*)fields);
        }

        jint method_count = 0;
        jmethodID* methods = nullptr;
        err = jvm->jvmti->GetClassMethods(klass, &method_count, &methods);
        if (err == JVMTI_ERROR_NONE && method_count > 0 && methods) {
            transitional_class.MethodsSize = method_count;
            transitional_class.Methods = static_cast<TransitionalMethod*>(CommonCalloc(method_count, sizeof(TransitionalMethod)));

            for (jint j = 0; j < method_count; j++) {
                char* methodName = nullptr;
                char* methodSig = nullptr;
                char* methodGeneric = nullptr;
                err = jvm->jvmti->GetMethodName(methods[j], &methodName, &methodSig, &methodGeneric);
                if (err == JVMTI_ERROR_NONE) {
                    char signature[2048] = {0};
                    strcat(signature, methodName);
                    strcat(signature, methodSig);

                    transitional_class.Methods[j].name = CommonStrDup(methodName);
                    transitional_class.Methods[j].signature = CommonStrDup(signature);

                    jvm->jvmti->Deallocate((unsigned char*)methodName);
                    jvm->jvmti->Deallocate((unsigned char*)methodSig);
                    if (methodGeneric) jvm->jvmti->Deallocate((unsigned char*)methodGeneric);
                }
            }
            jvm->jvmti->Deallocate((unsigned char*)methods);
        }

        RuntimeInstance.Classes[i] = transitional_class;
    }

    if (classes) {
        jvm->jvmti->Deallocate((unsigned char*)classes);
    }
}

void DeallocateRuntimeLayer() {

    for (unsigned i = 0; i < RuntimeInstance.ClassesSize; i++) {
        auto klass = RuntimeInstance.Classes[i];
        for (unsigned j = 0; j < klass.FieldsSize; j++) {
            auto meta = klass.Fields[j];
            CommonFree((void*)meta.name);
            CommonFree((void*)meta.signature);

            meta.name = nullptr;
            meta.signature = nullptr;
        }

        for (unsigned j = 0; j < klass.MethodsSize; j++) {
            auto meta = klass.Methods[j];
            CommonFree((void*)meta.name);
            CommonFree((void*)meta.signature);

            meta.name = nullptr;
            meta.signature = nullptr;
        }

        CommonFree((void*)klass.name);
        CommonFree(&klass);
    }

    RuntimeInstance.ClassesSize = 0;
    RuntimeInstance.Classes = nullptr;
}
