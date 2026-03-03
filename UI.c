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
	struct Pair point = {
		.x = x,
		.y = y,
	};
	return point;
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

enum EventType {
	EV_CURSOR_DOWN,
	EV_CURSOR_MOVE,
	EV_CURSOR_UP,
	EV_ACTIVATE,
	EV_DEACTIVATE,
};

struct Event {
	enum EventType type;

	struct Pair p;
};

struct Base {
	/* Parent may ask the element about its sizes */
	int (*get_width)(struct Base *obj);
	int (*get_height)(struct Base *obj);

	/* size - your box, layout there as you want */
	void (*layout)(struct Base *obj, struct Pair size);
	/* pvp is just an element's vp in the screen coordinates,
	 * svp - scissored pvp */
	void (*render)(struct Base *obj, struct Rect pvp, struct Rect svp);

	struct Base *(*pick)(struct Base *obj, struct Rect pvp, struct Rect svp, struct Pair p);
	void (*process_event)(struct Base *obj, struct Event ev);
};

struct Bitmap {
	struct BitmapClass *bc;
	int index;
	uint32_t color;
};

struct ChildBox {
	/* This is a place where a parent puts the calculated area for this element */
	struct Rect vp;

	union {
		struct Base *base;
		struct Bitmap *bitmap;
		uint32_t color;
	};
};

enum Align {
	ALIGN_BEGIN,
	ALIGN_MIDDLE,
	ALIGN_END,
};

#define INHERIT_PARENT -1
#define INHERIT_CHILD -2

struct BoxContent {
	int padding;

	enum Align h_align;
	enum Align v_align;

	struct ChildBox content;
};

struct Box {
	struct Base base;

	int width;
	int height;

	struct BoxContent bc;
};

struct PictureBitmap {
	struct Base base;

	uint32_t bg_color;

	struct BoxContent bc;
};

struct Text {
	struct Base base;

	uint8_t *font;
	const char *text;
	uint32_t color;
};

struct List {
	struct Base base;

	int horizontal;
	enum Align align;
	int space;

	int children;
	struct ChildBox *child;
};

struct Slider {
	struct Base base;

	int horizontal;
	int value;
	int frac;

	struct Pair p;
	int pressed;
	int old_value;

	struct {
		int size;
		struct ChildBox *c;
	} knob, line;
};

static void child_box_render(struct ChildBox *c, struct Rect pvp, struct Rect svp)
{
	struct Rect vp = rect_move(c->vp, pair_new(pvp.x1, pvp.y1));

	if (!rect_valid(vp)) {
		return;
	}

	svp = rect_intersect(vp, svp);

	if (!rect_valid(svp)) {
		return;
	}

	c->base->render(c->base, vp, svp);
}

static struct Base *child_box_pick(struct ChildBox *c, struct Rect pvp, struct Rect svp, struct Pair p)
{
	if (c->base->pick == NULL) {
		return NULL;
	}

	struct Rect vp = rect_move(c->vp, pair_new(pvp.x1, pvp.y1));

	if (!rect_valid(vp)) {
		return NULL;
	}

	svp = rect_intersect(vp, svp);

	if (!rect_valid(svp)) {
		return NULL;
	}

	return c->base->pick(c->base, vp, svp, p);
}

static void dummy_layout(struct Base *base, struct Pair size)
{
}

static struct Base *dummy_pick(struct Base *base, struct Rect pvp, struct Rect svp, struct Pair p)
{
	if (rect_hit(svp, p)) {
		return base;
	}
	return NULL;
}

static int color_box_get_width(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	return obj->width;
}

static int color_box_get_height(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	return obj->width;
}

