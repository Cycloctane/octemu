#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include "core.h"

#define OCTEMU_FREQ_HZ 600
#define OCTEMU_FOREGROUND_RGB 42,161,152
#define OCTEMU_BACKGROUND_RGB 0,43,54

static const SDL_Scancode keymapping[16] = {
    SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
    SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R,
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
    SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V
};

static uint8_t audio_samples[200];

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioStream *audio_stream = NULL;
static OctEmu *emu_core = NULL;
static pthread_t *eval_thread = NULL;

static atomic_uchar status = 1;     // 0: exiting, 1: running, 2: paused
static atomic_ushort keystroke = 0; // 0: none, 0-15 bit: keypad[0-15]
static atomic_bool sound = false;
static pthread_mutex_t gfx_lock = PTHREAD_MUTEX_INITIALIZER;
static bool gfx_reload = true;

static void *eval_loop(void *arg) {
    // assert(emu_core);
    const uint16_t interval_us = 1000000 / OCTEMU_FREQ_HZ;
    uint16_t timer = 0;
    srand((unsigned int)time(NULL));
    for (uint8_t s = 2; s; s = atomic_load(&status)) {
        if (s == 2) {
            usleep(200000);
            continue;
        }
        const uint16_t next_ins = octemu_peek_ins(emu_core);
        const bool update_gfx = next_ins == 0x00E0 || (next_ins & 0xF000) >> 12 == 0xD;
        if (update_gfx)
            pthread_mutex_lock(&gfx_lock);
        int err = octemu_eval(emu_core, atomic_load(&keystroke));
        if (update_gfx) {
            gfx_reload = true;
            pthread_mutex_unlock(&gfx_lock);
        }
        if (err) {
            atomic_store(&sound, 0);
            fputs("Emulator halted...", stderr);
            return NULL;
        }
        atomic_store(&sound, emu_core->sound != 0);
        usleep(interval_us);
        timer += interval_us;
        if (timer >= 16666) {
            octemu_tick(emu_core);
            timer = 0;
        }
    }
    return NULL;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom_file>\n", argv[0]);
        return SDL_APP_FAILURE;
    }
    emu_core = octemu_new();
    if (!emu_core)
        return SDL_APP_FAILURE;
    if (octemu_load_rom(emu_core, argv[1]))
        return SDL_APP_FAILURE;

    SDL_SetAppMetadata("octemu", NULL, NULL);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) ||
        !SDL_CreateWindowAndRenderer(
            "octemu", 640, 320, SDL_WINDOW_RESIZABLE, &window, &renderer) ||
        !SDL_SetRenderLogicalPresentation(
            renderer, 64, 32, SDL_LOGICAL_PRESENTATION_STRETCH)) {
        fputs(SDL_GetError(), stderr);
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderVSync(renderer, 1);

    const SDL_AudioSpec spec = {.channels = 1, .freq = 1000, .format = SDL_AUDIO_U8};
    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!audio_stream || !SDL_ResumeAudioStreamDevice(audio_stream)) {
        fputs(SDL_GetError(), stderr);
        return SDL_APP_FAILURE;
    }
    for (int i = 0; i < (sizeof(audio_samples) >> 1); i++) {
        audio_samples[i * 2] = 192;
        audio_samples[i * 2 + 1] = 64;
    }
    eval_thread = malloc(sizeof(pthread_t));
    if (!eval_thread || pthread_create(eval_thread, NULL, &eval_loop, NULL)) {
        fputs("Failed to create eval thread\n", stderr);
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    static uint8_t gfx_buffer[OCTEMU_GFX_HEIGHT][OCTEMU_GFX_WIDTH / 8];
    bool draw = false;

    if ((atomic_load(&status) == 1) && atomic_load(&sound)) {
        if (SDL_GetAudioStreamQueued(audio_stream) < 100 * sizeof(uint8_t))
            SDL_PutAudioStreamData(audio_stream, audio_samples, sizeof(audio_samples));
    } else if (SDL_GetAudioStreamQueued(audio_stream))
        SDL_ClearAudioStream(audio_stream);

    pthread_mutex_lock(&gfx_lock);
    if (gfx_reload) {
        memcpy(gfx_buffer, emu_core->gfx, sizeof(gfx_buffer));
        draw = true;
        gfx_reload = false;
    }
    pthread_mutex_unlock(&gfx_lock);

    if (draw) {
        SDL_SetRenderDrawColor(renderer, OCTEMU_BACKGROUND_RGB, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, OCTEMU_FOREGROUND_RGB, 255);
        for (int y = 0; y < OCTEMU_GFX_HEIGHT; y++) {
            for (int x = 0; x < (OCTEMU_GFX_WIDTH >> 3); x++) {
                for (int bit = 0; bit < 8; bit++) {
                    if (gfx_buffer[y][x] & (1 << (7 - bit))) {
                        SDL_RenderPoint(renderer, x * 8 + bit, y);
                    }
                }
            }
        }
        SDL_RenderPresent(renderer);
    }
    SDL_Delay(2);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    else if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        if (!pthread_mutex_trylock(&gfx_lock)) {
            gfx_reload = true;
            pthread_mutex_unlock(&gfx_lock);
        }
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        for (int i = 0; i < 16; i++) {
            if (event->key.scancode == keymapping[i]) {
                atomic_fetch_or(&keystroke, 1 << OctEmu_Keypad[i]);
                break;
            }
        }
    } else if (event->type == SDL_EVENT_KEY_UP) {
        switch (event->key.scancode) {
        case SDL_SCANCODE_ESCAPE:
            return SDL_APP_SUCCESS;
        case SDL_SCANCODE_SPACE: { // pause/resume
            const uint8_t current_status = atomic_load(&status);
            if (current_status == 1) {
                atomic_store(&status, 2);
                SDL_SetWindowTitle(window, "octemu (paused)");
            } else if (current_status == 2) {
                atomic_store(&status, 1);
                SDL_SetWindowTitle(window, "octemu");
            }
            break;
        }
        default:
            for (int i = 0; i < 16; i++) {
                if (event->key.scancode == keymapping[i]) {
                    atomic_fetch_and(&keystroke, ~(1 << OctEmu_Keypad[i]));
                    break;
                }
            }
        }
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if (eval_thread) {
        atomic_store(&status, 0);
        pthread_join(*eval_thread, NULL);
        free(eval_thread);
    }
    if (emu_core)
        octemu_free(emu_core);
}
