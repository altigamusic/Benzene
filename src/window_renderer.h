#pragma once
#include "ext.h"
#include "uniform.h"
#include <string>
#include <vector>

#include "debug_window.h"

class CameraKeyframeController;

/// <summary>
/// Responsible for managing all shader-related operations, as well as handling window resizing and dimensions.
/// </summary>
struct WindowRenderer
{
    WindowRenderer(DebugWindow& debugWindow) : debugWindow(debugWindow) {}

    int sidebarWidth = 0;
    int sidebarHeight = 0;
    int windowWidth = 1200;
    int windowHeight = 800;
    int viewportWidth = 800;
    int viewportHeight = 600;
    int timelineWidth = 0;
    int timelineHeight = 200;

    void resize(int width, int height);

    void load(const std::string& source, std::vector<Uniform>& uniformList, const std::string& currentGroup);
    void reload(std::vector<Uniform>& uniformList, const std::string& currentGroup);
    bool reloadFromFile(std::vector<Uniform>& uniformList, const std::string& currentGroup);

    void updateUniforms(
        float time, std::vector<Uniform>& uniformList, CameraKeyframeController& camera, bool isEndKeyframe, int quantDigits);

  private:
    GLuint program = 0;
    GLuint timeLocation = 0;
    GLuint resolutionLocation = (GLuint)-1;
    GLuint windowOffsetLocation = (GLuint)-1;
    GLuint cameraPositionLocation = (GLuint)-1;
    GLuint cameraTargetLocation = (GLuint)-1;
    GLuint cameraRotationLocation = (GLuint)-1;
    std::string currentShader;

    DebugWindow& debugWindow;

    std::string generateUniformCode(const std::vector<Uniform>& uniformList);
    std::vector<std::string> getUndeclaredIdentifiers(const std::string& error);
    void loadInternal(const std::string& source, std::vector<Uniform>& uniformList, const std::string& currentGroup, bool didTryInjecting);
};
