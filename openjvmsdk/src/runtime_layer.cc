#include "runtime_layer.h"

#include <cstring>
#include <iostream>
#include <mutex>
#include <unordered_map>

#include "common_exception.h"
#include "common_memory.h"
#include "scripts.h"

static std::unordered_map<std::string, bool> classCheckCache;

std::recursive_mutex RuntimeLayerMutex;

void GlobalErrorCallback(const char *Msg) {
    printf("FATAL ERROR\n");
    printf("%s\n", Msg);
    while (1) {}
}

RuntimeLayer RuntimeInstance;

TargetClassHeader ToTargetClassHeader(const TransitionalClass &klass) {
    TargetClassHeader header = {};

    header.Name = klass.name;
    header.Size = 0;

    header.FieldsSize = klass.FieldsSize;
    header.Fields = static_cast<TargetFieldHeader*>(CommonCalloc(klass.FieldsSize, sizeof(TargetFieldHeader)));
    for (U64 i = 0; i < klass.FieldsSize; i++) {
        header.Fields[i].Name = klass.Fields[i].name;
        header.Fields[i].Signature = klass.Fields[i].signature;
    }

    header.MethodsSize = klass.MethodsSize;
    header.Methods = static_cast<TargetMethodHeader*>(CommonCalloc(klass.MethodsSize, sizeof(TargetMethodHeader)));
    for (U64 i = 0; i < klass.MethodsSize; i++) {
        header.Methods[i].Name = klass.Methods[i].name;
        header.Methods[i].Signature = klass.Methods[i].signature;
    }

    return header;
}

BOOL CheckBaseTarget(const TransitionalClass &transitional_class) {
    if (!transitional_class.name) return false;

    std::string className(transitional_class.name);

    std::lock_guard<std::recursive_mutex> lock(RuntimeLayerMutex);

    auto it = classCheckCache.find(className);
    if (it != classCheckCache.end()) {
        return it->second;
    }

    TargetClassHeader targetHeader = ToTargetClassHeader(transitional_class);
    auto ptr = BaseScript();
    bool result = ExecuteTargetScript(*ptr, targetHeader);

    CommonFree(ptr);

    if (targetHeader.Fields) {
        CommonFree(targetHeader.Fields);
    }
    if (targetHeader.Methods) {
        CommonFree(targetHeader.Methods);
    }

    classCheckCache[className] = result;
    return result;
}

BOOL SetActionRuntimeLayer(RuntimeLayer::TargetAction action) {
    std::lock_guard<std::recursive_mutex> lock(RuntimeLayerMutex);

    if (RuntimeInstance.Action != RuntimeLayer::TargetAction::NONE) {
        RuntimeInstance.PreAction2 = RuntimeInstance.PreAction1;
        RuntimeInstance.PreAction1 = action;
        return true;
    }

    RuntimeInstance.Action = action;
    return true;
}

void UpdateRuntimeLayer() {
    std::lock_guard<std::recursive_mutex> lock(RuntimeLayerMutex);

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
            case RuntimeLayer::TargetAction::REFRESH_DATA_COLLECTED:
                DeallocateRuntimeLayer();
                CollectDataRuntimeLayer();
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
    #undef Deallocate
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

    TransitionalClass* newClasses = static_cast<TransitionalClass*>(CommonCalloc(class_count, sizeof(TransitionalClass)));

    JNIEnv* env = jvm->env;
    jclass classClass = env->FindClass("java/lang/Class");
    jmethodID getNameMethod = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");

    for (jint i = 0; i < class_count; i++) {
        jclass klass = classes[i];
        if (!klass) continue;

        TransitionalClass transitional_class = {};

        jstring nameStr = static_cast<jstring>(env->CallObjectMethod(klass, getNameMethod));
        if (!nameStr) {
            newClasses[i] = transitional_class;
            env->DeleteLocalRef(klass);
            continue;
        }
        const char* name = env->GetStringUTFChars(nameStr, nullptr);
        transitional_class.name = CommonStrDup(name);
        env->ReleaseStringUTFChars(nameStr, name);
        env->DeleteLocalRef(nameStr);

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
                    size_t nameLen = std::strlen(methodName);
                    size_t sigLen = std::strlen(methodSig);
                    char* signature = static_cast<char*>(CommonCalloc(nameLen + sigLen + 1, sizeof(char)));
                    std::memcpy(signature, methodName, nameLen);
                    std::memcpy(signature + nameLen, methodSig, sigLen);
                    signature[nameLen + sigLen] = '\0';

                    transitional_class.Methods[j].name = CommonStrDup(methodName);
                    transitional_class.Methods[j].signature = signature;

                    jvm->jvmti->Deallocate((unsigned char*)methodName);
                    jvm->jvmti->Deallocate((unsigned char*)methodSig);
                    if (methodGeneric) jvm->jvmti->Deallocate((unsigned char*)methodGeneric);
                }
            }
            jvm->jvmti->Deallocate((unsigned char*)methods);
        }

        transitional_class.Check = false;
        newClasses[i] = transitional_class;

        env->DeleteLocalRef(klass);
    }

    if (classClass) {
        env->DeleteLocalRef(classClass);
    }

    if (classes) {
        jvm->jvmti->Deallocate((unsigned char*)classes);
    }

    std::lock_guard<std::recursive_mutex> lock(RuntimeLayerMutex);
    RuntimeInstance.ClassesSize = class_count;
    RuntimeInstance.Classes = newClasses;
}

void DeallocateRuntimeLayer() {
    std::lock_guard<std::recursive_mutex> lock(RuntimeLayerMutex);

    for (unsigned i = 0; i < RuntimeInstance.ClassesSize; i++) {
        TransitionalClass &klass = RuntimeInstance.Classes[i];

        for (unsigned j = 0; j < klass.FieldsSize; j++) {
            auto &meta = klass.Fields[j];
            if (meta.name) CommonFree((void*)meta.name);
            if (meta.signature) CommonFree((void*)meta.signature);
        }
        if (klass.Fields) {
            CommonFree(klass.Fields);
        }

        for (unsigned j = 0; j < klass.MethodsSize; j++) {
            auto &meta = klass.Methods[j];
            if (meta.name) CommonFree((void*)meta.name);
            if (meta.signature) CommonFree((void*)meta.signature);
        }
        if (klass.Methods) {
            CommonFree(klass.Methods);
        }

        if (klass.name) {
            CommonFree((void*)klass.name);
        }
    }

    if (RuntimeInstance.Classes) {
        CommonFree(RuntimeInstance.Classes);
    }

    RuntimeInstance.ClassesSize = 0;
    RuntimeInstance.Classes = nullptr;
}