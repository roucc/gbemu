#include "cpu.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// define bits of F register
#define F_z 0x80
#define F_n 0x40 // subtraction flag (BCD)
#define F_h 0x20 // half carry flag (BCD)
#define F_c 0x10

// cpu struct
typedef struct CPU {
  // define registers in cpu
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

  // global interrupt flag
  uint8_t IME;
  uint8_t pending_IME;

  // global cycle count
  uint8_t cycle_count;

  // memory
  uint8_t _memory[65536];
  void (*hw_write)(uint16_t address, uint8_t val);
  uint8_t (*hw_read)(uint16_t address);
} CPU;

CPU *CPU_new() {
  CPU *cpu = calloc(1, sizeof(CPU));
  cpu->SP = 0xfffe;
  cpu->PC = 0x0100;
  cpu->IME = 0;
  cpu->pending_IME = 0;
  cpu->cycle_count = 0;
  cpu->hw_write = NULL;
  cpu->hw_read = NULL;
  return cpu;
}

void CPU_hardware(CPU *cpu, void (*hw_write)(uint16_t address, uint8_t val),
                  uint8_t (*hw_read)(uint16_t address)) {
  cpu->hw_read = hw_read;
  cpu->hw_write = hw_write;
}

void CPU_display(CPU *cpu) {
  printf("A=%02x\n", cpu->A);
  printf("B=%02x\n", cpu->B);
  printf("C=%02x\n", cpu->C);
  printf("D=%02x\n", cpu->D);
  printf("E=%02x\n", cpu->E);
  printf("H=%02x\n", cpu->H);
  printf("L=%02x\n", cpu->L);
  printf("F=%02x\n", cpu->F);
  printf("SP=%04x\n", cpu->SP);
  printf("PC=%04x\n", cpu->PC);
  printf("\n");
}

void CPU_core_dump(CPU *cpu) {
  FILE *f = fopen("core_dump.bin", "wb");
  fwrite(cpu->_memory, sizeof(cpu->_memory), 1, f);
  fclose(f);
  printf("core dumped to core_dump.bin\n");
}

uint8_t *CPU_memory(CPU *cpu) { return cpu->_memory; };

uint8_t CPU_read_memory(CPU *cpu, uint16_t src) {
  if (cpu->hw_read != NULL &&
      ((src >= 0xFF00 && src <= 0xFF7F) || src == 0xFFFF)) {
    return cpu->hw_read(src);
  }
  return cpu->_memory[src];
}

void CPU_write_memory(CPU *cpu, uint16_t dst, uint8_t val) {
  if (cpu->hw_write != NULL &&
      ((dst >= 0xFF00 && dst <= 0xFF7F) || dst == 0xFFFF)) {
    return cpu->hw_write(dst, val);
  }
  cpu->_memory[dst] = val;
}

// register sets:

uint8_t *CPU_r8(CPU *cpu, uint8_t n) {
  switch (n) {
  case 0:
    return &cpu->B;
  case 1:
    return &cpu->C;
  case 2:
    return &cpu->D;
  case 3:
    return &cpu->E;
  case 4:
    return &cpu->H;
  case 5:
    return &cpu->L;
  case 6:
    return &cpu->_memory[cpu->H << 8 | cpu->L];
    // FIXME: urm
  case 7:
    return &cpu->A;
  default:
    return NULL;
  }
};

uint16_t *CPU_r16(CPU *cpu, uint8_t n) {
  switch (n) {
  case 0: {
    return &cpu->BC;
  }
  case 1: {
    return &cpu->DE;
  }
  case 2: {
    return &cpu->HL;
  }
  case 3: {
    return &cpu->SP;
  }
  default: {
    return NULL;
  }
  }
};

uint16_t *CPU_r16stk(CPU *cpu, uint8_t n) {
  switch (n) {
  case 0: {
    return &cpu->BC;
  }
  case 1: {
    return &cpu->DE;
  }
  case 2: {
    return &cpu->HL;
  }
  case 3: {
    return &cpu->AF;
  }
  default: {
    return NULL;
  }
  }
};

uint16_t *CPU_r16mem(CPU *cpu, uint8_t n) {
  switch (n) {
  case 0: {
    return &cpu->BC;
  }
  case 1: {
    return &cpu->DE;
  }
  case 2: {
    uint16_t *hl = &cpu->HL;
    return hl;
  }
  case 3: {
    uint16_t *hl = &cpu->HL;
    return hl;
  }
  default: {
    return NULL;
  }
  }
};

void CPU_r16mem_post(CPU *cpu, uint8_t n) {
  switch (n) {
  case 2: {
    cpu->HL++;
    break;
  }
  case 3: {
    cpu->HL--;
    break;
  }
  default: {
    return;
  }
  };
};

uint8_t CPU_cond(CPU *cpu, uint8_t cond) {
  switch (cond) {
  case 0: {
    return !(cpu->F & F_z);
  };
  case 1: {
    return (cpu->F & F_z);
  };
  case 2: {
    return !(cpu->F & F_c);
  };
  case 3: {
    return (cpu->F & F_c);
  };
  default:
    return 0;
  };
}

uint8_t CPU_imm8(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  return CPU_read_memory(cpu, cpu->PC++);
};

uint16_t CPU_imm16(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  uint8_t lo = CPU_read_memory(cpu, cpu->PC++);
  uint8_t hi = CPU_read_memory(cpu, cpu->PC++);
  return (((uint16_t)hi) << 8) | (uint16_t)lo;
};

// block 0 instructions:

void CPU_nop(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  (void)cpu;
};

// TODO: stop
void CPU_stop(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  (void)cpu;
};

void CPU_LD_r16_imm16(CPU *cpu, uint8_t opcode) {
  uint16_t *dst = CPU_r16(cpu, (opcode >> 4) & 0x03);
  uint16_t src = CPU_imm16(cpu, opcode);
  *dst = src;
};

void CPU_LD_indirectr16mem_A(CPU *cpu, uint8_t opcode) {
  uint16_t *dst = CPU_r16mem(cpu, (opcode >> 4) & 0x03);
  CPU_write_memory(cpu, *dst, cpu->A);
  CPU_r16mem_post(cpu, (opcode >> 4) & 0x03);
};

void CPU_LD_A_indirectr16mem(CPU *cpu, uint8_t opcode) {
  uint16_t *src = CPU_r16mem(cpu, (opcode >> 4) & 0x03);
  cpu->A = CPU_read_memory(cpu, *src);
  CPU_r16mem_post(cpu, (opcode >> 4) & 0x03);
};

void CPU_LD_indirectimm16_SP(CPU *cpu, uint8_t opcode) {
  uint16_t dst = CPU_imm16(cpu, opcode);
  CPU_write_memory(cpu, dst, cpu->SP & 0xFF);
  CPU_write_memory(cpu, dst + 1, cpu->SP >> 8);
}

