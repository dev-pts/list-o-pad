#include <stdio.h>
#include <stdint.h>
#include <SDL2/SDL.h>

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
	printf("%i, %i, %i, %i\n", r.x1, r.y1, r.x2, r.y2);
}

static struct Rect rect_intersect(struct Rect s, struct Rect r)
{
	r.x1 = MAX(s.x1, r.x1);
	r.y1 = MAX(s.y1, r.y1);
	r.x2 = MIN(s.x2, r.x2);
	r.y2 = MIN(s.y2, r.y2);

	return r;
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

static void draw_icon(struct Rect r, struct Rect s, uint32_t color, int idx)
{
	draw_bitmap(rect_move(rect_new(0, 0, ICON_W - 1, ICON_H - 1), pair_new(r.x1, r.y1)), s, color, &icon[idx * ICON_W * ICON_H / 8]);
}

enum {
	COLOR_0 = 0xd2d4ca,
	COLOR_1 = 0xe1e3d9,
	COLOR_2 = 0xedefe2,
	COLOR_3 = 0xf7f9ec,
	COLOR_4 = 0xfdfff5,
};

struct Base {
	struct Rect vp;
	int (*get_width)(struct Base *obj);
	int (*get_height)(struct Base *obj);

	void (*layout)(struct Base *obj);
	void (*render)(struct Base *obj, struct Rect w, struct Rect s);
};

static struct {
	struct {
		uint32_t color;
		uint32_t text_color;
	} button;
	struct {
		uint32_t color;
		uint32_t title_color;
	} window;
} style = {
	.button = {
		.color = COLOR_2,
		.text_color = 0,
	},
	.window = {
		.color = COLOR_1,
		.title_color = COLOR_0,
	},
};

#if 0
Passive:
	get_size()

TextFlow:
	\\ Known-sized children are layouted left-to-right top-to-bottom.
	set_size()
	get_len()
	set_overflow(hidden/scroll)
	get_max_child_height()
	children: [Passive]

List:
	\\ Children are layouted to one direction only.
	set_size()
	set_direction(vertical/horizontal)
	set_layout(packed/even)
	set_justify(begin/end/middle)
	get_max_child_size()
	children

Bitmap:
	get_size()
	set_bitmap()

Text:
	get_size()
	set_text()
	get_len()

Box:
	get_size()
	set_child(Bitmap/Text/NULL)
	offset
	margin
	color

\\ This is your viewport.
\\ Do whatever you want.
obj->layout(w, h)

Parent calculates and passes the viewport to the children.

App:
	Table:
		rows: 2
			0:
				height: 20
		cols: 1
		cell:
			[0, 0]: TopPanel
			[0, 1]: Canvas

TopPanel:
	Table:
		rows: 1
		cols: 3
		cell:
			[0, 0]: List:
				direction: horizontal
				layout: packed
				justify: begin
				children:
					Bitmap
					Bitmap
					Bitmap
					Bitmap
			[1, 0]: List:
				direction: horizontal
				layout: packed
				justify: middle
				children:
					Bitmap
					Text
					Bitmap
			[2, 0]: List:
				direction: horizontal
				layout: packed
				justify: end
				children:
					Bitmap
					Bitmap
					Bitmap
					Bitmap
					Bitmap

RightPanel:
	Table:
		rows:
			row:
				height: 20
			row:
		cols:
			col:
		cell:
			[0, 0]:
				List:
					direction: horizontal
					layout: packed
					justify: begin
					children:
						Bitmap
						Bitmap
			[0, 1]:
				TextFlow

List will set it's size to the available viewport.
The list will find the apropriate viewport for all children and pass them to it.

#endif

#define INHERIT_PARENT -1
#define INHERIT_CHILD -2

struct Table {
	struct Base base;

	int rows;
	int cols;

	int *row;
	int *col;
	struct Base **child;
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

