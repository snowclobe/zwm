//! Pure tiling-layout geometry (bstack, grid), called from the C core over
//! FFI. No X11, no I/O — just arithmetic, so it is fully unit-testable
//! off-target.

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct ZovRect {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

/// Stacks `count` windows vertically inside (x, y, w, h), separated by `gap`,
/// dividing the height as evenly as integer math allows.
fn layout_column(x: i32, y: i32, w: i32, h: i32, gap: i32, count: usize) -> Vec<ZovRect> {
    if count == 0 || w <= 0 {
        return Vec::new();
    }
    let total_gap = gap * (count as i32 - 1);
    let avail_h = (h - total_gap).max(0);
    let base_h = avail_h / count as i32;
    let mut remainder = avail_h - base_h * count as i32;

    let mut rects = Vec::with_capacity(count);
    let mut cur_y = y;
    for _ in 0..count {
        let mut this_h = base_h;
        if remainder > 0 {
            this_h += 1;
            remainder -= 1;
        }
        rects.push(ZovRect { x, y: cur_y, w, h: this_h });
        cur_y += this_h + gap;
    }
    rects
}

/// Slices `count` windows side by side inside (x, y, w, h), separated by
/// `gap`, dividing the width as evenly as integer math allows.
fn layout_row(x: i32, y: i32, w: i32, h: i32, gap: i32, count: usize) -> Vec<ZovRect> {
    if count == 0 || h <= 0 {
        return Vec::new();
    }
    let total_gap = gap * (count as i32 - 1);
    let avail_w = (w - total_gap).max(0);
    let base_w = avail_w / count as i32;
    let mut remainder = avail_w - base_w * count as i32;

    let mut rects = Vec::with_capacity(count);
    let mut cur_x = x;
    for _ in 0..count {
        let mut this_w = base_w;
        if remainder > 0 {
            this_w += 1;
            remainder -= 1;
        }
        rects.push(ZovRect { x: cur_x, y, w: this_w, h });
        cur_x += this_w + gap;
    }
    rects
}

/// Computes window rectangles for a bottom-stack layout: up to `nmaster`
/// windows form a top row sized by `master_ratio` of the usable height, the
/// rest stack side by side in a row below (a left/right master-stack split
/// transposed to top/bottom).
pub fn compute_bstack(
    n: usize,
    screen_x: i32,
    screen_y: i32,
    screen_w: i32,
    screen_h: i32,
    gap: i32,
    master_ratio: f32,
    nmaster: usize,
) -> Vec<ZovRect> {
    if n == 0 {
        return Vec::new();
    }
    let gap = gap.max(0);
    let ratio = master_ratio.clamp(0.05, 0.95);

    let outer_x = screen_x + gap;
    let outer_y = screen_y + gap;
    let outer_w = (screen_w - 2 * gap).max(0);
    let outer_h = (screen_h - 2 * gap).max(0);

    let master_count = nmaster.min(n);
    let stack_count = n - master_count;

    if stack_count == 0 {
        return layout_row(outer_x, outer_y, outer_w, outer_h, gap, master_count);
    }
    if master_count == 0 {
        return layout_row(outer_x, outer_y, outer_w, outer_h, gap, stack_count);
    }

    let master_h = (((outer_h - gap).max(0) as f32) * ratio).round() as i32;
    let master_h = master_h.clamp(0, outer_h);
    let stack_y = outer_y + master_h + gap;
    let stack_h = (outer_h - master_h - gap).max(0);

    let mut rects = layout_row(outer_x, outer_y, outer_w, master_h, gap, master_count);
    rects.extend(layout_row(outer_x, stack_y, outer_w, stack_h, gap, stack_count));
    rects
}

/// Computes window rectangles for a grid layout (monsterwm/frankenwm-style):
/// windows are arranged in `cols` = ceil(sqrt(n)) columns of roughly equal
/// width, each column stacking as many rows as needed so every window gets
/// exactly one slot. `gap` is applied around the screen edge and between
/// cells.
pub fn compute_grid(
    n: usize,
    screen_x: i32,
    screen_y: i32,
    screen_w: i32,
    screen_h: i32,
    gap: i32,
) -> Vec<ZovRect> {
    if n == 0 {
        return Vec::new();
    }
    let gap = gap.max(0);
    let outer_x = screen_x + gap;
    let outer_y = screen_y + gap;
    let outer_w = (screen_w - 2 * gap).max(0);
    let outer_h = (screen_h - 2 * gap).max(0);

    let mut cols = 1usize;
    while cols * cols < n {
        cols += 1;
    }
    if n == 5 {
        cols = 2; // a 2x3 grid reads better than 3x2 for exactly 5 windows
    }

    let base_rows = n / cols;
    let extra = n % cols;

    let col_slices = layout_row(outer_x, outer_y, outer_w, outer_h, gap, cols);
    let mut rects = Vec::with_capacity(n);
    for (i, slice) in col_slices.iter().enumerate() {
        let rows_in_col = base_rows + if i < extra { 1 } else { 0 };
        rects.extend(layout_column(slice.x, outer_y, slice.w, outer_h, gap, rows_in_col));
    }
    rects
}

/// C ABI entry point. Writes up to `out_cap` rects into `out` and returns the
/// number actually written.
///
/// # Safety
/// `out` must be a valid pointer to at least `out_cap` writable `ZovRect`
/// slots, or null iff `out_cap` is 0.
#[no_mangle]
pub extern "C" fn zov_layout_grid(
    n: u32,
    screen_x: i32,
    screen_y: i32,
    screen_w: i32,
    screen_h: i32,
    gap: i32,
    out: *mut ZovRect,
    out_cap: u32,
) -> u32 {
    if out.is_null() || out_cap == 0 || n == 0 {
        return 0;
    }
    let rects = compute_grid(n as usize, screen_x, screen_y, screen_w, screen_h, gap);
    let write_n = rects.len().min(out_cap as usize);
    // SAFETY: caller contract above guarantees `out` has room for `out_cap`
    // slots, and write_n <= out_cap.
    let slice = unsafe { std::slice::from_raw_parts_mut(out, write_n) };
    slice.copy_from_slice(&rects[..write_n]);
    write_n as u32
}

/// C ABI entry point. Writes up to `out_cap` rects into `out` and returns the
/// number actually written.
///
/// # Safety
/// `out` must be a valid pointer to at least `out_cap` writable `ZovRect`
/// slots, or null iff `out_cap` is 0.
#[no_mangle]
pub extern "C" fn zov_layout_bstack(
    n: u32,
    screen_x: i32,
    screen_y: i32,
    screen_w: i32,
    screen_h: i32,
    gap: i32,
    master_ratio: f32,
    nmaster: u32,
    out: *mut ZovRect,
    out_cap: u32,
) -> u32 {
    if out.is_null() || out_cap == 0 || n == 0 {
        return 0;
    }
    let rects = compute_bstack(
        n as usize,
        screen_x,
        screen_y,
        screen_w,
        screen_h,
        gap,
        master_ratio,
        nmaster as usize,
    );
    let write_n = rects.len().min(out_cap as usize);
    // SAFETY: caller contract above guarantees `out` has room for `out_cap`
    // slots, and write_n <= out_cap.
    let slice = unsafe { std::slice::from_raw_parts_mut(out, write_n) };
    slice.copy_from_slice(&rects[..write_n]);
    write_n as u32
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bstack_single_window_fills_screen_minus_gap() {
        let r = compute_bstack(1, 0, 0, 1920, 1080, 10, 0.55, 1);
        assert_eq!(r, vec![ZovRect { x: 10, y: 10, w: 1900, h: 1060 }]);
    }

    #[test]
    fn bstack_stack_row_divides_width_evenly() {
        // nmaster = 0 so all 4 windows land in the single (stack) row.
        let r = compute_bstack(4, 0, 0, 1000, 1000, 0, 0.5, 0);
        assert_eq!(r.len(), 4);
        let widths: Vec<i32> = r.iter().map(|x| x.w).collect();
        assert_eq!(widths.iter().sum::<i32>(), 1000);
        assert!(widths.iter().max().unwrap() - widths.iter().min().unwrap() <= 1);
    }

    #[test]
    fn bstack_master_ratio_moves_row_border() {
        let short = compute_bstack(2, 0, 0, 1000, 1000, 0, 0.3, 1);
        let tall = compute_bstack(2, 0, 0, 1000, 1000, 0, 0.7, 1);
        assert!(short[0].h < tall[0].h);
        // Second (stack) window's y must move down along with a taller master.
        assert!(short[1].y < tall[1].y);
    }

    #[test]
    fn bstack_keeps_windows_within_screen_bounds() {
        let sx = 100;
        let sy = 50;
        let sw = 1280;
        let sh = 800;
        let gap = 16;
        for n in 1..=6usize {
            let rects = compute_bstack(n, sx, sy, sw, sh, gap, 0.55, 1);
            assert_eq!(rects.len(), n);
            for rect in &rects {
                assert!(rect.x >= sx + gap);
                assert!(rect.y >= sy + gap);
                assert!(rect.x + rect.w <= sx + sw - gap);
                assert!(rect.y + rect.h <= sy + sh - gap);
                assert!(rect.w >= 0 && rect.h >= 0);
            }
        }
    }

    #[test]
    fn grid_covers_every_window_exactly_once() {
        for n in 1..=9usize {
            let rects = compute_grid(n, 0, 0, 1200, 900, 8);
            assert_eq!(rects.len(), n, "n={n}");
        }
    }

    #[test]
    fn grid_four_windows_is_2x2() {
        let r = compute_grid(4, 0, 0, 1000, 1000, 0);
        assert_eq!(r.len(), 4);
        // Two distinct x's, two distinct y's, each shared by exactly two cells.
        let xs: std::collections::HashSet<i32> = r.iter().map(|c| c.x).collect();
        let ys: std::collections::HashSet<i32> = r.iter().map(|c| c.y).collect();
        assert_eq!(xs.len(), 2);
        assert_eq!(ys.len(), 2);
    }

    #[test]
    fn grid_keeps_cells_within_screen_bounds() {
        let sx = 100;
        let sy = 50;
        let sw = 1280;
        let sh = 800;
        let gap = 12;
        for n in 1..=9usize {
            let rects = compute_grid(n, sx, sy, sw, sh, gap);
            for rect in &rects {
                assert!(rect.x >= sx + gap);
                assert!(rect.y >= sy + gap);
                assert!(rect.x + rect.w <= sx + sw - gap);
                assert!(rect.y + rect.h <= sy + sh - gap);
                assert!(rect.w >= 0 && rect.h >= 0);
            }
        }
    }

    #[test]
    fn ffi_writes_expected_count_and_respects_capacity() {
        let mut buf = [ZovRect::default(); 3];
        let written = zov_layout_bstack(3, 0, 0, 900, 600, 5, 0.6, 1, buf.as_mut_ptr(), buf.len() as u32);
        assert_eq!(written, 3);

        // Capacity smaller than n: must not overflow the buffer.
        let mut small = [ZovRect::default(); 1];
        let written = zov_layout_bstack(3, 0, 0, 900, 600, 5, 0.6, 1, small.as_mut_ptr(), small.len() as u32);
        assert_eq!(written, 1);
    }
}