void CPU_inc_r16(CPU *cpu, uint8_t opcode) {
  uint16_t *src = CPU_r16(cpu, (opcode >> 4) & 0x03);
  (*src)++;
}

void CPU_dec_r16(CPU *cpu, uint8_t opcode) {
  uint16_t *src = CPU_r16(cpu, (opcode >> 4) & 0x03);
  (*src)--;
};

void CPU_add_hl_r16(CPU *cpu, uint8_t opcode) {
  uint16_t *r16 = CPU_r16(cpu, (opcode >> 4) & 0x03);
  uint16_t oldHL = cpu->HL;
  uint32_t result = cpu->HL + *r16;
  uint8_t zflag = cpu->F & F_z; // preserve Z flag
  cpu->F = zflag;               // clear N, H, C; Z remains
  cpu->F &= ~F_n;
  if (((oldHL & 0xFFF) + (*r16 & 0xFFF)) > 0xFFF)
    cpu->F |= F_h;
  if (result > 0xFFFF)
    cpu->F |= F_c;
  cpu->HL = result & 0xFFFF;
}

void CPU_inc_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, (opcode >> 3) & 0x07);
  uint8_t carry = cpu->F & F_c;
  uint8_t result = *src + 1;
  uint8_t half = ((*src & 0xF) == 0xF) ? F_h : 0;
  *src = result;
  cpu->F = carry; // preserve C
  if (result == 0)
    cpu->F |= F_z;
  cpu->F |= half;
  cpu->F &= ~F_n;
}

void CPU_dec_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, (opcode >> 3) & 0x07);
  uint8_t carry = cpu->F & F_c;
  uint8_t result = *src - 1;
  uint8_t half = ((*src & 0xF) == 0) ? F_h : 0;
  *src = result;
  cpu->F = carry; // preserve C
  if (result == 0)
    cpu->F |= F_z;
  cpu->F |= F_n;
  cpu->F |= half;
}

void CPU_LD_r8_imm8(CPU *cpu, uint8_t opcode) {
  uint8_t *dst = CPU_r8(cpu, (opcode >> 3) & 0x07);
  *dst = CPU_imm8(cpu, opcode);
};

void CPU_rlca(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  uint8_t msb = (cpu->A & 0x80) >> 7;
  cpu->A = (cpu->A << 1) | msb;
  cpu->F = msb ? F_c : 0;
}

void CPU_rrca(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  uint8_t lsb = cpu->A & 0x01;
  cpu->A = (cpu->A >> 1) | (lsb << 7);
  cpu->F = lsb ? F_c : 0;
}

void CPU_rla(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  uint8_t oldCarry = (cpu->F & F_c) ? 1 : 0;
  uint8_t msb = (cpu->A & 0x80) >> 7;
  cpu->A = (cpu->A << 1) | oldCarry;
  cpu->F = msb ? F_c : 0;
}

void CPU_rra(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  uint8_t oldCarry = (cpu->F & F_c) ? 1 : 0;
  uint8_t lsb = cpu->A & 0x01;
  cpu->A = (cpu->A >> 1) | (oldCarry << 7);
  cpu->F = lsb ? F_c : 0;
}

void CPU_daa(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  uint8_t correction = 0;
  uint8_t a = cpu->A;
  if (!(cpu->F & F_n)) { // after addition
    if (cpu->F & F_h || (a & 0x0F) > 9)
      correction |= 0x06;
    if (cpu->F & F_c || a > 0x99) {
      correction |= 0x60;
      cpu->F |= F_c;
    }
    a += correction;
  } else { // after subtraction
    if (cpu->F & F_h)
      correction |= 0x06;
    if (cpu->F & F_c)
      correction |= 0x60;
    a -= correction;
  }
  cpu->A = a;
  cpu->F &= ~F_h;
  if (cpu->A == 0)
    cpu->F |= F_z;
}

void CPU_cpl(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  cpu->A = ~cpu->A;
  cpu->F |= F_n | F_h;
};

void CPU_scf(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  cpu->F |= F_c;
};

void CPU_ccf(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  if (cpu->F & F_c) {
    cpu->F &= ~F_c;
  } else {
    cpu->F |= F_c;
  };
};

void CPU_jr_imm8(CPU *cpu, uint8_t opcode) {
  int8_t offset = (int8_t)CPU_imm8(cpu, opcode);
  cpu->PC += offset;
};

void CPU_jr_cond(CPU *cpu, uint8_t opcode) {
  int8_t offset = (int8_t)CPU_imm8(cpu, opcode);
  if (CPU_cond(cpu, (opcode >> 3) & 0x03))
    cpu->PC += offset;
}

void CPU_push_r16stk(CPU *cpu, uint8_t opcode) {
  uint16_t *src = CPU_r16stk(cpu, (opcode >> 4) & 0x03);
  CPU_write_memory(cpu, --cpu->SP, (*src >> 8) & 0xFF);
  CPU_write_memory(cpu, --cpu->SP, *src & 0xFF);
};

void CPU_pop_r16stk(CPU *cpu, uint8_t opcode) {
  uint16_t *dst = CPU_r16stk(cpu, (opcode >> 4) & 0x03);
  *dst = CPU_read_memory(cpu, cpu->SP++);
  // *dst |= cpu->memory[cpu->SP++] << 8;
  *dst |= CPU_read_memory(cpu, cpu->SP++) << 8;
};

// block 1 instructions:

void CPU_halt(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  // CPU_display(cpu);
  // CPU_core_dump(cpu);

  cpu->PC--;
};

void CPU_LD_r8_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t *dst = CPU_r8(cpu, (opcode >> 3) & 0x07);
  *dst = *src;
};

// block 2 instructions:

void CPU_add_a_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint16_t result = cpu->A + *src;
  uint8_t carry = cpu->F & F_c; // preserve C flag
  cpu->F = carry;               // clear Z, N, H then restore C
  if ((result & 0xFF) == 0)
    cpu->F |= F_z;
  if (((cpu->A & 0xF) + (*src & 0xF)) > 0xF)
    cpu->F |= F_h;
  if (result > 0xFF)
    cpu->F |= F_c;
  cpu->A = result & 0xFF;
}

void CPU_adc_a_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t carry = (cpu->F & F_c) ? 1 : 0;
  uint16_t result = cpu->A + *src + carry;
  cpu->F = 0;
  if ((result & 0xFF) == 0)
    cpu->F |= F_z;
  if (((cpu->A & 0xF) + (*src & 0xF) + carry) > 0xF)
    cpu->F |= F_h;
  if (result > 0xFF)
    cpu->F |= F_c;
  cpu->A = result & 0xFF;
}

