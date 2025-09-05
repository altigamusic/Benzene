#include "imgui_benzene_widgets.h"
#include "imgui.h"
#include "imgui_internal.h"

using namespace ImGui;

bool DragVector2(
    const char* label, ImVec2* v, float v_speed, const ImVec2* v_min, const ImVec2* v_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    ImGuiID id = window->GetID(label);

    // Calculate bounding box for the invisible button
    ImVec2 label_size = CalcTextSize(label, nullptr, true);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(CalcItemWidth(), label_size.y + g.Style.FramePadding.y * 2.0f);

    bool value_changed = false;

    if (!InvisibleButton(label, size) && IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left))
    {
        ImVec2 mouse_delta = GetIO().MouseDelta;
        ImVec2 v_old = *v;
        v->x += mouse_delta.x * v_speed;
        v->y -= mouse_delta.y * v_speed; // Upward movement increases the value

        // Clamp if min/max provided
        if (v_min)
        {
            v->x = ImMax(v->x, v_min->x);
            v->y = ImMax(v->y, v_min->y);
        }
        if (v_max)
        {
            v->x = ImMin(v->x, v_max->x);
            v->y = ImMin(v->y, v_max->y);
        }

        value_changed = mouse_delta.x != 0 || mouse_delta.y != 0;
    }

    // Render value and label
    ImVec2 value_pos = pos;
    char buf[128];
    ImFormatString(buf, IM_ARRAYSIZE(buf), format, v->x, v->y);
    ImDrawList* draw_list = GetWindowDrawList();
    draw_list->AddText(value_pos, GetColorU32(ImGuiCol_Text), buf);

    // Render label to the right
    if (label_size.x > 0.0f) RenderText(ImVec2(pos.x + size.x + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label);

    return value_changed;
}
