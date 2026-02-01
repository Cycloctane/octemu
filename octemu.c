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

#ifndef OCTEMU_TICKRATE_CHIP8
#define OCTEMU_TICKRATE_CHIP8 15
#endif
#ifndef OCTEMU_TICKRATE_SCHIP
#define OCTEMU_TICKRATE_SCHIP 200
#endif
#ifndef OCTEMU_FOREGROUND_RGB
#define OCTEMU_FOREGROUND_RGB 0x2AA198
#endif
#ifndef OCTEMU_BACKGROUND_RGB
#define OCTEMU_BACKGROUND_RGB 0x002B36
#endif
#ifndef OCTEMU_WINDOW_WIDTH
#define OCTEMU_WINDOW_WIDTH 640
#endif
#ifndef OCTEMU_WINDOW_HEIGHT
#define OCTEMU_WINDOW_HEIGHT 320
#endif
#ifndef OCTEMU_VERSION
#define OCTEMU_VERSION "dev"
#endif

#define EXITING 0
#define RUNNING 1
#define PAUSED 2
#define HALTED 3
#define RESET 4

#define load(var) atomic_load_explicit(&var, memory_order_acquire)
#define store(var, val) atomic_store_explicit(&var, val, memory_order_release)

static const SDL_Scancode keymapping[16] = {
    SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
    SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R,
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
    SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V
};

static uint8_t audio_samples[96];

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_AudioStream *audio_stream = NULL;
static OctEmu *emu_core = NULL;
static SDL_Thread *eval_thread = NULL;

static atomic_uchar status = RUNNING;
static atomic_ushort keypad = 0; // 0: none, 0-15 bit: keypad[0-15]
static atomic_bool sound = false, gfx_reload = true;
static SDL_Mutex *gfx_lock = NULL;
static uint8_t gfx_buffer[OCTEMU_GFX_HEIGHT][OCTEMU_GFX_WIDTH / 8];

static bool screenshot = false; // not shared

static int eval_loop(void *tickrate) {
    // assert(emu_core);
    srand((unsigned int)time(NULL));
    for (uint8_t s = PAUSED; s; s = load(status)) {
        if (s == PAUSED || s == HALTED) {
            usleep(200000);
            continue;
        } else if (s == RESET) {
            octemu_reset(emu_core);
            SDL_LockMutex(gfx_lock);
            memset(gfx_buffer, 0, sizeof(gfx_buffer));
            store(gfx_reload, true);
            SDL_UnlockMutex(gfx_lock);
            store(status, RUNNING);
            continue;
        }

        int err = 0;
        const uint16_t current_keypad = atomic_load(&keypad);
        for (int i = 0; i < *(int *)tickrate; i++) {
            err = octemu_eval(emu_core, current_keypad);
            if (err)
                break;
        }

        if (err) {
            store(sound, 0);
            fputs("Emulator halted...\n", stderr);
            store(status, HALTED);
            continue;
        } else if (emu_core->gfx_dirty) {
            SDL_LockMutex(gfx_lock);
            memcpy(gfx_buffer, emu_core->gfx, sizeof(gfx_buffer));
            store(gfx_reload, true);
            SDL_UnlockMutex(gfx_lock);
            emu_core->gfx_dirty = false;
        }
        store(sound, emu_core->sound != 0);

        usleep(16666);
        octemu_tick(emu_core);
    }
    return 0;
}

static int printscreen() {
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, NULL);
    if (!surface) {
        fprintf(stderr, "Failed to take screenshot: %s\n", SDL_GetError());
        return 1;
    }
    const time_t current_time = time(NULL);
    char file[27];
    strftime(file, sizeof(file), "octemu_%Y%m%d_%H%M%S.bmp", localtime(&current_time));
    if (!SDL_SaveBMP(surface, file)) {
        fprintf(stderr, "Failed to save screenshot: %s\n", SDL_GetError());
        SDL_DestroySurface(surface);
        return 1;
    }
    SDL_DestroySurface(surface);
    return 0;
}