void CPU_sbc_a_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t carry = (cpu->F & F_c) ? 1 : 0;
  uint16_t result = cpu->A - (*src + carry);
  cpu->F = F_n;
  if ((result & 0xFF) == 0)
    cpu->F |= F_z;
  if ((cpu->A & 0xF) < ((*src & 0xF) + carry))
    cpu->F |= F_h;
  if (cpu->A < (*src + carry))
    cpu->F |= F_c;
  cpu->A = result & 0xFF;
}

void CPU_cp_a_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t result = cpu->A - *src;
  cpu->F = F_n;
  if (result == 0)
    cpu->F |= F_z;
  if ((cpu->A & 0xF) < (*src & 0xF))
    cpu->F |= F_h;
  if (cpu->A < *src)
    cpu->F |= F_c;
}

void CPU_sub_a_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint16_t result = cpu->A - *src;
  cpu->F = F_n; // subtraction: set N
  if ((result & 0xFF) == 0)
    cpu->F |= F_z;
  if ((cpu->A & 0xF) < (*src & 0xF))
    cpu->F |= F_h;
  if (cpu->A < *src)
    cpu->F |= F_c;
  cpu->A = result & 0xFF;
}

void CPU_and_a_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  cpu->A &= *src;
  cpu->F = (cpu->A == 0 ? F_z : 0) | F_h;
}

void CPU_xor_a_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  cpu->A ^= *src;
  cpu->F = (cpu->A == 0) ? F_z : 0; // clear N, H, C
}

void CPU_or_a_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  cpu->A |= *src;
  cpu->F = (cpu->A == 0) ? F_z : 0;
}

// block 3 instructions:

void CPU_add_a_imm8(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  uint16_t result = cpu->A + src;
  uint8_t carry = cpu->F & F_c;
  cpu->F = carry;
  if ((result & 0xFF) == 0)
    cpu->F |= F_z;
  if (((cpu->A & 0xF) + (src & 0xF)) > 0xF)
    cpu->F |= F_h;
  if (result > 0xFF)
    cpu->F |= F_c;
  cpu->A = result & 0xFF;
}

void CPU_adc_a_imm8(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  uint8_t carry = (cpu->F & F_c) ? 1 : 0;
  uint16_t result = cpu->A + src + carry;
  cpu->F = 0;
  if ((result & 0xFF) == 0)
    cpu->F |= F_z;
  if (((cpu->A & 0xF) + (src & 0xF) + carry) > 0xF)
    cpu->F |= F_h;
  if (result > 0xFF)
    cpu->F |= F_c;
  cpu->A = result & 0xFF;
}

void CPU_sbc_a_imm8(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  uint8_t carry = (cpu->F & F_c) ? 1 : 0;
  uint16_t result = cpu->A - (src + carry);
  cpu->F = F_n;
  if ((result & 0xFF) == 0)
    cpu->F |= F_z;
  if ((cpu->A & 0xF) < ((src & 0xF) + carry))
    cpu->F |= F_h;
  if (cpu->A < (src + carry))
    cpu->F |= F_c;
  cpu->A = result & 0xFF;
}

void CPU_sub_a_imm8(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  uint16_t result = cpu->A - src;
  cpu->F = F_n;
  if ((result & 0xFF) == 0)
    cpu->F |= F_z;
  if ((cpu->A & 0xF) < (src & 0xF))
    cpu->F |= F_h;
  if (cpu->A < src)
    cpu->F |= F_c;
  cpu->A = result & 0xFF;
}

void CPU_and_a_imm8(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  cpu->A &= src;
  cpu->F = (cpu->A == 0 ? F_z : 0) | F_h;
}

void CPU_xor_a_imm8(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  cpu->A ^= src;
  cpu->F = (cpu->A == 0) ? F_z : 0;
}

void CPU_or_a_imm8(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  cpu->A |= src;
  cpu->F = (cpu->A == 0) ? F_z : 0;
}

void CPU_cp_a_imm8(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  uint8_t result = cpu->A - src;
  cpu->F = F_n;
  if (result == 0)
    cpu->F |= F_z;
  if ((cpu->A & 0xF) < (src & 0xF))
    cpu->F |= F_h;
  if (cpu->A < src)
    cpu->F |= F_c;
}

void CPU_ret_cond(CPU *cpu, uint8_t opcode) {
  if (CPU_cond(cpu, (opcode >> 3) & 0x03)) {
    cpu->PC = CPU_read_memory(cpu, cpu->SP++);
    cpu->PC |= CPU_read_memory(cpu, cpu->SP++) << 8;
  };
};

void CPU_ret(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  cpu->PC = CPU_read_memory(cpu, cpu->SP++);
  cpu->PC |= CPU_read_memory(cpu, cpu->SP++) << 8;
};

// void CPU_reti(CPU *cpu, uint8_t opcode) {
//   (void)opcode;
//   cpu->PC = CPU_read_memory(cpu, cpu->SP++);
//   cpu->PC |= CPU_read_memory(cpu, cpu->SP++) << 8;
//   cpu->IME = 1;
// };
void CPU_reti(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  uint8_t low = CPU_read_memory(cpu, cpu->SP++);
  uint8_t high = CPU_read_memory(cpu, cpu->SP++);
  cpu->PC = (high << 8) | low;
  cpu->IME = 1;
}

void CPU_jp_cond_imm16(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  if (CPU_cond(cpu, (opcode >> 3) & 0x03)) {
    cpu->PC = CPU_imm16(cpu, opcode);
  } else {
    cpu->PC += 2;
  }
};

void CPU_jp_imm16(CPU *cpu, uint8_t opcode) {
  cpu->PC = CPU_imm16(cpu, opcode);
};

void CPU_jp_hl(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  cpu->PC = cpu->HL;
};

void CPU_call_cond_imm16(CPU *cpu, uint8_t opcode) {
  uint16_t addr = CPU_imm16(cpu, opcode);
  if (CPU_cond(cpu, (opcode >> 3) & 0x03)) {
    CPU_write_memory(cpu, --cpu->SP, (cpu->PC >> 8) & 0xFF);
    CPU_write_memory(cpu, --cpu->SP, cpu->PC & 0xFF);
    cpu->PC = addr;
  }
}

void CPU_call_imm16(CPU *cpu, uint8_t opcode) {
  uint16_t addr = CPU_imm16(cpu, opcode);
  CPU_write_memory(cpu, --cpu->SP, (cpu->PC >> 8) & 0xFF);
  CPU_write_memory(cpu, --cpu->SP, cpu->PC & 0xFF);
  cpu->PC = addr;
}

void CPU_di(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  cpu->IME = 0;
}

void CPU_ei(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  cpu->pending_IME = 1;
}

