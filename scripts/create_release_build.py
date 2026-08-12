import math
import os
import subprocess
from dataclasses import dataclass
from enum import IntEnum
import json
import struct


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


def gen_keyframe_arrays(uniforms: list[Uniform], include_tension: bool):
    time_rows = []
    value_rows = []
    interpolation_rows = []
    tension_rows = []

    for uniform in uniforms:
        param_count = number_of_params(uniform)
        for i in range(param_count):
            keyframes = uniform.keyframes

            if len(keyframes) > 0:
                comment = f" // {keyframe_name(uniform, i)}"
                time_rows.append(f"    {', '.join(str(kf.time) for kf in keyframes)},{comment}")
                value_rows.append(f"    {', '.join(f'{kf.values[i]}f' for kf in keyframes)},{comment}")
                interpolation_rows.append(f"    {', '.join(str(kf.interpolation) for kf in keyframes)},{comment}")
                if include_tension:
                    tension_rows.append(f"    {', '.join(str(kf.tension) for kf in keyframes)},{comment}")

    declaration = "kf_time_t keyframeTimes[] = {\n" + "\n".join(time_rows) + "\n};\n"
    declaration += "float keyframeValues[] = {\n" + "\n".join(value_rows) + "\n};\n"
    declaration += "unsigned char keyframeInterpolations[] = {\n" + "\n".join(interpolation_rows) + "\n};"
    if include_tension:
        declaration += "\nchar keyframeTensions[] = {\n" + "\n".join(tension_rows) + "\n};"

    return declaration


def generate_byte_keyframe_array(uniforms: list[Uniform] | None, include_tension: bool, is_time_byte: bool):
    if not uniforms:
        return ""

    number_of_values = sum(number_of_params(uniform) for uniform in uniforms)
    number_of_keyframes = len(uniforms[0].keyframes)

    zipped_keyframes: list[list[Keyframe]] = zip(*(uniform.keyframes for uniform in uniforms), strict=True)

    max_values = [None] * number_of_values
    all_values: list[list[float]] = [[] for _ in range(number_of_values)]

    for value_keyframes in zipped_keyframes:
        time = value_keyframes[0].time
        interpolation = value_keyframes[0].interpolation
        tension = value_keyframes[0].tension

        for other_kf in value_keyframes[1:]:
            if time != other_kf.time or interpolation != other_kf.interpolation or abs(tension - other_kf.tension) > 1e-6:
                raise ValueError("Invalid camera keyframes - cannot optimize as bytes.")

        idx = 0
        for kf in value_keyframes:
            for v in kf.values:
                if max_values[idx] is None or abs(v) > max_values[idx]:
                    max_values[idx] = abs(v)
                all_values[idx].append(v)
                idx += 1

    # Normalize values and get factors
    # Factors are used to increase precision if the numbers are small
    normalized_values = [[] for _ in range(number_of_values)]
    factors = [1.0] * number_of_values

    for uniform_index, keyframe_values in enumerate(all_values):
        max_value = max_values[uniform_index]

        if max_value <= 1e-6:
            # Set everything to 1s
            factors[uniform_index] = max_value
            normalized_values[uniform_index] = [1.0] * len(keyframe_values)
            continue

        factors[uniform_index] = math.ceil(127 / max_value)
        normalized_values[uniform_index] = [min(127, max(-128, round(v * factors[uniform_index]))) for v in keyframe_values]

    array_rows = []
    for i, keyframe_values in enumerate(zip(*normalized_values, strict=True)):
        row = ", ".join(str(v) for v in keyframe_values)
        sample_keyframe = uniforms[0].keyframes[i]

        if include_tension:
            array_rows.append(f"{{{sample_keyframe.time}, {sample_keyframe.interpolation}, {sample_keyframe.tension}, {{{row}}}}}")
        else:
            array_rows.append(f"{{{sample_keyframe.time}, {sample_keyframe.interpolation}, {{{row}}}}}")

    return f"""
struct ByteKeyframe {{
    {"unsigned char" if is_time_byte else "int"} time;
    unsigned char interpolation;
    {"" if include_tension else "//"} float tension;
    char values[{number_of_values}];
}};

ByteKeyframe byteKeyframes[] = {{
    {",\n    ".join(array_rows)}
}};

unsigned char divisionFactors[{number_of_values}] = {{{", ".join(f"{factor}" for factor in factors)}}};
int tempTimes[{number_of_keyframes}] = {{}};
float tempValues[{number_of_keyframes}] = {{}};
unsigned char tempInterpolations[{number_of_keyframes}] = {{}};
{"" if include_tension else "//"}float tempTensions[{number_of_keyframes}] = {{}};
"""


