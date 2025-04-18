#pragma once

#include <stddef.h>
#include <stdint.h>

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

Cartridge *cart_load(const char *filename);
void cart_free(Cartridge *cart);
uint8_t cart_read(Cartridge *cart, uint16_t addr);
void cart_write(Cartridge *cart, uint16_t addr, uint8_t val);