void CPU_rst(CPU *cpu, uint8_t opcode) {
  // equivalent to saying call tgt3 * 8
  uint8_t tgt3 = opcode & 0x38;
  CPU_write_memory(cpu, --cpu->SP, (cpu->PC >> 8) & 0xFF);
  CPU_write_memory(cpu, --cpu->SP, cpu->PC & 0xFF);
  cpu->PC = tgt3;
};

void CPU_ldh_indirectc_a(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  CPU_write_memory(cpu, 0xFF00 + cpu->C, cpu->A);
};

void CPU_ldh_indirectimm8_a(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  CPU_write_memory(cpu, 0xFF00 + src, cpu->A);
};

void CPU_ld_a_indirectc(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  cpu->A = CPU_read_memory(cpu, 0xFF00 + cpu->C);
};

void CPU_ld_a_indirectimm8(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  cpu->A = CPU_read_memory(cpu, 0xFF00 + src);
};

void CPU_ld_indirectimm16_a(CPU *cpu, uint8_t opcode) {
  uint16_t dst = CPU_imm16(cpu, opcode);
  CPU_write_memory(cpu, dst, cpu->A);
};

void CPU_ld_a_indirectimm16(CPU *cpu, uint8_t opcode) {
  uint16_t src = CPU_imm16(cpu, opcode);
  cpu->A = CPU_read_memory(cpu, src);
};

void CPU_add_sp_imm8(CPU *cpu, uint8_t opcode) {
  int8_t offset = (int8_t)CPU_imm8(cpu, opcode);
  uint16_t oldSP = cpu->SP;
  cpu->SP += offset;
  cpu->F = 0; // clear Z and N
  if (((oldSP & 0xF) + (offset & 0xF)) > 0xF)
    cpu->F |= F_h;
  if (((oldSP & 0xFF) + (offset & 0xFF)) > 0xFF)
    cpu->F |= F_c;
}

void CPU_ld_hl_sp_imm8(CPU *cpu, uint8_t opcode) {
  int8_t offset = (int8_t)CPU_imm8(cpu, opcode);
  uint16_t result = cpu->SP + offset;
  cpu->HL = result;
  cpu->F = 0; // clear Z and N
  if (((cpu->SP & 0xF) + (((uint8_t)offset) & 0xF)) > 0xF)
    cpu->F |= F_h;
  if (((cpu->SP & 0xFF) + ((uint8_t)offset)) > 0xFF)
    cpu->F |= F_c;
}

void CPU_ld_sp_hl(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  cpu->SP = cpu->HL;
};

void CPU_ldh_a_indirectc(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  cpu->A = CPU_read_memory(cpu, 0xFF00 + cpu->C);
};

void CPU_ldh_a_indirectimm8(CPU *cpu, uint8_t opcode) {
  uint8_t src = CPU_imm8(cpu, opcode);
  cpu->A = CPU_read_memory(cpu, 0xFF00 + src);
};

// prefix instructions:

void CPU_rlc_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t c = *src & 0x80;
  *src = (*src << 1) | (c >> 7);
  if (c) {
    cpu->F |= F_c;
  } else {
    cpu->F &= ~F_c;
  };
};

void CPU_rrc_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t c = *src & 0x01;
  *src = (*src >> 1) | (c << 7);
  if (c) {
    cpu->F |= F_c;
  } else {
    cpu->F &= ~F_c;
  };
};

void CPU_rl_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *reg = CPU_r8(cpu, opcode & 0x07);
  uint8_t oldCarry = (cpu->F & F_c) ? 1 : 0;
  uint8_t msb = (*reg & 0x80) >> 7;
  *reg = (*reg << 1) | oldCarry;
  cpu->F = ((*reg == 0) ? F_z : 0) | (msb ? F_c : 0);
}

void CPU_rr_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *reg = CPU_r8(cpu, opcode & 0x07);
  uint8_t oldCarry = (cpu->F & F_c) ? 1 : 0;
  uint8_t lsb = *reg & 0x01;
  *reg = (*reg >> 1) | (oldCarry << 7);
  cpu->F = ((*reg == 0) ? F_z : 0) | (lsb ? F_c : 0);
}

void CPU_sla_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t c = *src & 0x80;
  *src = *src << 1;
  if (c) {
    cpu->F |= F_c;
  } else {
    cpu->F &= ~F_c;
  };
};

void CPU_sra_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t c = *src & 0x01;
  *src = (*src >> 1) | (*src & 0x80);
  if (c) {
    cpu->F |= F_c;
  } else {
    cpu->F &= ~F_c;
  };
};

void CPU_swap_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  *src = (*src >> 4) | (*src << 4);
  if (*src == 0) {
    cpu->F |= F_z;
  } else {
    cpu->F &= ~F_z;
  };
};

void CPU_srl_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t c = *src & 0x01;
  *src = *src >> 1;
  if (c) {
    cpu->F |= F_c;
  } else {
    cpu->F &= ~F_c;
  };
};

void CPU_bit_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t bit = (opcode >> 3) & 0x07;
  if (!(*src & (1 << bit))) {
    cpu->F |= F_z;
  } else {
    cpu->F &= ~F_z;
  };
  cpu->F &= ~F_n;
  cpu->F |= F_h;
};

void CPU_res_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t bit = (opcode >> 3) & 0x07;
  *src &= ~(1 << bit);
};

void CPU_set_r8(CPU *cpu, uint8_t opcode) {
  uint8_t *src = CPU_r8(cpu, opcode & 0x07);
  uint8_t bit = (opcode >> 3) & 0x07;
  *src |= (1 << bit);
};

// invalid insturction:

void CPU_invalid(CPU *cpu, uint8_t opcode) {
  (void)cpu;
  printf("invalid instruction: %02x\n", opcode);
  exit(1);
};

typedef void (*OpcodeHandler)(CPU *, uint8_t opcode);

