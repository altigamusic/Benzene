#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui_benzene_widgets.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <vector>

#ifdef WINDOWS
#include <windows.h>
#undef min
#undef max
#endif

using namespace ImGui;

const ImGuiCol SELECTION_COLOR = IM_COL32(50, 100, 200, 60);

static bool InvisibleButton(const char* str_id, const ImVec2& size_arg, bool* hovered, bool* held, ImGuiButtonFlags flags = 0)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    // Cannot use zero-size for InvisibleButton(). Unlike Button() there is not way to fallback using the label size.
    IM_ASSERT(size_arg.x != 0.0f && size_arg.y != 0.0f);

    const ImGuiID id = window->GetID(str_id);
    ImVec2 size = CalcItemSize(size_arg, 0.0f, 0.0f);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ItemSize(size);
    if (!ItemAdd(bb, id)) return false;

    bool pressed = ButtonBehavior(bb, id, hovered, held, flags);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, str_id, g.LastItemData.StatusFlags);
    return pressed;
}

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
    ImVec2 size = ImVec2(CalcItemWidth(), label_size.y);
    ImVec2 fullSize = size + ImVec2(g.Style.ItemInnerSpacing.x + label_size.x, g.Style.FramePadding.y * 2.0f);

    bool value_changed = false;

    if (!InvisibleButton(label, fullSize) && IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left))
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

