uniform vec3 _cp, _ct;
uniform float _t;
uniform vec2 _res;
out vec4 fragColor;

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = (fragCoord / _res - 0.5) / vec2(_res.y / _res.x, 1);

    float c = 0.5 + sin(uv.x * 10.0) + cos(sin(_t + uv.y) * 20.0);
    vec4 plasma = vec4(sin(c * 0.2 + cos(_t)), c * 0.15, cos(c * 0.1 + _t / .4) * .25, 1.0);

    fragColor = plasma;
}