static OpcodeHandler prefixTable[256] = {
    [0x00] = CPU_rlc_r8,  [0x01] = CPU_rlc_r8,  [0x02] = CPU_rlc_r8,
    [0x03] = CPU_rlc_r8,  [0x04] = CPU_rlc_r8,  [0x05] = CPU_rlc_r8,
    [0x06] = CPU_rlc_r8,  [0x07] = CPU_rlc_r8,  [0x08] = CPU_rrc_r8,
    [0x09] = CPU_rrc_r8,  [0x0A] = CPU_rrc_r8,  [0x0B] = CPU_rrc_r8,
    [0x0C] = CPU_rrc_r8,  [0x0D] = CPU_rrc_r8,  [0x0E] = CPU_rrc_r8,
    [0x0F] = CPU_rrc_r8,  [0x10] = CPU_rl_r8,   [0x11] = CPU_rl_r8,
    [0x12] = CPU_rl_r8,   [0x13] = CPU_rl_r8,   [0x14] = CPU_rl_r8,
    [0x15] = CPU_rl_r8,   [0x16] = CPU_rl_r8,   [0x17] = CPU_rl_r8,
    [0x18] = CPU_rr_r8,   [0x19] = CPU_rr_r8,   [0x1A] = CPU_rr_r8,
    [0x1B] = CPU_rr_r8,   [0x1C] = CPU_rr_r8,   [0x1D] = CPU_rr_r8,
    [0x1E] = CPU_rr_r8,   [0x1F] = CPU_rr_r8,   [0x20] = CPU_sla_r8,
    [0x21] = CPU_sla_r8,  [0x22] = CPU_sla_r8,  [0x23] = CPU_sla_r8,
    [0x24] = CPU_sla_r8,  [0x25] = CPU_sla_r8,  [0x26] = CPU_sla_r8,
    [0x27] = CPU_sla_r8,  [0x28] = CPU_sra_r8,  [0x29] = CPU_sra_r8,
    [0x2A] = CPU_sra_r8,  [0x2B] = CPU_sra_r8,  [0x2C] = CPU_sra_r8,
    [0x2D] = CPU_sra_r8,  [0x2E] = CPU_sra_r8,  [0x2F] = CPU_sra_r8,
    [0x30] = CPU_swap_r8, [0x31] = CPU_swap_r8, [0x32] = CPU_swap_r8,
    [0x33] = CPU_swap_r8, [0x34] = CPU_swap_r8, [0x35] = CPU_swap_r8,
    [0x36] = CPU_swap_r8, [0x37] = CPU_swap_r8, [0x38] = CPU_srl_r8,
    [0x39] = CPU_srl_r8,  [0x3A] = CPU_srl_r8,  [0x3B] = CPU_srl_r8,
    [0x3C] = CPU_srl_r8,  [0x3D] = CPU_srl_r8,  [0x3E] = CPU_srl_r8,
    [0x3F] = CPU_srl_r8,  [0x40] = CPU_bit_r8,  [0x41] = CPU_bit_r8,
    [0x42] = CPU_bit_r8,  [0x43] = CPU_bit_r8,  [0x44] = CPU_bit_r8,
    [0x45] = CPU_bit_r8,  [0x46] = CPU_bit_r8,  [0x47] = CPU_bit_r8,
    [0x48] = CPU_bit_r8,  [0x49] = CPU_bit_r8,  [0x4A] = CPU_bit_r8,
    [0x4B] = CPU_bit_r8,  [0x4C] = CPU_bit_r8,  [0x4D] = CPU_bit_r8,
    [0x4E] = CPU_bit_r8,  [0x4F] = CPU_bit_r8,  [0x50] = CPU_bit_r8,
    [0x51] = CPU_bit_r8,  [0x52] = CPU_bit_r8,  [0x53] = CPU_bit_r8,
    [0x54] = CPU_bit_r8,  [0x55] = CPU_bit_r8,  [0x56] = CPU_bit_r8,
    [0x57] = CPU_bit_r8,  [0x58] = CPU_bit_r8,  [0x59] = CPU_bit_r8,
    [0x5A] = CPU_bit_r8,  [0x5B] = CPU_bit_r8,  [0x5C] = CPU_bit_r8,
    [0x5D] = CPU_bit_r8,  [0x5E] = CPU_bit_r8,  [0x5F] = CPU_bit_r8,
    [0x60] = CPU_bit_r8,  [0x61] = CPU_bit_r8,  [0x62] = CPU_bit_r8,
    [0x63] = CPU_bit_r8,  [0x64] = CPU_bit_r8,  [0x65] = CPU_bit_r8,
    [0x66] = CPU_bit_r8,  [0x67] = CPU_bit_r8,  [0x68] = CPU_bit_r8,
    [0x69] = CPU_bit_r8,  [0x6A] = CPU_bit_r8,  [0x6B] = CPU_bit_r8,
    [0x6C] = CPU_bit_r8,  [0x6D] = CPU_bit_r8,  [0x6E] = CPU_bit_r8,
    [0x6F] = CPU_bit_r8,  [0x70] = CPU_bit_r8,  [0x71] = CPU_bit_r8,
    [0x72] = CPU_bit_r8,  [0x73] = CPU_bit_r8,  [0x74] = CPU_bit_r8,
    [0x75] = CPU_bit_r8,  [0x76] = CPU_bit_r8,  [0x77] = CPU_bit_r8,
    [0x78] = CPU_bit_r8,  [0x79] = CPU_bit_r8,  [0x7A] = CPU_bit_r8,
    [0x7B] = CPU_bit_r8,  [0x7C] = CPU_bit_r8,  [0x7D] = CPU_bit_r8,
    [0x7E] = CPU_bit_r8,  [0x7F] = CPU_bit_r8,  [0x80] = CPU_res_r8,
    [0x81] = CPU_res_r8,  [0x82] = CPU_res_r8,  [0x83] = CPU_res_r8,
    [0x84] = CPU_res_r8,  [0x85] = CPU_res_r8,  [0x86] = CPU_res_r8,
    [0x87] = CPU_res_r8,  [0x88] = CPU_res_r8,  [0x89] = CPU_res_r8,
    [0x8A] = CPU_res_r8,  [0x8B] = CPU_res_r8,  [0x8C] = CPU_res_r8,
    [0x8D] = CPU_res_r8,  [0x8E] = CPU_res_r8,  [0x8F] = CPU_res_r8,
    [0x90] = CPU_res_r8,  [0x91] = CPU_res_r8,  [0x92] = CPU_res_r8,
    [0x93] = CPU_res_r8,  [0x94] = CPU_res_r8,  [0x95] = CPU_res_r8,
    [0x96] = CPU_res_r8,  [0x97] = CPU_res_r8,  [0x98] = CPU_res_r8,
    [0x99] = CPU_res_r8,  [0x9A] = CPU_res_r8,  [0x9B] = CPU_res_r8,
    [0x9C] = CPU_res_r8,  [0x9D] = CPU_res_r8,  [0x9E] = CPU_res_r8,
    [0x9F] = CPU_res_r8,  [0xA0] = CPU_set_r8,  [0xA1] = CPU_set_r8,
    [0xA2] = CPU_set_r8,  [0xA3] = CPU_set_r8,  [0xA4] = CPU_set_r8,
    [0xA5] = CPU_set_r8,  [0xA6] = CPU_set_r8,  [0xA7] = CPU_set_r8,
    [0xA8] = CPU_set_r8,  [0xA9] = CPU_set_r8,  [0xAA] = CPU_set_r8,
    [0xAB] = CPU_set_r8,  [0xAC] = CPU_set_r8,  [0xAD] = CPU_set_r8,
    [0xAE] = CPU_set_r8,  [0xAF] = CPU_set_r8,  [0xB0] = CPU_set_r8,
    [0xB1] = CPU_set_r8,  [0xB2] = CPU_set_r8,  [0xB3] = CPU_set_r8,
    [0xB4] = CPU_set_r8,  [0xB5] = CPU_set_r8,  [0xB6] = CPU_set_r8,
    [0xB7] = CPU_set_r8,  [0xB8] = CPU_set_r8,  [0xB9] = CPU_set_r8,
    [0xBA] = CPU_set_r8,  [0xBB] = CPU_set_r8,  [0xBC] = CPU_set_r8,
    [0xBD] = CPU_set_r8,  [0xBE] = CPU_set_r8,  [0xBF] = CPU_set_r8,
    [0xC0] = CPU_bit_r8,  [0xC1] = CPU_bit_r8,  [0xC2] = CPU_bit_r8,
    [0xC3] = CPU_bit_r8,  [0xC4] = CPU_bit_r8,  [0xC5] = CPU_bit_r8,
    [0xC6] = CPU_bit_r8,  [0xC7] = CPU_bit_r8,  [0xC8] = CPU_bit_r8,
    [0xC9] = CPU_bit_r8,  [0xCA] = CPU_bit_r8,  [0xCB] = CPU_bit_r8,
    [0xCC] = CPU_bit_r8,  [0xCD] = CPU_bit_r8,  [0xCE] = CPU_bit_r8,
    [0xCF] = CPU_bit_r8,  [0xD0] = CPU_bit_r8,  [0xD1] = CPU_bit_r8,
    [0xD2] = CPU_bit_r8,  [0xD3] = CPU_bit_r8,  [0xD4] = CPU_bit_r8,
    [0xD5] = CPU_bit_r8,  [0xD6] = CPU_bit_r8,  [0xD7] = CPU_bit_r8,
    [0xD8] = CPU_bit_r8,  [0xD9] = CPU_bit_r8,  [0xDA] = CPU_bit_r8,
    [0xDB] = CPU_bit_r8,  [0xDC] = CPU_bit_r8,  [0xDD] = CPU_bit_r8,
    [0xDE] = CPU_bit_r8,  [0xDF] = CPU_bit_r8,  [0xE0] = CPU_bit_r8,
    [0xE1] = CPU_bit_r8,  [0xE2] = CPU_bit_r8,  [0xE3] = CPU_bit_r8,
    [0xE4] = CPU_bit_r8,  [0xE5] = CPU_bit_r8,  [0xE6] = CPU_bit_r8,
    [0xE7] = CPU_bit_r8,  [0xE8] = CPU_bit_r8,  [0xE9] = CPU_bit_r8,
    [0xEA] = CPU_bit_r8,  [0xEB] = CPU_bit_r8,  [0xEC] = CPU_bit_r8,
    [0xED] = CPU_bit_r8,  [0xEE] = CPU_bit_r8,  [0xEF] = CPU_bit_r8,
    [0xF0] = CPU_bit_r8,  [0xF1] = CPU_bit_r8,  [0xF2] = CPU_bit_r8,
    [0xF3] = CPU_bit_r8,  [0xF4] = CPU_bit_r8,  [0xF5] = CPU_bit_r8,
    [0xF6] = CPU_bit_r8,  [0xF7] = CPU_bit_r8,  [0xF8] = CPU_bit_r8,
    [0xF9] = CPU_bit_r8,  [0xFA] = CPU_bit_r8,  [0xFB] = CPU_bit_r8,
    [0xFC] = CPU_bit_r8,  [0xFD] = CPU_bit_r8,  [0xFE] = CPU_bit_r8,
    [0xFF] = CPU_bit_r8,
};

