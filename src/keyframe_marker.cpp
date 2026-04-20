#include "keyframe_marker.h"
#include "imgui/imgui.h"

namespace
{
constexpr int NUM_POSSIBLE_INTERPOLATIONS = 4;
const KeyframeInterpolation POSSIBLE_INTERPOLATIONS[] = {
    KeyframeInterpolation::Linear, KeyframeInterpolation::Step, KeyframeInterpolation::Tonemap, KeyframeInterpolation::Gain};
const char* INTERPOLATION_NAMES[] = {"Linear", "Step", "Power (ease-in/ease-out)", "Gain (ease-in-out)"};

void PlotTension(KeyframeInterpolation interpolation, float tension)
{
    constexpr int PLOT_RESOLUTION = 100;
    float values[PLOT_RESOLUTION] = {0};

    for (int i = 0; i < PLOT_RESOLUTION; i++)
    {
        float x = static_cast<float>(i) / static_cast<float>(PLOT_RESOLUTION);
        values[i] = interpolate0to1(x, interpolation, tension);
    }

    ImGui::PlotLines("##TensionGraph", values, PLOT_RESOLUTION);
}
} // namespace

bool KeyframeMarkerWithContextMenu(
    const char* label, bool* data, KeyframeInterpolation* interpolation, float* tension, bool showSplitToDual, bool* shouldSplitToDual)
{
    bool didChange = KeyframeMarker(label, data);

    if (*data && ImGui::BeginPopupContextItem(label))
    {
        if (showSplitToDual && shouldSplitToDual != nullptr)
        {
            if (ImGui::Selectable("Split to Dual Keyframe", false))
            {
                *shouldSplitToDual = true;
                didChange = true;
            }

            ImGui::Separator();
        }

        for (int i = 0; i < NUM_POSSIBLE_INTERPOLATIONS; i++)
        {
            bool isSelected = (*interpolation == POSSIBLE_INTERPOLATIONS[i]);

            if (ImGui::Selectable(INTERPOLATION_NAMES[i], isSelected))
            {
                *interpolation = POSSIBLE_INTERPOLATIONS[i];
                didChange = true;
            }

            if (isSelected) ImGui::SetItemDefaultFocus();
        }

        if (ImGui::SliderFloat("Tension", tension, 0.0f, 1.0f, "%.2f"))
        {
            didChange = true;
        }

        PlotTension(*interpolation, *tension);

        ImGui::EndPopup();
    }

    return didChange;
}
