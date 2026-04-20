import itertools
import os
import subprocess
from dataclasses import dataclass
from enum import IntEnum
import json


class InterpolationType(IntEnum):
    LINEAR = 0
    STEP = 1
    TONEMAP = 2
    GAIN = 3


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
    quantization: int | None


@dataclass
class Const:
    name: str
    type: str
    value: str


# Flags that can affect crinkled size either way
SHOULD_INJECT_CONSTS = True
SHOULD_COMBINE_UNIFORM_DEFINITIONS = True


UNIFORM_TYPE_TO_GL_FUNCTION = {
    "float": "glUniform1f",
    "vec2": "glUniform2f",
    "vec3": "glUniform3f",
    "color": "glUniform3f",
    "vec4": "glUniform4f",
}

UNIFORM_TYPE_TO_OPENGL_TYPE = {
    "float": "float",
    "vec2": "vec2",
    "vec3": "vec3",
    "color": "vec3",
    "vec4": "vec4",
}


def get_corrected_tension(tension, interpolation):
    # Copied from convert01ToCorrectFactor function in uniform.cpp
    if interpolation == InterpolationType.TONEMAP:
        return (tension - 0.5) * 20
    elif interpolation == InterpolationType.GAIN:
        return tension * 19 + 1
    else:
        # No factor needed for linear or step
        return 0


def parse_keyframe(kf: dict):
    interpolation = int(kf["interpolation"])
    value = kf["value"] if isinstance(kf["value"], list) else [kf["value"]]

    return Keyframe(int(kf["time"]), value, interpolation, get_corrected_tension(kf["tension"], interpolation))


def parse_config_file(filename):
    with open(filename, "r") as f:
        config = json.load(f)

    uniforms = [
        Uniform(uniform["name"], uniform["type"], [parse_keyframe(kf) for kf in uniform["keyframes"]], uniform.get("quantization"))
        for uniform in config["uniforms"]
    ]

    # Guarantee keyframe at 0
    # Necessary for runtime algorithm to work correctly
    for uniform in uniforms:
        if uniform.keyframes[0].time > 0:
            if len(uniform.keyframes) > 1:
                # Copy the first keyframe to time 0
                uniform.keyframes.insert(
                    0, Keyframe(0, uniform.keyframes[0].values, uniform.keyframes[0].interpolation, uniform.keyframes[0].tension)
                )
            else:
                # This uniform is a const so moving the keyframe to 0 will have no effect
                uniform.keyframes[0].time = 0

    return uniforms, config


def keyframe_to_array_string(keyframe: Keyframe, idx: int, include_tension: bool):
    tension = f", {keyframe.tension}" if include_tension else ""
    return f"{{{keyframe.time}, {keyframe.values[idx]:.6f}f, {keyframe.interpolation}{tension}}}"


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


def gen_keyframe_arrays(uniforms: list[Uniform], include_tension: bool):
    arrays = []
    index = 0

    for uniform in uniforms:
        for i in range(number_of_params(uniform)):
            kf_array = ", ".join(keyframe_to_array_string(kf, i, include_tension) for kf in uniform.keyframes)

            if len(kf_array) > 0:
                arrays.append(f"Keyframe keyframes{index}[] = {{{kf_array}}};")

            index += 1

    return arrays


def clamp_quantization_digits(digits) -> int:
    try:
        return max(0, int(digits))
    except (TypeError, ValueError):
        return 0


def get_shader_quantization_default(config: dict) -> int:
    return clamp_quantization_digits(config.get("shaderQuantizationDigits", 8))


def format_float_for_glsl(value: float, digits: int) -> str:
    if digits <= 0:
        return f"{int(round(value))}.0"
    return f"{round(value, digits):.{digits}f}"


def keyframe_values_to_const_value(type: str, values: list[float], digits: int):
    if type == "float":
        return format_float_for_glsl(values[0], digits)
    elif type == "vec2":
        return f"vec2({format_float_for_glsl(values[0], digits)},{format_float_for_glsl(values[1], digits)})"
    elif type in ("vec3", "color"):
        return (
            f"vec3({format_float_for_glsl(values[0], digits)}, {format_float_for_glsl(values[1], digits)},"
            f" {format_float_for_glsl(values[2], digits)})"
        )
    elif type == "vec4":
        return (
            f"vec4({format_float_for_glsl(values[0], digits)}, {format_float_for_glsl(values[1], digits)},"
            f" {format_float_for_glsl(values[2], digits)}, {format_float_for_glsl(values[3], digits)})"
        )
    else:
        return "0.0"


def uniform_to_const(uniform: Uniform, quantization_default_digits: int):
    try:
        keyframe_values = uniform.keyframes[0].values
    except IndexError:
        keyframe_values = [0] * number_of_params(uniform)

    digits = uniform.quantization if uniform.quantization is not None else quantization_default_digits
    digits = clamp_quantization_digits(digits)
    return Const(uniform.name, uniform.type, keyframe_values_to_const_value(uniform.type, keyframe_values, digits))


def split_animated_and_const_uniforms(uniforms: list[Uniform], quantization_default_digits: int):
    animated_uniforms = [uniform for uniform in uniforms if len(uniform.keyframes) > 1]
    consts = [uniform for uniform in uniforms if len(uniform.keyframes) <= 1]

    return animated_uniforms, [uniform_to_const(uniform, quantization_default_digits) for uniform in consts]


def generate_release_file_code(uniforms, include_tension: bool):
    # Generate all keyframe array declarations
    keyframe_arrays = "\n".join(gen_keyframe_arrays(uniforms, include_tension))

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
            params = [f"valueAtTime(time, keyframes{kf_index + j}, {number_of_keyframes})" for j in range(number_of_params(uniform))]
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


