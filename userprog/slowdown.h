#ifndef USERPROG_SLOWDOWN_H
#define USERPROG_SLOWDOWN_H

#include <stdbool.h>

void slowdown_init(void);
void slowdown_enable(void);
void slowdown_disable(void);
bool slowdown_enabled(void);

#endif
