#pragma once

#include "imgui.h"

bool DragVector2(const char* label, ImVec2* v, float v_speed = 1.0f, const ImVec2* v_min = nullptr, const ImVec2* v_max = nullptr,
    const char* format = "(%.3f, %.3f)", ImGuiSliderFlags flags = 0);
