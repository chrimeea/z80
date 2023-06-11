// gcc main.c -Ofast -lGLEW -lGLU -lGL -lglut -pthread -Wall

#include <GL/freeglut.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MAX0 0x01
#define MAX1 0x02
#define MAX2 0x04
#define MAX3 0x08
#define MAX4 0x10
#define MAX5 0x20
#define MAX6 0x40
#define MAX7 0x80
#define MAX8 0x100
#define MAX9 0x200
#define MAX10 0x400
#define MAX11 0x800
#define MAX12 0x1000
#define MAX13 0x2000
#define MAX14 0x4000
#define MAX15 0x8000
#define MAX16 0x10000

#define FLAG_C 0x01
#define FLAG_N 0x02
#define FLAG_PV 0x04
#define FLAG_HC 0x10
#define FLAG_Z 0x40
#define FLAG_S 0x80
#define MASK_ALL 0xFF
#define MASK_SZHVN 0xD6
#define MASK_NONE 0x00

#define REG8_ONE \
    (REG8) { .value = 1 }
#define REG16_ONE \
    (REG16) { .value = 1 }

#define sign(X) (X < 0)
#define zero(X) (X == 0)
#define is_bit(I, B) (I & (B))
#define register_is_bit(R, B) (is_bit(R.byte_value, B))
#define register_is_flag(B) (register_is_bit(z80_reg_af.bytes.low, B))
#define set_bit(I, B) (I |= (B))
#define unset_bit(I, B) (I &= ~(B))
#define set_or_unset_bit(I, B, V) (V ? set_bit(I, B) : unset_bit(I, B))
#define register_set_or_unset_bit(R, B, V) (set_or_unset_bit(R.byte_value, B, V))
#define register_set_or_unset_flag(B, V) (register_set_or_unset_bit(z80_reg_af.bytes.low, B, V))
#define register_split_8_to_4(R) (div(R.byte_value, MAX3))
#define register_set_8_from_4(R, Q, M) (R.byte_value = Q * MAX3 + M)
#define register_a_set_flag_s() (register_set_or_unset_flag(FLAG_S, sign(z80_reg_af.bytes.high.value)))
#define register_a_set_flag_z() (register_set_or_unset_flag(FLAG_Z, zero(z80_reg_af.bytes.high.value)))
#define register_a_set_flag_p() (register_set_or_unset_flag(FLAG_PV, __builtin_parity(z80_reg_af.bytes.high.byte_value)))

typedef union
{
    unsigned char byte_value;
    char value;
} REG8;

typedef union
{
    struct
    {
        REG8 low;
        REG8 high;
    } bytes;
    unsigned short byte_value;
    short value;
} REG16;

typedef struct
{
    GLfloat red;
    GLfloat green;
    GLfloat blue;
} RGB;

REG8 memory[MAX16];
REG8 keyboard[] = {(REG8){.value = 0x1F}, (REG8){.value = 0x1F}, (REG8){.value = 0x1F},
                   (REG8){.value = 0x1F}, (REG8){.value = 0x1F}, (REG8){.value = 0x1F}, (REG8){.value = 0x1F},
                   (REG8){.value = 0x1F}};
RGB ula_colors[] = {(RGB){0.0f, 0.0f, 0.0f}, (RGB){0.0f, 0.0f, 1.0f},
                    (RGB){1.0f, 0.0f, 0.0f}, (RGB){0.5f, 0.0f, 0.5f},
                    (RGB){0.0f, 1.0f, 0.0f}, (RGB){0.0f, 1.0f, 1.0f},
                    (RGB){1.0f, 1.0f, 0.0f}, (RGB){0.5f, 0.5f, 0.5f}};
RGB ula_bright_colors[] = {(RGB){0.0f, 0.0f, 0.0f}, (RGB){0.0f, 0.0f, 1.0f},
                           (RGB){1.0f, 0.0f, 0.0f}, (RGB){0.5f, 0.0f, 0.5f},
                           (RGB){0.0f, 1.0f, 0.0f}, (RGB){0.0f, 1.0f, 1.0f},
                           (RGB){1.0f, 1.0f, 0.0f}, (RGB){0.5f, 0.5f, 0.5f}};
const unsigned int memory_size = MAX16;
long double time_start, state_duration = 0.00000035L;
unsigned long z80_t_states_all = 0, ula_t_states_all = 0;
unsigned int ula_draw_counter = 0;
REG16 ula_addr_bitmap, ula_addr_attrib;
REG16 z80_reg_bc, z80_reg_de, z80_reg_hl, z80_reg_af, z80_reg_pc, z80_reg_sp, z80_reg_ix, z80_reg_iy;
REG16 z80_reg_bc_2, z80_reg_de_2, z80_reg_hl_2, z80_reg_af_2, z80_reg_pc_2, z80_reg_sp_2, z80_reg_ix_2, z80_reg_iy_2;
REG8 z80_reg_i, z80_reg_r, z80_data_bus;
REG8 *z80_all8[] = {&z80_reg_bc.bytes.high, &z80_reg_bc.bytes.low,
                    &z80_reg_de.bytes.high, &z80_reg_de.bytes.low,
                    &z80_reg_hl.bytes.high, &z80_reg_hl.bytes.low,
                    NULL, &z80_reg_af.bytes.high};
