#include "EditorState.h"
#include <algorithm>

Uniform* EditorState::findUniform(const std::string& name)
{
    auto it = std::find_if(config.uniformList.begin(), config.uniformList.end(), [&](const Uniform& u) { return u.name == name; });
    return it != config.uniformList.end() ? &*it : nullptr;
}

BenzeneConfig EditorState::toConfig() const
{
    BenzeneConfig out = config;
    out.cameraPosition = cameraController.positionUniform;
    out.cameraRotation = cameraController.rotationUniform;
    return out;
}

void EditorState::fromConfig(BenzeneConfig cfg)
{
    if (cfg.cameraPosition.has_value()) cameraController.positionUniform = cfg.cameraPosition.value();
    if (cfg.cameraRotation.has_value()) cameraController.rotationUniform = cfg.cameraRotation.value();
    cfg.cameraPosition.reset();
    cfg.cameraRotation.reset();
    config = std::move(cfg);
}
