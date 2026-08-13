import json
import os
import struct
import subprocess
from dataclasses import dataclass
from enum import IntEnum


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


UNIFORM_TYPE_TO_OPENGL_TYPE = {
    "float": "float",
    "vec2": "vec2",
    "vec3": "vec3",
    "color": "vec3",
    "vec4": "vec4",
}


def get_corrected_tension(tension, interpolation):
    # Copied from convert01ToCorrectFactor function in uniform.cpp, except we convert them to bytes
    # because it's unnoticeable.
    # These values are scaled 6x to have more range in the byte, the valueAtTime function
    # divides them by 6 appropriately.
    if interpolation == InterpolationType.TONEMAP:
        return int((tension - 0.5) * 120)
    elif interpolation == InterpolationType.GAIN:
        return int((tension * 19 + 1) * 6)
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
        if len(uniform["keyframes"]) > 0  # Empty uniforms get discarded
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


def keyframe_name(uniform: Uniform, idx: int):
    if uniform.type == "float":
        return uniform.name

    coord = "rgb"[idx] if uniform.type == "color" else "xyzw"[idx]

    return f"{uniform.name}.{coord}"


def compute_keyframe_scale(values: list[float]) -> float:
    """
    A single scale factor, such that multiplying `values` by it and rounding maps them into
    the signed byte range [-128, 127]. The original values are recovered by dividing by it.
    """
    max_abs = max(abs(v) for v in values)
    return quantize_float_bytes(127.0 / max_abs if max_abs > 0 else 1.0, 0)


def value_to_scaled_byte(value: float, scale: float) -> int:
    return max(-128, min(127, round(value * scale)))


def gen_keyframe_arrays(uniforms: list[Uniform], include_tension: bool):
    time_rows = []
    value_rows = []
    interpolation_rows = []
    tension_rows = []
    scales = []

    for uniform in uniforms:
        param_count = number_of_params(uniform)
        for i in range(param_count):
            keyframes = uniform.keyframes

            if len(keyframes) > 0:
                comment = f" // {keyframe_name(uniform, i)}"
                raw_values = [kf.values[i] for kf in keyframes]
                scale = compute_keyframe_scale(raw_values)
                scales.append(scale)

                time_rows.append(f"    {', '.join(str(kf.time) for kf in keyframes)},{comment}")
                value_rows.append(f"    {', '.join(str(value_to_scaled_byte(v, scale)) for v in raw_values)},{comment}")
                interpolation_rows.append(f"    {', '.join(str(kf.interpolation) for kf in keyframes)},{comment}")
                if include_tension:
                    tension_rows.append(f"    {', '.join(str(kf.tension) for kf in keyframes)},{comment}")

    declaration = "kf_time_t keyframeTimes[] = {\n" + "\n".join(time_rows) + "\n};\n"
    declaration += "char keyframeValues[] = {\n" + "\n".join(value_rows) + "\n};\n"
    declaration += "float keyframeScales[] = {" + ", ".join(f"{s}f" for s in scales) + "};\n"
    declaration += "unsigned char keyframeInterpolations[] = {\n" + "\n".join(interpolation_rows) + "\n};"
    if include_tension:
        declaration += "\nchar keyframeTensions[] = {\n" + "\n".join(tension_rows) + "\n};"

    return declaration


def total_uniform_value_count(uniforms: list[Uniform]):
    return sum(number_of_params(uniform) for uniform in uniforms)


def glsl_uniform_value_expression(uniform: Uniform, start_index: int):
    if uniform.type == "float":
        return f"_values[{start_index}]"

    values = ", ".join(f"_values[{start_index + i}]" for i in range(number_of_params(uniform)))
    return f"{UNIFORM_TYPE_TO_OPENGL_TYPE[uniform.type]}({values})"


def clamp_quantization_digits(digits) -> int:
    try:
        return max(0, int(digits))
    except (TypeError, ValueError):
        return 0


def get_shader_quantization_default(config: dict) -> int:
    return clamp_quantization_digits(config.get("shaderQuantizationDigits", 6))