REG16 *z80_bc_de_hl_sp[] = {&z80_reg_bc, &z80_reg_de, &z80_reg_hl, &z80_reg_sp};
REG16 *z80_bc_de_hl_af[] = {&z80_reg_bc, &z80_reg_de, &z80_reg_hl, &z80_reg_af};
int z80_rst_addr[] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};
bool running;
bool z80_maskable_interrupt_flag, z80_nonmaskable_interrupt_flag;
bool z80_iff1, z80_iff2, z80_can_execute;
int z80_imode;

int system_little_endian()
{
    int x = 1;
    return *(char *)&x;
}

long double time_in_seconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000L;
}

void time_seconds_to_timespec(struct timespec *ts, long double s)
{
    long double temp;
    ts->tv_nsec = modfl(s, &temp) * 1000000000;
    ts->tv_sec = temp;
}

void time_sync(unsigned long *t_states_all, int t_states)
{
    *t_states_all += t_states;
    struct timespec ts;
    time_seconds_to_timespec(&ts, time_start + *t_states_all * state_duration - time_in_seconds());
    nanosleep(&ts, &ts);
}

void register_exchange16(REG16 *reg, REG16 *alt)
{
    REG16 temp = *reg;
    *reg = *alt;
    *alt = temp;
}

void register_a_set_flag_s_z_p()
{
    register_a_set_flag_s();
    register_a_set_flag_z();
    register_a_set_flag_p();
}

void register_left8_with_flags(REG8 *reg, int mask, bool b)
{
    register_set_or_unset_flag(FLAG_C & mask, reg->byte_value & MAX7);
    register_set_or_unset_flag(FLAG_HC & mask, false);
    register_set_or_unset_flag(FLAG_N & mask, false);
    reg->byte_value <<= 1;
    set_or_unset_bit(reg->byte_value, MAX0, b);
}

void register_right8_with_flags(REG8 *reg, int mask, bool b)
{
    register_set_or_unset_flag(FLAG_C & mask, reg->byte_value & MAX0);
    register_set_or_unset_flag(FLAG_HC & mask, false);
    register_set_or_unset_flag(FLAG_N & mask, false);
    reg->byte_value >>= 1;
    set_or_unset_bit(reg->byte_value, MAX7, b);
}

void register_add8_with_flags(REG8 *reg, REG8 alt, int mask)
{
    int r = reg->byte_value + alt.byte_value;
    bool s = sign((short)r);
    register_set_or_unset_flag(FLAG_C & mask, r >= MAX8);
    register_set_or_unset_flag(FLAG_HC & mask, (r & 0x0F) >= MAX4);
    register_set_or_unset_flag(FLAG_N & mask, false);
    register_set_or_unset_flag(FLAG_PV & mask, sign(reg->value) == sign(alt.value) && s != sign(reg->value));
    register_set_or_unset_flag(FLAG_S & mask, s);
    register_set_or_unset_flag(FLAG_Z & mask, zero(r));
    reg->byte_value = r;
}

void register_sub8_with_flags(REG8 *reg, REG8 alt, int mask)
{
    short r = reg->byte_value - alt.byte_value;
    bool s = sign(r);
    register_set_or_unset_flag(FLAG_C & mask, alt.byte_value > reg->byte_value);
    register_set_or_unset_flag(FLAG_HC & mask, (alt.byte_value & 0x0F) > (reg->byte_value & 0x0F));
    register_set_or_unset_flag(FLAG_N & mask, true);
    register_set_or_unset_flag(FLAG_PV & mask, sign(reg->value) != sign(alt.value) && s != sign(reg->value));
    register_set_or_unset_flag(FLAG_S & mask, s);
    register_set_or_unset_flag(FLAG_Z & mask, zero(r));
    reg->byte_value = r;
}

void register_add16_with_flags(REG16 *reg, REG16 alt, int mask)
{
    int r = reg->byte_value + alt.byte_value;
    register_set_or_unset_flag(FLAG_C & mask, r >= MAX16);
    register_set_or_unset_flag(FLAG_HC & mask, (r & 0xFFF) >= MAX12);
    register_set_or_unset_flag(FLAG_N & mask, false);
    reg->byte_value = r;
}

void register_sub16_with_flags(REG16 *reg, REG16 alt, int mask)
{
    register_set_or_unset_flag(FLAG_C & mask, alt.byte_value > reg->byte_value);
    register_set_or_unset_flag(FLAG_HC & mask, (alt.byte_value & 0xFFF) > (reg->byte_value & 0xFFF));
    register_set_or_unset_flag(FLAG_N & mask, true);
    reg->byte_value -= alt.byte_value;
}

