#include <stdint.h>

#ifndef CPU_H
#define CPU_H

typedef struct CPU CPU;

CPU *CPU_new();
void CPU_read_rom(CPU *cpu, const char *filename);
unsigned char *CPU_memory(CPU *cpu);
void CPU_run(CPU *cpu, int);
void CPU_hardware(CPU *cpu, void (*hw_write)(uint16_t address, uint8_t val),
                  uint8_t (*hw_read)(uint16_t address));

#endif // CPU_H
