#include "timeline.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_benzene_widgets.h"
#include "ActionHistory.h"
#include "actions/MoveKeyframe.h"
#include "actions/MoveCameraKeyframe.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

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

namespace
{
struct ActiveCameraKeyframeMove
{
    UniformKeyframe positionKeyframe;
    UniformKeyframe rotationKeyframe;
    float newTime;
    bool isBeforeEnd;
    bool isAfterEnd;
};

struct ActiveUniformKeyframeMove
{
    std::string uniformName;
    UniformKeyframe keyframe;
    float newTime;
    bool isBeforeEnd;
    bool isAfterEnd;
};

// A keyframe at this index is the "end" half of a dual keyframe if the previous entry shares its time.
bool isEndHalfOfDual(int index, const std::vector<float>& keyframeTimes)
{
    return index > 0 && keyframeTimes[index - 1] == keyframeTimes[index];
}

// Determines whether the dragged keyframe (originally at keyframeTimes[index]) lands as the "end" half of a dual
// keyframe once moved to newTime - true if it merges onto the previous keyframe, false if it merges onto the next
// keyframe or doesn't merge with anything at all.
bool isEndHalfOfDualAfterMove(int index, float newTime, const std::vector<float>& keyframeTimes)
{
    if (index > 0 && keyframeTimes[index - 1] == newTime) return true;
    if (index < (int)keyframeTimes.size() - 1 && keyframeTimes[index + 1] == newTime) return false;
    return false;
}
} // namespace

bool renderTimelines(float& timeInBeats, float& minTime, float& maxTime, bool& isEndKeyframe, std::vector<Uniform>& uniformList,
    CameraKeyframeController& cameraController, const std::string& currentGroup, float demoTimeLength, ActionHistory& actionHistory,
    float* loopStart, float* loopEnd)
{
    bool didChange = false;

    static std::optional<ActiveCameraKeyframeMove> activeCameraKeyframeMove;
    static std::optional<ActiveUniformKeyframeMove> activeUniformKeyframeMove;

    ZoomPanSlider("Zoom", &minTime, &maxTime, 0.0f, demoTimeLength);
    didChange |= TimeSlider("Time", &timeInBeats, &isEndKeyframe, minTime, maxTime, loopStart, loopEnd);

    if (cameraController.positionUniform.keyframes.size() > 1)
    {
        // Camera slider is special because it's controlled differently
        // Camera position and camera target have the same keyframes
        std::vector<float> keyframes;

        for (const UniformKeyframe& keyframe : cameraController.positionUniform.keyframes)
            keyframes.push_back(keyframe.time);

        KeyframeMovementData kfMovement;
        bool sliderChanged =
            KeyframeSlider("Camera", &timeInBeats, &isEndKeyframe, minTime, maxTime, keyframes, &kfMovement, loopStart, loopEnd);

        if (kfMovement.index >= 0)
        {
            if (!activeCameraKeyframeMove.has_value())
            {
                const UniformKeyframe& posKf = cameraController.positionUniform.keyframes[kfMovement.index];

                bool isBeforeEnd = isEndHalfOfDual(kfMovement.index, keyframes);

                UniformKeyframe rotKf;
                if (kfMovement.index < (int)cameraController.rotationUniform.keyframes.size())
                {
                    rotKf = cameraController.rotationUniform.keyframes[kfMovement.index];
                }
                else
                {
                    rotKf.time = posKf.time;
                    rotKf.value = cameraController.rotationUniform.valueAtTime(posKf.time, isBeforeEnd);
                    rotKf.interpolation = KeyframeInterpolation::Linear;
                    rotKf.interpolationFactor = 0.0f;
                }

                activeCameraKeyframeMove = ActiveCameraKeyframeMove{posKf, rotKf, posKf.time, isBeforeEnd, isBeforeEnd};
            }

            if (sliderChanged)
            {
                float newTime = getSnappedKeyframePosition(kfMovement, maxTime, keyframes);

                cameraController.positionUniform.keyframes[kfMovement.index].time = newTime;
                cameraController.rotationUniform.keyframes[kfMovement.index].time = newTime;
                activeCameraKeyframeMove->newTime = newTime;
                activeCameraKeyframeMove->isAfterEnd = isEndHalfOfDualAfterMove(kfMovement.index, newTime, keyframes);

                didChange = true;
            }
        }
        else if (activeCameraKeyframeMove.has_value())
        {
            if (activeCameraKeyframeMove->newTime != activeCameraKeyframeMove->positionKeyframe.time)
            {
                actionHistory.record(std::make_unique<MoveCameraKeyframe>(activeCameraKeyframeMove->positionKeyframe,
                    activeCameraKeyframeMove->rotationKeyframe, activeCameraKeyframeMove->newTime, activeCameraKeyframeMove->isBeforeEnd,
                    activeCameraKeyframeMove->isAfterEnd));
            }
            activeCameraKeyframeMove.reset();
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

        bool sliderChanged = KeyframeSlider(
            uniform.name.c_str(), &timeInBeats, &isEndKeyframe, minTime, maxTime, keyframes, &kfMovement, loopStart, loopEnd);

        if (kfMovement.index >= 0)
        {
            if (!activeUniformKeyframeMove.has_value())
            {
                const UniformKeyframe& kf = uniform.keyframes[kfMovement.index];
                bool isBeforeEnd = isEndHalfOfDual(kfMovement.index, keyframes);
                activeUniformKeyframeMove = ActiveUniformKeyframeMove{uniform.name, kf, kf.time, isBeforeEnd, isBeforeEnd};
            }

            if (sliderChanged)
            {
                float newTime = getSnappedKeyframePosition(kfMovement, maxTime, keyframes);
                uniform.keyframes[kfMovement.index].time = newTime;
                activeUniformKeyframeMove->newTime = newTime;
                activeUniformKeyframeMove->isAfterEnd = isEndHalfOfDualAfterMove(kfMovement.index, newTime, keyframes);
                didChange = true;
            }
        }
        else if (activeUniformKeyframeMove.has_value() && activeUniformKeyframeMove->uniformName == uniform.name)
        {
            if (activeUniformKeyframeMove->newTime != activeUniformKeyframeMove->keyframe.time)
            {
                actionHistory.record(std::make_unique<MoveKeyframe>(uniform.name, activeUniformKeyframeMove->keyframe,
                    activeUniformKeyframeMove->newTime, activeUniformKeyframeMove->isBeforeEnd, activeUniformKeyframeMove->isAfterEnd));
            }
            activeUniformKeyframeMove.reset();
        }
    }

    return didChange;
}
