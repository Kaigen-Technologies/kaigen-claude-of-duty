// The shared tail of every surface bake program: evaluates owSurface once and packs the height at 16 bits across the two targets.
void main() {
    vec3 alb = vec3(0.5);
    float h = 0.5, rough = 0.5, metal = 0.0, ao = 1.0;
    owSurface(v_uv, alb, h, rough, metal, ao);
    float h8 = h * 255.0;
    float hq = floor(h8 + 0.5);
    frag_albedo = vec4(alb, hq / 255.0);
    frag_orm = vec4(ao, rough, metal, h8 - hq + 0.5);
}