	return -1;
}

static void table_layout(struct Base *base)
{
	struct Table *obj = (struct Table *)base;
	struct Rect vp = obj->base.vp;
	int even_width = table_calc_even(obj->col, obj->cols, rect_width(vp));
	int even_height = table_calc_even(obj->row, obj->rows, rect_height(vp));
	int y = vp.y1;

	for (int i = 0; i < obj->rows; i++) {
		int dim_r = obj->row[i];
		int x = vp.x1;
		int h;

		if (dim_r < 0) {
			h = even_height;
		} else {
			h = dim_r;
		}

		for (int j = 0; j < obj->cols; j++) {
			struct Base *c = obj->child[i * obj->cols + j];
			int dim_c = obj->col[j];
			int w;

			if (dim_c < 0) {
				w = even_width;
			} else {
				w = dim_c;
			}

			c->vp = rect_new(x, y, x + w - 1, y + h - 1);

			x += w;
		}

		y += h;
	}

	for (int i = 0; i < obj->rows; i++) {
		for (int j = 0; j < obj->cols; j++) {
			struct Base *c = obj->child[i * obj->cols + j];

			c->layout(c);
		}
	}
}

static void table_render(struct Base *base, struct Rect w, struct Rect s)
{
	struct Table *obj = (struct Table *)base;

	for (int i = 0; i < obj->rows; i++) {
		for (int j = 0; j < obj->cols; j++) {
			struct Base *c = obj->child[i * obj->cols + j];

			c->render(c, rect_intersect(w, c->vp), rect_intersect(s, c->vp));
		}
	}
}

struct List {
	struct Base base;

	int horizontal;
	int justify;

	int children;
	struct Base **child;
};

static void list_layout(struct Base *base)
{
	struct List *obj = (struct List *)base;
	struct Rect vp = obj->base.vp;
	int x = vp.x1;
	int y = vp.y1;

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];
		int x2, y2;
		int x1 = x;
		int y1 = y;

		if (obj->horizontal) {
			int width = c->get_width(c);

			x2 = x1 + width;
			y2 = vp.y2;

			x += width;
		} else {
			int height = c->get_height(c);

			x2 = vp.x2;
			y2 = y1 + height;

			y += height;
		}

		c->vp = rect_new(x1, y1, x2, y2);
	}

	if (obj->justify > 0) {
		if (obj->horizontal) {
			int offset = rect_width(vp) - (x - vp.x1);

			if (offset > 0) {
				if (obj->justify == 1) {
					offset /= 2;
				}

				for (int i = 0; i < obj->children; i++) {
					struct Base *c = obj->child[i];

					c->vp = rect_move(c->vp, pair_new(offset, 0));
				}
			}
		} else {
			int offset = rect_height(vp) - (y - vp.y1);

			if (offset > 0) {
				if (obj->justify == 1) {
					offset /= 2;
				}

				for (int i = 0; i < obj->children; i++) {
					struct Base *c = obj->child[i];

					c->vp = rect_move(c->vp, pair_new(0, offset));
				}
			}
		}
	}

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];

		c->layout(c);
	}
}

static void list_render(struct Base *base, struct Rect w, struct Rect s)
{
	struct List *obj = (struct List *)base;

	for (int i = 0; i < obj->children; i++) {
		struct Base *c = obj->child[i];

		c->render(c, rect_intersect(w, c->vp), rect_intersect(s, c->vp));
	}
}

struct Bitmap {
	struct Base base;

	uint8_t *bitmap;
	int width;
	int height;
	uint32_t color;
};

static int bitmap_get_width(struct Base *base)
{
	struct Bitmap *obj = (struct Bitmap *)base;
	return obj->width;
}

static int bitmap_get_height(struct Base *base)
{
	struct Bitmap *obj = (struct Bitmap *)base;
	return obj->height;
}

static void dummy_layout(struct Base *base)
{
}

static void bitmap_render(struct Base *base, struct Rect w, struct Rect s)
{
	struct Bitmap *obj = (struct Bitmap *)base;

	draw_bitmap(rect_move(rect_new(0, 0, obj->width - 1, obj->height - 1), pair_new(w.x1, w.y1)), s, obj->color, obj->bitmap);
}

struct Box {
	struct Base base;

	int padding;
	uint32_t color;

	struct {
		int width;
		int height;
	};

	struct {
		struct Rect child_vp;
		struct Base *child;
	};
};

static int box_get_width(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	if (obj->width == INHERIT_PARENT) {
		return INHERIT_PARENT;
	}

	if (obj->width == INHERIT_CHILD) {
		int width = obj->child->get_width(obj->child);

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
		int height = obj->child->get_height(obj->child);

		if (height == INHERIT_PARENT) {
			return INHERIT_PARENT;
		}

		return height + obj->padding * 2;
	}

	return obj->height;
}

static void box_layout(struct Base *base)
{
	struct Box *obj = (struct Box *)base;

	base->vp = rect_new(base->vp.x1 + obj->padding, base->vp.y1 + obj->padding, base->vp.x2 - obj->padding, base->vp.y2 - obj->padding);

	if (obj->child) {
		obj->child->vp = base->vp;
		obj->child->layout(obj->child);
	}
}

