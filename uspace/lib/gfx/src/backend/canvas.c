/*
 * Copyright (c) 2019 Jiri Svoboda
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup libgfx
 * @{
 */
/**
 * @file GFX canvas backend
 *
 * This implements a graphics context over a libgui canvas.
 * This is just for experimentation purposes and its kind of backwards.
 */

#include <gfx/backend/canvas.h>
#include <gfx/context.h>
#include <gfx/render.h>
#include <io/pixel.h>
#include <stdlib.h>
#include "../../private/backend/canvas.h"
#include "../../private/color.h"

static errno_t canvas_gc_set_color(void *, gfx_color_t *);
static errno_t canvas_gc_fill_rect(void *, gfx_rect_t *);

gfx_context_ops_t canvas_gc_ops = {
	.set_color = canvas_gc_set_color,
	.fill_rect = canvas_gc_fill_rect
};

/** Set color on canvas GC.
 *
 * Set drawing color on canvas GC.
 *
 * @param arg Canvas GC
 * @param color Color
 *
 * @return EOK on success or an error code
 */
static errno_t canvas_gc_set_color(void *arg, gfx_color_t *color)
{
	canvas_gc_t *cgc = (canvas_gc_t *) arg;

	cgc->color = PIXEL(0, color->r >> 8, color->g >> 8, color->b >> 8);
	return EOK;
}

/** Fill rectangle on canvas GC.
 *
 * @param arg Canvas GC
 * @param rect Rectangle
 *
 * @return EOK on success or an error code
 */
static errno_t canvas_gc_fill_rect(void *arg, gfx_rect_t *rect)
{
	canvas_gc_t *cgc = (canvas_gc_t *) arg;
	int x, y;

	// XXX We should handle p0.x > p1.x and p0.y > p1.y

	for (y = rect->p0.y; y < rect->p1.y; y++) {
		for (x = rect->p0.x; x < rect->p1.x; x++) {
			surface_put_pixel(cgc->surface, x, y, cgc->color);
		}
	}

	update_canvas(cgc->canvas, cgc->surface);

	return EOK;
}

/** Create canvas GC.
 *
 * Create graphics context for rendering into a canvas.
 *
 * @param con Canvas object
 * @param fout File to which characters are written (canvas)
 * @param rgc Place to store pointer to new GC.
 *
 * @return EOK on success or an error code
 */
errno_t canvas_gc_create(canvas_t *canvas, surface_t *surface,
    canvas_gc_t **rgc)
{
	canvas_gc_t *cgc = NULL;
	gfx_context_t *gc = NULL;
	errno_t rc;

	cgc = calloc(1, sizeof(canvas_gc_t));
	if (cgc == NULL) {
		rc = ENOMEM;
		goto error;
	}

	rc = gfx_context_new(&canvas_gc_ops, cgc, &gc);
	if (rc != EOK)
		goto error;

	cgc->gc = gc;
	cgc->canvas = canvas;
	cgc->surface = surface;
	*rgc = cgc;
	return EOK;
error:
	if (cgc != NULL)
		free(cgc);
	gfx_context_delete(gc);
	return rc;
}

/** Delete canvas GC.
 *
 * @param cgc Canvas GC
 */
errno_t canvas_gc_delete(canvas_gc_t *cgc)
{
	errno_t rc;

	rc = gfx_context_delete(cgc->gc);
	if (rc != EOK)
		return rc;

	free(cgc);
	return EOK;
}

/** Get generic graphic context from canvas GC.
 *
 * @param cgc Canvas GC
 * @return Graphic context
 */
gfx_context_t *canvas_gc_get_ctx(canvas_gc_t *cgc)
{
	return cgc->gc;
}

/** @}
 */
