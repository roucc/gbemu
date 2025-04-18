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

  // Memory
  uint8_t _memory[65536];

  // Hardware hooks
  void (*hw_write)(uint16_t, uint8_t);
  uint8_t (*hw_read)(uint16_t);

  // Interrupts
  uint8_t IME;
  uint8_t pending_IME;
  uint64_t cycle_count;

  // Cartridge
  Cartridge *cart;
} CPU;

// Interface
CPU *CPU_new();
void CPU_run(CPU *cpu, int);
void CPU_hardware(CPU *cpu, void (*hw_write)(uint16_t, uint8_t),
                  uint8_t (*hw_read)(uint16_t));
uint8_t *CPU_memory(CPU *cpu);

#endif // CPU_H
