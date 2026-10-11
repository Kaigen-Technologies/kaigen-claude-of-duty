// the sky dome's inputs; published by cod_look.c as _CodSky
// Include at file top (the type, for codegen) and in the fragment stage after renderer_shaders.glsl (the block).
#ifndef COD_SKY_TYPES
#define COD_SKY_TYPES
#define COD_SKY_CUMULUS_KM 1.5
#define COD_SKY_CIRRUS_KM 7.8
@reflect
struct CodSkyBlock {
    vec4 sun;             // xyz toward the sun, w the roll-off knee in cd/m2
    vec4 sun_irradiance;  // xyz extraterrestrial irradiance x tint, lux; w the roll-off power
    vec4 moon;            // xyz toward the moon, w horizon murk
    vec4 moon_irradiance; // xyz lux; w the viewer's altitude, km
    vec4 clouds;          // x cumulus coverage, y density, z quality (1 screen), w seconds
    vec4 clouds2;         // x cirrus coverage, y cirrus opacity, zw wind km/s
};
#endif

#if defined(HZ_RENDERER_FRAGMENT_SHADER) && !defined(COD_SKY_FS)
#define COD_SKY_FS
uniform _CodSky { CodSkyBlock cod_sky; };
#endif
