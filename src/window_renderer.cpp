#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include "window_renderer.h"
#include "CameraKeyframeController.h"
#include "intro.h"
#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

std::string WindowRenderer::generateUniformCode(const std::vector<Uniform>& uniformList)
{
    std::stringstream uniformStream;
    for (const Uniform& uniform : uniformList)
    {
        switch (uniform.type)
        {
        case UniformType::Float:
            uniformStream << "uniform float " << uniform.name << ";\n";
            break;
        case UniformType::Int:
            uniformStream << "uniform int " << uniform.name << ";\n";
            break;
        case UniformType::Bool:
            uniformStream << "uniform bool " << uniform.name << ";\n";
            break;
        case UniformType::Vec2:
            uniformStream << "uniform vec2 " << uniform.name << ";\n";
            break;
        case UniformType::Vec3:
        case UniformType::Color:
            uniformStream << "uniform vec3 " << uniform.name << ";\n";
            break;
        case UniformType::Vec4:
            uniformStream << "uniform vec4 " << uniform.name << ";\n";
            break;
        }
    }
    return uniformStream.str();
}

std::vector<std::string> WindowRenderer::getUndeclaredIdentifiers(const std::string& error)
{
    std::regex undeclaredIdentifierRegex(
        "Undeclared identifier:\\s*(\\w+)\\s*$|['\"](\\w+)['\"]\\s*:\\s*undeclared "
        "identifier|[uU]ndefined\\s+[Vv]ariable\\s+['\"](\\w+)['\"]",
        std::regex::icase);

    std::vector<std::string> result;
    std::smatch match;

    std::string::const_iterator searchStart(error.cbegin());
    while (std::regex_search(searchStart, error.cend(), match, undeclaredIdentifierRegex))
    {
        if (match.size() > 1)
        {
            // Only one group will match something and the other(s) will be empty
            for (size_t i = 1; i < match.size(); ++i)
            {
                if (match[i].matched && std::find(result.begin(), result.end(), match[i].str()) == result.end())
                {
                    result.push_back(match[i].str());
                    break;
                }
            }
        }
        searchStart = match.suffix().first;
    }

    return result;
}

void WindowRenderer::resize(int width, int height)
{
    windowWidth = width;
    windowHeight = height;
    sidebarWidth = windowWidth - viewportWidth;
    sidebarHeight = viewportHeight;
    timelineWidth = windowWidth;
    timelineHeight = windowHeight - viewportHeight;

    glViewport(sidebarWidth, timelineHeight, viewportWidth, viewportHeight);
}

void WindowRenderer::loadInternal(
    const std::string& fragmentShaderSource, std::vector<Uniform>& uniformList, const std::string& currentGroup, bool didTryInjecting)
{
    if (program != 0)
    {
        glDeleteProgram(program);
        program = 0;
    }

    std::string source =
        "#version 330\n"
        "uniform vec2 _res, _windowOffset;\n"
        "uniform float _t;\n"
        "uniform vec3 _cp, _ct, _cr;\n"
        "vec2 fragCoord = gl_FragCoord.xy - _windowOffset;\n" +
        generateUniformCode(uniformList) + fragmentShaderSource;

    const char* srcPtr = source.c_str();
    program = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &srcPtr);

    int length;
    char infoLog[500];
    glGetProgramInfoLog(program, 500, &length, infoLog);
    std::string error(infoLog, length);

    if (length > 0)
    {
        if (!didTryInjecting)
        {
            auto undeclaredIdentifiers = getUndeclaredIdentifiers(error);

            // Inject all undeclared identifiers as uniforms and recompile
            for (const std::string& name : undeclaredIdentifiers)
            {
                Uniform new_uniform{name, UniformType::Float};
                new_uniform.group = currentGroup;
                uniformList.push_back(new_uniform);
            }
            return loadInternal(fragmentShaderSource, uniformList, currentGroup, true);
        }

        lastError = error;
        return;
    }

    lastError.clear();

    timeLocation = glGetUniformLocation(program, "_t");
    resolutionLocation = glGetUniformLocation(program, "_res");
    windowOffsetLocation = glGetUniformLocation(program, "_windowOffset");
    cameraPositionLocation = glGetUniformLocation(program, "_cp");
    cameraTargetLocation = glGetUniformLocation(program, "_ct");
    cameraRotationLocation = glGetUniformLocation(program, "_cr");

    glViewport(sidebarWidth, timelineHeight, viewportWidth, viewportHeight);

    for (Uniform& uniform : uniformList)
        uniform.location = glGetUniformLocation(program, uniform.name.c_str());

    glUseProgram(program);
    initIntro(program);
}

void WindowRenderer::load(const std::string& source, std::vector<Uniform>& uniformList, const std::string& currentGroup)
{
    loadInternal(source, uniformList, currentGroup, false);
}

void WindowRenderer::reload(std::vector<Uniform>& uniformList, const std::string& currentGroup)
{
    loadInternal(currentShader, uniformList, currentGroup, false);
}

bool WindowRenderer::reloadFromFile(std::vector<Uniform>& uniformList, const std::string& currentGroup)
{
    const char* fragmentShaderPath = "shaders/FragmentShader.glsl";

    std::ifstream fragmentShaderFile(fragmentShaderPath);
    std::stringstream stringStream;
    stringStream << fragmentShaderFile.rdbuf();

    std::string s = stringStream.str();
    bool didChange = s != currentShader;

    if (didChange && !s.empty()) load(s, uniformList, currentGroup);

    currentShader = std::move(s);
    return didChange;
}

void WindowRenderer::updateUniforms(
    float time, std::vector<Uniform>& uniformList, CameraKeyframeController& camera, bool isEndKeyframe, int quantDigits)
{
    glUniform1f(timeLocation, time);
    glUniform2f(resolutionLocation, (float)viewportWidth, (float)viewportHeight);
    glUniform2f(windowOffsetLocation, (float)sidebarWidth, (float)timelineHeight);

    vec3 cp = camera.getPosition();
    vec3 ct = camera.getTarget();
    vec3 cr = camera.getRotation();

    glUniform3f(cameraPositionLocation, cp.x, cp.y, cp.z);
    glUniform3f(cameraTargetLocation, ct.x, ct.y, ct.z);
    glUniform3f(cameraRotationLocation, cr.x, cr.y, cr.z);

    for (Uniform& uniform : uniformList)
    {
        auto value = uniform.valueAtTime(time, isEndKeyframe, quantDigits);

        switch (uniform.type)
        {
        case UniformType::Float:
            glUniform1f(uniform.location, value.f);
            break;
        case UniformType::Int:
            glUniform1i(uniform.location, value.i);
            break;
        case UniformType::Bool:
            glUniform1i(uniform.location, value.b ? 1 : 0);
            break;
        case UniformType::Vec2:
            glUniform2f(uniform.location, value.v2[0], value.v2[1]);
            break;
        case UniformType::Vec3:
        case UniformType::Color:
            glUniform3f(uniform.location, value.v3[0], value.v3[1], value.v3[2]);
            break;
        case UniformType::Vec4:
            glUniform4f(uniform.location, value.v4[0], value.v4[1], value.v4[2], value.v4[3]);
            break;
        }
    }
}
