#ifndef DOOM_GENERIC
#define DOOM_GENERIC

#pragma pack(push, 1)

#include <stdlib.h>
#include <stdint.h>

#ifndef DOOMGENERIC_RESX
#define DOOMGENERIC_RESX 320
#endif  // DOOMGENERIC_RESX

#ifndef DOOMGENERIC_RESY
#define DOOMGENERIC_RESY 200
#endif  // DOOMGENERIC_RESY


#ifdef CMAP256

typedef uint8_t pixel_t;

#else  // CMAP256

typedef uint32_t pixel_t;

#endif  // CMAP256


extern pixel_t* DG_ScreenBuffer;

#ifdef __cplusplus
extern "C" {
#endif

void doomgeneric_Create(int argc, char **argv);
void doomgeneric_Tick();

void DG_Init();
void DG_SleepMs(uint32_t ms);
void DG_DrawFrame(void);
uint32_t DG_GetTicksMs(void);
int DG_GetKey(int* pressed, unsigned char* key);
void DG_SetWindowTitle(const char * title);

#ifdef __cplusplus
}
#endif

/* Added for Pintos to be able to run. */
#pragma pack(pop)

#undef DOOMGENERIC_RESX
#define DOOMGENERIC_RESX 320

#undef DOOMGENERIC_RESY
#define DOOMGENERIC_RESY 200

#endif //DOOM_GENERIC
