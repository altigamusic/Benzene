#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui_benzene_widgets.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <vector>

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

bool KeyframeSlider(const char* label, int* data, int min, int max, std::vector<int>& keyframes)
{
    const ImU32 TIMELINE_COLOR = GetColorU32(ImGuiCol_FrameBg);
    const ImU32 CURRENT_TIME_COLOR = GetColorU32(ImGuiCol_SliderGrab);
    const ImU32 KEYFRAME_OUTLINE_COLOR = GetColorU32(ImGuiCol_SliderGrabActive);
    const ImU32 KEYFRAME_INACTIVE_COLOR = GetColorU32(ImGuiCol_WindowBg);
    const ImU32 KEYFRAME_ACTIVE_COLOR = IM_COL32(0, 200, 0, 255);
    const int SNAP_THRESHOLD_PIXELS = 4;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    ImGuiID id = window->GetID(label);

    // Calculate bounding box for the invisible button
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(CalcItemWidth(), CalcTextSize("0", nullptr, true).y + g.Style.FramePadding.y * 2.0f);
    ImVec2 fullSize = ImVec2(size.x + CalcTextSize(label, nullptr, true).x + g.Style.ItemInnerSpacing.x, size.y);

    int snapThresholdValue = static_cast<int>(SNAP_THRESHOLD_PIXELS / size.x * (max - min));

    bool value_changed = false;

    if (!InvisibleButton(label, fullSize) && IsItemActive())
    {
        ImVec2 relativePos = GetIO().MousePos - pos;
        float ratio = relativePos.x / size.x;
        ratio = ImClamp(ratio, 0.0f, 1.0f);

        int new_value = static_cast<int>(min + ratio * (max - min));

        for (int kf : keyframes)
        {
            if (abs(kf - new_value) <= snapThresholdValue)
            {
                new_value = kf;
                break; // This could cause problems if two keyframes are very close to each other, fix later
            }
        }

        if (new_value != *data)
        {
            *data = new_value;
            value_changed = true;
        }
    }

    // Render timeline
    ImDrawList* draw_list = GetWindowDrawList();
    draw_list->AddLine(pos + ImVec2(0, size.y * 0.5f), pos + ImVec2(size.x, size.y * 0.5f), TIMELINE_COLOR, 4.0f);

    // Render current value indicator
    if (*data >= min && *data <= max)
    {
        float ratio = static_cast<float>(*data - min) / (max - min);
        float value_x = pos.x + ratio * size.x;
        ImVec2 start = ImVec2(value_x, pos.y);

        draw_list->AddLine(start, start + ImVec2(0, size.y + g.Style.FramePadding.y), CURRENT_TIME_COLOR);
    }

    // Render keyframes
    for (int kf : keyframes)
    {
        if (kf < min || kf > max) continue;
        const float KEYFRAME_SIZE = 10.0f;

        float ratio = static_cast<float>(kf - min) / (max - min);
        float kf_x = pos.x + ratio * size.x;

        // Keyframes are rhombus-shaped
        ImVec2 rhombus_center = ImVec2(kf_x, pos.y + size.y * 0.5f);
        ImVec2 rhombus_points[4] = {rhombus_center + ImVec2(0, -KEYFRAME_SIZE * 0.5f), rhombus_center + ImVec2(KEYFRAME_SIZE * 0.5f, 0),
            rhombus_center + ImVec2(0, KEYFRAME_SIZE * 0.5f), rhombus_center + ImVec2(-KEYFRAME_SIZE * 0.5f, 0)};

        draw_list->AddConvexPolyFilled(rhombus_points, 4, (kf == *data) ? KEYFRAME_ACTIVE_COLOR : KEYFRAME_INACTIVE_COLOR);

        draw_list->AddPolyline(rhombus_points, 4, KEYFRAME_OUTLINE_COLOR, ImDrawFlags_Closed, 2.);
    }

    // Render label
    ImVec2 label_pos = pos + ImVec2(size.x + g.Style.FramePadding.x, g.Style.FramePadding.y);
    draw_list->AddText(label_pos, GetColorU32(ImGuiCol_Text), label);

    return value_changed;
}