def generate_release_file(uniforms, output_filename, include_tension: bool):
    release_file_code = generate_release_file_code(uniforms, include_tension)

    with open(output_filename, "w") as f:
        f.write(release_file_code)


def generate_release_header(
    output_filename, bpm, length, resolution, default_interpolation_factor: float | None, interpolations_to_include: set[InterpolationType]
):
    default_interpolation = (
        f"#define DEFAULT_INTERPOLATION_FACTOR {default_interpolation_factor:.6f}f\n" if default_interpolation_factor is not None else ""
    )

    interpolation_includes = ""
    if InterpolationType.TONEMAP in interpolations_to_include:
        interpolation_includes += "#define INCLUDE_TONEMAP\n"
    if InterpolationType.GAIN in interpolations_to_include:
        interpolation_includes += "#define INCLUDE_GAIN\n"

    with open(output_filename, "w") as f:
        f.write(f"""#pragma once

#define BPM {bpm:.2f}f
#define DEMO_LENGTH {length:.2f}f
#define XRES {resolution[0]}
#define YRES {resolution[1]}
{default_interpolation}
{interpolation_includes}
""")


def get_default_interpolation_factor(uniforms: list[Uniform]) -> float | None:
    keyframes_that_require_tension = [
        keyframe
        for uniform in uniforms
        for keyframe in uniform.keyframes
        if keyframe.interpolation in (InterpolationType.TONEMAP, InterpolationType.GAIN)
    ]

    if len(keyframes_that_require_tension) == 0:
        return 0.0

    default_tension = keyframes_that_require_tension[0].tension

    # If all keyframes that require tension have the same tension, we can return it and save storage
    if all(abs(kf.tension - default_tension) < 1e-6 for kf in keyframes_that_require_tension):
        return default_tension

    return None


def generate_uniform_definitions(uniforms: list[Uniform]):
    if not SHOULD_COMBINE_UNIFORM_DEFINITIONS:
        return "\n".join([f"uniform {UNIFORM_TYPE_TO_OPENGL_TYPE[uniform.type]} {uniform.name};" for uniform in uniforms])

    # Combine all uniforms of the same type into a single definition, e.g. "uniform float u1, u2, u3;", because
    # the shader minifier doesn't optimize this by itself
    result = []

    for uniform_type, uniforms_of_type in itertools.groupby(
        sorted(uniforms, key=lambda u: UNIFORM_TYPE_TO_OPENGL_TYPE[u.type]), key=lambda u: UNIFORM_TYPE_TO_OPENGL_TYPE[u.type]
    ):
        uniform_definitions = ",".join([uniform.name for uniform in uniforms_of_type])
        result.append(f"uniform {UNIFORM_TYPE_TO_OPENGL_TYPE[uniform_type]} {uniform_definitions};")

    print("\n".join(result))
    return "\n".join(result)


def generate_minified_shader(shader_filename, uniforms: list[Uniform], consts: list[Const], output_filename, resolution=(800, 600)):
    TEMP_SHADER_FILENAME = "__temp_shader.glsl"
    TEMP_SHADER_VARNAME = "__temp_shader_glsl"

    with open(shader_filename, "r") as f:
        shader_code = f.read()

    # Add _t as uniform here so it gets minified with the rest
    uniform_definitions = generate_uniform_definitions([*uniforms, Uniform("_t", "float", [], None)])

    const_definition_list = [f"const {const.type} {const.name} = {const.value};" for const in consts]
    const_definitions = "\n".join(const_definition_list)

    injected_code = f"""#version 330
{uniform_definitions}
{const_definitions}
const vec2 _res = vec2({resolution[0]}, {resolution[1]});
vec2 fragCoord = gl_FragCoord.xy;
{shader_code}
"""

    with open(TEMP_SHADER_FILENAME, "w") as f:
        f.write(injected_code)

    subprocess.call(["shader_minifier.exe", "--aggressive-inlining", "-v", "-o", output_filename, TEMP_SHADER_FILENAME])
    os.remove(TEMP_SHADER_FILENAME)

    # Replace the file name with "fragmentShaderSource" (you can't do that using the shader minifier directly)
    # Also it has to be inlined, because it's imported in two different files
    with open(output_filename, "r") as f:
        minified_code = f.read()
    minified_code = minified_code.replace(f"const char *{TEMP_SHADER_VARNAME}", "const inline char *fragmentShaderSource")
    with open(output_filename, "w") as f:
        f.write(minified_code)


def main():
    os.makedirs("src/generated", exist_ok=True)
    config_filename = "config.json"
    output_filename = "src/generated/release.cpp"
    output_header_filename = "src/generated/release_config.h"
    shader_source_filename = "shaders/FragmentShader.glsl"
    shader_output_filename = "src/generated/shader.inl"

    uniforms, config = parse_config_file(config_filename)
    quantization_default_digits = get_shader_quantization_default(config)

    if SHOULD_INJECT_CONSTS:
        animated_uniforms, consts = split_animated_and_const_uniforms(uniforms, quantization_default_digits)
    else:
        animated_uniforms, consts = uniforms, []

    interpolations_to_include = set(keyframe.interpolation for uniform in animated_uniforms for keyframe in uniform.keyframes)
    default_interpolation_factor = get_default_interpolation_factor(animated_uniforms)
    include_tension = default_interpolation_factor is None

    generate_release_file(animated_uniforms, output_filename, include_tension)
    print(f"Generated {output_filename}")

    generate_release_header(
        output_header_filename,
        config["bpm"],
        config["lengthInBeats"],
        config["resolution"],
        default_interpolation_factor,
        interpolations_to_include,
    )

    generate_minified_shader(shader_source_filename, animated_uniforms, consts, shader_output_filename, config["resolution"])
    print(f"Generated {shader_output_filename}")


if __name__ == "__main__":
    main()
