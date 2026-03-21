//#define FPGA

#ifndef FPGA
#include <stdio.h>
#include <stdint.h>
#include <SDL2/SDL.h>

#include <time.h>

static uint64_t ts_ns()
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (uint64_t)ts.tv_sec * 1000000000LL + (uint64_t)ts.tv_nsec;
}

static struct {
	const char *str;
	uint64_t diff;
	uint64_t cnt;
} perf[65536];

#define MEASURE(...) \
	do { \
		uint64_t t1 = ts_ns(); \
		do { \
			__VA_ARGS__; \
		} while (0); \
		uint64_t t2 = ts_ns(); \
		perf[__LINE__].str = #__VA_ARGS__; \
		perf[__LINE__].diff += t2 - t1; \
		perf[__LINE__].cnt++; \
	} while (0)

static void dump_measures(void)
{
	printf("--------------------------------\n");
	for (int i = 0; i < 65536; i++) {
		if (perf[i].cnt) {
			printf("%.*s...: %li, %li\n", 30, perf[i].str, perf[i].cnt, perf[i].diff / perf[i].cnt);
		}
	}
	memset(perf, 0, sizeof(perf));
}

#else
#define MEASURE(...) __VA_ARGS__

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned int size_t;

#define NULL ((void *)0)

size_t strlen(const char *s)
{
	int i = 0;
	for (; s[i]; i++) {}
	return i;
}
#endif

#define H_RES 600
#define V_RES 1024

uint32_t fb[H_RES * V_RES];

static inline int MAX(int a, int b)
{
	return a > b ? a : b;
}

static inline int MIN(int a, int b)
{
	return a < b ? a : b;
}

static inline int ABS(int a)
{
	return a < 0 ? -a : a;
}

static inline int CLAMP(int a, int low, int high)
{
	return MIN(MAX(a, low), high);
}

struct Pair {
	union {
		struct {
			int x;
			int y;
		};
		struct {
			int w;
			int h;
		};
	};
};

static struct Pair pair_new(int x, int y)
{
	struct Pair p = {
		.x = x,
		.y = y,
	};
	return p;
}

static struct Pair pair_shift(struct Pair p1, struct Pair p2)
{
	struct Pair p = {
		.x = p1.x + p2.x,
		.y = p1.y + p2.y,
	};
	return p;
}

static struct Pair pair_unshift(struct Pair p1, struct Pair p2)
{
	struct Pair p = {
		.x = p1.x - p2.x,
		.y = p1.y - p2.y,
	};
	return p;
}

static int size_valid(struct Pair p)
{
	return p.w > 0 && p.h > 0;
}

struct Rect {
	union {
		struct {
			int x;
			int y;
		};
		struct Pair p;
	};
	union {
		struct {
			int w;
			int h;
		};
		struct Pair s;
	};
};

static struct Rect rect_new(int x, int y, int w, int h)
{
	struct Rect rect = {
		.x = x,
		.y = y,
		.w = w,
		.h = h,
	};
	return rect;
}

static struct Rect rect_move(struct Rect r, struct Pair p)
{
	r.x += p.x;
	r.y += p.y;
	return r;
}

static struct Rect rect_intersect(struct Rect s, struct Rect r)
{
	struct Rect rect = {
		.x = MAX(s.x, r.x),
		.y = MAX(s.y, r.y),
	};
	rect.w = MIN(s.x + s.w, r.x + r.w) - rect.x;
	rect.h = MIN(s.y + s.h, r.y + r.h) - rect.y;
	return rect;
}

static int rect_valid(struct Rect r)
{
	return size_valid(r.s);
}

static int rect_hit(struct Rect r, struct Pair p)
{
	return r.x <= p.x && p.x < (r.x + r.w) && r.y <= p.y && p.y < (r.y + r.h);
}

static int rect_equal(struct Rect r1, struct Rect r2)
{
	return memcmp(&r1, &r2, sizeof(struct Rect)) == 0;
}

static void rect_dump(struct Rect r)
{
#ifndef FPGA
	printf("%i, %i, %i, %i\n", r.x, r.y, r.w, r.h);
#endif
}

static void draw_pixel_check(struct Rect r, struct Pair p, uint32_t color)
{
	if (!rect_hit(r, p)) {
		return;
	}

	fb[p.y * H_RES + p.x] = color;
}

static void draw_rect(struct Rect r, uint32_t color)
{
	if (!rect_valid(r)) {
		return;
	}

	for (int i = r.y; i < r.y + r.h; i++) {
		for (int j = r.x; j < r.x + r.w; j++) {
			fb[i * H_RES + j] = color;
		}
	}
}

static void draw_rect_gradient(struct Rect r, struct Rect s, uint32_t color_start, uint32_t color_end, int horizontal)
{
	if (!rect_valid(r)) {
		return;
	}

	uint8_t cs_r = (color_start >> 16) & 0xff;
	uint8_t cs_g = (color_start >> 8) & 0xff;
	uint8_t cs_b = (color_start >> 0) & 0xff;
	uint8_t ce_r = (color_end >> 16) & 0xff;
	uint8_t ce_g = (color_end >> 8) & 0xff;
	uint8_t ce_b = (color_end >> 0) & 0xff;

	int len_r = ABS(ce_r - cs_r);
	int len_g = ABS(ce_g - cs_g);
	int len_b = ABS(ce_b - cs_b);

	int len = horizontal ? r.w : r.h;

	int slop_r = (len_r + len - 1) / len;
	int diff_r = slop_r * len;
	int slop_g = (len_g + len - 1) / len;
	int diff_g = slop_g * len;
	int slop_b = (len_b + len - 1) / len;
	int diff_b = slop_b * len;

	if (ce_r < cs_r) {
		slop_r = -slop_r;
	}
	if (ce_g < cs_g) {
		slop_g = -slop_g;
	}
	if (ce_b < cs_b) {
		slop_b = -slop_b;
	}

	if (horizontal) {
		for (int i = r.y; i < r.y + r.h; i++) {
			int32_t cs_r2 = cs_r;
			int32_t cs_g2 = cs_g;
			int32_t cs_b2 = cs_b;
			int cnt_r = 0;
			int cnt_g = 0;
			int cnt_b = 0;

			for (int j = r.x; j < r.x + r.w; j++) {
				uint32_t color = (cs_r2 << 16) | (cs_g2 << 8) | (cs_b2 << 0);

				draw_pixel_check(s, pair_new(j, i), color);

				cnt_r += len_r;
				if (cnt_r >= len) {
					cnt_r -= diff_r;
					cs_r2 = CLAMP(cs_r2 + slop_r, 0, 255);
				}
				cnt_g += len_g;
				if (cnt_g >= len) {
					cnt_g -= diff_g;
					cs_g2 = CLAMP(cs_g2 + slop_g, 0, 255);
				}
				cnt_b += len_b;
				if (cnt_b >= len) {
					cnt_b -= diff_b;
					cs_b2 = CLAMP(cs_b2 + slop_b, 0, 255);
				}
			}
		}
	} else {
		int32_t cs_r2 = cs_r;
		int32_t cs_g2 = cs_g;
		int32_t cs_b2 = cs_b;
		int cnt_r = 0;
		int cnt_g = 0;
		int cnt_b = 0;

		for (int i = r.y; i < r.y + r.h; i++) {
			uint32_t color = (cs_r2 << 16) | (cs_g2 << 8) | (cs_b2 << 0);

			for (int j = r.x; j < r.x + r.w; j++) {
				draw_pixel_check(s, pair_new(j, i), color);
			}

			cnt_r += len_r;
			if (cnt_r >= len) {
				cnt_r -= diff_r;
				cs_r2 = CLAMP(cs_r2 + slop_r, 0, 255);
			}
			cnt_g += len_g;
			if (cnt_g >= len) {
				cnt_g -= diff_g;
				cs_g2 = CLAMP(cs_g2 + slop_g, 0, 255);
			}
			cnt_b += len_b;
			if (cnt_b >= len) {
				cnt_b -= diff_b;
				cs_b2 = CLAMP(cs_b2 + slop_b, 0, 255);
			}
		}
	}
}

