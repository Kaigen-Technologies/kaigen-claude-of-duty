// the level map (ground, street and alleys, building footprints with their key-light edges, grain), the 10 m
// grid, the view cone, the contacts, the player arrow and the edge falloff, analytic in the element's UI pixels.
#include "common.glsl"
#include "ui/common_ui.glsl"

@vs
layout(set = 0, binding = 0) uniform _VSUniforms {
    vec2 resolution;
    vec2 _pad0;
    vec4 rect; // x, y, w, h in framebuffer px
} vs_uniforms;

layout(location = 0) out vec2 v_uv;

void main() {
    vec2 uv = ui_quad_uv(uint(gl_VertexIndex));
    v_uv = uv;
    gl_Position = ui_rect_to_clip(vs_uniforms.rect, uv, vs_uniforms.resolution);
}
@end

@fs
#include "cod_ui_common.glsl"

#define COD_MAP_ALLEYS 6
#define COD_MAP_BUILDINGS 32
#define COD_MAP_BLIPS 48

layout(set = 0, binding = 1) uniform _MapParams {
    vec4 view;    // x canvas size S (UI px), y metres across the view, z world x of the centre, w world z
    vec4 cone;    // x heading (rad, clockwise from north), y half fov (rad), z css px scale u, w opacity
    vec4 level;   // x cos(level yaw), y sin(level yaw), z level tx, w level tz: world = R * level + T
    vec4 counts;  // x blips, y buildings, z alleys, w corner radius (px)
    vec4 street;  // x street half width limit (level), y zmin, z zmax
    vec4 alleys[COD_MAP_ALLEYS];       // level x0, z0, x1, z1
    vec4 bld[64];                      // 2 per building: level x0, z0, x1, z1 ; then (tone, 0, 0, 0)
    vec4 blips[COD_MAP_BLIPS];         // world x, z, heading (rad), 1 = friend
} map;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 frag_color;

vec3 rgb8(float r, float g, float b) { return vec3(r, g, b) / 255.0; }

float cross2(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }

// pixel coverage of a 1 px line centred on offset d
float line_cov(float d) { return clamp(1.0 - abs(d), 0.0, 1.0); }

// coverage of a level-space rectangle, at `ppm` pixels per metre
float rect_cov(vec2 lv, vec2 a, vec2 b, float ppm) {
    float sd = min(min(lv.x - a.x, b.x - lv.x), min(lv.y - a.y, b.y - lv.y));
    return clamp(sd * ppm + 0.5, 0.0, 1.0);
}

// signed distance to the player arrow polygon (negative inside), in the arrow's own frame
float arrow_sd(vec2 p, float pr) {
    vec2 v0 = vec2(0.0, -pr * 1.55), v1 = vec2(pr * 1.15, pr * 1.3), v2 = vec2(0.0, pr * 0.6), v3 = vec2(-pr * 1.15, pr * 1.3);
    float d = 1e9;
    float s = 1.0;
    for (int i = 0; i < 4; i++) {
        vec2 a = i == 0 ? v0 : i == 1 ? v1 : i == 2 ? v2 : v3;
        vec2 b = i == 0 ? v1 : i == 1 ? v2 : i == 2 ? v3 : v0;
        vec2 e = b - a, w = p - a;
        vec2 q = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
        d = min(d, dot(q, q));
        bool c1 = p.y >= a.y, c2 = p.y < b.y, c3 = e.x * w.y > e.y * w.x;
        if ((c1 && c2 && c3) || (!c1 && !c2 && !c3))
            s = -s;
    }
    return s * sqrt(d);
}

