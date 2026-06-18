#include "save_dialog.h"
#include "imgui/imgui.h"

void SaveDialog::open() { show = true; }

bool SaveDialog::render(bool& shouldExit)
{
    if (!show) return false;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.125f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (!ImGui::Begin("Save?", &show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) return false;

    bool shouldSave = false;

    ImGui::Text("Save changes before closing?");
    if (ImGui::Button("Yes"))
    {
        show = false;
        shouldExit = true;
        shouldSave = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("No"))
    {
        show = false;
        shouldExit = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        show = false;
    }

    ImGui::End();

    return shouldSave;
}
