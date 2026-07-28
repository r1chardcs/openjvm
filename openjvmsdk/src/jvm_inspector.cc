#include "jvm_inspector.h"

#include <string>
#include <vector>
#include <ImGui/imgui.h>

#include <runtime_layer.h>
#include <scripts.h>
#include <class_analyzer.h>

#include "common_exception.h"

Renderable GetRenderableJvmInspector() {
    Renderable r;
    r.callback = RenderJvmInspector;
    return r;
}

Renderable GetRenderableUnhookMenu() {
    Renderable r;
    r.callback = RenderUnhookMenu;
    return r;
}

void RenderUnhookMenu(RenderContext context) {
    constexpr float buttonWidth = 90.0f;
    constexpr float buttonHeight = 40.0f;
    constexpr float margin = 20.0f;

    const ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowPos(io.DisplaySize.x - margin, io.DisplaySize.y - margin);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(buttonWidth, buttonHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    ImGui::Begin("##UnhookMenu", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.05f, 0.05f, 1.0f));

    if (ImGui::Button("HIDE", ImVec2(buttonWidth, buttonHeight))) {
        Out_Render_State = false;
    }

    ImGui::PopStyleColor(3);

    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

std::string SanitizeClassName(const char* name) {
    if (!name) return "";

    std::string result;
    result.reserve(strlen(name));

    for (const char* p = name; *p; p++) {
        unsigned char c = static_cast<unsigned char>(*p);

        bool isAllowed = (c >= 'A' && c <= 'Z') ||
                         (c >= 'a' && c <= 'z') ||
                         (c >= '0' && c <= '9') ||
                         c == '_' ||
                         c == '$' ||
                         c == '.';

        if (isAllowed) {
            result += *p;
        } else {
            result += '?';
        }
    }

    return result;
}

static std::vector<std::string> SplitTokens(const char* raw) {
    std::vector<std::string> tokens;
    if (!raw || raw[0] == '\0') return tokens;

    std::string s(raw);
    size_t pos = 0;
    while ((pos = s.find(',')) != std::string::npos) {
        tokens.push_back(s.substr(0, pos));
        s.erase(0, pos + 1);
    }
    tokens.push_back(s);

    return tokens;
}

static bool MatchesAny(const std::vector<std::string>& tokens, const std::string& package) {
    for (const auto& token : tokens) {
        if (!token.empty() && package.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void RenderJvmInspector(RenderContext context) {
    if (RuntimeInstance.Action != RuntimeLayer::TargetAction::NONE)
        return;

    static bool showGenerated = false;
    static bool showPrimitive = false;
    static bool showDetect = false;
    static char packageFilter[256] = "";
    static char blacklist[1024] = "";
    static char whitelist[1024] = "";

    auto classes = RuntimeInstance.Classes;
    auto classCount = RuntimeInstance.ClassesSize;

    ImGui::Begin("Classes Inspector", nullptr, ImGuiWindowFlags_MenuBar);
    ImGui::SetWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save To")) {
                ImGui::OpenPopup("SaveToPopup");
            }

            if (ImGui::BeginPopupModal("SaveToPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                static char path[256] = "classes_dump.txt";
                ImGui::InputText("Filename", path, sizeof(path));

                if (ImGui::Button("Save", ImVec2(120, 0))) {
                    if (strlen(path) > 0) {
                        if (FILE* file = fopen(path, "w")) {
                            const auto runtime_classes = RuntimeInstance.Classes;
                            const auto runtime_class_count = RuntimeInstance.ClassesSize;

                            for (U64 i = 0; i < runtime_class_count; i++) {
                                auto& cls = runtime_classes[i];
                                if (!cls.Name) continue;

                                fprintf(file, "Class Name: %s\n", cls.Name);
                                fprintf(file, "- Fields:\n");
                                for (U64 j = 0; j < cls.FieldsSize; j++) {
                                    auto& field = cls.Fields[j];
                                    if (field.name && field.signature) {
                                        fprintf(file, "    %s : %s\n", field.name, field.signature);
                                    }
                                }
                                fprintf(file, "- Methods:\n");
                                for (U64 j = 0; j < cls.MethodsSize; j++) {
                                    auto& method = cls.Methods[j];
                                    if (method.name && method.signature) {
                                        fprintf(file, "    %s\n", method.signature);
                                    }
                                }
                                fprintf(file, "\n");
                            }

                            fclose(file);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (ImGui::MenuItem("Refresh")) {
                SetActionRuntimeLayer(RuntimeLayer::TargetAction::REFRESH_DATA_COLLECTED);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Generated Classes", nullptr, &showGenerated);
            ImGui::MenuItem("Show Primitive Classes", nullptr, &showPrimitive);
            ImGui::MenuItem("Show Only Detect", nullptr, &showDetect);
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
    ImGui::Text("Loaded Classes: %llu", (unsigned long long)classCount);
    ImGui::Separator();

    const bool hasPackageFilter = packageFilter[0] != '\0';
    const std::vector<std::string> whitelistTokens = SplitTokens(whitelist);
    const std::vector<std::string> blacklistTokens = SplitTokens(blacklist);

    if (ImGui::BeginChild("ClassTree", ImVec2(0, 0), true)) {
        for (U64 i = 0; i < classCount; i++) {
            auto& cls = classes[i];
            if (!cls.Name) continue;

            std::string className = SanitizeClassName(cls.Name);

            const bool isGenerated = IsGenerateClass(&cls);
            const bool isPrimitive = IsPrimitive(&cls);

            if (!showGenerated && isGenerated) continue;
            if (!showPrimitive && isPrimitive) continue;
            if (showDetect) {
                if (!cls.Check) continue;
            }

            std::string package;
            size_t lastDot = className.rfind('.');
            if (lastDot != std::string::npos) {
                package = className.substr(0, lastDot);
            }

            if (hasPackageFilter && package.find(packageFilter) == std::string::npos) {
                continue;
            }

            if (!whitelistTokens.empty() && !MatchesAny(whitelistTokens, package)) {
                continue;
            }

            if (!blacklistTokens.empty() && MatchesAny(blacklistTokens, package)) {
                continue;
            }

            bool isChecked = cls.Check;

            ImGui::PushID(&cls);

            if (isChecked) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 80, 80, 255));
            }

            bool isOpen = false;
            if (cls.Check && cls.Owner) { isOpen = ImGui::TreeNode(_FORMAT("%s (%s)", className.c_str(), cls.Owner)); }
            else isOpen = ImGui::TreeNode(className.c_str());

            if (isChecked) {
                ImGui::PopStyleColor();
            }

            if (isOpen) {
                if (cls.FieldsSize > 0 && cls.Fields) {
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

                if (cls.MethodsSize > 0 && cls.Methods) {
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