static void draw_circle(struct Rect vp, struct Pair c, int r, uint32_t color, int smooth)
{
	int t1 = r / 2;
	int x = r;
	int y = 0;
	int t2 = t1 - x;

	while (x >= y) {
		uint32_t intens = color;

		if (smooth) {
			intens = (r - t1) * 255 / r;
			intens = intens | (intens << 8) | (intens << 16);

			for (int i = -x; i <= x; i++) {
				draw_pixel_check(vp, pair_new(c.x + i, c.y + y), color);
				draw_pixel_check(vp, pair_new(c.x + i, c.y - y), color);
			}

			if (t2 >= 0) {
				for (int i = -y; i <= y; i++) {
					draw_pixel_check(vp, pair_new(c.x + i, c.y + x), color);
					draw_pixel_check(vp, pair_new(c.x + i, c.y - x), color);
				}
			}
		}

		draw_pixel_check(vp, pair_new(c.x + x, c.y + y), intens);
		draw_pixel_check(vp, pair_new(c.x + x, c.y - y), intens);
		draw_pixel_check(vp, pair_new(c.x - x, c.y + y), intens);
		draw_pixel_check(vp, pair_new(c.x - x, c.y - y), intens);
		draw_pixel_check(vp, pair_new(c.x + y, c.y + x), intens);
		draw_pixel_check(vp, pair_new(c.x + y, c.y - x), intens);
		draw_pixel_check(vp, pair_new(c.x - y, c.y + x), intens);
		draw_pixel_check(vp, pair_new(c.x - y, c.y - x), intens);

		y++;
		t1 += y;
		t2 = t1 - x;

		if (t2 >= 0) {
			t1 = t2;
			x--;
		}
	}
}

struct BitmapData {
	struct Pair bb;
	uint8_t *data;
};

struct BitmapClass {
	struct Pair size;
	struct BitmapData *bd;
};

/*
 * Bitmap - bitmask two-dimensial array, each horizontal line is aligned to 8 bits
 * r - rectangle in screen coordinates, width and height is resulted bitmap in pixels
 * s - scissor rectangle in screen coordinates
 */
static void draw_bitmap(struct Rect r, struct Rect s, uint32_t color, const uint8_t *bitmap)
{
	struct Rect offset = rect_move(rect_intersect(r, s), pair_new(-r.x, -r.y));

	if (!rect_valid(offset)) {
		return;
	}

	int w = (r.w + 7) / 8;

	for (int i = offset.y; i < offset.y + offset.h; i++) {
		for (int j = offset.x; j < offset.x + offset.w; j++) {
			int idx = i * w * 8 + j;

			if (bitmap[idx / 8] & ((uint8_t)0x80 >> (idx % 8))) {
				struct Pair p = pair_new(r.x + j, r.y + i);

				fb[p.y * H_RES + p.x] = color;
			}
		}
	}
}

static void draw_bitmap2(struct Pair pvp, struct Rect svp, struct BitmapClass *font, int index, uint32_t color)
{
	struct BitmapData *bd = &font->bd[index];
	struct Rect offset = rect_intersect(rect_new(pvp.x, pvp.y, bd->bb.w, bd->bb.h), svp);

	if (!rect_valid(offset)) {
		return;
	}

	offset = rect_move(offset, pair_new(-pvp.x, -pvp.y));

	int wi = (bd->bb.w + 7) / 8;

	for (int i = offset.y; i < offset.y + offset.h; i++) {
		for (int j = offset.x; j < offset.x + offset.w; j++) {
			int idx = i * wi * 8 + j;

			if (bd->data[idx / 8] & ((uint8_t)0x80 >> (idx % 8))) {
				struct Pair p = pair_new(pvp.x + j, pvp.y + i);

				fb[p.y * H_RES + p.x] = color;
			}
		}
	}
}

static void draw_shadow(struct Rect r, int cnt, int skip, uint32_t color)
{
	if (!rect_valid(r)) {
		return;
	}

	int cy = cnt;

	for (int i = r.y; i < r.y + r.h; i++) {
		int cx = cnt;

		for (int j = r.x; j < r.x + r.w; j++) {
			fb[i * H_RES + j] = color;

			cx--;
			if (cx == 0) {
				cx = cnt;
				j += skip;
			}
		}

		cy--;
		if (cy == 0) {
			cy = cnt;
			i += skip;
		}
	}
}

#include "font.h"

static void draw_text(struct Rect r, struct Rect s, uint32_t color, const char *text)
{
	for (int k = 0; text[k]; k++) {
		int idx = (int)text[k] * FONT_H * FONT_W / 8;

		draw_bitmap(rect_new(r.x + k * FONT_W, r.y, FONT_W, FONT_H), s, color, &font[idx]);
	}
}

#include "icon.h"

enum EventCursorType {
	EV_CURSOR_DOWN,
	EV_CURSOR_MOVE,
	EV_CURSOR_UP,
};

enum EventActivateType {
	EV_ACTIVATE,
	EV_DEACTIVATE,
	EV_CANCEL,
};

#define INHERIT_PARENT -1
#define INHERIT_CHILD -2

struct Base {
	struct Base *parent;

	struct Rect vp;
	struct Rect svp;

	/* Parent may ask the element about its sizes.
	 * You can only return INHERIT_PARENT or >= 0. */
	int (*get_width)(struct Base *obj);
	int (*get_height)(struct Base *obj);

	/* Parent assigns you the vp,
	 * you just need to layout everything you want into the vp.
	 */
	void (*layout)(struct Base *obj, struct Pair size);
	void (*layout_children)(struct Base *obj);

	/* vp is your vp but in absolute coordinates.
	 * Please, respect the svp and dirty rect.
	 */
	void (*render)(struct Base *obj, struct Rect svp);

	/* Call before you are going to change the element.
	 * Goes up to the parent, the parent sets the dirty region,
	 * and sets valid to 0.
	 */
	int (*invalidate)(struct Base *obj);

	/* Return something if point p is inside yours svp */
	struct Base *(*pick)(struct Base *obj, struct Pair p);

	void (*handle_cursor_event)(struct Base *base, enum EventCursorType type, struct Pair p);
	void (*handle_activate_event)(struct Base *base, enum EventActivateType type);

	struct Base *next;

	struct Base *rerender_next;
	struct Rect dirty;
};

static struct Base *relayout;
static struct Base *rerender;
static struct Base *picked;

static int layout_cnt;
static int layout_enter_cnt;
static int render_cnt;
static int render_skip_cnt;

/* Helper function for parents */
static void ui_layout_child(struct Base *base, struct Base *child)
{
	layout_enter_cnt++;

	child->parent = base;

	if (base) {
		if (!rect_valid(base->vp)) {
			return;
		}
	}

	if (!rect_valid(child->vp)) {
		return;
	}

	if (child->layout) {
		child->layout(child, child->vp.s);
	}

	layout_cnt++;

	if (child->layout_children) {
		child->layout_children(child);
	}
}

static void ui_process(struct Base *root)
{
	while (relayout) {
		struct Base *base = relayout;

		relayout = base->next;
		base->next = NULL;

		ui_layout_child(base->parent, base);
	}

	while (rerender) {
		struct Base *base = rerender;

		rerender = base->rerender_next;

		draw_rect(base->dirty, 0x01579b);
		if (!rect_equal(base->dirty, base->svp)) {
			draw_rect(base->svp, 0x01579b);
		}

		root->render(root, base->dirty);
		if (!rect_equal(base->dirty, base->svp)) {
			root->render(root, base->svp);
		}

		base->rerender_next = NULL;
	}
}