void memory_load_rom(const char *filename)
{
    int n, remaining = memory_size;
    REG8 *m = memory;
    FILE *f = fopen(filename, "rb");
    do
    {
        n = fread(m, 1, remaining, f);
        m += n;
        remaining -= n;
    } while (n != 0);
    fclose(f);
}

REG8 *memory_ref8(const REG16 reg)
{
    return &memory[reg.byte_value];
}

REG8 memory_read8(const REG16 reg)
{
    return *memory_ref8(reg);
}

void memory_write8(const REG16 reg, const REG8 alt)
{
    *memory_ref8(reg) = alt;
}

REG8 *memory_ref8_indexed(const REG16 reg16, const REG8 reg8)
{
    return &memory[reg16.byte_value + reg8.value];
}

REG8 memory_read8_indexed(const REG16 reg16, const REG8 reg8)
{
    return *memory_ref8_indexed(reg16, reg8);
}

void memory_write8_indexed(const REG16 reg16, const REG8 reg8, REG8 alt)
{
    *memory_ref8_indexed(reg16, reg8) = alt;
}

REG16 *memory_ref16(REG16 reg)
{
    return (REG16 *)memory_ref8(reg);
}

REG16 memory_read16(REG16 reg)
{
    return *memory_ref16(reg);
}

void memory_write16(REG16 reg, REG16 alt)
{
    *memory_ref16(reg) = alt;
}

REG8 keyboard_read8(const REG16 reg)
{
    REG8 alt;
    alt.byte_value = 0x1F;
    int b = MAX0;
    for (int i = 0; i < 8; i++)
    {
        if (!register_is_bit(reg.bytes.high, b))
        {
            alt.byte_value &= keyboard[i].byte_value;
        }
        b <<= 1;
    }
    return alt;
}

void keyboard_press(unsigned char key, const bool value)
{
    // if (strcmp(key, "Caps_Lock") == 0) {
    //     register_set_or_unset_bit(keyboard[0], MAX0, value);
    if (key == 'z' || key == 'Z')
    {
        register_set_or_unset_bit(keyboard[0], MAX1, value);
    }
    else if (key == 'x' || key == 'X')
    {
        register_set_or_unset_bit(keyboard[0], MAX2, value);
    }
    else if (key == 'c' || key == 'C')
    {
        register_set_or_unset_bit(keyboard[0], MAX3, value);
    }
    else if (key == 'v' || key == 'V')
    {
        register_set_or_unset_bit(keyboard[0], MAX4, value);
    }
    else if (key == 'a' || key == 'A')
    {
        register_set_or_unset_bit(keyboard[1], MAX0, value);
    }
    else if (key == 's' || key == 'S')
    {
        register_set_or_unset_bit(keyboard[1], MAX1, value);
    }
    else if (key == 'd' || key == 'D')
    {
        register_set_or_unset_bit(keyboard[1], MAX2, value);
    }
    else if (key == 'f' || key == 'F')
    {
        register_set_or_unset_bit(keyboard[1], MAX3, value);
    }
    else if (key == 'g' || key == 'G')
    {
        register_set_or_unset_bit(keyboard[1], MAX4, value);
    }
    else if (key == 'q' || key == 'Q')
    {
        register_set_or_unset_bit(keyboard[2], MAX0, value);
    }
    else if (key == 'w' || key == 'W')
    {
        register_set_or_unset_bit(keyboard[2], MAX1, value);
    }
    else if (key == 'e' || key == 'E')
    {
        register_set_or_unset_bit(keyboard[2], MAX2, value);
    }
    else if (key == 'r' || key == 'R')
    {
        register_set_or_unset_bit(keyboard[2], MAX3, value);
    }
    else if (key == 't' || key == 'T')
    {
        register_set_or_unset_bit(keyboard[2], MAX4, value);
    }
    else if (key == '1')
    {
        register_set_or_unset_bit(keyboard[3], MAX0, value);
    }
    else if (key == '2')
    {
        register_set_or_unset_bit(keyboard[3], MAX1, value);
    }
    else if (key == '3')
    {
        register_set_or_unset_bit(keyboard[3], MAX2, value);
    }
    else if (key == '4')
    {
        register_set_or_unset_bit(keyboard[3], MAX3, value);
    }
    else if (key == '5')
    {
        register_set_or_unset_bit(keyboard[3], MAX4, value);
    }
    else if (key == '0')
    {
        register_set_or_unset_bit(keyboard[4], MAX0, value);
    }
    else if (key == '9')
    {
        register_set_or_unset_bit(keyboard[4], MAX1, value);
    }
    else if (key == '8')
    {
        register_set_or_unset_bit(keyboard[4], MAX2, value);
    }
    else if (key == '7')
    {
        register_set_or_unset_bit(keyboard[4], MAX3, value);
    }
    else if (key == '6')
    {
        register_set_or_unset_bit(keyboard[4], MAX4, value);
    }
    else if (key == 'p' || key == 'P')
    {
        register_set_or_unset_bit(keyboard[5], MAX0, value);
    }
    else if (key == 'o' || key == 'O')
    {
        register_set_or_unset_bit(keyboard[5], MAX1, value);
    }
    else if (key == 'i' || key == 'I')
    {
        register_set_or_unset_bit(keyboard[5], MAX2, value);
    }
    else if (key == 'u' || key == 'U')
    {
        register_set_or_unset_bit(keyboard[5], MAX3, value);
    }
    else if (key == 'y' || key == 'Y')
    {
        register_set_or_unset_bit(keyboard[5], MAX4, value);
    }
    else if (key == 13)
    {
        register_set_or_unset_bit(keyboard[6], MAX0, value);
    }
    else if (key == 'l' || key == 'L')
    {
        register_set_or_unset_bit(keyboard[6], MAX1, value);
    }
    else if (key == 'k' || key == 'K')
    {
        register_set_or_unset_bit(keyboard[6], MAX2, value);
    }
    else if (key == 'j' || key == 'J')
    {
        register_set_or_unset_bit(keyboard[6], MAX3, value);
    }
    else if (key == 'h' || key == 'H')
    {
        register_set_or_unset_bit(keyboard[6], MAX4, value);
    }
    else if (key == ' ')
    {
        register_set_or_unset_bit(keyboard[7], MAX0, value);
        // } else if (strcmp(key, "Shift_L") == 0 || strcmp(key, "Shift_R") == 0) {
        //     register_set_or_unset_bit(keyboard[7], MAX1, value);
    }
    else if (key == 'm' || key == 'M')
    {
        register_set_or_unset_bit(keyboard[7], MAX2, value);
    }
    else if (key == 'n' || key == 'N')
    {
        register_set_or_unset_bit(keyboard[7], MAX3, value);
    }
    else if (key == 'b' || key == 'B')
    {
        register_set_or_unset_bit(keyboard[7], MAX4, value);
    }
}

