#ifndef DEVICES_MOUSE_H
#define DEVICES_MOUSE_H

#include <stdbool.h>

void mouse_init(void);
int mouse_get_x(void);
int mouse_get_y(void);

bool mouse_get_button(int button);

#endif