#include "uniform_editor.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_benzene_widgets.h"
#include "components/keyframe_marker.h"
#include "actions/ChangeUniformValue.h"
#include "actions/DeleteKeyframe.h"
#include "actions/SplitKeyframeToDual.h"
#include "actions/DeleteUniform.h"
#include "actions/ChangeUniformType.h"
#include "actions/ChangeUniformGroup.h"
#include "actions/ChangeUniformQuantization.h"
#include "actions/AddUniformGroup.h"
#include "actions/RenameUniformGroup.h"
#include <algorithm>

std::optional<std::string> nameDialog(const char* str_id)
{
    static char name[50] = "";
    std::optional<std::string> result = std::nullopt;

    if (ImGui::BeginPopup(str_id))
    {
        if (ImGui::IsWindowAppearing())
        {
            // Window just opened - clear name and focus
            name[0] = 0;
            ImGui::SetKeyboardFocusHere(0);
        }

        bool wasEnterPressed = ImGui::InputText("Name", name, 50, ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::Button("OK") || wasEnterPressed)
        {
            result = std::string(name);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return result;
}

struct ActiveDragInfo
{
    std::string uniformName;
    std::optional<UniformKeyframe> beforeKeyframe;
    float targetTime;
    bool targetIsEnd;
};

bool renderSingleUniformTab(const std::string& group, float time, bool isEndKeyframe, bool& shouldKeepPlaying,
    bool& outShouldReloadFragmentShader, EditorState& editorState, ActionHistory& actionHistory)
{
    std::vector<Uniform>& uniformList = editorState.config.uniformList;
    const std::vector<std::string>& groups = editorState.groups;

    bool didAnythingChange = false;
    outShouldReloadFragmentShader = false;

    static std::optional<ActiveDragInfo> activeDrag;

    std::optional<std::string> uniformNameToDelete;

    for (auto uniformIt = uniformList.begin(); uniformIt != uniformList.end(); ++uniformIt)
    {
        Uniform& uniform = *uniformIt;

        if (uniform.group != group) continue;

        UniformValue value = uniform.valueAtTime(time, isEndKeyframe);
        bool didThisUniformChange = false;
        UniformKeyframe* keyframeAtCurrentTime = uniform.getKeyframeAtTime(time, isEndKeyframe);

        float lastKeyframeTime = uniform.keyframes.empty() ? 0 : uniform.keyframes.back().time;
        bool isBeyondLastKeyframe = time > lastKeyframeTime;
        float targetTime = isBeyondLastKeyframe ? lastKeyframeTime : time;
        bool targetIsEnd = isBeyondLastKeyframe ? true : isEndKeyframe;
        UniformKeyframe* targetKeyframe = uniform.getKeyframeAtTime(targetTime, targetIsEnd);

        bool itemActivated = false;
        bool itemDeactivated = false;

        switch (uniform.type)
        {
        case UniformType::Float:
            didThisUniformChange = ImGui::DragFloat(uniform.name.c_str(), &value.f, 0.005f);
            break;
        case UniformType::Int:
            didThisUniformChange = ImGui::DragInt(uniform.name.c_str(), &value.i, 0.005f);
            break;
        case UniformType::Bool:
            didThisUniformChange = ImGui::Checkbox(uniform.name.c_str(), &value.b);
            break;
        case UniformType::Vec2:
            ImGui::SetNextItemWidth(ImGui::CalcItemWidth() / 2.);
            didThisUniformChange = ImGui::DragFloat(("##XComp" + uniform.name).c_str(), value.v2, 0.005f);
            ImGui::SetNextItemWidth(ImGui::CalcItemWidth() / 2.);
            ImGui::SameLine();
            didThisUniformChange |= ImGui::DragFloat(uniform.name.c_str(), value.v2 + 1, 0.005f);
            break;
        case UniformType::Color:
            didThisUniformChange = ImGui::ColorEdit3(uniform.name.c_str(), value.v3);
            break;
        }

        itemActivated |= ImGui::IsItemActivated();
        itemDeactivated |= ImGui::IsItemDeactivatedAfterEdit();

        if (itemActivated)
            activeDrag = ActiveDragInfo{
                uniform.name, targetKeyframe ? std::optional<UniformKeyframe>(*targetKeyframe) : std::nullopt, targetTime, targetIsEnd};

        int uniformTypeIndex = uniform.type == UniformType::Float   ? 0
                               : uniform.type == UniformType::Vec2  ? 1
                               : uniform.type == UniformType::Vec3  ? 2
                               : uniform.type == UniformType::Color ? 3
                                                                    : 0;

        char* items[] = {"float", "vec2", "color"};

        if (ImGui::BeginPopupContextItem(uniform.name.c_str()))
        {
            if (ImGui::Combo("Type", &uniformTypeIndex, items, 3))
            {
                UniformType newType = uniform.type;
                switch (uniformTypeIndex)
                {
                case 0:
                    newType = UniformType::Float;
                    break;
                case 1:
                    newType = UniformType::Vec2;
                    break;
                case 2:
                    newType = UniformType::Color;
                    break;
                default:
                    break;
                }

                // Don't activate didThisUniformChange here so a keyframe won't be created
                actionHistory.execute(
                    std::make_unique<ChangeUniformType>(uniform.name, uniform.type, uniform.keyframes, newType), editorState);
                didAnythingChange = true;
                outShouldReloadFragmentShader = true;
            }

            int uniformGroupIndex = uniform.group.empty() ? 0 : std::find(groups.begin(), groups.end(), uniform.group) - groups.begin() + 1;

            // Combo takes an array, so convert the groups to a const char*[]
            std::vector<const char*> groupItems;
            groupItems.reserve(groups.size() + 1);
            groupItems.push_back("(no group)");
            for (const std::string& g : groups)
                groupItems.push_back(g.c_str());

            if (ImGui::Combo("Group", &uniformGroupIndex, groupItems.data(), groupItems.size()))
            {
                std::string afterGroup = uniformGroupIndex == 0 ? std::string() : groups[uniformGroupIndex - 1];
                actionHistory.execute(std::make_unique<ChangeUniformGroup>(uniform.name, uniform.group, afterGroup), editorState);
                didAnythingChange = true;
            }

            int quantizationIndex = uniform.quantization.has_value() ? uniform.quantization.value() + 1 : 0;
            const char* quantizationItems[] = {"Default", "0", "1", "2", "3", "4", "5", "6"};

            if (ImGui::Combo("Quantization", &quantizationIndex, quantizationItems, 8))
            {
                std::optional<int> afterQuantization = quantizationIndex == 0 ? std::nullopt : std::optional<int>(quantizationIndex - 1);
                actionHistory.execute(
                    std::make_unique<ChangeUniformQuantization>(uniform.name, uniform.quantization, afterQuantization), editorState);
                didAnythingChange = true;
            }

            if (ImGui::Selectable("Delete"))
            {
                uniformNameToDelete = uniform.name;
                ImGui::EndPopup();
                continue;
            }

            ImGui::EndPopup();
        }

        bool hasKeyframeAtCurrentTime = keyframeAtCurrentTime != nullptr;
        bool shouldHaveKeyframeAtCurrentTime = hasKeyframeAtCurrentTime;
        KeyframeInterpolation interpolation =
            hasKeyframeAtCurrentTime ? keyframeAtCurrentTime->interpolation : KeyframeInterpolation::Linear;
        float tension = hasKeyframeAtCurrentTime ? keyframeAtCurrentTime->interpolationFactor : 0.5f;

        bool canSplitToDual = hasKeyframeAtCurrentTime && uniform.countKeyframesAtTime(time) < 2;
        bool shouldSplitToDual = false;

        ImGui::SameLine();
        bool didKeyframeInfoChange = KeyframeMarkerWithContextMenu(
            (uniform.name + "_kf").c_str(), &shouldHaveKeyframeAtCurrentTime, &interpolation, &tension, canSplitToDual, &shouldSplitToDual);

        if (shouldSplitToDual && keyframeAtCurrentTime != nullptr)
        {
            actionHistory.execute(
                std::make_unique<SplitKeyframeToDual>(uniform.name, time, keyframeAtCurrentTime->value, interpolation, tension),
                editorState);
            didAnythingChange = true;
            shouldKeepPlaying = false;
        }

        // Set a keyframe if the uniform changed *only if* it's before another keyframe!
        // This is because if it's after the last one, it's more natural to just update the last keyframe value instead.
        // However, if we're between two keyframes, we don't know which keyframe the user would want to change, or how to interpolate the
        // data.
        bool shouldSetKeyframeDueToUniformChange = didThisUniformChange && !isBeyondLastKeyframe;
        bool shouldSetKeyframeDueToMarkerChange = didKeyframeInfoChange && shouldHaveKeyframeAtCurrentTime;

        bool shouldSetKeyframe = shouldSetKeyframeDueToUniformChange || shouldSetKeyframeDueToMarkerChange;
        bool shouldRemoveKeyframe = didKeyframeInfoChange && !shouldHaveKeyframeAtCurrentTime && hasKeyframeAtCurrentTime;
        bool shouldUpdateLastKeyframeValue = didThisUniformChange && isBeyondLastKeyframe;

        if (shouldSetKeyframe)
        {
            if (shouldSetKeyframeDueToMarkerChange && !shouldSetKeyframeDueToUniformChange)
            {
                // Marker-driven changes (toggling the keyframe on, changing interpolation/tension) are discrete,
                // single-frame events - record them immediately rather than relying on the value-widget's drag tracking below.
                std::optional<UniformKeyframe> before =
                    keyframeAtCurrentTime ? std::optional<UniformKeyframe>(*keyframeAtCurrentTime) : std::nullopt;
                uniform.setKeyframeAtTime(time, isEndKeyframe, value, interpolation, tension);
                // actionHistory.record(
                //     std::make_unique<ChangeUniformValue>(uniform.name, time, isEndKeyframe, before, value, interpolation, tension));
            }
            else
            {
                uniform.setKeyframeAtTime(time, isEndKeyframe, value, interpolation, tension);
            }
            didAnythingChange = true;
            shouldKeepPlaying = false; // Pause only if a keyframe was created, no other reason
        }
        else if (shouldUpdateLastKeyframeValue)
        {
            uniform.setKeyframeAtTime(lastKeyframeTime, true, value, interpolation, tension);
            didAnythingChange = true;
        }
        else if (shouldRemoveKeyframe)
        {
            // Remove the keyframe at the current time
            actionHistory.execute(std::make_unique<DeleteKeyframe>(uniform.name, time, isEndKeyframe, *keyframeAtCurrentTime), editorState);
            didAnythingChange = true;
        }

        if (itemDeactivated && activeDrag.has_value() && activeDrag->uniformName == uniform.name)
        {
            UniformKeyframe* finalKeyframe = uniform.getKeyframeAtTime(activeDrag->targetTime, activeDrag->targetIsEnd);
            if (finalKeyframe)
            {
                actionHistory.record(std::make_unique<ChangeUniformValue>(uniform.name, activeDrag->targetTime, activeDrag->targetIsEnd,
                    activeDrag->beforeKeyframe, finalKeyframe->value, finalKeyframe->interpolation, finalKeyframe->interpolationFactor));
            }
            activeDrag.reset();
        }
    }

    if (uniformNameToDelete.has_value())
    {
        Uniform* uniformToDelete = editorState.findUniform(*uniformNameToDelete);
        if (uniformToDelete) actionHistory.execute(std::make_unique<DeleteUniform>(*uniformToDelete), editorState);
        outShouldReloadFragmentShader = true;
    }

    return didAnythingChange;
}

bool renderAndUpdateUniforms(float time, bool isEndKeyframe, bool& shouldKeepPlaying, bool& outShouldReloadFragmentShader,
    EditorState& editorState, ActionHistory& actionHistory, int sidebarHeight)
{
    if (editorState.config.uniformList.empty()) return false;

    bool didAnythingChange = false;
    outShouldReloadFragmentShader = false;

    ImGui::SeparatorText("Uniforms");

    // Force the uniform window to leave enough room for the camera panel
    float maxWindowHeight = sidebarHeight - ImGui::GetTextLineHeightWithSpacing() * 21.0f;
    float windowHeight = min(sidebarHeight / 2, maxWindowHeight);

    ImGui::BeginChild("Uniforms", ImVec2(0, windowHeight));

    ImGui::BeginTabBar("Uniforms");

    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) ImGui::OpenPopup("Add Group Name");

    auto result = nameDialog("Add Group Name");
    if (result.has_value()) actionHistory.execute(std::make_unique<AddUniformGroup>(result.value()), editorState);

    static int currentlyRenamedGroup = -1;

    if (ImGui::BeginTabItem("Unsorted"))
    {
        editorState.currentGroup = "";
        didAnythingChange |=
            renderSingleUniformTab("", time, isEndKeyframe, shouldKeepPlaying, outShouldReloadFragmentShader, editorState, actionHistory);
        ImGui::EndTabItem();
    }

    for (int i = 0; i < (int)editorState.groups.size(); i++)
    {
        std::string group = editorState.groups[i];
        bool openRenameDialog = false;

        if (ImGui::BeginTabItem(group.c_str()))
        {
            editorState.currentGroup = group;

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::Selectable("Rename"))
                {
                    currentlyRenamedGroup = i;
                    ImGui::CloseCurrentPopup();

                    ImGui::OpenPopup("Rename Group");
                    openRenameDialog = true;
                }

                ImGui::EndPopup();
            }

            bool tabNeedsReload = false;
            didAnythingChange |=
                renderSingleUniformTab(group, time, isEndKeyframe, shouldKeepPlaying, tabNeedsReload, editorState, actionHistory);
            outShouldReloadFragmentShader |= tabNeedsReload;

            ImGui::EndTabItem();
        }

        // This is necessary because OpenPopup needs to be called from the outside of the stack
        if (openRenameDialog) ImGui::OpenPopup("Rename Group");
    }

    result = nameDialog("Rename Group");

    if (result.has_value())
    {
        actionHistory.execute(std::make_unique<RenameUniformGroup>(editorState.groups[currentlyRenamedGroup], result.value()), editorState);
        currentlyRenamedGroup = -1;
    }

    ImGui::EndTabBar();

    ImGui::EndChild();

    return didAnythingChange;
}