void keyboard_press_down(unsigned char key, int x, int y)
{
    keyboard_press(key, true);
}

void keyboard_press_up(unsigned char key, int x, int y)
{
    keyboard_press(key, false);
}

REG8 port_read8(const REG16 reg)
{
    if (reg.byte_value == 0xFE)
    {
        return keyboard_read8(reg);
    }
    else
    {
        return (REG8){.byte_value = 0xFF};
    }
}

void port_write8(const REG16 reg, const REG8 alt)
{
}

void z80_reset()
{
    z80_nonmaskable_interrupt_flag = false;
    z80_maskable_interrupt_flag = false;
    z80_iff1 = false;
    z80_iff2 = false;
    z80_can_execute = true;
    z80_imode = 0;
    z80_reg_af.byte_value = 0xFFFF;
    z80_reg_sp.byte_value = 0xFFFF;
    z80_reg_pc.byte_value = 0;
    z80_reg_r.byte_value = 0;
    running = true;
    time_start = time_in_seconds();
}

void z80_memory_refresh()
{
    z80_reg_r.byte_value = (z80_reg_r.byte_value + 1) % MAX7;
}

REG8 z80_next8()
{
    REG8 v = memory_read8(z80_reg_pc);
    z80_reg_pc.byte_value++;
    return v;
}

REG16 z80_next16()
{
    REG16 v = memory_read16(z80_reg_pc);
    z80_reg_pc.byte_value += 2;
    return v;
}

void z80_push16(const REG16 reg)
{
    z80_reg_sp.byte_value -= 2;
    memory_write16(z80_reg_sp, reg);
}

REG16 z80_pop16()
{
    REG16 v = memory_read16(z80_reg_sp);
    z80_reg_sp.byte_value += 2;
    return v;
}

REG8 z80_fetch_opcode()
{
    z80_memory_refresh();
    return z80_next8();
}

REG8 *z80_decode_reg8(REG8 reg, int pos, int t, int *r)
{
    int i = reg.byte_value >> pos & 0x07;
    if (i == 0x06)
    {
        *r += t;
        return memory_ref8(z80_reg_hl);
    }
    else
    {
        return z80_all8[i];
    }
}

bool z80_decode_condition(REG8 reg) {
	int i = reg.byte_value >> 3 & 0x07;
	switch (i) {
		case 0x00: return !register_is_flag(FLAG_Z);
		case 0x01: return register_is_flag(FLAG_Z);
		case 0x02: return !register_is_flag(FLAG_C);
		case 0x03: return register_is_flag(FLAG_C);
		case 0x04: return !register_is_flag(FLAG_PV);
		case 0x05: return register_is_flag(FLAG_PV);
		case 0x06: return !register_is_flag(FLAG_S);
		case 0x07: return register_is_flag(FLAG_S);
		default: return 0; //fail
	}
}

int z80_jump_with_condition(bool c)
{
	REG16 reg = z80_next16();
    if (c)
    {
        z80_reg_pc = reg;
    }
    return 10;
}