static void ui_update_dirty(struct Base *base)
{
	if (!rect_valid(base->svp)) {
		return;
	}

	base->dirty = base->svp;

	if (base->rerender_next || rerender == base) {
		return;
	}

	base->rerender_next = rerender;
	rerender = base;
}

static void ui_set_vp(struct Base *base, struct Base *child, struct Rect vp)
{
	struct Rect avp_new = rect_move(vp, base->vp.p);
	struct Rect svp_new = rect_intersect(avp_new, base->svp);

	if (!rect_valid(avp_new) || !rect_valid(svp_new)) {
		memset(&child->vp, 0, sizeof(child->vp));
		memset(&child->svp, 0, sizeof(child->svp));
		return;
	}

	child->vp = avp_new;
	child->svp = svp_new;
}

static void ui_invalidate(struct Base *base)
{
	struct Base *last = base;

	ui_update_dirty(base);

	while (base) {
		last = base;

		if (base->invalidate) {
			if (!base->invalidate(base)) {
				break;
			}
		}

		base = base->parent;
	}

	last->next = relayout;
	relayout = last;
}

static void ui_render(struct Base *base, struct Rect svp)
{
	svp = rect_intersect(svp, base->svp);

	if (!rect_valid(svp)) {
		render_skip_cnt++;
		return;
	}

	base->render(base, svp);
	render_cnt++;
}

static struct Base *ui_pick_child(struct Base *base, struct Pair p)
{
	/* Don't want to be picked */
	if (!base->pick) {
		return NULL;
	}

	if (!rect_valid(base->svp)) {
		return NULL;
	}

	if (!rect_hit(base->svp, p)) {
		return NULL;
	}

	return base->pick(base, p);
}

static void ui_handle_cursor_event(struct Base *base, enum EventCursorType type, struct Pair p)
{
	if (!base->handle_cursor_event) {
		return;
	}

	base->handle_cursor_event(base, type, pair_unshift(p, base->vp.p));
}

static void ui_handle_activate_event(struct Base *base, enum EventActivateType type)
{
	if (!base->handle_activate_event) {
		return;
	}

	base->handle_activate_event(base, type);
}

static int ui_get_width(struct Base *base)
{
	return base->get_width(base);
}

static int ui_get_height(struct Base *base)
{
	return base->get_height(base);
}

static struct Base *ui_dummy_pick(struct Base *base, struct Pair p)
{
	return base;
}

static int ui_dummy_inherit_parent(struct Base *base)
{
	return INHERIT_PARENT;
}

static void ui_cursor_down(struct Base *base, struct Pair p)
{
	struct Base *new_pick;

	new_pick = ui_pick_child(base, p);

	if (picked && picked != new_pick) {
		ui_handle_activate_event(picked, EV_DEACTIVATE);
	}

	picked = new_pick;

	if (picked) {
		ui_handle_cursor_event(picked, EV_CURSOR_DOWN, p);
	}
}

static void ui_cursor_move(struct Pair p)
{
	if (!picked) {
		return;
	}

	ui_handle_cursor_event(picked, EV_CURSOR_MOVE, p);
}

static void ui_cursor_up(struct Base *base, struct Pair p)
{
	if (!picked) {
		return;
	}

	ui_handle_cursor_event(picked, EV_CURSOR_UP, p);

	if (picked == ui_pick_child(base, p)) {
		ui_handle_activate_event(picked, EV_ACTIVATE);
	} else {
		ui_handle_activate_event(picked, EV_CANCEL);
	}

	picked = NULL;
}

enum Align {
	ALIGN_BEGIN,
	ALIGN_MIDDLE,
	ALIGN_END,
};

static struct Pair ui_align(struct Pair size, struct Pair content_size, enum Align h_align, enum Align v_align)
{
	struct Pair ret = {};

	if (h_align) {
		int offset = size.w - content_size.w;

		if (h_align == ALIGN_MIDDLE) {
			offset /= 2;
		}

		ret.x = offset;
	}

	if (v_align) {
		int offset = size.h - content_size.h;

		if (v_align == ALIGN_MIDDLE) {
			offset /= 2;
		}

		ret.y = offset;
	}

	return ret;
}

struct Box {
	struct Base base;

	/* == 0 - disabled
	 * > 0 - exact
	 * INHERIT_PARENT - give me something
	 * INHERIT_CHILD - my content is my size
	 *
	 * If INHERIT_CHILD is set, but content reports INHERIT_PARENT,
	 * then we will report INHERIT_PARENT too.
	 */
	int width;
	int height;

	/* Content placement inside the width/height */
	enum Align h_align;
	enum Align v_align;

	uint32_t color;

	struct Base *content;
};

enum UIButtonType {
	UI_BUTTON_NORMAL,
	UI_BUTTON_SWITCH,
	UI_BUTTON_STICK,
};

struct Glyph {
	struct Base base;

	uint32_t bg_color;

	struct BitmapClass *bc;
	int index;
	uint32_t color;
};

struct Bitmap {
	struct Base base;

	uint32_t bg_color;

	enum Align h_align;
	enum Align v_align;

	struct BitmapClass *bc;
	int index;
	uint32_t color;

	struct Pair offset;
};

struct ButtonBitmap {
	struct Bitmap super;

	enum UIButtonType type;

	int activated;

	void (*on_activate)(struct Base *base);
};

struct Text {
	struct Base base;

	uint8_t *font;
	const char *text;
	uint32_t color;
};

struct ColorBox {
	struct Base base;

	int width;
	int height;
	uint32_t color;

	void (*on_click)(struct ColorBox *obj);
};

struct GradientBox {
	struct ColorBox super;

	int horizontal;
	uint32_t color_end;
};

struct List {
	struct Base base;

	int horizontal;
	int space;

	int children;
	struct Base **child;
};

struct Slider {
	struct Base base;

	int horizontal;
	int min;
	int max;
	int value;

	int len;
	int frac;

	struct Base *knob;
	struct Base *line;

	int ks;
	int split;

	void (*on_changed)(int value);
};

struct BoxExpander {
	struct Box super;

	int width;
	int height;
};

struct Circle {
	struct Base base;

	int radius;
	uint32_t color;
};

static int ui_box_get_width(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	switch (obj->width) {
	case INHERIT_PARENT:
		return INHERIT_PARENT;
	case INHERIT_CHILD:
		int width = ui_get_width(obj->content);

		if (width < 0) {
			return INHERIT_PARENT;
		}

		return width;
	default:
		return obj->width;
	}
}

static int ui_box_get_height(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	switch (obj->height) {
	case INHERIT_PARENT:
		return INHERIT_PARENT;
	case INHERIT_CHILD:
		int height = ui_get_height(obj->content);

		if (height < 0) {
			return INHERIT_PARENT;
		}

		return height;
	default:
		return obj->height;
	}
}

static void ui_box_layout(struct Base *base, struct Pair size)
{
	struct Box *obj = (struct Box *)base;
	struct Rect vp = rect_new(0, 0, ui_get_width(obj->content), ui_get_height(obj->content));

	if (vp.w < 0) {
		vp.w = size.w;
	}

	if (vp.h < 0) {
		vp.h = size.h;
	}

	ui_set_vp(base, obj->content, rect_move(vp, ui_align(size, vp.s, obj->h_align, obj->v_align)));
}

static void ui_box_layout_children(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	ui_layout_child(base, obj->content);
}

static int ui_box_invalidate(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	if (obj->width == INHERIT_CHILD) {
		if (ui_get_width(obj->content) != base->vp.w) {
			return 1;
		}
	}

	if (obj->height == INHERIT_CHILD) {
		if (ui_get_height(obj->content) != base->vp.h) {
			return 1;
		}
	}

	return 0;
}

static void ui_box_render(struct Base *base, struct Rect svp)
{
	struct Box *obj = (struct Box *)base;

	draw_rect(svp, obj->color);

	ui_render(obj->content, svp);
}

