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

static struct Pair pair_move(struct Pair p1, struct Pair p2)
{
	struct Pair p = {
		.x = p1.x + p2.x,
		.y = p1.y + p2.y,
	};
	return p;
}

struct Rect {
	int x1;
	int y1;
	int x2;
	int y2;
};

static struct Rect rect_new(int sx1, int sy1, int sx2, int sy2)
{
	struct Rect rect = {
		.x1 = sx1,
		.y1 = sy1,
		.x2 = sx2,
		.y2 = sy2,
	};
	return rect;
}

static struct Rect rect_new2(struct Pair coord, struct Pair size)
{
	struct Rect rect = {
		.x1 = coord.x,
		.y1 = coord.y,
		.x2 = coord.x + size.w - 1,
		.y2 = coord.y + size.h - 1,
	};
	return rect;
}

static struct Rect rect_move(struct Rect r, struct Pair p)
{
	struct Rect rect = {
		.x1 = r.x1 + p.x,
		.y1 = r.y1 + p.y,
		.x2 = r.x2 + p.x,
		.y2 = r.y2 + p.y,
	};
	return rect;
}

static struct Rect rect_intersect(struct Rect s, struct Rect r)
{
	struct Rect rect = {
		.x1 = MAX(s.x1, r.x1),
		.y1 = MAX(s.y1, r.y1),
		.x2 = MIN(s.x2, r.x2),
		.y2 = MIN(s.y2, r.y2),
	};
	return rect;
}

static struct Rect rect_union(struct Rect s, struct Rect r)
{
	struct Rect rect = {
		.x1 = MIN(s.x1, r.x1),
		.y1 = MIN(s.y1, r.y1),
		.x2 = MAX(s.x2, r.x2),
		.y2 = MAX(s.y2, r.y2),
	};
	return rect;
}

static struct Rect rect_mi(struct Rect s, struct Rect pos, struct Rect d)
{
	struct Rect rect = {
		.x1 = MAX(s.x1 + pos.x1, d.x1),
		.y1 = MAX(s.y1 + pos.y1, d.y1),
		.x2 = MIN(s.x2 + pos.x1, d.x2),
		.y2 = MIN(s.y2 + pos.y1, d.y2),
	};
	return rect;
}

static int rect_width(struct Rect r)
{
	return r.x2 - r.x1 + 1;
}

static int rect_height(struct Rect r)
{
	return r.y2 - r.y1 + 1;
}

static struct Pair rect_size(struct Rect r)
{
	return pair_new(rect_width(r), rect_height(r));
}

static struct Pair rect_coord(struct Rect r)
{
	return pair_new(r.x1, r.y1);
}

static int rect_valid(struct Rect r)
{
	return r.x1 <= r.x2 && r.y1 <= r.y2;
}

static int rect_hit(struct Rect r, struct Pair p)
{
	return r.x1 <= p.x && p.x <= r.x2 && r.y1 <= p.y && p.y <= r.y2;
}