static void box_content_layout(struct BoxContent *bc, struct Pair content_size, struct Pair layout_size)
{
	if (content_size.w < 0) {
		content_size.w = layout_size.w - bc->padding * 2;
	}

	if (content_size.h < 0) {
		content_size.h = layout_size.h - bc->padding * 2;
	}

	struct Rect vp = rect_new(bc->padding, bc->padding, layout_size.w - 1 - bc->padding, layout_size.h - 1 - bc->padding);
	struct Rect cp = rect_new(0, 0, content_size.w - 1, content_size.h - 1);

	cp = rect_move(cp, pair_new(bc->padding, bc->padding));

	if (bc->h_align) {
		int offset = rect_width(vp) - rect_width(cp);

		if (bc->h_align == ALIGN_MIDDLE) {
			offset /= 2;
		}

		cp = rect_move(cp, pair_new(offset, 0));
	}

	if (bc->v_align) {
		int offset = rect_height(vp) - rect_height(cp);

		if (bc->v_align == ALIGN_MIDDLE) {
			offset /= 2;
		}

		cp = rect_move(cp, pair_new(0, offset));
	}

	bc->content.vp = cp;
}

static void color_box_layout(struct Base *base, struct Pair size)
{
	struct Box *obj = (struct Box *)base;

	box_content_layout(&obj->bc, pair_new(obj->width, obj->height), size);
}

static void color_box_render(struct Base *base, struct Rect pvp, struct Rect svp)
{
	struct Box *obj = (struct Box *)base;
	struct BoxContent *bc = &obj->bc;
	struct Rect sv = rect_new(pvp.x1 + bc->padding, pvp.y1 + bc->padding, pvp.x2 - bc->padding, pvp.y2 - bc->padding);
	struct Rect cv = rect_move(bc->content.vp, pair_new(pvp.x1, pvp.y1));

	draw_rect(rect_intersect(cv, sv), obj->bc.content.color);
}

static int bitmap_get_width(struct Base *base)
{
	struct PictureBitmap *obj = (struct PictureBitmap *)base;

	return obj->bc.content.bitmap->bc->size.w + obj->bc.padding * 2;
}

static int bitmap_get_height(struct Base *base)
{
	struct PictureBitmap *obj = (struct PictureBitmap *)base;

	return obj->bc.content.bitmap->bc->size.h + obj->bc.padding * 2;
}

static void bitmap_layout(struct Base *base, struct Pair size)
{
	struct PictureBitmap *obj = (struct PictureBitmap *)base;
	struct Bitmap *b = obj->bc.content.bitmap;
	struct BitmapData *bd = &b->bc->bd[b->index];

	box_content_layout(&obj->bc, bd->bb, size);
}

static void bitmap_render(struct Base *base, struct Rect pvp, struct Rect svp)
{
	struct PictureBitmap *obj = (struct PictureBitmap *)base;
	struct Bitmap *b = obj->bc.content.bitmap;
	struct BoxContent *bc = &obj->bc;
	struct Rect vp = rect_new(pvp.x1 + bc->padding, pvp.y1 + bc->padding, pvp.x2 - bc->padding, pvp.y2 - bc->padding);
	struct Rect cp = rect_move(bc->content.vp, pair_new(pvp.x1, pvp.y1));

	draw_rect(svp, obj->bg_color);

	draw_bitmap2(pair_new(cp.x1, cp.y1), rect_intersect(vp, svp), b->bc, b->index, b->color);
}

static struct Base *bitmap_pick(struct Base *base, struct Rect pvp, struct Rect svp, struct Pair p)
{
	struct PictureBitmap *obj = (struct PictureBitmap *)base;
	struct BoxContent *bc = &obj->bc;
	struct Rect vp = rect_new(pvp.x1 + bc->padding, pvp.y1 + bc->padding, pvp.x2 - bc->padding, pvp.y2 - bc->padding);

	svp = rect_intersect(vp, svp);

	if (rect_hit(svp, p)) {
		return base;
	}

	return NULL;
}

static void bitmap_process_event(struct Base *base, struct Event ev)
{
	struct PictureBitmap *obj = (struct PictureBitmap *)base;

	switch (ev.type) {
	case EV_CURSOR_DOWN:
	case EV_ACTIVATE:
		obj->bg_color = 0x4caf50;
		break;
	case EV_DEACTIVATE:
		obj->bg_color = 0x01579b;
		break;
	default:
		break;
	}
}

static int box_get_width(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	if (obj->width == INHERIT_PARENT) {
		return INHERIT_PARENT;
	}

	if (obj->width == INHERIT_CHILD) {
		int width = obj->bc.content.base->get_width(obj->bc.content.base);

		if (width == INHERIT_PARENT) {
			return INHERIT_PARENT;
		}

		return width + obj->bc.padding * 2;
	}

	return obj->width;
}

