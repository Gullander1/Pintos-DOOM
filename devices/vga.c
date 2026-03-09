#include "devices/vga.h"
#include "devices/font.h"

#include "devices/speaker.h"
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"  
#include "threads/pte.h"     
#include <stdio.h>

#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* VGA text screen support.  See [FREEVGA] for more information. */

/* Number of columns and rows on the text display. */
#define COL_CNT 80
#define ROW_CNT 25

/* Current cursor position.  (0,0) is in the upper left corner of
  the display. */
static size_t cx, cy;

/* Attribute value for gray text on a black background. */
#define GRAY_ON_BLACK 0x07

/* Framebuffer.  See [FREEVGA] under "VGA Text Mode Operation".
  The character at (x,y) is fb[y][x][0].
  The attribute at (x,y) is fb[y][x][1]. */
static uint8_t (*fb)[COL_CNT][2];

/* BACK BUFFER AND MOUSE BUFFER */
static uint16_t *back_buffer;
static uint16_t mouse_backup[19 * 13];

static void clear_row(size_t y);
static void cls(void);
static void newline(void);
static void move_cursor(void);
static void find_cursor(size_t *x, size_t *y);

/* For window graphics */
static uint16_t *vesa_fb; 
extern uint32_t *init_page_dir; 

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

static const char mouse_cursor[19][13] = {
    "X           ",
    "XX          ",
    "X.X         ",
    "X..X        ",
    "X...X       ",
    "X....X      ",
    "X.....X     ",
    "X......X    ",
    "X.......X   ",
    "X........X  ",
    "X.....XXXXXX",
    "X..X..X     ",
    "X.X X..X    ",
    "XX   X..X   ",
    "X     XX    ",
    "            ",
    "            ",
    "            ",
    "            "
};

/* Initializes the VGA text display. */
static void init(void)
{
	/* Already initialized? */
	static bool inited;
	if (!inited) {
		fb = ptov(0xb8000);
		find_cursor(&cx, &cy);
		inited = true;
	}
}

/* Writes C to the VGA text display, interpreting control
  characters in the conventional ways.  */
void vga_putc(int c)
{
	/* Disable interrupts to lock out interrupt handlers
	  that might write to the console. */
	enum intr_level old_level = intr_disable();

	init();

	switch (c) {
		case '\n':
			newline();
			break;

		case '\f':
			cls();
			break;

		case '\b':
			if (cx > 0) {
				cx--;
			}
			break;

		case '\r':
			cx = 0;
			break;

		case '\t':
			cx = ROUND_UP(cx + 1, 8);
			if (cx >= COL_CNT) {
				newline();
			}
			break;

		case '\a':
			intr_set_level(old_level);
			speaker_beep();
			intr_disable();
			break;

		default:
			fb[cy][cx][0] = c;
			fb[cy][cx][1] = GRAY_ON_BLACK;
			if (++cx >= COL_CNT) {
				newline();
			}
			break;
	}

	/* Update cursor position. */
	move_cursor();

	intr_set_level(old_level);
}

/* Clears the screen and moves the cursor to the upper left. */
static void cls(void)
{
	size_t y;

	for (y = 0; y < ROW_CNT; y++) {
		clear_row(y);
	}

	cx = cy = 0;
	move_cursor();
}

/* Clears row Y to spaces. */
static void clear_row(size_t y)
{
	size_t x;

	for (x = 0; x < COL_CNT; x++) {
		fb[y][x][0] = ' ';
		fb[y][x][1] = GRAY_ON_BLACK;
	}
}

/* Advances the cursor to the first column in the next line on
  the screen.  If the cursor is already on the last line on the
  screen, scrolls the screen upward one line. */
static void newline(void)
{
	cx = 0;
	cy++;
	if (cy >= ROW_CNT) {
		cy = ROW_CNT - 1;
		memmove(&fb[0], &fb[1], sizeof fb[0] * (ROW_CNT - 1));
		clear_row(ROW_CNT - 1);
	}
}

/* Moves the hardware cursor to (cx,cy). */
static void move_cursor(void)
{
	/* See [FREEVGA] under "Manipulating the Text-mode Cursor". */
	uint16_t cp = cx + COL_CNT * cy;
	outw(0x3d4, 0x0e | (cp & 0xff00));
	outw(0x3d4, 0x0f | (cp << 8));
}

/* Reads the current hardware cursor position into (*X,*Y). */
static void find_cursor(size_t *x, size_t *y)
{
	/* See [FREEVGA] under "Manipulating the Text-mode Cursor". */
	uint16_t cp;

	outb(0x3d4, 0x0e);
	cp = inb(0x3d5) << 8;

	outb(0x3d4, 0x0f);
	cp |= inb(0x3d5);

	*x = cp % COL_CNT;
	*y = cp / COL_CNT;
}

/* --- VESA Graphics Driver --- */

