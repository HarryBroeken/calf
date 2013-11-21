/* Calf DSP Library
 * Custom controls (line graph, knob).
 * Copyright (C) 2007-2010 Krzysztof Foltman, Torben Hohn, Markus Schmidt
 * and others
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <calf/ctl_linegraph.h>
#include <gdk/gdkkeysyms.h>
#include <cairo/cairo.h>
#include <math.h>
#include <gdk/gdk.h>
#include <sys/time.h>
#include <iostream>
#include <calf/giface.h>

using namespace calf_plugins;

static void
calf_line_graph_draw_grid( CalfLineGraph* lg, cairo_t *ctx, std::string &legend, bool vertical, float pos )
{
    int sx = lg->size_x;
    int sy = lg->size_y;
    int ox = lg->pad_x;
    int oy = lg->pad_y;
    
    float x = 0, y = 0;
    
    cairo_text_extents_t tx;
    int size;
    if (!legend.empty()) {
        cairo_text_extents(ctx, legend.c_str(), &tx);
        size = vertical ? tx.height : tx.width;
        size += 5;
    } else {
        size = 0;
    }
    
    if (vertical)
    {
        x = floor(ox + pos * sx) + 0.5;
        cairo_move_to(ctx, x, oy);
        cairo_line_to(ctx, x, oy + sy - size);
        cairo_stroke(ctx);
        if (!legend.empty()) {
            cairo_set_source_rgba(ctx, 0.0, 0.0, 0.0, 0.5);
            cairo_move_to(ctx, x - (tx.x_bearing + tx.width / 2.0), oy + sy - 2);
            cairo_show_text(ctx, legend.c_str());
        }
    }
    else
    {
        y = floor(oy + sy / 2 - (sy / 2 - 1) * pos) + 0.5;
        cairo_move_to(ctx, ox, y);
        cairo_line_to(ctx, ox + sx - size, y);
        cairo_stroke(ctx);
        
        if (!legend.empty()) {
            cairo_set_source_rgba(ctx, 0.0, 0.0, 0.0, 0.5);
            cairo_move_to(ctx, ox + sx - 4 - tx.width, y + tx.height/2 - 2);
            cairo_show_text(ctx, legend.c_str());
        }
    }
    if (lg->debug > 2) printf("* grid vert: %d, x: %d, y: %d, label: %s\n", vertical ? 1 : 0, (int)x, (int)y, legend.c_str());
}

static int
calf_line_graph_draw_graph( CalfLineGraph* lg, cairo_t *ctx, float *data, int mode = 0 )
{
    if (lg->debug) printf("(draw graph)\n");
    
    int sx = lg->size_x;
    int sy = lg->size_y;
    int ox = lg->pad_x;
    int oy = lg->pad_y;
    
    int _last = 0;
    float y = 0.f;
    int startdraw = -1;
    
    for (int i = 0; i < sx; i++) {
        y = (oy + sy / 2 - (sy / 2 - 1) * data[i]);
        if (lg->debug > 2) printf("* graph x: %d, y: %.5f, data: %.5f\n", i, y, data[i]);
        switch (mode) {
            case 0:
            default:
                // we want to draw a line
                if (i and (data[i] < INFINITY or i == sx - 1)) {
                    cairo_line_to(ctx, ox + i, y);
                } else if (i and startdraw >= 0) {
                    continue;
                } else {
                    cairo_move_to(ctx, ox, y);
                    if (startdraw < 0)
                        startdraw = i;
                }
                break;
            case 1:
                // bars are used
                if (i and ((data[i] < INFINITY) or i == sx - 1)) {
                    cairo_rectangle(ctx, ox + _last, (int)y, i - _last, sy - (int)y + oy);
                    _last = i;
                    if (startdraw < 0)
                        startdraw = ox + _last;
                } else {
                    continue;
                }
                break;
            case 2:
                // this one is drawing little boxes at the values position
                if (i and ((data[i] < INFINITY) or i == sx - 1)) {
                    cairo_rectangle(ctx, ox + _last, (int)y - 1, i - _last, 2);
                    _last = i;
                    if (startdraw < 0)
                        startdraw = ox + _last;
                } else {
                    continue;
                }
                break;
            case 3:
                // this one is drawing bars centered on the x axis
                if (i and ((data[i] < INFINITY) or i == sx - 1)) {
                    cairo_rectangle(ctx, ox + _last, oy + sy / 2, i - _last, -1 * data[i] * (sy / 2));
                    _last = i;
                    if (startdraw < 0)
                        startdraw = ox + _last;
                } else {
                    continue;
                }
                break;
            case 4:
                // this one is drawing bars centered on the x axis with 1
                // as the center
                if (i and ((data[i] < INFINITY) or i == sx - 1)) {
                    cairo_rectangle(ctx, ox + _last, oy + sy / 2, i - _last, -1 * data[i] * (sy / 2));
                    _last = i;
                    if (startdraw < 0)
                        startdraw = ox + _last;
                } else {
                    continue;
                }
                break;
        }
    }
    if(!mode) {
        cairo_stroke(ctx);
    } else {
        cairo_fill(ctx);
    }
    return startdraw;
}

static void
calf_line_graph_draw_moving( CalfLineGraph* lg, cairo_t *ctx, float *data, int direction, int count )
{
    if (lg->debug) printf("(draw moving)\n");
    
    int sx = lg->size_x;
    int sy = lg->size_y;
    int ox = lg->pad_x;
    int oy = lg->pad_y;
    
    int _last = 0;
    int startdraw = -1;
    
    int sm = (direction == LG_MOVING_UP || direction == LG_MOVING_DOWN ? sy : sx);
    int om = (direction == LG_MOVING_UP || direction == LG_MOVING_DOWN ? oy : ox);
    for (int i = 0; i < sm; i++) {
        if (lg->debug > 2) printf("* moving i: %d, dir: %d, data: %.5f\n", i, direction, data[i]);
        if (i and ((data[i] < INFINITY) or i == sm - 1)) {
            cairo_set_source_rgba(ctx, 0.35, 0.4, 0.2, (data[i] + 1) / 2.f);
            switch (direction) {
                case LG_MOVING_UP:
                    cairo_rectangle(ctx, ox + _last, oy + sy - 1 - count, i - _last, 1);
                    break;
                case LG_MOVING_DOWN:
                    cairo_rectangle(ctx, ox + _last, oy + count, i - _last, 1);
                    break;
                case LG_MOVING_LEFT:
                    cairo_rectangle(ctx, ox + sx - 1 - count, oy + _last, 1, i - _last);
                    break;
                case LG_MOVING_RIGHT:
                    cairo_rectangle(ctx, ox + count, oy + _last, 1, i - _last);
                    break;
            }
            
            cairo_fill(ctx);
            _last = i;
            if (startdraw < 0)
                startdraw = om + _last;
        }
        else
            continue;
    }
    
}

void calf_line_graph_draw_crosshairs(CalfLineGraph* lg, cairo_t* cache_cr, bool gradient, int gradient_rad, float alpha, int mask, bool circle, int x, int y, std::string label)
{
    if (lg->debug) printf("(draw crosshairs)\n");
    // crosshairs
    
    int sx = lg->size_x;
    int sy = lg->size_y;
    int ox = lg->pad_x;
    int oy = lg->pad_y;
    
    int _x = ox + x;
    int _y = ox + y;
    
    cairo_pattern_t *pat;
    
    if(mask > 0 and circle) {
        // draw a circle in the center of the crosshair leaving out
        // the lines
        // ne
        cairo_move_to(cache_cr, _x + 1, _y);
        cairo_arc (cache_cr, _x + 1, _y, mask, 1.5 * M_PI, 2 * M_PI);
        cairo_close_path(cache_cr);
        // se
        cairo_move_to(cache_cr, _x + 1, _y + 1);
        cairo_arc (cache_cr, _x + 1, _y + 1, mask, 0, 0.5 * M_PI);
        cairo_close_path(cache_cr);
        // sw
        cairo_move_to(cache_cr, _x, _y + 1);
        cairo_arc (cache_cr, _x, _y + 1, mask, 0.5 * M_PI, M_PI);
        cairo_close_path(cache_cr);
        // nw
        cairo_move_to(cache_cr, _x, _y);
        cairo_arc (cache_cr, _x, _y, mask, M_PI, 1.5 * M_PI);
        cairo_close_path(cache_cr);
        
        cairo_set_source_rgba(cache_cr, 0, 0, 0, alpha);
        cairo_fill(cache_cr);
    }
    if(gradient and gradient_rad > 0) {
        // draw the crosshairs with a steady gradient around
        pat = cairo_pattern_create_radial(_x, _y, 1, _x, _y, gradient_rad * 2);
        cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, alpha);
        cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, 0);
        // top
        cairo_rectangle(cache_cr, _x, _y - gradient_rad, 1, gradient_rad - mask);
        // right
        cairo_rectangle(cache_cr, _x + mask, _y, gradient_rad - mask, 1);
        // bottom
        cairo_rectangle(cache_cr, _x, _y + mask, 1, gradient_rad - mask);
        // left
        cairo_rectangle(cache_cr, _x - gradient_rad, _y, gradient_rad - mask, 1);
        
        cairo_set_source(cache_cr, pat);
        cairo_fill(cache_cr);
    } else if(gradient) {
        // draw the crosshairs with a gradient to the frame
        // top
        cairo_rectangle(cache_cr, _x, oy, 1, y - mask);
        pat = cairo_pattern_create_linear(_x, oy, _x, _y);
        cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, 0);
        cairo_pattern_add_color_stop_rgba(pat, 1, 0, 0, 0, alpha);
        cairo_set_source(cache_cr, pat);
        cairo_fill(cache_cr);
        // right
        cairo_rectangle(cache_cr, _x + mask, _y, sx - x - mask, 1);
        pat = cairo_pattern_create_linear(_x, oy, sx, oy);
        cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, alpha);
        cairo_pattern_add_color_stop_rgba(pat, 1, 0, 0, 0, 0);
        cairo_set_source(cache_cr, pat);
        cairo_fill(cache_cr);
        // bottom
        cairo_rectangle(cache_cr, _x, _y + mask, 1, sy - y - mask);
        pat = cairo_pattern_create_linear(_x, _y, _x, oy + sy);
        cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, alpha);
        cairo_pattern_add_color_stop_rgba(pat, 1, 0, 0, 0, 0);
        cairo_set_source(cache_cr, pat);
        cairo_fill(cache_cr);
        // left
        cairo_rectangle(cache_cr, ox, _y, x - mask, 1);
        pat = cairo_pattern_create_linear(ox, oy, _x, oy);
        cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, 0);
        cairo_pattern_add_color_stop_rgba(pat, 1, 0, 0, 0, alpha);
        cairo_set_source(cache_cr, pat);
        cairo_fill(cache_cr);
    } else {
        // draw normal crosshairs
        // top
        cairo_move_to(cache_cr, _x + 0.5, oy + 0.5);
        cairo_line_to(cache_cr, _x + 0.5, _y - mask + 0.5);
        // right
        cairo_move_to(cache_cr, _x + mask + 0.5, _y + 0.5);
        cairo_line_to(cache_cr, ox + sx + 0.5, _y + 0.5);
        // bottom
        cairo_move_to(cache_cr, _x + 0.5, _y + mask + 0.5);
        cairo_line_to(cache_cr, _x + 0.5, oy + sy + 0.5);
        // left
        cairo_move_to(cache_cr, ox + 0.5, _y + 0.5);
        cairo_line_to(cache_cr, _x - mask + 0.5, _y + 0.5);
        
        cairo_set_source_rgba(cache_cr, 0, 0, 0, alpha);
        cairo_stroke(cache_cr);
    }
    if(label != "") {
        // draw label
        cairo_set_source_rgba(cache_cr, 0, 0, 0, 0.5);
        cairo_move_to(cache_cr, lg->mouse_x + 3, lg->mouse_y - 3);
        cairo_show_text(cache_cr, label.c_str());
        cairo_fill(cache_cr);
    }
}


void calf_line_graph_draw_freqhandles(CalfLineGraph* lg, cairo_t* c)
{
    // freq_handles
    if (lg->debug) printf("(draw handles)\n");
    
    int sx = lg->size_x;
    int sy = lg->size_y;
    int ox = lg->pad_x;
    int oy = lg->pad_y;
    
    if (lg->freqhandles > 0) {
        cairo_set_source_rgba(c, 0.0, 0.0, 0.0, 1.0);
        cairo_set_line_width(c, 1.0);

        for (int i = 0; i < lg->freqhandles; i++) {
            FreqHandle *handle = &lg->freq_handles[i];
            if(!handle->is_active())
                continue;
            
            
            if (handle->value_x > 0.0 && handle->value_x < 1.0) {
                int val_x = round(handle->value_x * sx);
                int val_y = (handle->dimensions >= 2) ? round(handle->value_y * sy) : 0;
                float pat_alpha;
                bool grad;
                // choose colors between dragged and normal state
                if (lg->handle_hovered == i) {
                    pat_alpha = 0.3;
                    grad = false;
                    cairo_set_source_rgba(c, 0, 0, 0, 0.7);
                } else {
                    pat_alpha = 0.15;
                    grad = true;
                    //cairo_set_source_rgb(c, 0.44, 0.5, 0.21);
                    cairo_set_source_rgba(c, 0, 0, 0, 0.5);
                }
                if (handle->dimensions >= 2) {
                    cairo_move_to(c, val_x + 11, val_y);
                } else {
                    cairo_move_to(c, val_x + 11, oy + 15);
                }
                // draw the freq label
                float freq = exp((handle->value_x) * log(1000)) * 20.0;
                std::stringstream ss;
                ss << round(freq) << " Hz";
                cairo_show_text(c, ss.str().c_str());
                
                // draw the label on top
                if (handle->label && handle->label[0]) {
                    
                    cairo_select_font_face(c, "Bitstream Vera Sans",
                            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
                    cairo_set_font_size(c, 9);
                    cairo_text_extents_t te;

                    cairo_text_extents(c, handle->label, &te);
                    if (handle->dimensions >= 2) {
                        cairo_move_to(c, val_x - 3 - te.width, val_y);
                    } else {
                        cairo_move_to(c, val_x - 3 - te.width, oy + 15);
                    }
                    cairo_show_text(c, handle->label);
                }
                cairo_stroke(c);
                
                if (handle->dimensions == 1) {
                    // draw the main line
                    cairo_move_to(c, ox + val_x + 0.5, oy);
                    cairo_line_to(c, ox + val_x + 0.5, oy + sy);
                    cairo_stroke(c);
                    // draw some one-dimensional bling-bling
                    cairo_pattern_t *pat;
                    switch(handle->style) {
                        default:
                        case 0:
                            // bell filters, default
                            pat = cairo_pattern_create_linear(ox, oy,  ox, sy);
                            cairo_pattern_add_color_stop_rgba(pat, 0.f, 0, 0, 0, 0);
                            cairo_pattern_add_color_stop_rgba(pat, 0.5, 0, 0, 0, pat_alpha);
                            cairo_pattern_add_color_stop_rgba(pat, 1.f, 0, 0, 0, 0);
                            cairo_rectangle(c, ox + val_x - 7, oy, 6, sy);
                            cairo_rectangle(c, ox + val_x + 2, oy, 6, sy);
                            break;
                        case 1:
                            // hipass
                            pat = cairo_pattern_create_linear(ox, oy, ox + val_x, oy);
                            cairo_pattern_add_color_stop_rgba(pat, 0.f, 0, 0, 0, 0);
                            cairo_pattern_add_color_stop_rgba(pat, 1.f, 0, 0, 0, pat_alpha);
                            cairo_rectangle(c, ox, oy, val_x - 1, sy);
                            break;
                        case 2:
                            // loshelf
                            pat = cairo_pattern_create_linear(ox, oy, ox, sy);
                            cairo_pattern_add_color_stop_rgba(pat, 0.f, 0, 0, 0, 0);
                            cairo_pattern_add_color_stop_rgba(pat, 0.5, 0, 0, 0, pat_alpha * 1.5);
                            cairo_pattern_add_color_stop_rgba(pat, 1.f, 0, 0, 0, 0);
                            cairo_rectangle(c, ox, oy, val_x - 1, sy);
                            break;
                        case 3:
                            // hishelf
                            pat = cairo_pattern_create_linear(ox, oy, ox, sy);
                            cairo_pattern_add_color_stop_rgba(pat, 0.f, 0, 0, 0, 0);
                            cairo_pattern_add_color_stop_rgba(pat, 0.5, 0, 0, 0, pat_alpha * 1.5);
                            cairo_pattern_add_color_stop_rgba(pat, 1.f, 0, 0, 0, 0);
                            cairo_rectangle(c, ox + val_x + 2, oy, sx - val_x - 2, sy);
                            break;
                        case 4:
                            // lopass
                            pat = cairo_pattern_create_linear(ox + val_x, oy, ox + sx, oy);
                            cairo_pattern_add_color_stop_rgba(pat, 0.f, 0, 0, 0, pat_alpha);
                            cairo_pattern_add_color_stop_rgba(pat, 1.f, 0, 0, 0, 0);
                            cairo_rectangle(c, ox + val_x + 2, oy, sx - val_x - 1, sy);
                            break;
                    }
                    cairo_set_source(c, pat);
                    cairo_fill(c);
                    cairo_pattern_destroy(pat);
                } else {
                    int mask = 30 - log10(1 + handle->value_z * 9) * 30 + 7;
                    // (CalfLineGraph* lg, cairo_t* c, bool gradient, int gradient_rad, float alpha, int mask, bool circle, int x, int y, std::string label, int ox, int oy, int sx, int sy)
                    std::string s = "";
                    calf_line_graph_draw_crosshairs(lg, c, grad, -1, pat_alpha, mask, true, val_x, val_y, s);
                    
                }
            }
        }
    }
}

void calf_line_graph_draw_background(CalfLineGraph* lg, cairo_t* c) 
{
    if (lg->debug) printf("(draw background)\n");
    
    // outer frame (black)
    int pad = 0;
    
    int sx = lg->size_x;
    int sy = lg->size_y;
    int ox = lg->pad_x;
    int oy = lg->pad_y;
    
    cairo_rectangle(
        c, pad, pad, sx + ox * 2 - pad * 2, sy + oy * 2 - pad * 2);
    cairo_set_source_rgb(c, 0, 0, 0);
    cairo_fill(c);
            
    
    // black light effect
    pad = 1;
    cairo_rectangle(
        c, pad, pad, sx + ox * 2 - pad * 2, sy + oy * 2 - pad * 2);
    cairo_pattern_t *pat2 = cairo_pattern_create_linear (
        0, 0, 0, sy + oy * 2 - pad * 2);
    cairo_pattern_add_color_stop_rgba (pat2, 0, 0.23, 0.23, 0.23, 1);
    cairo_pattern_add_color_stop_rgba (pat2, 0.5, 0, 0, 0, 1);
    cairo_set_source (c, pat2);
    cairo_fill(c);
    cairo_pattern_destroy(pat2);
            
    cairo_rectangle(c, ox - 1, oy - 1, sx + 2, sy + 2);
    cairo_set_source_rgb (c, 0, 0, 0);
    cairo_fill(c);
    
    // inner yellowish screen
    cairo_pattern_t *pt = cairo_pattern_create_linear(ox, oy, ox, sy);
    cairo_pattern_add_color_stop_rgb(pt, 0.0,     0.71, 0.82, 0.33);
    cairo_pattern_add_color_stop_rgb(pt, 1.0,     0.89, 1.00, 0.54);
    cairo_set_source (c, pt);
    cairo_rectangle(c, ox, oy, sx, sy);
    cairo_fill(c);
    cairo_pattern_destroy(pt);
    
    // top shadow
    pt = cairo_pattern_create_linear(ox, oy, ox, oy + 7);
    cairo_pattern_add_color_stop_rgba(pt, 0.0, 0,0,0,0.6);
    cairo_pattern_add_color_stop_rgba(pt, 1.0, 0,0,0,0);
    cairo_set_source (c, pt);
    cairo_rectangle(c, ox, oy, sx, 7);
    cairo_fill(c);
    cairo_pattern_destroy(pt);
    
    // left shadow
    pt = cairo_pattern_create_linear(ox, oy, ox + 5, oy);
    cairo_pattern_add_color_stop_rgba(pt, 0.0, 0,0,0,0.3);
    cairo_pattern_add_color_stop_rgba(pt, 1.0, 0,0,0,0);
    cairo_set_source (c, pt);
    cairo_rectangle(c, ox, oy, 5, sy);
    cairo_fill(c);
    cairo_pattern_destroy(pt);
    
    // right shadow
    pt = cairo_pattern_create_linear(ox + sx - 5, oy, ox + sx, oy);
    cairo_pattern_add_color_stop_rgba(pt, 0.0, 0,0,0,0);
    cairo_pattern_add_color_stop_rgba(pt, 1.0, 0,0,0,0.3);
    cairo_set_source (c, pt);
    cairo_rectangle(c, ox + sx - 5, oy, 5, sy);
    cairo_fill(c);
    cairo_pattern_destroy(pt);
    
    // left dull
    pt = cairo_pattern_create_linear(ox, oy, ox + sx / 2, oy);
    cairo_pattern_add_color_stop_rgba(pt, 0.0, 0,0,0,0.1);
    cairo_pattern_add_color_stop_rgba(pt, 1.0, 0,0,0,0);
    cairo_set_source (c, pt);
    cairo_rectangle(c, ox, oy, sx / 2, sy);
    cairo_fill(c);
    cairo_pattern_destroy(pt);
    
    // right dull
    pt = cairo_pattern_create_linear(ox + sx / 2, oy, ox + sx, oy);
    cairo_pattern_add_color_stop_rgba(pt, 0.0, 0,0,0,0);
    cairo_pattern_add_color_stop_rgba(pt, 1.0, 0,0,0,0.1);
    cairo_set_source (c, pt);
    cairo_rectangle(c, ox + sx / 2, oy, sx / 2, sy);
    cairo_fill(c);
    cairo_pattern_destroy(pt);
    
    // light sources
    int div = 1;
    int light_w = sx;
    while(light_w / div > 300) 
        div += 1;
    int w = sx / div;
    cairo_rectangle(c, ox, oy, sx, sy);
    for(int i = 0; i < div; i ++) {
        cairo_pattern_t *pt = cairo_pattern_create_radial(
           ox + w * i + w / 2.f, oy, 1,
           ox + w * i + w / 2.f, ox + sy * 0.25, w / 2.f);
        cairo_pattern_add_color_stop_rgba (pt, 0, 1, 1, 0.8, 0.9);
        cairo_pattern_add_color_stop_rgba (pt, 1, 0.89, 1.00, 0.45, 0);
        cairo_set_source (c, pt);
        cairo_fill_preserve(c);
        pt = cairo_pattern_create_radial(
           ox + w * i + w / 2.f, oy + sy, 1,
           ox + w * i + w / 2.f, ox + sy * 0.75, w / 2.f);
        cairo_pattern_add_color_stop_rgba (pt, 0, 1, 1, 0.8, 0.9);
        cairo_pattern_add_color_stop_rgba (pt, 1, 0.89, 1.00, 0.45, 0);
        cairo_set_source (c, pt);
        cairo_fill_preserve(c);
    }
}
static void
calf_line_graph_destroy_surfaces (GtkWidget *widget)
{
    g_assert(CALF_IS_LINE_GRAPH(widget));
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);
    
    if (lg->debug) printf("{destroy surfaces}\n");
    
    // destroy all surfaces - and don't tell anybody about it - hehe
    if( lg->background_surface )
        cairo_surface_destroy( lg->background_surface );
    if( lg->grid_surface )
        cairo_surface_destroy( lg->grid_surface );
    if( lg->cache_surface )
        cairo_surface_destroy( lg->cache_surface );
    if( lg->moving_surface[0] )
        cairo_surface_destroy( lg->moving_surface[0] );
    if( lg->moving_surface[1] )
        cairo_surface_destroy( lg->moving_surface[1] );
    if( lg->handles_surface )
        cairo_surface_destroy( lg->handles_surface );
    if( lg->realtime_surface )
        cairo_surface_destroy( lg->realtime_surface );
}
static void
calf_line_graph_create_surfaces (GtkWidget *widget)
{
    g_assert(CALF_IS_LINE_GRAPH(widget));
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);
    
    if (lg->debug) printf("{create surfaces}\n");
    
    int width  = widget->allocation.width;
    int height = widget->allocation.height;
    
    // the size of the "real" drawing area
    lg->size_x = width  - lg->pad_x * 2;
    lg->size_y = height - lg->pad_y * 2;
        
    calf_line_graph_destroy_surfaces(widget);
    // create the background surface.
    // background holds the graphics of the frame and the yellowish
    // background light for faster redrawing of static stuff
    lg->background_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height );
    
    // create the grid surface.
    // this one is used as a cache for the grid on the background in the
    // cache phase. If a graph or dot in cache phase needs to be redrawn
    // we don't need to redraw the whole grid.
    lg->grid_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height );
        
    // create the cache surface.
    // cache holds a copy of the background with a static part of
    // the grid and some static curves and dots.
    lg->cache_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height );
    
    // create the moving surface.
    // moving is used as a cache for any slowly moving graphics like
    // spectralizer or waveforms
    lg->moving_surface[0] = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height );
    
    // create the moving temp surface.
    // moving is used as a cache for any slowly moving graphics like
    // spectralizer or waveforms
    lg->moving_surface[1] = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height );
        
    // create the handles surface.
    // this one contains the handles graphics to avoid redrawing
    // each cycle
    lg->handles_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height );
        
    // create the realtime surface.
    // realtime is used to cache the realtime graphics for drawing the
    // crosshairs on top if nothing else changed
    lg->realtime_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height );
        
    lg->force_cache = true;
}

static cairo_t
*calf_line_graph_switch_context(CalfLineGraph* lg, cairo_t *ctx, cairo_impl *cimpl)
{
    if (lg->debug) printf("{switch context}\n");
    
    cimpl->context = ctx;
    cairo_select_font_face(ctx, "Bitstream Vera Sans",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(ctx, 9);
    cairo_set_line_join(ctx, CAIRO_LINE_JOIN_MITER);
    cairo_rectangle(ctx, lg->pad_x, lg->pad_y, lg->size_x, lg->size_y);
    cairo_clip(ctx);
    return ctx;
}

static void
calf_line_graph_copy_surface(cairo_t *ctx, cairo_surface_t *source, float fade = 1.f)
{
    // copy a surface to a cairo context
    cairo_save(ctx);
    cairo_set_source_surface(ctx, source, 0, 0);
    if (fade < 1.0) {
        cairo_paint_with_alpha(ctx, fade * 0.35 + 0.05);
    } else {
        cairo_paint(ctx);
    }
    cairo_restore(ctx);
}

static void
calf_line_graph_clear_surface(cairo_t *ctx)
{
    // clears a surface to transparent
    cairo_save (ctx);
    cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
    cairo_paint (ctx);
    cairo_restore (ctx);
}

void calf_line_graph_expose_request (GtkWidget *widget, bool force)
{
    // someone thinks we should redraw the line graph. let's see what
    // the plugin thinks about. To do that a bitmask is sent to the
    // plugin which can be changed. If the plugin returns true or if
    // the request is in response of something like dragged handles, an
    // exposition of the widget is requested from GTK
    
    g_assert(CALF_IS_LINE_GRAPH(widget));
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);
    
    // quit if no source available
    if (!lg->source) return;
    
    if (lg->debug > 1) printf("\n\n### expose request %d ###\n", lg->generation);
    
    // let a bitmask be switched by the plugin to determine the layers
    // we want to draw. We set all cache layers to true if force_cache
    // is set otherwise default is to draw nothing. The return value
    // tells us whether the plugin wants to draw at all or not.
    lg->layers = 0;
    //if (lg->force_cache || lg->recreate_surfaces)
        //lg->layers |= LG_CACHE_GRID | LG_CACHE_GRAPH | LG_CACHE_DOT | LG_CACHE_MOVING;
    
    //if (lg->debug > 1) {
        //printf("bitmask ");
        //dsp::print_bits(sizeof(lg->layers), &lg->layers);
        //printf("\n");
    //}
    
    // if plugin returns true (something has obviously changed) or if
    // the requestor forces a redraw, request an exposition of the widget
    // from GTK
    if (lg->source->get_layers(lg->source_id, lg->generation, lg->layers) or force)
        gtk_widget_queue_draw(widget);
}

static gboolean
calf_line_graph_expose (GtkWidget *widget, GdkEventExpose *event)
{
    g_assert(CALF_IS_LINE_GRAPH(widget));
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);
    
    // quit if no source available
    if (!lg->source) return FALSE;
    
    if (lg->debug) printf("\n\n####### exposing %d #######\n", lg->generation);
    
    // cairo context of the window
    cairo_t *c            = gdk_cairo_create(GDK_DRAWABLE(widget->window));
    
    // recreate surfaces if someone needs it (init of the widget,
    // resizing the window..)
    if (lg->recreate_surfaces) {
        if (lg->debug) printf("recreation...\n");
        calf_line_graph_create_surfaces(widget);
        
        // all surfaces were recreated, so background is empty.
        // draw the yellowish lighting on the background surface
        cairo_t *bg = cairo_create(lg->background_surface);
        calf_line_graph_draw_background(lg, bg);
        cairo_destroy(bg);
    }
    
    // the cache, grid and realtime surface wrapped in a cairo context
    cairo_t *grid_c       = cairo_create( lg->grid_surface );
    cairo_t *cache_c      = cairo_create( lg->cache_surface );
    cairo_t *realtime_c   = cairo_create( lg->realtime_surface );
    
    if (lg->recreate_surfaces) {
        // and copy it to the grid surface in case no grid is drawn
        if (lg->debug) printf("copy bg->grid\n");
        calf_line_graph_copy_surface(grid_c, lg->background_surface);
        
        // and copy it to the cache surface in case no cache is drawn
        if (lg->debug) printf("copy bg->cache\n");
        calf_line_graph_copy_surface(cache_c, lg->background_surface);
        
        // and copy it to the realtime surface in case no realtime is drawn
        if (lg->debug) printf("copy bg->realtime\n");
        calf_line_graph_copy_surface(realtime_c, lg->background_surface);
    }
    if (lg->recreate_surfaces or lg->force_redraw) {
        // reset generation value and request a new expose event
        lg->generation = 0;
        lg->source->get_layers(lg->source_id, lg->generation, lg->layers);
    }
    
    int sx = lg->size_x;
    int sy = lg->size_y;
    int ox = lg->pad_x;
    int oy = lg->pad_y;
    
    if (lg->debug) printf("width: %d height: %d x: %d y: %d\n", sx, sy, ox, oy);
    
    if (lg->debug) {
        printf("bitmask ");
        dsp::print_bits(sizeof(lg->layers), &lg->layers);
        printf("\n");
    }
    
    // context used for the actual surface we want to draw on. It is
    // switched over the drawing process via calf_line_graph_switch_context
    cairo_t *ctx, *_ctx;
    
    // the contexts for both moving curve caches
    cairo_t *moving_c[2];
    moving_c[0]           = cairo_create( lg->moving_surface[0] );
    moving_c[1]           = cairo_create( lg->moving_surface[1] );
    
    // the colors to reset to between the drawing cycles
    GdkColor grid_color   = { (int)(0.66 * 65535), (int)(0.15 * 65535),
                              (int)(0.2 * 65535), (int)(0.0 * 65535) };
    GdkColor graph_color  = { (int)(1 * 65535), (int)(0.35 * 65535),
                              (int)(0.4 * 65535), (int)(0.2 * 65535) };
    GdkColor dot_color    = { (int)(1 * 65535), (int)(0.35 * 65535),
                              (int)(0.4 * 65535), (int)(0.2 * 65535) };
    GdkColor moving_color = { (int)(1 * 65535), (int)(0.35 * 65535),
                              (int)(0.4 * 65535), (int)(0.2 * 65535) };
    
    // the line widths to switch to between cycles
    float grid_width  = 1.0;
    float graph_width = 1.5;
    float dot_width   = 0.0;
    
    // more vars we have to initialize, mainly stuff we use in callback
    // functions
    float *data        = new float[2 * std::max(lg->size_x, lg->size_y)];
    float pos          = 0;
    bool vertical      = false;
    std::string legend = "";
    int size           = 0;
    int direction      = 0;
    float x, y;
    
    // a cairo wrapper to hand over contexts to the plugin for setting
    // line colors, widths aso
    cairo_impl cimpl;
    
    // some state variables used to determine what has happened
    bool realtime_drawn = false;
    bool cache_drawn    = false;
    bool grid_drawn     = false;
    
    int drawing_phase;
    
    // check if we can skip the whole drawing stuff and go on with
    // copying everything we drawed before
    
    if (!lg->layers)
        goto finalize;
    
    
    // 
    
    
        
    if ( lg->force_cache
      or lg->force_redraw
      or lg->layers & LG_CACHE_GRID
      or lg->layers & LG_CACHE_GRAPH
      or lg->layers & LG_CACHE_DOT) {
        if (lg->debug) printf("\n->cache\n");
        
        // someone needs a redraw of the cache so start with the cache
        // phase
        drawing_phase = 0; 
        
        // set the right context to work with
        _ctx = cache_c;
        
        // and switch to grid surface in case we want to draw on it
        if (lg->debug) printf("switch to grid\n");
        ctx = calf_line_graph_switch_context(lg, grid_c, &cimpl);
    } else {
        if (lg->debug) printf("\n->realtime\n");
        
        // no cache drawing neccessary, so skip the first drawing phase
        drawing_phase = 1;
        
        // set the right context to work with
        _ctx        = realtime_c;
        
        // and switch to the realtime surface
        if (lg->debug) printf("switch to realtime\n");
        ctx = calf_line_graph_switch_context(lg, realtime_c, &cimpl);
    }
    
    
    
    for (int phase = drawing_phase; phase < 2; phase++) {
        // draw elements on the realtime and/or the cache surface
        
        if (lg->debug) printf("\n### drawing phase %d\n", phase);
        
        ///////////////////////////////////////////////////////////////
        // GRID
        ///////////////////////////////////////////////////////////////
        
        if ((lg->layers & LG_CACHE_GRID and !phase) || (lg->layers & LG_REALTIME_GRID and phase)) {
            // The plugin can set "vertical" to 1
            // to force drawing of vertical lines instead of horizontal ones
            // (which is the default)
            // size and color of the grid (which can be set by the plugin
            // via the context) are reset for every line.
            if (!phase) {
                // we're in cache phase and it seems we really want to
                // draw new grid lines. so "clear" the grid surface
                // with a pure background
                if (lg->debug) printf("copy bg->grid\n");
                calf_line_graph_copy_surface(ctx, lg->background_surface);
                grid_drawn = true;
            }
            for (int a = 0;
                legend = std::string(),
                gdk_cairo_set_source_color(ctx, &grid_color),
                cairo_set_line_width(ctx, grid_width),
                lg->source->get_gridline(lg->source_id, a, phase, pos, vertical, legend, &cimpl);
                a++)
            {
                if (!a and lg->debug) printf("(draw grid)\n");
                calf_line_graph_draw_grid( lg, ctx, legend, vertical, pos );
            }
        }
        if (!phase) {
            // we're in cache phase so we have to switch back to
            // the cache surface after drawing the grid on its surface
            if (lg->debug) printf("switch to cache\n");
            ctx = calf_line_graph_switch_context(lg, _ctx, &cimpl);
            // if a grid was drawn copy it to cache
            if (grid_drawn) {
                if (lg->debug) printf("copy grid->cache\n");
                calf_line_graph_copy_surface(ctx, lg->grid_surface);
                cache_drawn = true;
            }
        }
        
        ///////////////////////////////////////////////////////////////
        // GRAPHS
        ///////////////////////////////////////////////////////////////
        
        if ((lg->layers & LG_CACHE_GRAPH and !phase) || (lg->layers & LG_REALTIME_GRAPH and phase)) {
            // Cycle through all graphs and hand over the amount of horizontal
            // pixels. The plugin is expected to set all corresponding vertical
            // values in an array.
            // size and color of the graph (which can be set by the plugin
            // via the context) are reset for every graph.
        
            if (!phase) {
                // we are drawing the first graph in cache phase, so
                // prepare the cache surface with the grid surface
                if (lg->debug) printf("copy grid->cache\n");
                calf_line_graph_copy_surface(ctx, lg->grid_surface);
                cache_drawn = true;
            } else if (!realtime_drawn) {
                // we're in realtime phase and the realtime surface wasn't
                // reset to cache by now (because there was no cache
                // phase and no realtime grid was drawn)
                // so "clear" the realtime surface with the cache
                if (lg->debug) printf("copy cache->realtime\n");
                calf_line_graph_copy_surface(ctx, lg->cache_surface, lg->force_cache ? 1 : lg->fade);
                realtime_drawn = true;
            }
            
            for(int a = 0;
                lg->mode = 0,
                gdk_cairo_set_source_color(ctx, &graph_color),
                cairo_set_line_width(ctx, graph_width),
                lg->source->get_graph(lg->source_id, a, phase, data, lg->size_x, &cimpl, &lg->mode);
                a++)
            {
                if (lg->debug) printf("graph %d\n", a);
                calf_line_graph_draw_graph( lg, ctx, data, lg->mode );
            }
        }
        
        ///////////////////////////////////////////////////////////////
        // MOVING
        ///////////////////////////////////////////////////////////////
        
        if ((lg->layers & LG_CACHE_MOVING and !phase) || (lg->layers & LG_REALTIME_MOVING and phase)) {
            // we have a moving curve. switch to moving surface and
            // clear it before we start to draw
             if (lg->debug) printf("switch to moving %d\n", lg->movesurf);
            ctx = calf_line_graph_switch_context(lg, moving_c[lg->movesurf], &cimpl);
            calf_line_graph_clear_surface(ctx);
            
            if (!phase and !cache_drawn) {
                // we are drawing the first moving in cache phase and
                // no cache has been created by now, so
                // prepare the cache surface with the grid surface
                if (lg->debug) printf("copy grid->cache\n");
                calf_line_graph_copy_surface(ctx, lg->grid_surface);
                cache_drawn = true;
            } else if (phase and !realtime_drawn) {
                // we're in realtime phase and the realtime surface wasn't
                // reset to cache by now (because there was no cache
                // phase and no realtime grid was drawn)
                // so "clear" the realtime surface with the cache
                if (lg->debug) printf("copy cache->realtime\n");
                calf_line_graph_copy_surface(ctx, lg->cache_surface, lg->force_cache ? 1 : lg->fade);
                realtime_drawn = true;
            }
            
            int a;
            for(a = 0;
                gdk_cairo_set_source_color(ctx, &moving_color),
                lg->source->get_moving(lg->source_id, a, direction, data, lg->size_x, lg->size_y, &cimpl);
                a++)
            {
                if (lg->debug) printf("moving %d\n", a);
                calf_line_graph_draw_moving(lg, ctx, data, direction, a);
            }
            // set moving distances according to direction
            int x = 0;
            int y = 0;
            switch (direction ) {
                case LG_MOVING_LEFT:
                    x = -a;
                    y = 0;
                case LG_MOVING_RIGHT:
                    x = a;
                    y = 0;
                case LG_MOVING_UP:
                    x = 0;
                    y = -a;
                case LG_MOVING_DOWN:
                    x = 0;
                    y = a;
            }
            // copy the old moving surface to the riht position on the
            // new surface
            if (lg->debug) printf("copy cached moving->moving\n");
            cairo_set_source_surface(ctx, lg->moving_surface[(int)!lg->movesurf], x, y);
            cairo_paint(ctx);
            
            // switch back to the actual context
            if (lg->debug) printf("switch to realtime/cache\n");
            ctx = calf_line_graph_switch_context(lg, _ctx, &cimpl);
            
            if (lg->debug) printf("copy moving->realtime/cache\n");
            calf_line_graph_copy_surface(ctx, lg->moving_surface[lg->movesurf], 1);
            
            // toggle the moving cache
            lg->movesurf = (int)!lg->movesurf;
        }
        
        ///////////////////////////////////////////////////////////////
        // DOTS
        ///////////////////////////////////////////////////////////////
        
        if ((lg->layers & LG_CACHE_DOT and !phase) || (lg->layers & LG_REALTIME_DOT and phase)) {
            // Cycle through all dots. The plugin is expected to set the x
            // and y value of the dot.
            // color of the dot (which can be set by the plugin
            // via the context) is reset for every graph.
            
            if (!cache_drawn and !phase) {
                // we are drawing dots in cache phase while
                // the cache wasn't renewed (no graph was drawn), so
                // prepare the cache surface with the grid surface
                if (lg->debug) printf("copy grid->cache\n");
                calf_line_graph_copy_surface(ctx, lg->grid_surface);
                cache_drawn = true;
            }
            if (!realtime_drawn and phase) {
                // we're in realtime phase and the realtime surface wasn't
                // reset to cache by now (because there was no cache
                // phase and no realtime grid or graph was drawn)
                // so "clear" the realtime surface with the cache
                if (lg->debug) printf("copy cache->realtime\n");
                calf_line_graph_copy_surface(ctx, lg->cache_surface, lg->force_cache ? 1 : lg->fade);
                realtime_drawn = true;
            }
            for (int a = 0;
                gdk_cairo_set_source_color(ctx, &dot_color),
                cairo_set_line_width(ctx, dot_width),
                lg->source->get_dot(lg->source_id, a, phase, x, y, size = 3, &cimpl);
                a++)
            {
                if (lg->debug) printf("dot %d\n", a);
                float yv = oy + sy / 2 - (sy / 2 - 1) * y;
                cairo_arc(ctx, ox + x * sx, yv, size, 0, 2 * M_PI);
                cairo_fill(ctx);
            }
        }
        
        if (!phase) {
            // if we have a second cycle for drawing on the realtime
            // after the cache was renewed it's time to copy the
            // cache to the realtime and switch the target surface
            if (lg->debug) printf("switch to realtime\n");
            ctx  = calf_line_graph_switch_context(lg, realtime_c, &cimpl);
            _ctx = realtime_c;
            
            if (cache_drawn) {
                // copy the cache to the realtime if it was changed
                if (lg->debug) printf("copy cache->realtime\n");
                calf_line_graph_copy_surface(ctx, lg->cache_surface, lg->force_cache ? 1 : lg->fade);
                realtime_drawn = true;
            } else if (grid_drawn) {
                // copy the grid to the realtime if it was changed
                if (lg->debug) printf("copy grid->realtime\n");
                calf_line_graph_copy_surface(ctx, lg->grid_surface, lg->force_cache ? 1 : lg->fade);
                realtime_drawn = true;
            }
            
            // check if we can skip the whole realtime phase
            if (!(lg->layers & LG_REALTIME_GRID)
            and !(lg->layers & LG_REALTIME_GRAPH)
            and !(lg->layers & LG_REALTIME_DOT)) {
                 phase = 2;
            }
        }
    } // one or two cycles for drawing cached and non-cached elements
    
    
    // 
    
    
    finalize:
    if (lg->debug) printf("\n### finalize\n");
    
    // whatever happened - we need to copy the realtime surface to the
    // window surface
    //if (lg->debug) printf("switch to window\n");
    //ctx = calf_line_graph_switch_context(lg, c, &cimpl);
    if (lg->debug) printf("copy realtime->window\n");
    calf_line_graph_copy_surface(c, lg->realtime_surface);
    
    // if someone changed the handles via drag'n'drop or externally we
    // need a redraw of the handles surface
    if (lg->freqhandles and (lg->handle_redraw or lg->force_redraw)) {
        cairo_t *hs = cairo_create(lg->handles_surface);
        calf_line_graph_clear_surface(hs);
        calf_line_graph_draw_freqhandles(lg, hs);
        cairo_destroy(hs);
    }
    
    // if we're using frequency handles we need to copy them to the
    // window
    if (lg->freqhandles) {
        if (lg->debug) printf("copy handles->window\n");
        calf_line_graph_copy_surface(c, lg->handles_surface);
    }
    
    // and draw the crosshairs on top if neccessary
    if (lg->use_crosshairs && lg->crosshairs_active && lg->mouse_x > 0
        && lg->mouse_y > 0 && lg->handle_grabbed < 0) {
        std::string s;
        s = lg->source->get_crosshair_label((int)(lg->mouse_x - ox), (int)(lg->mouse_y - oy), sx, sy, &cimpl);
        cairo_set_line_width(c, 1),
        calf_line_graph_draw_crosshairs(lg, c, false, 0, 0.5, 5, false, lg->mouse_x - ox, lg->mouse_y - oy, s);
    }
    
    lg->force_cache       = false;
    lg->force_redraw      = false;
    lg->handle_redraw     = 0;
    lg->recreate_surfaces = 0;
    lg->layers            = 0;
    
    // destroy all temporarily created cairo contexts
    cairo_destroy(c);
    cairo_destroy(realtime_c);
    cairo_destroy(grid_c);
    cairo_destroy(cache_c);
    cairo_destroy(moving_c[0]);
    cairo_destroy(moving_c[1]);
    
    lg->generation += 1;
    
    return TRUE;
}

static int
calf_line_graph_get_handle_at(CalfLineGraph *lg, double x, double y)
{
    int sx = lg->size_x;
    int sy = lg->size_y;
    int ox = lg->pad_x;
    int oy = lg->pad_y;
    
    sx += sx % 2 - 1;
    sy += sy % 2 - 1;

    // loop on all handles
    for (int i = 0; i < lg->freqhandles; i++) {
        FreqHandle *handle = &lg->freq_handles[i];
        if (!handle->is_active())
            continue;

        if (handle->dimensions == 1) {
            // if user clicked inside a vertical band with width HANDLE_WIDTH handle is considered grabbed
            if (lg->mouse_x <= ox + round(handle->value_x * sx + HANDLE_WIDTH / 2.0) + 0.5 &&
                lg->mouse_x >= ox + round(handle->value_x * sx - HANDLE_WIDTH / 2.0) - 0.5 ) {
                return i;
            }
        } else if (handle->dimensions >= 2) {
            double dx = lg->mouse_x - round(ox + handle->value_x * sx);
            double dy = lg->mouse_y - round(oy + handle->value_y * sy);

            // if mouse clicked inside circle of HANDLE_WIDTH
            if (sqrt(dx * dx + dy * dy) <= HANDLE_WIDTH / 2.0)
                return i;
        }
    }
    return -1;
}

static gboolean
calf_line_graph_pointer_motion (GtkWidget *widget, GdkEventMotion *event)
{
    g_assert(CALF_IS_LINE_GRAPH(widget));
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);
    
    int sx = lg->size_x;
    int sy = lg->size_y;
    int ox = lg->pad_x;
    int oy = lg->pad_y;
    
    sx += sx % 2 - 1;
    sy += sy % 2 - 1;
   
    lg->mouse_x = event->x;
    lg->mouse_y = event->y;

    if (lg->handle_grabbed >= 0) {
        FreqHandle *handle = &lg->freq_handles[lg->handle_grabbed];

        float new_x_value = float(event->x - ox) / float(sx);
        float new_y_value = float(event->y - oy) / float(sy);

        if (new_x_value < handle->left_bound) {
            new_x_value = handle->left_bound;
        } else if (new_x_value > handle->right_bound) {
            new_x_value = handle->right_bound;
        }

        // restrict y range by top and bottom
        if (handle->dimensions >= 2) {
            if(new_y_value < 0.0) new_y_value = 0.0;
            if(new_y_value > 1.0) new_y_value = 1.0;
        }

        if (new_x_value != handle->value_x ||
            new_y_value != handle->value_y) {
            handle->value_x = new_x_value;
            handle->value_y = new_y_value;

            g_signal_emit_by_name(widget, "freqhandle-changed", handle);
        }
        lg->handle_redraw = 1;
        calf_line_graph_expose_request(widget, true);
    }
    if (event->is_hint)
    {
        gdk_event_request_motions(event);
    }

    int handle_hovered = calf_line_graph_get_handle_at(lg, event->x, event->y);
    if (handle_hovered != lg->handle_hovered) {
        if (lg->handle_grabbed >= 0 || 
            handle_hovered != -1) {
            gdk_window_set_cursor(widget->window, lg->hand_cursor);
            lg->handle_hovered = handle_hovered;
        } else {
            gdk_window_set_cursor(widget->window, lg->arrow_cursor);
            lg->handle_hovered = -1;
        }
        lg->handle_redraw = 1;
        calf_line_graph_expose_request(widget, true);
    }
    if(lg->crosshairs_active and lg->use_crosshairs) {
        calf_line_graph_expose_request(widget, true);
    }
    return TRUE;
}

static gboolean
calf_line_graph_button_press (GtkWidget *widget, GdkEventButton *event)
{
    g_assert(CALF_IS_LINE_GRAPH(widget));
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);
    bool inside_handle = false;

    int i = calf_line_graph_get_handle_at(lg, lg->mouse_x, lg->mouse_y);
    if (i != -1)
    {
        FreqHandle *handle = &lg->freq_handles[i];

        if (handle->dimensions == 1) {
            // if user clicked inside a vertical band with width HANDLE_WIDTH handle is considered grabbed
            lg->handle_grabbed = i;
            inside_handle = true;

            if (lg->enforce_handle_order) {
                // look for previous one dimensional handle to find left_bound
                for (int j = i - 1; j >= 0; j--) {
                    FreqHandle *prevhandle = &lg->freq_handles[j];
                    if(prevhandle->is_active() && prevhandle->dimensions == 1) {
                        handle->left_bound = prevhandle->value_x + lg->min_handle_distance;
                        break;
                    }
                }

                // look for next one dimensional handle to find right_bound
                for (int j = i + 1; j < lg->freqhandles; j++) {
                    FreqHandle *nexthandle = &lg->freq_handles[j];
                    if(nexthandle->is_active() && nexthandle->dimensions == 1) {
                        handle->right_bound = nexthandle->value_x - lg->min_handle_distance;
                        break;
                    }
                }
            }
        } else if (handle->dimensions >= 2) {
            lg->handle_grabbed = i;
            inside_handle = true;
        }
    }

    if (inside_handle && event->type == GDK_2BUTTON_PRESS) {
        FreqHandle &handle = lg->freq_handles[lg->handle_grabbed];
        handle.value_x = handle.default_value_x;
        handle.value_y = handle.default_value_y;
        g_signal_emit_by_name(widget, "freqhandle-changed", &handle);
    }

    if(!inside_handle) {
        lg->crosshairs_active = !lg->crosshairs_active;
    }
    
    calf_line_graph_expose_request(widget, true);
    gtk_widget_grab_focus(widget);
    gtk_grab_add(widget);
    
    return TRUE;
}

static gboolean
calf_line_graph_button_release (GtkWidget *widget, GdkEventButton *event)
{
    g_assert(CALF_IS_LINE_GRAPH(widget));
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);

    lg->handle_grabbed = -1;

    if (GTK_WIDGET_HAS_GRAB(widget))
        gtk_grab_remove(widget);
        
    calf_line_graph_expose_request(widget, true);
    return TRUE;
}

static gboolean
calf_line_graph_scroll (GtkWidget *widget, GdkEventScroll *event)
{
    g_assert(CALF_IS_LINE_GRAPH(widget));
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);

    int i = calf_line_graph_get_handle_at(lg, lg->mouse_x, lg->mouse_y);
    if (i != -1)
    {
        FreqHandle *handle = &lg->freq_handles[i];
        if (handle->dimensions == 3) {
            if (event->direction == GDK_SCROLL_UP) {
                handle->value_z += 0.05;
                if(handle->value_z > 1.0) {
                    handle->value_z = 1.0;
                }
                g_signal_emit_by_name(widget, "freqhandle-changed", handle);
            } else if (event->direction == GDK_SCROLL_DOWN) {
                handle->value_z -= 0.05;
                if(handle->value_z < 0.0) {
                    handle->value_z = 0.0;
                }
                g_signal_emit_by_name(widget, "freqhandle-changed", handle);
            }
            lg->handle_redraw = 1;
        }
    }
    return TRUE;
}

static gboolean
calf_line_graph_enter (GtkWidget *widget, GdkEventCrossing *event)
{
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);
    
    if (lg->debug) printf("[enter]\n");
    return TRUE;
}

static gboolean
calf_line_graph_leave (GtkWidget *widget, GdkEventCrossing *event)
{
    
    g_assert(CALF_IS_LINE_GRAPH(widget));
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);
    
    if (lg->debug) printf("[leave]\n");
    if (lg->mouse_x >= 0 or lg->mouse_y >= 0)
        calf_line_graph_expose_request(widget, true);
    lg->mouse_x = -1;
    lg->mouse_y = -1;

    return TRUE;
}


void calf_line_graph_set_square(CalfLineGraph *graph, bool is_square)
{
    g_assert(CALF_IS_LINE_GRAPH(graph));
    graph->is_square = is_square;
}

static void
calf_line_graph_size_request (GtkWidget *widget,
                           GtkRequisition *requisition)
{
    g_assert(CALF_IS_LINE_GRAPH(widget));
    
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);
    
    if (lg->debug) printf("size request\n");
}

static void
calf_line_graph_size_allocate (GtkWidget *widget,
                           GtkAllocation *allocation)
{
    g_assert(CALF_IS_LINE_GRAPH(widget));
    CalfLineGraph *lg = CALF_LINE_GRAPH(widget);
    
    if (lg->debug) printf("size allocation\n");
    
    GtkWidgetClass *parent_class = (GtkWidgetClass *) g_type_class_peek_parent( CALF_LINE_GRAPH_GET_CLASS( lg ) );
    
    // remember the allocation
    widget->allocation = *allocation;
    
    // reset the allocation if a square widget is requested
    GtkAllocation &a = widget->allocation;
    if (lg->is_square)
    {
        if (a.width > a.height)
        {
            a.x += (a.width - a.height) / 2;
            a.width = a.height;
        }
        if (a.width < a.height)
        {
            a.y += (a.height - a.width) / 2;
            a.height = a.width;
        }
    }
    
    lg->size_x = a.width  - lg->pad_x * 2;
    lg->size_y = a.height - lg->pad_y * 2;
    
    lg->recreate_surfaces = 1;
    parent_class->size_allocate( widget, &a );
}


static void
calf_line_graph_class_init (CalfLineGraphClass *klass)
{
    // GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->expose_event = calf_line_graph_expose;
    widget_class->size_request = calf_line_graph_size_request;
    widget_class->size_allocate = calf_line_graph_size_allocate;
    widget_class->button_press_event = calf_line_graph_button_press;
    widget_class->button_release_event = calf_line_graph_button_release;
    widget_class->motion_notify_event = calf_line_graph_pointer_motion;
    widget_class->scroll_event = calf_line_graph_scroll;
    widget_class->enter_notify_event = calf_line_graph_enter;
    widget_class->leave_notify_event = calf_line_graph_leave;

    g_signal_new("freqhandle-changed",
         G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
         0, NULL, NULL,
         g_cclosure_marshal_VOID__POINTER,
         G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
calf_line_graph_unrealize (GtkWidget *widget, CalfLineGraph *lg)
{
    if (lg->debug) printf("unrealize\n");
    calf_line_graph_destroy_surfaces(widget);
}

static void
calf_line_graph_init (CalfLineGraph *lg)
{
    GtkWidget *widget = GTK_WIDGET(lg);
    
    if (lg->debug) printf("lg init\n");
    
    GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS | GTK_SENSITIVE | GTK_PARENT_SENSITIVE);
    gtk_widget_add_events(widget, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    
    widget->requisition.width  = 40;
    widget->requisition.height = 40;
    lg->force_cache          = true;
    lg->force_redraw         = false;
    lg->zoom                 = 1;
    lg->param_zoom           = -1;
    lg->recreate_surfaces    = 1;
    lg->mode                 = 0;
    lg->movesurf             = 0;
    lg->generation           = 0;
    lg->arrow_cursor         = gdk_cursor_new(GDK_RIGHT_PTR);
    lg->hand_cursor          = gdk_cursor_new(GDK_FLEUR);
    lg->layers               = LG_CACHE_GRID    | LG_CACHE_GRAPH
                             | LG_CACHE_DOT     | LG_CACHE_MOVING
                             | LG_REALTIME_GRID | LG_REALTIME_GRAPH
                             | LG_REALTIME_DOT  | LG_REALTIME_MOVING;
    
    g_signal_connect(GTK_OBJECT(widget), "unrealize", G_CALLBACK(calf_line_graph_unrealize), (gpointer)lg);
    
    for(int i = 0; i < FREQ_HANDLES; i++) {
        FreqHandle *handle      = &lg->freq_handles[i];
        handle->active          = false;
        handle->param_active_no = -1;
        handle->param_x_no      = -1;
        handle->param_y_no      = -1;
        handle->value_x         = -1.0;
        handle->value_y         = -1.0;
        handle->param_x_no      = -1;
        handle->label           = NULL;
        handle->left_bound      = 0.0 + lg->min_handle_distance;
        handle->right_bound     = 1.0 - lg->min_handle_distance;
    }

    lg->handle_grabbed      = -1;
    lg->handle_hovered      = -1;
    lg->handle_redraw       = 1;
    lg->min_handle_distance = 0.025;
    
    lg->background_surface = NULL;
    lg->grid_surface       = NULL;
    lg->cache_surface      = NULL;
    lg->moving_surface[0]  = NULL;
    lg->moving_surface[1]  = NULL;
    lg->handles_surface    = NULL;
    lg->realtime_surface   = NULL;
}

GtkWidget *
calf_line_graph_new()
{
    return GTK_WIDGET( g_object_new (CALF_TYPE_LINE_GRAPH, NULL ));
}

GType
calf_line_graph_get_type (void)
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo type_info = {
            sizeof(CalfLineGraphClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_line_graph_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfLineGraph),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_line_graph_init
        };

        GTypeInfo *type_info_copy = new GTypeInfo(type_info);

        for (int i = 0; ; i++) {
            char *name = g_strdup_printf("CalfLineGraph%u%d", ((unsigned int)(intptr_t)calf_line_graph_class_init) >> 16, i);
            if (g_type_from_name(name)) {
                free(name);
                continue;
            }
            type = g_type_register_static( GTK_TYPE_DRAWING_AREA,
                                           name,
                                           type_info_copy,
                                           (GTypeFlags)0);
            free(name);
            break;
        }
    }
    return type;
}