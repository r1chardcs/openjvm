#include "jvm_inspector.h"

#include <string>

#include "runtime_layer.h"
#include <ImGui/imgui.h>

Renderable GetRenderableJvmInspector() {
    Renderable r;
    r.callback = RenderJvmInspector;
    return r;
}

void RenderJvmInspector(RenderContext context) {
    static bool showGenerated = false;
    static bool showPrimitive = false;
    static char packageFilter[256] = "";
    static char blacklist[1024] = "";
    static char whitelist[1024] = "";

    if (RuntimeInstance.State != RuntimeLayer::TargetState::SUCCESS) {
        ImGui::Begin("Classes Inspector", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        ImGui::Text("Collecting data...");
        ImGui::End();
        return;
    }

    auto classes = RuntimeInstance.Classes;
    auto classCount = RuntimeInstance.ClassesSize;

    ImGui::Begin("Classes Inspector", nullptr, ImGuiWindowFlags_MenuBar);
    ImGui::SetWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Refresh")) {
                CollectDataRuntimeLayer();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Generated Classes", nullptr, &showGenerated);
            ImGui::MenuItem("Show Primitive Classes", nullptr, &showPrimitive);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::BeginGroup();
    ImGui::Text("Package Filter:");
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::InputText("##packagefilter", packageFilter, sizeof(packageFilter));
    ImGui::PopItemWidth();

    ImGui::Text("Whitelist (comma separated):");
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::InputText("##whitelist", whitelist, sizeof(whitelist));
    ImGui::PopItemWidth();

    ImGui::Text("Blacklist (comma separated):");
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::InputText("##blacklist", blacklist, sizeof(blacklist));
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::Separator();
    ImGui::Text("Loaded Classes: %llu", classCount);
    ImGui::Separator();

    if (ImGui::BeginChild("ClassTree", ImVec2(0, 0), true)) {
        for (U64 i = 0; i < classCount; i++) {
            auto& cls = classes[i];
            if (!cls.name) continue;

            std::string className(cls.name);

            bool isGenerated = className.find("$") != std::string::npos;
            bool isPrimitive = className.find("[") != std::string::npos ||
                               className == "int" || className == "long" ||
                               className == "float" || className == "double" ||
                               className == "boolean" || className == "byte" ||
                               className == "char" || className == "short" ||
                               className == "void";

            if (!showGenerated && isGenerated) continue;
            if (!showPrimitive && isPrimitive) continue;

            std::string package;
            size_t lastDot = className.rfind('.');
            if (lastDot != std::string::npos) {
                package = className.substr(0, lastDot);
            }

            if (strlen(packageFilter) > 0 && package.find(packageFilter) == std::string::npos) {
                continue;
            }

            if (strlen(whitelist) > 0) {
                std::string wl(whitelist);
                size_t pos = 0;
                bool found = false;
                while ((pos = wl.find(',')) != std::string::npos) {
                    std::string token = wl.substr(0, pos);
                    if (package.find(token) != std::string::npos) {
                        found = true;
                        break;
                    }
                    wl.erase(0, pos + 1);
                }
                if (!found && package.find(wl) == std::string::npos) continue;
            }

            if (strlen(blacklist) > 0) {
                std::string bl(blacklist);
                size_t pos = 0;
                bool blocked = false;
                while ((pos = bl.find(',')) != std::string::npos) {
                    std::string token = bl.substr(0, pos);
                    if (package.find(token) != std::string::npos) {
                        blocked = true;
                        break;
                    }
                    bl.erase(0, pos + 1);
                }
                if (blocked || package.find(bl) != std::string::npos) continue;
            }

            ImGui::PushID(i);
            bool isOpen = ImGui::TreeNode(cls.name);

            if (isOpen) {
                if (cls.FieldsSize > 0) {
                    if (ImGui::TreeNode("Fields")) {
                        for (U64 j = 0; j < cls.FieldsSize; j++) {
                            auto& field = cls.Fields[j];
                            if (field.name && field.signature) {
                                ImGui::Text("  %s : %s", field.name, field.signature);
                            }
                        }
                        ImGui::TreePop();
                    }
                }

                if (cls.MethodsSize > 0) {
                    if (ImGui::TreeNode("Methods")) {
                        for (U64 j = 0; j < cls.MethodsSize; j++) {
                            auto& method = cls.Methods[j];
                            if (method.name && method.signature) {
                                ImGui::Text("  %s", method.signature);
                            }
                        }
                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}