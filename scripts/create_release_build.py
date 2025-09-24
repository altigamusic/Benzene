import os
from re import sub
import subprocess

UNIFORM_TYPE_TO_GL_FUNCTION = {
    "float": "glUniform1f",
    "vec2": "glUniform2f",
    "vec3": "glUniform3f",
    "color": "glUniform3f",
    "vec4": "glUniform4f",
}


def getCorrectedTension(tension, interpolation):
    # Interpolation 0 - linear, 1 - step, 2 - tonemap, 3 - gain
    # No factor needed for linear or step
    if interpolation not in (2, 3):
        return 0

    a = tension

    if a < 0.5:
        a *= 2
    else:
        a = 1 + (a - 0.5) * 20

    if interpolation == 2:  # Tonemap
        a -= 1

    return a


def parse_keyframe(keyframe_string):
    if keyframe_string == "":
        return None

    parts = keyframe_string.split(",")
    if len(parts) != 4:
        raise ValueError(f"Invalid keyframe string: {keyframe_string}")

    time, value, interpolation, tension = parts
    time = int(time)
    interpolation = int(interpolation)
    tension = round(getCorrectedTension(float(tension), interpolation))
    values = [float(v) for v in value.split("/")]

    return time, values, interpolation, tension


def parse_uniforms_file(filename):
    uniforms = []

    with open(filename, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(";")
            if len(parts) < 2:
                continue

            keyframes = list(filter(bool, map(parse_keyframe, parts[2:])))
            uniforms.append({"name": parts[0], "type": parts[1], "keyframes": keyframes})

    return uniforms


def keyframe_to_array(keyframe, idx):
    time, values, interpolation, tension = keyframe
    return f"{{{time}, {values[idx]:.6f}f, {interpolation}, {tension}}}"


def number_of_params(uniform):
    if uniform["type"] == "float":
        return 1
    elif uniform["type"] == "vec2":
        return 2
    elif uniform["type"] in ("vec3", "color"):
        return 3
    elif uniform["type"] == "vec4":
        return 4
    else:
        raise ValueError(f"Unsupported uniform type: {uniform['type']}")


def gen_keyframe_arrays(uniforms):
    arrays = []
    index = 0

    for uniform in uniforms:
        for i in range(number_of_params(uniform)):
            kf_array = ", ".join(keyframe_to_array(kf, i) for kf in uniform["keyframes"])

            if len(kf_array) > 0:
                arrays.append(f"Keyframe keyframes{index}[] = {{{kf_array}}};")

            index += 1

    return arrays


def generate_release_file_code(uniforms):
    # Generate all keyframe array declarations
    keyframe_arrays = "\n".join(gen_keyframe_arrays(uniforms))

    # Generate uniform location assignments
    uniform_locations = "\n".join(
        f'    UNIFORMS[{i}] = glGetUniformLocation(program, VAR_{uniform["name"]});' for i, uniform in enumerate(uniforms)
    )

    kf_index = 0
    updates = []

    for i, uniform in enumerate(uniforms):
        gl_func = UNIFORM_TYPE_TO_GL_FUNCTION.get(uniform["type"])

        if not gl_func:
            raise ValueError(f"Unsupported uniform type: {uniform['type']}")

        number_of_keyframes = len(uniform["keyframes"])
        if number_of_keyframes == 0:
            params = ["0.0f"] * number_of_params(uniform)
        else:
            params = [
                f"valueAtTime(time, keyframes{kf_index + j}, {number_of_keyframes})" for j in range(number_of_params(uniform))]
        param_string = ",".join(params)
        kf_index += len(params)

        updates.append(f"    {gl_func}(UNIFORMS[{i}], {param_string});")
    uniform_updates = "\n".join(updates)

    return f"""#include "../release.h"

GLuint UNIFORMS[{len(uniforms)}] = {{0}};
GLuint timeUniformLocation = 0;

{keyframe_arrays}

void locateUniforms(GLuint program) {{
    timeUniformLocation = glGetUniformLocation(program, VAR__t);
{uniform_locations}
}}

void updateUniforms(long time) {{
    glUniform1f(timeUniformLocation, time / 1000.0f);
{uniform_updates}
}}
"""


def generate_release_file(uniforms, output_filename):
    release_file_code = generate_release_file_code(uniforms)

    with open(output_filename, "w") as f:
        f.write(release_file_code)


def generate_minified_shader(shader_filename, uniforms, output_filename, resolution=(800, 600)):
    with open(shader_filename, "r") as f:
        shader_code = f.read()

    uniform_definitions = [f"uniform {uniform['type']} {uniform['name']};" for uniform in uniforms]
    uniform_definition_code = "\n".join(uniform_definitions)

    injected_code = f"""#version 330
uniform float _t;
{uniform_definition_code}
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
    uniforms_filename = "uniforms.txt"
    output_filename = "src/generated/release.cpp"
    shader_source_filename = "shaders/FragmentShader.glsl"
    shader_output_filename = "src/generated/shader.inl"

    uniforms = parse_uniforms_file(uniforms_filename)

    generate_release_file(uniforms, output_filename)
    print(f"Generated {output_filename}")

    generate_minified_shader(shader_source_filename, uniforms, shader_output_filename)
    print(f"Generated {shader_output_filename}")


if __name__ == "__main__":
    main()
