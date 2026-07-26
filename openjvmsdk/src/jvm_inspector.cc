#include "jvm_inspector.h"

#include <string>
#include <vector>
#include <unordered_map>

#include "runtime_layer.h"
#include "scripts.h"

#include "class_analyzer.h"

#include <ImGui/imgui.h>

#include "common_memory.h"


Renderable GetRenderableJvmInspector() {
    Renderable r;
    r.callback = RenderJvmInspector;
    return r;
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
    static char packageFilter[256] = "";
    static char blacklist[1024] = "";
    static char whitelist[1024] = "";

    auto classes = RuntimeInstance.Classes;
    auto classCount = RuntimeInstance.ClassesSize;

    ImGui::Begin("Classes Inspector", nullptr, ImGuiWindowFlags_MenuBar);
    ImGui::SetWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Refresh")) {
                SetActionRuntimeLayer(RuntimeLayer::TargetAction::REFRESH_DATA_COLLECTED);
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
    ImGui::Text("Loaded Classes: %llu", (unsigned long long)classCount);
    ImGui::Separator();

    const bool hasPackageFilter = packageFilter[0] != '\0';
    const std::vector<std::string> whitelistTokens = SplitTokens(whitelist);
    const std::vector<std::string> blacklistTokens = SplitTokens(blacklist);

    if (ImGui::BeginChild("ClassTree", ImVec2(0, 0), true)) {
        for (U64 i = 0; i < classCount; i++) {
            auto& cls = classes[i];
            if (!cls.name) continue;

            std::string className = SanitizeClassName(cls.name);

            const bool isGenerated = IsGenerateClass(&cls);
            const bool isPrimitive = IsPrimitive(&cls);

            if (!showGenerated && isGenerated) continue;
            if (!showPrimitive && isPrimitive) continue;

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

            bool isOpen = ImGui::TreeNode(className.c_str());

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