void CPU_prefix(CPU *cpu, uint8_t opcode) {
  (void)opcode;
  // reads next opcode (prefix instruction) and executes it
  uint8_t prefixOpcode = CPU_read_memory(cpu, cpu->PC++);
  // printf("Executing prefix: PC=%04X, opcode=%02X\n", cpu->PC,
  // cpu->memory[cpu->PC]);
  prefixTable[prefixOpcode](cpu, prefixOpcode);
}

static OpcodeHandler opcodeTable[256] = {
    [0x00] = CPU_nop,
    [0x01] = CPU_LD_r16_imm16,
    [0x02] = CPU_LD_indirectr16mem_A,
    [0x03] = CPU_inc_r16,
    [0x04] = CPU_inc_r8,
    [0x05] = CPU_dec_r8,
    [0x06] = CPU_LD_r8_imm8,
    [0x07] = CPU_rlca,
    [0x08] = CPU_LD_indirectimm16_SP,
    [0x09] = CPU_add_hl_r16,
    [0x0A] = CPU_LD_A_indirectr16mem,
    [0x0B] = CPU_dec_r16,
    [0x0C] = CPU_inc_r8,
    [0x0D] = CPU_dec_r8,
    [0x0E] = CPU_LD_r8_imm8,
    [0x0F] = CPU_rrca,
    [0x10] = CPU_stop,
    [0x11] = CPU_LD_r16_imm16,
    [0x12] = CPU_LD_indirectr16mem_A,
    [0x13] = CPU_inc_r16,
    [0x14] = CPU_inc_r8,
    [0x15] = CPU_dec_r8,
    [0x16] = CPU_LD_r8_imm8,
    [0x17] = CPU_rla,
    [0x18] = CPU_jr_imm8,
    [0x19] = CPU_add_hl_r16,
    [0x1A] = CPU_LD_A_indirectr16mem,
    [0x1B] = CPU_dec_r16,
    [0x1C] = CPU_inc_r8,
    [0x1D] = CPU_dec_r8,
    [0x1E] = CPU_LD_r8_imm8,
    [0x1F] = CPU_rra,
    [0x20] = CPU_jr_cond,
    [0x21] = CPU_LD_r16_imm16,
    [0x22] = CPU_LD_indirectr16mem_A,
    [0x23] = CPU_inc_r16,
    [0x24] = CPU_inc_r8,
    [0x25] = CPU_dec_r8,
    [0x26] = CPU_LD_r8_imm8,
    [0x27] = CPU_daa,
    [0x28] = CPU_jr_cond,
    [0x29] = CPU_add_hl_r16,
    [0x2A] = CPU_LD_A_indirectr16mem,
    [0x2B] = CPU_dec_r16,
    [0x2C] = CPU_inc_r8,
    [0x2D] = CPU_dec_r8,
    [0x2E] = CPU_LD_r8_imm8,
    [0x2F] = CPU_cpl,
    [0x30] = CPU_jr_cond,
    [0x31] = CPU_LD_r16_imm16,
    [0x32] = CPU_LD_indirectr16mem_A,
    [0x33] = CPU_inc_r16,
    [0x34] = CPU_inc_r8,
    [0x35] = CPU_dec_r8,
    [0x36] = CPU_LD_r8_imm8,
    [0x37] = CPU_scf,
    [0x38] = CPU_jr_cond,
    [0x39] = CPU_add_hl_r16,
    [0x3A] = CPU_LD_A_indirectr16mem,
    [0x3B] = CPU_dec_r16,
    [0x3C] = CPU_inc_r8,
    [0x3D] = CPU_dec_r8,
    [0x3E] = CPU_LD_r8_imm8,
    [0x3F] = CPU_ccf,
    [0x40] = CPU_LD_r8_r8,
    [0x41] = CPU_LD_r8_r8,
    [0x42] = CPU_LD_r8_r8,
    [0x43] = CPU_LD_r8_r8,
    [0x44] = CPU_LD_r8_r8,
    [0x45] = CPU_LD_r8_r8,
    [0x46] = CPU_LD_r8_r8,
    [0x47] = CPU_LD_r8_r8,
    [0x48] = CPU_LD_r8_r8,
    [0x49] = CPU_LD_r8_r8,
    [0x4A] = CPU_LD_r8_r8,
    [0x4B] = CPU_LD_r8_r8,
    [0x4C] = CPU_LD_r8_r8,
    [0x4D] = CPU_LD_r8_r8,
    [0x4E] = CPU_LD_r8_r8,
    [0x4F] = CPU_LD_r8_r8,
    [0x50] = CPU_LD_r8_r8,
    [0x51] = CPU_LD_r8_r8,
    [0x52] = CPU_LD_r8_r8,
    [0x53] = CPU_LD_r8_r8,
    [0x54] = CPU_LD_r8_r8,
    [0x55] = CPU_LD_r8_r8,
    [0x56] = CPU_LD_r8_r8,
    [0x57] = CPU_LD_r8_r8,
    [0x58] = CPU_LD_r8_r8,
    [0x59] = CPU_LD_r8_r8,
    [0x5A] = CPU_LD_r8_r8,
    [0x5B] = CPU_LD_r8_r8,
    [0x5C] = CPU_LD_r8_r8,
    [0x5D] = CPU_LD_r8_r8,
    [0x5E] = CPU_LD_r8_r8,
    [0x5F] = CPU_LD_r8_r8,
    [0x60] = CPU_LD_r8_r8,
    [0x61] = CPU_LD_r8_r8,
    [0x62] = CPU_LD_r8_r8,
    [0x63] = CPU_LD_r8_r8,
    [0x64] = CPU_LD_r8_r8,
    [0x65] = CPU_LD_r8_r8,
    [0x66] = CPU_LD_r8_r8,
    [0x67] = CPU_LD_r8_r8,
    [0x68] = CPU_LD_r8_r8,
    [0x69] = CPU_LD_r8_r8,
    [0x6A] = CPU_LD_r8_r8,
    [0x6B] = CPU_LD_r8_r8,
    [0x6C] = CPU_LD_r8_r8,
    [0x6D] = CPU_LD_r8_r8,
    [0x6E] = CPU_LD_r8_r8,
    [0x6F] = CPU_LD_r8_r8,
    [0x70] = CPU_LD_r8_r8,
    [0x71] = CPU_LD_r8_r8,
    [0x72] = CPU_LD_r8_r8,
    [0x73] = CPU_LD_r8_r8,
    [0x74] = CPU_LD_r8_r8,
    [0x75] = CPU_LD_r8_r8,
    [0x76] = CPU_halt,
    [0x77] = CPU_LD_r8_r8,
    [0x78] = CPU_LD_r8_r8,
    [0x79] = CPU_LD_r8_r8,
    [0x7A] = CPU_LD_r8_r8,
    [0x7B] = CPU_LD_r8_r8,
    [0x7C] = CPU_LD_r8_r8,
    [0x7D] = CPU_LD_r8_r8,
    [0x7E] = CPU_LD_r8_r8,
    [0x7F] = CPU_LD_r8_r8,
    [0x80] = CPU_add_a_r8,
    [0x81] = CPU_add_a_r8,
    [0x82] = CPU_add_a_r8,
    [0x83] = CPU_add_a_r8,
    [0x84] = CPU_add_a_r8,
    [0x85] = CPU_add_a_r8,
    [0x86] = CPU_add_a_r8,
    [0x87] = CPU_add_a_r8,
    [0x88] = CPU_adc_a_r8,
    [0x89] = CPU_adc_a_r8,
    [0x8A] = CPU_adc_a_r8,
    [0x8B] = CPU_adc_a_r8,
    [0x8C] = CPU_adc_a_r8,
    [0x8D] = CPU_adc_a_r8,
    [0x8E] = CPU_adc_a_r8,
    [0x8F] = CPU_adc_a_r8,
    [0x90] = CPU_sub_a_r8,
    [0x91] = CPU_sub_a_r8,
    [0x92] = CPU_sub_a_r8,
    [0x93] = CPU_sub_a_r8,
    [0x94] = CPU_sub_a_r8,
    [0x95] = CPU_sub_a_r8,
    [0x96] = CPU_sub_a_r8,
    [0x97] = CPU_sub_a_r8,
    [0x98] = CPU_sbc_a_r8,
    [0x99] = CPU_sbc_a_r8,
    [0x9A] = CPU_sbc_a_r8,
    [0x9B] = CPU_sbc_a_r8,
    [0x9C] = CPU_sbc_a_r8,
    [0x9D] = CPU_sbc_a_r8,
    [0x9E] = CPU_sbc_a_r8,
    [0x9F] = CPU_sbc_a_r8,
    [0xA0] = CPU_and_a_r8,
    [0xA1] = CPU_and_a_r8,
    [0xA2] = CPU_and_a_r8,
    [0xA3] = CPU_and_a_r8,
    [0xA4] = CPU_and_a_r8,
    [0xA5] = CPU_and_a_r8,
    [0xA6] = CPU_and_a_r8,
    [0xA7] = CPU_and_a_r8,
    [0xA8] = CPU_xor_a_r8,
    [0xA9] = CPU_xor_a_r8,
    [0xAA] = CPU_xor_a_r8,
    [0xAB] = CPU_xor_a_r8,
    [0xAC] = CPU_xor_a_r8,
    [0xAD] = CPU_xor_a_r8,
    [0xAE] = CPU_xor_a_r8,
    [0xAF] = CPU_xor_a_r8,
    [0xB0] = CPU_or_a_r8,
    [0xB1] = CPU_or_a_r8,
    [0xB2] = CPU_or_a_r8,
    [0xB3] = CPU_or_a_r8,
    [0xB4] = CPU_or_a_r8,
    [0xB5] = CPU_or_a_r8,
    [0xB6] = CPU_or_a_r8,
    [0xB7] = CPU_or_a_r8,
    [0xB8] = CPU_cp_a_r8,
    [0xB9] = CPU_cp_a_r8,
    [0xBA] = CPU_cp_a_r8,
    [0xBB] = CPU_cp_a_r8,
    [0xBC] = CPU_cp_a_r8,
    [0xBD] = CPU_cp_a_r8,
    [0xBE] = CPU_cp_a_r8,
    [0xBF] = CPU_cp_a_r8,
    [0xC0] = CPU_ret_cond,
    [0xC1] = CPU_pop_r16stk,
    [0xC2] = CPU_jp_cond_imm16,
    [0xC3] = CPU_jp_imm16,
    [0xC4] = CPU_call_cond_imm16,
    [0xC5] = CPU_push_r16stk,
    [0xC6] = CPU_add_a_imm8,
    [0xC7] = CPU_rst,
    [0xC8] = CPU_ret_cond,
    [0xC9] = CPU_ret,
    [0xCA] = CPU_jp_cond_imm16,
    [0xCB] = CPU_prefix,
    [0xCC] = CPU_call_cond_imm16,
    [0xCD] = CPU_call_imm16,
    [0xCE] = CPU_adc_a_imm8,
    [0xCF] = CPU_rst,
    [0xD0] = CPU_ret_cond,
    [0xD1] = CPU_pop_r16stk,
    [0xD2] = CPU_jp_cond_imm16,
    [0xD3] = CPU_invalid,
    [0xD4] = CPU_call_cond_imm16,
    [0xD5] = CPU_push_r16stk,
    [0xD6] = CPU_sub_a_imm8,
    [0xD7] = CPU_rst,
    [0xD8] = CPU_ret_cond,
    [0xD9] = CPU_reti,
    [0xDA] = CPU_jp_cond_imm16,
    [0xDB] = CPU_invalid,
    [0xDC] = CPU_call_cond_imm16,
    [0xDD] = CPU_invalid,
    [0xDE] = CPU_sbc_a_imm8,
    [0xDF] = CPU_rst,
    [0xE0] = CPU_ldh_indirectimm8_a,
    [0xE1] = CPU_pop_r16stk,
    [0xE2] = CPU_ldh_indirectc_a,
    [0xE3] = CPU_invalid,
    [0xE4] = CPU_call_cond_imm16,
    [0xE5] = CPU_push_r16stk,
    [0xE6] = CPU_and_a_imm8,
    [0xE7] = CPU_rst,
    [0xE8] = CPU_add_sp_imm8,
    [0xE9] = CPU_jp_hl,
    [0xEA] = CPU_ld_indirectimm16_a,
    [0xEB] = CPU_invalid,
    [0xEC] = CPU_call_cond_imm16,
    [0xED] = CPU_invalid,
    [0xEE] = CPU_xor_a_imm8,
    [0xEF] = CPU_rst,
    [0xF0] = CPU_ldh_a_indirectimm8,
    [0xF1] = CPU_pop_r16stk,
    [0xF2] = CPU_ldh_a_indirectc,
    [0xF3] = CPU_di,
    [0xF4] = CPU_call_cond_imm16,
    [0xF5] = CPU_push_r16stk,
    [0xF6] = CPU_or_a_imm8,
    [0xF7] = CPU_rst,
    [0xF8] = CPU_ld_hl_sp_imm8,
    [0xF9] = CPU_ld_sp_hl,
    [0xFA] = CPU_ld_a_indirectimm16,
    [0xFB] = CPU_ei,
    [0xFC] = CPU_call_cond_imm16,
    [0xFD] = CPU_invalid,
    [0xFE] = CPU_cp_a_imm8,
    [0xFF] = CPU_rst,
};