int z80_jump_rel_with_condition(bool c)
{
	REG8 reg = z80_next8();
    if (c)
    {
        z80_reg_pc.byte_value += reg.value;
        return 12;
    } else {
		return 7;
	}
}

int z80_ret_with_condition(bool c)
{
    if (c)
    {
        z80_reg_pc = z80_pop16();
        return 15;
    }
    else
    {
        return 11;
    }
}

int z80_call_with_condition(bool c)
{
	REG16 reg = z80_next16();
	if (c)
	{
		z80_push16(z80_reg_pc);
		z80_reg_pc = reg;
		return 17;
	}
	else
	{
		return 10;
	}
}

int z80_execute(REG8 reg)
{
    int t = 4;
    REG8 alt;
    div_t qr;
    bool c, hc;
    switch (reg.byte_value)
    {
    case 0x00: //NOP
        return t;
    case 0x01: //LD dd,nn
    case 0x11:
    case 0x21:
    case 0x31:
        *z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03] = z80_next16();
        return 10;
    case 0x02: //LD (BC),A
        memory_write8(z80_reg_bc, z80_reg_af.bytes.high);
        return 7;
    case 0x03: //INC ss
    case 0x13:
    case 0x23:
    case 0x33:
        register_add16_with_flags(z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03], REG16_ONE, MASK_NONE);
        return 6;
    case 0x04: //INC r
    case 0x0C:
    case 0x14:
    case 0x1C:
    case 0x24:
    case 0x2C:
    case 0x34:
    case 0x3C:
        register_add8_with_flags(z80_decode_reg8(reg, 3, 7, &t), REG8_ONE, MASK_SZHVN);
        return t;
    case 0x05: // DEC r
    case 0x0D:
    case 0x15:
    case 0x1D:
    case 0x25:
    case 0x2D:
    case 0x35:
    case 0x3D:
        register_sub8_with_flags(z80_decode_reg8(reg, 3, 7, &t), REG8_ONE, MASK_SZHVN);
        return t;
    case 0x06: //LD r,NN
    case 0x0E:
    case 0x16:
    case 0x1E:
    case 0x26:
    case 0x2E:
    case 0x36:
    case 0x3E:
        t = 7;
        *z80_decode_reg8(reg, 3, 3, &t) = z80_next8();
        return t;
    case 0x07: //RLCA
        register_left8_with_flags(&z80_reg_af.bytes.high, MASK_ALL, register_is_bit(z80_reg_af.bytes.high, MAX7));
        return t;
    case 0x08: //EX AF,AF’
        register_exchange16(&z80_reg_af, &z80_reg_af_2);
        return t;
    case 0x09: //ADD HL,ss
    case 0x19:
    case 0x29:
    case 0x39:
        register_add16_with_flags(&z80_reg_hl, *z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03], MASK_ALL);
        return 11;
    case 0x0A: //LD A,(BC)
        z80_reg_af.bytes.high = memory_read8(z80_reg_bc);
        return 7;
    case 0x0B: //DEC ss
    case 0x1B:
    case 0x2B:
    case 0x3B:
        register_sub16_with_flags(z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03], REG16_ONE, MASK_NONE);
        return 6;
    case 0x0F: //RRCA
        register_right8_with_flags(&z80_reg_af.bytes.high, MASK_ALL, register_is_bit(z80_reg_af.bytes.high, MAX0));
        return t;
    case 0x10: //DJNZ NN
        alt = z80_next8();
        z80_reg_bc.bytes.high.byte_value--;
        if (zero(z80_reg_bc.bytes.high.value))
        {
            return 8;
        }
        else
        {
            z80_reg_pc.byte_value += alt.value;
            return 13;
        }
    case 0x12: //LD (DE),A
        memory_write8(z80_reg_de, z80_reg_af.bytes.high);
        return 7;
    case 0x17: //RLA
        register_left8_with_flags(&z80_reg_af.bytes.high, MASK_ALL, register_is_flag(FLAG_C));
        return t;
    case 0x18: //JR NN
		return z80_jump_rel_with_condition(true);
    case 0x1A: //LD A,(DE)
        z80_reg_af.bytes.high = memory_read8(z80_reg_de);
        return 7;
    case 0x1F: //RRA
        register_right8_with_flags(&z80_reg_af.bytes.high, MASK_ALL, register_is_flag(FLAG_C));
        return t;
    case 0x20: //JR CC,NN
    case 0x28:
    case 0x30:
    case 0x38:
        return z80_jump_rel_with_condition(z80_decode_condition(reg));
    case 0x22: // LD (HHLL),HL
        memory_write16(z80_next16(), z80_reg_hl);
        return 16;
    case 0x27: //DAA
        qr = register_split_8_to_4(z80_reg_af.bytes.high);
        c = register_is_flag(FLAG_C);
        hc = register_is_flag(FLAG_HC);
        if (c == false && hc == false && qr.quot == 0x90 && qr.rem == 0x09)
        {
            z80_reg_af.bytes.high.byte_value += 0x00;
            register_set_or_unset_flag(FLAG_C, false);
        }
        else if (c == false && hc == false && qr.quot == 0x08 && qr.rem == 0xAF)
        {
            z80_reg_af.bytes.high.byte_value += 0x06;
            register_set_or_unset_flag(FLAG_C, false);
        }
        else if (c == false && hc == true && qr.quot == 0x09 && qr.rem == 0x03)
        {
            z80_reg_af.bytes.high.byte_value += 0x06;
            register_set_or_unset_flag(FLAG_C, false);
        }
        else if (c == false && hc == false && qr.quot == 0xAF && qr.rem == 0x09)
        {
            z80_reg_af.bytes.high.byte_value += 0x60;
            register_set_or_unset_flag(FLAG_C, true);
        }
        else if (c == false && hc == false && qr.quot == 0x9F && qr.rem == 0xAF)
        {
            z80_reg_af.bytes.high.byte_value += 0x66;
            register_set_or_unset_flag(FLAG_C, true);
        }
        else if (c == false && hc == true && qr.quot == 0xAF && qr.rem == 0x03)
        {
            z80_reg_af.bytes.high.byte_value += 0x66;
            register_set_or_unset_flag(FLAG_C, true);
        }
        else if (c == true && hc == false && qr.quot == 0x02 && qr.rem == 0x09)
        {
            z80_reg_af.bytes.high.byte_value += 0x60;
            register_set_or_unset_flag(FLAG_C, true);
        }
        else if (c == true && hc == false && qr.quot == 0x02 && qr.rem == 0xAF)
        {
            z80_reg_af.bytes.high.byte_value += 0x66;
            register_set_or_unset_flag(FLAG_C, true);
        }
        else if (c == false && hc == true && qr.quot == 0x03 && qr.rem == 0x03)
        {
            z80_reg_af.bytes.high.byte_value += 0x66;
            register_set_or_unset_flag(FLAG_C, true);
        }
        else if (c == false && hc == false && qr.quot == 0x09 && qr.rem == 0x09)
        {
            z80_reg_af.bytes.high.byte_value += 0x00;
            register_set_or_unset_flag(FLAG_C, false);
        }
        else if (c == false && hc == true && qr.quot == 0x08 && qr.rem == 0x6F)
        {
            z80_reg_af.bytes.high.byte_value += 0xFA;
            register_set_or_unset_flag(FLAG_C, false);
        }
        else if (c == true && hc == false && qr.quot == 0x7F && qr.rem == 0x09)
        {
            z80_reg_af.bytes.high.byte_value += 0xA0;
            register_set_or_unset_flag(FLAG_C, true);
        }
        else if (c == true && hc == true && qr.quot == 0x67 && qr.rem == 0x6F)
        {
            z80_reg_af.bytes.high.byte_value += 0x9A;
            register_set_or_unset_flag(FLAG_C, true);
        }
        register_a_set_flag_s_z_p();
        return t;
    case 0x2A: //LD HL,(HHLL)
        z80_reg_hl = memory_read16(z80_next16());
        return 16;
    case 0x2F: //CPL
        z80_reg_af.bytes.high.byte_value = ~z80_reg_af.bytes.high.byte_value;
        register_set_or_unset_flag(FLAG_N | FLAG_HC, true);
        return t;
    case 0x32: //LD (HHLL),A
        memory_write8(z80_next16(), z80_reg_af.bytes.high);
        return 16;
    case 0x37: //SCF
        register_set_or_unset_flag(FLAG_C, true);
        register_set_or_unset_flag(FLAG_N | FLAG_HC, false);
        return t;
    case 0x3A: //LD A,(HHLL)
        z80_reg_af.bytes.high = memory_read8(z80_next16());
        return 13;
    case 0x3F: //CCF
        c = register_is_flag(FLAG_C);
        register_set_or_unset_flag(FLAG_HC, c);
        register_set_or_unset_flag(FLAG_C, !c);
        register_set_or_unset_flag(FLAG_N, false);
        return t;
    case 0x40: //LD r,r
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47:
    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F:
    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57:
    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B:
    case 0x5C:
    case 0x5D:
    case 0x5E:
    case 0x5F:
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x69:
    case 0x6A:
    case 0x6B:
    case 0x6C:
    case 0x6D:
    case 0x6E:
    case 0x6F:
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x77:
    case 0x78:
    case 0x79:
    case 0x7A:
    case 0x7B:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F:
        *z80_decode_reg8(reg, 3, 3, &t) = *z80_decode_reg8(reg, 0, 3, &t);
        return t;
    case 0x80: //ADD A,r
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x86:
    case 0x87:
        register_add8_with_flags(&z80_reg_af.bytes.high, *z80_decode_reg8(reg, 0, 3, &t), MASK_ALL);
        return t;
    case 0x88: //ADC A,r
    case 0x89:
    case 0x8A:
    case 0x8B:
    case 0x8C:
    case 0x8D:
    case 0x8E:
    case 0x8F:
        z80_reg_af.bytes.high.byte_value += register_is_flag(FLAG_C);
        register_add8_with_flags(&z80_reg_af.bytes.high, *z80_decode_reg8(reg, 0, 3, &t), MASK_ALL);
        return t;
    case 0x90: //SUB A,r
    case 0x91:
    case 0x92:
    case 0x93:
    case 0x94:
    case 0x95:
    case 0x96:
    case 0x97:
        register_sub8_with_flags(&z80_reg_af.bytes.high, *z80_decode_reg8(reg, 0, 3, &t), MASK_ALL);
        return t;
    case 0x98: //SBC A,r
    case 0x99:
    case 0x9A:
    case 0x9B:
    case 0x9C:
    case 0x9D:
    case 0x9E:
    case 0x9F:
        z80_reg_af.bytes.high.byte_value -= register_is_flag(FLAG_C);
        register_sub8_with_flags(&z80_reg_af.bytes.high, *z80_decode_reg8(reg, 0, 3, &t), MASK_ALL);
        return t;
    case 0xA0: //AND A,r
    case 0xA1:
    case 0xA2:
    case 0xA3:
    case 0xA4:
    case 0xA5:
    case 0xA6:
    case 0xA7:
        z80_reg_af.bytes.high.byte_value &= z80_decode_reg8(reg, 0, 3, &t)->byte_value;
        register_a_set_flag_s_z_p();
        register_set_or_unset_flag(FLAG_N | FLAG_C, false);
        register_set_or_unset_flag(FLAG_HC, true);
        return t;
    case 0xA8: //XOR A,r
    case 0xA9:
    case 0xAA:
    case 0xAB:
    case 0xAC:
    case 0xAD:
    case 0xAE:
    case 0xAF:
        z80_reg_af.bytes.high.byte_value ^= z80_decode_reg8(reg, 0, 3, &t)->byte_value;
        register_a_set_flag_s_z_p();
        register_set_or_unset_flag(FLAG_C | FLAG_N | FLAG_HC, false);
        return t;
    case 0xB0: //OR A,r
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB6:
    case 0xB7:
        z80_reg_af.bytes.high.byte_value |= z80_decode_reg8(reg, 0, 3, &t)->byte_value;
        register_a_set_flag_s_z_p();
        register_set_or_unset_flag(FLAG_C | FLAG_N | FLAG_HC, false);
        return t;
    case 0xB8: //CP A,r
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
        alt = z80_reg_af.bytes.high;
        register_sub8_with_flags(&alt, *z80_decode_reg8(reg, 0, 3, &t), MASK_ALL);
        return t;
    case 0xC0: //RET CC
    case 0xC8:
        return z80_ret_with_condition(z80_decode_condition(reg));
    case 0xC1: //POP qq
    case 0xD1:
    case 0xE1:
    case 0xF1:
        *z80_bc_de_hl_af[reg.byte_value >> 4 & 0x03] = z80_pop16();
        return t;
    case 0xC2: //JP CC,HHLL
    case 0xCA:
		return z80_jump_with_condition(z80_decode_condition(reg));
	case 0xC3: //JP HHLL
		return z80_jump_with_condition(true);
	case 0xC4: //CALL CC,HHLL
	case 0xCC:
		return z80_call_with_condition(z80_decode_condition(reg));
	case 0xC5: //PUSH qq
	case 0xD5:
	case 0xE5:
	case 0xF5:
        z80_push16(*z80_bc_de_hl_af[reg.byte_value >> 4 & 0x03]);
        return 10;
    case 0xC6: //ADD A,NN
		register_add8_with_flags(&z80_reg_af.bytes.high, z80_next8(), MASK_ALL);
		return 7;
	case 0xC7: //RST p
		z80_push16(z80_reg_pc);
		z80_reg_pc.value = z80_rst_addr[reg.byte_value >> 3 & 0x07];
		return 11;
	case 0xC9: //RET
		z80_ret_with_condition(true);
		return 10;
	case 0xCB: //CB
		reg = z80_fetch_opcode();
		switch (reg.byte_value) {
			default:
				return 0; //fail
		}
	case 0xCD: //CALL HHLL
		return z80_call_with_condition(true);
	case 0xCE: //ADC A,NN
        z80_reg_af.bytes.high.byte_value += register_is_flag(FLAG_C);
        register_add8_with_flags(&z80_reg_af.bytes.high, z80_next8(), MASK_ALL);
        return 7;
    default:
        return 0; // fail
    }
}

