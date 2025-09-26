out vec4 fragColor;

const vec2 ep = vec2(.00035, -.00035);
const float far = 80.;

float box(vec3 p, vec3 r) {
    p = abs(p) - r;
    return max(max(p.x, p.y), p.z);
}

vec2 map(vec3 p) {
    vec2 h, t;
    t = vec2(box(p, vec3(2)), 1);
    h = vec2(box(p, vec3(1, 2.5, 1)), 2);
    t = t.x < h.x ? t : h;
    return t;
}

vec2 raycast(vec3 ro, vec3 rd) {
    vec2 h, t = vec2(0.);
    for(int i = 0; i < 128; i++) {
        h = map(ro + rd * t.x);
        if(h.x < .0001 || t.x > far)
            break;
        t.x += h.x;
        t.y = h.y;
    }
    return t;
}

void main() {
    vec2 uv = (fragCoord / _res - 0.5) / vec2(_res.y / _res.x, 1);
    vec3 ro = _cp, cw = normalize(_ct - ro), cu = normalize(cross(cw, vec3(0, 1, 0))), cv = normalize(cross(cu, cw)), rd = mat3(cu, cv, cw) * normalize(vec3(uv, .5)), ld = normalize(vec3(-.1, .4, .3)), co, fo;
    co = fo = vec3(.1, .1, .1) - length(uv) * .1;
    vec2 t = raycast(ro, rd);
    
    if(t.x < far) {
        vec3 po = ro + rd * t.x;
        vec3 no = normalize(ep.xyy * map(po + ep.xyy).x + ep.yyx * map(po + ep.yyx).x +
            ep.yxy * map(po + ep.yxy).x + ep.xxx * map(po + ep.xxx).x);
        vec3 al = vec3(.5, .5, .5);

        if(t.y > 1.0)
            al = vec3(.0, .0, .0);

        float dif = max(0., dot(no, ld)), fre = pow(1. + dot(no, rd), 4.), spe = pow(max(dot(reflect(-ld, no), -rd), 0.), 30.), ao = clamp(map(po + no * .1).x / .1, 0., 1.), sss = smoothstep(0., 1., map(po + ld * .4).x / .4);
        co = mix(spe + al * (ao + .2) * (dif + sss * .5), fo, fre);
        co = mix(fo, co, exp(-.0001 * t.x * t.x * t.x));
    }

    fragColor = vec4(pow(max(co, 0.), vec3(.4545)), 1);
}