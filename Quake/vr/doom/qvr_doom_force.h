/* qvr_doom_force.h -- included first in every file of qvr_doom.dll (ForcedIncludeFiles): Doom's heap is a private one
   (freed whole when Doom shuts down: qvr_doom_shutdown), its text goes to the engine's console, and an exit() anywhere
   returns to the engine as an error instead of ending the process. qvr_doom_dll.c implements these. */

#ifndef QVR_DOOM_FORCE_H
#define QVR_DOOM_FORCE_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void* qvr_doom_malloc(size_t size);
void* qvr_doom_calloc(size_t count, size_t size);
void* qvr_doom_realloc(void* p, size_t size);
void qvr_doom_free(void* p);
char* qvr_doom_strdup(const char* s);
int qvr_doom_printf(const char* format, ...);
int qvr_doom_fprintf(FILE* stream, const char* format, ...);
int qvr_doom_vfprintf(FILE* stream, const char* format, va_list args);
int qvr_doom_puts(const char* text);
int qvr_doom_putchar(int c);
void QVR_DoomFatal(const char* message);

#ifndef QVR_DOOM_GLUE
#define malloc qvr_doom_malloc
#define calloc qvr_doom_calloc
#define realloc qvr_doom_realloc
#define free qvr_doom_free
#define strdup qvr_doom_strdup
#define _strdup qvr_doom_strdup
#define printf qvr_doom_printf
#define fprintf qvr_doom_fprintf
#define vfprintf qvr_doom_vfprintf
#define puts qvr_doom_puts
#define putchar qvr_doom_putchar
#define exit(code) QVR_DoomFatal("exit() called")
#endif

#endif