static void rect_dump(struct Rect r)
{
#ifndef FPGA
	printf("%i, %i, %i, %i\n", r.x1, r.y1, r.x2, r.y2);
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

	for (int i = r.y1; i <= r.y2; i++) {
		for (int j = r.x1; j <= r.x2; j++) {
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

	int len = horizontal ? r.x2 - r.x1 : r.y2 - r.y1;

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
		for (int i = r.y1; i <= r.y2; i++) {
			int32_t cs_r2 = cs_r;
			int32_t cs_g2 = cs_g;
			int32_t cs_b2 = cs_b;
			int hz_r = 0;
			int hz_g = 0;
			int hz_b = 0;

			for (int j = r.x1; j <= r.x2; j++) {
				uint32_t color = (cs_r2 << 16) | (cs_g2 << 8) | (cs_b2 << 0);

				draw_pixel_check(s, pair_new(j, i), color);

				hz_r += len_r;
				if (hz_r >= len) {
					hz_r -= diff_r;
					cs_r2 = CLAMP(cs_r2 + slop_r, 0, 255);
				}
				hz_g += len_g;
				if (hz_g >= len) {
					hz_g -= diff_g;
					cs_g2 = CLAMP(cs_g2 + slop_g, 0, 255);
				}
				hz_b += len_b;
				if (hz_b >= len) {
					hz_b -= diff_b;
					cs_b2 = CLAMP(cs_b2 + slop_b, 0, 255);
				}
			}
		}
	} else {
		int32_t cs_r2 = cs_r;
		int32_t cs_g2 = cs_g;
		int32_t cs_b2 = cs_b;
		int hz_r = 0;
		int hz_g = 0;
		int hz_b = 0;

		for (int i = r.y1; i <= r.y2; i++) {
			uint32_t color = (cs_r2 << 16) | (cs_g2 << 8) | (cs_b2 << 0);

			for (int j = r.x1; j <= r.x2; j++) {
				draw_pixel_check(s, pair_new(j, i), color);
			}

			hz_r += len_r;
			if (hz_r >= len) {
				hz_r -= diff_r;
				cs_r2 = CLAMP(cs_r2 + slop_r, 0, 255);
			}
			hz_g += len_g;
			if (hz_g >= len) {
				hz_g -= diff_g;
				cs_g2 = CLAMP(cs_g2 + slop_g, 0, 255);
			}
			hz_b += len_b;
			if (hz_b >= len) {
				hz_b -= diff_b;
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
	struct Rect offset = rect_move(rect_intersect(r, s), pair_new(-r.x1, -r.y1));

	if (!rect_valid(offset)) {
		return;
	}

	int w = (rect_width(r) + 7) / 8;

	for (int i = offset.y1; i <= offset.y2; i++) {
		for (int j = offset.x1; j <= offset.x2; j++) {
			int idx = i * w * 8 + j;

			if (bitmap[idx / 8] & ((uint8_t)0x80 >> (idx % 8))) {
				struct Pair p = pair_new(r.x1 + j, r.y1 + i);

				fb[p.y * H_RES + p.x] = color;
			}
		}
	}
}

static void draw_bitmap2(struct Pair pvp, struct Rect svp, struct BitmapClass *font, int index, uint32_t color)
{
	struct BitmapData *bd = &font->bd[index];
	struct Rect offset = rect_intersect(rect_move(rect_new(0, 0, bd->bb.w - 1, bd->bb.h - 1), pvp), svp);

	if (!rect_valid(offset)) {
		return;
	}

	offset = rect_move(offset, pair_new(-pvp.x, -pvp.y));

	int wi = (bd->bb.w + 7) / 8;

	for (int i = offset.y1; i <= offset.y2; i++) {
		for (int j = offset.x1; j <= offset.x2; j++) {
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

	for (int i = r.y1; i <= r.y2; i++) {
		int cx = cnt;

		for (int j = r.x1; j <= r.x2; j++) {
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

		draw_bitmap(rect_move(rect_new(0, 0, FONT_W - 1, FONT_H - 1), pair_new(r.x1 + k * FONT_W, r.y1)), s, color, &font[idx]);
	}
}

#include "icon.h"

static struct Rect dirty;

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
	/* Area you are supposed to draw */
	struct Rect vp;
	/* Scissored area (svp = vp * svp), respect it */
	struct Rect svp;

	/* Parent may ask the element about its sizes */
	int (*get_width)(struct Base *obj);
	int (*get_height)(struct Base *obj);

	/* Parent assigns you the vp and svp,
	 * you just need to layout everything you want into the vp */
	void (*layout)(struct Base *obj);
	/* Draw whatever you want in you svp * dirty box */
	void (*render)(struct Base *obj);

	/* Return something if point p is inside yours svp */
	struct Base *(*pick)(struct Base *obj, struct Pair p);

	void (*handle_cursor_event)(struct Base *base, enum EventCursorType type, struct Pair p);
	void (*handle_activate_event)(struct Base *base, enum EventActivateType type);
};

enum Align {
	ALIGN_BEGIN,
	ALIGN_MIDDLE,
	ALIGN_END,
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

	/* Area for the content is width/height - 2 * padding */
	int padding;

	/* Content placement inside the width/height - 2 * padding area */
	enum Align h_align;
	enum Align v_align;

	struct Base *content;
};

enum UIButtonType {
	UI_BUTTON_NORMAL,
	UI_BUTTON_SWITCH,
	UI_BUTTON_STICK,
};

struct Bitmap {
	struct Base base;

	uint32_t bg_color;

	enum Align h_align;
	enum Align v_align;

	struct BitmapClass *bc;
	int index;
	uint32_t color;

	struct Rect vp;
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

	struct {
		struct Base *c;
		int size;
	} knob, line;

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

struct ColorPicker {
	struct Base base;

	struct Slider *gb_rrg;
	struct Slider *gb_rgg;
	struct Slider *gb_ggb;
	struct Slider *gb_gbb;
	struct Slider *gb_bbr;
	struct Slider *gb_brr;

	struct Slider *gb_bc;
	struct Slider *gb_cw;
};

/* Helper function for parents */
static void ui_layout_child(struct Base *base, struct Base *child, struct Rect vp, struct Rect svp)
{
	child->vp = vp;
	child->svp = svp;

	if (child->layout) {
		child->layout(child);
	}

	if (!rect_valid(dirty)) {
		dirty = svp;
	} else {
		dirty = rect_union(dirty, svp);
	}
}

static void ui_render(struct Base *base)
{
	base->render(base);
}

static struct Base *ui_pick(struct Base *base, struct Pair p)
{
	/* Don't want to be picked */
	if (!base->pick) {
		return NULL;
	}

	return base->pick(base, p);
}

static struct Base *ui_dummy_pick(struct Base *base, struct Pair p)
{
	if (rect_hit(base->svp, p)) {
		return base;
	}
	return NULL;
}

static int ui_dummy_inherit_parent(struct Base *base)
{
	return INHERIT_PARENT;
}

static struct Base *new_pick;
static struct Base *picked;

static void ui_cursor_down(struct Base *base, int x, int y)
{
	new_pick = base->pick(base, pair_new(x, y));

	if (picked && picked != new_pick) {
		picked->handle_activate_event(picked, EV_DEACTIVATE);
	}

	picked = new_pick;

	if (picked) {
		picked->handle_cursor_event(picked, EV_CURSOR_DOWN, pair_new(x, y));
	}
}

static void ui_cursor_move(int x, int y)
{
	if (!picked) {
		return;
	}

	picked->handle_cursor_event(picked, EV_CURSOR_MOVE, pair_new(x, y));
}

static void ui_cursor_up(struct Base *base, int x, int y)
{
	if (!picked) {
		return;
	}

	picked->handle_cursor_event(picked, EV_CURSOR_UP, pair_new(x, y));

	new_pick = base->pick(base, pair_new(x, y));

	if (picked == new_pick) {
		picked->handle_activate_event(picked, EV_ACTIVATE);
	} else {
		picked->handle_activate_event(picked, EV_CANCEL);
	}

	picked = NULL;
}

static void ui_process(struct Base *base, struct Rect screen)
{
	memset(&dirty, 0, sizeof(dirty));

	base->vp = screen;
	base->svp = screen;

	/* They enlarge dirty region */
	base->layout(base);

	dirty = rect_intersect(dirty, screen);
	if (!rect_valid(dirty)) {
		return;
	}

	draw_rect(dirty, 0x01579b);

	base->render(base);
}

static int ui_box_get_width(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	if (obj->width == INHERIT_PARENT) {
		return INHERIT_PARENT;
	}

	if (obj->width == INHERIT_CHILD) {
		int width = obj->content->get_width(obj->content);

		if (width == INHERIT_PARENT) {
			return INHERIT_PARENT;
		}

		return width + obj->padding * 2;
	}

	return obj->width;
}

static int ui_box_get_height(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	if (obj->height == INHERIT_PARENT) {
		return INHERIT_PARENT;
	}

	if (obj->height == INHERIT_CHILD) {
		int height = obj->content->get_height(obj->content);

		if (height == INHERIT_PARENT) {
			return INHERIT_PARENT;
		}

		return height + obj->padding * 2;
	}

	return obj->height;
}

static void ui_box_layout(struct Base *base)
{
	struct Box *obj = (struct Box *)base;
	struct Pair layout_size = rect_size(base->vp);
	struct Rect vp = rect_new(obj->padding, obj->padding, layout_size.w - 1 - obj->padding, layout_size.h - 1 - obj->padding);
	struct Pair content_size = pair_new(obj->content->get_width(obj->content), obj->content->get_height(obj->content));

	if (content_size.w < 0) {
		content_size.w = layout_size.w;
	}

	if (content_size.h < 0) {
		content_size.h = layout_size.h;
	}

	struct Rect cvp = rect_move(rect_new(0, 0, content_size.w - 1, content_size.h - 1), pair_new(obj->padding, obj->padding));

	if (obj->h_align) {
		int offset = rect_width(vp) - rect_width(cvp);

		if (obj->h_align == ALIGN_MIDDLE) {
			offset /= 2;
		}

		cvp = rect_move(cvp, pair_new(offset, 0));
	}

	if (obj->v_align) {
		int offset = rect_height(vp) - rect_height(cvp);

		if (obj->v_align == ALIGN_MIDDLE) {
			offset /= 2;
		}

		cvp = rect_move(cvp, pair_new(0, offset));
	}

	vp = rect_move(vp, rect_coord(base->vp));
	cvp = rect_move(cvp, rect_coord(base->vp));

	ui_layout_child(base, obj->content, cvp, rect_intersect(cvp, vp));
}

static void ui_box_render(struct Base *base)
{
	struct Box *obj = (struct Box *)base;
	struct Rect r = rect_intersect(base->svp, dirty);

	if (!rect_valid(r)) {
		return;
	}

	ui_render(obj->content);
}

static struct Base *ui_box_pick(struct Base *base, struct Pair p)
{
	struct Box *obj = (struct Box *)base;

	if (!rect_hit(base->svp, p)) {
		return NULL;
	}

	return ui_pick(obj->content, p);
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

static void ui_color_box_render(struct Base *base)
{
	struct ColorBox *obj = (struct ColorBox *)base;
	struct Rect r = rect_intersect(base->svp, dirty);

	if (!rect_valid(r)) {
		return;
	}

	draw_rect(r, obj->color);
}

static void ui_color_gradient_render(struct Base *base)
{
	struct GradientBox *obj = (struct GradientBox *)base;
	struct Rect r = rect_intersect(base->svp, dirty);

	if (!rect_valid(r)) {
		return;
	}

	draw_rect_gradient(base->vp, r, obj->super.color, obj->color_end, obj->horizontal);
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

static void ui_bitmap_layout(struct Base *base)
{
	struct Bitmap *obj = (struct Bitmap *)base;
	struct Rect vp = base->vp;
	struct Rect cvp = rect_new(0, 0, obj->bc->bd[obj->index].bb.w - 1, obj->bc->bd[obj->index].bb.h - 1);

	if (obj->h_align) {
		int offset = rect_width(vp) - rect_width(cvp);

		if (obj->h_align == ALIGN_MIDDLE) {
			offset /= 2;
		}

		cvp = rect_move(cvp, pair_new(offset, 0));
	}

	if (obj->v_align) {
		int offset = rect_height(vp) - rect_height(cvp);

		if (obj->v_align == ALIGN_MIDDLE) {
			offset /= 2;
		}

		cvp = rect_move(cvp, pair_new(0, offset));
	}

	obj->vp = rect_move(cvp, rect_coord(base->vp));
}

static void ui_bitmap_render(struct Base *base)
{
	struct Bitmap *obj = (struct Bitmap *)base;
	struct Rect r = rect_intersect(base->svp, dirty);

	if (!rect_valid(r)) {
		return;
	}

	draw_rect(r, obj->bg_color);

	draw_bitmap2(rect_coord(obj->vp), r, obj->bc, obj->index, obj->color);
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

static void ui_text_render(struct Base *base)
{
	struct Text *obj = (struct Text *)base;
	struct Rect r = rect_intersect(base->svp, dirty);

	if (!rect_valid(r)) {
		return;
	}

	draw_text(base->vp, r, obj->color, obj->text);
}

static void ui_slider_layout(struct Base *base)
{
	struct Slider *obj = (struct Slider *)base;
	struct Pair size = rect_size(base->vp);

	if (obj->horizontal) {
		obj->len = base->vp.x2 - base->vp.x1;
	} else {
		obj->len = base->vp.y2 - base->vp.y1;
	}

	obj->len -= obj->knob.size;
	/* So that we have convenient ranges 1: [1, 1.5], 2: [1.5, 2.5], ... */
	obj->frac = obj->len / (2 * (obj->max - obj->min));

	{
		struct Base *knob = obj->knob.c;
		int s = obj->knob.size;
		int x = 0;
		int y = 0;
		int w = size.w;
		int h = size.h;

		if (s < 0) {
			if (obj->horizontal) {
				s = h;
			} else {
				s = w;
			}
		}

		if (obj->horizontal) {
			x = (obj->value - obj->min) * obj->len / (obj->max - obj->min);
			w = s;
		} else {
			y = (obj->value - obj->min) * obj->len / (obj->max - obj->min);
			h = s;
		}

		struct Rect vp = rect_move(rect_new(0, 0, w - 1, h - 1), pair_new(x + base->vp.x1, base->vp.y2 - y - obj->knob.size));
		struct Rect svp = rect_intersect(vp, base->svp);

		ui_layout_child(base, knob, vp, svp);
	}

	{
		struct Base *line = obj->line.c;
		int s = obj->line.size;
		int x = 0;
		int y = 0;
		int w = size.w;
		int h = size.h;

		if (s < 0) {
			if (obj->horizontal) {
				s = h;
			} else {
				s = w;
			}
		}

		if (obj->horizontal) {
			y = (h - s) / 2;
			h = s;
		} else {
			x = (w - s) / 2;
			w = s;
			h = obj->len;
		}

		struct Rect vp = rect_move(rect_new(0, 0, w - 1, h - 1), pair_new(x + base->vp.x1, y + base->vp.y1 + obj->knob.size / 2));
		struct Rect svp = rect_intersect(vp, base->svp);

		ui_layout_child(base, line, vp, svp);
	}
}

static void ui_slider_render(struct Base *base)
{
	struct Slider *obj = (struct Slider *)base;
	struct Rect r = rect_intersect(base->svp, dirty);

	if (!rect_valid(r)) {
		return;
	}

	ui_render(obj->line.c);
	ui_render(obj->knob.c);
}

static struct Base *ui_slider_pick(struct Base *base, struct Pair p)
{
	struct Slider *obj = (struct Slider *)base;

	if (rect_hit(obj->knob.c->svp, p)) {
		return base;
	}

	if (rect_hit(base->svp, p)) {
		return base;
	}

	return NULL;
}

static void ui_slider_handle_cursor_event(struct Base *base, enum EventCursorType type, struct Pair p)
{
	struct Slider *obj = (struct Slider *)base;
	int value;

	switch (type) {
	case EV_CURSOR_DOWN:
	case EV_CURSOR_MOVE:
		if (obj->horizontal) {
			value = obj->min + (obj->max - obj->min) * (p.x - (base->vp.x1 + obj->knob.size / 2) - obj->frac) / obj->len;
		} else {
			value = obj->min + (obj->max - obj->min) * (base->vp.y2 - obj->knob.size - (p.y - obj->knob.size / 2 - obj->frac)) / obj->len;
		}

		obj->value = CLAMP(value, obj->min, obj->max);

		obj->on_changed(obj->value);
		break;
	default:
		break;
	}
}

static void ui_slider_handle_activate_event(struct Base *base, enum EventActivateType type)
{
}

static int ui_list_get_width(struct Base *base)
{
	struct List *obj = (struct List *)base;
	int w_max = INHERIT_PARENT;

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];
		int cw = c->get_width(c);

		if (obj->horizontal) {
			if (cw < 0) {
				return cw;
			}

			w_max += cw;
		} else {
			if (cw > w_max) {
				w_max = cw;
			}
		}
	}

	if (obj->horizontal) {
		w_max += (obj->children - 1) * obj->space;
	}

	return w_max;
}

static int ui_list_get_height(struct Base *base)
{
	struct List *obj = (struct List *)base;
	int h_max = INHERIT_PARENT;

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];
		int ch = c->get_height(c);

		if (obj->horizontal) {
			if (ch > h_max) {
				h_max = ch;
			}
		} else {
			if (ch < 0) {
				return ch;
			}

			h_max += ch;
		}
	}

	if (!obj->horizontal) {
		h_max += (obj->children - 1) * obj->space;
	}

	return h_max;
}

static void ui_list_layout(struct Base *base)
{
	struct List *obj = (struct List *)base;
	struct Pair size = rect_size(base->vp);

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
				memset(&c->vp, 0, sizeof(c->vp)); \
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
			c->vp = rect_move(rect_new(0, 0, width - 1, height - 1), pair_new(x + base->vp.x1, y + base->vp.y1)); \
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

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];

		ui_layout_child(base, c, c->vp, rect_intersect(c->vp, base->svp));
	}
}

static void ui_list_render(struct Base *base)
{
	struct List *obj = (struct List *)base;
	struct Rect r = rect_intersect(base->svp, dirty);

	if (!rect_valid(r)) {
		return;
	}

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];

		ui_render(c);
	}
}

static struct Base *ui_list_pick(struct Base *base, struct Pair p)
{
	struct List *obj = (struct List *)base;

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];
		struct Base *ret = ui_pick(c, p);

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

static void ui_circle_render(struct Base *base)
{
	struct Circle *obj = (struct Circle *)base;
	struct Rect r = rect_intersect(base->svp, dirty);

	if (!rect_valid(r)) {
		return;
	}

	draw_circle(r, pair_new((base->vp.x1 + base->vp.x2) / 2, (base->vp.y1 + base->vp.y2) / 2), obj->radius, obj->color, 1);
}

static void ui_color_picker_layout(struct Base *base)
{
}

static void ui_color_picker_render(struct Base *base)
{
}

static void ui_color_picker_cursor_event(struct Base *base, enum EventCursorType type, struct Pair p)
{
}

static void ui_color_picker_activate_event(struct Base *base, enum EventActivateType type)
{
}

#define UI_REF(child) ((struct Base *)&child)
#define UI_CHILDREN(...) ((struct Base *[]) { __VA_ARGS__ })
#define UI_CHILDREN_SIZE(a) (sizeof(a) / sizeof(struct Base *))

#define UI_BOX_RAW(_width, _height, _padding, _h_align, _v_align, _content) \
	{ \
		.base = { \
			.get_width = ui_box_get_width, \
			.get_height = ui_box_get_height, \
			.layout = ui_box_layout, \
			.render = ui_box_render, \
			.pick = ui_box_pick, \
		}, \
		.width = _width, \
		.height = _height, \
		.padding = _padding, \
		.h_align = _h_align, \
		.v_align = _v_align, \
		.content = _content, \
	}

#define UI_BOX(_width, _height, _padding, _h_align, _v_align,_content) \
	(struct Box)UI_BOX_RAW(_width, _height, _padding, _h_align, _v_align,_content)

#define UI_COLOR_BOX(_width, _height, _color) \
	(struct ColorBox) { \
		.base = { \
			.get_width = ui_color_box_get_width, \
			.get_height = ui_color_box_get_height, \
			.render = ui_color_box_render, \
		}, \
		.width = _width, \
		.height = _height, \
		.color = _color, \
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
			.bg_color = 0x01579b, \
			.h_align = ALIGN_MIDDLE, \
			.v_align = ALIGN_MIDDLE, \
		}, \
		.type = _type, \
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

#define UI_LIST(_horizontal, _children) \
	(struct List) { \
		.base = { \
			.get_width = ui_list_get_width, \
			.get_height = ui_list_get_height, \
			.layout = ui_list_layout, \
			.render = ui_list_render, \
			.pick = ui_list_pick, \
		}, \
		.horizontal = _horizontal, \
		.space = 4, \
		.children = UI_CHILDREN_SIZE(_children), \
		.child = _children, \
	}

#define UI_SLIDER(_horizontal, ...) \
	(struct Slider) { \
		.base = { \
			.get_width = ui_dummy_inherit_parent, \
			.get_height = ui_dummy_inherit_parent, \
			.layout = ui_slider_layout, \
			.render = ui_slider_render, \
			.pick = ui_slider_pick, \
			.handle_cursor_event = ui_slider_handle_cursor_event, \
			.handle_activate_event = ui_slider_handle_activate_event, \
		}, \
		.horizontal = _horizontal, \
		.value = 1, \
		.min = 1, \
		.max = 16, \
		.line = { \
			.size = 4, \
			.c = UI_REF(UI_COLOR_BOX(INHERIT_PARENT, INHERIT_PARENT, 0)), \
		}, \
		.knob = { \
			.size = 4, \
			.c = UI_REF(UI_COLOR_BOX(INHERIT_PARENT, INHERIT_PARENT, 0xeff4ff)), \
		}, \
		__VA_ARGS__ \
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

#define UI_SLIDER_GRADIENT(_horizontal, _color_start, _color_end, ...) \
	(struct Slider) { \
		.base = { \
			.get_width = ui_dummy_inherit_parent, \
			.get_height = ui_dummy_inherit_parent, \
			.layout = ui_slider_layout, \
			.render = ui_slider_render, \
			.pick = ui_slider_pick, \
			.handle_cursor_event = ui_slider_handle_cursor_event, \
			.handle_activate_event = ui_slider_handle_activate_event, \
		}, \
		.horizontal = _horizontal, \
		.value = 0, \
		.min = 0, \
		.max = 100, \
		.line = { \
			.size = -1, \
			.c = UI_REF(UI_GRADIENT_BOX(INHERIT_PARENT, INHERIT_PARENT, _color_start, _color_end, _horizontal)), \
		}, \
		.knob = { \
			.size = 0, \
			.c = UI_REF(UI_COLOR_BOX(INHERIT_PARENT, INHERIT_PARENT, 0xeff4ff)), \
		}, \
		__VA_ARGS__ \
	}

#define UI_COLOR_PICKER() \
	(struct ColorPicker) { \
		.base = { \
			.get_width = ui_dummy_inherit_parent, \
			.get_height = ui_dummy_inherit_parent, \
			.layout = ui_color_picker_layout, \
			.render = ui_color_picker_render, \
			.pick = ui_dummy_pick, \
			.handle_cursor_event = ui_color_picker_cursor_event, \
			.handle_activate_event = ui_color_picker_activate_event, \
		}, \
		.gb_rrg = &UI_SLIDER_GRADIENT(0, 0x000000, 0xffffff, 0), \
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

static struct BoxExpander settings_expander = UI_BOX_EXPANDER(UI_BOX_RAW(0, 0, 0, ALIGN_BEGIN, ALIGN_MIDDLE, UI_REF(settings)), INHERIT_CHILD, 140);

static void ui_button_bitmap_settings_activate(struct Base *base);

static struct List top_panel = UI_LIST(1,
	UI_CHILDREN(
		UI_REF(
			UI_BOX(INHERIT_PARENT, INHERIT_CHILD, 0, ALIGN_BEGIN, ALIGN_BEGIN,
				UI_REF(
					UI_LIST(1,
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
			UI_BOX(INHERIT_PARENT, INHERIT_CHILD, 0, ALIGN_MIDDLE, ALIGN_BEGIN,
				UI_REF(
					UI_LIST(1,
						UI_CHILDREN(
							UI_REF(UI_BUTTON_BITMAP(8, UI_BUTTON_NORMAL)),
							UI_REF(UI_BOX(INHERIT_CHILD, INHERIT_CHILD, 0, ALIGN_MIDDLE, ALIGN_MIDDLE, UI_REF(UI_TEXT("  7 / 101")))),
							UI_REF(UI_BUTTON_BITMAP(4, UI_BUTTON_NORMAL)),
						)
					)
				)
			)
		),
		UI_REF(
			UI_BOX(INHERIT_PARENT, INHERIT_CHILD, 0, ALIGN_END, ALIGN_BEGIN,
				UI_REF(
					UI_LIST(1,
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

static uint32_t brush_color_hue = 0xffff00;
static int brush_color_saturation = 0;
static int brush_color_value = 100;
static char color_rgb_text[12] = "255 255 255";
static struct Text color_rgb = UI_TEXT(color_rgb_text);
static struct Slider ui_color_picker_value = UI_SLIDER_GRADIENT(1, 0x000000, 0xff8800, .on_changed = ui_color_picker_value_changed);
static struct Slider ui_color_picker_saturation = UI_SLIDER_GRADIENT(1, 0x884400, 0x888888, .on_changed = ui_color_picker_saturation_changed);

static struct ColorBox brush_color[4] = {
	UI_COLOR_BOX(20, 20, 0xeff4ff),
	UI_COLOR_BOX(20, 20, 0xeff4ff),
	UI_COLOR_BOX(20, 20, 0xeff4ff),
	UI_COLOR_BOX(20, 20, 0xeff4ff),
};

static void ui_color_picker_changed(void)
{
	uint32_t color = brush_color_hue;

	uint8_t r = (color >> 16) & 0xff;
	uint8_t g = (color >> 8) & 0xff;
	uint8_t b = (color >> 0) & 0xff;

	r = r * brush_color_value / 100;
	g = g * brush_color_value / 100;
	b = b * brush_color_value / 100;

	uint32_t l = MAX(MAX(r, g), b);

	r = (r * (100 - brush_color_saturation) + l * brush_color_saturation) / 100;
	g = (g * (100 - brush_color_saturation) + l * brush_color_saturation) / 100;
	b = (b * (100 - brush_color_saturation) + l * brush_color_saturation) / 100;

	snprintf(color_rgb_text, sizeof(color_rgb_text), "%d %d %d", r, g, b);

	struct GradientBox *cv = (struct GradientBox *)ui_color_picker_value.line.c;

	cv->color_end = brush_color_hue;

	struct GradientBox *cs = (struct GradientBox *)ui_color_picker_saturation.line.c;

	cs->super.color = brush_color_hue;
	cs->color_end = l | (l << 8) | (l << 16);

	for (int i = 0; i < ARRAY_SIZE(brush_color); i++) {
		brush_color[i].color = (r << 16) | (g << 8) | b;
	}
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

static struct List settings = UI_LIST(1,
	UI_CHILDREN(
		UI_REF(
			UI_BOX(240, INHERIT_CHILD, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
				UI_REF(
					UI_LIST(0,
						UI_CHILDREN(
							UI_REF(
								UI_BOX(INHERIT_PARENT, 20, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
									UI_REF(
										UI_LIST(1,
											UI_CHILDREN(
												UI_REF(brush_color[0]),
												UI_REF(
													UI_SLIDER_GRADIENT(1, 0xff00ff, 0xff0000, .on_changed = ui_color_picker_rbr_changed)
												),
												UI_REF(
													UI_SLIDER_GRADIENT(1, 0xff0000, 0xffff00, .on_changed = ui_color_picker_rrg_changed)
												),
												UI_REF(brush_color[1]),
											)
										)
									)
								)
							),
							UI_REF(
								UI_LIST(1,
									UI_CHILDREN(
										UI_REF(
											UI_BOX(20, INHERIT_PARENT, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
												UI_REF(
													UI_SLIDER_GRADIENT(0, 0xff00ff, 0x0000ff, .on_changed = ui_color_picker_rbb_changed)
												)
											)
										),
										UI_REF(
											UI_LIST(0,
												UI_CHILDREN(
													UI_REF(
														UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
															UI_REF(
																UI_COLOR_BOX(0, 0, 0xeff4ff)
															)
														)
													),
													UI_REF(
														UI_BOX(INHERIT_PARENT, 20, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
															UI_REF(
																UI_LIST(1,
																	UI_CHILDREN(
																		UI_REF(ui_color_picker_value),
																		UI_REF(ui_color_picker_saturation),
																	)
																)
															)
														)
													),
													UI_REF(
														UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
															UI_REF(color_rgb)
														)
													),
												)
											)
										),
										UI_REF(
											UI_BOX(20, INHERIT_PARENT, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
												UI_REF(
													UI_SLIDER_GRADIENT(0, 0xffff00, 0x00ff00, .on_changed = ui_color_picker_rgg_changed)
												)
											)
										),
									)
								)
							),
							UI_REF(
								UI_BOX(INHERIT_PARENT, 20, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
									UI_REF(
										UI_LIST(1,
											UI_CHILDREN(
												UI_REF(brush_color[2]),
												UI_REF(
													UI_SLIDER_GRADIENT(1, 0x0000ff, 0x00ffff, .on_changed = ui_color_picker_bgb_changed)
												),
												UI_REF(
													UI_SLIDER_GRADIENT(1, 0x00ffff, 0x00ff00, .on_changed = ui_color_picker_gbg_changed)
												),
												UI_REF(brush_color[3]),
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
			UI_BOX(INHERIT_CHILD, INHERIT_CHILD, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
				UI_REF(
					UI_LIST(0,
						UI_CHILDREN(
							UI_REF(
								UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
									UI_REF(settings_brush_circle)
								)
							),
							UI_REF(
								UI_BOX(50, INHERIT_CHILD, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
									UI_REF(settings_brush_text)
								)
							),
						)
					)
				)
			)
		),
		UI_REF(
			UI_BOX(30, INHERIT_PARENT, 0, ALIGN_MIDDLE, ALIGN_MIDDLE,
				UI_REF(UI_SLIDER(0, .on_changed = ui_brush_size_process_event))
			)
		),
	)
);

struct List app = UI_LIST(0,
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
}

static void ui_brush_size_process_event(int value)
{
	struct Circle *obj = &settings_brush_circle;

	obj->radius = value;

	snprintf(settings_brush_text_raw, sizeof(settings_brush_text_raw), "%d", obj->radius % 100);
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
				ui_cursor_down(&app.base, e.button.x, e.button.y);
				break;
			case SDL_MOUSEMOTION:
				ui_cursor_move(e.motion.x, e.motion.y);
				break;
			case SDL_MOUSEBUTTONUP:
				ui_cursor_up(&app.base, e.button.x, e.button.y);
				break;
			}

			ui_process(&app.base, rect_new(0, 0, H_RES - 1, V_RES - 1));
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
