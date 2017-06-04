/*
 * Copyright 2017 Michael Forney <mforney@mforney.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <pixman.h>
#include <math.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_CACHE_H
#include FT_SIZES_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include "utils/errors.h"
#include "utils/filepath.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "netsurf/bitmap.h"
#include "netsurf/browser_window.h"
#include "netsurf/content.h"
#include "netsurf/layout.h"
#include "netsurf/plotters.h"

#include "tiny/render.h"

#define LENGTH(a) (sizeof(a) / sizeof((a)[0]))
#define BOLD_WEIGHT 700
#define PIXMAN_COLOR(c) {\
	.red = ((c) & 0xff) * 0x101,\
	.green = (((c) >> 8) & 0xff) * 0x101,\
	.blue = (((c) >> 16) & 0xff) * 0x101,\
	.alpha = 0xffff,\
}

enum face {
	FACE_SANS_SERIF,
	FACE_SANS_SERIF_BOLD,
	FACE_SANS_SERIF_ITALIC,
	FACE_SANS_SERIF_ITALIC_BOLD,
	FACE_SERIF,
	FACE_SERIF_BOLD,
	FACE_MONOSPACE,
	FACE_MONOSPACE_BOLD,
	FACE_CURSIVE,
	FACE_FANTASY,
	FACE_COUNT,
};

struct faceid {
	char *file;
	int charmap;
};

struct glyph {
	pixman_image_t *image;
	FT_Vector advance;
};

struct sizedata {
	struct faceid *faceid;
	struct glyph *glyphs;
};

extern char **respaths;

static FT_Library library;
static FTC_Manager manager;
static FTC_CMapCache cmapcache;
static struct faceid *faces[FACE_COUNT];
static pixman_glyph_cache_t *glyphcache;
static pixman_image_t *target;
static struct rect curclip;

/* font handling */
static nserror
fterror(FT_Error err)
{
	switch (err) {
	case FT_Err_Ok:
		return NSERROR_OK;
	case FT_Err_Out_Of_Memory:
		return NSERROR_NOMEM;
	case FT_Err_Cannot_Open_Resource:
		return NSERROR_NOT_FOUND;
	case FT_Err_Invalid_Argument:
	case FT_Err_Invalid_File_Format:
		return NSERROR_INVALID;
	case FT_Err_Unimplemented_Feature:
		return NSERROR_NOT_IMPLEMENTED;
	default:
		return NSERROR_UNKNOWN;
	}
}

static FT_Error
requester(FTC_FaceID id, FT_Library library, FT_Pointer data, FT_Face *ftface)
{
	struct faceid *face = (void *)id;
	FT_Error err;
	int i;

	err = FT_New_Face(library, face->file, 0, ftface);
	if (err)
		return err;
	err = FT_Select_Charmap(*ftface, FT_ENCODING_UNICODE);
	if (err)
		return err;
	for (i = 0; i < (*ftface)->num_charmaps; ++i) {
		if ((*ftface)->charmap == (*ftface)->charmaps[i]) {
			face->charmap = i;
			break;
		}
	}

	return FT_Err_Ok;
}

static struct faceid *
newface(const char *file, const char *font)
{
	struct faceid *face;
	char buf[PATH_MAX];
	FT_Face ftface;
	FT_Error err;

	face = malloc(sizeof(*face));
	if (!face)
		goto err0;
	if (!file) {
		filepath_sfind(respaths, buf, font);
		file = buf;
	}
	face->file = strdup(file);
	if (!face->file)
		goto err1;

	err = FTC_Manager_LookupFace(manager, face, &ftface);
	if (err)
		goto err2;

	return face;

err2:
	free(face->file);
err1:
	free(face);
err0:
	return NULL;
}

