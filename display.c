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

// Helper function to get tile address based on LCDC register
uint8_t *get_tile_address(uint8_t tile_index, uint8_t lcdc, uint8_t *vram) {
  // LCDC bit 4 controls which tile data table to use
  if (lcdc & 0x10) {
    // Use 0x8000-0x8FFF, tile index is unsigned (0-255)
    return vram + tile_index * 16;
  } else {
    // Use 0x8800-0x97FF, tile index is signed (-128 to 127)
    return vram + 0x1000 + ((int8_t)tile_index) * 16;
  }
}

void DISPLAY_plot_tile(uint8_t *tile, int x, int y, uint32_t *pixels,
                       uint8_t palette) {
  for (int row = 0; row < 8; row++) {
    uint8_t b0 = *tile++;
    uint8_t b1 = *tile++;
    for (int col = 0; col < 8; col++) {
      uint8_t X = x + col;
      uint8_t Y = y + row;
      if (X >= DISPLAY_WIDTH || Y >= DISPLAY_HEIGHT) {
        continue;
      }
      uint8_t colorindex = ((b0 & 0x80) >> 7) | ((b1 & 0x80) >> 6);
      // Apply palette mapping
      colorindex = (palette >> (colorindex * 2)) & 0x03;
      uint32_t color = colors[colorindex];
      pixels[Y * DISPLAY_WIDTH + X] = color;
      b0 <<= 1;
      b1 <<= 1;
    }
  }
}

void DISPLAY_plot_sprite(uint8_t *tile, int x, int y, uint32_t *pixels,
                         uint8_t attributes, uint8_t palette) {
  bool xflip = attributes & 0x20;
  bool yflip = attributes & 0x40;
  bool priority = !(attributes & 0x80); // 0 means above background

  for (int row = 0; row < 8; row++) {
    int actual_row = yflip ? 7 - row : row;
    uint8_t b0 = tile[actual_row * 2];
    uint8_t b1 = tile[actual_row * 2 + 1];

    for (int col = 0; col < 8; col++) {
      int actual_col = xflip ? 7 - col : col;
      uint8_t pixel_x = x + col;
      uint8_t pixel_y = y + row;

      if (pixel_x >= DISPLAY_WIDTH || pixel_y >= DISPLAY_HEIGHT) {
        continue;
      }

      uint8_t colorindex = ((b0 >> (7 - actual_col)) & 1) |
                           (((b1 >> (7 - actual_col)) & 1) << 1);
      if (colorindex == 0)
        continue; // Transparent pixel

      // Apply palette mapping
      colorindex = (palette >> (colorindex * 2)) & 0x03;
      uint32_t color = colors[colorindex];

      // Only draw if we have priority or the background is white
      if (priority || pixels[pixel_y * DISPLAY_WIDTH + pixel_x] == colors[0]) {
        pixels[pixel_y * DISPLAY_WIDTH + pixel_x] = color;
      }
    }
  }
}

