// Tileable procedural noise for the surface bakes: every function wraps its lattice by `per`, so a bake over uv in [0,1) tiles.
// Included inside an @fs block. Hashes are sine-free (Hoskins).

float owHash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}
float owHash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
vec2 owHash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}
vec3 owHash32(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}
vec4 owHash42(vec2 p) {
    vec4 p4 = fract(vec4(p.xyxy) * vec4(0.1031, 0.1030, 0.0973, 0.1099));
    p4 += dot(p4, p4.wzxy + 33.33);
    return fract((p4.xxyz + p4.yzzw) * p4.zywx);
}

vec2 owGrad2(vec2 i, vec2 per) {
    float a = owHash12(mod(i, per) + 0.317) * 6.28318530718;
    return vec2(cos(a), sin(a));
}

// periodic gradient noise, about [-1, 1]
float owNoise(vec2 p, vec2 per) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float a = dot(owGrad2(i + vec2(0.0, 0.0), per), f - vec2(0.0, 0.0));
    float b = dot(owGrad2(i + vec2(1.0, 0.0), per), f - vec2(1.0, 0.0));
    float c = dot(owGrad2(i + vec2(0.0, 1.0), per), f - vec2(0.0, 1.0));
    float d = dot(owGrad2(i + vec2(1.0, 1.0), per), f - vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y) * 1.4142;
}
float owNoise01(vec2 p, vec2 per) { return owNoise(p, per) * 0.5 + 0.5; }

float owValue(vec2 p, vec2 per) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = owHash12(mod(i + vec2(0.0, 0.0), per) + 1.7);
    float b = owHash12(mod(i + vec2(1.0, 0.0), per) + 1.7);
    float c = owHash12(mod(i + vec2(0.0, 1.0), per) + 1.7);
    float d = owHash12(mod(i + vec2(1.0, 1.0), per) + 1.7);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float owFbm(vec2 p, vec2 per, int oct, float gain) {
    float s = 0.0, a = 0.5, n = 0.0;
    for (int i = 0; i < 10; i++) {
        if (i >= oct) break;
        s += a * owNoise(p, per);
        n += a;
        p *= 2.0; per *= 2.0; a *= gain;
    }
    return s / max(n, 1e-4);
}
float owFbm01(vec2 p, vec2 per, int oct, float gain) { return owFbm(p, per, oct, gain) * 0.5 + 0.5; }

// ridged fbm, [0, 1]: sharp creases
float owRidged(vec2 p, vec2 per, int oct, float gain) {
    float s = 0.0, a = 0.5, n = 0.0;
    for (int i = 0; i < 10; i++) {
        if (i >= oct) break;
        float v = 1.0 - abs(owNoise(p, per));
        s += a * v * v;
        n += a;
        p *= 2.0; per *= 2.0; a *= gain;
    }
    return s / max(n, 1e-4);
}

// billowy fbm: puffy clumps
float owBillow(vec2 p, vec2 per, int oct, float gain) {
    float s = 0.0, a = 0.5, n = 0.0;
    for (int i = 0; i < 10; i++) {
        if (i >= oct) break;
        s += a * abs(owNoise(p, per));
        n += a;
        p *= 2.0; per *= 2.0; a *= gain;
    }
    return s / max(n, 1e-4);
}

vec2 owWarp(vec2 p, vec2 per, float amp, int oct) {
    vec2 q = vec2(owFbm(p + vec2(1.7, 9.2), per, oct, 0.5), owFbm(p + vec2(8.3, 2.8), per, oct, 0.5));
    return p + amp * q;
}

// periodic Worley: x = F1, y = F2, z and w = hashes of the F1 cell
vec4 owWorley(vec2 p, vec2 per, float jitter) {
    vec2 ip = floor(p), fp = fract(p);
    float f1 = 8.0, f2 = 8.0;
    vec2 id = vec2(0.0);
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(float(x), float(y));
            vec2 cell = mod(ip + g, per);
            vec2 o = owHash22(cell + 0.771) * jitter + (1.0 - jitter) * 0.5;
            vec2 r = g + o - fp;
            float d = dot(r, r);
            if (d < f1) { f2 = f1; f1 = d; id = owHash22(cell + 3.117); }
            else if (d < f2) { f2 = d; }
        }
    }
    return vec4(sqrt(f1), sqrt(f2), id);
}

// distance to the Voronoi cell edge (Quilez two-pass)
float owVoronoiEdge(vec2 p, vec2 per, float jitter) {
    vec2 ip = floor(p), fp = fract(p);
    vec2 mr = vec2(0.0), mg = vec2(0.0);
    float md = 8.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(float(x), float(y));
            vec2 o = owHash22(mod(ip + g, per) + 0.771) * jitter + (1.0 - jitter) * 0.5;
            vec2 r = g + o - fp;
            float d = dot(r, r);
            if (d < md) { md = d; mr = r; mg = g; }
        }
    }
    md = 8.0;
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            vec2 g = mg + vec2(float(x), float(y));
            vec2 o = owHash22(mod(ip + g, per) + 0.771) * jitter + (1.0 - jitter) * 0.5;
            vec2 r = g + o - fp;
            vec2 diff = r - mr;
            if (dot(diff, diff) > 1e-5) {
                md = min(md, dot(0.5 * (mr + r), normalize(diff)));
            }
        }
    }
    return md;
}

// warped Voronoi edges thinned and broken up, [0, 1], 1 = deep crack
float owCracks(vec2 p, vec2 per, float jitter, float width, float breakUp) {
    vec2 wp = owWarp(p, per, 0.20, 3);
    float e = owVoronoiEdge(wp, per, jitter);
    float c = 1.0 - smoothstep(0.0, width, e);
    float mask = owFbm01(p * 1.7 + 11.3, per * 1.7, 4, 0.55);
    c *= smoothstep(breakUp, breakUp + 0.28, mask);
    return clamp(c, 0.0, 1.0);
}

float owSat(float x) { return clamp(x, 0.0, 1.0); }
vec3 owSat3(vec3 x) { return clamp(x, 0.0, 1.0); }
float owRemap(float x, float a, float b, float c, float d) {
    return c + (d - c) * clamp((x - a) / max(b - a, 1e-5), 0.0, 1.0);
}
vec2 owRot(vec2 p, float a) {
    float s = sin(a), c = cos(a);
    return mat2(c, -s, s, c) * p;
}
// authoring colours in gamma space, output linear
vec3 owSRGB(vec3 c) {
    return mix(pow((c + 0.055) / 1.055, vec3(2.4)), c / 12.92, step(c, vec3(0.04045)));
}
// shear that keeps tileability: k and stretch must be integers
vec2 owShear(vec2 p, float k, float stretch) { return vec2(p.x + p.y * k, p.y * stretch); }
vec2 owShearPer(vec2 per, float stretch) { return vec2(per.x, per.y * stretch); }

// long thin streaks along a sheared axis, [0, 1]
float owScratches(vec2 p, vec2 per, float stretch, float k, float thin) {
    vec2 q = owShear(p, k, stretch);
    vec2 qper = owShearPer(per, stretch);
    float n = owFbm01(q, qper, 4, 0.5);
    return smoothstep(thin, thin + 0.06, n) * (1.0 - smoothstep(thin + 0.06, thin + 0.2, n));
}
