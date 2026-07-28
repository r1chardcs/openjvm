#include "scripts.h"

#include "common_memory.h"
#include "ImGui/imgui.h"
#include "resources/malware_detection.h"

PTargetScript BaseScript() {
    const auto script
        = reinterpret_cast<const char *>(Resources::malware_detection);

    return CreateTargetScript(0, "Malware Detection", script, CHECKING_TYPE_HEADER);
}

Renderable GetRenderableScriptMenu() {
    Renderable r;
    r.callback = RenderScriptMenu;

    return r;
}


extern PTargetScript Out_Update_Script;

void RenderScriptMenu(RenderContext context) {
    static char addressBuffer[256] = "openjvm";
    static char idBuffer[32] = "1";
    static PTargetServerScript loadedScript = nullptr;
    static char scriptName[256] = "";
    static char scriptSource[4096] = "";
    static I64 scriptRing = 0;
    static bool loadFailed = false;
    static float statusTimer = 0.0f;
    static const char* statusText = nullptr;
    static ImVec4 statusColor;

    const float contentWidth = 320.0f;
    const float loadButtonWidth = contentWidth - 78.0f;

    ImGui::SetNextWindowSizeConstraints(ImVec2(contentWidth + 20, 0), ImVec2(contentWidth + 20, FLT_MAX));
    ImGui::Begin("Script", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::PushItemWidth(contentWidth);
    ImGui::InputTextWithHint("##address", "Server address", addressBuffer, sizeof(addressBuffer));
    ImGui::PopItemWidth();

    ImGui::SetNextItemWidth(70);
    ImGui::InputTextWithHint("##id", "ID", idBuffer, sizeof(idBuffer));
    ImGui::SameLine();

    if (ImGui::Button("Load", ImVec2(loadButtonWidth, 0))) {
        I64 id = atoll(idBuffer);

        if (loadedScript) {
            CommonFree((void*)loadedScript->Name);
            CommonFree((void*)loadedScript->Source);
            CommonFree(loadedScript);
            loadedScript = nullptr;
        }

        loadedScript = GetScriptByID(id, CheckAvailableAddress(addressBuffer));

        if (loadedScript) {
            strncpy(scriptName, loadedScript->Name, sizeof(scriptName) - 1);
            scriptName[sizeof(scriptName) - 1] = '\0';

            strncpy(scriptSource, loadedScript->Source, sizeof(scriptSource) - 1);
            scriptSource[sizeof(scriptSource) - 1] = '\0';

            scriptRing = loadedScript->Ring;
            loadFailed = false;
        } else {
            scriptName[0] = '\0';
            scriptSource[0] = '\0';
            scriptRing = 0;
            loadFailed = true;
        }
    }

    if (loadFailed) {
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Script not found");
    }

    if (loadedScript) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "%s", scriptName);
        ImGui::SameLine();
        ImGui::TextDisabled("ring %lld", scriptRing);

        if (ImGui::TreeNode("Source")) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.17f, 1.0f));
            ImGui::BeginChild("ScriptSource", ImVec2(contentWidth, 120), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            ImGui::TextWrapped("%s", scriptSource);
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (ImGui::Button("Apply", ImVec2(contentWidth, 30))) {
            PTargetScript script = ToTargetScript(loadedScript);
            loadedScript = nullptr;

            Out_Update_Script = script;

            statusText = "Applied";
            statusColor = ImVec4(0.6f, 0.9f, 0.6f, 1.0f);
            statusTimer = 2.0f;
        }
    }

    if (statusTimer > 0.0f) {
        ImGui::SameLine();
        ImGui::TextColored(statusColor, "%s", statusText);
        statusTimer -= ImGui::GetIO().DeltaTime;
    }

    ImGui::End();
}

const char * CheckAvailableAddress(const char *source) {
    if (!strcmp(source, "openjvm")) {
        return "185.46.11.68:8000"; // Мой сервер, с нужными мне скриптами
    }
    return source;
}
