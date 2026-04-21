#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <emscripten/emscripten.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include "../core.h"
#include "../octemu.h"

#define RUNNING 1
#define PAUSED 2
#define HALTED 3

#define INTERVAL_NS 16666666

static uint8_t audio_samples[96];

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_AudioStream *audio_stream = NULL;
static OctEmu *emu_core = NULL;

static int tickrate = OCTEMU_TICKRATE_SCHIP;
static uint32_t color_fg = OCTEMU_FOREGROUND_RGB, color_bg = OCTEMU_BACKGROUND_RGB;

static uint8_t status = HALTED;
static uint16_t keypad = 0; // 0: none, 0-15 bit: keypad[0-15]


EMSCRIPTEN_KEEPALIVE
const char *get_version() { return OCTEMU_VERSION; }

EMSCRIPTEN_KEEPALIVE
int set_mode(const int m) {
    if (!emu_core || (m != OCTEMU_MODE_CHIP8 && m != OCTEMU_MODE_SCHIP && m != OCTEMU_MODE_OCTO))
        return 1;
    emu_core->mode = (OctEmuMode)m;
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int set_tickrate(const int t) {
    if (t < 1 || t > 1000)
        return 1;
    tickrate = t;
    return 0;
}

EMSCRIPTEN_KEEPALIVE
void set_color(const uint32_t fg, const uint32_t bg) {
    color_fg = fg & 0xFFFFFF;
    color_bg = bg & 0xFFFFFF;
}

EMSCRIPTEN_KEEPALIVE
int run(const uint8_t *rom, const size_t size) {
    if (!emu_core)
        return 1;
    if (emu_core->rom)
        octemu_clear_rom(emu_core);
    if (octemu_load_rom(emu_core, rom, size))
        return 1;
    status = RUNNING;
    return 0;
}

static uint64_t eval_loop(void *_, SDL_TimerID id, uint64_t interval) {
    // assert(emu_core);
    if (status == PAUSED || status == HALTED) {
        return INTERVAL_NS * 10;
    }

    int err = 0;
    for (int i = 0; i < tickrate; i++) {
        err = octemu_eval(emu_core, keypad);
        if (err || (emu_core->mode == OCTEMU_MODE_CHIP8 && emu_core->gfx_dirty))
            break;
    }
    if (err) {
        emu_core->sound = 0;
        fputs("Emulator halted...\n", stderr);
        status = HALTED;
        return INTERVAL_NS * 10;
    }

    octemu_tick(emu_core);
    return INTERVAL_NS;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    emu_core = octemu_new(OCTEMU_MODE_OCTO);
    if (!emu_core)
        return SDL_APP_FAILURE;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) ||
        !SDL_CreateWindowAndRenderer("octemu",
                                     OCTEMU_WINDOW_WIDTH, OCTEMU_WINDOW_HEIGHT,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer) ||
        !SDL_SetRenderLogicalPresentation(renderer, OCTEMU_GFX_WIDTH, OCTEMU_GFX_HEIGHT,
                                          SDL_LOGICAL_PRESENTATION_STRETCH)) {
        goto err;
    }
    texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        OCTEMU_GFX_WIDTH, OCTEMU_GFX_HEIGHT);
    if (!texture ||
        !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST) ||
        !SDL_SetRenderVSync(renderer, 1))
        goto err;

    const SDL_AudioSpec spec = {.channels = 1, .freq = 1000, .format = SDL_AUDIO_U8};
    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!audio_stream || !SDL_ResumeAudioStreamDevice(audio_stream))
        goto err;
    for (int i = 0; i < (sizeof(audio_samples) >> 1); i++) {
        audio_samples[i * 2] = 192;
        audio_samples[i * 2 + 1] = 64;
    }

    srand((unsigned int)time(NULL));
    if (!SDL_AddTimerNS(INTERVAL_NS * 10, eval_loop, NULL))
        goto err;
    return SDL_APP_CONTINUE;

err:
    fputs(SDL_GetError(), stderr);
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    if (status == RUNNING && emu_core->sound) {
        if (SDL_GetAudioStreamQueued(audio_stream) < 50 * sizeof(uint8_t))
            SDL_PutAudioStreamData(audio_stream, audio_samples, sizeof(audio_samples));
    } else if (SDL_GetAudioStreamQueued(audio_stream))
        SDL_ClearAudioStream(audio_stream);

    if (emu_core->gfx_dirty) {
        uint32_t *pixels;
        int pitch;
        if (!SDL_LockTexture(texture, NULL, (void **)&pixels, &pitch))
            return SDL_APP_FAILURE;
        for (int y = 0; y < OCTEMU_GFX_HEIGHT; y++) {
            for (int x = 0; x < (OCTEMU_GFX_WIDTH >> 3); x++) {
                for (int bit = 0; bit < 8; bit++) {
                    const uint16_t pos = y * OCTEMU_GFX_WIDTH + x * 8 + bit;
                    if (emu_core->gfx[y][x] & (1 << (7 - bit)))
                        pixels[pos] = (color_fg & 0xFFFFFF) | 0xFF000000;
                    else
                        pixels[pos] = (color_bg & 0xFFFFFF) | 0xFF000000;
                }
            }
        }
        emu_core->gfx_dirty = false;
        SDL_UnlockTexture(texture);
    }
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    else if (event->type == SDL_EVENT_KEY_DOWN) {
        for (int i = 0; i < 16; i++) {
            if (event->key.scancode == keymapping[i]) {
                keypad |= (1 << OctEmu_Keypad[i]);
                break;
            }
        }
    } else if (event->type == SDL_EVENT_KEY_UP) {
        switch (event->key.scancode) {
        case SDL_SCANCODE_SPACE: { // pause/resume
            if (status == RUNNING)
                status = PAUSED;
            else if (status == PAUSED)
                status = RUNNING;
            break;
        }
        case SDL_SCANCODE_F5: // reset
            octemu_reset(emu_core);
            status = RUNNING;
            break;
        default:
            for (int i = 0; i < 16; i++) {
                if (event->key.scancode == keymapping[i]) {
                    keypad &= ~(1 << OctEmu_Keypad[i]);
                    break;
                }
            }
        }
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult res) {}
