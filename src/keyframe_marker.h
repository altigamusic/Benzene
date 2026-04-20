#pragma once

#include "imgui/imgui_benzene_widgets.h"
#include "uniform_data_types.h"

bool KeyframeMarkerWithContextMenu(const char* label, bool* data, KeyframeInterpolation* interpolation, float* tension,
    bool showSplitToDual = false, bool* shouldSplitToDual = nullptr);