static int div_counter = 0;
static int tima_counter = 0;

// cycles elapsed by instruction for real per-instruction cycle counts
void CPU_update_timer(CPU *cpu, int cycles_elapsed) {
  // --- DIV logic ---
  div_counter += cycles_elapsed;
  while (div_counter >= 256) {
    div_counter -= 256;
    uint8_t div = CPU_read_memory(cpu, 0xFF04);
    CPU_write_memory(cpu, 0xFF04, div + 1);
  }

  // --- TIMA logic ---
  uint8_t tac = CPU_read_memory(cpu, 0xFF07);
  if (!(tac & 0x04))
    return; // timer disabled

  int freq_select = tac & 0x03;
  int threshold;
  switch (freq_select) {
  case 0:
    threshold = 1024;
    break;
  case 1:
    threshold = 16;
    break;
  case 2:
    threshold = 64;
    break;
  case 3:
    threshold = 256;
    break;
  }

  tima_counter += cycles_elapsed;
  while (tima_counter >= threshold) {
    tima_counter -= threshold;

    uint8_t tima = CPU_read_memory(cpu, 0xFF05);
    uint8_t tma = CPU_read_memory(cpu, 0xFF06);
    uint8_t if_reg = CPU_read_memory(cpu, 0xFF0F);

    if (tima == 0xFF) {
      tima = tma;
      if_reg |= 0x04; // timer interrupt
    } else {
      tima++;
    }

    CPU_write_memory(cpu, 0xFF05, tima);
    CPU_write_memory(cpu, 0xFF0F, if_reg);
  }
}

