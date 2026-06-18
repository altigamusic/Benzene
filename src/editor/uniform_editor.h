#pragma once
#include "uniform.h"
#include <optional>
#include <string>
#include <vector>

std::optional<std::string> nameDialog(const char* str_id);

bool renderSingleUniformTab(const std::string& group, float time, bool isEndKeyframe, bool& shouldKeepPlaying,
    bool& outShouldReloadFragmentShader, std::vector<Uniform>& uniformList, const std::vector<std::string>& groups);

/// <summary>
/// Render the uniform editor menu, and update all uniforms according to user input.
/// </summary>
/// <param name="time">The current time.</param>
/// <param name="isEndKeyframe">True if the current keyframe is a dual keyframe on its second half, false otherwise.</param>
/// <param name="shouldKeepPlaying">If playback should be paused, this bool will be set to false.</param>
/// <param name="outShouldReloadFragmentShader">
/// This bool will be set to true if the fragment shader should be reloaded and false otherwise.
/// </param>
/// <param name="uniformList">The list of uniforms. If the user updates a uniform, it will be modified in this list.</param>
/// <param name="groups">The list of groups. If the user modifies groups, this list will be modified.</param>
/// <param name="currentGroup">The current group. If the user switches tabs, this parameter will be modified.</param>
/// <param name="menuHeight">The height of the menu.</param>
/// <returns>True if anything changed and the screen should rerender, false otherwise.</returns>
bool renderAndUpdateUniforms(float time, bool isEndKeyframe, bool& shouldKeepPlaying, bool& outShouldReloadFragmentShader,
    std::vector<Uniform>& uniformList, std::vector<std::string>& groups, std::string& currentGroup, int menuHeight);