static struct Base *ui_box_pick(struct Base *base, struct Pair p)
{
	struct Box *obj = (struct Box *)base;

	return ui_pick_child(obj->content, p);
}

static int ui_color_box_get_width(struct Base *base)
{
	struct ColorBox *obj = (struct ColorBox *)base;

	return obj->width;
}

static int ui_color_box_get_height(struct Base *base)
{
	struct ColorBox *obj = (struct ColorBox *)base;

	return obj->height;
}

static void ui_color_box_render(struct Base *base, struct Rect svp)
{
	struct ColorBox *obj = (struct ColorBox *)base;

	draw_rect(svp, obj->color);
}

static void ui_color_box_handle_activate_event(struct Base *base, enum EventActivateType type)
{
	struct ColorBox *obj = (struct ColorBox *)base;

	switch (type) {
	case EV_ACTIVATE:
		if (obj->on_click) {
			obj->on_click(obj);
		}
		break;
	case EV_DEACTIVATE:
		break;
	case EV_CANCEL:
		break;
	}
}

static void ui_color_gradient_render(struct Base *base, struct Rect svp)
{
	struct GradientBox *obj = (struct GradientBox *)base;

	draw_rect_gradient(base->vp, svp, obj->super.color, obj->color_end, obj->horizontal);
}

static int ui_glyph_get_width(struct Base *base)
{
	struct Glyph *obj = (struct Glyph *)base;
	struct BitmapData *bd = &obj->bc->bd[obj->index];

	return bd->bb.w;
}

static int ui_glyph_get_height(struct Base *base)
{
	struct Glyph *obj = (struct Glyph *)base;
	struct BitmapData *bd = &obj->bc->bd[obj->index];

	return bd->bb.h;
}

static void ui_glyph_render(struct Base *base, struct Rect svp)
{
	struct Glyph *obj = (struct Glyph *)base;

	draw_rect(svp, obj->bg_color);

	draw_bitmap2(base->vp.p, svp, obj->bc, obj->index, obj->color);
}

static int ui_bitmap_get_width(struct Base *base)
{
	struct Bitmap *obj = (struct Bitmap *)base;

	return obj->bc->size.w;
}

static int ui_bitmap_get_height(struct Base *base)
{
	struct Bitmap *obj = (struct Bitmap *)base;

	return obj->bc->size.h;
}

static void ui_bitmap_layout(struct Base *base, struct Pair size)
{
	struct Bitmap *obj = (struct Bitmap *)base;

	// FIXME Use normal child with ui_set_vp()
	obj->offset = ui_align(size, obj->bc->bd[obj->index].bb, obj->h_align, obj->v_align);
}

static void ui_bitmap_render(struct Base *base, struct Rect svp)
{
	struct Bitmap *obj = (struct Bitmap *)base;

	draw_rect(svp, obj->bg_color);

	draw_bitmap2(pair_shift(obj->offset, base->vp.p), svp, obj->bc, obj->index, obj->color);
}

static void ui_bitmap_handle_cursor_event(struct Base *base, enum EventCursorType type, struct Pair p)
{
	struct ButtonBitmap *obj = (struct ButtonBitmap *)base;

	switch (type) {
	case EV_CURSOR_DOWN:
		if (obj->activated) {
			break;
		}

		obj->super.bg_color = 0x4caf50;
		ui_update_dirty(base);
		break;
	default:
		break;
	}
}

static void ui_bitmap_handle_activate_event(struct Base *base, enum EventActivateType type)
{
	struct ButtonBitmap *obj = (struct ButtonBitmap *)base;

	switch (type) {
	case EV_ACTIVATE:
		switch (obj->type) {
		case UI_BUTTON_NORMAL:
			obj->super.bg_color = 0x01579b;

			obj->activated = 1;
			obj->on_activate(base);
			obj->activated = 0;
			break;
		case UI_BUTTON_SWITCH:
			if (obj->activated) {
				obj->super.bg_color = 0x01579b;

				obj->activated = 0;
				obj->on_activate(base);
			} else {
				obj->activated = 1;
				obj->on_activate(base);
			}
			break;
		case UI_BUTTON_STICK:
			obj->activated = 1;
			obj->on_activate(base);
			break;
		}
		break;
	case EV_CANCEL:
		if (obj->type == UI_BUTTON_NORMAL || !obj->activated) {
			obj->super.bg_color = 0x01579b;
			ui_update_dirty(base);
		}
		break;
	default:
		break;
	}
}

static int ui_text_get_width(struct Base *base)
{
	struct Text *obj = (struct Text *)base;

	return FONT_W * strlen(obj->text);
}

static int ui_text_get_height(struct Base *base)
{
	return FONT_H;
}

static void ui_text_render(struct Base *base, struct Rect svp)
{
	struct Text *obj = (struct Text *)base;

	draw_text(base->vp, svp, obj->color, obj->text);
}

static void ui_slider_layout(struct Base *base, struct Pair size)
{
	struct Slider *obj = (struct Slider *)base;
	int w = size.w;
	int h = size.h;
	int ks;
	int ls;
	int kw = ui_get_width(obj->knob);
	int kh = ui_get_height(obj->knob);

	if (obj->horizontal) {
		ks = kw;
		ls = ui_get_height(obj->line);

		obj->len = size.w;
	} else {
		ks = kh;
		ls = ui_get_width(obj->line);

		obj->len = size.h;
	}

	if (ks < 0) {
		if (obj->horizontal) {
			ks = h;
		} else {
			ks = w;
		}
	}

	if (ls < 0) {
		if (obj->horizontal) {
			ls = h;
		} else {
			ls = w;
		}
	}

	obj->len -= ks;
	/* So that we have convenient ranges 1: [1, 1.5], 2: [1.5, 2.5], ... */
	obj->frac = obj->len / (2 * (obj->max - obj->min));
	obj->ks = ks;

	{
		int x = 0;
		int y = 0;
		int w = size.w;
		int h = size.h;

		if (obj->horizontal) {
			x = (obj->value - obj->min) * obj->len / (obj->max - obj->min);
			w = ks;

			if (obj->split) {
				h = kh;

				if (obj->split > 0) {
					y += size.h - kh;
				}
			}
		} else {
			y = (obj->value - obj->min) * obj->len / (obj->max - obj->min);
			y = size.h - y - ks;
			h = ks;

			if (obj->split) {
				w = kw;

				if (obj->split > 0) {
					x += size.w - kw;
				}
			}
		}

		ui_set_vp(base, obj->knob, rect_new(x, y, w, h));
	}

	{
		int x = 0;
		int y = 0;
		int w = size.w;
		int h = size.h;

		if (obj->horizontal) {
			x += ks / 2;
			y = (h - ls) / 2;

			w = obj->len;
			/* The most right position of the knob is x = obj->len, so... */
			w++;

			if (obj->split) {
				h = size.h - kh;

				if (obj->split < 0) {
					y += kh;
				}
			} else {
				h = ls;
			}
		} else {
			x = (w - ls) / 2;
			y += ks / 2;

			if (obj->split) {
				w = size.w - kw;

				if (obj->split < 0) {
					x += kw;
				}
			} else {
				w = ls;
			}

			h = obj->len;
			/* The most right position of the knob is y = obj->len, so... */
			h++;
		}

		ui_set_vp(base, obj->line, rect_new(x, y, w, h));
	}
}

static void ui_slider_layout_children(struct Base *base)
{
	struct Slider *obj = (struct Slider *)base;

	ui_layout_child(base, obj->line);
	ui_layout_child(base, obj->knob);
}

static void ui_slider_render(struct Base *base, struct Rect svp)
{
	struct Slider *obj = (struct Slider *)base;

	ui_render(obj->line, svp);
	ui_render(obj->knob, svp);
}

