#include "timeline.h"
#include "imgui/imgui.h"
#include "imgui/imgui_benzene_widgets.h"
#include <algorithm>
#include <cmath>

static std::vector<int> getAllKeyframes(const std::vector<Uniform>& uniformList, const CameraKeyframeController& cameraController)
{
    std::vector<int> keyframes;

    auto appendKeyframes = [&keyframes](const Uniform& uniform)
    {
        int previousTime = -1;
        for (const UniformKeyframe& keyframe : uniform.keyframes)
        {
            int time = static_cast<int>(keyframe.time);
            auto f = std::find(keyframes.begin(), keyframes.end(), time);
            if (f == keyframes.end())
            {
                keyframes.push_back(time);
            }
            // If this is a dual keyframe, make sure the array also has a dual keyframe
            else if (previousTime == time && (f + 1 == keyframes.end() || *(f + 1) != time))
            {
                keyframes.insert(f + 1, time);
            }

            previousTime = time;
        }
    };

    for (const Uniform& uniform : uniformList)
        appendKeyframes(uniform);

    appendKeyframes(cameraController.positionUniform);
    appendKeyframes(cameraController.rotationUniform);

    std::sort(keyframes.begin(), keyframes.end());

    return keyframes;
}

static float getSnappedKeyframePosition(KeyframeMovementData& kfMovement, float maxTime, const std::vector<float>& keyframes)
{
    float minValue = kfMovement.index == 0 ? 0 : keyframes[kfMovement.index - 1];
    float maxValue = kfMovement.index == (int)keyframes.size() - 1 ? maxTime : keyframes[kfMovement.index + 1];

    // If the keyframe on either side is already dual, prevent merging another keyframe into it by restricting the bounds
    bool isDualKeyframeBefore = kfMovement.index > 1 && keyframes[kfMovement.index - 1] == keyframes[kfMovement.index - 2];
    bool isDualKeyframeAfter =
        kfMovement.index < (int)keyframes.size() - 2 && keyframes[kfMovement.index + 1] == keyframes[kfMovement.index + 2];

    if (isDualKeyframeBefore) minValue += 1;
    if (isDualKeyframeAfter) maxValue -= 1;

    return std::round(std::clamp(kfMovement.newTime, minValue, maxValue));
}

static bool hasDualKeyframe(int t, const std::vector<int>& keyframes)
{
    auto it = std::find(keyframes.begin(), keyframes.end(), t);
    return it != keyframes.end() && it + 1 != keyframes.end() && *(it + 1) == t;
}

static int findPreviousKeyframe(int t, const std::vector<int>& keyframes, bool& isEnd)
{
    // Check dual keyframes
    if (isEnd && hasDualKeyframe(t, keyframes))
    {
        // We're on the end half of a dual keyframe - move to the start half
        isEnd = false;
        return t;
    }

    auto it = std::lower_bound(keyframes.begin(), keyframes.end(), t);

    if (it != keyframes.begin())
    {
        --it;
        isEnd = true; // When moving back we always move to the end
        return *it;
    }

    return -1; // No previous keyframe
}

static int findNextKeyframe(int t, const std::vector<int>& keyframes, bool& isEnd)
{
    if (!isEnd && hasDualKeyframe(t, keyframes))
    {
        // We're on the start half of a dual keyframe - move to the end half
        isEnd = true;
        return t;
    }

    auto it = std::upper_bound(keyframes.begin(), keyframes.end(), t);

    if (it != keyframes.end())
    {
        isEnd = false; // When moving forward we always move to the start
        return *it;
    }

    return -1; // No next keyframe
}

bool scrubToPreviousKeyframe(
    float& timeInBeats, bool& isEndKeyframe, const std::vector<Uniform>& uniformList, const CameraKeyframeController& cameraController)
{
    std::vector<int> keyframes = getAllKeyframes(uniformList, cameraController);
    int prevKeyframe = findPreviousKeyframe(static_cast<int>(timeInBeats), keyframes, isEndKeyframe);
    if (prevKeyframe == -1) return false;
    timeInBeats = static_cast<float>(prevKeyframe);
    return true;
}