static FTC_FaceID
lookupface(const struct plot_font_style *style)
{
	bool bold = style->weight >= 700;

	switch (style->family) {
	case PLOT_FONT_FAMILY_SANS_SERIF:
	default:
		if (style->flags & FONTF_ITALIC || style->flags & FONTF_OBLIQUE)
			return faces[bold ? FACE_SANS_SERIF_ITALIC_BOLD : FACE_SANS_SERIF_ITALIC];
		else
			return faces[bold ? FACE_SANS_SERIF_BOLD : FACE_SANS_SERIF];
	case PLOT_FONT_FAMILY_SERIF:
		return faces[bold ? FACE_SERIF_BOLD : FACE_SERIF];
	case PLOT_FONT_FAMILY_MONOSPACE:
		return faces[bold ? FACE_MONOSPACE_BOLD : FACE_MONOSPACE];
	case PLOT_FONT_FAMILY_CURSIVE:
		return faces[FACE_CURSIVE];
	case PLOT_FONT_FAMILY_FANTASY:
		return faces[FACE_FANTASY];
	}
}

static void
destroysizedata(void *object)
{
	FT_Size size = object;
	struct sizedata *data = size->generic.data;
	struct glyph *glyph;
	size_t n;

	pixman_glyph_cache_freeze(glyphcache);
	for (n = size->face->num_glyphs, glyph = data->glyphs; n; --n, ++glyph) {
		if (glyph->image)
			pixman_glyph_cache_remove(glyphcache, size, glyph);
	}
	pixman_glyph_cache_thaw(glyphcache);
	free(data->glyphs);
	free(data);
}

static FT_Size
lookupsize(const struct plot_font_style *style)
{
	FTC_ScalerRec scaler;
	FT_Size size;
	FT_Error err;
	struct sizedata *data;

	scaler.face_id = lookupface(style);
	scaler.width = scaler.height = style->size * 64 / 1024;
	scaler.pixel = 0;
	//scaler.width = scaler.height = 20;
	//scaler.pixel = 1;
	scaler.x_res = scaler.y_res = browser_get_dpi();

	err = FTC_Manager_LookupSize(manager, &scaler, &size);
	if (err)
		return NULL;

	if (!size->generic.data) {
		data = malloc(sizeof(*data));
		if (!data)
			return NULL;
		data->glyphs = calloc(size->face->num_glyphs, sizeof(data->glyphs[0]));
		data->faceid = scaler.face_id;
		size->generic.data = data;
		size->generic.finalizer = destroysizedata;
	}

	err = FT_Activate_Size(size);
	if (err)
		return NULL;

	return size;
}

static inline uint8_t flipbits(uint8_t b)
{
	b = ((b << 1) & 0xaa) | ((b >> 1) & 0x55);
	b = ((b << 2) & 0xcc) | ((b >> 2) & 0x33);
	b = ((b << 4) & 0xf0) | ((b >> 4) & 0x0f);
	return b;
}

static nserror
outlineimage(FT_Outline *outline, int *left, int *top, pixman_image_t **image)
{
	FT_Bitmap bitmap;
	FT_BBox box;
	nserror err;
	int w, h;

	FT_Outline_Get_CBox(outline, &box);
	box.xMin = box.xMin & ~0x3f;
	box.yMin = box.yMin & ~0x3f;
	box.xMax = (box.xMax + 0x3f) & ~0x3f;
	box.yMax = (box.yMax + 0x3f) & ~0x3f;
	FT_Outline_Translate(outline, -box.xMin, -box.yMin);
	w = (box.xMax - box.xMin) >> 6;
	h = (box.yMax - box.yMin) >> 6;
	*left = -box.xMin >> 6;
	*top = (box.yMin >> 6) + h;
	*image = pixman_image_create_bits(PIXMAN_a8, w, h, NULL, 0);
	bitmap.width = w;
	bitmap.rows = h;
	bitmap.pitch = pixman_image_get_stride(*image);
	bitmap.buffer = (void *)pixman_image_get_data(*image);
	bitmap.num_grays = 0x100;
	bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
	err = fterror(FT_Outline_Get_Bitmap(library, outline, &bitmap));
	if (err) {
		pixman_image_unref(*image);
		return err;
	}

	return NSERROR_OK;
}

