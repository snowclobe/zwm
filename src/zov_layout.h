/* Hand-written mirror of the Rust FFI boundary in
 * rust/zovwm-layout/src/lib.rs. Keep these two in sync manually: any change
 * to the Rust `#[repr(C)]` struct or `extern "C"` signature must be
 * reflected here too. */
#ifndef ZOVWM_ZOV_LAYOUT_H
#define ZOVWM_ZOV_LAYOUT_H

#include <stdint.h>

typedef struct ZovRect {
	int32_t x, y, w, h;
} ZovRect;

/* Computes up to `n` window rectangles for a master-stack tiling layout and
 * writes them into `out` (capacity `out_cap` entries). Returns the number of
 * rectangles actually written (min(n, out_cap)). Pure function, no side
 * effects, safe to call with out == NULL only when out_cap == 0. */
uint32_t zov_layout_master_stack(uint32_t n,
                                  int32_t screen_x, int32_t screen_y,
                                  int32_t screen_w, int32_t screen_h,
                                  int32_t gap, float master_ratio,
                                  uint32_t nmaster,
                                  ZovRect *out, uint32_t out_cap);

/* Grid layout: windows arranged in ceil(sqrt(n)) columns, each column
 * stacking as many rows as needed so every window gets exactly one cell. */
uint32_t zov_layout_grid(uint32_t n,
                          int32_t screen_x, int32_t screen_y,
                          int32_t screen_w, int32_t screen_h,
                          int32_t gap,
                          ZovRect *out, uint32_t out_cap);

#endif /* ZOVWM_ZOV_LAYOUT_H */