static void print_usage(const char *argv0) {
    printf("Usage: %s [option...] <rom_file>\n\nOPTIONS\n", argv0);
    puts("-m chip8|schip|octo\tmode (default octo)");
    printf("-t <uint>\t\ttickrate (default %d in chip8 mode, %d in schip/octo mode)\n",
           OCTEMU_TICKRATE_CHIP8, OCTEMU_TICKRATE_SCHIP);
    puts("-v\t\t\tprint version and exit\n");
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    int opt;
    static int tickrate = 0;
    OctEmuMode mode = OCTEMU_MODE_OCTO;
    while ((opt = getopt(argc, argv, "t:m:v?h")) != -1) {
        switch (opt) {
        case 't':
            tickrate = atoi(optarg);
            if (tickrate < 1 || tickrate > 1000) { // 1 - 1000 cycles per frame
                fputs("Invalid tickrate\n", stderr);
                print_usage(argv[0]);
                return SDL_APP_FAILURE;
            }
            break;
        case 'm':
            if (!strcmp(optarg, "chip8"))
                mode = OCTEMU_MODE_CHIP8;
            else if (!strcmp(optarg, "schip"))
                mode = OCTEMU_MODE_SCHIP;
            else if (!strcmp(optarg, "octo"))
                mode = OCTEMU_MODE_OCTO;
            else {
                fputs("Invalid mode\n", stderr);
                print_usage(argv[0]);
                return SDL_APP_FAILURE;
            }
            break;
        case 'v':
            printf("octemu %s\n", OCTEMU_VERSION);
            return SDL_APP_SUCCESS;
        case '?':
        case 'h':
            print_usage(argv[0]);
            return SDL_APP_SUCCESS;
        default:
            print_usage(argv[0]);
            return SDL_APP_FAILURE;
        }
    }
    if (optind >= argc) {
        print_usage(argv[0]);
        return SDL_APP_FAILURE;
    }
    if (!tickrate)
        tickrate = (mode == OCTEMU_MODE_CHIP8) ? OCTEMU_TICKRATE_CHIP8 : OCTEMU_TICKRATE_SCHIP;
    emu_core = octemu_new(mode);
    if (!emu_core || octemu_load_rom_file(emu_core, argv[optind]))
        return SDL_APP_FAILURE;

    SDL_SetAppMetadata("octemu", NULL, NULL);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) ||
        !SDL_CreateWindowAndRenderer("octemu", OCTEMU_WINDOW_WIDTH, OCTEMU_WINDOW_HEIGHT,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer) ||
        !SDL_SetRenderLogicalPresentation(renderer, OCTEMU_GFX_WIDTH, OCTEMU_GFX_HEIGHT,
                                          SDL_LOGICAL_PRESENTATION_STRETCH)) {
        goto err;
    }
    texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        OCTEMU_GFX_WIDTH, OCTEMU_GFX_HEIGHT);
    if (!texture || !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST))
        goto err;
    SDL_SetRenderVSync(renderer, 1);

    const SDL_AudioSpec spec = {.channels = 1, .freq = 1000, .format = SDL_AUDIO_U8};
    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!audio_stream || !SDL_ResumeAudioStreamDevice(audio_stream))
        goto err;
    for (int i = 0; i < (sizeof(audio_samples) >> 1); i++) {
        audio_samples[i * 2] = 192;
        audio_samples[i * 2 + 1] = 64;
    }

    gfx_lock = SDL_CreateMutex();
    if (!gfx_lock)
        goto err;
    eval_thread = SDL_CreateThread(eval_loop, "eval_loop", &tickrate);
    if (!eval_thread)
        goto err;

    return SDL_APP_CONTINUE;

err:
    fputs(SDL_GetError(), stderr);
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    static uint8_t local_buffer[OCTEMU_GFX_HEIGHT][OCTEMU_GFX_WIDTH / 8];

    if (load(status) == RUNNING && load(sound)) {
        if (SDL_GetAudioStreamQueued(audio_stream) < 50 * sizeof(uint8_t))
            SDL_PutAudioStreamData(audio_stream, audio_samples, sizeof(audio_samples));
    } else if (SDL_GetAudioStreamQueued(audio_stream))
        SDL_ClearAudioStream(audio_stream);

    if (load(gfx_reload)) {
        SDL_LockMutex(gfx_lock);
        memcpy(local_buffer, gfx_buffer, sizeof(local_buffer));
        store(gfx_reload, false);
        SDL_UnlockMutex(gfx_lock);

        uint32_t *pixels;
        int pitch;
        if (!SDL_LockTexture(texture, NULL, (void **)&pixels, &pitch))
            return SDL_APP_FAILURE;
        for (int y = 0; y < OCTEMU_GFX_HEIGHT; y++) {
            for (int x = 0; x < (OCTEMU_GFX_WIDTH >> 3); x++) {
                for (int bit = 0; bit < 8; bit++) {
                    const uint16_t pos = y * OCTEMU_GFX_WIDTH + x * 8 + bit;
                    if (local_buffer[y][x] & (1 << (7 - bit)))
                        pixels[pos] = (OCTEMU_FOREGROUND_RGB & 0xFFFFFF) | 0xFF000000;
                    else
                        pixels[pos] = (OCTEMU_BACKGROUND_RGB & 0xFFFFFF) | 0xFF000000;
                }
            }
        }
        SDL_UnlockTexture(texture);
    }
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    if (screenshot) {
        printscreen();
        screenshot = false;
    }
    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    else if (event->type == SDL_EVENT_KEY_DOWN) {
        for (int i = 0; i < 16; i++) {
            if (event->key.scancode == keymapping[i]) {
                atomic_fetch_or_explicit(&keypad, 1 << OctEmu_Keypad[i], memory_order_acq_rel);
                break;
            }
        }
    } else if (event->type == SDL_EVENT_KEY_UP) {
        switch (event->key.scancode) {
        case SDL_SCANCODE_ESCAPE:
            return SDL_APP_SUCCESS;
        case SDL_SCANCODE_SPACE: { // pause/resume
            const uint8_t current_status = load(status);
            if (current_status == RUNNING) {
                store(status, PAUSED);
                SDL_SetWindowTitle(window, "octemu (paused)");
            } else if (current_status == PAUSED) {
                store(status, RUNNING);
                SDL_SetWindowTitle(window, "octemu");
            }
            break;
        }
        case SDL_SCANCODE_F5: // reset
            if (load(status) == PAUSED)
                SDL_SetWindowTitle(window, "octemu");
            store(status, RESET);
            break;
        case SDL_SCANCODE_F12: // screenshot
            screenshot = true;
            break;
        default:
            for (int i = 0; i < 16; i++) {
                if (event->key.scancode == keymapping[i]) {
                    atomic_fetch_and_explicit(&keypad, ~(1 << OctEmu_Keypad[i]), memory_order_acq_rel);
                    break;
                }
            }
        }
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if (texture)
        SDL_DestroyTexture(texture);
    if (eval_thread) {
        store(status, EXITING);
        SDL_WaitThread(eval_thread, NULL);
    }
    if (gfx_lock)
        SDL_DestroyMutex(gfx_lock);
    if (emu_core)
        octemu_free(emu_core);
}