bool TimeSlider(const char* label, int* data, int min, int max)
{
    const ImU32 TIMELINE_COLOR = GetColorU32(ImGuiCol_Text);
    const ImU32 CURRENT_TIME_COLOR = GetColorU32(ImGuiCol_SliderGrab);

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    ImGuiID id = window->GetID(label);

    // Calculate bounding box for the invisible button
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(CalcItemWidth(), CalcTextSize("0", nullptr, true).y + g.Style.FramePadding.y * 2.0f);
    ImVec2 fullSize = ImVec2(size.x + CalcTextSize("9999999", nullptr, true).x + g.Style.ItemInnerSpacing.x, size.y);

    bool value_changed = false;

    if (!InvisibleButton(label, fullSize) && IsItemActive())
    {
        ImVec2 relativePos = GetIO().MousePos - pos;
        float ratio = relativePos.x / size.x;
        ratio = ImClamp(ratio, 0.0f, 1.0f);

        int new_value = static_cast<int>(min + ratio * (max - min));

        if (new_value != *data)
        {
            *data = new_value;
            value_changed = true;
        }
    }

    // Render timeline
    ImDrawList* draw_list = GetWindowDrawList();
    draw_list->AddLine(pos + ImVec2(0, size.y * .5f), pos + ImVec2(size.x, size.y * .5f), TIMELINE_COLOR, 1.0f);

    // Render current value indicator
    if (*data >= min && *data <= max)
    {
        float ratio = static_cast<float>(*data - min) / (max - min);
        ImVec2 start = pos + ImVec2(ratio * size.x, size.y * .5f);

        // Upside-down triangle
        ImVec2 points[3] = {start, start + ImVec2(3, -6), start + ImVec2(-3, -6)};
        draw_list->AddConvexPolyFilled(points, 3, TIMELINE_COLOR);

        // Line down
        draw_list->AddLine(start, start + ImVec2(0, size.y * .5f + g.Style.FramePadding.y), CURRENT_TIME_COLOR);
    }

    // Render time
    char buf[64];
    ImFormatString(buf, IM_ARRAYSIZE(buf), "%d", *data);

    ImVec2 value_pos = pos + ImVec2(size.x + g.Style.FramePadding.x, g.Style.FramePadding.y);
    draw_list->AddText(value_pos, GetColorU32(ImGuiCol_Text), buf);

    return value_changed;
}

bool KeyframeMarker(const char* label, bool* data)
{
    const ImU32 KEYFRAME_OUTLINE_COLOR = GetColorU32(ImGuiCol_SliderGrabActive);
    const ImU32 KEYFRAME_INACTIVE_COLOR = GetColorU32(ImGuiCol_FrameBg);
    const ImU32 KEYFRAME_HOVERED_COLOR = GetColorU32(ImGuiCol_FrameBgHovered);
    const ImU32 KEYFRAME_ACTIVE_COLOR = IM_COL32(0, 200, 0, 255);
    const float KEYFRAME_RADIUS = 6;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    ImGuiID id = window->GetID(label);

    // Calculate bounding box for the button
    ImVec2 pos = window->DC.CursorPos;
    float size = GetFrameHeight();
    ImRect bb = ImRect(pos.x, pos.y, pos.x + size, pos.y + size);

    ItemSize(bb, g.Style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    if (pressed)
    {
        *data = !*data;
        MarkItemEdited(id);
    }

    // Render button
    ImVec2 center = bb.GetCenter();
    ImVec2 rhombus_points[4] = {center + ImVec2(-KEYFRAME_RADIUS, 0), center + ImVec2(0, -KEYFRAME_RADIUS),
        center + ImVec2(KEYFRAME_RADIUS, 0), center + ImVec2(0, KEYFRAME_RADIUS)};

    ImDrawList* draw_list = GetWindowDrawList();

    auto color = *data ? KEYFRAME_ACTIVE_COLOR : (hovered ? KEYFRAME_HOVERED_COLOR : KEYFRAME_INACTIVE_COLOR);
    draw_list->AddConvexPolyFilled(rhombus_points, 4, color);
    draw_list->AddPolyline(rhombus_points, 4, KEYFRAME_OUTLINE_COLOR, ImDrawFlags_Closed, 1.);

    return pressed;
}