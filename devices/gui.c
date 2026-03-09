#include "devices/gui.h"
#include "devices/vga.h"
#include "devices/mouse.h" 
#include "devices/rtc.h"
#include "threads/malloc.h" 
#include "threads/thread.h" 
#include <string.h>   
#include <stdio.h>
#include <stdint.h>

struct list window_list;

static int current_mx = 512;
static int current_my = 384;

static int win_cursor_x = 5;
static int win_cursor_y = 25;

/* Help functions */
uint16_t vesa_convert_color(uint32_t rgba) {
    uint8_t r = (rgba >> 16) & 0xFF;
    uint8_t g = (rgba >> 8) & 0xFF;
    uint8_t b = (rgba) & 0xFF;

    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/* Init. functions */
void wm_init(void) {
    list_init(&window_list);
}

void gui_init(void) {
    vesa_clear_screen(0x05D6); 
    gui_draw_taskbar();
    vesa_update_display(512, 384);
}

/* Fundamental GUI functions */
void gui_draw_background(void) {
    vesa_clear_screen(0x05D6); 

}

void gui_draw_taskbar(void) {
    vesa_draw_rect(0, BAR_Y, SCREEN_WIDTH, BAR_HEIGHT, 0xC618);
    for(int i=0; i<SCREEN_WIDTH; i++) vesa_putpixel(i, BAR_Y, 0xFFFF);

    gui_draw_button(START_X + 5, BAR_Y + 5, START_W, 30, "Pintos", 0x05D6);

    vesa_draw_rect(WINDOW_LIST_X, BAR_Y + 5, WINDOW_LIST_W, 30, 0x8410);
    vesa_draw_string(WINDOW_LIST_X + 10, BAR_Y + 15, "Modified PINTOS OS for DOOM", 0xBDF7);
}

void gui_draw_button(int x, int y, int w, int h, const char* label, uint16_t color) {
    uint16_t light = 0xDEFB;
    uint16_t dark = 0x4208;
    
    vesa_draw_rect(x, y, w, h, color);
    for(int i=0; i<w; i++) { vesa_putpixel(x+i, y, light); vesa_putpixel(x+i, y+h-1, dark); }
    for(int i=0; i<h; i++) { vesa_putpixel(x, y+i, light); vesa_putpixel(x+w-1, y+i, dark); }
    
    vesa_draw_string(x + (w/2) - 25, y + (h/2) - 4, label, 0xFFFF);
}

/* WM */
struct window* wm_create_window(int x, int y, int w, int h, const char* title, uint16_t color) {
    struct window *win = malloc(sizeof(struct window));
    if (!win) return NULL;

    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->color = color;
    win->is_dragging = false;
    strlcpy(win->title, title, sizeof win->title);

    int inner_w = w - 4;
    int inner_h = h - 22;
    win->canvas = malloc(inner_w * inner_h * sizeof(uint16_t));
    win->has_canvas_data = false;

    win->content_pos = 0;
    win->cursor_x = 5;
    win->cursor_y = 25;
    memset(win->content, 0, sizeof(win->content));

    list_push_back(&window_list, &win->elem);

    wm_draw_all();
    vesa_update_display(current_mx, current_my);

    return win;
}

struct window* wm_find_at(int mx, int my) {
    struct list_elem *e;
    for (e = list_rbegin(&window_list); e != list_rend(&window_list); e = list_prev(e)) {
        struct window *w = list_entry(e, struct window, elem);
        if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h)
            return w;
    }
    return NULL;
}


void wm_close_window(struct window *win) {
    if (win == NULL) return;

    list_remove(&win->elem);

    if (win->canvas != NULL) {
        free(win->canvas);
    }

    free(win);
}

/* Window drawing + data */
void wm_draw_all(void) {
    gui_draw_background();

    struct list_elem *e;
    for (e = list_begin(&window_list); e != list_end(&window_list); e = list_next(e)) {
        struct window *w = list_entry(e, struct window, elem);
        
        vesa_draw_window_frame(w->x, w->y, w->w, w->h);
        vesa_draw_rect(w->x + 2, w->y + 2, w->w - 4, 18, 0x0010);
        vesa_draw_string(w->x + 10, w->y + 5, w->title, 0xFFFF);

        int close_x = w->x + w->w - 20;
        int close_y = w->y + 2;
        vesa_draw_rect(close_x, close_y, 18, 18, 0xF800);
        vesa_draw_string(close_x + 5, close_y + 5, "X", 0xFFFF);

        if (w->has_canvas_data && w->canvas != NULL) {
            int inner_w = w->w - 4;
            int inner_h = w->h - 22;
            
            for (int y = 0; y < inner_h; y++) {
                for (int x = 0; x < inner_w; x++) {
                    vesa_putpixel(w->x + 2 + x, w->y + 20 + y, w->canvas[y * inner_w + x]);
                }
            }
        } 
        else {
            uint16_t bg_color = (strcmp(w->title, "Console") == 0) ? 0x0000 : 0x8410;
            vesa_draw_rect(w->x + 2, w->y + 20, w->w - 4, w->h - 22, bg_color);

            int tx = 5; int ty = 25;
            for (int i = 0; i < w->content_pos; i++) {
                if (w->content[i] == '\n') { tx = 5; ty += 12; }
                else {
                    vesa_draw_char(w->x + tx, w->y + ty, w->content[i], 0x07E0);
                    tx += 8;
                }
                if (tx > w->w - 15) { tx = 5; ty += 12; }
            }
        }
    }
    gui_draw_taskbar();
}

