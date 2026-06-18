#include "uniform_editor.h"
#include "imgui/imgui.h"
#include "imgui/imgui_benzene_widgets.h"
#include "keyframe_marker.h"
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

bool renderSingleUniformTab(const std::string& group, float time, bool isEndKeyframe, bool& shouldKeepPlaying,
    bool& outShouldReloadFragmentShader, std::vector<Uniform>& uniformList, const std::vector<std::string>& groups)
{
    bool didAnythingChange = false;
    outShouldReloadFragmentShader = false;

    auto uniformToDelete = uniformList.end();

    for (auto uniformIt = uniformList.begin(); uniformIt != uniformList.end(); ++uniformIt)
    {
        Uniform& uniform = *uniformIt;

        if (uniform.group != group) continue;

        UniformValue value = uniform.valueAtTime(time, isEndKeyframe);
        bool didThisUniformChange = false;
        UniformKeyframe* keyframeAtCurrentTime = uniform.getKeyframeAtTime(time, isEndKeyframe);

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
            didThisUniformChange = DragVector2(uniform.name.c_str(), (ImVec2*)(&value.v2), 0.005f);
            break;
        case UniformType::Color:
            didThisUniformChange = ImGui::ColorEdit3(uniform.name.c_str(), value.v3);
            break;
        }

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
                switch (uniformTypeIndex)
                {
                case 0:
                    uniform.type = UniformType::Float;
                    break;
                case 1:
                    uniform.type = UniformType::Vec2;
                    break;
                case 2:
                    uniform.type = UniformType::Color;
                    break;
                default:
                    break;
                }

                // Don't activate didThisUniformChange here so a keyframe won't be created
                didAnythingChange = true;
                outShouldReloadFragmentShader = true;
                uniform.keyframes.clear();
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
                if (uniformGroupIndex == 0) // No group
                    uniform.group.clear();
                else
                    uniform.group = groups[uniformGroupIndex - 1];

                didAnythingChange = true;
            }

            int quantizationIndex = uniform.quantization.has_value() ? uniform.quantization.value() + 1 : 0;
            const char* quantizationItems[] = {"Default", "0", "1", "2", "3", "4", "5", "6"};

            if (ImGui::Combo("Quantization", &quantizationIndex, quantizationItems, 8))
            {
                if (quantizationIndex == 0)
                    uniform.quantization.reset();
                else
                    uniform.quantization = quantizationIndex - 1;

                didAnythingChange = true;
            }

            if (ImGui::Selectable("Delete"))
            {
                uniformToDelete = uniformIt;
                ImGui::End();
                continue;
            }

            ImGui::End();
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
            uniform.insertKeyframeAtTime(time, true, keyframeAtCurrentTime->value, interpolation, tension);
            didAnythingChange = true;
            shouldKeepPlaying = false;
        }

        float lastKeyframeTime = uniform.keyframes.empty() ? 0 : uniform.keyframes.back().time;
        bool isBeyondLastKeyframe = time > lastKeyframeTime;

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
            uniform.setKeyframeAtTime(time, isEndKeyframe, value, interpolation, tension);
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
            uniform.removeKeyframeAtTime(time, isEndKeyframe);
            didAnythingChange = true;
        }
    }

    if (uniformToDelete != uniformList.end())
    {
        uniformList.erase(uniformToDelete);
        outShouldReloadFragmentShader = true;
    }

    return didAnythingChange;
}

bool renderAndUpdateUniforms(float time, bool isEndKeyframe, bool& shouldKeepPlaying, bool& outShouldReloadFragmentShader,
    std::vector<Uniform>& uniformList, std::vector<std::string>& groups, std::string& currentGroup, int sidebarHeight)
{
    if (uniformList.empty()) return false;

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
    if (result.has_value()) groups.push_back(result.value());

    static int currentlyRenamedGroup = -1;

    if (ImGui::BeginTabItem("Unsorted"))
    {
        currentGroup = "";
        didAnythingChange |=
            renderSingleUniformTab("", time, isEndKeyframe, shouldKeepPlaying, outShouldReloadFragmentShader, uniformList, groups);
        ImGui::EndTabItem();
    }

    for (int i = 0; i < (int)groups.size(); i++)
    {
        std::string group = groups[i];
        bool openRenameDialog = false;

        if (ImGui::BeginTabItem(group.c_str()))
        {
            currentGroup = group;

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
            didAnythingChange |= renderSingleUniformTab(group, time, isEndKeyframe, shouldKeepPlaying, tabNeedsReload, uniformList, groups);
            outShouldReloadFragmentShader |= tabNeedsReload;

            ImGui::EndTabItem();
        }

        // This is necessary because OpenPopup needs to be called from the outside of the stack
        if (openRenameDialog) ImGui::OpenPopup("Rename Group");
    }

    result = nameDialog("Rename Group");

    if (result.has_value())
    {
        for (Uniform& uniform : uniformList)
        {
            if (uniform.group == groups[currentlyRenamedGroup]) uniform.group = result.value();
        }

        groups[currentlyRenamedGroup] = result.value();
        currentlyRenamedGroup = -1;
    }

    ImGui::EndTabBar();

    ImGui::EndChild();

    return didAnythingChange;
}
