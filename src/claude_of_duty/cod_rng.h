// xoshiro128** seeded by SplitMix32: same seed and call order, same street.
#pragma once
#include "lib/hzmath.h"
#include "lib/typedefs.h"

typedef struct CodRng {
  u32 s0, s1, s2, s3;
  f64 spare; // the cached second Box-Muller sample
  b32 has_spare;
} CodRng;

force_inline u32 cod_rng_rotl(u32 x, u32 k) { return (x << k) | (x >> (32u - k)); }

// one SplitMix32 output
force_inline u32 cod_splitmix32(u32 *z) {
  *z += 0x9e3779b9u;
  u32 x = *z;
  x = (x ^ (x >> 16)) * 0x21f0aaadu;
  x = (x ^ (x >> 15)) * 0x735a2d97u;
  return x ^ (x >> 15);
}

// SplitMix32 spreads one seed over the four state words
force_inline CodRng cod_rng_make(u32 seed) {
  CodRng r = {0};
  u32 z = seed;
  r.s0 = cod_splitmix32(&z);
  r.s1 = cod_splitmix32(&z);
  r.s2 = cod_splitmix32(&z);
  r.s3 = cod_splitmix32(&z);
  return r;
}

// uniform u32
force_inline u32 cod_rng_u32(CodRng *r) {
  u32 result = cod_rng_rotl(r->s1 * 5u, 7) * 9u;
  u32 t = r->s1 << 9;
  r->s2 ^= r->s0;
  r->s3 ^= r->s1;
  r->s1 ^= r->s2;
  r->s0 ^= r->s3;
  r->s2 ^= t;
  r->s3 = cod_rng_rotl(r->s3, 11);
  return result;
}

// uniform [0,1) in double precision, as JS computes it
force_inline f64 cod_rng_f64(CodRng *r) { return (f64)cod_rng_u32(r) / 4294967296.0; }

// uniform [0,1)
force_inline f32 cod_rng_float(CodRng *r) { return (f32)cod_rng_f64(r); }

// uniform [min,max)
force_inline f32 cod_rng_range(CodRng *r, f32 min, f32 max) { return (f32)((f64)min + ((f64)max - (f64)min) * cod_rng_f64(r)); }

// uniform integer [min,max]
force_inline i32 cod_rng_int(CodRng *r, i32 min, i32 max) { return min + (i32)(cod_rng_u32(r) % (u32)(max - min + 1)); }

// uniform [-1,1]
force_inline f32 cod_rng_signed(CodRng *r) { return (f32)(cod_rng_f64(r) * 2.0 - 1.0); }

// true with probability p
force_inline b32 cod_rng_chance(CodRng *r, f32 p) { return cod_rng_f64(r) < (f64)p; }

// standard normal, Box-Muller; the pair's second sample is cached
force_inline f32 cod_rng_gauss(CodRng *r) {
  if (r->has_spare) {
    r->has_spare = false;
    return (f32)r->spare;
  }
  f64 u = 0.0;
  while (u == 0.0)
    u = cod_rng_f64(r);
  f64 rad = m_sqrt(-2.0 * m_log(u));
  f64 th = 2.0 * M_PI * cod_rng_f64(r);
  r->spare = rad * m_sin(th);
  r->has_spare = true;
  return (f32)(rad * m_cos(th));
}

// an index into an array of `count`
force_inline u32 cod_rng_pick(CodRng *r, u32 count) { return cod_rng_u32(r) % count; }

// a point uniformly inside the unit disc
force_inline v2 cod_rng_disc(CodRng *r) {
  f64 rad = m_sqrt(cod_rng_f64(r));
  f64 a = cod_rng_f64(r) * M_PI * 2.0;
  return (v2){(f32)(m_cos(a) * rad), (f32)(m_sin(a) * rad)};
}

// an independent stream derived from this one
force_inline CodRng cod_rng_fork(CodRng *r) { return cod_rng_make(cod_rng_u32(r)); }