void DISPLAY_gbmemory_to_sdl(uint32_t *pixels, CPU *cpu) {
  uint8_t *vram = CPU_memory(cpu) + 0x8000;

  // Clear the screen with background color
  for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
    pixels[i] = colors[cpu->bgp & 0x03]; // Color 0 from palette
  }

  // Don't render anything if LCD is off
  if (!(cpu->lcdc & 0x80))
    return;

  // 1. Render Background if enabled
  if (cpu->lcdc & 0x01) {
    // Select background map based on LCDC bit 3
    const uint8_t *bg_map = vram + (cpu->lcdc & 0x08 ? 0x1C00 : 0x1800);

    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
      for (int x = 0; x < DISPLAY_WIDTH; x++) {
        // Calculate position in the 256x256 background map
        uint8_t bg_x = (x + cpu->scx) & 0xFF;
        uint8_t bg_y = (y + cpu->scy) & 0xFF;

        // Get tile index from background map
        uint8_t tile_x = bg_x / 8;
        uint8_t tile_y = bg_y / 8;
        uint8_t tile_index = bg_map[tile_y * 32 + tile_x];

        // Get tile data address
        uint8_t *tile = get_tile_address(tile_index, cpu->lcdc, vram);

        // Plot single pixel from the tile
        uint8_t tile_pixel_x = bg_x % 8;
        uint8_t tile_pixel_y = bg_y % 8;

        uint8_t b0 = tile[tile_pixel_y * 2];
        uint8_t b1 = tile[tile_pixel_y * 2 + 1];

        uint8_t colorindex = ((b0 >> (7 - tile_pixel_x)) & 1) |
                             (((b1 >> (7 - tile_pixel_x)) & 1) << 1);

        // Apply palette mapping
        colorindex = (cpu->bgp >> (colorindex * 2)) & 0x03;
        pixels[y * DISPLAY_WIDTH + x] = colors[colorindex];
      }
    }
  }

  // 2. Render Window if enabled
  if ((cpu->lcdc & 0x20) && cpu->wx <= 166 && cpu->wy < DISPLAY_HEIGHT) {
    // Select window map based on LCDC bit 6
    const uint8_t *win_map = vram + (cpu->lcdc & 0x40 ? 0x1C00 : 0x1800);

    for (int y = 0; y < DISPLAY_HEIGHT - cpu->wy; y++) {
      if (y + cpu->wy >= DISPLAY_HEIGHT)
        break;

      for (int x = 0; x < DISPLAY_WIDTH - (cpu->wx - 7); x++) {
        if (x + cpu->wx - 7 < 0 || x + cpu->wx - 7 >= DISPLAY_WIDTH)
          continue;

        // Calculate position in the window
        uint8_t win_x = x;
        uint8_t win_y = y;

        // Get tile index from window map
        uint8_t tile_x = win_x / 8;
        uint8_t tile_y = win_y / 8;
        uint8_t tile_index = win_map[tile_y * 32 + tile_x];

        // Get tile data address
        uint8_t *tile = get_tile_address(tile_index, cpu->lcdc, vram);

        // Plot single pixel from the tile
        uint8_t tile_pixel_x = win_x % 8;
        uint8_t tile_pixel_y = win_y % 8;

        uint8_t b0 = tile[tile_pixel_y * 2];
        uint8_t b1 = tile[tile_pixel_y * 2 + 1];

        uint8_t colorindex = ((b0 >> (7 - tile_pixel_x)) & 1) |
                             (((b1 >> (7 - tile_pixel_x)) & 1) << 1);

        // Apply palette mapping
        colorindex = (cpu->bgp >> (colorindex * 2)) & 0x03;
        pixels[(y + cpu->wy) * DISPLAY_WIDTH + (x + cpu->wx - 7)] =
            colors[colorindex];
      }
    }
  }

  // 3. Render Sprites if enabled
  if (cpu->lcdc & 0x02) {
    uint8_t sprite_height = (cpu->lcdc & 0x04) ? 16 : 8;
    uint8_t *oam = cpu->_memory + 0xFE00; // Object Attribute Memory

    // Process sprites in priority order (lower x has higher priority)
    for (int sprite = 0; sprite < 40; sprite++) {
      uint8_t y = oam[sprite * 4] - 16;         // Y position
      uint8_t x = oam[sprite * 4 + 1] - 8;      // X position
      uint8_t tile_index = oam[sprite * 4 + 2]; // Tile index
      uint8_t attributes = oam[sprite * 4 + 3]; // Attributes

      // Skip if sprite is off-screen
      if (y >= DISPLAY_HEIGHT || x >= DISPLAY_WIDTH)
        continue;

      // For 8x16 sprites, last bit of tile index is ignored
      if (sprite_height == 16) {
        tile_index &= 0xFE;
      }

      uint8_t *tile = vram + tile_index * 16;
      uint8_t palette = (attributes & 0x10) ? cpu->obp1 : cpu->obp0;

      DISPLAY_plot_sprite(tile, x, y, pixels, attributes, palette);

      // For 8x16 sprites, draw the bottom half
      if (sprite_height == 16) {
        DISPLAY_plot_sprite(vram + (tile_index + 1) * 16, x, y + 8, pixels,
                            attributes, palette);
      }
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
