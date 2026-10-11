// Layered iron oxide shared by the rusted metals; included inside an @fs block after the noise library.
vec3 owRustColour(float t, float grain) {
    // young rust is orange, mature rust dark red-brown, old rust near-black
    vec3 c1 = owSRGB(vec3(0.560, 0.290, 0.110));
    vec3 c2 = owSRGB(vec3(0.380, 0.180, 0.085));
    vec3 c3 = owSRGB(vec3(0.190, 0.100, 0.060));
    vec3 c4 = owSRGB(vec3(0.640, 0.400, 0.190)); // powdery bloom
    vec3 c = mix(c1, c2, smoothstep(0.15, 0.6, t));
    c = mix(c, c3, smoothstep(0.55, 1.0, t));
    c = mix(c, c4, smoothstep(0.55, 0.95, grain) * 0.45);
    return c * (0.82 + 0.36 * grain);
}
