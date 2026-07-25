cmake_minimum_required(VERSION 3.16)

add_library(ImGui STATIC imgui.cpp imgui_demo.cpp imgui_draw.cpp imgui_impl_opengl3.cpp imgui_impl_win32.cpp imgui_tables.cpp imgui_widgets.cpp)

target_link_libraries(ImGui PUBLIC OpenGL32 dwmapi)