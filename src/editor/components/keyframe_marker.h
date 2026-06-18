#pragma once

#include "../../imgui/imgui_benzene_widgets.h"
#include "../../uniform_data_types.h"
#include <optional>
#include <variant>

struct KeyframeMarkerResult
{
    struct KeyframeAdded
    {
    };
    struct KeyframeRemoved
    {
    };
    struct SplitToDual
    {
    };

    struct ChangeInterpolation
    {
        KeyframeInterpolation from, to;
    };

    struct ChangeTension
    {
        float from, to;
    };

    /// <summary>
    /// The user changed the tension but didn't release the mouse yet.
    /// </summary>
    struct DragTension
    {
    }; 

    std::variant<KeyframeAdded, KeyframeRemoved, SplitToDual, ChangeInterpolation, ChangeTension, DragTension> value;
};

std::optional<KeyframeMarkerResult> KeyframeMarkerWithContextMenu(
    const char* label, bool hasKeyframe, KeyframeInterpolation* interpolation, float* tension, bool showSplitToDual = false);
