#ifndef CPU_H
#define CPU_H

#include "cartridge.h" // Needed for Cartridge*
#include <stdint.h>

typedef struct CPU {
  // Registers
  union {
    struct {
      uint8_t C, B;
    };
    uint16_t BC;
  };
  union {
    struct {
      uint8_t E, D;
    };
    uint16_t DE;
  };
  union {
    struct {
      uint8_t L, H;
    };
    uint16_t HL;
  };
  union {
    struct {
      uint8_t F, A;
    };
    uint16_t AF;
  };
  uint16_t SP, PC;

  // Memory mapped IO

  // interrupt registers
  uint8_t ly; // current scan line
  uint8_t direction_state;
  uint8_t button_state;
  uint8_t joyp;
  uint8_t if_reg;
  uint8_t ie_reg;

  // display registers
  uint8_t lcdc; // 0xFF40 – LCD control
  uint8_t scy;  // 0xFF42 – SCY
  uint8_t scx;  // 0xFF43 – SCX
  uint8_t wy;   // 0xFF4A – WY
  uint8_t wx;   // 0xFF4B – WX
  uint8_t bgp;  // 0xFF47 – BG palette data
  uint8_t obp0; // 0xFF48 – OBJ palette 0 data
  uint8_t obp1; // 0xFF49 – OBJ palette 1 data

  // Timer registers
  uint8_t divr; // 0xFF04 – Divider (increments every 256 cycles)
  uint8_t tima; // 0xFF05 – Timer counter
  uint8_t tma;  // 0xFF06 – Timer modulo (reload value)
  uint8_t tac;  // 0xFF07 – Timer control

  // LCD status registers
  uint8_t stat; // 0xFF41 – LCD STAT
  uint8_t lyc;  // 0xFF45 – LYC compare value

  // Memory
  uint8_t _memory[65536];

  // Other
  uint8_t IME;
  uint8_t pending_IME;
  uint64_t cycle_count;
  uint8_t halted;

  // Cartridge
  Cartridge *cart;
} CPU;

// Interface
CPU *CPU_new();
void CPU_run(CPU *cpu, int);
uint8_t *CPU_memory(CPU *cpu);
uint8_t CPU_read_memory(CPU *cpu, uint16_t address);
void CPU_write_memory(CPU *cpu, uint16_t addr, uint8_t val);
uint8_t *CPU_io_pointer(CPU *cpu, uint16_t address);
void CPU_check_stat_interrupt(CPU *cpu, uint8_t mode);
void CPU_display(CPU *cpu);

#endif // CPU_H
