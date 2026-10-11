// the look's lighting budgets every COD surface applies; published by cod_look.c as _CodLook
// Include at file top (the type, for codegen) and in each stage after renderer_shaders.glsl (the block).
#ifndef COD_LOOK_TYPES
#define COD_LOOK_TYPES
#define COD_LOOK_ROOMS 10
#define COD_LOOK_VIEW_FLASHES 2
@reflect
struct CodLookBlock {
    vec4 key_gain;    // xyz multiplier on the main light's (index 0, the sun) direct term: the scene key over the engine's physical sun; w = AO-on-direct fraction (0.35 x ao strength)
    vec4 sky_fill;    // xyz sky fill irradiance, pre-exposed; w hemispheric gain
    vec4 ground_fill; // xyz ground fill irradiance, pre-exposed; w sun-bounce wrap gain
    vec4 fill_dir;    // x,y sky gate smoothstep over N.y; z,w ground gate over -N.y
    vec4 indirect;    // x IBL/ambient diffuse budget, y interior indirect floor, z room count, w AO visibility floor (0.25)
    vec4 room_xf;     // world -> level 2D: (cos, sin, tx, tz)
    vec4 sun_dir;     // xyz toward the sun, w night 0..1
    vec4 rooms[COD_LOOK_ROOMS];   // level space (cx, cz, hx, hz)
    vec4 rooms_y[COD_LOOK_ROOMS]; // (y0, y1, 0, 0)
    vec4 view_key;    // the viewmodel rig: xyz lux, pre-exposed; w fill occlusion
    vec4 view_fill;   // w environment intensity, replacing the material's
    vec4 view_rim;
    vec4 view_bounce;
    vec4 view_hemi_sky;
    vec4 view_hemi_ground;
    vec4 view_flash[COD_LOOK_VIEW_FLASHES];       // xyz world, w cutoff metres
    vec4 view_flash_color[COD_LOOK_VIEW_FLASHES]; // xyz candela, pre-exposed
};
#endif

#if defined(HZ_RENDERER_FRAGMENT_SHADER) && !defined(COD_LOOK_FS)
#define COD_LOOK_FS
uniform _CodLook { CodLookBlock cod_look; };
#endif