void wm_draw_pixel_block(struct window *w, uint32_t *pixels, int width, int height) {
    if (w == NULL || pixels == NULL) return;

    int inner_w = w->w - 4;
    int inner_h = w->h - 22;

    bool is_top_window = false;
    if (!list_empty(&window_list) && list_back(&window_list) == &w->elem) {
        is_top_window = true;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int screen_x = w->x + x + 2;
            int screen_y = w->y + y + 20;

            if (x < inner_w && y < inner_h) {
                uint32_t p = pixels[y * width + x];
                uint16_t color = vesa_convert_color(p);

                if (w->canvas != NULL) {
                    w->canvas[y * inner_w + x] = color;
                    w->has_canvas_data = true;
                }

                if (!w->is_dragging && is_top_window) {
                    if (screen_x < SCREEN_WIDTH && screen_y < SCREEN_HEIGHT) {
                        vesa_putpixel(screen_x, screen_y, color);
                    }
                }
            }
        }
    }
    
    if (!w->is_dragging && is_top_window) {
        vesa_copy_region(w->x, w->y, w->w, w->h);

        if (current_mx + 13 >= w->x && current_mx <= w->x + w->w &&
            current_my + 19 >= w->y && current_my <= w->y + w->h) {
            vesa_draw_cursor(current_mx, current_my);
        }
    }
}

void wm_write_to_window(struct window *w, const char *buf, unsigned size) {
    if (w == NULL) return;

    for (unsigned i = 0; i < size; i++) {
        char c = buf[i];

        if (w->content_pos < (int)sizeof(w->content) - 1) {
            w->content[w->content_pos++] = c;
        }

        if (c == '\n') {
            win_cursor_x = 5;
            win_cursor_y += 12;
        } else {
            vesa_draw_char(w->x + win_cursor_x, w->y + win_cursor_y, c, 0xFFFF);
            win_cursor_x += 8;
        }

        if (win_cursor_x > w->w - 15) {
            win_cursor_x = 5;
            win_cursor_y += 12;
        }

        if (win_cursor_y > w->h - 15) {
            win_cursor_y = 25; 
        }
    }
    vesa_update_display(current_mx, current_my);
}

/* Head loop */
void gui_thread_func(void *aux UNUSED) {
    int old_mx = 512, old_my = 384;
    struct window *drag_win = NULL;

    while (1) {
        int mx = mouse_get_x();
        int my = mouse_get_y();
        bool l_click = mouse_get_button(0);

        current_mx = mx;
        current_my = my;

        bool mouse_moved = (mx != old_mx || my != old_my);
        bool needs_full_redraw = false;

        if (l_click && !drag_win) {
            struct window *hit = wm_find_at(mx, my);
            if (hit) {
                int close_x = hit->x + hit->w - 20;
                int close_y = hit->y + 2;
                
                if (mx >= close_x && mx <= close_x + 18 && 
                    my >= close_y && my <= close_y + 18) {
                    
                    wm_close_window(hit);
                    needs_full_redraw = true;
                } 
                else {
                    list_remove(&hit->elem);
                    list_push_back(&window_list, &hit->elem);
                    
                    if (my < hit->y + 20) {
                        drag_win = hit;
                        drag_win->is_dragging = true;
                    }
                    needs_full_redraw = true; 
                }
            }
        } else if (!l_click) {
            if (drag_win != NULL) {
                drag_win->is_dragging = false;
                drag_win = NULL;
            }
        }

        if (drag_win && mouse_moved) {
            drag_win->x += (mx - old_mx);
            drag_win->y += (my - old_my);
            needs_full_redraw = true;
        }

        if (needs_full_redraw) {
            wm_draw_all();
            vesa_update_display(mx, my); 
        } 
        else if (mouse_moved) {
            vesa_move_cursor(old_mx, old_my, mx, my);
        }

        old_mx = mx; 
        old_my = my;
        
        thread_yield(); 
    }
}