/* DOOM ON PINTOS (2026) KEVIN GULLANDER*/
#include <syscall.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#define _SKIP_MKDIR_FIX 
#include "doomtype.h"
#include "doomgeneric.h"
#include "m_fixed.h"
#include "doomdef.h"  
#include "d_event.h"
#include "d_main.h"
#include "i_system.h"

/* STATIC MEMORY ALLOCATION (PINTOS HEAP)
   Since Pintos often lacks a robust dynamic allocator, we define a 
   static 40MB array to act as the game's heap. */
#define DOOM_HEAP_SIZE (40 * 1024 * 1024) 
static uint8_t doom_heap[DOOM_HEAP_SIZE] __attribute__ ((aligned (16)));
static size_t heap_pointer = 0;

/* DATA TYPES */
struct _wad_file { int fd; uint8_t *mapped; int length; };
typedef struct _wad_file wad_file_t;

/* GLOBAL STATE & BUFFERS
   Internal Doom resolution is 320x200. These buffers hold the 
   raw frame and the scaled 640x400 output for Pintos. */
int net_client_connected = 0;
int drone = 0;
int snd_musicdevice = 0;

static uint32_t doom_internal_buffer[1024 * 768]; 
static uint32_t pintos_scale_buffer[1024 * 768];
static uint32_t internal_ticks = 0;

#define KEY_TIMEOUT 4
static int key_timers[256] = {0};

void draw_frame(void* buffer, int width, int height);
uint32_t sys_time(void);

wad_file_t *W_OpenFile(const char *name) {

    int fd = open(name);
    if (fd < 0) {
        printf("!!! CRITICAL: FAILED TO OPEN WAD FILE: %s !!!\n", name);
        return NULL;
    }
    
    int len = filesize(fd);
    wad_file_t *wf = malloc(sizeof(wad_file_t));
    if (!wf) return NULL;

    wf->fd = fd; 
    wf->length = len;
    wf->mapped = NULL; 
    
    printf("WAD OPENED: %s (%d bytes). Using on-demand disk I/O.\n", name, len);

    return wf;
}

/* PINTOS SYSCALL WRAPPERS (HAL)
   These functions bridge the C code to the Pintos kernel using inline 
   assembly. Ensure your kernel has syscalls 21, 22, and 23 implemented. */
uint32_t sys_time(void) {
    uint32_t ticks_out;
    asm volatile ("pushl $22; int $0x30; addl $4, %%esp;" : "=a" (ticks_out));
    return ticks_out;
}

int sys_getc(void) {
    int c;
    asm volatile (
        "pushl $23; "          
        "int $0x30; "          
        "addl $4, %%esp; "     
        : "=a" (c)             
        : 
        : "memory"
    );
    return c;
}

void draw_frame(void* buffer, int width, int height) {
    asm volatile ("pushl %%ebx; pushl %0; pushl %1; pushl %2; pushl $21; int $0x30; addl $16, %%esp; popl %%ebx;" 
        : : "r" (height), "r" (width), "r" (buffer) : "eax", "memory" );
}

void I_Sleep(int ms) { 
    (void)ms;
    //asm volatile ("pushl %%ebx; pushl %0; pushl $13; int $0x30; addl $8, %%esp; popl %%ebx;" : : "r" (ms) : "eax", "memory");
}

void *malloc(size_t size) {
    size_t aligned_size = (size + 7) & ~7;
    if (heap_pointer + aligned_size > DOOM_HEAP_SIZE) return NULL;
    void *ptr = &doom_heap[heap_pointer];
    heap_pointer += aligned_size;
    return ptr;
}

/* INPUT TRANSLATION
   Maps Pintos ASCII/Scan codes to the internal Doom keyboard format. */
unsigned char map_pintos_to_doom(int c) {
    if (c == 'w') return 0xad;      // Up arrow
    if (c == 's') return 0xaf;      // Down arrow
    if (c == 'a') return 0xac;      // Left arrow
    if (c == 'd') return 0xae;      // Right arrow
    if (c == ' ') return ' ';       // Spacebar (Vanilla DOOM: Use/Open)
    if (c == 13)  return 13;        // Enter
    
    if (c == 'f') return 'f';       // Shoot
    if (c == 'e') return 'e';       // Open door
    
    if (c >= '1' && c <= '7') return c; // Weapon-button
    
    return (unsigned char)c;
}

/* DOOMGENERIC IMPLEMENTATION
   Handles screen initialization and pixel-doubling logic (scaling). */
void DG_Init(void) { 

    for(int i=0; i<1024*768; i++) doom_internal_buffer[i] = 0;
    DG_ScreenBuffer = doom_internal_buffer; 
}

void DG_DrawFrame(void) {
    uint32_t* src = (uint32_t*)doom_internal_buffer;
    uint32_t* dst = (uint32_t*)pintos_scale_buffer;

    for (int y = 0; y < 200; y++) {
        uint32_t* line1 = dst + (y * 2) * 640;
        uint32_t* line2 = dst + (y * 2 + 1) * 640;
        
        for (int x = 0; x < 320; x++) {
            uint32_t color = *src++;
            line1[0] = color;
            line1[1] = color;
            line2[0] = color;
            line2[1] = color;
            line1 += 2;
            line2 += 2;
        }
    }
    draw_frame(pintos_scale_buffer, 640, 400);
    internal_ticks++; 
}