static void ui_slider_handle_cursor_event(struct Base *base, enum EventCursorType type, struct Pair p)
{
	struct Slider *obj = (struct Slider *)base;
	int value;

	switch (type) {
	case EV_CURSOR_DOWN:
	case EV_CURSOR_MOVE:
		if (obj->horizontal) {
			value = obj->min + (obj->max - obj->min) * (p.x - obj->ks / 2 - obj->frac) / obj->len;
		} else {
			value = obj->min + (obj->max - obj->min) * (base->vp.h - obj->ks - (p.y - obj->ks / 2 - obj->frac)) / obj->len;
		}

		obj->value = CLAMP(value, obj->min, obj->max);

		obj->on_changed(obj->value);

		if (obj->ks) {
			ui_invalidate(base);
		}
		break;
	default:
		break;
	}
}

static int ui_list_get_width(struct Base *base)
{
	struct List *obj = (struct List *)base;

	if (obj->horizontal) {
		int w_max = 0;

		for (int i = 0; i < obj->children; i++) {
			struct Base *c = obj->child[i];
			int cw = c->get_width(c);

			if (cw < 0) {
				return cw;
			}

			w_max += cw;
		}

		return w_max + (obj->children - 1) * obj->space;
	} else {
		int w_max = INHERIT_PARENT;

		for (int i = 0; i < obj->children; i++) {
			struct Base *c = obj->child[i];
			int cw = c->get_width(c);

			if (cw > w_max) {
				w_max = cw;
			}
		}

		return w_max;
	}
}

static int ui_list_get_height(struct Base *base)
{
	struct List *obj = (struct List *)base;

	if (obj->horizontal) {
		int h_max = INHERIT_PARENT;

		for (int i = 0; i < obj->children; i++) {
			struct Base *c = obj->child[i];
			int ch = c->get_height(c);

			if (ch > h_max) {
				h_max = ch;
			}
		}

		return h_max;
	} else {
		int h_max = 0;

		for (int i = 0; i < obj->children; i++) {
			struct Base *c = obj->child[i];
			int ch = c->get_height(c);

			if (ch < 0) {
				return ch;
			}

			h_max += ch;
		}

		return h_max + (obj->children - 1) * obj->space;
	}
}

static void ui_list_layout(struct Base *base, struct Pair size)
{
	struct List *obj = (struct List *)base;

#define LIST_LAYOUT(_method, _size, _dim, _dim_size, _method2, _dim_size2, _size2) \
	do { \
		int x = 0; \
		int y = 0; \
		int even_size = 0; \
		int last_size = 0; \
		int total_size = 0; \
		int iwos = 0; \
		int children = 0; \
\
		for (int i = 0; i < obj->children; i++) { \
			struct Base *c = obj->child[i]; \
			int cs = c->_method(c); \
\
			if (cs == 0) { \
				continue; \
			} \
\
			children++; \
\
			if (cs > 0) { \
				total_size += cs; \
			} else { \
				iwos++; \
			} \
		} \
\
		total_size += (children - 1) * obj->space; \
\
		if (iwos) { \
			even_size = _size - total_size; \
			even_size /= iwos; \
			last_size = _size - total_size - even_size * (iwos - 1); \
		} \
\
		for (int i = 0; i < obj->children; i++) { \
			struct Base *c = obj->child[i]; \
			int _dim_size = c->_method(c); \
			int _dim_size2 = c->_method2(c); \
\
			if (_dim_size == 0 || _dim_size2 == 0) { \
				ui_set_vp(base, c, rect_new(0, 0, 0, 0)); \
				continue; \
			} \
\
			if (_dim_size < 0) { \
				_dim_size = even_size; \
\
				iwos--; \
				if (iwos == 0) { \
					_dim_size = last_size; \
				} \
			} \
\
			if (_dim_size2 < 0) { \
				_dim_size2 = _size2; \
			} \
\
			ui_set_vp(base, c, rect_new(x, y, width, height)); \
\
			_dim += _dim_size; \
			_dim += obj->space; \
		} \
\
		_dim -= obj->space; \
	} while (0)

	if (obj->horizontal) {
		LIST_LAYOUT(get_width, size.w, x, width, get_height, height, size.h);
	} else {
		LIST_LAYOUT(get_height, size.h, y, height, get_width, width, size.w);
	}
}

static void ui_list_layout_children(struct Base *base)
{
	struct List *obj = (struct List *)base;

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];

		ui_layout_child(base, c);
	}
}

static void ui_list_render(struct Base *base, struct Rect svp)
{
	struct List *obj = (struct List *)base;

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];

		ui_render(c, svp);
	}
}

static struct Base *ui_list_pick(struct Base *base, struct Pair p)
{
	struct List *obj = (struct List *)base;

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];
		struct Base *ret = ui_pick_child(c, p);

		if (ret) {
			return ret;
		}
	}
	return NULL;
}

static int ui_circle_get_width(struct Base *base)
{
	struct Circle *obj = (struct Circle *)base;

	return obj->radius * 2 + 1;
}

static int ui_circle_get_height(struct Base *base)
{
	struct Circle *obj = (struct Circle *)base;

	return obj->radius * 2 + 1;
}

static void ui_circle_render(struct Base *base, struct Rect svp)
{
	struct Circle *obj = (struct Circle *)base;

	draw_circle(svp, pair_new(base->vp.x + base->vp.w / 2, base->vp.y + base->vp.h / 2), obj->radius, obj->color, 1);
}

#define UI_REF(child) ((struct Base *)&child)
#define UI_CHILDREN(...) ((struct Base *[]) { __VA_ARGS__ })
#define UI_CHILDREN_SIZE(a) (sizeof(a) / sizeof(struct Base *))

#define UI_BOX_RAW(_width, _height, _h_align, _v_align, _color, _content) \
	{ \
		.base = { \
			.get_width = ui_box_get_width, \
			.get_height = ui_box_get_height, \
			.layout = ui_box_layout, \
			.layout_children = ui_box_layout_children, \
			.invalidate = ui_box_invalidate, \
			.render = ui_box_render, \
			.pick = ui_box_pick, \
		}, \
		.width = _width, \
		.height = _height, \
		.h_align = _h_align, \
		.v_align = _v_align, \
		.color = _color, \
		.content = _content, \
	}

#define UI_BOX(_width, _height, _h_align, _v_align, _color, _content) \
	(struct Box)UI_BOX_RAW(_width, _height, _h_align, _v_align, _color, _content)

#define UI_COLOR_BOX(_width, _height, _color, ...) \
	(struct ColorBox) { \
		.base = { \
			.get_width = ui_color_box_get_width, \
			.get_height = ui_color_box_get_height, \
			.render = ui_color_box_render, \
			.pick = ui_dummy_pick, \
			.handle_activate_event = ui_color_box_handle_activate_event, \
		}, \
		.width = _width, \
		.height = _height, \
		.color = _color, \
		__VA_ARGS__ \
	}

#define UI_GRADIENT_BOX(_width, _height, _color_start, _color_end, _horizontal) \
	(struct GradientBox) { \
		.super = { \
			.base = { \
				.get_width = ui_color_box_get_width, \
				.get_height = ui_color_box_get_height, \
				.render = ui_color_gradient_render, \
			}, \
			.width = _width, \
			.height = _height, \
			.color = _color_start, \
		}, \
		.color_end = _color_end, \
		.horizontal = _horizontal, \
	}

#define UI_BUTTON_BITMAP(_index, _type, ...) \
	(struct ButtonBitmap) { \
		.super = { \
			.base = { \
				.get_width = ui_bitmap_get_width, \
				.get_height = ui_bitmap_get_height, \
				.layout = ui_bitmap_layout, \
				.render = ui_bitmap_render, \
				.pick = ui_dummy_pick, \
				.handle_cursor_event = ui_bitmap_handle_cursor_event, \
				.handle_activate_event = ui_bitmap_handle_activate_event, \
			}, \
			.bc = &icon, \
			.index = _index, \
			.color = 0xffffff, \
			.bg_color = 0x4caf50, \
			.h_align = ALIGN_MIDDLE, \
			.v_align = ALIGN_MIDDLE, \
		}, \
		.type = _type, \
		__VA_ARGS__ \
	}