static nserror
bitmapimage(FT_Bitmap *bitmap, pixman_image_t **image)
{
	FT_Bitmap tmp;
	nserror err;
	uint8_t *src, *dst;
	int i, j, pitch;

	switch (bitmap->pixel_mode) {
	case FT_PIXEL_MODE_MONO:
		*image = pixman_image_create_bits_no_clear(PIXMAN_a1, bitmap->width, bitmap->rows, NULL, 0);
		if (!*image)
			return NSERROR_NOMEM;
		src = bitmap->buffer;
		dst = (void *)pixman_image_get_data(*image);
		pitch = pixman_image_get_stride(*image);
		for (i = 0; i < bitmap->rows; ++i) {
			for (j = 0; j < bitmap->pitch; ++j)
				dst[j] = flipbits(src[j]);
			src += bitmap->pitch;
			dst += pitch;
		}
		break;
	case FT_PIXEL_MODE_GRAY:
		if (bitmap->num_grays != 0x100)
			return NSERROR_NOT_IMPLEMENTED;
		FT_Bitmap_Init(&tmp);
		err = FT_Bitmap_Convert(library, bitmap, &tmp, 4);
		if (err)
			return fterror(err);
		*image = pixman_image_create_bits_no_clear(PIXMAN_a8, tmp.width, tmp.rows, (void *)tmp.buffer, tmp.pitch);
		if (!*image) {
			FT_Bitmap_Done(library, &tmp);
			return NSERROR_NOMEM;
		}
		break;
	default:
		return NSERROR_NOT_IMPLEMENTED;
	}

	return NSERROR_OK;
}

static nserror
lookupglyph(FT_Size size, uint32_t c, struct glyph **glyph, const void **cacheentry)
{
	struct sizedata *data = size->generic.data;
	FT_UInt idx;
	FT_GlyphSlot slot = size->face->glyph;
	struct glyph *g;
	pixman_image_t *image;
	FT_Error err;
	int x, y;
	const void *entry;

	idx = FTC_CMapCache_Lookup(cmapcache, data->faceid, data->faceid->charmap, c);
	g = &data->glyphs[idx];
	if (!g->image) {
		err = fterror(FT_Load_Glyph(size->face, idx, FT_LOAD_FORCE_AUTOHINT));
		if (err)
			return err;
		switch (slot->format) {
		case FT_GLYPH_FORMAT_OUTLINE:
			err = outlineimage(&slot->outline, &x, &y, &image);
			break;
		case FT_GLYPH_FORMAT_BITMAP:
			err = bitmapimage(&slot->bitmap, &image);
			x = -slot->bitmap_left;
			y = slot->bitmap_top;
			break;
		default:
			return NSERROR_NOT_IMPLEMENTED;
		}
		if (err)
			return err;
		pixman_glyph_cache_freeze(glyphcache);
		entry = pixman_glyph_cache_insert(glyphcache, size, g, x, y, image);
		pixman_glyph_cache_thaw(glyphcache);
		if (!entry) {
			pixman_image_unref(image);
			return NSERROR_NOMEM;
		}
		g->image = image;
		g->advance = slot->advance;
		if (cacheentry)
			*cacheentry = entry;
	} else if (cacheentry) {
		*cacheentry = pixman_glyph_cache_lookup(glyphcache, size, g);
		if (!*cacheentry)
			return NSERROR_NOT_FOUND;
	}
	*glyph = g;

	return NSERROR_OK;
}

/* layout */
static nserror
layout_width(const struct plot_font_style *style, const char *string, size_t length, int *width)
{
	FT_Size size;
	uint32_t c;
	size_t n;
	struct glyph *glyph;
	nserror err;

	size = lookupsize(style);
	if (!size)
		return NSERROR_NOT_FOUND;
	*width = 0;
	while (length) {
		c = utf8_to_ucs4(string, length);
		err = lookupglyph(size, c, &glyph, NULL);
		if (err)
			return err;
		*width += glyph->advance.x >> 6;
		n = utf8_next(string, length, 0);
		string += n;
		length -= n;
	}

	return NSERROR_OK;
}

