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

// interrupt registers
uint8_t ly = 0;                 // current scan line
uint8_t direction_state = 0x0F; // 0 = pressed
uint8_t button_state = 0x0F;
uint8_t joyp = 0x3F;
uint8_t if_reg = 0xE1;
uint8_t ie_reg = 0x00;

// Timer registers
uint8_t divr = 0; // 0xFF04 – Divider (increments every 256 cycles)
uint8_t tima = 0; // 0xFF05 – Timer counter
uint8_t tma = 0;  // 0xFF06 – Timer modulo (reload value)
uint8_t tac = 0;  // 0xFF07 – Timer control

// LCD status registers
uint8_t stat = 0; // 0xFF41 – LCD STAT
uint8_t lyc = 0;  // 0xFF45 – LYC compare value

void hw_write(uint16_t address, uint8_t val) {
  switch (address) {
  case 0xFF44:
    // vblank
    ly = val;
    break;
  case 0xFF00:
    // joyp
    joyp = val;
    break;
  case 0xFF0F:
    // interrupt flag
    if_reg = val;
    break;
  case 0xFFFF:
    // interrupt enable
    ie_reg = val;
    break;
  case 0xFF04:
    divr = 0; // writing resets div
    break;
  case 0xFF05:
    tima = val;
    break;
  case 0xFF06:
    tma = val;
    break;
  case 0xFF07:
    tac = val;
    break;
  case 0xFF41:
    stat = val;
    break;
  case 0xFF45:
    lyc = val;
    break;
  }
}

uint8_t hw_read(uint16_t address) {
  switch (address) {
  case 0xFF44:
    // vblank
    return ly;
  case 0xFF00: {
    // joyp
    uint8_t select = joyp & 0xF0;

    if (!(select & 0x10)) {
      joyp = select | direction_state;
    } else if (!(select & 0x20)) {
      joyp = select | button_state;
    } else {
      joyp = select | 0x0F; // nothing selected
    }
    return joyp;
  }
  case 0xFF0F:
    return if_reg;
  case 0xFFFF:
    return ie_reg;
  case 0xFF04:
    return divr;
  case 0xFF05:
    return tima;
  case 0xFF06:
    return tma;
  case 0xFF07:
    return tac;
  case 0xFF41:
    return stat;
  case 0xFF45:
    return lyc;
  default:
    return 0;
  }
}

void check_stat_interrupt(uint8_t mode) {
  if (ly == lyc) {
    stat |= 0x04; // LYC=LY flag
    if (stat & 0x40) {
      if_reg |= 0x02; // request STAT interrupt
    }
  } else {
    stat &= ~0x04;
  }

  stat = (stat & 0xFC) | (mode & 0x03); // set mode

  switch (mode) {
  case 0: // hblank
    if (stat & 0x08) {
      if_reg |= 0x02;
    }
    break;
  case 1: // vblank
    if (stat & 0x10) {
      if_reg |= 0x02;
    }
    break;
  case 2: // oam
    if (stat & 0x20) {
      if_reg |= 0x02;
    }
    break;
  case 3: // lcd
    // no stat interrupt
    break;
  }
}

void DISPLAY_gbmemory_to_sdl(uint32_t *pixels, uint8_t *vram) {
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
  CPU_hardware(cpu, hw_write, hw_read);

  // CPU_read_rom(cpu, argv[1]);
  cpu->cart = cart_load(argv[1]);
  if (!cpu->cart) {
    printf("Failed to load ROM\n");
    return 1;
  }
  printf("Loaded %zu bytes of ROM\n", cpu->cart->rom_size);

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
          direction_state =
              (direction_state & ~BUTTON_UP) | (pressed ? BUTTON_UP : 0);
          break;
        case SDLK_s:
          direction_state =
              (direction_state & ~BUTTON_DOWN) | (pressed ? BUTTON_DOWN : 0);
          break;
        case SDLK_a:
          direction_state =
              (direction_state & ~BUTTON_LEFT) | (pressed ? BUTTON_LEFT : 0);
          break;
        case SDLK_d:
          direction_state =
              (direction_state & ~BUTTON_RIGHT) | (pressed ? BUTTON_RIGHT : 0);
          break;
        case SDLK_k:
          button_state = (button_state & ~BUTTON_A) | (pressed ? BUTTON_A : 0);
          break;
        case SDLK_j:
          button_state = (button_state & ~BUTTON_B) | (pressed ? BUTTON_B : 0);
          break;
        case SDLK_l:
          button_state =
              (button_state & ~BUTTON_SELECT) | (pressed ? BUTTON_SELECT : 0);
          break;
        case SDLK_SEMICOLON:
          button_state =
              (button_state & ~BUTTON_START) | (pressed ? BUTTON_START : 0);
          break;
        }
      }
    }

    // cpu loop
    for (int scanline = 0; scanline < 144; scanline++) {
      ly = scanline;
      for (int j = 0; j < batches; j++) {
        // mode 2, oam
        check_stat_interrupt(2);
        CPU_run(cpu, 80);

        // mode 3, lcd
        check_stat_interrupt(3);
        CPU_run(cpu, 172);

        // mode 0, hblank
        check_stat_interrupt(0);
        CPU_run(cpu, 204);
      }
    }

    // vblank loop
    for (int scanline = 144; scanline < 154; scanline++) {
      ly = scanline;
      for (int j = 0; j < batches; j++) {
        // mode 1, vblank
        check_stat_interrupt(1);
        if (ly == 144) {
          // request vblank interrupt
          if_reg |= 0x01;
        }
        CPU_run(cpu, 456);
      }
    }

    // draw screen
    DISPLAY_gbmemory_to_sdl(pixels, CPU_memory(cpu) + 0x8000);
    SDL_UpdateTexture(texture, NULL, pixels, 160 * sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
    // wait for vsync
    SDL_RenderPresent(renderer);
  }
};