#define UI_GLYPH(...) \
	(struct Glyph) { \
		.base = { \
			.get_width = ui_glyph_get_width, \
			.get_height = ui_glyph_get_height, \
			.render = ui_glyph_render, \
		}, \
		__VA_ARGS__ \
	}

#define UI_TEXT(_text) \
	(struct Text) { \
		.base = { \
			.get_width = ui_text_get_width, \
			.get_height = ui_text_get_height, \
			.render = ui_text_render, \
		}, \
		.font = font, \
		.text = _text, \
		.color = 0xeff4ff, \
	}

#define UI_LIST(_horizontal, _space, _children) \
	(struct List) { \
		.base = { \
			.get_width = ui_list_get_width, \
			.get_height = ui_list_get_height, \
			.layout = ui_list_layout, \
			.layout_children = ui_list_layout_children, \
			.render = ui_list_render, \
			.pick = ui_list_pick, \
		}, \
		.horizontal = _horizontal, \
		.space = _space, \
		.children = UI_CHILDREN_SIZE(_children), \
		.child = _children, \
	}

#define UI_CIRCLE(_radius, _color) \
	(struct Circle) { \
		.base = { \
			.get_width = ui_circle_get_width, \
			.get_height = ui_circle_get_height, \
			.render = ui_circle_render, \
		}, \
		.radius = _radius, \
		.color = _color, \
	}

#define UI_BOX_EXPANDER(_box, _width2, _height2) \
	(struct BoxExpander) { \
		.super = _box, \
		.width = _width2, \
		.height = _height2, \
	}

#define UI_SLIDER(...) \
	(struct Slider) { \
		.base = { \
			.get_width = ui_dummy_inherit_parent, \
			.get_height = ui_dummy_inherit_parent, \
			.layout = ui_slider_layout, \
			.layout_children = ui_slider_layout_children, \
			.render = ui_slider_render, \
			.pick = ui_dummy_pick, \
			.handle_cursor_event = ui_slider_handle_cursor_event, \
		}, \
		__VA_ARGS__ \
	}

static struct ButtonBitmap ui_button_bitmap_pen;
static struct ButtonBitmap ui_button_bitmap_pencil;
static struct ButtonBitmap ui_button_bitmap_eraser;
static struct ButtonBitmap ui_button_bitmap_picker;

static struct Base *tool_group[] = {
	(struct Base *)&ui_button_bitmap_pen,
	(struct Base *)&ui_button_bitmap_pencil,
	(struct Base *)&ui_button_bitmap_eraser,
	(struct Base *)&ui_button_bitmap_picker,
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

static void ui_button_bitmap_tool_activate(struct Base *base)
{
	for (int i = 0; i < ARRAY_SIZE(tool_group); i++) {
		struct ButtonBitmap *tool = (struct ButtonBitmap *)tool_group[i];

		ui_update_dirty(tool_group[i]);

		if (tool_group[i] == base) {
			continue;
		}

		tool->activated = 0;
		tool->super.bg_color = 0x01579b;
	}
}

static struct ButtonBitmap ui_button_bitmap_pen = UI_BUTTON_BITMAP(0, UI_BUTTON_STICK, .on_activate = ui_button_bitmap_tool_activate);
static struct ButtonBitmap ui_button_bitmap_pencil = UI_BUTTON_BITMAP(1, UI_BUTTON_STICK, .on_activate = ui_button_bitmap_tool_activate);
static struct ButtonBitmap ui_button_bitmap_eraser = UI_BUTTON_BITMAP(2, UI_BUTTON_STICK, .on_activate = ui_button_bitmap_tool_activate);
static struct ButtonBitmap ui_button_bitmap_picker = UI_BUTTON_BITMAP(14, UI_BUTTON_STICK, .on_activate = ui_button_bitmap_tool_activate);

static struct List settings;

static struct BoxExpander settings_expander = UI_BOX_EXPANDER(UI_BOX_RAW(0, 0, ALIGN_BEGIN, ALIGN_MIDDLE, 0x01579b, UI_REF(settings)), INHERIT_CHILD, INHERIT_CHILD);

static void ui_button_bitmap_settings_activate(struct Base *base);

static struct List top_panel = UI_LIST(1, 4,
	UI_CHILDREN(
		UI_REF(
			UI_BOX(INHERIT_PARENT, INHERIT_CHILD, ALIGN_MIDDLE, ALIGN_BEGIN, 0x4caf50,
				UI_REF(
					UI_LIST(1, 4,
						UI_CHILDREN(
							UI_REF(ui_button_bitmap_pen),
							UI_REF(ui_button_bitmap_pencil),
							UI_REF(ui_button_bitmap_eraser),
							UI_REF(ui_button_bitmap_picker),
							UI_REF(UI_BUTTON_BITMAP(3, UI_BUTTON_SWITCH, .on_activate = ui_button_bitmap_settings_activate)),
						)
					)
				)
			)
		),
		UI_REF(
			UI_BOX(INHERIT_PARENT, INHERIT_CHILD, ALIGN_MIDDLE, ALIGN_BEGIN, 0x4caf50,
				UI_REF(
					UI_LIST(1, 4,
						UI_CHILDREN(
							UI_REF(UI_BUTTON_BITMAP(8, UI_BUTTON_NORMAL)),
							UI_REF(UI_BOX(INHERIT_CHILD, INHERIT_PARENT, ALIGN_MIDDLE, ALIGN_MIDDLE, 0x4caf50, UI_REF(UI_TEXT("007 / 101")))),
							UI_REF(UI_BUTTON_BITMAP(4, UI_BUTTON_NORMAL)),
						)
					)
				)
			)
		),
		UI_REF(
			UI_BOX(INHERIT_PARENT, INHERIT_CHILD, ALIGN_MIDDLE, ALIGN_BEGIN, 0x4caf50,
				UI_REF(
					UI_LIST(1, 4,
						UI_CHILDREN(
							UI_REF(UI_BUTTON_BITMAP(6, UI_BUTTON_NORMAL)),
							UI_REF(UI_BUTTON_BITMAP(7, UI_BUTTON_NORMAL)),
							UI_REF(UI_BUTTON_BITMAP(9, UI_BUTTON_NORMAL)),
							UI_REF(UI_BUTTON_BITMAP(10, UI_BUTTON_NORMAL)),
							UI_REF(UI_BUTTON_BITMAP(5, UI_BUTTON_SWITCH)),
						)
					)
				)
			)
		),
	)
);

static void ui_brush_size_process_event(int value);

static void ui_color_picker_value_changed(int value);
static void ui_color_picker_saturation_changed(int value);

static struct Circle settings_brush_circle = UI_CIRCLE(1, 0xeff4ff);
static char settings_brush_text_raw[3] = "1";
static struct Text settings_brush_text = UI_TEXT(settings_brush_text_raw);

static uint32_t brush_color_hue = 0xff8800;
static int brush_color_saturation = 0;
static int brush_color_value = 100;
static char color_rgb_text[12] = "255 255 255";
static struct Text color_rgb = UI_TEXT(color_rgb_text);
static struct Slider ui_color_picker_value = UI_SLIDER(
	.horizontal = 1,
	.split = 1,
	.min = 0,
	.max = 100,
	.value = 0,
	.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, 0x000000, 0xff8800, 1)),
	.knob = UI_REF(
		UI_GLYPH(
			.bc = &icon,
			.index = 15,
			.color = 0xeff4ff,
			.bg_color = 0,
		)
	),
	.on_changed = ui_color_picker_value_changed,
);
static struct Slider ui_color_picker_saturation = UI_SLIDER(
	.horizontal = 1,
	.split = 1,
	.min = 0,
	.max = 100,
	.value = 0,
	.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, 0x884400, 0xffffff, 1)),
	.knob = UI_REF(
		UI_GLYPH(
			.bc = &icon,
			.index = 15,
			.color = 0xeff4ff,
			.bg_color = 0,
		)
	),
	.on_changed = ui_color_picker_saturation_changed,
);

