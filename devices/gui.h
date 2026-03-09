#ifndef DEVICES_GUI_H
#define DEVICES_GUI_H

#include <stdint.h>
#include <stdbool.h>
#include <list.h>

#define BAR_HEIGHT 40
#define BAR_Y (768 - BAR_HEIGHT)
#define START_X 0
#define START_W 80
#define WINDOW_LIST_X 115
#define WINDOW_LIST_W (1024 - WINDOW_LIST_X - 10)

struct window {
    int x, y, w, h;
    char title[32];
    uint16_t color;
    bool is_dragging;
    
    int cursor_x;
    int cursor_y;
    char content[2048];
    int content_pos;

    uint16_t *canvas;
    bool has_canvas_data;
    
    struct list_elem elem;
};

void wm_init(void);
void gui_init(void);
void gui_draw_taskbar(void);
void gui_draw_button(int x, int y, int w, int h, const char* label, uint16_t color);
void gui_thread_func(void *aux);
void gui_draw_background(void);

struct window* wm_create_window(int x, int y, int w, int h, const char* title, uint16_t color);
struct window* wm_find_at(int mx, int my);
void wm_draw_all(void);
void wm_close_window(struct window *win);
void wm_write_to_window(struct window *w, const char *buf, unsigned size);
void wm_draw_pixel_block(struct window *w, uint32_t *pixels, int width, int height);

uint16_t vesa_convert_color(uint32_t rgba);

extern struct list window_list; 

#endif