bool KeyframeSlider(const char* label, float* data, bool* isEnd, float min, float max, const std::vector<float>& keyframes,
    KeyframeMovementData* movement, float* loopStart, float* loopEnd)
{
    const ImU32 TIMELINE_COLOR = GetColorU32(ImGuiCol_FrameBg);
    const ImU32 CURRENT_TIME_COLOR = GetColorU32(ImGuiCol_SliderGrab);
    const ImU32 KEYFRAME_OUTLINE_COLOR = GetColorU32(ImGuiCol_SliderGrabActive);
    const ImU32 KEYFRAME_INACTIVE_COLOR = GetColorU32(ImGuiCol_WindowBg);
    const ImU32 KEYFRAME_ACTIVE_COLOR = IM_COL32(0, 200, 0, 255);
    const ImU32 KEYFRAME_DRAGGED_COLOR = IM_COL32(100, 200, 200, 255);
    const int SNAP_THRESHOLD_PIXELS = 4;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    ImGuiID id = window->GetID(label);

    // This works because only one keyframe in one slider can be dragged at a time
    static int draggedKeyframeIndex = -1;
    static ImGuiID draggedId = 0;

    // Calculate bounding box for the invisible button
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(CalcItemWidth(), CalcTextSize("0", nullptr, true).y + g.Style.FramePadding.y * 2.0f);
    ImVec2 fullSize = ImVec2(size.x + CalcTextSize(label, nullptr, true).x + g.Style.ItemInnerSpacing.x, size.y);

    float snapThresholdValue = SNAP_THRESHOLD_PIXELS / size.x * (max - min);

    bool value_changed = false;
    bool pressed = InvisibleButton(label, fullSize, ImGuiButtonFlags_PressedOnClick);

    if (pressed || IsItemActive())
    {
        draggedId = id;
        ImVec2 relativePos = GetIO().MousePos - pos;
        float ratio = relativePos.x / size.x;
        ratio = ImClamp(ratio, 0.0f, 1.0f);
        float new_value = min + ratio * (max - min);

        if (pressed)
        {
            // Check if a keyframe is being dragged
            auto kf = std::find_if(keyframes.begin(), keyframes.end(),
                [new_value, snapThresholdValue](float kf) { return abs(kf - new_value) <= snapThresholdValue; });

            // Check for dual keyframes
            if (kf == keyframes.end())
                draggedKeyframeIndex = -1;
            else
            {
                // If this is a dual keyframe and the click position is to the right of the keyframe,
                // we select the end half (the next keyframe)
                auto nextKf = kf + 1;
                if (nextKf != keyframes.end() && *nextKf == *kf && new_value > *kf) kf = nextKf;

                draggedKeyframeIndex = kf == keyframes.end() ? -1 : kf - keyframes.begin();
            }
        }
        else
        {
            // Snap to whole number
            float fractionalValue = new_value;
            new_value = roundf(new_value);

            // Move the time slider
            if (new_value != *data)
            {
                *data = new_value;
                if (isEnd != nullptr) *isEnd = fractionalValue > new_value;
                value_changed = true;
            }

            if (draggedKeyframeIndex >= 0 && movement != nullptr)
            {
                // Move the dragged keyframe
                movement->index = draggedKeyframeIndex;
                movement->newTime = new_value;
                value_changed = keyframes[draggedKeyframeIndex] != new_value;
            }
        }
    }
    else if (draggedId == id)
    {
        draggedKeyframeIndex = -1;
        draggedId = 0;
    }

    // Render timeline
    ImDrawList* draw_list = GetWindowDrawList();
    draw_list->AddLine(pos + ImVec2(0, size.y * 0.5f), pos + ImVec2(size.x, size.y * 0.5f), TIMELINE_COLOR, 4.0f);

    // Render loop region
    if (loopStart != nullptr && loopEnd != nullptr && max > min)
    {
        float startRatio = ImClamp((*loopStart - min) / (max - min), 0.0f, 1.0f);
        float endRatio = ImClamp((*loopEnd - min) / (max - min), 0.0f, 1.0f);
        float regionStartX = pos.x + startRatio * size.x;
        float regionEndX = pos.x + endRatio * size.x;
        draw_list->AddRectFilled(ImVec2(regionStartX, pos.y - 2), ImVec2(regionEndX, pos.y + size.y + 2), SELECTION_COLOR);
    }

    // Render current value indicator
    if (*data >= min && *data <= max)
    {
        float ratio = (*data - min) / (max - min);
        float value_x = pos.x + ratio * size.x;
        ImVec2 start = ImVec2(value_x, pos.y);

        draw_list->AddLine(start, start + ImVec2(0, size.y + g.Style.FramePadding.y), CURRENT_TIME_COLOR);
    }

    // Render keyframes
    for (int kfIndex = 0; kfIndex < keyframes.size(); kfIndex++)
    {
        float kf = keyframes[kfIndex];
        if (kf < min || kf > max) continue;
        const float KEYFRAME_SIZE = 10.0f;

        float ratio = (kf - min) / (max - min);
        float kf_x = pos.x + ratio * size.x;

        // Keyframes are rhombus-shaped
        ImVec2 rhombus_center = ImVec2(kf_x, pos.y + size.y * 0.5f);
        ImVec2 rhombus_top = rhombus_center + ImVec2(0, -KEYFRAME_SIZE * 0.5f);
        ImVec2 rhombus_right = rhombus_center + ImVec2(KEYFRAME_SIZE * 0.5f, 0);
        ImVec2 rhombus_bottom = rhombus_center + ImVec2(0, KEYFRAME_SIZE * 0.5f);
        ImVec2 rhombus_left = rhombus_center + ImVec2(-KEYFRAME_SIZE * 0.5f, 0);
        ImVec2 rhombus_points[4] = {rhombus_top, rhombus_right, rhombus_bottom, rhombus_left};

        bool isBeingDragged = draggedKeyframeIndex >= 0 && kfIndex == draggedKeyframeIndex && draggedId == id;
        bool isLeftHalf = (kfIndex + 1 < keyframes.size() && keyframes[kfIndex + 1] == kf);
        bool isRightHalf = (kfIndex > 0 && keyframes[kfIndex - 1] == kf);
        bool isDualKeyframe = isLeftHalf || isRightHalf;
        bool isActive = (kf == *data) && (!isDualKeyframe || isEnd == nullptr || (isRightHalf ? *isEnd : !*isEnd));

        auto keyframeColor = isBeingDragged ? KEYFRAME_DRAGGED_COLOR : (isActive ? KEYFRAME_ACTIVE_COLOR : KEYFRAME_INACTIVE_COLOR);

        if (isLeftHalf)
        {
            draw_list->AddTriangleFilled(rhombus_top, rhombus_left, rhombus_bottom, keyframeColor);
        }
        else if (isRightHalf)
        {
            draw_list->AddTriangleFilled(rhombus_top, rhombus_bottom, rhombus_right, keyframeColor);
            draw_list->AddLine(rhombus_top, rhombus_bottom, KEYFRAME_OUTLINE_COLOR, 2.0f);
        }
        else
        {
            draw_list->AddConvexPolyFilled(rhombus_points, 4, keyframeColor);
        }

        draw_list->AddPolyline(rhombus_points, 4, KEYFRAME_OUTLINE_COLOR, ImDrawFlags_Closed, 2.);
    }

    // Render label
    ImVec2 label_pos = pos + ImVec2(size.x + g.Style.FramePadding.x, g.Style.FramePadding.y);
    draw_list->AddText(label_pos, GetColorU32(ImGuiCol_Text), label);

    if (movement != nullptr) movement->index = draggedKeyframeIndex;

    return value_changed;
}

