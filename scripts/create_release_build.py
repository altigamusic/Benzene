import os
import subprocess
from dataclasses import dataclass
import json


@dataclass
class Keyframe:
    time: int
    values: list[float]
    interpolation: int
    tension: float


@dataclass
class Uniform:
    name: str
    type: str
    keyframes: list[Keyframe]


@dataclass
class Const:
    name: str
    type: str
    value: str


SHOULD_INJECT_CONSTS = True  # TODO: Check if this is efficient
UNIFORM_TYPE_TO_GL_FUNCTION = {
    "float": "glUniform1f",
    "vec2": "glUniform2f",
    "vec3": "glUniform3f",
    "color": "glUniform3f",
    "vec4": "glUniform4f",
}


def getCorrectedTension(tension, interpolation):
    # Interpolation 0 = linear, 1 = step, 2 = tonemap, 3 = gain
    # Copied from convert01ToCorrectFactor function in uniform.cpp
    if interpolation == 2:  # Tonemap
        return (tension - 0.5) * 20
    elif interpolation == 3:  # Gain
        return tension * 19 + 1
    else:
        # No factor needed for linear or step
        return 0


def parse_keyframe(kf: dict):
    interpolation = int(kf["interpolation"])
    value = kf["value"] if isinstance(kf["value"], list) else [kf["value"]]

    return Keyframe(int(kf["time"]), value, interpolation, getCorrectedTension(kf["tension"], interpolation))


def parse_config_file(filename):
    with open(filename, "r") as f:
        config = json.load(f)

    uniforms = [
        Uniform(uniform["name"], uniform["type"], [parse_keyframe(kf) for kf in uniform["keyframes"]])
        for uniform in config["uniforms"]
    ]

    bpm = config["bpm"]

    return uniforms, bpm


def keyframe_to_array_string(keyframe: Keyframe, idx: int):
    return f"{{{keyframe.time}, {keyframe.values[idx]:.6f}f, {keyframe.interpolation}, {keyframe.tension}}}"


def number_of_params(uniform: Uniform):
    if uniform.type == "float":
        return 1
    elif uniform.type == "vec2":
        return 2
    elif uniform.type in ("vec3", "color"):
        return 3
    elif uniform.type == "vec4":
        return 4
    else:
        raise ValueError(f"Unsupported uniform type: {uniform.type}")


def gen_keyframe_arrays(uniforms: list[Uniform]):
    arrays = []
    index = 0

    for uniform in uniforms:
        for i in range(number_of_params(uniform)):
            kf_array = ", ".join(keyframe_to_array_string(kf, i) for kf in uniform.keyframes)

            if len(kf_array) > 0:
                arrays.append(f"Keyframe keyframes{index}[] = {{{kf_array}}};")

            index += 1

    return arrays


def keyframe_values_to_const_value(type: str, values: list[float]):
    if type == "float":
        return f"{values[0]:.6f}"
    elif type == "vec2":
        return f"vec2({values[0]:.6f},{values[1]:.6f})"
    elif type in ("vec3", "color"):
        return f"vec3({values[0]:.6f}, {values[1]:.6f}, {values[2]:.6f})"
    elif type == "vec4":
        return f"vec4({values[0]:.6f}, {values[1]:.6f}, {values[2]:.6f}, {values[3]:.6f})"
    else:
        return "0.0"


def uniform_to_const(uniform: Uniform):
    try:
        keyframe_values = uniform.keyframes[0].values
    except IndexError:
        keyframe_values = [0] * number_of_params(uniform)

    return Const(uniform.name, uniform.type, keyframe_values_to_const_value(uniform.type, keyframe_values))


def split_animated_and_const_uniforms(uniforms: list[Uniform]):
    animated_uniforms = [uniform for uniform in uniforms if len(uniform.keyframes) > 1]
    consts = [uniform for uniform in uniforms if len(uniform.keyframes) <= 1]

    return animated_uniforms, list(map(uniform_to_const, consts))