static struct ColorBox brush_color = UI_COLOR_BOX(INHERIT_PARENT, INHERIT_PARENT, 0xeff4ff);

static void ui_color_picker_changed(void)
{
	uint8_t r = (brush_color_hue >> 16) & 0xff;
	uint8_t g = (brush_color_hue >> 8) & 0xff;
	uint8_t b = (brush_color_hue >> 0) & 0xff;

	uint32_t cas_r = CLAMP(r + 255 * brush_color_saturation / 100, 0, 255);
	uint32_t cas_g = CLAMP(g + 255 * brush_color_saturation / 100, 0, 255);
	uint32_t cas_b = CLAMP(b + 255 * brush_color_saturation / 100, 0, 255);

	r = r * brush_color_value / 100;
	g = g * brush_color_value / 100;
	b = b * brush_color_value / 100;

	uint32_t l = MAX(MAX(r, g), b);

	r = (r * (100 - brush_color_saturation) + l * brush_color_saturation) / 100;
	g = (g * (100 - brush_color_saturation) + l * brush_color_saturation) / 100;
	b = (b * (100 - brush_color_saturation) + l * brush_color_saturation) / 100;

	// FIXME Move to Text? color_rgb->set_text("%d %d %d", r, g, b)?
	snprintf(color_rgb_text, sizeof(color_rgb_text), "%d %d %d", r, g, b);

	ui_invalidate(&color_rgb.base);

	struct GradientBox *cv = (struct GradientBox *)ui_color_picker_value.line;

	cv->color_end = (cas_r << 16) | (cas_g << 8) | cas_b;

	ui_update_dirty(&cv->super.base);

	struct GradientBox *cs = (struct GradientBox *)ui_color_picker_saturation.line;

	cs->super.color = brush_color_hue;

	ui_update_dirty(&cs->super.base);

	brush_color.color = (r << 16) | (g << 8) | b;
	ui_update_dirty(&brush_color.base);
}

static void ui_color_picker_rbr_changed(int value)
{
	value = (100 - value) * 0xff / 100;

	brush_color_hue = 0xff0000 | (value << 0);

	ui_color_picker_changed();
}

static void ui_color_picker_rbb_changed(int value)
{
	value = value * 0xff / 100;

	brush_color_hue = 0x0000ff | (value << 16);

	ui_color_picker_changed();
}

static void ui_color_picker_rrg_changed(int value)
{
	value = value * 0xff / 100;

	brush_color_hue = 0xff0000 | (value << 8);

	ui_color_picker_changed();
}

static void ui_color_picker_rgg_changed(int value)
{
	value = value * 0xff / 100;

	brush_color_hue = 0x00ff00 | (value << 16);

	ui_color_picker_changed();
}

static void ui_color_picker_bgb_changed(int value)
{
	value = value * 0xff / 100;

	brush_color_hue = 0x0000ff | (value << 8);

	ui_color_picker_changed();
}

static void ui_color_picker_gbg_changed(int value)
{
	value = (100 - value) * 0xff / 100;

	brush_color_hue = 0x00ff00 | (value << 0);

	ui_color_picker_changed();
}

static void ui_color_picker_value_changed(int value)
{
	brush_color_value = value;

	ui_color_picker_changed();
}

static void ui_color_picker_saturation_changed(int value)
{
	brush_color_saturation = value;

	ui_color_picker_changed();
}

static void ui_brush_color_set(struct ColorBox *obj)
{
	brush_color_hue = obj->color;
	brush_color_value = 100;
	brush_color_saturation = 0;

	ui_color_picker_value.value = 100;
	ui_color_picker_saturation.value = 0;

	ui_color_picker_changed();

	ui_invalidate(&ui_color_picker_value.base);
	ui_invalidate(&ui_color_picker_saturation.base);
}

