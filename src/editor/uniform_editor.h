#pragma once
#include "EditorState.h"
#include "ActionHistory.h"
#include <optional>
#include <string>

std::optional<std::string> nameDialog(const char* str_id);

bool renderSingleUniformTab(const std::string& group, float time, bool isEndKeyframe, bool& shouldKeepPlaying,
    bool& outShouldReloadFragmentShader, EditorState& editorState, ActionHistory& actionHistory);

/// <summary>
/// Render the uniform editor menu, and update all uniforms according to user input.
/// </summary>
/// <param name="time">The current time.</param>
/// <param name="isEndKeyframe">True if the current keyframe is a dual keyframe on its second half, false otherwise.</param>
/// <param name="shouldKeepPlaying">If playback should be paused, this bool will be set to false.</param>
/// <param name="outShouldReloadFragmentShader">
/// This bool will be set to true if the fragment shader should be reloaded and false otherwise.
/// </param>
/// <param name="editorState">The editor state. Uniforms and groups may be modified.</param>
/// <param name="actionHistory">The action history. Recorded actions will be pushed here.</param>
/// <param name="menuHeight">The height of the menu.</param>
/// <returns>True if anything changed and the screen should rerender, false otherwise.</returns>
bool renderAndUpdateUniforms(float time, bool isEndKeyframe, bool& shouldKeepPlaying, bool& outShouldReloadFragmentShader,
    EditorState& editorState, ActionHistory& actionHistory, int menuHeight);
