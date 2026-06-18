#pragma once
#include "../Action.h"
#include "../uniform.h"
#include <string>
#include <vector>

class DeleteTime : public Action
{
    float startTime;
    float length;

    struct DeletedKeyframes
    {
        std::string uniformName;
        enum class Source
        {
            Regular,
            CameraPosition,
            CameraRotation
        } source;
        std::vector<UniformKeyframe> keyframes;
    };
    std::vector<DeletedKeyframes> deletedPerUniform;

  public:
    DeleteTime(float startTime, float length);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
