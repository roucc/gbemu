#include "cartridge.h"
#include "cpu.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <stdbool.h>

#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 144
#define DISPLAY_SCALE 5

#define BUTTON_RIGHT 0x01
#define BUTTON_LEFT 0x02
#define BUTTON_UP 0x04
#define BUTTON_DOWN 0x08
#define BUTTON_A 0x01
#define BUTTON_B 0x02
#define BUTTON_SELECT 0x04
#define BUTTON_START 0x08

static uint32_t colors[4] = {
    [0] = 0xFFFFFFFF,
    [1] = 0xFF404040,
    [2] = 0xFFBFBFBF,
    [3] = 0xFF000000,
};
void DISPLAY_plot_tile(uint8_t *tile, int x, int y, uint32_t *pixels) {
  for (int row = 0; row < 8; row++) {
    uint8_t b0 = *tile++;
    uint8_t b1 = *tile++;

    for (int col = 0; col < 8; col++) {
      uint8_t X = x + col;
      uint8_t Y = y + row;
      if (X >= DISPLAY_WIDTH || Y >= DISPLAY_HEIGHT) {
        continue;
      }
      uint8_t colorindex = ((b0 & 0x80) >> 6) | ((b1 & 0x80) >> 7);
      uint32_t color = colors[colorindex];

      if (colorindex != 0) {
        pixels[Y * DISPLAY_WIDTH + X] = color;
      }
      b0 <<= 1;
      b1 <<= 1;
    }
  }
}

void DISPLAY_gbmemory_to_sdl(uint32_t *pixels, CPU *cpu) {

  uint8_t *vram = CPU_memory(cpu) + 0x8000;

  for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
    pixels[i] = colors[0];
  }

  const uint8_t *bg_map = vram + 0x1800;
  for (int row = 0; row < 32; row++) {
    for (int col = 0; col < 32; col++) {
      uint8_t tile_index = bg_map[row * 32 + col];
      uint8_t *tile = vram + tile_index * 16 + 0x1000;
      DISPLAY_plot_tile(tile, col * 8, row * 8, pixels);
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("syntax: %s rom\n", argv[0]);
    exit(1);
  };

  CPU *cpu = CPU_new();

  cpu->cart = cart_load(argv[1]);
  if (!cpu->cart) {
    printf("Failed to load ROM\n");
    return 1;
  }
  printf("Loaded %zu bytes of ROM\n", cpu->cart->rom_size);
  printf("Cartridge type: %02X\n", cpu->cart->rom[0x0147]);

  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window *window = SDL_CreateWindow(
      "gameboy emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      DISPLAY_WIDTH * DISPLAY_SCALE, DISPLAY_HEIGHT * DISPLAY_SCALE, 0);
  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           DISPLAY_WIDTH, DISPLAY_HEIGHT);
  SDL_Rect dest_rect = {0, 0, DISPLAY_WIDTH * DISPLAY_SCALE,
                        DISPLAY_HEIGHT * DISPLAY_SCALE};
  uint32_t *pixels = malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint32_t));
  memset(pixels, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint32_t));

  // default run speed
  int batches = 1;
  while (1) {
    // handle inputs
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        exit(0);
      }
      if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        bool pressed = (event.type == SDL_KEYDOWN) ? 0 : 1;

        switch (event.key.keysym.sym) {
        case SDLK_SPACE:
          // boost
          batches = pressed ? 1 : 10;
          break;
        case SDLK_w:
          cpu->direction_state =
              (cpu->direction_state & ~BUTTON_UP) | (pressed ? BUTTON_UP : 0);
          break;
        case SDLK_s:
          cpu->direction_state = (cpu->direction_state & ~BUTTON_DOWN) |
                                 (pressed ? BUTTON_DOWN : 0);
          break;
        case SDLK_a:
          cpu->direction_state = (cpu->direction_state & ~BUTTON_LEFT) |
                                 (pressed ? BUTTON_LEFT : 0);
          break;
        case SDLK_d:
          cpu->direction_state = (cpu->direction_state & ~BUTTON_RIGHT) |
                                 (pressed ? BUTTON_RIGHT : 0);
          break;
        case SDLK_k:
          cpu->button_state =
              (cpu->button_state & ~BUTTON_A) | (pressed ? BUTTON_A : 0);
          break;
        case SDLK_j:
          cpu->button_state =
              (cpu->button_state & ~BUTTON_B) | (pressed ? BUTTON_B : 0);
          break;
        case SDLK_l:
          cpu->button_state = (cpu->button_state & ~BUTTON_SELECT) |
                              (pressed ? BUTTON_SELECT : 0);
          break;
        case SDLK_SEMICOLON:
          cpu->button_state = (cpu->button_state & ~BUTTON_START) |
                              (pressed ? BUTTON_START : 0);
          break;
        case SDLK_ESCAPE:
          CPU_display(cpu);
          break;
        case SDLK_q:
          exit(0);
          break;
        }
      }
    }

    // cpu loop
    for (int scanline = 0; scanline < 144; scanline++) {
      cpu->ly = scanline;
      for (int j = 0; j < batches; j++) {
        // mode 2, oam
        CPU_check_stat_interrupt(cpu, 2);
        CPU_run(cpu, 80);

        // mode 3, lcd
        CPU_check_stat_interrupt(cpu, 3);
        CPU_run(cpu, 172);

        // mode 0, hblank
        CPU_check_stat_interrupt(cpu, 0);
        CPU_run(cpu, 204);
      }
    }

    // vblank loop
    for (int scanline = 144; scanline < 154; scanline++) {
      cpu->ly = scanline;
      for (int j = 0; j < batches; j++) {
        // mode 1, vblank
        CPU_check_stat_interrupt(cpu, 1);
        if (cpu->ly == 144) {
          // request vblank interrupt
          cpu->if_reg |= 0x01;
        }
        CPU_run(cpu, 456);
      }
    }

    // draw screen
    DISPLAY_gbmemory_to_sdl(pixels, cpu);
    SDL_UpdateTexture(texture, NULL, pixels, 160 * sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
    // wait for vsync
    SDL_RenderPresent(renderer);
  }
};
