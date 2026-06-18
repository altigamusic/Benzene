#include "InsertTime.h"
#include "../EditorState.h"

InsertTime::InsertTime(float startTime, float length) : startTime(startTime), length(length) {}

void InsertTime::invoke(EditorState& state)
{
    state.forEachUniform(
        [&](Uniform& u)
        {
            for (UniformKeyframe& kf : u.keyframes)
                if (kf.time >= startTime) kf.time += length;
        });
    state.cameraController.forceMovement();
}

void InsertTime::undo(EditorState& state)
{
    state.forEachUniform(
        [&](Uniform& u)
        {
            for (UniformKeyframe& kf : u.keyframes)
                if (kf.time >= startTime + length) kf.time -= length;
        });
    state.cameraController.forceMovement();
}

std::string InsertTime::describe() const { return "Insert time"; }
