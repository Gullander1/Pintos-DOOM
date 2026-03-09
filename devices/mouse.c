#include "devices/mouse.h"
#include "threads/interrupt.h"
#include "threads/io.h"
#include "devices/vga.h"
#include <stdio.h>

/* Ports */
#define MOUSE_DATA_PORT 0x60
#define MOUSE_STATUS_PORT 0x64
#define MOUSE_COMMAND_PORT 0x64

static intr_handler_func mouse_interrupt;
static int mouse_x = 512, mouse_y = 384;
static uint8_t mouse_buttons = 0;

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout-- && (inb(MOUSE_STATUS_PORT) & 1) == 0);
    } else {
        while (timeout-- && (inb(MOUSE_STATUS_PORT) & 2));
    }
}

/* Write to mouse */
static void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0xD4);
    mouse_wait(1);
    outb(MOUSE_DATA_PORT, data);
}

void mouse_init(void) {
    intr_register_ext(0x2c, mouse_interrupt, "PS/2 Mouse");

    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0xA8);

    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0x20);
    mouse_wait(0);
    uint8_t status = (inb(MOUSE_DATA_PORT) | 2);
    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0x60);
    mouse_wait(1);
    outb(MOUSE_DATA_PORT, status);

    mouse_write(0xF4);
    mouse_wait(0);
    inb(MOUSE_DATA_PORT);
}

static void mouse_interrupt(struct intr_frame *f UNUSED) {
    static uint8_t packet[3];
    static int byte_count = 0;

    uint8_t input = inb(MOUSE_DATA_PORT);
    if (byte_count == 0 && !(input & 0x08)) return; 

    packet[byte_count++] = input;

    if (byte_count == 3) {
        byte_count = 0;

        mouse_buttons = packet[0] & 0x07;

        int32_t dx = packet[1];
        int32_t dy = packet[2];

        if (packet[0] & 0x10) dx |= 0xFFFFFF00;
        if (packet[0] & 0x20) dy |= 0xFFFFFF00;

        mouse_x += dx;
        mouse_y -= dy; 

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > 1010) mouse_x = 1010;
        if (mouse_y > 750) mouse_y = 750;
    }
}

bool mouse_get_button(int button) {
    return (mouse_buttons & (1 << button)) != 0;
}

int mouse_get_x(void) { return mouse_x; }
int mouse_get_y(void) { return mouse_y; }