def generate_release_file_code(uniforms):
    # Generate all keyframe array declarations
    keyframe_arrays = "\n".join(gen_keyframe_arrays(uniforms))

    # Generate uniform location assignments
    uniform_locations = "\n".join(
        f"    UNIFORMS[{i}] = glGetUniformLocation(program, VAR_{uniform.name});" for i, uniform in enumerate(uniforms)
    )

    kf_index = 0
    updates = []

    for i, uniform in enumerate(uniforms):
        gl_func = UNIFORM_TYPE_TO_GL_FUNCTION.get(uniform.type)

        if not gl_func:
            raise ValueError(f"Unsupported uniform type: {uniform.type}")

        number_of_keyframes = len(uniform.keyframes)
        if number_of_keyframes == 0:
            params = ["0.0f"] * number_of_params(uniform)
        else:
            params = [
                f"valueAtTime(time, keyframes{kf_index + j}, {number_of_keyframes})" for j in range(number_of_params(uniform))]
        param_string = ",".join(params)
        kf_index += len(params)

        updates.append(f"    {gl_func}(UNIFORMS[{i}], {param_string});")

        uniform_definition_line = f"GLuint UNIFORMS[{len(uniforms)}] = {{0}};"

    uniform_updates = "\n".join(updates)

    return f"""#include "../release.h"

{uniform_definition_line if len(uniforms) > 0 else ""}
GLuint timeUniformLocation = 0;

{keyframe_arrays}

void locateUniforms(GLuint program) {{
    timeUniformLocation = glGetUniformLocation(program, VAR__t);
{uniform_locations}
}}

void updateUniforms(float time) {{
    glUniform1f(timeUniformLocation, time);
{uniform_updates}
}}
"""


def generate_release_file(uniforms, output_filename):
    release_file_code = generate_release_file_code(uniforms)

    with open(output_filename, "w") as f:
        f.write(release_file_code)


def generate_release_header(output_filename, bpm):
    with open(output_filename, "w") as f:
        f.write(f"""#pragma once

#define BPM {bpm:.2f}f
""")


def generate_minified_shader(shader_filename, uniforms: list[Uniform], consts: list[Const], output_filename, resolution=(800, 600)):
    with open(shader_filename, "r") as f:
        shader_code = f.read()

    uniform_definitions = [f"uniform {uniform.type} {uniform.name};" for uniform in uniforms]
    uniform_definition_code = "\n".join(uniform_definitions)

    const_definitions = [f"const {const.type} {const.name} = {const.value};" for const in consts]
    const_definition_code = "\n".join(const_definitions)

    injected_code = f"""#version 330
uniform float _t;
{uniform_definition_code}
{const_definition_code}
const vec2 _res = vec2({resolution[0]}, {resolution[1]});
vec2 fragCoord = gl_FragCoord.xy;
{shader_code}
"""

    with open("__temp_shader.glsl", "w") as f:
        f.write(injected_code)

    subprocess.call(["shader_minifier.exe", "--aggressive-inlining", "-v", "-o", output_filename, "__temp_shader.glsl"])
    os.remove("__temp_shader.glsl")

    # Replace the file name with "fragmentShaderSource" (you can't do that using the shader minifier directly)
    # Also it has to be inlined, because it's imported in two different files
    with open(output_filename, "r") as f:
        minified_code = f.read()
    minified_code = minified_code.replace("const char *__temp_shader_glsl", "const inline char *fragmentShaderSource")
    with open(output_filename, "w") as f:
        f.write(minified_code)


def main():
    print(os.listdir("."))
    config_filename = "config.json"
    output_filename = "src/generated/release.cpp"
    output_header_filename = "src/generated/release_config.h"
    shader_source_filename = "shaders/FragmentShader.glsl"
    shader_output_filename = "src/generated/shader.inl"

    uniforms, bpm = parse_config_file(config_filename)

    if SHOULD_INJECT_CONSTS:
        animated_uniforms, consts = split_animated_and_const_uniforms(uniforms)
    else:
        animated_uniforms, consts = uniforms, []

    generate_release_file(animated_uniforms, output_filename)
    print(f"Generated {output_filename}")

    generate_release_header(output_header_filename, bpm)

    generate_minified_shader(shader_source_filename, animated_uniforms, consts, shader_output_filename)
    print(f"Generated {shader_output_filename}")


if __name__ == "__main__":
    main()