int z80_nonmaskable_interrupt()
{
    z80_iff2 = z80_iff1;
    z80_iff1 = false;
    z80_push16(z80_reg_pc);
    z80_reg_pc.byte_value = 0x66;
    return 11;
}

int z80_maskable_interrupt()
{
    z80_iff1 = false;
    z80_iff2 = false;
    switch (z80_imode)
    {
    case 0:
        // TODO: wait 2 cycles for interrupting device to write to data_bus
        z80_memory_refresh();
        return z80_execute(z80_data_bus) + 2;
    case 1:
        z80_push16(z80_reg_pc);
        z80_reg_pc.byte_value = 0x38;
        return 13;
    case 2:
        z80_push16(z80_reg_pc);
        z80_reg_pc = memory_read16((REG16){.bytes.high = z80_reg_i, .bytes.low = z80_data_bus});
        return 19;
    default:
        return 0; // fail
    }
}

int z80_run_one()
{
    if (z80_nonmaskable_interrupt_flag)
    {
        z80_nonmaskable_interrupt_flag = false;
        return z80_nonmaskable_interrupt();
    }
    else if (z80_maskable_interrupt_flag)
    {
        z80_maskable_interrupt_flag = false;
        if (z80_iff1)
        {
            return z80_maskable_interrupt();
        }
    }
    else if (z80_can_execute)
    {
        return z80_execute(z80_fetch_opcode());
    }
    return 0;
}

