#pragma once

#include "imgui.h"
#include <vector>
#include "../uniform_data_types.h"

struct KeyframeMovementData
{
    int index;
    int newTime;
};

bool DragVector2(const char* label, ImVec2* v, float v_speed = 1.0f, const ImVec2* v_min = nullptr, const ImVec2* v_max = nullptr,
    const char* format = "(%.3f, %.3f)", ImGuiSliderFlags flags = 0);
bool KeyframeSlider(const char* label, int* data, int min, int max, std::vector<int>& keyframes, KeyframeMovementData* movement);
bool TimeSlider(const char* label, int* data, int min, int max);
bool KeyframeMarker(const char* label, bool* data, KeyframeInterpolation* interpolation, float* tension);