// signed distance to the contact triangle (negative inside)
float blip_sd(vec2 p, float r) {
    vec2 a = vec2(0.0, -r * 1.5), b = vec2(r * 1.15, r * 1.1), c = vec2(-r * 1.15, r * 1.1);
    vec2 e0 = b - a, e1 = c - b, e2 = a - c;
    vec2 v0 = p - a, v1 = p - b, v2 = p - c;
    vec2 q0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 q1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 q2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(q0, q0), s * (v0.x * e0.y - v0.y * e0.x)), vec2(dot(q1, q1), s * (v1.x * e1.y - v1.y * e1.x))),
                 vec2(dot(q2, q2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

// a canvas shadowBlur of a compact shape: a Gaussian of sigma blur / 2 widened by the shape's own spread, scaled by area
float shape_glow(float dist, float area, float spread, float sigma) {
    float s2 = sigma * sigma + spread * spread;
    return min(area / (6.2831853 * s2), 1.0) * exp(-dist * dist / (2.0 * s2));
}

vec3 over3(vec3 dst, vec3 c, float a) { return mix(dst, c, a); }

void main() {
    float S = map.view.x;
    float ppm = S / map.view.y;
    float u = map.cone.z;
    vec2 p = v_uv * S;
    vec2 d = p - vec2(0.5 * S);
    vec2 world = map.view.zw + d / ppm;

    // level space of this pixel
    vec2 wl = world - map.level.zw;
    float lc = map.level.x, ls = map.level.y;
    vec2 lv = vec2(lc * wl.x - ls * wl.y, ls * wl.x + lc * wl.y);

    // out-of-play ground, then the street and alleys as the lighter negative space between the blocks
    vec3 col = rgb8(43.0, 52.0, 61.0);
    float open = clamp((min(map.street.x - abs(lv.x), min(lv.y - map.street.y, map.street.z - lv.y))) * ppm + 0.5, 0.0, 1.0);
    for (int i = 0; i < COD_MAP_ALLEYS; i++) {
        if (i >= int(map.counts.z))
            break;
        vec4 a = map.alleys[i];
        open = max(open, rect_cov(lv, a.xy, a.zw, ppm));
    }
    col = over3(col, rgb8(99.0, 113.0, 126.0), open);

    // building footprints: a tone by height, then a light return on the north and west edges and a dark one south and east
    for (int i = 0; i < COD_MAP_BUILDINGS; i++) {
        if (i >= int(map.counts.y))
            break;
        vec4 r = map.bld[2 * i];
        float t = map.bld[2 * i + 1].x;
        float cov = rect_cov(lv, r.xy, r.zw, ppm);
        if (cov <= 0.0)
            continue;
        vec3 tone = rgb8(mix(50.0, 68.0, t), mix(59.0, 79.0, t), mix(68.0, 90.0, t));
        col = over3(col, tone, cov);
        col = over3(col, rgb8(206.0, 228.0, 244.0), 0.20 * rect_cov(lv, r.xy, vec2(r.z, r.y + 0.34), ppm));
        col = over3(col, rgb8(206.0, 228.0, 244.0), 0.20 * rect_cov(lv, r.xy, vec2(r.x + 0.34, r.w), ppm));
        col = over3(col, rgb8(3.0, 7.0, 10.0), 0.34 * rect_cov(lv, vec2(r.x, r.w - 0.34), r.zw, ppm));
        col = over3(col, rgb8(3.0, 7.0, 10.0), 0.34 * rect_cov(lv, vec2(r.z - 0.34, r.y), r.zw, ppm));
    }

    // grain: no map surface is a flat colour
    col += (hz_hash12(floor(world * 2.69)) - 0.5) * (5.5 / 255.0);

    // 10 m grid, phase-locked to the world: 1 px lines on whole columns and rows
    float on_line = 0.0;
    for (int k = 0; k < 2; k++) {
        float nx = floor(world.x / 10.0) + float(k);
        float x = floor((nx * 10.0 - map.view.z) * ppm + 0.5 * S + 0.5) + 0.5;
        on_line = max(on_line, line_cov(p.x - x));
        float nz = floor(world.y / 10.0) + float(k);
        float y = floor((nz * 10.0 - map.view.w) * ppm + 0.5 * S + 0.5) + 0.5;
        on_line = max(on_line, line_cov(p.y - y));
    }
    col = over3(col, rgb8(10.0, 17.0, 23.0), 0.20 * on_line);

    // view cone: a radial gradient inside the sector, then its 1 px outline
    float r = length(d);
    float cone_r = S * 0.42;
    float heading = map.cone.x, hf = map.cone.y;
    float a0 = -1.5707963 + heading - hf, a1 = -1.5707963 + heading + hf;
    float rel = mod(atan(d.y, d.x) - a0 + 25.1327412, 6.2831853);
    float span_a = a1 - a0;
    vec2 e0 = vec2(cos(a0), sin(a0)), e1 = vec2(cos(a1), sin(a1));
    float edge0 = cross2(e0, d), edge1 = -cross2(e1, d);
    float in_wedge = clamp(min(edge0, edge1) + 0.5, 0.0, 1.0) * (rel <= span_a + 0.1 ? 1.0 : 0.0);
    float in_disc = clamp(cone_r - r + 0.5, 0.0, 1.0);
    float t = clamp((r - 2.0) / (cone_r - 2.0), 0.0, 1.0);
    float cone_a = t < 0.7 ? mix(0.26, 0.075, t / 0.7) : mix(0.075, 0.0, (t - 0.7) / 0.3);
    col = over3(col, rgb8(222.0, 242.0, 255.0), cone_a * in_wedge * in_disc);
    float on_ray = 0.0;
    if (r <= cone_r + 1.0) {
        if (dot(d, e0) > -0.5)
            on_ray = max(on_ray, line_cov(edge0));
        if (dot(d, e1) > -0.5)
            on_ray = max(on_ray, line_cov(edge1));
    }
    if (rel <= span_a)
        on_ray = max(on_ray, line_cov(r - cone_r));
    col = over3(col, rgb8(226.0, 244.0, 255.0), 0.17 * on_ray);

    // contacts: a triangle turned to its heading under its glow (shadowBlur 6 u)
    float br = 3.4 * u;
    float bsig = 3.0 * u;
    float b_area = 2.99 * br * br;
    for (int i = 0; i < COD_MAP_BLIPS; i++) {
        if (i >= int(map.counts.x))
            break;
        vec4 b = map.blips[i];
        vec2 c = (b.xy - map.view.zw) * ppm + 0.5 * S;
        vec2 bd = p - c;
        float reach = br * 1.6 + bsig * 3.5;
        if (dot(bd, bd) > reach * reach)
            continue;
        float bc = cos(b.z), bs = sin(b.z);
        vec2 bq = vec2(bc * bd.x + bs * bd.y, -bs * bd.x + bc * bd.y);
        bool friend_ = b.w > 0.5;
        vec3 fill = friend_ ? rgb8(126.0, 196.0, 255.0) : rgb8(255.0, 74.0, 58.0);
        float fill_a = friend_ ? 0.95 : 0.96;
        vec3 glow = friend_ ? rgb8(120.0, 190.0, 255.0) : rgb8(255.0, 60.0, 40.0);
        float glow_a = friend_ ? 0.7 : 0.85;
        // the glow is centred on the triangle's centroid
        float g = shape_glow(length(bq - vec2(0.0, br * 0.23)), b_area, br * 0.5, bsig);
        col = over3(col, glow, glow_a * fill_a * g);
        col = over3(col, fill, fill_a * clamp(0.5 - blip_sd(bq, br), 0.0, 1.0));
    }

    // the player arrow turned to the heading: glow, 1.6 u dark stroke, glow, white fill
    float cs = cos(heading), sn = sin(heading);
    vec2 q = vec2(cs * d.x + sn * d.y, -sn * d.x + cs * d.y);
    float pr = 4.8 * u;
    float lw = 1.6 * u;
    float sigma = 2.5 * u;
    float sd = arrow_sd(q, pr);
    if (sd < sigma * 4.0 + lw) {
        float a_area = 2.0 * (0.5 * (pr * 1.15) * (pr * 1.9) + 0.5 * (pr * 1.15) * (pr * 0.7));
        float g = shape_glow(max(sd, 0.0) + pr * 0.5, a_area, pr * 0.6, sigma);
        col = over3(col, rgb8(180.0, 225.0, 255.0), 0.85 * g);
        col = over3(col, rgb8(2.0, 6.0, 10.0), 0.85 * clamp(lw * 0.5 + 0.5 - abs(sd), 0.0, 1.0));
        col = over3(col, rgb8(246.0, 252.0, 255.0), clamp(0.5 - sd, 0.0, 1.0));
    }

    // edge falloff so the map sinks into the frame
    float vt = clamp((r - S * 0.28) / (S * 0.44), 0.0, 1.0);
    col = over3(col, vec3(0.0), 0.17 * vt);

    // the canvas element's border radius
    vec2 half_box = vec2(0.5 * S);
    vec2 pc = abs(p - half_box) - (half_box - map.counts.w);
    float rd = length(max(pc, 0.0)) - map.counts.w;
    float cov = clamp(0.5 - rd, 0.0, 1.0);
    frag_color = cod_ui_out(vec4(col, cov * map.cone.w));
}
@end
