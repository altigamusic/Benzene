#include "debug_window.h"
#include "imgui/imgui.h"

void DebugWindow::open(const std::string& newError)
{
    error = newError;
    show = true;
}

void DebugWindow::close()
{
    error.clear();
    show = false;
}

void DebugWindow::render()
{
    if (!show) return;
    if (ImGui::Begin("Shader Debug", &show))
    {
        ImGui::TextUnformatted(error.c_str());
        ImGui::End();
        if (!show) close();
    }
}