bool TimeSlider(const char* label, float* data, bool* isEnd, float min, float max, float* loopStart, float* loopEnd)
{
    const ImU32 TIMELINE_COLOR = GetColorU32(ImGuiCol_Text);
    const ImU32 CURRENT_TIME_COLOR = GetColorU32(ImGuiCol_SliderGrab);
    const ImU32 LOOP_MARKER_COLOR = GetColorU32(ImGuiCol_SliderGrabActive);
    const ImU32 LOOP_MARKER_HOVER_COLOR = IM_COL32(100, 200, 255, 255);
    const float BRACKET_WIDTH = 8.0f;
    const float BRACKET_HEIGHT = 12.0f;
    const float MARKER_HOVER_RADIUS = 5.0f;
    const float SNAP_THRESHOLD = 8.0f;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    ImGuiID id = window->GetID(label);

    // Static drag state: -1 = none, 0 = time slider, 1 = loop start, 2 = loop end
    static int draggedElement = -1;
    static ImGuiID draggedId = 0;

    // Check if loop markers are available
    bool hasLoopMarkers = (loopStart != nullptr && loopEnd != nullptr && max > min);

    // Calculate bounding box for the invisible button
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(CalcItemWidth(), CalcTextSize("0", nullptr, true).y + g.Style.FramePadding.y * 2.0f);
    ImVec2 fullSize = ImVec2(size.x + CalcTextSize("9999999", nullptr, true).x + g.Style.ItemInnerSpacing.x, size.y);

    bool value_changed = false;
    bool pressed = InvisibleButton(label, fullSize, ImGuiButtonFlags_PressedOnClick);

    // Pre-calculate loop marker positions if available
    float viewRange = 0.0f;
    float startX = 0.0f, endX = 0.0f;
    ImRect startMarkerBB, endMarkerBB;
    if (hasLoopMarkers)
    {
        viewRange = max - min;
        float startRatio = ImClamp((*loopStart - min) / viewRange, -0.1f, 1.1f);
        float endRatio = ImClamp((*loopEnd - min) / viewRange, -0.1f, 1.1f);
        startX = pos.x + startRatio * size.x;
        endX = pos.x + endRatio * size.x;

        float markerVCenter = pos.y + size.y / 2.0f;
        startMarkerBB =
            ImRect(startX, markerVCenter - BRACKET_HEIGHT / 2.0f, startX + BRACKET_WIDTH, markerVCenter + BRACKET_HEIGHT / 2.0f);
        endMarkerBB = ImRect(endX - BRACKET_WIDTH, markerVCenter - BRACKET_HEIGHT / 2.0f, endX, markerVCenter + BRACKET_HEIGHT / 2.0f);
    }

    if (pressed || IsItemActive())
    {
        draggedId = id;
        ImVec2 relativePos = GetIO().MousePos - pos;
        float ratio = relativePos.x / size.x;
        ratio = ImClamp(ratio, 0.0f, 1.0f);
        float mouseValue = min + ratio * (max - min);
        float snapThresholdValue = SNAP_THRESHOLD / size.x * (max - min);

        if (pressed)
        {
            // Detect which element is being clicked
            draggedElement = (hasLoopMarkers && startMarkerBB.Contains(GetIO().MousePos)) ? 1
                             : (hasLoopMarkers && endMarkerBB.Contains(GetIO().MousePos)) ? 2
                                                                                          : 0;
            if (ImGui::GetIO().KeyCtrl && loopStart != nullptr && loopEnd != nullptr)
            {
                // Ctrl-drag should mark the selection
                *loopStart = ImClamp(roundf(mouseValue), 0.0f, max);
                *loopEnd = *loopStart;
                draggedElement = 2;
            }
        }
        else
        {
            // Drag occurred
            // Handle dragging of the appropriate element
            if (draggedElement == 0)
            {
                // Dragging time slider
                float new_value = roundf(mouseValue);
                if (new_value != *data)
                {
                    *data = new_value;
                    if (isEnd != nullptr) *isEnd = mouseValue > new_value;
                    value_changed = true;
                }
            }
            else if (draggedElement == 1 && loopStart != nullptr && loopEnd != nullptr)
            {
                // Dragging loop start marker
                float newStart = ImClamp(roundf(mouseValue), 0.0f, *loopEnd - 1.f);
                if (newStart != *loopStart)
                {
                    *loopStart = newStart;
                    value_changed = true;
                }
            }
            else if (draggedElement == 2 && loopStart != nullptr && loopEnd != nullptr)
            {
                // Dragging loop end marker
                float newEnd = ImClamp(roundf(mouseValue), *loopStart + 1.f, max);

                if (newEnd != *loopEnd)
                {
                    *loopEnd = newEnd;
                    value_changed = true;
                }
            }
        }
    }
    else if (draggedId == id)
    {
        draggedElement = -1;
        draggedId = 0;
    }

    // Render timeline
    ImDrawList* draw_list = GetWindowDrawList();
    draw_list->AddLine(pos + ImVec2(0, size.y * .5f), pos + ImVec2(size.x, size.y * .5f), TIMELINE_COLOR, 1.0f);

    if (hasLoopMarkers)
    {
        // Draw loop brackets
        if (*loopStart >= min && *loopStart <= max)
        {
            bool markerHovered = startMarkerBB.Contains(GetIO().MousePos);
            bool isBeingDragged = draggedElement == 1 && draggedId == id;
            ImU32 markerColor = isBeingDragged ? LOOP_MARKER_HOVER_COLOR : (markerHovered ? LOOP_MARKER_HOVER_COLOR : LOOP_MARKER_COLOR);

            // Draw < (with point at correct time)
            ImVec2 top = ImVec2(startX + BRACKET_WIDTH, pos.y + size.y / 2.0f - BRACKET_HEIGHT / 2.0f);
            ImVec2 center = ImVec2(startX, pos.y + size.y / 2.0f);
            ImVec2 bottom = ImVec2(startX + BRACKET_WIDTH, pos.y + size.y / 2.0f + BRACKET_HEIGHT / 2.0f);

            draw_list->AddLine(top, center, markerColor, 2.0f);
            draw_list->AddLine(center, bottom, markerColor, 2.0f);
        }

        if (*loopEnd >= min && *loopEnd <= max)
        {
            bool markerHovered = endMarkerBB.Contains(GetIO().MousePos);
            bool isBeingDragged = draggedElement == 2 && draggedId == id;
            ImU32 markerColor = isBeingDragged ? LOOP_MARKER_HOVER_COLOR : (markerHovered ? LOOP_MARKER_HOVER_COLOR : LOOP_MARKER_COLOR);

            // Draw > (with point at correct time)
            ImVec2 top = ImVec2(endX - BRACKET_WIDTH, pos.y + size.y / 2.0f - BRACKET_HEIGHT / 2.0f);
            ImVec2 center = ImVec2(endX, pos.y + size.y / 2.0f);
            ImVec2 bottom = ImVec2(endX - BRACKET_WIDTH, pos.y + size.y / 2.0f + BRACKET_HEIGHT / 2.0f);

            draw_list->AddLine(top, center, markerColor, 2.0f);
            draw_list->AddLine(center, bottom, markerColor, 2.0f);
        }

        float startRatio = ImClamp((*loopStart - min) / (max - min), 0.0f, 1.0f);
        float endRatio = ImClamp((*loopEnd - min) / (max - min), 0.0f, 1.0f);
        float regionStartX = pos.x + startRatio * size.x;
        float regionEndX = pos.x + endRatio * size.x;
        draw_list->AddRectFilled(ImVec2(regionStartX, pos.y - 2), ImVec2(regionEndX, pos.y + size.y + 2), SELECTION_COLOR);
    }

    // Render current value indicator
    if (*data >= min && *data <= max)
    {
        float ratio = (*data - min) / (max - min);
        ImVec2 start = pos + ImVec2(ratio * size.x, size.y * .5f);

        // Upside-down triangle
        ImVec2 points[3] = {start, start + ImVec2(3, -6), start + ImVec2(-3, -6)};
        draw_list->AddConvexPolyFilled(points, 3, TIMELINE_COLOR);

        // Line down
        draw_list->AddLine(start, start + ImVec2(0, size.y * .5f + g.Style.FramePadding.y), CURRENT_TIME_COLOR);
    }

    // Render time
    char buf[128];
    if (loopStart == nullptr || loopEnd == nullptr)
        ImFormatString(buf, IM_ARRAYSIZE(buf), "%.3f", *data);
    else
        ImFormatString(buf, IM_ARRAYSIZE(buf), "%.3f (selection: %.0f-%.0f)", *data, *loopStart, *loopEnd);

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

bool PlayPauseButton(bool shouldDrawPauseIcon)
{
    // Draw play/pause icons using ImGui's draw list
    float height = ImGui::GetFrameHeight();
    ImVec2 buttonSize(height, height);
    ImVec2 iconSize(10, 10);
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    bool hovered, held;
    bool didChange = InvisibleButton("##PlayPause", buttonSize, &hovered, &held);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);

    drawList->AddRectFilled(cursorPos, cursorPos + buttonSize, GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg));

    ImVec2 pad = (buttonSize - iconSize) / 2;

    if (shouldDrawPauseIcon)
    {
        // Draw Pause icon: two vertical bars
        float barWidth = 4.0f;
        ImVec2 p1 = cursorPos + pad;
        ImVec2 p2 = p1 + ImVec2(barWidth, iconSize.y);
        ImVec2 p3 = ImVec2(p1.x + iconSize.x - barWidth, p1.y);
        ImVec2 p4 = p1 + iconSize;
        drawList->AddRectFilled(p1, p2, color);
        drawList->AddRectFilled(p3, p4, color);
    }
    else
    {
        // Draw Play icon: right-pointing triangle
        ImVec2 p1 = cursorPos + pad;
        ImVec2 p2 = ImVec2(p1.x, p1.y + iconSize.y);
        ImVec2 p3 = ImVec2(p1.x + iconSize.x, p1.y + iconSize.y / 2.f);
        drawList->AddTriangleFilled(p1, p2, p3, color);
    }

    return didChange;
}

