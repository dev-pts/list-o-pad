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

static int pair_equal(struct Pair p1, struct Pair p2)
{
	return memcmp(&p1, &p2, sizeof(struct Pair)) == 0;
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
	rect.w = MAX(MIN(s.x + s.w, r.x + r.w) - rect.x, 0);
	rect.h = MAX(MIN(s.y + s.h, r.y + r.h) - rect.y, 0);
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

struct Interpolator {
	int s_start;
	int s_end;
	int s_len;
	int v_start;
	int v_len;
	int value;

	int slop;
	int step;
	int cnt;
	int sign;
};

/* Interpolate [v_start, v_end] into [s_start, s_end] */
static struct Interpolator interpolator_create(int v_start, int v_end, int s_start, int s_end)
{
	int len = s_end - s_start;

	struct Interpolator obj = {
		.s_start = s_start,
		.s_end = s_end,
		.s_len = len,
		.v_start = v_start,
		.value = v_start,
		/* We are dividing by 2 to have even parts at the beginning and at the end */
		.cnt = len / 2,
		.sign = 1,
	};

	obj.v_len = v_end - v_start;
	obj.slop = obj.v_len / len;
	obj.step = obj.slop * len;

	if (obj.v_len < 0) {
		obj.v_len = -obj.v_len;
		obj.step = -obj.step;
		obj.sign = -1;
	}

	return obj;
}

static void interpolator_reset(struct Interpolator *obj)
{
	obj->value = obj->v_start;
	obj->cnt = obj->s_len / 2;
}

static void interpolator_step(struct Interpolator *obj)
{
	obj->cnt += obj->v_len;

	if (obj->cnt >= obj->s_len) {
		obj->cnt -= obj->step;
		obj->value += obj->slop;

		/* Handle accumulated error */
		if (obj->cnt >= obj->s_len) {
			obj->cnt -= obj->s_len;
			obj->value += obj->sign;
		}
	}
}

/* Map value from [s_start, s_end] to [v_start, v_end] */
static int interpolator_get(struct Interpolator *obj, int t)
{
	return obj->v_start + (obj->v_len * (t - obj->s_start) + obj->s_len / 2) / obj->s_len * obj->sign;
}

static void draw_rect_gradient(struct Rect r, struct Rect s, uint32_t color_start, uint32_t color_end, int horizontal)
{
	if (!rect_valid(r)) {
		return;
	}

	int end = (horizontal ? r.w : r.h) - 1;

	struct Interpolator i_r = interpolator_create((color_start >> 16) & 0xff, (color_end >> 16) & 0xff, 0, end);
	struct Interpolator i_g = interpolator_create((color_start >> 8) & 0xff, (color_end >> 8) & 0xff, 0, end);
	struct Interpolator i_b = interpolator_create((color_start >> 0) & 0xff, (color_end >> 0) & 0xff, 0, end);

	if (horizontal) {
		for (int i = r.y; i < r.y + r.h; i++) {
			for (int j = r.x; j < r.x + r.w; j++) {
				uint32_t color = (i_r.value << 16) | (i_g.value << 8) | (i_b.value << 0);

				draw_pixel_check(s, pair_new(j, i), color);

				interpolator_step(&i_r);
				interpolator_step(&i_g);
				interpolator_step(&i_b);
			}

			interpolator_reset(&i_r);
			interpolator_reset(&i_g);
			interpolator_reset(&i_b);
		}
	} else {
		for (int i = r.y; i < r.y + r.h; i++) {
			uint32_t color = (i_r.value << 16) | (i_g.value << 8) | (i_b.value << 0);

			for (int j = r.x; j < r.x + r.w; j++) {
				draw_pixel_check(s, pair_new(j, i), color);
			}

			interpolator_step(&i_r);
			interpolator_step(&i_g);
			interpolator_step(&i_b);
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

	/* Local placement.
	 * Useful, for example, for window with elements inside it,
	 * and it moves - nothing inside it will be re-layouted,
	 * only re-rendered. */
	struct Rect lvp;
	/* Placement in screen coordinates.
	 * Rendering happens inside it (with respect to svp). */
	struct Rect vp;
	/* Viewport in screen coordinates.
	 * The element can be partly seen or not seen at all.
	 * svp is the actual viewing rect, and is used during rendering.
	 * The picking also happens agains svp. */
	struct Rect svp;

	int valid;

	/* Parent may ask the element about its sizes.
	 * You can only return INHERIT_PARENT or >= 0. */
	int (*get_width)(struct Base *obj);
	int (*get_height)(struct Base *obj);

	/* Parent assigns you the lvp,
	 * you just need to layout everything you want into this lvp.
	 */
	void (*layout)(struct Base *obj, struct Pair size);
	void (*layout_children)(struct Base *obj);

	/* Render everything you needed inside your vp
	 * with respect to the svp argument.
	 * Don't render outside this svp.
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

	if (child->valid) {
		return;
	}

	child->valid = 1;

	if (!rect_valid(child->lvp)) {
		return;
	}

	if (child->layout) {
		child->layout(child, child->lvp.s);
	}

	if (child->layout_children) {
		child->layout_children(child);
	}

	layout_cnt++;
}

static void ui_process(struct Base *root)
{
	while (relayout) {
		struct Base *base = relayout;

		relayout = base->next;
		base->next = NULL;

		if (base->layout) {
			base->layout(base, base->lvp.s);
		}
		if (base->layout_children) {
			base->layout_children(base);
		}
		base->valid = 1;
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
	if (rerender == base || base->rerender_next) {
		return;
	}

	if (!rect_valid(base->svp)) {
		return;
	}

	base->dirty = base->svp;

	base->rerender_next = rerender;
	rerender = base;
}

static void ui_set_vp(struct Base *base, struct Rect vp)
{
	base->valid = pair_equal(base->lvp.s, vp.s);
	base->lvp = vp;
}

static void ui_invalidate(struct Base *base)
{
	if (relayout == base || base->next) {
		return;
	}

	ui_update_dirty(base);

	base->next = relayout;
	relayout = base;

	while (base) {
		base->valid = 0;

		if ((base->invalidate && !base->invalidate(base)) || !base->parent) {
			if (relayout == base || base->next) {
				return;
			}

			base->next = relayout;
			relayout = base;
			return;
		}

		base = base->parent;
	}
}

static void ui_render(struct Base *base, struct Base *child, struct Rect svp)
{
	struct Rect avp_new = rect_move(child->lvp, base->vp.p);
	struct Rect svp_new = rect_intersect(avp_new, base->svp);

	if (!rect_valid(avp_new) || !rect_valid(svp_new)) {
		memset(&child->vp, 0, sizeof(child->vp));
		memset(&child->svp, 0, sizeof(child->svp));
		return;
	}

	child->vp = avp_new;
	child->svp = svp_new;

	if (!child->render) {
		return;
	}

	svp = rect_intersect(svp, child->svp);

	if (!rect_valid(svp)) {
		render_skip_cnt++;
		return;
	}

	child->render(child, svp);
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

struct DummyBox {
	struct Base base;

	int width;
	int height;
};

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
	char *text;
	int text_len;
	uint32_t color;
};

struct ColorBox {
	struct Base base;

	int width;
	int height;
	uint32_t color;

	void (*on_click)(struct ColorBox *obj, void *data);
	void *data;
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

	struct Pair p;
	int offset;
	int child_start;
	int global_index;
	void (*generate)(struct Base *base, int index);
	int has_min;
	int has_max;
	int index_min;
	int index_max;
	void (*on_scroll)(struct List *obj, int delta);
};

struct Slider {
	struct Base base;

	int horizontal;
	int min;
	int max;
	int value;

	int len;

	struct Base *knob;
	struct Base *line;

	int ks;
	int split;

	void (*on_changed)(struct Slider *obj, int value);
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

static int ui_list_scroll(struct List *obj, int delta)
{
	int s = 0;
	int max_offset = 0;
	int old_offset = obj->offset;

	if (obj->horizontal) {
		s = obj->child[0]->lvp.w + obj->space;
		max_offset = obj->base.svp.w - obj->base.lvp.w;
	} else {
		s = obj->child[0]->lvp.h + obj->space;
		max_offset = obj->base.svp.h - obj->base.lvp.h;
	}

	obj->offset += delta;

	while (obj->offset > 0) {
		if (obj->has_min && obj->global_index <= obj->index_min) {
			obj->offset = 0;
			delta = obj->offset - old_offset;
			break;
		}

		obj->global_index--;
		obj->offset -= s;
		obj->child_start--;
		if (obj->child_start < 0) {
			obj->child_start = obj->children - 1;
		}

		if (obj->generate) {
			obj->generate(obj->child[obj->child_start], obj->global_index);
		}
	}

	while (obj->offset <= -s) {
		if (obj->has_max && obj->global_index + obj->children > obj->index_max) {
			obj->offset = MAX(obj->offset, max_offset - 1);
			delta = obj->offset - old_offset;
			break;
		}

		if (obj->generate) {
			obj->generate(obj->child[obj->child_start], obj->global_index + obj->children);
		}

		obj->global_index++;
		obj->offset += s;
		obj->child_start++;
		if (obj->child_start == obj->children) {
			obj->child_start = 0;
		}
	}

	ui_invalidate(&obj->base);

	return delta;
}

static void ui_text_set(struct Text *obj, const char *fmt, ...);
static void ui_rotary_generate(struct Base *base, int index)
{
	struct List *list = (struct List *)base;
	struct Box *box = (struct Box *)list->child[0];
	struct Text *text = (struct Text *)box->content;

	ui_text_set(text, "%i", index + 1);
}

static void ui_rotary_handle_cursor_event(struct Base *base, enum EventCursorType type, struct Pair p)
{
	struct List *obj = (struct List *)base;
	int delta = 0;

	if (obj->horizontal) {
		delta = p.x - obj->p.x;
	} else {
		delta = p.y - obj->p.y;
	}

	switch (type) {
	case EV_CURSOR_DOWN:
		obj->p = p;
		break;
	case EV_CURSOR_MOVE:
		if (obj->on_scroll) {
			obj->on_scroll(obj, delta);
		}
		obj->p = p;
		break;
	default:
		break;
	}
}

static int ui_dummy_box_get_width(struct Base *base)
{
	struct DummyBox *obj = (struct DummyBox *)base;

	return obj->width;
}

static int ui_dummy_box_get_height(struct Base *base)
{
	struct DummyBox *obj = (struct DummyBox *)base;

	return obj->height;
}

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

	ui_set_vp(obj->content, rect_move(vp, ui_align(size, vp.s, obj->h_align, obj->v_align)));
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
		if (ui_get_width(obj->content) != base->lvp.w) {
			return 1;
		}
	}

	if (obj->height == INHERIT_CHILD) {
		if (ui_get_height(obj->content) != base->lvp.h) {
			return 1;
		}
	}

	return 0;
}

static void ui_box_render(struct Base *base, struct Rect svp)
{
	struct Box *obj = (struct Box *)base;

	draw_rect(svp, obj->color);

	ui_render(base, obj->content, svp);
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

static void ui_text_set(struct Text *obj, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(obj->text, obj->text_len, fmt, ap);
	va_end(ap);

	ui_invalidate(&obj->base);
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
	obj->ks = ks;

	{
		struct Interpolator ir = interpolator_create(0, obj->len, obj->min, obj->max);
		int point = interpolator_get(&ir, obj->value);
		int x = 0;
		int y = 0;
		int w = size.w;
		int h = size.h;

		if (obj->horizontal) {
			x = point;
			w = ks;

			if (obj->split) {
				h = kh;

				if (obj->split > 0) {
					y += size.h - kh;
				}
			}
		} else {
			y = point;
			y = size.h - y - ks;
			h = ks;

			if (obj->split) {
				w = kw;

				if (obj->split > 0) {
					x += size.w - kw;
				}
			}
		}

		ui_set_vp(obj->knob, rect_new(x, y, w, h));
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

		ui_set_vp(obj->line, rect_new(x, y, w, h));
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

	ui_render(base, obj->line, svp);
	ui_render(base, obj->knob, svp);
}

static void ui_slider_handle_cursor_event(struct Base *base, enum EventCursorType type, struct Pair p)
{
	struct Slider *obj = (struct Slider *)base;
	struct Interpolator ir = interpolator_create(obj->min, obj->max, 0, obj->len);
	int point;

	switch (type) {
	case EV_CURSOR_DOWN:
	case EV_CURSOR_MOVE:
		if (obj->horizontal) {
			point = p.x - obj->ks / 2;
		} else {
			point = base->vp.h - obj->ks - (p.y - obj->ks / 2);
		}

		obj->value = interpolator_get(&ir, CLAMP(point, 0, obj->len));

		obj->on_changed(obj, obj->value);

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
		_dim += obj->offset; \
\
		{ \
			int start = obj->child_start; \
			int end = obj->children; \
\
			for (int j = 0; j < 2; j++) { \
				for (int i = start; i < end; i++) { \
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
				start = 0; \
				end = obj->child_start; \
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
		{ \
			int start = obj->child_start; \
			int end = obj->children; \
\
			for (int j = 0; j < 2; j++) { \
				for (int i = start; i < end; i++) { \
					struct Base *c = obj->child[i]; \
					int _dim_size = c->_method(c); \
					int _dim_size2 = c->_method2(c); \
\
					if (_dim_size == 0 || _dim_size2 == 0) { \
						ui_set_vp(c, rect_new(0, 0, 0, 0)); \
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
					ui_set_vp(c, rect_new(x, y, width, height)); \
\
					_dim += _dim_size; \
					_dim += obj->space; \
				} \
				start = 0; \
				end = obj->child_start; \
			} \
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
	int start = obj->child_start;
	int end = obj->children;

	for (int j = 0; j < 2; j++) {
		for (int i = start; i < end; i++) {
			struct Base *c = obj->child[i];

			ui_render(base, c, svp);
		}

		start = 0;
		end = obj->child_start;
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

#define UI_DUMMY \
	(struct Base) { \
		.get_width = ui_dummy_inherit_parent, \
		.get_height = ui_dummy_inherit_parent, \
	}

#define UI_DUMMY_BOX(_width, _height) \
	(struct DummyBox) { \
		.base = { \
			.get_width = ui_dummy_box_get_width, \
			.get_height = ui_dummy_box_get_height, \
		}, \
		.width = _width, \
		.height = _height, \
	}

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
		.text_len = sizeof(_text), \
		.color = 0xeff4ff, \
	}

#define UI_LIST(_horizontal, _space, _children, ...) \
	(struct List) { \
		.base = { \
			.get_width = ui_list_get_width, \
			.get_height = ui_list_get_height, \
			.layout = ui_list_layout, \
			.layout_children = ui_list_layout_children, \
			.render = ui_list_render, \
			.pick = ui_list_pick, \
			__VA_ARGS__ \
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
static void ui_button_bitmap_pages_activate(struct Base *base);

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
							UI_REF(UI_BOX(100, INHERIT_PARENT, ALIGN_MIDDLE, ALIGN_MIDDLE, 0x4caf50, UI_REF(UI_TEXT("007 / 101")))),
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
							UI_REF(UI_BUTTON_BITMAP(5, UI_BUTTON_SWITCH, .on_activate = ui_button_bitmap_pages_activate)),
						)
					)
				)
			)
		),
	)
);

static void ui_brush_size_process_event(struct Slider *obj, int value);

static void ui_color_picker_value_changed(struct Slider *obj, int value);
static void ui_color_picker_saturation_changed(struct Slider *obj, int value);

static struct Circle settings_brush_circle = UI_CIRCLE(1, 0xeff4ff);
static char settings_brush_text_raw[3] = "1";
static struct Text settings_brush_text = UI_TEXT(settings_brush_text_raw);

static struct Glyph color_active = UI_GLYPH(
	.bc = &icon,
	.index = 15,
	.color = 0xeff4ff,
	.bg_color = 0,
);

static uint32_t brush_color_hue = 0xff0000;
static int brush_color_saturation = 100;
static int brush_color_value = 0;
static char color_rgb_text[12] = "0 0 0";
static struct Text color_rgb = UI_TEXT(color_rgb_text);
static struct Slider ui_color_picker_value = UI_SLIDER(
	.horizontal = 1,
	.split = 1,
	.min = 0,
	.max = 100,
	.value = 0,
	.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, 0x000000, 0xffffff, 1)),
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
	.value = 100,
	.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0, 1)),
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

static struct ColorBox brush_color = UI_COLOR_BOX(INHERIT_PARENT, INHERIT_PARENT, 0);

static void ui_color_picker_rbr_changed(struct Slider *obj, int value);
static void ui_color_picker_rbb_changed(struct Slider *obj, int value);
static void ui_color_picker_rrg_changed(struct Slider *obj, int value);
static void ui_color_picker_rgg_changed(struct Slider *obj, int value);
static void ui_color_picker_bgb_changed(struct Slider *obj, int value);
static void ui_color_picker_gbg_changed(struct Slider *obj, int value);

struct ColorPointerSliderData {
	struct Slider *slider;
	struct DummyBox dummy;
	int index;
};

#define CP_SLIDER(_data, _name, _horizontal, _split, _color_start, _color_end, _index, _cb) \
	static struct ColorPointerSliderData _data; \
	static struct Slider _name = UI_SLIDER( \
		.horizontal = _horizontal, \
		.split = _split, \
		.min = 0, \
		.max = 100, \
		.value = 0, \
		.line = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, _color_start, _color_end, _horizontal)), \
		.knob = UI_REF(_data.dummy), \
		.on_changed = _cb, \
	); \
	static struct ColorPointerSliderData _data = { \
		.slider = &_name, \
		.dummy = UI_DUMMY_BOX(7, 7), \
		.index = _index, \
	};

CP_SLIDER(rxg_rxx_data, rxg_rxx_slider, 1, 1, 0xff00ff, 0xff0000, 15, ui_color_picker_rbr_changed)
CP_SLIDER(rxx_rgx_data, rxx_rgx_slider, 1, 1, 0xff0000, 0xffff00, 15, ui_color_picker_rrg_changed)
CP_SLIDER(rxb_xxb_data, rxb_xxb_slider, 0, 1, 0xff00ff, 0x0000ff, 18, ui_color_picker_rbb_changed)
CP_SLIDER(rgx_xgx_data, rgx_xgx_slider, 0, -1, 0xffff00, 0x00ff00, 16, ui_color_picker_rgg_changed)
CP_SLIDER(xxg_xgb_data, xxg_xgb_slider, 1, -1, 0x0000ff, 0x00ffff, 17, ui_color_picker_bgb_changed)
CP_SLIDER(xgb_xgx_data, xgb_xgx_slider, 1, -1, 0x00ffff, 0x00ff00, 17, ui_color_picker_gbg_changed)

static struct ColorPointerSliderData *cp_slider[] = {
	&rxg_rxx_data,
	&rxx_rgx_data,
	&rxb_xxb_data,
	&rgx_xgx_data,
	&xxg_xgb_data,
	&xgb_xgx_data,
};

struct BrushColorSetData {
	struct List list;
	struct Box *box;
	struct Base dummy;
	int index;
	uint32_t color;
};

static void ui_brush_color_set(struct List *obj);

static void ui_color_box_handle_activate_event(struct Base *base, enum EventActivateType type)
{
	struct List *obj = (struct List *)base;

	switch (type) {
	case EV_ACTIVATE:
		ui_brush_color_set(obj);
		break;
	default:
		break;
	}
}

#define BRUSH_COLOR_SET_DATA_REF(_data, _box, _cp, _child1, _child2, _color, _index, _ref) \
	static struct BrushColorSetData _data; \
	static struct Box _box = UI_BOX(20, 7, ALIGN_MIDDLE, ALIGN_BEGIN, 0, UI_REF(_ref)); \
	static struct ColorBox _cp = UI_COLOR_BOX(20, 13, _color); \
	\
	static struct BrushColorSetData _data = { \
		.list = UI_LIST(0, 0, \
			UI_CHILDREN( \
				UI_REF(_child1), \
				UI_REF(_child2), \
			), \
			.pick = ui_dummy_pick, \
			.handle_activate_event = ui_color_box_handle_activate_event, \
		), \
		.box = &_box, \
		.dummy = UI_DUMMY, \
		.index = _index, \
		.color = _color, \
	};

#define BRUSH_COLOR_SET_DATA(_data, _box, _cp, _child1, _child2, _color, _index) \
	BRUSH_COLOR_SET_DATA_REF(_data, _box, _cp, _child1, _child2, _color, _index, _data.dummy)

BRUSH_COLOR_SET_DATA_REF(r___data, r__, cp_r__, cp_r__, r__, 0xff0000, 15, color_active)
BRUSH_COLOR_SET_DATA(r_b_data, r_b, cp_r_b, cp_r_b, r_b, 0xff00ff, 15)
BRUSH_COLOR_SET_DATA(rg__data, rg_, cp_rg_, cp_rg_, rg_, 0xffff00, 15)
BRUSH_COLOR_SET_DATA(__b_data, __b, cp___b, __b, cp___b, 0x0000ff, 17)
BRUSH_COLOR_SET_DATA(_gb_data, _gb, cp__gb, _gb, cp__gb, 0x00ffff, 17)
BRUSH_COLOR_SET_DATA(_g__data, _g_, cp__g_, _g_, cp__g_, 0x00ff00, 17)

static struct BrushColorSetData *cp_data[] = {
	&r_b_data,
	&r___data,
	&rg__data,
	&__b_data,
	&_gb_data,
	&_g__data,
};

static void ui_cp_c_reselect(struct List *obj)
{
	for (int i = 0; i < ARRAY_SIZE(cp_data); i++) {
		if (&cp_data[i]->list == obj) {
			cp_data[i]->box->content = (struct Base *)&color_active;

			color_active.index = cp_data[i]->index;

			ui_invalidate(&cp_data[i]->box->base);
		} else if (cp_data[i]->box->content != &cp_data[i]->dummy) {
			cp_data[i]->box->content = &cp_data[i]->dummy;

			ui_invalidate(&cp_data[i]->box->base);
		}
	}
}

static void ui_cp_slider_reselect(struct Slider *obj)
{
	for (int i = 0; i < ARRAY_SIZE(cp_slider); i++) {
		if (cp_slider[i]->slider == obj) {
			obj->knob = (struct Base *)&color_active;

			color_active.index = cp_slider[i]->index;

			ui_invalidate(&obj->base);
		} else if (cp_slider[i]->slider->knob == &color_active.base) {
			cp_slider[i]->slider->knob = &cp_slider[i]->dummy.base;

			ui_invalidate(&cp_slider[i]->slider->base);
		}
	}
}

static void ui_color_picker_changed(struct Slider *obj)
{
	uint8_t hr = (brush_color_hue >> 16) & 0xff;
	uint8_t hg = (brush_color_hue >> 8) & 0xff;
	uint8_t hb = (brush_color_hue >> 0) & 0xff;

	uint32_t r = hr * brush_color_value / 100;
	uint32_t g = hg * brush_color_value / 100;
	uint32_t b = hb * brush_color_value / 100;

	uint32_t l = MAX(MAX(r, g), b);

	uint32_t l2 = MAX(MAX(hr, hg), hb);
	uint32_t cas_r = (hr * (100 - brush_color_saturation) + l2 * brush_color_saturation) / 100;
	uint32_t cas_g = (hg * (100 - brush_color_saturation) + l2 * brush_color_saturation) / 100;
	uint32_t cas_b = (hb * (100 - brush_color_saturation) + l2 * brush_color_saturation) / 100;
	uint32_t cav_r = r;
	uint32_t cav_g = g;
	uint32_t cav_b = b;

	r = (r * (100 - brush_color_saturation) + l * brush_color_saturation) / 100;
	g = (g * (100 - brush_color_saturation) + l * brush_color_saturation) / 100;
	b = (b * (100 - brush_color_saturation) + l * brush_color_saturation) / 100;

	ui_text_set(&color_rgb, "%d %d %d", r, g, b);

	struct GradientBox *cv = (struct GradientBox *)ui_color_picker_value.line;

	cv->color_end = (cas_r << 16) | (cas_g << 8) | cas_b;

	ui_update_dirty(&cv->super.base);

	struct GradientBox *cs = (struct GradientBox *)ui_color_picker_saturation.line;

	cs->super.color = (cav_r << 16) | (cav_g << 8) | cav_b;
	cs->color_end = (l << 16) | (l << 8) | l;

	ui_update_dirty(&cs->super.base);

	brush_color.color = (r << 16) | (g << 8) | b;
	ui_update_dirty(&brush_color.base);

	if (obj) {
		ui_cp_slider_reselect(obj);
		ui_cp_c_reselect(NULL);
	}
}

static void ui_color_picker_rbr_changed(struct Slider *obj, int value)
{
	value = (100 - value) * 0xff / 100;

	brush_color_hue = 0xff0000 | (value << 0);

	ui_color_picker_changed(obj);
}

static void ui_color_picker_rbb_changed(struct Slider *obj, int value)
{
	value = value * 0xff / 100;

	brush_color_hue = 0x0000ff | (value << 16);

	ui_color_picker_changed(obj);
}

static void ui_color_picker_rrg_changed(struct Slider *obj, int value)
{
	value = value * 0xff / 100;

	brush_color_hue = 0xff0000 | (value << 8);

	ui_color_picker_changed(obj);
}

static void ui_color_picker_rgg_changed(struct Slider *obj, int value)
{
	value = value * 0xff / 100;

	brush_color_hue = 0x00ff00 | (value << 16);

	ui_color_picker_changed(obj);
}

static void ui_color_picker_bgb_changed(struct Slider *obj, int value)
{
	value = value * 0xff / 100;

	brush_color_hue = 0x0000ff | (value << 8);

	ui_color_picker_changed(obj);
}

static void ui_color_picker_gbg_changed(struct Slider *obj, int value)
{
	value = (100 - value) * 0xff / 100;

	brush_color_hue = 0x00ff00 | (value << 0);

	ui_color_picker_changed(obj);
}

static void ui_color_picker_value_changed(struct Slider *obj, int value)
{
	brush_color_value = value;

	ui_color_picker_changed(NULL);
}

static void ui_color_picker_saturation_changed(struct Slider *obj, int value)
{
	brush_color_saturation = value;

	ui_color_picker_changed(NULL);
}

static void ui_brush_color_set(struct List *obj)
{
	for (int i = 0; i < ARRAY_SIZE(cp_data); i++) {
		if (&cp_data[i]->list == obj) {
			brush_color_hue = cp_data[i]->color;
			break;
		}
	}
	brush_color_value = 100;
	brush_color_saturation = 0;

	ui_color_picker_value.value = 100;
	ui_color_picker_saturation.value = 0;

	ui_invalidate(&ui_color_picker_value.base);
	ui_invalidate(&ui_color_picker_saturation.base);

	ui_color_picker_changed(NULL);
	ui_cp_slider_reselect(NULL);
	ui_cp_c_reselect(obj);
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
												UI_REF(r_b_data.list),
												UI_REF(
													UI_BOX(100, 20, ALIGN_BEGIN, ALIGN_BEGIN, 0,
														UI_REF(rxg_rxx_slider)
													)
												),
												UI_REF(r___data.list),
												UI_REF(
													UI_BOX(100, 20, ALIGN_BEGIN, ALIGN_BEGIN, 0,
														UI_REF(rxx_rgx_slider)
													)
												),
												UI_REF(rg__data.list),
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
												UI_REF(rxb_xxb_slider)
											)
										),
										UI_REF(
											UI_LIST(0, 4,
												UI_CHILDREN(
													UI_REF(UI_DUMMY),
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
												UI_REF(rgx_xgx_slider)
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
												UI_REF(__b_data.list),
												UI_REF(
													UI_BOX(100, 20, ALIGN_BEGIN, ALIGN_BEGIN, 0,
														UI_REF(xxg_xgb_slider)
													)
												),
												UI_REF(_gb_data.list),
												UI_REF(
													UI_BOX(100, 20, ALIGN_BEGIN, ALIGN_BEGIN, 0,
														UI_REF(xgb_xgx_slider)
													)
												),
												UI_REF(_g__data.list),
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

static char page_num[][4] = {
	(char[4]) { "1" },
	(char[4]) { "2" },
	(char[4]) { "3" },
	(char[4]) { "4" },
	(char[4]) { "5" },
	(char[4]) { "6" },
	(char[4]) { "7" },
	(char[4]) { "8" },
	(char[4]) { "9" },
	(char[4]) { "10" },
};

#define PAGE_ELEMENT(num) \
	UI_REF( \
		UI_LIST(0, 4, \
			UI_CHILDREN( \
				UI_REF( \
					UI_BOX(INHERIT_PARENT, INHERIT_CHILD, ALIGN_MIDDLE, ALIGN_MIDDLE, 0, \
						UI_REF( \
							UI_TEXT(page_num[num]) \
						) \
					) \
				), \
				UI_REF( \
					UI_COLOR_BOX(75, 128, 0x777777) \
				), \
			) \
		) \
	) \

#define PAGE_SCROLL(_color) \
	UI_REF( \
		UI_BOX(30, 12, ALIGN_MIDDLE, ALIGN_MIDDLE, 0, \
			UI_REF( \
				UI_COLOR_BOX(INHERIT_PARENT, 4, _color) \
			) \
		) \
	)

#define UI_ROTARY(_horizontal, _space, _children, ...) \
	(struct List) { \
		.base = { \
			.get_width = ui_list_get_width, \
			.get_height = ui_list_get_height, \
			.layout = ui_list_layout, \
			.layout_children = ui_list_layout_children, \
			.render = ui_list_render, \
			.pick = ui_dummy_pick, \
			.handle_cursor_event = ui_rotary_handle_cursor_event, \
		}, \
		.horizontal = _horizontal, \
		.space = _space, \
		.children = UI_CHILDREN_SIZE(_children), \
		.child = _children, \
		__VA_ARGS__ \
	}

static struct List pages_thumbs = UI_ROTARY(1, 4,
	UI_CHILDREN(
		PAGE_ELEMENT(0),
		PAGE_ELEMENT(1),
		PAGE_ELEMENT(2),
		PAGE_ELEMENT(3),
		PAGE_ELEMENT(4),
		PAGE_ELEMENT(5),
		PAGE_ELEMENT(6),
		PAGE_ELEMENT(7),
		PAGE_ELEMENT(8),
		PAGE_ELEMENT(9),
	),
	.generate = ui_rotary_generate,
	.has_min = 1,
	.index_min = 0,
	.has_max = 1,
	.index_max = 21,
);

static void scroll_pages(struct List *obj, int delta);

static struct List pages_scroll = UI_ROTARY(0, 10,
	UI_CHILDREN(
		PAGE_SCROLL(0xff0000),
		PAGE_SCROLL(0xffff00),
		PAGE_SCROLL(0xff00ff),
		PAGE_SCROLL(0xffffff),
		PAGE_SCROLL(0x00ff00),
		PAGE_SCROLL(0x00ffff),
		PAGE_SCROLL(0x0000ff),
		PAGE_SCROLL(0x000000),
	),
	.on_scroll = scroll_pages,
);

static void scroll_pages(struct List *obj, int delta)
{
	ui_list_scroll(&pages_scroll, -ui_list_scroll(&pages_thumbs, -delta));
}

static struct List pages = UI_LIST(1, 4,
	UI_CHILDREN(
		UI_REF(
			UI_BOX(INHERIT_PARENT, INHERIT_CHILD, ALIGN_BEGIN, ALIGN_BEGIN, 0x01579b,
				UI_REF(pages_thumbs)
			)
		),
		UI_REF(
			UI_BOX(INHERIT_CHILD, INHERIT_PARENT, ALIGN_BEGIN, ALIGN_BEGIN, 0x01579b,
				UI_REF(pages_scroll)
			)
		),
	)
);

static struct BoxExpander pages_expander = UI_BOX_EXPANDER(UI_BOX_RAW(0, 0, ALIGN_BEGIN, ALIGN_MIDDLE, 0x01579b, UI_REF(pages)), INHERIT_CHILD, INHERIT_CHILD);

struct List app = UI_LIST(0, 4,
	UI_CHILDREN(
		UI_REF(top_panel),
		UI_REF(settings_expander),
		UI_REF(pages_expander),
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

static void ui_button_bitmap_pages_activate(struct Base *base)
{
	struct BoxExpander *be = &pages_expander;
	int w = be->width;
	int h = be->height;

	be->width = be->super.width;
	be->height = be->super.height;

	be->super.width = w;
	be->super.height = h;

	ui_invalidate(be->super.base.parent);
}

static void ui_brush_size_process_event(struct Slider *obj, int value)
{
	settings_brush_circle.radius = value;

	ui_invalidate(&settings_brush_circle.base);

	ui_text_set(&settings_brush_text, "%d", value % 100);
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

	app.base.lvp = app.base.vp = app.base.svp = app.base.dirty = rect_new(0, 0, H_RES, V_RES);

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
