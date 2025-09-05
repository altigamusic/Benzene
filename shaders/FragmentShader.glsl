out vec4 fragColor;
const float PI=3.14159265359;
const float TAU=2.*PI;

const vec3 R = vec3(1,0,0);
const vec3 G = vec3(0,1,0);

mat2 rot(float angle) {
    float c = cos(angle), s = sin(angle);
    return mat2(c, -s, s, c);
}

float circle(vec2 uv, vec2 p, float r) {
    return smoothstep(r, r + 0.01, length(uv - p));
}

void main(){
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv=(fragCoord/_res-0.5)/vec2(_res.y/_res.x,1);  //2d uvs
    vec3 col=vec3(0.5)*cos(uv.x*PI)*.5+.5;
    
    vec2 p = vec2(rad, 0);
    p *= rot(_t);
    col = mix(R, col, circle(uv, p, 0.1));
    p *= rot(TAU / 3.0);
    col = mix(R, col, circle(uv, p, 0.1));
    p *= rot(TAU / 3.0);
    col = mix(R, col, circle(uv, p, 0.1));

    col = mix(G, col, circle(uv, p1, 0.1));

    fragColor=vec4(col,1);
}