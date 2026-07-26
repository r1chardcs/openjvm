cmake_minimum_required(VERSION 3.16)

include_directories(include)
add_library(openjvmsdk SHARED src/instance.cc
        include/jni_helper.h
        src/jni_helper.cc
        include/overlay.h
        src/overlay.cc
        include/runtime_layer.h
        src/runtime_layer.cc
        include/jvm_inspector.h
        src/jvm_inspector.cc)

target_link_libraries(openjvmsdk PRIVATE scriptsdk MinHook ImGui)