static nserror
layout_position(const struct plot_font_style *style, const char *string, size_t length, int x, size_t *char_offset, int *actual_x)
{
	FT_Size size;
	const char *s;
	uint32_t c;
	size_t n;
	struct glyph *glyph;
	int dx;
	nserror err;

	size = lookupsize(style);
	if (!size)
		return NSERROR_NOT_FOUND;
	dx = x;
	for (s = string; length; s += n, length -= n) {
		c = utf8_to_ucs4(s, length);
		n = utf8_next(s, length, 0);
		err = lookupglyph(size, c, &glyph, NULL);
		if (err)
			return err;
		if (dx < glyph->advance.x >> 7)
			break;
		dx -= glyph->advance.x >> 6;
	}
	*actual_x += x + dx;
	*char_offset = s - string;

	return NSERROR_OK;
}

static nserror
layout_split(const struct plot_font_style *style, const char *string, size_t length, int x, size_t *char_offset, int *actual_x)
{
	FT_Size size;
	const char *s;
	uint32_t c;
	size_t n;
	struct glyph *glyph;
	int width = 0, splitidx = 0, splitx;

	size = lookupsize(style);
	if (!size)
		return NSERROR_NOT_FOUND;
	for (s = string; length; s += n, length -=n) {
		c = utf8_to_ucs4(s, length);
		n = utf8_next(s, length, 0);
		if (lookupglyph(size, c, &glyph, NULL) != NSERROR_OK)
			continue;
		if (c == ' ') {
			splitx = width;
			splitidx = s - string;
		}
		width += glyph->advance.x >> 6;
		if (width > x && splitidx) {
			*actual_x = splitx;
			*char_offset = splitidx;
			return NSERROR_OK;
		}
	}

	*actual_x = width;
	*char_offset = s - string;
	return NSERROR_OK;
}

static struct gui_layout_table layout_table = {
	.width = layout_width,
	.position = layout_position,
	.split = layout_split,
};
struct gui_layout_table *tiny_layout_table = &layout_table;

/* bitmaps */
static void *
bitmap_create(int width, int height, unsigned int state)
{
	pixman_format_code_t format;
	format = (state & BITMAP_OPAQUE) ? PIXMAN_x8b8g8r8 : PIXMAN_a8b8g8r8;
	return pixman_image_create_bits(format, width, height, NULL, 0);
}

static void
bitmap_destroy(void *bitmap)
{
	pixman_image_unref(bitmap);
}

static void
bitmap_set_opaque(void *bitmap, bool opaque)
{
	LOG("bitmap_set_opaque: %d\n", opaque);
}

static bool
bitmap_get_opaque(void *bitmap)
{
	pixman_image_t *image = bitmap;
	return PIXMAN_FORMAT_A(pixman_image_get_format(image)) > 0;
}

static bool
bitmap_test_opaque(void *bitmap)
{
	LOG("bitmap_test_opaque\n");
	return false;
}

static unsigned char *
bitmap_get_buffer(void *bitmap)
{
	pixman_image_t *image = bitmap;
	return (void *)pixman_image_get_data(image);
}

static size_t
bitmap_get_rowstride(void *bitmap)
{
	pixman_image_t *image = bitmap;
	return pixman_image_get_stride(image);
}

static int
bitmap_get_width(void *bitmap)
{
	pixman_image_t *image = bitmap;
	return pixman_image_get_width(image);
}

static int
bitmap_get_height(void *bitmap)
{
	pixman_image_t *image = bitmap;
	return pixman_image_get_height(image);
}

static size_t
bitmap_get_bpp(void *bitmap)
{
	pixman_image_t *image = bitmap;
	return pixman_image_get_depth(image) / 8;
}

static bool
bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	return true;
}

static void
bitmap_modified(void *bitmap)
{
}

static nserror
bitmap_render(struct bitmap *bitmap, struct hlcache_handle *content)
{
	pixman_image_t *image = (void *)bitmap;
	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = tiny_plotter_table,
	};

	target = image;
	content_scaled_redraw(content, pixman_image_get_width(image), pixman_image_get_height(image), &ctx);
	//printf("bitmap_render\n");
	return NSERROR_OK;
}