uint32_t DG_GetTicksMs(void) { return sys_time(); }

int DG_GetKey(int* pressed, unsigned char* key) {
    int c;
    
    while ((c = sys_getc()) != -1) {
        if (c >= 0 && c < 256) {
            if (key_timers[c] == 0) {
                key_timers[c] = KEY_TIMEOUT;
                *pressed = 1;
                *key = map_pintos_to_doom(c);
                return 1; 
            }
            key_timers[c] = KEY_TIMEOUT;

        }
    }

    static uint32_t last_tick = 0;
    if (internal_ticks != last_tick) {
        last_tick = internal_ticks;
        
        for (int i = 0; i < 256; i++) {
            if (key_timers[i] > 0) {
                key_timers[i]--;
                if (key_timers[i] == 0) {
                    *pressed = 0;
                    *key = map_pintos_to_doom(i);
                    return 1;
                }
            }
        }
    }

    return 0;
}

void DG_SetWindowTitle(const char *t) { }

/* INPUT/SYSTEM */
void I_GetEvent(void) {
    event_t event;
    int pressed;
    unsigned char key;

    while (DG_GetKey(&pressed, &key)) {
        event.type = pressed ? ev_keydown : ev_keyup;
        event.data1 = key;
        D_PostEvent(&event);
    }
}

int I_GetTime(void) {
    return (int)internal_ticks;
}

int I_GetSfxLumpNum(void* sfx) { 
    return 0; 
}

int I_GetTimeMS(void) { return sys_time(); }

/* UTILITY FUNCTIONS (LIBC REPLACEMENTS)
   Minimal implementations of string functions for the Pintos environment. */
int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0) {
        if (*s1 != *s2) return (unsigned char)*s1 - (unsigned char)*s2;
        if (*s1 == '\0') return 0;
        s1++; s2++; n--;
    }
    return 0;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 || *s2) {
        unsigned char c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 + 32 : *s1;
        unsigned char c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 + 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return 0;
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n-- > 0 && (*s1 || *s2)) {
        unsigned char c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 + 32 : *s1;
        unsigned char c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 + 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return 0;
}

void free(void *ptr) { (void)ptr; }

/* WAD RESOURCE MANAGEMENT
   Responsible for loading the WAD file (Where's All the Data) 
   into the static heap memory at startup. */
int W_Read(wad_file_t *wf, int off, void *buf, int len) {
    if (!wf || off + len > wf->length) return 0;
    
    if (wf->mapped != NULL) {
        memcpy(buf, wf->mapped + off, len);
        return len;
    } else {
        seek(wf->fd, off);
        
        int total_read = 0;
        while (total_read < len) {
            int r = read(wf->fd, (char *)buf + total_read, len - total_read);
            if (r <= 0) break;
            total_read += r;
        }
        return total_read;
    }
}

void W_CloseFile(wad_file_t *wf) { if(wf) close(wf->fd); }

int W_FileLength(wad_file_t *wf) { return wf ? wf->length : 0; }

/* MAIN */
int main(int argc, char **argv) {
    doomgeneric_Create(argc, argv);
    while (1) { doomgeneric_Tick(); }
    return 0;
}

/* DOOM ENGINE STUBS
   Empty functions required by the Doom engine for features like 
   sound, networking, or joysticks that are not yet implemented. */
void I_PrecacheSounds(void) {}
void I_UpdateSound(void) {}
void I_SubmitSound(void) {}
void I_ShutdownSound(void) {}
void I_SetChannels(void) {}
int I_StartSound(int id, int v, int s, int p, int pr) { return 0; }
void I_StopSound(int h) {}
int I_SoundIsPlaying(int h) { return 0; }
void I_UpdateSoundParams(int a, int b, int c, int d) {}
void I_InitMusic(void) {}
void I_ShutdownMusic(void) {}
void I_SetMusicVolume(int v) {}
void I_PauseSong(int h) {}
void I_ResumeSong(int h) {}
void I_StopSong(int h) {}
void I_PlaySong(int h, int l) {}
int I_RegisterSong(void* d, int l) { return 0; }
void I_UnRegisterSong(int h) {}
int I_MusicIsPlaying(int h) { return 0; }
void I_InitInput(void) {}
void I_InitJoystick(void) {}
void I_BindJoystickGui(void) {}
void I_BindJoystickVariables(void) {}
void vanilla_keyboard_mapping(void) {}
void I_Endoom(void) {}
void StatCopy(void) {}
void StatDump(void) {}
void I_InitTimer(void) {}
void I_WaitVBL(int count) {}
void I_InitSound(void) {}
void I_BindSoundVariables(void) {}

void W_ParseCommandLine(void) {}
void W_Checksum(void) {}
int W_Seek(wad_file_t *wf, int o, int r) { (void)wf;(void)o;(void)r; return 0; }