static struct List settings = UI_LIST(1, 4,
	UI_CHILDREN(
		UI_REF(brush_color),
		UI_REF(
			UI_BOX(INHERIT_CHILD, INHERIT_CHILD, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
				UI_REF(
					UI_LIST(0, 4,
						UI_CHILDREN(
							UI_REF(
								UI_BOX(INHERIT_CHILD, 20, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
									UI_REF(
										UI_LIST(1, 4,
											UI_CHILDREN(
												UI_REF(UI_COLOR_BOX(20, 20, 0xff00ff, .on_click = ui_brush_color_set)),
												UI_REF(
													UI_BOX(100, 20, ALIGN_BEGIN, ALIGN_BEGIN, 0,
														UI_REF(
															UI_SLIDER(
																.horizontal = 1,
																.split = 1,
																.min = 0,
																.max = 100,
																.value = 0,
																.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, 0xff00ff, 0xff0000, 1)),
																.knob = UI_REF(
																	UI_GLYPH(
																		.bc = &icon,
																		.index = 15,
																		.color = 0xeff4ff,
																		.bg_color = 0,
																	)
																),
																.on_changed = ui_color_picker_rbr_changed,
															)
														)
													)
												),
												UI_REF(UI_COLOR_BOX(20, 20, 0xff0000, .on_click = ui_brush_color_set)),
												UI_REF(
													UI_BOX(100, 20, ALIGN_BEGIN, ALIGN_BEGIN, 0,
														UI_REF(
															UI_SLIDER(
																.horizontal = 1,
																.split = 1,
																.min = 0,
																.max = 100,
																.value = 0,
																.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, 0xff0000, 0xffff00, 1)),
																.knob = UI_REF(
																	UI_GLYPH(
																		.bc = &icon,
																		.index = 15,
																		.color = 0xeff4ff,
																		.bg_color = 0,
																	)
																),
																.on_changed = ui_color_picker_rrg_changed,
															)
														)
													)
												),
												UI_REF(UI_COLOR_BOX(20, 20, 0xffff00, .on_click = ui_brush_color_set)),
											)
										)
									)
								)
							),
							UI_REF(
								UI_LIST(1, 4,
									UI_CHILDREN(
										UI_REF(
											UI_BOX(20, 100, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
												UI_REF(
													UI_SLIDER(
														.horizontal = 0,
														.split = 1,
														.min = 0,
														.max = 100,
														.value = 0,
														.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, 0xff00ff, 0x0000ff, 0)),
														.knob = UI_REF(
															UI_GLYPH(
																.bc = &icon,
																.index = 18,
																.color = 0xeff4ff,
																.bg_color = 0,
															)
														),
														.on_changed = ui_color_picker_rbb_changed,
													)
												)
											)
										),
										UI_REF(
											UI_LIST(0, 4,
												UI_CHILDREN(
													UI_REF(
														UI_BOX(INHERIT_PARENT, INHERIT_PARENT, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
															UI_REF(
																UI_LIST(1, 4,
																	UI_CHILDREN(
																		UI_REF(UI_COLOR_BOX(20, 20, 0x000000, .on_click = ui_brush_color_set)),
																		UI_REF(UI_COLOR_BOX(20, 20, 0xffffff, .on_click = ui_brush_color_set)),
																	)
																)
															)
														)
													),
													UI_REF(
														UI_BOX(INHERIT_PARENT, 20, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
															UI_REF(
																UI_LIST(1, 4,
																	UI_CHILDREN(
																		UI_REF(
																			UI_BOX(100, 20, ALIGN_BEGIN, ALIGN_BEGIN, 0,
																				UI_REF(ui_color_picker_value)
																			)
																		),
																		UI_REF(
																			UI_BOX(100, 20, ALIGN_BEGIN, ALIGN_BEGIN, 0,
																				UI_REF(ui_color_picker_saturation)
																			)
																		),
																	)
																)
															)
														)
													),
													UI_REF(
														UI_BOX(INHERIT_PARENT, INHERIT_PARENT, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
															UI_REF(color_rgb)
														)
													),
												)
											)
										),
										UI_REF(
											UI_BOX(20, 100, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
												UI_REF(
													UI_SLIDER(
														.horizontal = 0,
														.split = -1,
														.min = 0,
														.max = 100,
														.value = 0,
														.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, 0xffff00, 0x00ff00, 0)),
														.knob = UI_REF(
															UI_GLYPH(
																.bc = &icon,
																.index = 16,
																.color = 0xeff4ff,
																.bg_color = 0,
															)
														),
														.on_changed = ui_color_picker_rgg_changed,
													)
												)
											)
										),
									)
								)
							),
							UI_REF(
								UI_BOX(INHERIT_CHILD, 20, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
									UI_REF(
										UI_LIST(1, 4,
											UI_CHILDREN(
												UI_REF(UI_COLOR_BOX(20, 20, 0x0000ff, .on_click = ui_brush_color_set)),
												UI_REF(
													UI_BOX(100, 20, ALIGN_BEGIN, ALIGN_BEGIN, 0,
														UI_REF(
															UI_SLIDER(
																.horizontal = 1,
																.split = -1,
																.min = 0,
																.max = 100,
																.value = 0,
																.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, 0x0000ff, 0x00ffff, 1)),
																.knob = UI_REF(
																	UI_GLYPH(
																		.bc = &icon,
																		.index = 17,
																		.color = 0xeff4ff,
																		.bg_color = 0,
																	)
																),
																.on_changed = ui_color_picker_bgb_changed,
															)
														)
													)
												),
												UI_REF(UI_COLOR_BOX(20, 20, 0x00ffff, .on_click = ui_brush_color_set)),
												UI_REF(
													UI_BOX(100, 20, ALIGN_BEGIN, ALIGN_BEGIN, 0,
														UI_REF(
															UI_SLIDER(
																.horizontal = 1,
																.split = -1,
																.min = 0,
																.max = 100,
																.value = 0,
																.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, 0x00ffff, 0x00ff00, 1)),
																.knob = UI_REF(
																	UI_GLYPH(
																		.bc = &icon,
																		.index = 17,
																		.color = 0xeff4ff,
																		.bg_color = 0,
																	)
																),
																.on_changed = ui_color_picker_gbg_changed,
															)
														)
													)
												),
												UI_REF(UI_COLOR_BOX(20, 20, 0x00ff00, .on_click = ui_brush_color_set)),
											)
										)
									)
								)
							),
						)
					)
				)
			)
		),
		UI_REF(
			UI_BOX(INHERIT_CHILD, INHERIT_CHILD, ALIGN_MIDDLE, ALIGN_MIDDLE, 0x01579b,
				UI_REF(
					UI_LIST(0, 4,
						UI_CHILDREN(
							UI_REF(
								UI_BOX(INHERIT_PARENT, INHERIT_PARENT, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
									UI_REF(settings_brush_circle)
								)
							),
							UI_REF(
								UI_BOX(50, INHERIT_CHILD, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
									UI_REF(settings_brush_text)
								)
							),
						)
					)
				)
			)
		),
		UI_REF(
			UI_BOX(30, INHERIT_PARENT, ALIGN_MIDDLE, ALIGN_MIDDLE, 0,
				UI_REF(
					UI_SLIDER(
						.horizontal = 0,
						.min = 1,
						.max = 16,
						.value = 1,
						.line = UI_REF(UI_COLOR_BOX(4, INHERIT_PARENT, 0x4caf50)),
						.knob = UI_REF(UI_COLOR_BOX(INHERIT_PARENT, 6, 0xeff4ff)),
						.on_changed = ui_brush_size_process_event,
					)
				)
			)
		),
	)
);

struct List app = UI_LIST(0, 4,
	UI_CHILDREN(
		UI_REF(top_panel),
		UI_REF(settings_expander),
		UI_REF(UI_COLOR_BOX(INHERIT_PARENT, INHERIT_PARENT, 0xeff4ff)),
	)
);

static void ui_button_bitmap_settings_activate(struct Base *base)
{
	struct BoxExpander *be = &settings_expander;
	int w = be->width;
	int h = be->height;

	be->width = be->super.width;
	be->height = be->super.height;

	be->super.width = w;
	be->super.height = h;

	ui_invalidate(be->super.base.parent);
}

static void ui_brush_size_process_event(int value)
{
	struct Circle *obj = &settings_brush_circle;

	obj->radius = value;

	snprintf(settings_brush_text_raw, sizeof(settings_brush_text_raw), "%d", obj->radius % 100);

	ui_invalidate(&settings_brush_circle.base);
	ui_invalidate(&settings_brush_text.base);
}

#ifndef FPGA
int main()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("SDL init failed.\n");
		return -1;
	}

	SDL_Window* sdl_window = NULL;
	SDL_Renderer* sdl_renderer = NULL;
	SDL_Texture* sdl_texture = NULL;

	sdl_window = SDL_CreateWindow("Square", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, H_RES, V_RES, SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
	if (!sdl_window) {
		printf("Window creation failed: %s\n", SDL_GetError());
		return -1;
	}

	sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED);
	if (!sdl_renderer) {
		printf("Renderer creation failed: %s\n", SDL_GetError());
		return -1;
	}

	sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, H_RES, V_RES);
	if (!sdl_texture) {
		printf("Texture creation failed: %s\n", SDL_GetError());
		return -1;
	}

	app.base.vp = app.base.svp = app.base.dirty = rect_new(0, 0, H_RES, V_RES);

	relayout = &app.base;
	rerender = &app.base;

	while (1) {
		SDL_Event e;
		if (SDL_WaitEvent(&e)) {
			if (e.type == SDL_QUIT) {
				break;
			}

			switch (e.type) {
			case SDL_WINDOWEVENT:
				switch (e.window.event) {
				case SDL_WINDOWEVENT_EXPOSED:
					break;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				ui_cursor_down(&app.base, pair_new(e.button.x, e.button.y));
				break;
			case SDL_MOUSEMOTION:
				ui_cursor_move(pair_new(e.motion.x, e.motion.y));
				break;
			case SDL_MOUSEBUTTONUP:
				ui_cursor_up(&app.base, pair_new(e.button.x, e.button.y));
				break;
			}

			layout_cnt = 0;
			layout_enter_cnt = 0;
			render_cnt = 0;
			render_skip_cnt = 0;

			printf("--------------------------------\n");
			ui_process(&app.base);

			printf("%i, %i, %i, %i\n", layout_enter_cnt, layout_cnt, render_cnt, render_skip_cnt);
		}

		SDL_UpdateTexture(sdl_texture, NULL, fb, H_RES * 4);
		SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
		SDL_RenderPresent(sdl_renderer);

#if 0
		for (int i = 0; i < 100; i++) {
			if (SDL_PollEvent(&e)) {
				if (e.type == SDL_QUIT) {
					goto out;
				}
			}

			app.row[1]++;
			render_app((struct Base *)&app, rect_new(0, 0, H_RES - 1, V_RES - 1));

			SDL_UpdateTexture(sdl_texture, NULL, fb, H_RES * 4);
			SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
			SDL_RenderPresent(sdl_renderer);

			SDL_Delay(20);
		}
#endif
	}

out:
	SDL_DestroyTexture(sdl_texture);
	SDL_DestroyRenderer(sdl_renderer);
	SDL_DestroyWindow(sdl_window);
	SDL_Quit();

	return 0;
}
#endif
