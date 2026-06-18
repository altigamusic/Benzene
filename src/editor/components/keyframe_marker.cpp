#include "keyframe_marker.h"
#include "../../imgui/imgui.h"

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

std::optional<KeyframeMarkerResult> KeyframeMarkerWithContextMenu(
    const char* label, bool hasKeyframe, KeyframeInterpolation* interpolation, float* tension, bool showSplitToDual)
{
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushID(label);
    const ImGuiID tensionKey = ImGui::GetID("tensionAtDragStart");
    ImGui::PopID();
    constexpr float NO_TENSION = -1.0f;

    float tensionAtDragStart = storage->GetFloat(tensionKey, NO_TENSION);

    bool didToggle = KeyframeMarker(label, &hasKeyframe);

    if (didToggle)
    {
        return hasKeyframe ? KeyframeMarkerResult{KeyframeMarkerResult::KeyframeAdded{}}
                           : KeyframeMarkerResult{KeyframeMarkerResult::KeyframeRemoved{}};
    }

    std::optional<KeyframeMarkerResult> result;

    if (!hasKeyframe || !ImGui::BeginPopupContextItem(label))
    {
        storage->SetFloat(tensionKey, NO_TENSION);
        return result;
    }

    if (showSplitToDual)
    {
        if (ImGui::Selectable("Split to Dual Keyframe", false))
        {
            result = KeyframeMarkerResult{KeyframeMarkerResult::SplitToDual{}};
        }

        ImGui::Separator();
    }

    for (int i = 0; i < NUM_POSSIBLE_INTERPOLATIONS; i++)
    {
        bool isSelected = (*interpolation == POSSIBLE_INTERPOLATIONS[i]);

        if (ImGui::Selectable(INTERPOLATION_NAMES[i], isSelected))
        {
            KeyframeInterpolation from = *interpolation;
            *interpolation = POSSIBLE_INTERPOLATIONS[i];
            if (from != *interpolation)
            {
                result = KeyframeMarkerResult{
                    KeyframeMarkerResult::ChangeInterpolation{from, *interpolation}
                };
            }
        }

        if (isSelected) ImGui::SetItemDefaultFocus();
    }

    float previousTension = *tension;
    if (ImGui::SliderFloat("Tension", tension, 0.0f, 1.0f, "%.2f"))
    {
        if (tensionAtDragStart == NO_TENSION) storage->SetFloat(tensionKey, previousTension);

        result = KeyframeMarkerResult{KeyframeMarkerResult::DragTension{}};
    }

    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        if (tensionAtDragStart != NO_TENSION)
        {
            result = KeyframeMarkerResult{
                KeyframeMarkerResult::ChangeTension{tensionAtDragStart, *tension}
            };
            storage->SetFloat(tensionKey, NO_TENSION);
        }
    }

    PlotTension(*interpolation, *tension);

    ImGui::EndPopup();

    return result;
}
