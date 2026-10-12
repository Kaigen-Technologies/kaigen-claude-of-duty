// Claude of Duty post passes: the motion blur and the contact shadows, enabled per quality preset, and the low-health treatment.

typedef struct CodPost {
  CodBlur *blur;
  CodContact *contact;
  CodLowHealth *lowhealth;
} CodPost;

CodPost *cod_post_init(EcsWorld *w, RendererState *r, Allocator *alloc, GpuBuffer exposure) {
  CodPost *p = ALLOC(alloc, CodPost);
  p->blur = cod_blur_init(r, alloc);
  p->lowhealth = cod_lowhealth_init(r, alloc, exposure); // registered after the blur: treats the blurred frame
  p->contact = cod_contact_init(r, alloc);
  return p;
}

// main lane before the renderer: the level's preset gates each pass, the key light aims the contact ray
void cod_post_sync(CodPost *p, const CodLook *look, RendererQuality level, v4 hurt) {
  u32 quality = MIN((u32)level, (u32)COD_QUALITY_ULTRA);
  CodQualityPreset q = cod_quality_preset(quality);
  p->blur->enabled = q.motion_blur;
  p->contact->enabled = quality >= COD_QUALITY_MEDIUM;
  p->contact->sun = look->key_dir;
  p->lowhealth->hurt = hurt;
}