static int box_get_height(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	if (obj->height == INHERIT_PARENT) {
		return INHERIT_PARENT;
	}

	if (obj->height == INHERIT_CHILD) {
		int height = obj->bc.content.base->get_height(obj->bc.content.base);

		if (height == INHERIT_PARENT) {
			return INHERIT_PARENT;
		}

		return height + obj->bc.padding * 2;
	}

	return obj->height;
}

static void box_layout(struct Base *base, struct Pair size)
{
	struct Box *obj = (struct Box *)base;
	int w = box_get_width(base);
	int h = box_get_height(base);

	box_content_layout(&obj->bc, pair_new(w, h), size);

	if (obj->bc.content.base == NULL) {
		return;
	}

	obj->bc.content.base->layout(obj->bc.content.base, rect_size(obj->bc.content.vp));
}

static void box_render(struct Base *base, struct Rect pvp, struct Rect svp)
{
	struct Box *obj = (struct Box *)base;

	if (obj->bc.content.base == NULL) {
		return;
	}

	child_box_render(&obj->bc.content, pvp, svp);
}

static struct Base *box_pick(struct Base *base, struct Rect pvp, struct Rect svp, struct Pair p)
{
	struct Box *obj = (struct Box *)base;
	struct BoxContent *bc = &obj->bc;

	if (bc->content.base == NULL) {
		return base;
	}

	struct Rect vp = rect_new(pvp.x1 + bc->padding, pvp.y1 + bc->padding, pvp.x2 - bc->padding, pvp.y2 - bc->padding);

	return child_box_pick(&bc->content, vp, svp, p);
}

static int text_get_width(struct Base *base)
{
	struct Text *obj = (struct Text *)base;

	return FONT_W * strlen(obj->text);
}

static int text_get_height(struct Base *base)
{
	return FONT_H;
}

static void render_text(struct Base *base, struct Rect w, struct Rect s)
{
	struct Text *obj = (struct Text *)base;

	draw_text(w, s, obj->color, obj->text);
}

static void slider_layout(struct Base *base, struct Pair size)
{
	struct Slider *obj = (struct Slider *)base;

	{
		struct ChildBox *knob = obj->knob.c;
		int kw = obj->knob.size;
		int kh = obj->knob.size;
		int x = 0;
		int y = 0;
		int w = size.w;
		int h = size.h;

		if (obj->horizontal) {
			obj->frac = (w - kw);

			x = (w - kw) * obj->value / 100;
			w = kw;
		} else {
			obj->frac = (h - kh);

			y = (h - kh) * (100 - obj->value) / 100;
			h = kh;
		}

		knob->vp = rect_new(x, y, x + w - 1, y + h - 1);
		knob->base->layout(knob->base, rect_size(knob->vp));
	}

	{
		struct ChildBox *line = obj->line.c;
		int lw = obj->line.size;
		int lh = obj->line.size;
		int x = 0;
		int y = 0;
		int w = size.w;
		int h = size.h;

		if (obj->horizontal) {
			y = (h - lh) / 2;
			h = lh;
		} else {
			x = (w - lw) / 2;
			w = lw;
		}

		line->vp = rect_move(rect_new(0, 0, w, h), pair_new(x, y));
		line->base->layout(line->base, rect_size(line->vp));
	}
}

static void slider_render(struct Base *base, struct Rect pvp, struct Rect svp)
{
	struct Slider *obj = (struct Slider *)base;

	child_box_render(obj->line.c, pvp, svp);
	child_box_render(obj->knob.c, pvp, svp);
}

static struct Base *slider_pick(struct Base *base, struct Rect pvp, struct Rect svp, struct Pair p)
{
	struct Slider *obj = (struct Slider *)base;

	if (child_box_pick(obj->knob.c, pvp, svp, p)) {
		return base;
	}

	return NULL;
}

