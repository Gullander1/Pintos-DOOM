#ifndef DEVICES_VGA_H
#define DEVICES_VGA_H

#include <stdint.h>
#include <stdbool.h>

void vga_putc (int c);

void vesa_init(void);
void vesa_putpixel(int x, int y, uint16_t color);
void vesa_clear_screen(uint16_t color);
void vesa_draw_rect(int x, int y, int w, int h, uint16_t color);
void vesa_draw_char(int x, int y, char c, uint16_t color);
void vesa_draw_string(int x, int y, const char *str, uint16_t color);
void vesa_draw_window_frame(int x, int y, int w, int h);
uint16_t vesa_convert_color(uint32_t rgba);

void vesa_update_display(int mouse_x, int mouse_y);
void vesa_draw_cursor(int x, int y);
void vesa_move_cursor(int old_x, int old_y, int new_x, int new_y);
void vesa_copy_region(int x, int y, int w, int h);

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

#endif /* devices/vga.h */