static struct gui_bitmap_table bitmap_table = {
	.create = bitmap_create,
	.destroy = bitmap_destroy,
	.set_opaque = bitmap_set_opaque,
	.get_opaque = bitmap_get_opaque,
	.test_opaque = bitmap_test_opaque,
	.get_buffer = bitmap_get_buffer,
	.get_rowstride = bitmap_get_rowstride,
	.get_width = bitmap_get_width,
	.get_height = bitmap_get_height,
	.get_bpp = bitmap_get_bpp,
	.save = bitmap_save,
	.modified = bitmap_modified,
	.render = bitmap_render,
};
struct gui_bitmap_table *tiny_bitmap_table = &bitmap_table;

/* plotters */
static bool
plotoutline(FT_Outline *outline, colour c)
{
	pixman_color_t color = PIXMAN_COLOR(c);
	pixman_image_t *image, *solid;
	int x, y, w, h;

	if (outlineimage(outline, &x, &y, &image) != NSERROR_OK)
		goto err0;
	w = pixman_image_get_width(image);
	h = pixman_image_get_height(image);

	solid = pixman_image_create_solid_fill(&color);
	if (!solid)
		goto err1;
	pixman_image_composite32(PIXMAN_OP_OVER, solid, image, target, 0, 0, 0, 0, -x, -y, w, h);
	pixman_image_unref(image);
	pixman_image_unref(solid);

	return true;

err1:
	pixman_image_unref(image);
err0:
	return false;
}

static bool
plot_clip(const struct rect *clip)
{
	pixman_region32_t region;
	pixman_box32_t box;

	if (!clip) {
		curclip = (struct rect){INT_MIN, INT_MIN, INT_MAX, INT_MAX};
		return pixman_image_set_clip_region32(target, NULL);
	}

	curclip = *clip;
	box = (pixman_box32_t){clip->x0, clip->y0, clip->x1, clip->y1};
	pixman_region32_init_with_extents(&region, &box);

	return pixman_image_set_clip_region32(target, &region);
}

static bool
plot_arc(int x, int y, int radius, int angle1, int angle2, const plot_style_t *style)
{
	LOG("plot_arc");
	return true;
}

static bool
plot_disc(int x, int y, int r, const plot_style_t *style)
{
	x <<= 6;
	y <<= 6;
	r <<= 6;

	FT_Outline outline;
	FT_Pos c = (double)r * 4 * (sqrt(2) - 1) / 3;
	FT_Vector points[] = {
		{x - r, -y},
		{x - r, -(y - c)},
		{x - c, -(y - r)},
		{x, -(y - r)},
		{x + c, -(y - r)},
		{x + r, -(y - c)},
		{x + r, -y},
		{x + r, -(y + c)},
		{x + c, -(y + r)},
		{x, -(y + r)},
		{x - c, -(y + r)},
		{x - r, -(y + c)},
	};
	char tags[] = {1, 2, 2, 1, 2, 2, 1, 2, 2, 1, 2, 2};
	short contour = LENGTH(points) - 1;

	outline.n_contours = 1;
	outline.n_points = LENGTH(points);
	outline.contours = &contour;
	outline.points = points;
	outline.tags = tags;
	outline.flags = FT_OUTLINE_OWNER;

	return plotoutline(&outline, style->fill_colour);
}

static bool
plot_line(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	pixman_color_t fill_color = PIXMAN_COLOR(style->stroke_colour);
	pixman_box32_t box = {
		min(x0, x1), min(y0, y1),
		max(x0, x1), max(y0, y1),
	};

	if (!style->stroke_width)
		return true;
	if (box.x1 < curclip.x0)
		box.x1 = min(box.x2, curclip.x0);
	if (box.y1 < curclip.y0)
		box.y1 = min(box.y2, curclip.y0);
	if (curclip.x1 < box.x2)
		box.x2 = max(box.x1, curclip.x1);
	if (curclip.y1 < box.y2)
		box.y2 = max(box.y1, curclip.y1);
	if (box.x1 == box.x2 && box.y1 == box.y2)
		return true;

	if (x0 == x1) {
		box.x1 -= style->stroke_width / 2;
		box.x2 += (style->stroke_width + 1) / 2;
	} else if (y0 == y1) {
		box.y1 -= style->stroke_width / 2;
		box.y2 += (style->stroke_width + 1) / 2;
	} else {
		LOG("plot diagonal line not implemented");
		return false;
	}

	return pixman_image_fill_boxes(PIXMAN_OP_SRC, target, &fill_color, 1, &box);
}

