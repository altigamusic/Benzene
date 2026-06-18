#pragma once
#include "config.h"
#include "CameraKeyframeController.h"
#include <vector>
#include <string>

struct EditorState
{
    BenzeneConfig config;
    std::vector<std::string> groups;
    std::string currentGroup;
    CameraKeyframeController cameraController;

    BenzeneConfig toConfig() const;
    void fromConfig(BenzeneConfig config);
    Uniform* findUniform(const std::string& name);

    template <typename F> void forEachUniform(F&& fn)
    {
        for (Uniform& u : config.uniformList)
            fn(u);
        fn(cameraController.positionUniform);
        fn(cameraController.rotationUniform);
    }
};
