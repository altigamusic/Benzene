#include "DeleteTime.h"
#include "../EditorState.h"
#include <algorithm>

DeleteTime::DeleteTime(float startTime, float length) : startTime(startTime), length(length) {}

void DeleteTime::invoke(EditorState& state)
{
    deletedPerUniform.clear();
    float endTime = startTime + length;

    auto processUniform = [&](Uniform& u, const std::string& name, DeletedKeyframes::Source source)
    {
        std::vector<UniformKeyframe> deleted;
        auto it = std::remove_if(u.keyframes.begin(), u.keyframes.end(),
            [&](const UniformKeyframe& kf)
            {
                if (kf.time >= startTime && kf.time < endTime)
                {
                    deleted.push_back(kf);
                    return true;
                }
                return false;
            });
        u.keyframes.erase(it, u.keyframes.end());

        for (UniformKeyframe& kf : u.keyframes)
            if (kf.time >= endTime) kf.time -= length;

        if (!deleted.empty()) deletedPerUniform.push_back({name, source, std::move(deleted)});
    };

    for (Uniform& u : state.config.uniformList)
        processUniform(u, u.name, DeletedKeyframes::Source::Regular);
    processUniform(state.cameraController.positionUniform, "", DeletedKeyframes::Source::CameraPosition);
    processUniform(state.cameraController.rotationUniform, "", DeletedKeyframes::Source::CameraRotation);

    state.cameraController.forceMovement();
}

void DeleteTime::undo(EditorState& state)
{
    state.forEachUniform(
        [&](Uniform& u)
        {
            for (UniformKeyframe& kf : u.keyframes)
                if (kf.time >= startTime) kf.time += length;
        });

    for (const DeletedKeyframes& entry : deletedPerUniform)
    {
        Uniform* u = nullptr;
        switch (entry.source)
        {
        case DeletedKeyframes::Source::Regular:
            u = state.findUniform(entry.uniformName);
            break;
        case DeletedKeyframes::Source::CameraPosition:
            u = &state.cameraController.positionUniform;
            break;
        case DeletedKeyframes::Source::CameraRotation:
            u = &state.cameraController.rotationUniform;
            break;
        }
        if (!u) continue;

        for (const UniformKeyframe& kf : entry.keyframes)
        {
            auto pos = std::lower_bound(u->keyframes.begin(), u->keyframes.end(), kf,
                [](const UniformKeyframe& a, const UniformKeyframe& b) { return a.time < b.time; });
            u->keyframes.insert(pos, kf);
        }
    }

    state.cameraController.forceMovement();
}

std::string DeleteTime::describe() const { return "Delete time"; }