static bool
plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	pixman_color_t fill_color = PIXMAN_COLOR(style->fill_colour);
	pixman_box32_t box = {min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)};
	return pixman_image_fill_boxes(PIXMAN_OP_SRC, target, &fill_color, 1, &box);
}

static bool
plot_polygon(const int *p, unsigned int n, const plot_style_t *style)
{
	FT_Outline outline;
	FT_Vector *v;
	short contour = n - 1;

	outline.n_contours = 1;
	outline.n_points = n;
	outline.contours = &contour;
	outline.points = reallocarray(NULL, n, sizeof(outline.points[0]));
	if (!outline.points)
		goto err0;
	outline.tags = malloc(n);
	if (!outline.tags)
		goto err1;
	memset(outline.tags, 1, n);
	outline.flags = FT_OUTLINE_OWNER;

	for (v = outline.points; n; ++v, --n, p += 2) {
		v->x = p[0] << 6;
		v->y = -p[1] << 6;
	}

	if (!plotoutline(&outline, style->fill_colour))
		goto err2;

	free(outline.tags);
	free(outline.points);

	return true;

err2:
	free(outline.tags);
err1:
	free(outline.points);
err0:
	return false;
}

static bool
plot_path(const float *p, unsigned int n, colour fill, float width, colour c, const float transform[6])
{
	LOG("plot_path\n");
	return false;
}

static bool
plot_bitmap(int x, int y, int width, int height, struct bitmap *bitmap, colour bg, bitmap_flags_t flags)
{
	struct pixman_transform transform;
	pixman_image_t *image = (void *)bitmap;
	pixman_fixed_t sx, sy;

	if (width != pixman_image_get_width(image) || height != pixman_image_get_height(image)) {
		sx = pixman_int_to_fixed(pixman_image_get_width(image)) / width;
		sy = pixman_int_to_fixed(pixman_image_get_height(image)) / height;
		pixman_transform_init_scale(&transform, sx, sy);
		pixman_image_set_transform(image, &transform);
		pixman_image_set_filter(image, PIXMAN_FILTER_GOOD, NULL, 0);
	} else {
		pixman_image_set_transform(image, NULL);
	}
        /* netsurf gives us images with non-premultiplied alpha, so set image as
	 * the mask here so that the bitmap alpha component gets multiplied with
	 * the bitmap color components. */
	pixman_image_composite32(PIXMAN_OP_OVER, image, image, target, 0, 0, 0, 0, x, y, width, height);
	return true;
}

static bool
plot_text(int x, int y, const char *text, size_t length, const struct plot_font_style *style)
{
	FT_Size size;
	struct glyph *glyph;
	uint32_t c;
	int n, i;
	int dx = 0, dy = 0;
	pixman_glyph_t *glyphs;
	size_t nglyphs;
	pixman_image_t *solid;
	const void *entry;
	pixman_color_t color = PIXMAN_COLOR(style->foreground);

	if (!length)
		return true;
	size = lookupsize(style);
	if (!size)
		return false;
	nglyphs = utf8_bounded_length(text, length);
	glyphs = reallocarray(NULL, nglyphs, sizeof(*glyph));
	for (i = 0; length; ++i) {
		c = utf8_to_ucs4(text, length);
		n = utf8_next(text, length, 0);
		if (lookupglyph(size, c, &glyph, &entry) != NSERROR_OK)
			return false;
		glyphs[i].x = dx;
		glyphs[i].y = dy;
		glyphs[i].glyph = entry;
		dx += glyph->advance.x >> 6;
		text += n;
		length -= n;
	}
	solid = pixman_image_create_solid_fill(&color);
	if (!solid) {
		free(glyphs);
		return false;
	}
	pixman_composite_glyphs_no_mask(PIXMAN_OP_OVER, solid, target, 0, 0, x, y, glyphcache, nglyphs, glyphs);
	pixman_image_unref(solid);
	free(glyphs);

	return true;
}