void CPU_instruction(CPU *cpu) {
  uint8_t opcode = CPU_read_memory(cpu, cpu->PC++);
  OpcodeHandler handler = opcodeTable[opcode];
  handler(cpu, opcode);
  if (cpu->pending_IME) {
    cpu->IME = 1;
    cpu->pending_IME = 0;
  }
  // 4 is an average cycle time
  CPU_update_timer(cpu, 4);
};

void CPU_run(CPU *cpu, int cycles) {
  for (int i = 0; i < cycles; i++) {
    // handle interrupts if IME is set and if interrupt is pending
    uint8_t iflags = CPU_read_memory(cpu, 0xFF0F);
    uint8_t ienable = CPU_read_memory(cpu, 0xFFFF);

    if (cpu->IME && (iflags & ienable)) {
      for (int j = 0; j < 5; j++) {
        if ((iflags & (1 << j)) && (ienable & (1 << j))) {
          cpu->IME = 0;
          iflags &= ~(1 << j);
          CPU_write_memory(cpu, 0xFF0F, iflags);

          // push return address
          CPU_write_memory(cpu, --cpu->SP, (cpu->PC >> 8) & 0xFF);
          CPU_write_memory(cpu, --cpu->SP, cpu->PC & 0xFF);
          cpu->PC = 0x40 + j * 8; // jump to ISR
          break;
        }
      }
    }
    // CPU_display(cpu);
    CPU_instruction(cpu);
  }
}

void CPU_read_rom(CPU *cpu, const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    printf("error: could not open file %s\n", filename);
    exit(1);
  }
  // Read the entire ROM starting at memory address 0.
  size_t bytesRead = fread(cpu->_memory, 1, sizeof(cpu->_memory), file);
  printf("Loaded %zu bytes of ROM\n", bytesRead);
  fclose(file);
}