/* Start the grpahicscard and intitate */
void vesa_init(void) 
{
    /* Ask PCI-buss where the graphics card is (Bus 0, Dev 2, Reg 0x10) */
    outl(0xCF8, 0x80001010); 
    uint32_t pci_bar0 = inl(0xCFC);
    uint32_t phys_addr = pci_bar0 & 0xFFFFFFF0;

	back_buffer = palloc_get_multiple(PAL_ASSERT, (1024 * 768 * 2) / 4096 + 1);

    if (phys_addr != 0) 
    {
        uint32_t *pt = palloc_get_page(PAL_ASSERT | PAL_ZERO);
        size_t pde_idx = phys_addr >> 22;
        
        init_page_dir[pde_idx] = vtop(pt) | 0x03;

        for (int i = 0; i < 1024; i++) {
            pt[i] = (phys_addr + i * 4096) | 0x03;
        }

        asm volatile("movl %0, %%cr3" : : "r"(vtop(init_page_dir)));
        
        vesa_fb = (uint16_t *) phys_addr;
        printf("VESA Driver: OK (Address 0x%08X)\n", phys_addr);
    } 
    else 
    {
        printf("VESA Driver: FAILED TO FIND DEVICE!\n");
    }
}

/* ----- FOR GRAPHICS NOT IN CONSOLE ----- */
/* Draw a single pixel on the screen */
void vesa_putpixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        back_buffer[y * SCREEN_WIDTH + x] = color;
    }
}

/* Clear the screen and put only one color */
void vesa_clear_screen(uint16_t color) 
{
    if (back_buffer == NULL) return;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        back_buffer[i] = color;
    }
}

/* Draw a filled rectangle */
void vesa_draw_rect(int x, int y, int w, int h, uint16_t color) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            vesa_putpixel(j, i, color);
        }
    }
}

/* Draw a window frame */
void vesa_draw_window_frame(int x, int y, int w, int h) {
    uint16_t light = 0xDEFB;
    uint16_t dark = 0x4208;
    uint16_t mid = 0x8410;

    // The background
    vesa_draw_rect(x, y, w, h, mid);

    for(int i = 0; i < w; i++) vesa_putpixel(x + i, y, light);
    for(int i = 0; i < h; i++) vesa_putpixel(x, y + i, light);

    for(int i = 0; i < w; i++) vesa_putpixel(x + i, y + h - 1, dark);
    for(int i = 0; i < h; i++) vesa_putpixel(x + w - 1, y + i, dark);
}

/* Draw a single characther on the screen */
void vesa_draw_char(int x, int y, char c, uint16_t color) {
    if ((unsigned char) c > 127) return;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (font8x8_basic[(int)c][row] & (0x80 >> col)) {
                vesa_putpixel(x + col, y + row, color);
            }
        }
    }
}

/* Draw a whole string */
void vesa_draw_string(int x, int y, const char *str, uint16_t color) {
    while (*str) {
        vesa_draw_char(x, y, *str, color);
        x += 8;
        str++;
    }
}

/* Update the display */
void vesa_update_display(int mouse_x, int mouse_y) {
    if (vesa_fb != NULL && back_buffer != NULL) {
        memcpy(vesa_fb, back_buffer, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
        
        vesa_draw_cursor(mouse_x, mouse_y);
    }
}

/* Draw the cursor */
void vesa_draw_cursor(int x, int y) {
    if (vesa_fb == NULL) return;

    for (int row = 0; row < 19; row++) { 
        for (int col = 0; col < 13; col++) {
            int sx = x + col;
            int sy = y + row;

            if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
                char pixel_type = mouse_cursor[row][col];
                if (pixel_type == 'X')
                    vesa_fb[sy * SCREEN_WIDTH + sx] = 0x0000;
                else if (pixel_type == '.')
                    vesa_fb[sy * SCREEN_WIDTH + sx] = 0xFFFF;
            }
        }
    }
}

void vesa_move_cursor(int old_x, int old_y, int new_x, int new_y) {
    vesa_copy_region(old_x, old_y, 13, 19);

    vesa_draw_cursor(new_x, new_y);
}

void vesa_copy_region(int x, int y, int w, int h) {
    if (vesa_fb == NULL || back_buffer == NULL) return;

    int start_x = (x < 0) ? 0 : x;
    int start_y = (y < 0) ? 0 : y;
    
    int end_x = (x + w > SCREEN_WIDTH) ? SCREEN_WIDTH : x + w;
    int end_y = (y + h > SCREEN_HEIGHT) ? SCREEN_HEIGHT : y + h;

    int copy_width = end_x - start_x;
    
    if (copy_width <= 0) return;

    for (int i = start_y; i < end_y; i++) {
        int offset = i * SCREEN_WIDTH + start_x;
        memcpy(&vesa_fb[offset], &back_buffer[offset], copy_width * 2); 
    }
}