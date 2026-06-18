#pragma once
#include "CameraKeyframeController.h"
#include "InputState.h"
#include "uniform.h"
#include <string>
#include <vector>

bool scrubToPreviousKeyframe(
    float& t, bool& isEndKeyframe, const std::vector<Uniform>& uniformList, const CameraKeyframeController& cameraController);

bool scrubToNextKeyframe(
    float& t, bool& isEndKeyframe, const std::vector<Uniform>& uniformList, const CameraKeyframeController& cameraController);

bool handleKeyScrubbing(const KeyboardState& keyboard, float& t, bool& isEndKeyframe, int maxTimelineTime,
    const std::vector<Uniform>& uniformList, const CameraKeyframeController& cameraController);

/// <summary>
/// Renders timelines for all uniforms.
/// Also handles user input for moving keyframes, zooming/panning the timeline, and scrubbing.
/// Returns true if the time has changed from user input.
/// </summary>
/// <param name="time">The current time. Modified if the user scrubs or moves a keyframe.</param>
/// <param name="minTime">The start point to draw the timelines from. Modified if the user zooms or pans.</param>
/// <param name="maxTime">The end point to draw the timelines to. Modified if the user zooms or pans.</param>
/// <param name="loopStart">The start of the loop, nullptr if there's no loop. Modified if the user moves the loop points.</param>
/// <param name="loopEnd">The end of the loop, nullptr if there's no loop. Modified if the user moves the loop points.</param>
/// <returns>True if the time has changed from user input, false if nothing was altered.</returns>
bool renderTimelines(float* time, float& minTime, float& maxTime, bool& isEndKeyframe, std::vector<Uniform>& uniformList,
    CameraKeyframeController& cameraController, const std::string& currentGroup, int demoTimeLength, float* loopStart = nullptr,
    float* loopEnd = nullptr);
