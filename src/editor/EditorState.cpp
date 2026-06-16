#include "EditorState.h"

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