def generate_byte_keyframe_updates(uniforms: list[Uniform] | None, offset: int, include_tension: bool):
    if not uniforms:
        return ""

    number_of_values = sum(number_of_params(uniform) for uniform in uniforms)
    number_of_keyframes = len(uniforms[0].keyframes)

    tension_assignment = "\n            tempTensions[i] = byteKeyframes[i].tension;" if include_tension else ""
    tension_arg = "tempTensions, " if include_tension else ""

    return f"""
    for (int k = 0; k < {number_of_values}; k++)
    {{
        for (int i = 0; i < {number_of_keyframes}; i++)
        {{
            tempTimes[i] = byteKeyframes[i].time;
            tempValues[i] = ((float)byteKeyframes[i].values[k]) / ((float)divisionFactors[k]);
            tempInterpolations[i] = byteKeyframes[i].interpolation;{tension_assignment}
        }}

        values[k + {offset}] = valueAtTime(time, tempTimes, tempValues, tempInterpolations, {tension_arg}0, {number_of_keyframes});
    }}
"""


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


def quantize_keyframe(keyframe: Keyframe, quantization_bytes: int):
    quantized_values = [quantize_float_bytes(v, quantization_bytes) for v in keyframe.values]
    return Keyframe(keyframe.time, quantized_values, keyframe.interpolation, keyframe.tension)


def quantize_uniform(uniform: Uniform, quantization_default_digits: int):
    quantization_bytes = uniform.quantization if uniform.quantization is not None else quantization_default_digits
    quantized_keyframes = [quantize_keyframe(kf, quantization_bytes) for kf in uniform.keyframes]

    return Uniform(uniform.name, uniform.type, quantized_keyframes, uniform.quantization)


def split_animated_and_const_uniforms(uniforms: list[Uniform], quantization_default_digits: int):
    animated_uniforms = [quantize_uniform(uniform, quantization_default_digits) for uniform in uniforms if len(uniform.keyframes) > 1]
    consts = [uniform_to_const(uniform, quantization_default_digits) for uniform in uniforms if len(uniform.keyframes) <= 1]

    return animated_uniforms, consts


def generate_release_file_code(uniforms: list[Uniform], length: int, include_tension: bool, save_camera_as_bytes: bool):
    camera_uniforms = None

    if save_camera_as_bytes:
        # Remove camera uniforms from keyframe creation
        camera_uniforms = [uniform for uniform in uniforms if uniform.name in ("_cp", "_cr")]
        uniforms = [uniform for uniform in uniforms if uniform.name not in ("_cp", "_cr")]

    # Generate a single combined keyframe array declaration
    keyframe_arrays = gen_keyframe_arrays(uniforms, include_tension)

    tension_arg = "keyframeTensions, " if include_tension else ""

    keyframe_counts = [len(uniform.keyframes) for uniform in uniforms for _ in range(number_of_params(uniform))]
    value_index = 1 + len(keyframe_counts)

    camera_uniform_value_count = (
        sum(number_of_params(camera_uniform) for camera_uniform in camera_uniforms) if camera_uniforms is not None else 0
    )
    uniform_value_count = value_index + camera_uniform_value_count

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
        values[i + 1] = valueAtTime(time, keyframeTimes, keyframeValues, keyframeInterpolations, {tension_arg}offset, keyframeCounts[i]);
        offset += keyframeCounts[i];
    }}"""

    byte_keyframes = generate_byte_keyframe_array(camera_uniforms, include_tension, length <= 255)
    byte_keyframe_updates = generate_byte_keyframe_updates(camera_uniforms, value_index, include_tension)

    return f"""#include "../release.h"

GLuint valuesUniformLocation = 0;
float values[{uniform_value_count}] = {{0}};

{keyframe_arrays}
{byte_keyframes}

void locateUniforms(GLuint program) {{
    valuesUniformLocation = glGetUniformLocation(program, VAR__values);
}}

void updateUniforms(float time) {{
{uniform_updates}
{byte_keyframe_updates}
    glUniform1fv(valuesUniformLocation, {uniform_value_count}, values);
}}
"""


def generate_release_file(uniforms, output_filename, length: int, include_tension: bool, save_camera_as_bytes: bool):
    release_file_code = generate_release_file_code(uniforms, length, include_tension, save_camera_as_bytes)

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
    save_camera_as_bytes = config.get("saveCameraAsBytes", False)
    length = config["lengthInBeats"]

    if SHOULD_INJECT_CONSTS:
        animated_uniforms, consts = split_animated_and_const_uniforms(uniforms, quantization_default_digits)
    else:
        animated_uniforms, consts = uniforms, []

    interpolations_to_include = set(keyframe.interpolation for uniform in animated_uniforms for keyframe in uniform.keyframes)
    default_interpolation_factor = get_default_interpolation_factor(animated_uniforms)
    include_tension = default_interpolation_factor is None

    generate_release_file(animated_uniforms, output_filename, length, include_tension, save_camera_as_bytes)
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