def quantize_float_bytes(value: float, quantization_bytes: int) -> float:
    """
    Truncate the float to (2+quantization_bytes) bytes:
    If quantization_bytes is 0, the float will have 2 bytes and its last 2 bytes will be zeroed.
    If quantization_bytes is 1, the float will have 3 bytes and its last byte will be zeroed.
    If quantization_bytes is 2 or more, the float will have 4 bytes and no quantization will be applied.
    """
    # Convert the float to binary representation
    value_bytes = struct.unpack(">BBBB", struct.pack(">f", value))

    # Zero out the last bytes according to the quantization level
    quantized_bytes = value_bytes[: 2 + quantization_bytes] + (0, 0)
    quantized_bytes = quantized_bytes[:4]  # Ensure we only have 4 bytes total

    # Convert back to float
    quantized_value = struct.unpack(">f", struct.pack(">BBBB", *quantized_bytes))[0]
    return quantized_value


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
    return Const(uniform.name, uniform.type, keyframe_values_to_const_value(uniform.type, keyframe_values, digits))


def split_animated_and_const_uniforms(uniforms: list[Uniform], quantization_default_digits: int):
    animated_uniforms = [uniform for uniform in uniforms if len(uniform.keyframes) > 1]
    consts = [uniform_to_const(uniform, quantization_default_digits) for uniform in uniforms if len(uniform.keyframes) <= 1]

    return animated_uniforms, consts


def generate_release_file_code(uniforms: list[Uniform], include_tension: bool):
    # Generate a single combined keyframe array declaration
    keyframe_arrays = gen_keyframe_arrays(uniforms, include_tension)

    tension_arg = "keyframeTensions + offset, " if include_tension else ""

    keyframe_counts = [len(uniform.keyframes) for uniform in uniforms for _ in range(number_of_params(uniform))]
    value_index = 1 + len(keyframe_counts)

    uniform_value_count = value_index

    if any(count > 255 for count in keyframe_counts):
        # If any uniform has more than 255 keyframes, the count can't be saved as a byte.
        # The fix is easy - save as shorts - but I don't believe this will actually
        # happen so I didn't add code to handle it
        raise ValueError("A uniform has more than 255 keyframes - optimization not possible")

    uniform_updates = f"""    values[0] = time;
    unsigned char keyframeCounts[] = {{{", ".join(str(count) for count in keyframe_counts)}}};
    int offset = 0;
    for (int i = 0; i < {len(keyframe_counts)}; i++)
    {{
        values[i + 1] = valueAtTime(time, keyframeTimes + offset, keyframeValues + offset, keyframeInterpolations + offset, {tension_arg}keyframeCounts[i], keyframeScales[i]);
        offset += keyframeCounts[i];
    }}"""

    return f"""#include "../release.h"

GLuint valuesUniformLocation = 0;
float values[{uniform_value_count}] = {{0}};

{keyframe_arrays}

void locateUniforms(GLuint program) {{
    valuesUniformLocation = glGetUniformLocation(program, VAR__values);
}}

void updateUniforms(float time) {{
{uniform_updates}
    glUniform1fv(valuesUniformLocation, {uniform_value_count}, values);
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

typedef {"unsigned char" if length <= 255 else "int"} kf_time_t;
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
    result = [f"uniform float _values[{total_uniform_value_count(uniforms)}];"]
    value_index = 0

    for uniform in uniforms:
        value_expression = glsl_uniform_value_expression(uniform, value_index)
        result.append(f"{UNIFORM_TYPE_TO_OPENGL_TYPE[uniform.type]} {uniform.name} = {value_expression};")
        value_index += number_of_params(uniform)

    return "\n".join(result)


def generate_minified_shader(shader_filename, uniforms: list[Uniform], consts: list[Const], output_filename, resolution=(800, 600)):
    TEMP_SHADER_FILENAME = "__temp_shader.glsl"
    TEMP_SHADER_VARNAME = "__temp_shader_glsl"

    with open(shader_filename, "r") as f:
        shader_code = f.read()

    # Add _t as uniform here so it gets minified with the rest
    uniform_definitions = generate_uniform_definitions([Uniform("_t", "float", [], None), *uniforms])

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
    length = config["lengthInBeats"]

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
        length,
        config["resolution"],
        default_interpolation_factor,
        interpolations_to_include,
    )

    generate_minified_shader(shader_source_filename, animated_uniforms, consts, shader_output_filename, config["resolution"])
    print(f"Generated {shader_output_filename}")


if __name__ == "__main__":
    main()