bool scrubToNextKeyframe(
    float& timeInBeats, bool& isEndKeyframe, const std::vector<Uniform>& uniformList, const CameraKeyframeController& cameraController)
{
    std::vector<int> keyframes = getAllKeyframes(uniformList, cameraController);
    int nextKeyframe = findNextKeyframe(static_cast<int>(timeInBeats), keyframes, isEndKeyframe);
    if (nextKeyframe == -1) return false;
    timeInBeats = static_cast<float>(nextKeyframe);
    return true;
}

bool handleKeyScrubbing(const KeyboardState& keyboard, float& timeInBeats, bool& isEndKeyframe, int maxTimelineTime,
    const std::vector<Uniform>& uniformList, const CameraKeyframeController& cameraController)
{
    std::vector<int> keyframes = getAllKeyframes(uniformList, cameraController);

    if (keyboard.wasKeyPressed(VK_LEFT))
    {
        if (keyboard.isDown(VK_CONTROL)) return scrubToPreviousKeyframe(timeInBeats, isEndKeyframe, uniformList, cameraController);

        if (isEndKeyframe && hasDualKeyframe(static_cast<int>(timeInBeats), keyframes))
        {
            // If we're on the end half of a dual keyframe, move to the start half without changing the time
            isEndKeyframe = false;
        }
        else
        {
            timeInBeats = max(0, (int)std::ceil(timeInBeats) - 1);
            isEndKeyframe = true; // When moving back we always move to the end
        }

        return true;
    }

    if (keyboard.wasKeyPressed(VK_RIGHT))
    {
        if (keyboard.isDown(VK_CONTROL)) return scrubToNextKeyframe(timeInBeats, isEndKeyframe, uniformList, cameraController);

        if (!isEndKeyframe && hasDualKeyframe(static_cast<int>(timeInBeats), keyframes))
        {
            // If we're on the start half of a dual keyframe, move to the end half without changing the time
            isEndKeyframe = true;
        }
        else
        {
            timeInBeats = min(maxTimelineTime, (int)std::floor(timeInBeats) + 1);
            isEndKeyframe = false; // When moving forward we always move to the start
        }

        return true;
    }

    return false;
}

bool renderTimelines(float* timeInBeats, float& minTime, float& maxTime, bool& isEndKeyframe, std::vector<Uniform>& uniformList,
    CameraKeyframeController& cameraController, const std::string& currentGroup, int demoTimeLength, float* loopStart, float* loopEnd)
{
    bool didChange = false;

    ZoomPanSlider("Zoom", &minTime, &maxTime, 0.0f, (float)demoTimeLength);
    didChange |= TimeSlider("Time", timeInBeats, &isEndKeyframe, minTime, maxTime, loopStart, loopEnd);

    if (cameraController.positionUniform.keyframes.size() > 1)
    {
        // Camera slider is special because it's controlled differently
        // Camera position and camera target have the same keyframes
        std::vector<float> keyframes;

        for (const UniformKeyframe& keyframe : cameraController.positionUniform.keyframes)
            keyframes.push_back(keyframe.time);

        KeyframeMovementData kfMovement;
        if (KeyframeSlider("Camera", timeInBeats, &isEndKeyframe, minTime, maxTime, keyframes, &kfMovement))
        {
            if (kfMovement.index >= 0)
            {
                float newTime = getSnappedKeyframePosition(kfMovement, maxTime, keyframes);

                cameraController.positionUniform.keyframes[kfMovement.index].time = newTime;
                cameraController.rotationUniform.keyframes[kfMovement.index].time = newTime;
            }

            didChange = true;
        }
    }

    for (Uniform& uniform : uniformList)
    {
        // Display timelines only for animated uniforms, i.e. uniforms with 2+ keyframes
        if (uniform.keyframes.size() <= 1 || uniform.group != currentGroup) continue;

        std::vector<float> keyframes;

        for (const UniformKeyframe& keyframe : uniform.keyframes)
            keyframes.push_back(keyframe.time);

        KeyframeMovementData kfMovement;

        if (KeyframeSlider(uniform.name.c_str(), timeInBeats, &isEndKeyframe, minTime, maxTime, keyframes, &kfMovement))
        {
            if (kfMovement.index >= 0)
                uniform.keyframes[kfMovement.index].time = getSnappedKeyframePosition(kfMovement, maxTime, keyframes);

            didChange = true;
        }
    }

    return didChange;
}