static const struct plotter_table plotter_table = {
	.clip = plot_clip,
	.arc = plot_arc,
	.disc = plot_disc,
	.line = plot_line,
	.rectangle = plot_rectangle,
	.polygon = plot_polygon,
	.path = plot_path,
	.bitmap = plot_bitmap,
	.text = plot_text,
};
const struct plotter_table *tiny_plotter_table = &plotter_table;

/* initialization */
nserror
render_init(void)
{
	struct faceid *face;
	nserror err;

	glyphcache = pixman_glyph_cache_create();
	if (!glyphcache)
		return NSERROR_NOMEM;

	err = fterror(FT_Init_FreeType(&library));
	if (err)
		goto err1;
	// TODO: Tweak this parameters.
	err = fterror(FTC_Manager_New(library, 6, 10, 2048 * 1024, requester, NULL, &manager));
	if (err)
		goto err2;
	err = fterror(FTC_CMapCache_New(manager, &cmapcache));
	if (err)
		goto err3;

	face = newface(nsoption_charp(tiny_face_sans_serif), TINY_FONT_SANS_SERIF);
	if (!face)
		goto err3;
	faces[FACE_SANS_SERIF] = face;

	face = newface(nsoption_charp(tiny_face_sans_serif_bold), TINY_FONT_SANS_SERIF_BOLD);
	faces[FACE_SANS_SERIF_BOLD] = face ? face : faces[FACE_SANS_SERIF];

	face = newface(nsoption_charp(tiny_face_sans_serif_italic), TINY_FONT_SANS_SERIF_ITALIC);
	faces[FACE_SANS_SERIF_ITALIC] = face ? face : faces[FACE_SANS_SERIF];

	face = newface(nsoption_charp(tiny_face_sans_serif_italic_bold), TINY_FONT_SANS_SERIF_ITALIC_BOLD);
	faces[FACE_SANS_SERIF_ITALIC_BOLD] = face ? face : faces[FACE_SANS_SERIF];

	face = newface(nsoption_charp(tiny_face_serif), TINY_FONT_SERIF);
	faces[FACE_SERIF] = face ? face : faces[FACE_SANS_SERIF];

	face = newface(nsoption_charp(tiny_face_serif_bold), TINY_FONT_SERIF_BOLD);
	faces[FACE_SERIF_BOLD] = face ? face : faces[FACE_SERIF];

	face = newface(nsoption_charp(tiny_face_monospace), TINY_FONT_MONOSPACE);
	faces[FACE_MONOSPACE] = face ? face : faces[FACE_SANS_SERIF];

	face = newface(nsoption_charp(tiny_face_monospace), TINY_FONT_MONOSPACE_BOLD);
	faces[FACE_MONOSPACE_BOLD] = face ? face : faces[FACE_MONOSPACE];

	face = newface(nsoption_charp(tiny_face_cursive), TINY_FONT_CURSIVE);
	faces[FACE_CURSIVE] = face ? face : faces[FACE_SANS_SERIF];

	face = newface(nsoption_charp(tiny_face_fantasy), TINY_FONT_FANTASY);
	faces[FACE_FANTASY] = face ? face : faces[FACE_SANS_SERIF];

	return NSERROR_OK;

err3:
	FTC_Manager_Done(manager);
err2:
	FT_Done_FreeType(library);
err1:
	return NSERROR_UNKNOWN;
}

void
render_finalize(void)
{
	FTC_Manager_Done(manager);
	FT_Done_FreeType(library);
	pixman_glyph_cache_destroy(glyphcache);
}

void
render_settarget(pixman_image_t *image)
{
	target = image;
}