static void box_render(struct Base *base, struct Rect w, struct Rect s)
{
	struct Box *obj = (struct Box *)base;

	draw_rect(rect_intersect(base->vp, rect_intersect(w, s)), obj->color);

	if (obj->child) {
		obj->child->render(obj->child, w, s);
	}
}

struct Text {
	struct Base base;

	const char *text;
	uint32_t color;
};

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

static struct Table app = {
	.base = {
		.layout = table_layout,
		.render = table_render,
	},
	.rows = 2,
	.cols = 1,
	.row = (int[]) {
		100,
		INHERIT_PARENT,
	},
	.col = (int[]) {
		INHERIT_PARENT,
	},
	.child = (struct Base*[]) {
		(struct Base *)&(struct Table) {
			.base = {
				.layout = table_layout,
				.render = table_render,
			},
			.rows = 1,
			.cols = 3,
			.row = (int[]) {
				INHERIT_PARENT,
			},
			.col = (int[]) {
				INHERIT_PARENT, INHERIT_PARENT, INHERIT_PARENT,
			},
			.child = (struct Base*[]) {
				(struct Base *)&(struct List) {
					.base = {
						.layout = list_layout,
						.render = list_render,
					},
					.horizontal = 1,
					.children = 4,
					.child = (struct Base*[]) {
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[0 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[1 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[2 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[3 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
					},
				},
				(struct Base *)&(struct List) {
					.base = {
						.layout = list_layout,
						.render = list_render,
					},
					.horizontal = 1,
					.justify = 1,
					.children = 3,
					.child = (struct Base*[]) {
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[8 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
						(struct Base *)&(struct Text) {
							.base = {
								.get_width = text_get_width,
								.get_height = text_get_height,
								.layout = dummy_layout,
								.render = render_text,
							},
							.text = "7 / 101",
							.color = 0xeff4ff,
						},
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[4 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
					},
				},
				(struct Base *)&(struct List) {
					.base = {
						.layout = list_layout,
						.render = list_render,
					},
					.horizontal = 1,
					.justify = 2,
					.children = 5,
					.child = (struct Base*[]) {
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[6 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[7 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[9 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[10 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
						(struct Base *)&(struct Box) {
							.base = {
								.get_width = box_get_width,
								.get_height = box_get_height,
								.layout = box_layout,
								.render = box_render,
							},
							.width = INHERIT_CHILD,
							.height = INHERIT_CHILD,
							.padding = 2,
							.color = 0x2d89ef,
							.child = (struct Base *)&(struct Bitmap) {
								.base = {
									.get_width = bitmap_get_width,
									.get_height = bitmap_get_height,
									.layout = dummy_layout,
									.render = bitmap_render,
								},
								.bitmap = &icon[5 * ICON_W * ICON_H / 8],
								.width = ICON_W,
								.height = ICON_H,
								.color = 0xeff4ff,
							},
						},
					},
				},
			},
		},
		(struct Base *)&(struct Box) {
			.base = {
				.get_width = box_get_width,
				.get_height = box_get_height,
				.layout = box_layout,
				.render = box_render,
			},
			.width = INHERIT_PARENT,
			.height = INHERIT_PARENT,
			.color = 0xeff4ff,
		},
	},
};

static void render_app(struct Base *base, struct Rect w)
{
	draw_rect(w,  0x2b5797);

	base->vp = w;

	base->layout(base);
	base->render(base, w, w);
}

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

	sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!sdl_renderer) {
		printf("Renderer creation failed: %s\n", SDL_GetError());
		return -1;
	}

	sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, H_RES, V_RES);
	if (!sdl_texture) {
		printf("Texture creation failed: %s\n", SDL_GetError());
		return -1;
	}

	render_app((struct Base *)&app, rect_new(0, 0, H_RES - 1, V_RES - 1));

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
					//ui_redraw();
					break;
				}
				break;
			case SDL_MOUSEMOTION:
				//controller_mouse_moved(e.motion.x, e.motion.y);
				break;
			case SDL_MOUSEBUTTONDOWN:
#if 0
				if (controller_mouse_pressed())
					SDL_CaptureMouse(SDL_TRUE);
#endif
				break;
			case SDL_MOUSEBUTTONUP:
#if 0
				if (controller_mouse_released())
					SDL_CaptureMouse(SDL_FALSE);
#endif
				break;
			}
		}

		SDL_UpdateTexture(sdl_texture, NULL, fb, H_RES * 4);
		SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
		SDL_RenderPresent(sdl_renderer);
	}

	SDL_DestroyTexture(sdl_texture);
	SDL_DestroyRenderer(sdl_renderer);
	SDL_DestroyWindow(sdl_window);
	SDL_Quit();

	return 0;
}
