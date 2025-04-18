#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROM_SIZE (2 * 1024 * 1024)
#define MAX_RAM_SIZE (32 * 1024)

typedef struct {
  uint8_t *rom;
  size_t rom_size;
  uint8_t ram[MAX_RAM_SIZE];

  uint8_t rom_bank;
  uint8_t ram_bank;
  uint8_t ram_enable;
  uint8_t banking_mode;
} Cartridge;

Cartridge *cart_load(const char *filename) {
  Cartridge *cart = calloc(1, sizeof(Cartridge));
  if (!cart)
    return NULL;

  FILE *f = fopen(filename, "rb");
  if (!f) {
    free(cart);
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  cart->rom_size = ftell(f);
  rewind(f);

  if (cart->rom_size > MAX_ROM_SIZE) {
    fclose(f);
    free(cart);
    return NULL;
  }

  cart->rom = malloc(cart->rom_size);
  if (!cart->rom) {
    fclose(f);
    free(cart);
    return NULL;
  }

  if (fread(cart->rom, 1, cart->rom_size, f) != cart->rom_size) {
    fprintf(stderr, "error: failed to fully read ROM\n");
    free(cart->rom);
    free(cart);
    fclose(f);
    return NULL;
  }

  fclose(f);
  cart->rom_bank = 1;
  return cart;
}

uint8_t cart_read(Cartridge *cart, uint16_t addr) {
  if (addr < 0x4000) {
    return cart->rom[addr];
  } else if (addr < 0x8000) {
    size_t offset = (cart->rom_bank & 0x1F) * 0x4000 + (addr - 0x4000);
    return cart->rom[offset % cart->rom_size];
  } else if (addr >= 0xA000 && addr < 0xC000 && cart->ram_enable) {
    size_t offset =
        (cart->banking_mode ? cart->ram_bank : 0) * 0x2000 + (addr - 0xA000);
    return cart->ram[offset % MAX_RAM_SIZE];
  }
  return 0xFF;
}

void cart_write(Cartridge *cart, uint16_t addr, uint8_t val) {
  if (addr < 0x2000) {
    cart->ram_enable = (val & 0x0F) == 0x0A;
  } else if (addr < 0x4000) {
    uint8_t bank = val & 0x1F;
    cart->rom_bank = bank ? bank : 1;
  } else if (addr < 0x6000) {
    if (cart->banking_mode)
      cart->ram_bank = val & 0x03;
    else
      cart->rom_bank = (cart->rom_bank & 0x1F) | ((val & 0x03) << 5);
  } else if (addr < 0x8000) {
    cart->banking_mode = val & 0x01;
  } else if (addr >= 0xA000 && addr < 0xC000 && cart->ram_enable) {
    size_t offset =
        (cart->banking_mode ? cart->ram_bank : 0) * 0x2000 + (addr - 0xA000);
    cart->ram[offset % MAX_RAM_SIZE] = val;
  }
}

void cart_free(Cartridge *cart) {
  if (!cart)
    return;
  free(cart->rom);
  free(cart);
}
