cmake_minimum_required(VERSION 3.16)

add_library(MinHook STATIC buffer.c hook.c trampoline.c hde/hde32.c hde/hde64.c)