bool ZoomPanSlider(const char* label, float* start, float* end, float min, float max)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    ImGuiID id = window->GetID(label);

    // Calculate bounding box for the invisible button
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(CalcItemWidth(), CalcTextSize("0", nullptr, true).y + g.Style.FramePadding.y * 2.0f);
    ImVec2 fullSize = ImVec2(size.x + CalcTextSize(label, nullptr, true).x + g.Style.ItemInnerSpacing.x, size.y);

    bool hovered, held;

    bool valueChanged = false;

    bool pressed = InvisibleButton(label, fullSize, &hovered, &held);

    if (!pressed && IsItemActive())
    {
        auto& io = GetIO();

        // Lock and hide the mouse while dragging
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        ImVec2 mouseDelta = io.MousePos - io.MouseClickedPos[0];
        // Teleport only after the mouse moved, because otherwise the teleportation overrides the move
        if (mouseDelta != ImVec2(0, 0)) TeleportMousePos(io.MouseClickedPos[0]);

        float range = *end - *start;
        float panAmount = mouseDelta.x / size.x * (max - min);
        float zoomAmount = mouseDelta.y / size.x * (max - min);
        float newStart = *start + panAmount;
        float newEnd = *end + panAmount;

        float newRange = std::max(0.01f, range - zoomAmount);
        float center = (newStart + newEnd) / 2.0f;
        newStart = center - newRange / 2.0f;
        newEnd = center + newRange / 2.0f;

        // Clamp to min/max
        if (newStart < min)
        {
            newStart = min;
            newEnd = std::min(max, newStart + newRange);
        }
        else if (newEnd > max)
        {
            newEnd = max;
            newStart = std::max(min, newEnd - newRange);
        }

        if (newStart != *start || newEnd != *end)
        {
            *start = newStart;
            *end = newEnd;
            valueChanged = true;
        }
    }

    // Render timeline
    ImDrawList* drawList = GetWindowDrawList();
    drawList->AddRect(pos, pos + size, GetColorU32(ImGuiCol_FrameBg), 0.0f, ImDrawFlags_None, 1.0f);

    // Render current range indicator
    if (*start >= min && *end <= max)
    {
        float startRatio = (*start - min) / (max - min);
        float endRatio = (*end - min) / (max - min);
        float startX = pos.x + startRatio * size.x;
        float endX = pos.x + endRatio * size.x;
        ImVec2 rectMin = ImVec2(startX, pos.y);
        ImVec2 rectMax = ImVec2(endX, pos.y + size.y);
        drawList->AddRectFilled(rectMin, rectMax,
            GetColorU32(held      ? ImGuiCol_ScrollbarGrabActive
                        : hovered ? ImGuiCol_ScrollbarGrabHovered
                                  : ImGuiCol_ScrollbarGrab));
    }

    ImGui::SetItemTooltip("Left/Right: Pan\nUp/Down: Zoom");

    return valueChanged;
}