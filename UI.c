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

struct BitmapFont {
	struct Pair size;
	struct BitmapData *bitmap;
};

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
	struct BitmapFont *bitmap;
	int index;
	uint32_t color;
};

struct Text {
	uint8_t *font;
	const char *text;
	uint32_t color;
};

struct ChildBox {
	/* This is a place where a parent puts the calculated area for this element */
	struct Rect vp;

	union {
		struct Base *base;
		struct Bitmap bitmap;
		struct Text text;
	};
};

enum Align {
	ALIGN_BEGIN,
	ALIGN_MIDDLE,
	ALIGN_END
};

struct BoxContent {
	enum Align h_align;
	enum Align v_align;

	union {
		struct ChildBox child;
		struct {
			int children;
			struct ChildBox *childs;
		};
	};
};

#define INHERIT_PARENT -1
#define INHERIT_CHILD -2

struct Box {
	struct Base base;

	int width;
	int height;

	/* Subtract this from resulted width and height */
	int padding;
	/* Draw rect with this color in the resulted area *after* padding */
	uint32_t color;

	struct BoxContent *content;
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

static void draw_bitmap2(struct Pair pvp, struct Rect svp, struct BitmapFont *font, int index, uint32_t color)
{
	struct BitmapData *bd = &font->bitmap[index];
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

static int bitmap_get_width(struct Base *base)
{
	struct Box *box = (struct Box *)base;
	struct Bitmap *obj = &box->content->child.bitmap;

	return obj->bitmap->size.w + box->padding * 2;
}

static int bitmap_get_height(struct Base *base)
{
	struct Box *box = (struct Box *)base;
	struct Bitmap *obj = &box->content->child.bitmap;

	return obj->bitmap->size.h + box->padding * 2;
}

static void bitmap_layout(struct Base *base, struct Pair size)
{
	struct Box *obj = (struct Box *)base;
	struct Bitmap *b = &obj->content->child.bitmap;
	struct Rect vp = rect_new(obj->padding, obj->padding, size.w - 1 - obj->padding, size.h - 1 - obj->padding);
	struct Rect cp = rect_new(0, 0, b->bitmap->bitmap[b->index].bb.w - 1, b->bitmap->bitmap[b->index].bb.h - 1);

	cp = rect_move(cp, pair_new(obj->padding, obj->padding));

	if (obj->content->h_align) {
		int offset = rect_width(vp) - rect_width(cp);

		if (obj->content->h_align == ALIGN_MIDDLE) {
			offset /= 2;
		}

		cp = rect_move(cp, pair_new(offset, 0));
	}

	if (obj->content->v_align) {
		int offset = rect_height(vp) - rect_height(cp);

		if (obj->content->v_align == ALIGN_MIDDLE) {
			offset /= 2;
		}

		cp = rect_move(cp, pair_new(0, offset));
	}

	obj->content->child.vp = cp;
}

static void bitmap_render(struct Base *base, struct Rect pvp, struct Rect svp)
{
	struct Box *box = (struct Box *)base;
	struct Bitmap *obj = &box->content->child.bitmap;
	struct Rect vp = rect_new(pvp.x1 + box->padding, pvp.y1 + box->padding, pvp.x2 - box->padding, pvp.y2 - box->padding);
	struct Rect cp = rect_move(box->content->child.vp, pair_new(pvp.x1, pvp.y1));

	draw_rect(svp, box->color);

	svp = rect_intersect(vp, svp);

	draw_bitmap2(pair_new(cp.x1, cp.y1), svp, obj->bitmap, obj->index, obj->color);
}

static struct Base *bitmap_pick(struct Base *base, struct Rect pvp, struct Rect svp, struct Pair p)
{
	struct Box *box = (struct Box *)base;
	struct Rect vp = rect_new(pvp.x1 + box->padding, pvp.y1 + box->padding, pvp.x2 - box->padding, pvp.y2 - box->padding);

	svp = rect_intersect(vp, svp);

	if (rect_hit(svp, p)) {
		return base;
	}

	return NULL;
}

static void bitmap_process_event(struct Base *base, struct Event ev)
{
	struct Box *box = (struct Box *)base;

	switch (ev.type) {
	case EV_CURSOR_DOWN:
	case EV_ACTIVATE:
		box->color = 0x1298e1;
		break;
	case EV_DEACTIVATE:
		box->color = 0x01579b;
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
		int width = obj->content->child.base->get_width(obj->content->child.base);

		if (width == INHERIT_PARENT) {
			return INHERIT_PARENT;
		}

		return width + obj->padding * 2;
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
		int height = obj->content->child.base->get_height(obj->content->child.base);

		if (height == INHERIT_PARENT) {
			return INHERIT_PARENT;
		}

		return height + obj->padding * 2;
	}

	return obj->height;
}

static void box_layout(struct Base *base, struct Pair size)
{
	struct Box *obj = (struct Box *)base;

	if (obj->content == NULL) {
		return;
	}

	obj->content->child.vp = rect_new(obj->padding, obj->padding, size.w - 1 - obj->padding, size.h - 1 - obj->padding);
	obj->content->child.base->layout(obj->content->child.base, rect_size(obj->content->child.vp));
}

static void box_render(struct Base *base, struct Rect pvp, struct Rect svp)
{
	struct Box *obj = (struct Box *)base;

	draw_rect(svp, obj->color);

	if (obj->content) {
		child_box_render(&obj->content->child, pvp, svp);
	}
}

static int text_get_width(struct Base *base)
{
	struct Text *obj = &((struct Box *)base)->content->child.text;

	return FONT_W * strlen(obj->text);
}

static int text_get_height(struct Base *base)
{
	return FONT_H;
}

static void render_text(struct Base *base, struct Rect w, struct Rect s)
{
	struct Text *obj = &((struct Box *)base)->content->child.text;

	draw_text(w, s, obj->color, obj->text);
}

struct Slider {
	struct Box box;

	int horizontal;
	int value;
	int frac;

	struct Pair p;
	int pressed;
	int old_value;
};

static void slider_layout(struct Base *base, struct Pair size)
{
	struct Slider *obj = (struct Slider *)base;

	{
		struct ChildBox *knob = &obj->box.content->childs[1];
		int kw = knob->base->get_width(knob->base);
		int kh = knob->base->get_height(knob->base);
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
	}

	{
		struct ChildBox *line = &obj->box.content->childs[0];
		int lw = line->base->get_width(line->base);
		int lh = line->base->get_height(line->base);
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
	}
}

static void slider_render(struct Base *base, struct Rect pvp, struct Rect svp)
{
	struct Slider *obj = (struct Slider *)base;
	struct ChildBox *line = &obj->box.content->childs[0];
	struct ChildBox *knob = &obj->box.content->childs[1];
	struct Box *box = &obj->box;

	draw_rect(svp, box->color);

	child_box_render(line, pvp, svp);
	child_box_render(knob, pvp, svp);
}

static struct Base *slider_pick(struct Base *base, struct Rect pvp, struct Rect svp, struct Pair p)
{
	struct Slider *obj = (struct Slider *)base;
	struct ChildBox *knob = &obj->box.content->childs[1];

	if (child_box_pick(knob, pvp, svp, p)) {
		return base;
	}

	return NULL;
}

static void slider_process_event(struct Base *base, struct Event ev)
{
	struct Slider *obj = (struct Slider *)base;
	struct ChildBox *knob = &obj->box.content->childs[1];
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
			obj->value = obj->old_value + (knob->vp.x1 + diff) * 100 / obj->frac;

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

struct Table {
	struct Box box;

	int space;

	int rows;
	int cols;

	int *row;
	int *col;
};

static int table_calc_even(int *dim, int cnt, int vp)
{
	int wo_dim = 0;
	int total_dim = vp;

	for (int i = 0; i < cnt; i++) {
		if (dim[i] < 0) {
			wo_dim++;
		} else {
			total_dim -= dim[i];
		}
	}

	if (wo_dim) {
		return total_dim / wo_dim;
	}

	return 0;
}

static void table_layout(struct Base *base, struct Pair size)
{
	struct Table *obj = (struct Table *)base;
	int even_width = table_calc_even(obj->col, obj->cols, size.w - (obj->cols - 1) * obj->space);
	int even_height = table_calc_even(obj->row, obj->rows, size.h - (obj->rows - 1) * obj->space);
	int y = 0;

	for (int i = 0; i < obj->rows; i++) {
		int dim_r = obj->row[i];
		int x = 0;
		int h;

		if (dim_r < 0) {
			h = even_height;
		} else {
			h = dim_r;
		}

		for (int j = 0; j < obj->cols; j++) {
			struct ChildBox *c = &obj->box.content->childs[i * obj->cols + j];
			int dim_c = obj->col[j];
			int w;

			if (dim_c < 0) {
				w = even_width;
			} else {
				w = dim_c;
			}

			c->vp = rect_new(x, y, x + w - 1, y + h - 1);

			x += w;
			x += obj->space;
		}

		x -= obj->space;

		if (x >= size.w) {
			int offset = size.w - x + 1;

			for (int j = 0; j < obj->cols; j++) {
				struct ChildBox *c = &obj->box.content->childs[i * obj->cols + j];
				int dim_c = obj->col[j];

				if (dim_c < 0) {
					c->vp = rect_new(c->vp.x1, c->vp.y1, c->vp.x2 + offset, c->vp.y2);
					break;
				}
			}
		}

		y += h;
		y += obj->space;
	}

	y -= obj->space;

	if (y >= size.h) {
		int offset = size.h - y + 1;

		for (int i = 0; i < obj->rows; i++) {
			int dim_r = obj->row[i];

			if (dim_r < 0) {
				for (int j = 0; j < obj->cols; j++) {
					struct ChildBox *c = &obj->box.content->childs[i * obj->cols + j];

					c->vp = rect_new(c->vp.x1, c->vp.y1, c->vp.x2, c->vp.y2 + offset);
				}
			}
		}
	}

	for (int i = 0; i < obj->rows; i++) {
		for (int j = 0; j < obj->cols; j++) {
			struct ChildBox *c = &obj->box.content->childs[i * obj->cols + j];

			c->base->layout(c->base, rect_size(c->vp));
		}
	}
}

static void table_render(struct Base *base, struct Rect pvp, struct Rect svp)
{
	struct Table *obj = (struct Table *)base;

	for (int i = 0; i < obj->rows; i++) {
		for (int j = 0; j < obj->cols; j++) {
			struct ChildBox *c = &obj->box.content->childs[i * obj->cols + j];

			child_box_render(c, pvp, svp);
		}
	}
}

static struct Base *table_pick(struct Base *base, struct Rect pvp, struct Rect svp, struct Pair p)
{
	struct Table *obj = (struct Table *)base;

	for (int i = 0; i < obj->rows; i++) {
		for (int j = 0; j < obj->cols; j++) {
			struct ChildBox *c = &obj->box.content->childs[i * obj->cols + j];
			struct Base *ret = child_box_pick(c, pvp, svp, p);

			if (ret) {
				return ret;
			}
		}
	}

	return NULL;
}

struct List {
	struct Box box;

	int horizontal;
	int align;
	int space;
};

static void list_layout(struct Base *base, struct Pair size)
{
	struct List *obj = (struct List *)base;
	int x = 0;
	int y = 0;

	if (obj->horizontal) {
		for (int i = 0; i < obj->box.content->children; i++) {
			struct ChildBox *c = &obj->box.content->childs[i];
			int width = c->base->get_width(c->base);

			c->vp = rect_new(x, y, x + width - 1, size.h - 1);

			x += width;
			x += obj->space;
		}

		x -= obj->space;
	} else {
		for (int i = 0; i < obj->box.content->children; i++) {
			struct ChildBox *c = &obj->box.content->childs[i];
			int height = c->base->get_height(c->base);

			c->vp = rect_new(x, y, size.w - 1, y + height - 1);

			y += height;
			y += obj->space;
		}

		y -= obj->space;
	}

	if (obj->align) {
		if (obj->horizontal) {
			int offset = size.w - x;

			if (offset > 0) {
				if (obj->align == ALIGN_MIDDLE) {
					offset /= 2;
				}

				for (int i = 0; i < obj->box.content->children; i++) {
					struct ChildBox *c = &obj->box.content->childs[i];

					c->vp = rect_move(c->vp, pair_new(offset, 0));
				}
			}
		} else {
			int offset = size.h - y;

			if (offset > 0) {
				if (obj->align == ALIGN_MIDDLE) {
					offset /= 2;
				}

				for (int i = 0; i < obj->box.content->children; i++) {
					struct ChildBox *c = &obj->box.content->childs[i];

					c->vp = rect_move(c->vp, pair_new(0, offset));
				}
			}
		}
	}

	for (int i = 0; i < obj->box.content->children; i++) {
		struct ChildBox *c = &obj->box.content->childs[i];

		c->base->layout(c->base, rect_size(c->vp));
	}
}

static void list_render(struct Base *base, struct Rect pvp, struct Rect svp)
{
	struct List *obj = (struct List *)base;

	for (int i = 0; i < obj->box.content->children; i++) {
		struct ChildBox *c = &obj->box.content->childs[i];

		child_box_render(c, pvp, svp);
	}
}

static struct Base *list_pick(struct Base *base, struct Rect pvp, struct Rect svp, struct Pair p)
{
	struct Table *obj = (struct Table *)base;

	for (int i = 0; i < obj->box.content->children; i++) {
		struct ChildBox *c = &obj->box.content->childs[i];
		struct Base *ret = child_box_pick(c, pvp, svp, p);

		if (ret) {
			return ret;
		}
	}

	return NULL;
}

#define GROUP(...) { __VA_ARGS__ }

#define UI_CHILDS(child) { .base = (struct Base *)&child }

#define UI_TABLE(_rows, _cols, _row, _col, ...) \
	(struct Table) { \
		.rows = _rows, \
		.cols = _cols, \
		.row = (int[]) _row, \
		.col = (int[]) _col, \
		.space = 2, \
		.box = { \
			.base = { \
				.layout = table_layout, \
				.render = table_render, \
				.pick = table_pick, \
			}, \
			.width = INHERIT_PARENT, \
			.height = INHERIT_PARENT, \
			.content = &(struct BoxContent) { \
				.childs = (struct ChildBox[]) { \
					__VA_ARGS__ \
				}, \
			}, \
		}, \
	}

#define UI_LIST(_horizontal, _align, _children, ...) \
	(struct List) { \
		.horizontal = _horizontal, \
		.align = _align, \
		.space = 2, \
		.box = { \
			.base = { \
				.layout = list_layout, \
				.render = list_render, \
				.pick = list_pick, \
			}, \
			.width = INHERIT_PARENT, \
			.height = INHERIT_PARENT, \
			.content = &(struct BoxContent) { \
				.children = _children, \
				.childs = (struct ChildBox[]) { \
					__VA_ARGS__ \
				}, \
			}, \
		}, \
	}

#define UI_BITMAP(_index) \
	(struct Box) { \
		.base = { \
			.get_width = bitmap_get_width, \
			.get_height = bitmap_get_height, \
			.layout = bitmap_layout, \
			.render = bitmap_render, \
			.pick = bitmap_pick, \
			.process_event = bitmap_process_event, \
		}, \
		.width = INHERIT_CHILD, \
		.height = INHERIT_CHILD, \
		.padding = 2, \
		.color = 0x01579b, \
		.content = &(struct BoxContent) { \
			.h_align = ALIGN_MIDDLE, \
			.v_align = ALIGN_MIDDLE, \
			.child = { \
				.bitmap = { \
					.bitmap = &icon, \
					.index = _index, \
					.color = 0xffffff, \
				}, \
			}, \
		}, \
	}

#define UI_TEXT(_text) \
	(struct Box) { \
		.base = { \
			.get_width = text_get_width, \
			.get_height = text_get_height, \
			.layout = dummy_layout, \
			.render = render_text, \
		}, \
		.width = INHERIT_CHILD, \
		.height = INHERIT_CHILD, \
		.content = &(struct BoxContent) { \
			.child = { \
				.text = { \
					.font = font, \
					.text = _text, \
					.color = 0xeff4ff, \
				}, \
			}, \
		}, \
	}

#define UI_BOX(_width, _height, _padding, _color, ...) \
	(struct Box) { \
		.base = { \
			.get_width = box_get_width, \
			.get_height = box_get_height, \
			.layout = box_layout, \
			.render = box_render, \
			__VA_ARGS__ \
		}, \
		.width = _width, \
		.height = _height, \
		.padding = _padding, \
		.color = _color, \
	}

#define UI_SLIDER(_width, _height, _padding, _horizontal) \
	(struct Slider) { \
		.horizontal = _horizontal, \
		.value = 0, \
		.box = { \
			.base = { \
				.get_width = box_get_width, \
				.get_height = box_get_height, \
				.layout = slider_layout, \
				.render = slider_render, \
				.pick = slider_pick, \
				.process_event = slider_process_event, \
			}, \
			.width = _width, \
			.height = _height, \
			.padding = _padding, \
			.color = 0x1298e1, \
			.content = &(struct BoxContent) { \
				.children = 2, \
				.childs = (struct ChildBox[]) { \
					UI_CHILDS(UI_BOX(10, 10, 0, 0)), \
					UI_CHILDS(UI_BOX(16, 16, 0, 0xeff4ff, .pick = dummy_pick)), \
				}, \
			}, \
		}, \
	}

struct Table app = UI_TABLE(3, 1, GROUP(40, 100 + 40, -1), GROUP(-1),
	UI_CHILDS(
		UI_TABLE(1, 3, GROUP(-1), GROUP(-1, -1, -1),
			UI_CHILDS(
				UI_LIST(1, 0, 4,
					UI_CHILDS(UI_BITMAP(0)),
					UI_CHILDS(UI_BITMAP(1)),
					UI_CHILDS(UI_BITMAP(2)),
					UI_CHILDS(UI_BITMAP(3)),
				)
			),
			UI_CHILDS(
				UI_LIST(1, 1, 3,
					UI_CHILDS(UI_BITMAP(8)),
					UI_CHILDS(UI_TEXT("__7 / 101")),
					UI_CHILDS(UI_BITMAP(4)),
				)
			),
			UI_CHILDS(
				UI_LIST(1, 2, 5,
					UI_CHILDS(UI_BITMAP(6)),
					UI_CHILDS(UI_BITMAP(7)),
					UI_CHILDS(UI_BITMAP(9)),
					UI_CHILDS(UI_BITMAP(10)),
					UI_CHILDS(UI_BITMAP(5)),
				)
			),
		)
	),
	UI_CHILDS(
		UI_TABLE(1, 7, GROUP(-1), GROUP(80, 100, 40, 100, 40, 40, -1),
			UI_CHILDS(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0xeff4ff)),
			UI_CHILDS(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0xeff4ff)),
			UI_CHILDS(UI_SLIDER(INHERIT_PARENT, INHERIT_PARENT, 0, 0)),
			UI_CHILDS(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0xeff4ff)),
			UI_CHILDS(UI_SLIDER(INHERIT_PARENT, INHERIT_PARENT, 0, 0)),
			UI_CHILDS(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0xeff4ff)),
			UI_CHILDS(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0xeff4ff)),
		)
	),
	UI_CHILDS(UI_BOX(INHERIT_PARENT, INHERIT_PARENT, 0, 0xeff4ff)),
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
				new_pick = app.box.base.pick(&app.box.base, rect_new(0, 0, H_RES - 1, V_RES - 1), rect_new(0, 0, H_RES - 1, V_RES - 1), pair_new(e.button.x, e.button.y));

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
				new_pick = app.box.base.pick(&app.box.base, rect_new(0, 0, H_RES - 1, V_RES - 1), rect_new(0, 0, H_RES - 1, V_RES - 1), pair_new(e.button.x, e.button.y));

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