void *z80_run(void *args)
{
    while (running)
    {
        time_sync(&z80_t_states_all, z80_run_one());
    }
    return NULL;
}

void ula_point(const int x, const int y, const int c, const bool b)
{
    RGB color = b ? ula_bright_colors[c] : ula_colors[c];
    glColor3f(color.red, color.green, color.blue);
    glBegin(GL_POINTS);
    glVertex2f((x + 48.0f) / 304.0f, (y + 48.0f) / 288.0f);
    glEnd();
}

int ula_draw_line(int y)
{
    if (y > 63 && y < 256)
    {
        int x = 0;
        ula_addr_attrib.byte_value = 0x5800 + y / 8 * 32;
        for (int i = 0; i < 32; i++)
        {
            REG8 reg_bitmap = memory_read8(ula_addr_bitmap);
            REG8 reg_attrib = memory_read8(ula_addr_attrib);
            int ink = reg_attrib.byte_value & 7;
            int paper = reg_attrib.byte_value >> 3 & 7;
            bool flash = register_is_bit(reg_attrib, MAX7);
            if (flash && ula_draw_counter == 0)
            {
                int temp = ink;
                ink = paper;
                paper = temp;
            }
            bool brightness = register_is_bit(reg_attrib, MAX6);
            int b = MAX7;
            for (int j = 0; j < 8; j++)
            {
                ula_point(x + j, y, register_is_bit(reg_bitmap, b) ? ink : paper, brightness);
                b >>= 1;
            }
            ula_addr_bitmap.byte_value++;
            ula_addr_attrib.byte_value++;
            x += 8;
        }
        y++;
        register_set_or_unset_bit(ula_addr_bitmap, 5, is_bit(y, MAX3));
        register_set_or_unset_bit(ula_addr_bitmap, 6, is_bit(y, MAX4));
        register_set_or_unset_bit(ula_addr_bitmap, 7, is_bit(y, MAX5));
        register_set_or_unset_bit(ula_addr_bitmap, 8, is_bit(y, MAX0));
        register_set_or_unset_bit(ula_addr_bitmap, 9, is_bit(y, MAX1));
        register_set_or_unset_bit(ula_addr_bitmap, 10, is_bit(y, MAX2));
        register_set_or_unset_bit(ula_addr_bitmap, 11, is_bit(y, MAX6));
        register_set_or_unset_bit(ula_addr_bitmap, 12, is_bit(y, MAX7));
    }
    return 224;
}