static void slider_process_event(struct Base *base, struct Event ev)
{
	struct Slider *obj = (struct Slider *)base;
	int diff;

	switch (ev.type) {
	case EV_CURSOR_DOWN:
		obj->p = ev.p;
		obj->pressed = 1;
		obj->old_value = obj->value;
		break;
	case EV_CURSOR_MOVE:
		if (obj->pressed) {
			if (obj->horizontal) {
				diff = ev.p.x - obj->p.x;
			} else {
				diff = obj->p.y - ev.p.y;
			}
			obj->value = obj->old_value + (obj->knob.c->vp.x1 + diff) * 100 / obj->frac;

			if (obj->value < 0) {
				obj->value = 0;
			} else if (obj->value > 100) {
				obj->value = 100;
			}
		}
		break;
	case EV_CURSOR_UP:
		obj->pressed = 0;
		break;
	default:
		break;
	}
}

static int list_get_width(struct Base *base)
{
	struct List *obj = (struct List *)base;
	int w_max = INHERIT_PARENT;

	for (int i = 0; i < obj->children; i++) {
		struct ChildBox *c = &obj->child[i];
		int cw = c->base->get_width(c->base);

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

static int list_get_height(struct Base *base)
{
	struct List *obj = (struct List *)base;
	int h_max = INHERIT_PARENT;

	for (int i = 0; i < obj->children; i++) {
		struct ChildBox *c = &obj->child[i];
		int ch = c->base->get_height(c->base);

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

	return h_max;
}

static void list_layout(struct Base *base, struct Pair size)
{
	struct List *obj = (struct List *)base;

#define LIST_LAYOUT(_method, _size, _dim, _dim_size, _dim_offset) \
	do { \
		int x = 0; \
		int y = 0; \
		int even_size = 0; \
		int last_size = 0; \
		int total_size = 0; \
		int iwos = 0; \
		int width = size.x; \
		int height = size.y; \
\
		for (int i = 0; i < obj->children; i++) { \
			struct ChildBox *c = &obj->child[i]; \
			int cs = c->base->_method(c->base); \
\
			if (cs >= 0) { \
				total_size += cs; \
			} else { \
				iwos++; \
			} \
		} \
\
		total_size += (obj->children - 1) * obj->space; \
\
		if (iwos) { \
			even_size = _size - total_size; \
			even_size /= iwos; \
			last_size = _size - total_size - even_size * (iwos - 1); \
		} \
\
		for (int i = 0; i < obj->children; i++) { \
			struct ChildBox *c = &obj->child[i]; \
			_dim_size = c->base->_method(c->base); \
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
			c->vp = rect_new(x, y, x + width - 1, y + height - 1); \
\
			_dim += _dim_size; \
			_dim += obj->space; \
		} \
\
		_dim -= obj->space; \
\
		if (obj->align) { \
			int x_offset = 0; \
			int y_offset = 0; \
\
			_dim_offset = _size - _dim; \
\
			if (_dim_offset > 0) { \
				if (obj->align == ALIGN_MIDDLE) { \
					_dim_offset /= 2; \
				} \
\
				for (int i = 0; i < obj->children; i++) { \
					struct ChildBox *c = &obj->child[i]; \
\
					c->vp = rect_move(c->vp, pair_new(x_offset, y_offset)); \
				} \
			} \
		} \
	} while (0)

	if (obj->horizontal) {
		LIST_LAYOUT(get_width, size.w, x, width, x_offset);
	} else {
		LIST_LAYOUT(get_height, size.h, y, height, y_offset);
	}

	for (int i = 0; i < obj->children; i++) {
		struct ChildBox *c = &obj->child[i];

		c->base->layout(c->base, rect_size(c->vp));
	}
}

static void list_render(struct Base *base, struct Rect pvp, struct Rect svp)
{
	struct List *obj = (struct List *)base;

	for (int i = 0; i < obj->children; i++) {
		struct ChildBox *c = &obj->child[i];

		child_box_render(c, pvp, svp);
	}
}

static struct Base *list_pick(struct Base *base, struct Rect pvp, struct Rect svp, struct Pair p)
{
	struct List *obj = (struct List *)base;

	for (int i = 0; i < obj->children; i++) {
		struct ChildBox *c = &obj->child[i];
		struct Base *ret = child_box_pick(c, pvp, svp, p);

		if (ret) {
			return ret;
		}
	}

	return NULL;
}

#define GROUP(...) { __VA_ARGS__ }

#define UI_CHILD(child) { .base = (struct Base *)&child }
#define UI_CHILDREF(child) &(struct ChildBox) { .base = (struct Base *)&child }

#define UI_LIST(_horizontal, _align, _children, ...) \
	(struct List) { \
		.base = { \
			.get_width = list_get_width, \
			.get_height = list_get_height, \
			.layout = list_layout, \
			.render = list_render, \
			.pick = list_pick, \
		}, \
		.horizontal = _horizontal, \
		.align = _align, \
		.space = 4, \
		.children = _children, \
		.child = (struct ChildBox[]) { \
			__VA_ARGS__ \
		}, \
	}

#define UI_BITMAP(_index) \
	(struct PictureBitmap) { \
		.base = { \
			.get_width = bitmap_get_width, \
			.get_height = bitmap_get_height, \
			.layout = bitmap_layout, \
			.render = bitmap_render, \
			.pick = bitmap_pick, \
			.process_event = bitmap_process_event, \
		}, \
		.bg_color = 0x01579b, \
		.bc = { \
			.padding = 2, \
			.h_align = ALIGN_MIDDLE, \
			.v_align = ALIGN_MIDDLE, \
			.content = { \
				.bitmap = &(struct Bitmap) { \
					.bc = &icon, \
					.index = _index, \
					.color = 0xffffff, \
				}, \
			}, \
		}, \
	}

#define UI_TEXT(_text) \
	(struct Text) { \
		.base = { \
			.get_width = text_get_width, \
			.get_height = text_get_height, \
			.layout = dummy_layout, \
			.render = render_text, \
		}, \
		.font = font, \
		.text = _text, \
		.color = 0xeff4ff, \
	}

#define UI_BOX(_width, _height, _padding, _color, ...) \
	(struct Box) { \
		.base = { \
			.get_width = color_box_get_width, \
			.get_height = color_box_get_height, \
			.layout = color_box_layout, \
			.render = color_box_render, \
			__VA_ARGS__ \
		}, \
		.width = _width, \
		.height = _height, \
		.bc = { \
			.padding = _padding, \
			.h_align = ALIGN_MIDDLE, \
			.v_align = ALIGN_MIDDLE, \
			.content = { \
				.color = _color, \
			}, \
		}, \
	}

#define UI_WRAPPER(_width, _height, _padding, _child) \
	(struct Box) { \
		.base = { \
			.get_width = box_get_width, \
			.get_height = box_get_height, \
			.layout = box_layout, \
			.render = box_render, \
			.pick = box_pick, \
		}, \
		.width = _width, \
		.height = _height, \
		.bc = { \
			.padding = _padding, \
			.content = { .base = (struct Base *)&_child }, \
		}, \
	}

#define UI_SLIDER(_padding, _horizontal) \
	(struct Slider) { \
		.base = { \
			.layout = slider_layout, \
			.render = slider_render, \
			.pick = slider_pick, \
			.process_event = slider_process_event, \
		}, \
		.horizontal = _horizontal, \
		.value = 0, \
		.line = { \
			.size = 4, \
			.c = UI_CHILDREF(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0)), \
		}, \
		.knob = { \
			.size = 16, \
			.c = UI_CHILDREF(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0xeff4ff, .pick = dummy_pick)), \
		}, \
	}

struct List app = UI_LIST(0, ALIGN_BEGIN, 3,
	UI_CHILD(
		UI_LIST(1, ALIGN_BEGIN, 3,
			UI_CHILD(
				UI_WRAPPER(INHERIT_PARENT, INHERIT_CHILD, 0,
					UI_LIST(1, ALIGN_BEGIN, 4,
						UI_CHILD(UI_BITMAP(0)),
						UI_CHILD(UI_BITMAP(1)),
						UI_CHILD(UI_BITMAP(2)),
						UI_CHILD(UI_BITMAP(3)),
					)
				)
			),
			UI_CHILD(
				UI_WRAPPER(INHERIT_PARENT, INHERIT_CHILD, 0,
					UI_LIST(1, ALIGN_MIDDLE, 3,
						UI_CHILD(UI_BITMAP(8)),
						UI_CHILD(UI_TEXT("__7 / 101")),
						UI_CHILD(UI_BITMAP(4)),
					)
				)
			),
			UI_CHILD(
				UI_WRAPPER(INHERIT_PARENT, INHERIT_CHILD, 0,
					UI_LIST(1, ALIGN_END, 5,
						UI_CHILD(UI_BITMAP(6)),
						UI_CHILD(UI_BITMAP(7)),
						UI_CHILD(UI_BITMAP(9)),
						UI_CHILD(UI_BITMAP(10)),
						UI_CHILD(UI_BITMAP(5)),
					)
				)
			),
		)
	),
	UI_CHILD(
		UI_WRAPPER(INHERIT_PARENT, 140, 0,
			UI_LIST(1, ALIGN_BEGIN, 7,
				UI_CHILD(UI_BOX(100, 100, 0, 0xeff4ff)),
				UI_CHILD(
					UI_WRAPPER(40, INHERIT_PARENT, 0,
						UI_SLIDER(0, 0)
					)
				),
				UI_CHILD(
					UI_WRAPPER(INHERIT_CHILD, INHERIT_PARENT, 0,
						UI_LIST(0, ALIGN_BEGIN, 3,
							UI_CHILD(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0)),
							UI_CHILD(UI_TEXT("RGB 255 255 255")),
							UI_CHILD(UI_TEXT("HSV 180 100 100")),
						)
					)
				),
				UI_CHILD(UI_BOX(100, 100, 0, 0xeff4ff)),
				UI_CHILD(
					UI_WRAPPER(40, INHERIT_PARENT, 0,
						UI_SLIDER(0, 0)
					)
				),
				UI_CHILD(
					UI_WRAPPER(INHERIT_CHILD, INHERIT_PARENT, 0,
						UI_LIST(0, ALIGN_BEGIN, 3,
							UI_CHILD(UI_BITMAP(13)),
							UI_CHILD(UI_BITMAP(12)),
							UI_CHILD(UI_BITMAP(11)),
						)
					)
				),
				UI_CHILD(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0xeff4ff)),
			)
		)
	),
	UI_CHILD(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0xeff4ff)),
);

