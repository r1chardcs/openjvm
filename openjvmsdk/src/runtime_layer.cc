#include "runtime_layer.h"

#include <cstring>
#include <iostream>

#include "common_exception.h"
#include "common_memory.h"

RuntimeLayer RuntimeInstance;
#undef Deallocate

void InitializeRuntimeLayer() {
    RuntimeInstance.State = RuntimeLayer::TargetState::INITIALIZING;
    RuntimeInstance.JVM = CreateJVM(MethodGetJvm_JNI);

    if (const auto errBuf = CommonCheckError(); errBuf) {
        RuntimeInstance.State = RuntimeLayer::TargetState::UNHANDLED_EXCEPTION;
        Throw(errBuf);
    }
}

void CollectDataRuntimeLayer() {
    if (RuntimeInstance.State == RuntimeLayer::TargetState::UNHANDLED_EXCEPTION) return;
    RuntimeInstance.State = RuntimeLayer::TargetState::DATA_COLLECTION;

    auto jvm = RuntimeInstance.JVM;
    if (!jvm || !jvm->jvmti || !jvm->env) {
        RuntimeInstance.State = RuntimeLayer::TargetState::UNHANDLED_EXCEPTION;
        return;
    }

    jint class_count = 0;
    jclass* classes = nullptr;
    jvmtiError err = jvm->jvmti->GetLoadedClasses(&class_count, &classes);
    if (err != JVMTI_ERROR_NONE || class_count == 0 || classes == nullptr) {
        RuntimeInstance.State = RuntimeLayer::TargetState::UNHANDLED_EXCEPTION;
        return;
    }

    RuntimeInstance.ClassesSize = class_count;
    RuntimeInstance.Classes = static_cast<TransitionalClass*>(CommonCalloc(class_count, sizeof(TransitionalClass)));
    if (!RuntimeInstance.Classes) {
        RuntimeInstance.State = RuntimeLayer::TargetState::UNHANDLED_EXCEPTION;
        return;
    }

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

    RuntimeInstance.State = RuntimeLayer::TargetState::SUCCESS;
}