void ula_draw_screen_once()
{
    ula_addr_bitmap.byte_value = 0x4000;
    ula_addr_attrib.byte_value = 0;
    for (int i = 0; i < 312; i++)
    {
        time_sync(&ula_t_states_all, ula_draw_line(i));
    }
    glFlush();
}

void *ula_draw_screen(void *args)
{
    while (running)
    {
        z80_maskable_interrupt_flag = true;
        ula_draw_screen_once();
        ula_draw_counter = (ula_draw_counter + 1) % 16;
    }
    return NULL;
}

int main(int argc, char **argv)
{
    pthread_t z80_id, ula_id;
    if (system_little_endian())
    {
        glutInit(&argc, argv);
        glutInitDisplayMode(GLUT_SINGLE);
        glutInitWindowSize(304, 288);

        // glutInitWindowPosition(200, 100);
        glutCreateWindow("Cristian Mocanu Z80");
        glPointSize(1.0f);
        glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // glutDisplayFunc(renderScene);
        glutKeyboardFunc(keyboard_press_down);
        glutKeyboardUpFunc(keyboard_press_up);
        if (argc == 2)
        {
            memory_load_rom(argv[1]);
        }
        z80_reset();
        pthread_create(&z80_id, NULL, z80_run, NULL);
        pthread_create(&ula_id, NULL, ula_draw_screen, NULL);
        glutMainLoop();
    }
    running = false;
    return 0;
}

// TODO: bright colors
// TODO: keyboard caps lock and shift
// TODO: border, UART, sound, tape
// TODO: debugger