static void render_app(struct Base *base, struct Rect w)
{
	draw_rect(w, 0x01579b);

	base->layout(base, rect_size(w));
	base->render(base, w, w);
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

	struct Base *new_pick = NULL;
	struct Base *picked = NULL;

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
					render_app((struct Base *)&app, rect_new(0, 0, H_RES - 1, V_RES - 1));
					break;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				new_pick = app.base.pick(&app.base, rect_new(0, 0, H_RES - 1, V_RES - 1), rect_new(0, 0, H_RES - 1, V_RES - 1), pair_new(e.button.x, e.button.y));

				if (picked && picked != new_pick) {
					picked->process_event(picked, (struct Event) { EV_DEACTIVATE });
				}
				picked = new_pick;

				if (picked) {
					picked->process_event(picked, (struct Event) { EV_CURSOR_DOWN, pair_new(e.button.x, e.button.y) });
					render_app((struct Base *)&app, rect_new(0, 0, H_RES - 1, V_RES - 1));
				}

				break;
			case SDL_MOUSEMOTION:
				if (picked) {
					picked->process_event(picked, (struct Event) { EV_CURSOR_MOVE, pair_new(e.motion.x, e.motion.y) });
					render_app((struct Base *)&app, rect_new(0, 0, H_RES - 1, V_RES - 1));
				}
				break;
			case SDL_MOUSEBUTTONUP:
				new_pick = app.base.pick(&app.base, rect_new(0, 0, H_RES - 1, V_RES - 1), rect_new(0, 0, H_RES - 1, V_RES - 1), pair_new(e.button.x, e.button.y));

				if (picked == NULL) {
					break;
				}

				picked->process_event(picked, (struct Event) { EV_CURSOR_UP, pair_new(e.button.x, e.button.y) });

				if (picked == new_pick) {
					picked->process_event(picked, (struct Event) { EV_ACTIVATE });
				} else {
					picked->process_event(picked, (struct Event) { EV_DEACTIVATE });
					picked = NULL;
				}

				render_app((struct Base *)&app, rect_new(0, 0, H_RES - 1, V_RES - 1));
				break;
			}
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
