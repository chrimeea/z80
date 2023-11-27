// gcc main.c -Ofast -lGLEW -lGLU -lGL -lglut -pthread -lm -Wall
// SHIFT = SS; ALT = CS; ESC = SS + CS
// mkfifo tape
// cat file.tzx > tape

#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <GL/freeglut.h>
// #include <sys/ioctl.h>
// #include <linux/kd.h>

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
#define MASK_HNC 0x13
#define MASK_HVNC 0x17
#define MASK_NONE 0x00

#define REG8_ONE \
    (REG8) { .value = 1 }
#define REG16_ONE \
    (REG16) { .value = 1 }

#define SCREEN_WIDTH 352
#define SCREEN_HEIGHT 304

#define sign(X) (X < 0)
#define is_bit(I, B) (I & (B))
#define register_is_zero(X) ((X).value == 0)
#define register_is_bit(R, B) (is_bit((R).byte_value, B))
#define register_is_flag(B) (register_is_bit(z80_reg_af.bytes.low, B))
#define set_bit(I, B) (I |= (B))
#define unset_bit(I, B) (I &= ~(B))
#define set_or_unset_bit(I, B, V) (V ? set_bit(I, B) : unset_bit(I, B))
#define register_set_or_unset_bit(R, B, V) (set_or_unset_bit((R).byte_value, B, V))
#define register_set_or_unset_flag(B, V) (register_set_or_unset_bit(z80_reg_af.bytes.low, B, V))
#define register_split_8_to_4(R) (div((R).byte_value, MAX4))
#define time_ts_to_seconds(T) (T.tv_sec + T.tv_nsec / 1000000000.0L)

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

typedef struct T
{
    unsigned long t_states;
    void (*task)();
    struct T *next;
} TASK;

REG8 memory[MAX16];
RGB ula_screen[SCREEN_HEIGHT][SCREEN_WIDTH];
RGB ula_border[SCREEN_HEIGHT];
REG8 keyboard[] = {(REG8){.value = 0x1F}, (REG8){.value = 0x1F}, (REG8){.value = 0x1F},
                   (REG8){.value = 0x1F}, (REG8){.value = 0x1F}, (REG8){.value = 0x1F}, (REG8){.value = 0x1F},
                   (REG8){.value = 0x1F}};
RGB ula_colors[] = {(RGB){0.0f, 0.0f, 0.0f}, (RGB){0.0f, 0.0f, 0.9f},
                    (RGB){0.5f, 0.0f, 0.0f}, (RGB){0.4f, 0.0f, 0.4f},
                    (RGB){0.0f, 0.9f, 0.0f}, (RGB){0.0f, 0.4f, 0.4f},
                    (RGB){0.9f, 0.9f, 0.0f}, (RGB){0.9f, 0.9f, 0.9f}};
RGB ula_bright_colors[] = {(RGB){0.0f, 0.0f, 0.0f}, (RGB){0.0f, 0.0f, 1.0f},
                           (RGB){1.0f, 0.0f, 0.0f}, (RGB){0.5f, 0.0f, 0.5f},
                           (RGB){0.0f, 1.0f, 0.0f}, (RGB){0.0f, 1.0f, 1.0f},
                           (RGB){1.0f, 1.0f, 0.0f}, (RGB){1.0f, 1.0f, 1.0f}};
const unsigned int memory_size = MAX16;
long double time_start, state_duration = 0.00000025L;
unsigned long z80_t_states_all = 0, ula_t_states_all = 0;
unsigned int ula_draw_counter = 0;
int ula_border_color;
// int sound_console_fd;
bool sound_ear = false, sound_mic = false;
REG16 ula_addr_bitmap, ula_addr_attrib;
REG16 z80_reg_bc, z80_reg_de, z80_reg_hl, z80_reg_af, z80_reg_pc, z80_reg_sp, z80_reg_ix, z80_reg_iy;
REG16 z80_reg_bc_2, z80_reg_de_2, z80_reg_hl_2, z80_reg_af_2, z80_reg_pc_2, z80_reg_sp_2, z80_reg_ix_2, z80_reg_iy_2;
REG8 z80_reg_i, z80_reg_r, z80_data_bus;
REG8 *z80_all8[] = {&z80_reg_bc.bytes.high, &z80_reg_bc.bytes.low,
                    &z80_reg_de.bytes.high, &z80_reg_de.bytes.low,
                    &z80_reg_hl.bytes.high, &z80_reg_hl.bytes.low,
                    NULL, &z80_reg_af.bytes.high};
REG16 *z80_bc_de_hl_sp[] = {&z80_reg_bc, &z80_reg_de, &z80_reg_hl, &z80_reg_sp};
REG16 *z80_bc_de___sp[] = {&z80_reg_bc, &z80_reg_de, &z80_reg_ix, &z80_reg_sp};
REG16 *z80_bc_de_hl_af[] = {&z80_reg_bc, &z80_reg_de, &z80_reg_hl, &z80_reg_af};
int z80_rst_addr[] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};
bool running;
bool z80_maskable_interrupt_flag, z80_nonmaskable_interrupt_flag;
bool z80_iff1, z80_iff2, z80_can_execute, z80_halt;
int z80_imode;
int tape_pause, tape_block_size;
REG8 tape_block[2097152];
TASK *rt_timeline_head, *rt_pending;
// int debug = 10;

void to_binary(unsigned char c, char *o)
{
    o[8] = 0;
    o[7] = (c & MAX0) ? '1' : '0';
    o[6] = (c & MAX1) ? '1' : '0';
    o[5] = (c & MAX2) ? '1' : '0';
    o[4] = (c & MAX3) ? '1' : '0';
    o[3] = (c & MAX4) ? '1' : '0';
    o[2] = (c & MAX5) ? '1' : '0';
    o[1] = (c & MAX6) ? '1' : '0';
    o[0] = (c & MAX7) ? '1' : '0';
}

int system_little_endian()
{
    int x = 1;
    return *(char *)&x;
}

long double time_in_seconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return time_ts_to_seconds(ts);
}

void time_seconds_to_timespec(struct timespec *ts, long double s)
{
    long double temp;
    ts->tv_nsec = modfl(s, &temp) * 1000000000.0L;
    ts->tv_sec = temp;
}

void time_sleep_in_seconds(long double s)
{
    struct timespec ts;
    if (s > 0.0L)
    {
        time_seconds_to_timespec(&ts, s);
        nanosleep(&ts, &ts);
    }
}

void time_sync(unsigned long *t_states_all, int t_states)
{
    *t_states_all += t_states;
    time_sleep_in_seconds(time_start + *t_states_all * state_duration - time_in_seconds());
}

void register_set_8_from_4(REG8 *reg, div_t d)
{
    reg->byte_value = d.quot * MAX4 + d.rem;
}
void register_exchange16(REG16 *reg, REG16 *alt)
{
    REG16 temp = *reg;
    *reg = *alt;
    *alt = temp;
}

void register_set_flag_s_z_p(REG8 reg, int mask)
{
    register_set_or_unset_flag(FLAG_S & mask, sign(reg.value));
    register_set_or_unset_flag(FLAG_Z & mask, register_is_zero(reg));
    register_set_or_unset_flag(FLAG_PV & mask, !__builtin_parity(reg.byte_value));
}

void register_left8_with_flags(REG8 *reg, int mask, bool b)
{
    register_set_or_unset_flag(FLAG_C & mask, reg->byte_value & MAX7);
    register_set_or_unset_flag(FLAG_HC & mask, false);
    register_set_or_unset_flag(FLAG_N & mask, false);
    reg->byte_value <<= 1;
    set_or_unset_bit(reg->byte_value, MAX0, b);
    register_set_flag_s_z_p(*reg, mask);
}

void register_right8_with_flags(REG8 *reg, int mask, bool b)
{
    register_set_or_unset_flag(FLAG_C & mask, reg->byte_value & MAX0);
    register_set_or_unset_flag(FLAG_HC & mask, false);
    register_set_or_unset_flag(FLAG_N & mask, false);
    reg->byte_value >>= 1;
    set_or_unset_bit(reg->byte_value, MAX7, b);
    register_set_flag_s_z_p(*reg, mask);
}

void register_add8_with_flags(REG8 *reg, REG8 alt, int mask)
{
    REG8 other;
    int r = reg->byte_value + alt.byte_value;
    other.byte_value = r;
    bool s = sign(other.value);
    register_set_or_unset_flag(FLAG_C & mask, r >= MAX8);
    register_set_or_unset_flag(FLAG_HC & mask, (reg->byte_value & 0x0F) + (alt.byte_value & 0x0F) >= MAX4);
    register_set_or_unset_flag(FLAG_N & mask, false);
    register_set_or_unset_flag(FLAG_PV & mask, sign(reg->value) == sign(alt.value) && s != sign(reg->value));
    register_set_or_unset_flag(FLAG_S & mask, s);
    register_set_or_unset_flag(FLAG_Z & mask, other.value == 0);
    *reg = other;
}

void register_sub8_with_flags(REG8 *reg, REG8 alt, int mask)
{
    REG8 other;
    other.value = reg->value - alt.value;
    bool s = sign(other.value);
    register_set_or_unset_flag(FLAG_C & mask, alt.byte_value > reg->byte_value);
    register_set_or_unset_flag(FLAG_HC & mask, (alt.byte_value & 0x0F) > (reg->byte_value & 0x0F));
    register_set_or_unset_flag(FLAG_N & mask, true);
    register_set_or_unset_flag(FLAG_PV & mask, sign(reg->value) != sign(alt.value) && s != sign(reg->value));
    register_set_or_unset_flag(FLAG_S & mask, s);
    register_set_or_unset_flag(FLAG_Z & mask, other.value == 0);
    *reg = other;
}

void register_add16_with_flags(REG16 *reg, REG16 alt, int mask)
{
    REG16 other;
    int r = reg->byte_value + alt.byte_value;
    other.byte_value = r;
    bool s = sign(other.value);
    register_set_or_unset_flag(FLAG_C & mask, r >= MAX16);
    register_set_or_unset_flag(FLAG_HC & mask, (reg->byte_value & 0xFFF) + (alt.byte_value & 0xFFF) >= MAX12);
    register_set_or_unset_flag(FLAG_N & mask, false);
    register_set_or_unset_flag(FLAG_PV & mask, sign(reg->value) == sign(alt.value) && s != sign(reg->value));
    register_set_or_unset_flag(FLAG_S & mask, s);
    register_set_or_unset_flag(FLAG_Z & mask, other.value == 0);
    *reg = other;
}

void register_sub16_with_flags(REG16 *reg, REG16 alt, int mask)
{
    REG16 other;
    other.value = reg->value - alt.value;
    bool s = sign(other.value);
    register_set_or_unset_flag(FLAG_C & mask, alt.byte_value > reg->byte_value);
    register_set_or_unset_flag(FLAG_HC & mask, (alt.byte_value & 0xFFF) > (reg->byte_value & 0xFFF));
    register_set_or_unset_flag(FLAG_N & mask, true);
    register_set_or_unset_flag(FLAG_PV & mask, sign(reg->value) != sign(alt.value) && s != sign(reg->value));
    register_set_or_unset_flag(FLAG_S & mask, s);
    register_set_or_unset_flag(FLAG_Z & mask, other.value == 0);
    *reg = other;
}

void memory_load_rom(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    fread(memory, 1, memory_size, f);
    fclose(f);
}

REG8 memory_read8(const REG16 reg)
{
    return memory[reg.byte_value];
}

void memory_write8(const REG16 reg, const REG8 alt)
{
    memory[reg.byte_value] = alt;
}

REG8 memory_read8_indexed(const REG16 reg16, const REG8 reg8)
{
    return memory[reg16.byte_value + reg8.value];
}

void memory_write8_indexed(const REG16 reg16, const REG8 reg8, REG8 alt)
{
    memory[reg16.byte_value + reg8.value] = alt;
}

REG16 memory_read16(REG16 reg)
{
    return *((REG16 *)&memory[reg.byte_value]);
}

void memory_write16(REG16 reg, REG16 alt)
{
    *((REG16 *)&memory[reg.byte_value]) = alt;
}

REG8 *memory_ref8(const REG16 reg)
{
    return &memory[reg.byte_value];
}

REG8 *memory_ref8_indexed(const REG16 reg16, const REG8 reg8)
{
    return &memory[reg16.byte_value + reg8.value];
}

REG16 *memory_ref16(REG16 reg)
{
    return (REG16 *)memory_ref8(reg);
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
    // if (alt.byte_value == 0x1e && reg.byte_value == 0xbffe) {
    //     debug = 0;
    //     z80_iff1 = z80_iff2 = false;
    // }
    return alt;
}

void keyboard_press(unsigned char key, const bool value)
{
    int modifier = glutGetModifiers();
    if (modifier & GLUT_ACTIVE_ALT || value)
    {
        register_set_or_unset_bit(keyboard[0], MAX0, value);
    }
    if (modifier & GLUT_ACTIVE_SHIFT || value)
    {
        register_set_or_unset_bit(keyboard[7], MAX1, value);
    }
    switch (tolower(key))
    {
    case 'z':
        register_set_or_unset_bit(keyboard[0], MAX1, value);
        break;
    case 'x':
        register_set_or_unset_bit(keyboard[0], MAX2, value);
        break;
    case 'c':
        register_set_or_unset_bit(keyboard[0], MAX3, value);
        break;
    case 'v':
        register_set_or_unset_bit(keyboard[0], MAX4, value);
        break;
    case 'a':
        register_set_or_unset_bit(keyboard[1], MAX0, value);
        break;
    case 's':
        register_set_or_unset_bit(keyboard[1], MAX1, value);
        break;
    case 'd':
        register_set_or_unset_bit(keyboard[1], MAX2, value);
        break;
    case 'f':
        register_set_or_unset_bit(keyboard[1], MAX3, value);
        break;
    case 'g':
        register_set_or_unset_bit(keyboard[1], MAX4, value);
        break;
    case 'q':
        register_set_or_unset_bit(keyboard[2], MAX0, value);
        break;
    case 'w':
        register_set_or_unset_bit(keyboard[2], MAX1, value);
        break;
    case 'e':
        register_set_or_unset_bit(keyboard[2], MAX2, value);
        break;
    case 'r':
        register_set_or_unset_bit(keyboard[2], MAX3, value);
        break;
    case 't':
        register_set_or_unset_bit(keyboard[2], MAX4, value);
        break;
    case '1':
    case '!':
        register_set_or_unset_bit(keyboard[3], MAX0, value);
        break;
    case '2':
    case '@':
        register_set_or_unset_bit(keyboard[3], MAX1, value);
        break;
    case '3':
    case '#':
        register_set_or_unset_bit(keyboard[3], MAX2, value);
        break;
    case '4':
    case '$':
        register_set_or_unset_bit(keyboard[3], MAX3, value);
        break;
    case '5':
    case '%':
        register_set_or_unset_bit(keyboard[3], MAX4, value);
        break;
    case '0':
    case ')':
        register_set_or_unset_bit(keyboard[4], MAX0, value);
        break;
    case '9':
    case '(':
        register_set_or_unset_bit(keyboard[4], MAX1, value);
        break;
    case '8':
    case '*':
        register_set_or_unset_bit(keyboard[4], MAX2, value);
        break;
    case '7':
    case '&':
        register_set_or_unset_bit(keyboard[4], MAX3, value);
        break;
    case '6':
    case '^':
        register_set_or_unset_bit(keyboard[4], MAX4, value);
        break;
    case 'p':
        register_set_or_unset_bit(keyboard[5], MAX0, value);
        break;
    case 'o':
        register_set_or_unset_bit(keyboard[5], MAX1, value);
        break;
    case 'i':
        register_set_or_unset_bit(keyboard[5], MAX2, value);
        break;
    case 'u':
        register_set_or_unset_bit(keyboard[5], MAX3, value);
        break;
    case 'y':
        register_set_or_unset_bit(keyboard[5], MAX4, value);
        break;
    case 13:
        register_set_or_unset_bit(keyboard[6], MAX0, value);
        break;
    case 'l':
        register_set_or_unset_bit(keyboard[6], MAX1, value);
        break;
    case 'k':
        register_set_or_unset_bit(keyboard[6], MAX2, value);
        break;
    case 'j':
        register_set_or_unset_bit(keyboard[6], MAX3, value);
        break;
    case 'h':
        register_set_or_unset_bit(keyboard[6], MAX4, value);
        break;
    case ' ':
        register_set_or_unset_bit(keyboard[7], MAX0, value);
        break;
    case 'm':
        register_set_or_unset_bit(keyboard[7], MAX2, value);
        break;
    case 'n':
        register_set_or_unset_bit(keyboard[7], MAX3, value);
        break;
    case 'b':
        register_set_or_unset_bit(keyboard[7], MAX4, value);
        break;
    case 27:
        register_set_or_unset_bit(keyboard[0], MAX0, value);
        register_set_or_unset_bit(keyboard[7], MAX1, value);
        break;
    }
}

void keyboard_press_down(unsigned char key, int x, int y)
{
    keyboard_press(key, false);
}

void keyboard_press_up(unsigned char key, int x, int y)
{
    keyboard_press(key, true);
}

void sound_ear_on_off(bool on)
{
    if (on && !sound_ear)
    {
        sound_ear = true;
        // ioctl(sound_console_fd, KIOCSOUND, 2147483647);
    }
    else if (!on && sound_ear)
    {
        sound_ear = false;
        // ioctl(sound_console_fd, KIOCSOUND, 0);
    }
}

REG8 port_read8(const REG16 reg)
{
    if (reg.bytes.low.byte_value % 2 == 0)
    {
        REG8 alt = keyboard_read8(reg);
        register_set_or_unset_bit(alt, MAX6, sound_ear);
        return alt;
    }
    else
    {
        return (REG8){.byte_value = 0xFF};
    }
}

void port_write8(const REG16 reg, const REG8 alt)
{
    if (reg.bytes.low.byte_value % 2 == 0)
    {
        ula_border_color = alt.byte_value & 0x07;
        sound_mic = ((alt.byte_value & MAX3) == 0);
        sound_ear_on_off(alt.byte_value & MAX4);
    }
}

void z80_print()
{
    char o[9];
    to_binary(z80_reg_af.bytes.low.byte_value, o);
    printf("  BC   DE   HL   AF   PC   SP   IX   IY  I  RIM  IFF1 SZ5H3PNC\n");
    printf("%04x %04x %04x %04x %04x %04x %04x %04x %02x %02x %d %s %s\n",
           z80_reg_bc.byte_value,
           z80_reg_de.byte_value,
           z80_reg_hl.byte_value,
           z80_reg_af.byte_value,
           z80_reg_pc.byte_value,
           z80_reg_sp.byte_value,
           z80_reg_ix.byte_value,
           z80_reg_iy.byte_value,
           z80_reg_i.byte_value,
           z80_reg_r.byte_value,
           z80_imode,
           z80_iff1 ? " true" : "false",
           o);
}

void z80_reset()
{
    z80_nonmaskable_interrupt_flag = false;
    z80_maskable_interrupt_flag = false;
    z80_iff1 = z80_iff2 = false;
    z80_halt = false;
    z80_can_execute = true;
    z80_imode = 0;
    z80_reg_af.byte_value = 0xFFFF;
    z80_reg_sp.byte_value = 0xFFFF;
    z80_reg_pc.byte_value = 0;
    z80_reg_r.byte_value = 0;
    running = true;
    time_start = time_in_seconds();
    z80_t_states_all = ula_t_states_all = 0;
    sound_mic = false;
    sound_ear_on_off(false);
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

REG8 *z80_decode_reg8(REG8 reg, int pos, bool *r)
{
    int i = reg.byte_value >> pos & 0x07;
    if (i == 0x06)
    {
        *r = true;
        return memory_ref8(z80_reg_hl);
    }
    else
    {
        *r = false;
        return z80_all8[i];
    }
}

bool z80_decode_condition(REG8 reg)
{
    int i = reg.byte_value >> 3 & 0x07;
    switch (i)
    {
    case 0x00:
        return !register_is_flag(FLAG_Z);
    case 0x01:
        return register_is_flag(FLAG_Z);
    case 0x02:
        return !register_is_flag(FLAG_C);
    case 0x03:
        return register_is_flag(FLAG_C);
    case 0x04:
        return !register_is_flag(FLAG_PV);
    case 0x05:
        return register_is_flag(FLAG_PV);
    case 0x06:
        return !register_is_flag(FLAG_S);
    case 0x07:
        return register_is_flag(FLAG_S);
    default:
        return 0; // fail
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
    }
    else
    {
        return 7;
    }
}

int z80_ret_with_condition(bool c)
{
    if (c)
    {
        z80_reg_pc = z80_pop16();
        return 11;
    }
    else
    {
        return 5;
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
    REG8 *alt;
    REG8 duplicate_a = z80_reg_af.bytes.high;
    REG16 *other = &z80_reg_iy;
    div_t qr, qr_alt;
    bool c, hc, hl;
    int mask = MASK_ALL;
    if (z80_halt)
    {
        reg.byte_value = 0x00;
        z80_reg_pc.byte_value--;
    }
    switch (reg.byte_value)
    {
    case 0x00: // NOP
        return 4;
    case 0x01: // LD dd,nn
    case 0x11:
    case 0x21:
    case 0x31:
        *z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03] = z80_next16();
        return 10;
    case 0x02: // LD (BC),A
        memory_write8(z80_reg_bc, z80_reg_af.bytes.high);
        return 7;
    case 0x03: // INC ss
    case 0x13:
    case 0x23:
    case 0x33:
        register_add16_with_flags(z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03], REG16_ONE, MASK_NONE);
        return 6;
    case 0x04: // INC r
    case 0x0C:
    case 0x14:
    case 0x1C:
    case 0x24:
    case 0x2C:
    case 0x34:
    case 0x3C:
        register_add8_with_flags(z80_decode_reg8(reg, 3, &hl), REG8_ONE, MASK_SZHVN);
        return hl ? 11 : 4;
    case 0x05: // DEC r
    case 0x0D:
    case 0x15:
    case 0x1D:
    case 0x25:
    case 0x2D:
    case 0x35:
    case 0x3D:
        register_sub8_with_flags(z80_decode_reg8(reg, 3, &hl), REG8_ONE, MASK_SZHVN);
        return hl ? 11 : 4;
    case 0x06: // LD r,n
    case 0x0E:
    case 0x16:
    case 0x1E:
    case 0x26:
    case 0x2E:
    case 0x36:
    case 0x3E:
        *z80_decode_reg8(reg, 3, &hl) = z80_next8();
        return hl ? 10 : 7;
    case 0x07: // RLCA
        register_left8_with_flags(&z80_reg_af.bytes.high, MASK_HNC, register_is_bit(z80_reg_af.bytes.high, MAX7));
        return 4;
    case 0x08: // EX AF,AF’
        register_exchange16(&z80_reg_af, &z80_reg_af_2);
        return 4;
    case 0x09: // ADD HL,ss
    case 0x19:
    case 0x29:
    case 0x39:
        register_add16_with_flags(&z80_reg_hl, *z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03], MASK_HNC);
        return 11;
    case 0x0A: // LD A,(BC)
        z80_reg_af.bytes.high = memory_read8(z80_reg_bc);
        return 7;
    case 0x0B: // DEC ss
    case 0x1B:
    case 0x2B:
    case 0x3B:
        register_sub16_with_flags(z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03], REG16_ONE, MASK_NONE);
        return 6;
    case 0x0F: // RRCA
        register_right8_with_flags(&z80_reg_af.bytes.high, MASK_HNC, register_is_bit(z80_reg_af.bytes.high, MAX0));
        return 4;
    case 0x10: // DJNZ e
        alt = &(REG8){.value = z80_next8().value};
        z80_reg_bc.bytes.high.byte_value--;
        if (register_is_zero(z80_reg_bc.bytes.high))
        {
            return 8;
        }
        else
        {
            z80_reg_pc.byte_value += alt->value;
            return 13;
        }
    case 0x12: // LD (DE),A
        memory_write8(z80_reg_de, z80_reg_af.bytes.high);
        return 7;
    case 0x17: // RLA
        register_left8_with_flags(&z80_reg_af.bytes.high, MASK_HNC, register_is_flag(FLAG_C));
        return 4;
    case 0x18: // JR e
        return z80_jump_rel_with_condition(true);
    case 0x1A: // LD A,(DE)
        z80_reg_af.bytes.high = memory_read8(z80_reg_de);
        return 7;
    case 0x1F: // RRA
        register_right8_with_flags(&z80_reg_af.bytes.high, MASK_HNC, register_is_flag(FLAG_C));
        return 4;
    case 0x20: // JR NZ,e
        return z80_jump_rel_with_condition(!register_is_flag(FLAG_Z));
    case 0x28: // JR Z,e
        return z80_jump_rel_with_condition(register_is_flag(FLAG_Z));
    case 0x30: // JR NC,e
        return z80_jump_rel_with_condition(!register_is_flag(FLAG_C));
    case 0x38: // JR C,e
        return z80_jump_rel_with_condition(register_is_flag(FLAG_C));
    case 0x22: // LD (nn),HL
        memory_write16(z80_next16(), z80_reg_hl);
        return 16;
    case 0x27: // DAA
        qr = register_split_8_to_4(z80_reg_af.bytes.high);
        c = register_is_flag(FLAG_C);
        hc = register_is_flag(FLAG_HC);
        if (c == false && hc == false && qr.quot <= 0x09 && qr.rem <= 0x09)
        {
        }
        else if (c == false && hc == false && qr.quot <= 0x08 && qr.rem >= 0x0A && qr.rem <= 0x0F)
        {
            z80_reg_af.bytes.high.byte_value += 0x06;
        }
        else if (c == false && hc == true && qr.quot <= 0x09 && qr.rem <= 0x03)
        {
            z80_reg_af.bytes.high.byte_value += 0x06;
        }
        else if (c == false && hc == false && qr.quot >= 0x0A && qr.quot <= 0x0F && qr.rem <= 0x09)
        {
            z80_reg_af.bytes.high.byte_value += 0x60;
            register_set_or_unset_flag(FLAG_C, true);
        }
        else if (c == false && hc == false && qr.quot >= 0x09 && qr.quot <= 0x0F && qr.rem >= 0x0A && qr.rem <= 0x0F)
        {
            z80_reg_af.bytes.high.byte_value += 0x66;
            register_set_or_unset_flag(FLAG_C, true);
        }
        else if (c == false && hc == true && qr.quot >= 0x0A && qr.quot <= 0x0F && qr.rem <= 0x03)
        {
            z80_reg_af.bytes.high.byte_value += 0x66;
            register_set_or_unset_flag(FLAG_C, true);
        }
        else if (c == true && hc == false && qr.quot <= 0x02 && qr.rem <= 0x09)
        {
            z80_reg_af.bytes.high.byte_value += 0x60;
        }
        else if (c == true && hc == false && qr.quot <= 0x02 && qr.rem >= 0x0A && qr.rem <= 0x0F)
        {
            z80_reg_af.bytes.high.byte_value += 0x66;
        }
        else if (c == true && hc == true && qr.quot <= 0x03 && qr.rem <= 0x03)
        {
            z80_reg_af.bytes.high.byte_value += 0x66;
        }
        else if (c == false && hc == true && qr.quot <= 0x08 && qr.rem >= 0x06 && qr.rem <= 0x0F)
        {
            z80_reg_af.bytes.high.byte_value += 0xFA;
        }
        else if (c == true && hc == false && qr.quot >= 0x07 && qr.quot <= 0x0F && qr.rem <= 0x09)
        {
            z80_reg_af.bytes.high.byte_value += 0xA0;
        }
        else if (c == true && hc == true && qr.quot >= 0x06 && qr.quot <= 0x07 && qr.rem >= 0x06 && qr.rem <= 0x0F)
        {
            z80_reg_af.bytes.high.byte_value += 0x9A;
        }
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        return 4;
    case 0x2A: // LD HL,(nn)
        z80_reg_hl = memory_read16(z80_next16());
        return 16;
    case 0x2F: // CPL
        z80_reg_af.bytes.high.byte_value = ~z80_reg_af.bytes.high.byte_value;
        register_set_or_unset_flag(FLAG_N | FLAG_HC, true);
        return 4;
    case 0x32: // LD (nn),A
        memory_write8(z80_next16(), z80_reg_af.bytes.high);
        return 13;
    case 0x37: // SCF
        register_set_or_unset_flag(FLAG_C, true);
        register_set_or_unset_flag(FLAG_N | FLAG_HC, false);
        return 4;
    case 0x3A: // LD A,(nn)
        z80_reg_af.bytes.high = memory_read8(z80_next16());
        return 13;
    case 0x3F: // CCF
        c = register_is_flag(FLAG_C);
        register_set_or_unset_flag(FLAG_HC, c);
        register_set_or_unset_flag(FLAG_C, !c);
        register_set_or_unset_flag(FLAG_N, false);
        return 4;
    case 0x40: // LD r,r
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
        alt = z80_decode_reg8(reg, 0, &hl);
        *z80_decode_reg8(reg, 3, &hl) = *alt;
        return hl ? 7 : 4;
    case 0x76: // HALT
        z80_halt = true;
        return 4;
    case 0x80: // ADD A,r
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x86:
    case 0x87:
        register_add8_with_flags(&z80_reg_af.bytes.high, *z80_decode_reg8(reg, 0, &hl), MASK_ALL);
        return hl ? 7 : 4;
    case 0x88: // ADC A,r
    case 0x89:
    case 0x8A:
    case 0x8B:
    case 0x8C:
    case 0x8D:
    case 0x8E:
    case 0x8F:
        c = register_is_flag(FLAG_C);
        register_add8_with_flags(&z80_reg_af.bytes.high, *z80_decode_reg8(reg, 0, &hl), mask);
        if (c)
        {
            mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
            register_add8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
        }
        return hl ? 7 : 4;
    case 0x90: // SUB A,r
    case 0x91:
    case 0x92:
    case 0x93:
    case 0x94:
    case 0x95:
    case 0x96:
    case 0x97:
        register_sub8_with_flags(&z80_reg_af.bytes.high, *z80_decode_reg8(reg, 0, &hl), MASK_ALL);
        return hl ? 7 : 4;
    case 0x98: // SBC A,r
    case 0x99:
    case 0x9A:
    case 0x9B:
    case 0x9C:
    case 0x9D:
    case 0x9E:
    case 0x9F:
        c = register_is_flag(FLAG_C);
        register_sub8_with_flags(&z80_reg_af.bytes.high, *z80_decode_reg8(reg, 0, &hl), mask);
        if (c)
        {
            mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
            register_sub8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
        }
        return hl ? 7 : 4;
    case 0xA0: // AND r
    case 0xA1:
    case 0xA2:
    case 0xA3:
    case 0xA4:
    case 0xA5:
    case 0xA6:
    case 0xA7:
        z80_reg_af.bytes.high.byte_value &= z80_decode_reg8(reg, 0, &hl)->byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_N | FLAG_C, false);
        register_set_or_unset_flag(FLAG_HC, true);
        return hl ? 7 : 4;
    case 0xA8: // XOR r
    case 0xA9:
    case 0xAA:
    case 0xAB:
    case 0xAC:
    case 0xAD:
    case 0xAE:
    case 0xAF:
        z80_reg_af.bytes.high.byte_value ^= z80_decode_reg8(reg, 0, &hl)->byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_C | FLAG_N | FLAG_HC, false);
        return hl ? 7 : 4;
    case 0xB0: // OR r
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB6:
    case 0xB7:
        z80_reg_af.bytes.high.byte_value |= z80_decode_reg8(reg, 0, &hl)->byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_C | FLAG_N | FLAG_HC, false);
        return hl ? 7 : 4;
    case 0xB8: // CP r
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
        register_sub8_with_flags(&duplicate_a, *z80_decode_reg8(reg, 0, &hl), MASK_ALL);
        return hl ? 7 : 4;
    case 0xC0: // RET CC
    case 0xC8:
    case 0xD0:
    case 0xD8:
    case 0xE0:
    case 0xE8:
    case 0xF0:
    case 0xF8:
        return z80_ret_with_condition(z80_decode_condition(reg));
    case 0xC1: // POP qq
    case 0xD1:
    case 0xE1:
    case 0xF1:
        *z80_bc_de_hl_af[reg.byte_value >> 4 & 0x03] = z80_pop16();
        return 10;
    case 0xC2: // JP CC,nn
    case 0xCA:
    case 0xD2:
    case 0xDA:
    case 0xE2:
    case 0xEA:
    case 0xF2:
    case 0xFA:
        return z80_jump_with_condition(z80_decode_condition(reg));
    case 0xC3: // JP nn
        return z80_jump_with_condition(true);
    case 0xC4: // CALL CC,nn
    case 0xCC:
    case 0xD4:
    case 0xDC:
    case 0xE4:
    case 0xEC:
    case 0xF4:
    case 0xFC:
        return z80_call_with_condition(z80_decode_condition(reg));
    case 0xC5: // PUSH qq
    case 0xD5:
    case 0xE5:
    case 0xF5:
        z80_push16(*z80_bc_de_hl_af[reg.byte_value >> 4 & 0x03]);
        return 11;
    case 0xC6: // ADD A,n
        register_add8_with_flags(&z80_reg_af.bytes.high, z80_next8(), MASK_ALL);
        return 7;
    case 0xC7: // RST p
    case 0xD7:
    case 0xCF:
    case 0xDF:
    case 0xE7:
    case 0xEF:
    case 0xF7:
    case 0xFF:
        z80_push16(z80_reg_pc);
        z80_reg_pc.value = z80_rst_addr[reg.byte_value >> 3 & 0x07];
        return 11;
    case 0xC9: // RET
        z80_ret_with_condition(true);
        return 10;
    case 0xCB: // CB
        reg = z80_fetch_opcode();
        alt = z80_decode_reg8(reg, 0, &hl);
        switch (reg.byte_value)
        {
        case 0x00: // RLC r
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            register_left8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX7));
            return hl ? 15 : 8;
        case 0x08: // RRC r
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            register_right8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX0));
            return hl ? 15 : 8;
        case 0x10: // RL r
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            register_left8_with_flags(alt, MASK_ALL, register_is_flag(FLAG_C));
            return hl ? 15 : 8;
        case 0x18: // RR r
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x1F:
            register_right8_with_flags(alt, MASK_ALL, register_is_flag(FLAG_C));
            return hl ? 15 : 8;
        case 0x20: // SLA r
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
            register_left8_with_flags(alt, MASK_ALL, false);
            return hl ? 15 : 8;
        case 0x28: // SRA r
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
            register_right8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX7));
            return hl ? 15 : 8;
        case 0x30: // SLL r
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
            register_left8_with_flags(alt, MASK_ALL, true);
            return hl ? 15 : 8;
        case 0x38: // SRL r
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            register_right8_with_flags(alt, MASK_ALL, false);
            return hl ? 15 : 8;
        case 0x40: // BIT b,r
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
        case 0x76:
        case 0x77:
        case 0x78:
        case 0x79:
        case 0x7A:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
            register_set_or_unset_flag(FLAG_Z, !register_is_bit(*alt, 0x01 << (reg.byte_value >> 3 & 0x07)));
            register_set_or_unset_flag(FLAG_HC, true);
            register_set_or_unset_flag(FLAG_N, false);
            return hl ? 12 : 8;
        case 0x80: // RES b,r
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
        case 0x90:
        case 0x91:
        case 0x92:
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x96:
        case 0x97:
        case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9B:
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F:
        case 0xA0:
        case 0xA1:
        case 0xA2:
        case 0xA3:
        case 0xA4:
        case 0xA5:
        case 0xA6:
        case 0xA7:
        case 0xA8:
        case 0xA9:
        case 0xAA:
        case 0xAB:
        case 0xAC:
        case 0xAD:
        case 0xAE:
        case 0xAF:
        case 0xB0:
        case 0xB1:
        case 0xB2:
        case 0xB3:
        case 0xB4:
        case 0xB5:
        case 0xB6:
        case 0xB7:
        case 0xB8:
        case 0xB9:
        case 0xBA:
        case 0xBB:
        case 0xBC:
        case 0xBD:
        case 0xBE:
        case 0xBF:
            register_set_or_unset_bit(*alt, 0x01 << (reg.byte_value >> 3 & 0x07), false);
            return hl ? 15 : 8;
        case 0xC0: // SET b,r
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC4:
        case 0xC5:
        case 0xC6:
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA:
        case 0xCB:
        case 0xCC:
        case 0xCD:
        case 0xCE:
        case 0xCF:
        case 0xD0:
        case 0xD1:
        case 0xD2:
        case 0xD3:
        case 0xD4:
        case 0xD5:
        case 0xD6:
        case 0xD7:
        case 0xD8:
        case 0xD9:
        case 0xDA:
        case 0xDB:
        case 0xDC:
        case 0xDD:
        case 0xDE:
        case 0xDF:
        case 0xE0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
        case 0xF0:
        case 0xF1:
        case 0xF2:
        case 0xF3:
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
        case 0xF8:
        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFD:
        case 0xFE:
        case 0xFF:
            register_set_or_unset_bit(*alt, 0x01 << (reg.byte_value >> 3 & 0x07), true);
            return hl ? 15 : 8;
        default:
            return 0; // fail
        }
    case 0xCD: // CALL nn
        return z80_call_with_condition(true);
    case 0xCE: // ADC A,n
        c = register_is_flag(FLAG_C);
        register_add8_with_flags(&z80_reg_af.bytes.high, z80_next8(), mask);
        if (c)
        {
            mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
            register_add8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
        }
        return 7;
    case 0xD3: // OUT (n),A
        port_write8((REG16){.bytes.high = z80_reg_af.bytes.high, .bytes.low = z80_next8()}, z80_reg_af.bytes.high);
        return 11;
    case 0xD6: // SUB n
        register_sub8_with_flags(&z80_reg_af.bytes.high, z80_next8(), MASK_ALL);
        return 7;
    case 0xD9: // EXX
        register_exchange16(&z80_reg_bc, &z80_reg_bc_2);
        register_exchange16(&z80_reg_de, &z80_reg_de_2);
        register_exchange16(&z80_reg_hl, &z80_reg_hl_2);
        return 4;
    case 0xDB: // IN A,(n)
        z80_reg_af.bytes.high = port_read8((REG16){.bytes.high = z80_reg_af.bytes.high, .bytes.low = z80_next8()});
        return 11;
    case 0xDD: // DD
        other = &z80_reg_ix;
    case 0xFD: // FD
        z80_bc_de___sp[2] = other;
        reg = z80_fetch_opcode();
        switch (reg.byte_value)
        {
        case 0x09: // ADD IX,pp
        case 0x19:
        case 0x29:
        case 0x39:
            register_add16_with_flags(other, *z80_bc_de___sp[reg.byte_value >> 4 & 0x03], MASK_HNC);
            return 15;
        case 0x21: // LD IX,nn
            *other = z80_next16();
            return 14;
        case 0x22: // LD (nn),IX
            memory_write16(z80_next16(), *other);
            return 20;
        case 0x23: // INC IX
            register_add16_with_flags(other, REG16_ONE, MASK_NONE);
            return 10;
        case 0x2A: // LD IX,(nn)
            *other = memory_read16(z80_next16());
            return 20;
        case 0x2B: // DEC IX
            register_sub16_with_flags(other, REG16_ONE, MASK_NONE);
            return 10;
        case 0x34: // INC (IX+d)
            register_add8_with_flags(memory_ref8_indexed(*other, z80_next8()), REG8_ONE, MASK_SZHVN);
            return 23;
        case 0x35: // DEC (IX+d)
            register_sub8_with_flags(memory_ref8_indexed(*other, z80_next8()), REG8_ONE, MASK_SZHVN);
            return 23;
        case 0x36: // LD (IX+d),n
            duplicate_a = z80_next8();
            memory_write8_indexed(*other, duplicate_a, z80_next8());
            return 19;
        case 0x46: // LD r,(IX+d)
        case 0x4E:
        case 0x56:
        case 0x5E:
        case 0x66:
        case 0x6E:
        case 0x7E:
            *z80_decode_reg8(reg, 3, &hl) = memory_read8_indexed(*other, z80_next8());
            return 19;
        case 0x70: // LD (IX+d),r
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x77:
            alt = z80_decode_reg8(reg, 0, &hl);
            memory_write8_indexed(z80_reg_iy, z80_next8(), *alt);
            return 19;
        case 0x86: // ADD A,(IX+d)
            register_add8_with_flags(&z80_reg_af.bytes.high, memory_read8_indexed(*other, z80_next8()), MASK_ALL);
            return 19;
        case 0x8E: // ADC A,(IX+d)
            c = register_is_flag(FLAG_C);
            register_add8_with_flags(&z80_reg_af.bytes.high, memory_read8_indexed(*other, z80_next8()), mask);
            if (c)
            {
                mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
                register_add8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
            }
            return 19;
        case 0x96: // SUB (IX+d)
            register_sub8_with_flags(&z80_reg_af.bytes.high, memory_read8_indexed(*other, z80_next8()), MASK_ALL);
            return 19;
        case 0x9E: // SBC (IX+d)
            c = register_is_flag(FLAG_C);
            register_sub8_with_flags(&z80_reg_af.bytes.high, memory_read8_indexed(*other, z80_next8()), mask);
            if (c)
            {
                mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
                register_sub8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
            }
            return 19;
        case 0xA6: // AND (IX+d)
            z80_reg_af.bytes.high.byte_value &= memory_read8_indexed(*other, z80_next8()).byte_value;
            register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
            register_set_or_unset_flag(FLAG_N | FLAG_C, false);
            register_set_or_unset_flag(FLAG_HC, true);
            return 19;
        case 0xAE: // XOR (IX+d)
            z80_reg_af.bytes.high.byte_value ^= memory_read8_indexed(*other, z80_next8()).byte_value;
            register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
            register_set_or_unset_flag(FLAG_HC | FLAG_N | FLAG_C, false);
            return 19;
        case 0xB6: // OR (IX+d)
            z80_reg_af.bytes.high.byte_value |= memory_read8_indexed(*other, z80_next8()).byte_value;
            register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
            register_set_or_unset_flag(FLAG_HC | FLAG_N | FLAG_C, false);
            return 19;
        case 0xBE: // CP (IX+d)
            register_sub8_with_flags(&duplicate_a, memory_read8_indexed(*other, z80_next8()), MASK_ALL);
            return 19;
        case 0xCB: // DDCB
            alt = memory_ref8_indexed(*other, z80_next8());
            reg = z80_next8();
            switch (reg.byte_value)
            {
            case 0x06: // RLC (IX+d)
                register_left8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX7));
                return 23;
            case 0x0E: // RRC (IX+d)
                register_right8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX0));
                return 23;
            case 0x16: // RL (IX+d)
                register_left8_with_flags(alt, MASK_ALL, register_is_flag(FLAG_C));
                return 23;
            case 0x1E: // RR (IX+d)
                register_right8_with_flags(alt, MASK_ALL, register_is_flag(FLAG_C));
                return 23;
            case 0x26: // SLA (IX+d)
                register_left8_with_flags(alt, MASK_ALL, false);
                return 23;
            case 0x2E: // SRA (IX+d)
                register_right8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX7));
                return 23;
            case 0x36: // SLL (IX+d)
                register_left8_with_flags(alt, MASK_ALL, true);
                return 23;
            case 0x3E: // SRL (IX+d)
                register_right8_with_flags(alt, MASK_ALL, false);
                return 23;
            case 0x46: // BIT b,(IX+d)
            case 0x4E:
            case 0x56:
            case 0x5E:
            case 0x66:
            case 0x6E:
            case 0x76:
            case 0x7E:
                register_set_or_unset_flag(FLAG_Z, !register_is_bit(*alt, 0x01 << (reg.byte_value >> 3 & 0x07)));
                register_set_or_unset_flag(FLAG_HC, true);
                register_set_or_unset_flag(FLAG_N, false);
                return 20;
            case 0x86: // RES b,(IX+d)
            case 0x8E:
            case 0x96:
            case 0x9E:
            case 0xA6:
            case 0xAE:
            case 0xB6:
            case 0xBE:
                register_set_or_unset_bit(*alt, 0x01 << (reg.byte_value >> 3 & 0x07), false);
                return 23;
            case 0xC6: // SET b,(IX+d)
            case 0xCE:
            case 0xD6:
            case 0xDE:
            case 0xE6:
            case 0xEE:
            case 0xF6:
            case 0xFE:
                register_set_or_unset_bit(*alt, 0x01 << (reg.byte_value >> 3 & 0x07), true);
                return 23;
            default:
                return 0; // fail
            }
        case 0xE1: // POP IX
            *other = z80_pop16();
            return 14;
        case 0xE3: // EX (SP),IX
            register_exchange16(other, memory_ref16(z80_reg_sp));
            return 23;
        case 0xE5: // PUSH IX
            z80_push16(*other);
            return 15;
        case 0xE9: // JP (IX)
            z80_reg_pc = *other;
            return 8;
        case 0xF9: // LD SP,IX
            z80_reg_sp = *other;
            return 10;
        default:
            return 0; // fail
        }
    case 0xDE: // SBC A,n
        c = register_is_flag(FLAG_C);
        register_sub8_with_flags(&z80_reg_af.bytes.high, z80_next8(), mask);
        if (c)
        {
            mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
            register_sub8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
        }
        return 7;
    case 0xE3: // EX (SP),HL
        register_exchange16(memory_ref16(z80_reg_sp), &z80_reg_hl);
        return 19;
    case 0xE6: // AND n
        z80_reg_af.bytes.high.byte_value &= z80_next8().byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_N | FLAG_C, false);
        register_set_or_unset_flag(FLAG_HC, true);
        return 7;
    case 0xE9: // JP HL
        z80_reg_pc = z80_reg_hl;
        return 4;
    case 0xEB: // EX DE,HL
        register_exchange16(&z80_reg_de, &z80_reg_hl);
        return 4;
    case 0xED: // ED
        reg = z80_fetch_opcode();
        switch (reg.byte_value)
        {
        case 0x40: // IN r,(C)
        case 0x48:
        case 0x50:
        case 0x58:
        case 0x60:
        case 0x68:
        case 0x78:
            alt = z80_decode_reg8(reg, 3, &hl);
            *alt = port_read8(z80_reg_bc);
            register_set_flag_s_z_p(*alt, MASK_ALL);
            register_set_or_unset_flag(FLAG_HC | FLAG_N, false);
            return 12;
        case 0x41: // OUT (C),r
        case 0x49:
        case 0x51:
        case 0x59:
        case 0x61:
        case 0x69:
        case 0x79:
            port_write8(z80_reg_bc, *z80_decode_reg8(reg, 3, &hl));
            return 12;
        case 0x42: // SBC HL,ss
        case 0x52:
        case 0x62:
        case 0x72:
            c = register_is_flag(FLAG_C);
            register_sub16_with_flags(&z80_reg_hl, *z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03], mask);
            if (c)
            {
                mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
                register_sub16_with_flags(&z80_reg_hl, REG16_ONE, mask);
            }
            return 15;
        case 0x43: // LD (nn),dd
        case 0x53:
        case 0x63:
        case 0x73:
            memory_write16(z80_next16(), *z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03]);
            return 20;
        case 0x44: // NEG
            z80_reg_af.bytes.high.value = 0;
            register_sub8_with_flags(&z80_reg_af.bytes.high, duplicate_a, MASK_ALL);
            return 8;
        case 0x45: // RETN
            z80_reg_pc = z80_pop16();
            z80_iff1 = z80_iff2;
            return 14;
        case 0x46: // IM 0
            z80_imode = 0;
            return 8;
        case 0x47: // LD I,A
            z80_reg_i = z80_reg_af.bytes.high;
            return 9;
        case 0x4A: // ADC HL,ss
        case 0x5A:
        case 0x6A:
        case 0x7A:
            c = register_is_flag(FLAG_C);
            register_add16_with_flags(&z80_reg_hl, *z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03], mask);
            if (c)
            {
                mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
                register_add16_with_flags(&z80_reg_hl, REG16_ONE, mask);
            }
            return 15;
        case 0x4B: // LD dd,(nn)
        case 0x5B:
        case 0x6B:
        case 0x7B:
            *z80_bc_de_hl_sp[reg.byte_value >> 4 & 0x03] = memory_read16(z80_next16());
            return 20;
        case 0x4D: // RETI
            z80_reg_pc = z80_pop16();
            return 14;
        case 0x4F: // LD R,A
            z80_reg_r = z80_reg_af.bytes.high;
            return 9;
        case 0x56: // IM 1
            z80_imode = 1;
            return 8;
        case 0x57: // LD A,I
            z80_reg_af.bytes.high = z80_reg_i;
            register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
            register_set_or_unset_flag(FLAG_PV, z80_iff2);
            register_set_or_unset_flag(FLAG_HC | FLAG_N, false);
            return 9;
        case 0x5E: // IM 2
            z80_imode = 2;
            return 8;
        case 0x5F: // LD A,R
            z80_reg_af.bytes.high = z80_reg_r;
            register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
            register_set_or_unset_flag(FLAG_PV, z80_iff2);
            register_set_or_unset_flag(FLAG_HC | FLAG_N, false);
            return 9;
        case 0x67: // RRD
            alt = memory_ref8(z80_reg_hl);
            qr = register_split_8_to_4(*alt);
            qr_alt = register_split_8_to_4(z80_reg_af.bytes.high);
            register_set_8_from_4(&z80_reg_af.bytes.high, (div_t){.quot = qr_alt.quot, .rem = qr.rem});
            register_set_8_from_4(alt, (div_t){.quot = qr_alt.rem, .rem = qr.quot});
            register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
            register_set_or_unset_flag(FLAG_HC | FLAG_N, false);
            return 18;
        case 0x6F: // RLD
            alt = memory_ref8(z80_reg_hl);
            qr = register_split_8_to_4(*alt);
            qr_alt = register_split_8_to_4(z80_reg_af.bytes.high);
            register_set_8_from_4(&z80_reg_af.bytes.high, (div_t){.quot = qr_alt.quot, .rem = qr.quot});
            register_set_8_from_4(alt, (div_t){.quot = qr.rem, .rem = qr_alt.rem});
            register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
            register_set_or_unset_flag(FLAG_HC | FLAG_N, false);
            return 18;
        case 0xA0: // LDI & LDIR
        case 0xB0:
            memory_write8(z80_reg_de, memory_read8(z80_reg_hl));
            z80_reg_de.value++;
            z80_reg_hl.value++;
            z80_reg_bc.value--;
            register_set_or_unset_flag(FLAG_PV, z80_reg_bc.value != 0);
            register_set_or_unset_flag(FLAG_HC | FLAG_N, false);
            if (reg.byte_value == 0xB0 && register_is_flag(FLAG_PV))
            {
                z80_reg_pc.value -= 2;
                return 21;
            }
            else
            {
                return 16;
            }
        case 0xA1: // CPI & CPIR
        case 0xB1:
            register_sub8_with_flags(&duplicate_a, memory_read8(z80_reg_hl), MASK_ALL);
            z80_reg_hl.value++;
            z80_reg_bc.value--;
            register_set_or_unset_flag(FLAG_PV, z80_reg_bc.value != 0);
            if (reg.byte_value == 0xB1 && register_is_flag(FLAG_PV) && !register_is_flag(FLAG_Z))
            {
                z80_reg_pc.value -= 2;
                return 21;
            }
            else
            {
                return 16;
            }
        case 0xA2: // INI & INIR
        case 0xB2:
            memory_write8(z80_reg_hl, port_read8(z80_reg_bc));
            z80_reg_bc.bytes.high.value--;
            z80_reg_hl.value++;
            register_set_or_unset_flag(FLAG_Z, register_is_zero(z80_reg_bc.bytes.high));
            register_set_or_unset_flag(FLAG_N, true);
            if (reg.byte_value == 0xB2 && !register_is_flag(FLAG_Z))
            {
                z80_reg_pc.value -= 2;
                return 21;
            }
            else
            {
                return 16;
            }
        case 0xA3: // OUTI & OTIR
        case 0xB3:
            z80_reg_bc.bytes.high.value--;
            port_write8(z80_reg_bc, memory_read8(z80_reg_hl));
            z80_reg_hl.value++;
            register_set_or_unset_flag(FLAG_Z, register_is_zero(z80_reg_bc.bytes.high));
            register_set_or_unset_flag(FLAG_N, true);
            if (reg.byte_value == 0xB3 && !register_is_flag(FLAG_Z))
            {
                z80_reg_pc.value -= 2;
                return 21;
            }
            else
            {
                return 16;
            }
        case 0xA8: // LDD & LDDR
        case 0xB8:
            memory_write8(z80_reg_de, memory_read8(z80_reg_hl));
            z80_reg_de.value--;
            z80_reg_hl.value--;
            z80_reg_bc.value--;
            register_set_or_unset_flag(FLAG_PV, z80_reg_bc.value != 0);
            register_set_or_unset_flag(FLAG_HC | FLAG_N, false);
            if (reg.byte_value == 0xB8 && register_is_flag(FLAG_PV))
            {
                z80_reg_pc.value -= 2;
                return 21;
            }
            else
            {
                return 16;
            }
        case 0xA9: // CPD & CPDR
        case 0xB9:
            register_sub8_with_flags(&duplicate_a, memory_read8(z80_reg_hl), MASK_ALL);
            z80_reg_hl.value--;
            z80_reg_bc.value--;
            register_set_or_unset_flag(FLAG_PV, z80_reg_bc.value != 0);
            if (reg.byte_value == 0xB9 && register_is_flag(FLAG_PV) && !register_is_flag(FLAG_Z))
            {
                z80_reg_pc.value -= 2;
                return 21;
            }
            else
            {
                return 16;
            }
        case 0xAA: // IND & INDR
        case 0xBA:
            memory_write8(z80_reg_hl, port_read8(z80_reg_bc));
            z80_reg_bc.bytes.high.value--;
            z80_reg_hl.value--;
            register_set_or_unset_flag(FLAG_Z, register_is_zero(z80_reg_bc.bytes.high));
            register_set_or_unset_flag(FLAG_N, true);
            if (reg.byte_value == 0xBA && !register_is_flag(FLAG_Z))
            {
                z80_reg_pc.value -= 2;
                return 21;
            }
            else
            {
                return 16;
            }
        case 0xAB: // OUTD & OTDR
        case 0xBB:
            z80_reg_bc.bytes.high.value--;
            port_write8(z80_reg_bc, memory_read8(z80_reg_hl));
            z80_reg_hl.value--;
            register_set_or_unset_flag(FLAG_Z, register_is_zero(z80_reg_bc.bytes.high));
            register_set_or_unset_flag(FLAG_N, true);
            if (reg.byte_value == 0xBB && !register_is_flag(FLAG_Z))
            {
                z80_reg_pc.value -= 2;
                return 21;
            }
            else
            {
                return 16;
            }
        default:
            return 0; // fail
        }
    case 0xEE: // XOR n
        z80_reg_af.bytes.high.byte_value ^= z80_next8().byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_C | FLAG_N | FLAG_HC, false);
        return 7;
    case 0xF3: // DI
        z80_iff1 = z80_iff2 = false;
        return 4;
    case 0xF6: // OR n
        z80_reg_af.bytes.high.byte_value |= z80_next8().byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_C | FLAG_N | FLAG_HC, false);
        return 7;
    case 0xF9: // LD SP,HL
        z80_reg_sp = z80_reg_hl;
        return 6;
    case 0xFB: // EI
        z80_iff1 = z80_iff2 = true;
        return 4;
    case 0xFE: // CP n
        register_sub8_with_flags(&duplicate_a, z80_next8(), MASK_ALL);
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
        z80_halt = false;
        z80_nonmaskable_interrupt_flag = false;
        return z80_nonmaskable_interrupt();
    }
    else if (z80_maskable_interrupt_flag)
    {
        z80_halt = false;
        z80_maskable_interrupt_flag = false;
        if (z80_iff1)
        {
            return z80_maskable_interrupt();
        }
    }
    if (z80_can_execute)
    {
        // if (z80_reg_pc.byte_value == 0xea60) {
        //     debug = 0;
        // }
        // if (debug < 10) {
        //     z80_print();
        //     debug++;
        // }
        return z80_execute(z80_fetch_opcode());
    }
    else
    {
        return 0;
    }
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
    RGB color = (b ? ula_bright_colors[c] : ula_colors[c]);
    ula_screen[y][x] = color;
}

int ula_draw_line(int y)
{
    int i, j, x = 0;
    if (y > 55 && y < 248)
    {
        y -= 56;
        for (j = 0; j < 48; j++)
        {
            ula_point(x + j, y + 56, ula_border_color, false);
        }
        x = 48;
        ula_addr_attrib.byte_value = 0x5800 + y / 8 * 32;
        for (i = 0; i < 32; i++)
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
                ula_point(x + j, y + 56, register_is_bit(reg_bitmap, b) ? ink : paper, brightness);
                b >>= 1;
            }
            ula_addr_bitmap.byte_value++;
            ula_addr_attrib.byte_value++;
            x += 8;
        }
        for (j = 0; j < 48; j++)
        {
            ula_point(x + j, y + 56, ula_border_color, false);
        }
        y++;
        register_set_or_unset_bit(ula_addr_bitmap, MAX5, is_bit(y, MAX3));
        register_set_or_unset_bit(ula_addr_bitmap, MAX6, is_bit(y, MAX4));
        register_set_or_unset_bit(ula_addr_bitmap, MAX7, is_bit(y, MAX5));
        register_set_or_unset_bit(ula_addr_bitmap, MAX8, is_bit(y, MAX0));
        register_set_or_unset_bit(ula_addr_bitmap, MAX9, is_bit(y, MAX1));
        register_set_or_unset_bit(ula_addr_bitmap, MAX10, is_bit(y, MAX2));
        register_set_or_unset_bit(ula_addr_bitmap, MAX11, is_bit(y, MAX6));
        register_set_or_unset_bit(ula_addr_bitmap, MAX12, is_bit(y, MAX7));
    }
    else
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            ula_point(j, y, ula_border_color, false);
        }
    }
    return 224;
}

void ula_draw_screen_once()
{
    z80_maskable_interrupt_flag = true;
    ula_addr_bitmap.byte_value = 0x4000;
    for (int i = 0; i < SCREEN_HEIGHT; i++)
    {
        time_sync(&ula_t_states_all, ula_draw_line(i));
    }
    ula_draw_counter = (ula_draw_counter + 1) % 16;
    time_sync(&ula_t_states_all, 8 * 224); // vertical retrace
}

void *ula_run(void *args)
{
    while (running)
    {
        ula_draw_screen_once();
    }
    return NULL;
}

void draw_screen()
{
    glBegin(GL_POINTS);
    for (int y = 0; y < SCREEN_HEIGHT; y++)
    {
        for (int x = 0; x < SCREEN_WIDTH; x++)
        {
            RGB color = ula_screen[y][x];
            glColor3f(color.red, color.green, color.blue);
            glVertex2i(x, y);
        }
    }
    glEnd();
    glFlush();
    glutPostRedisplay();
}

void tape_play_pulse(int t_states)
{
    sound_ear = true;
    time_sleep_in_seconds(t_states * state_duration);
    sound_ear = false;
    time_sleep_in_seconds(t_states * state_duration);
}

void tape_play_block()
{
    int i, j, b, v;
    v = (tape_block[0].byte_value < 0x80 ? 8063 : 3223);
    for (i = 0; i < v; i++)
    {
        tape_play_pulse(2168);
    }
    tape_play_pulse(667);
    tape_play_pulse(735);
    for (i = 0; i < tape_block_size; i++)
    {
        b = MAX7;
        for (j = 0; j < 8; j++)
        {
            v = (register_is_bit(tape_block[i], b) ? 1710 : 855);
            tape_play_pulse(v);
            tape_play_pulse(v);
            b >>= 1;
        }
    }
    time_sleep_in_seconds(tape_pause / 1000.0);
}

void tape_read_block_10(FILE *f)
{
    fread(&tape_pause, 1, 2, f);
    fread(&tape_block_size, 1, 2, f);
    fread(tape_block, 1, tape_block_size, f);
    tape_play_block();
}

void tape_read_block_15(FILE *f)
{
    int states_per_sample, used;
    fread(&states_per_sample, 1, 2, f);
    fread(&tape_pause, 1, 2, f);
    fread(&used, 1, 1, f);
    fread(&tape_block_size, 1, 3, f);
    fread(tape_block, 1, ceil(tape_block_size / 8.0), f);
    // tape_play_block();
}

void tape_load_tzx(FILE *f)
{
    char id;
    int n = fread(tape_block, 1, 11, f);
    if (n == 11 && strncmp("ZXTape!\x1A", (const char *)tape_block, 8) == 0 && tape_block[9].byte_value <= 20)
    {
        id = tape_block[10].byte_value;
        while (n != 0)
        {
            switch (id)
            {
            case 0x10:
                tape_read_block_10(f);
                break;
            case 0x15:
                tape_read_block_15(f);
                break;
            }
            n = fread(&id, 1, 1, f);
        }
    }
}

void *tape_run(void *args)
{
    while (true)
    {
        FILE *f = fopen("tape", "rb");
        if (f != NULL)
        {
            tape_load_tzx(f);
            fclose(f);
        }
    }
}

void rt_advance_head()
{
    if (rt_timeline_head != NULL)
    {
        TASK *p = rt_timeline_head;
        rt_timeline_head = p->next;
        free(p);
    }
}

bool rt_add_pending_task(TASK *task)
{
    if (rt_pending != NULL)
    {
        rt_pending = task;
        return true;
    }
    else
    {
        return false;
    }
}

void rt_add_task(TASK *task)
{
    TASK *c = rt_timeline_head;
    TASK *p = NULL;
    while (c != NULL && c->t_states < task->t_states)
    {
        p = c;
        c = c->next;
    }
    if (p == NULL)
    {
        rt_timeline_head = task;
    }
    else
    {
        p->next = task;
    }
    task->next = c;
}

void rt_run()
{
    while (running)
    {
        unsigned long t_states = (rt_timeline_head == NULL ? 1 : rt_timeline_head->t_states);
        time_sync(&z80_t_states_all, t_states - z80_t_states_all);
        if (rt_pending != NULL)
        {
            rt_add_task(rt_pending);
            rt_pending = NULL;
        }
        if (rt_timeline_head != NULL)
        {
            rt_timeline_head->task();
        }
        rt_advance_head();
    }
}

int main(int argc, char **argv)
{
    pthread_t z80_id, ula_id, tape_id;
    if (system_little_endian())
    {
        // sound_console_fd = open("/dev/console", O_WRONLY);
        // atexit(z80_reset);
        glutInit(&argc, argv);
        glutInitDisplayMode(GLUT_SINGLE);
        glutInitWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);

        // glutInitWindowPosition(200, 100);
        glutCreateWindow("Cristian Mocanu Z80");
        // glPointSize(1.0f);
        glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // glMatrixMode(GL_PROJECTION);
        // glLoadIdentity();
        // glMatrixMode(GL_MODELVIEW);
        // glLoadIdentity();
        glTranslatef(-1.0, 1.0, 0.0);
        glScalef(2.0f / SCREEN_WIDTH, -2.0f / SCREEN_HEIGHT, 0.0f);
        glutKeyboardFunc(keyboard_press_down);
        glutKeyboardUpFunc(keyboard_press_up);
        if (argc == 2)
        {
            memory_load_rom(argv[1]);
        }
        z80_reset();
        pthread_create(&z80_id, NULL, z80_run, NULL);
        pthread_create(&ula_id, NULL, ula_run, NULL);
        pthread_create(&tape_id, NULL, tape_run, NULL);
        glutDisplayFunc(draw_screen);
        glutMainLoop();
    }
    running = false;
    return 0;
}

// TODO: ula in the same thread advance beam after execute
// https://worldofspectrum.org/faq/reference/48kreference.htm
// TODO: tape
// TODO: uart
// TODO: debugger
// TODO: use pixel shader with drawArrays
// https://stackoverflow.com/questions/19102180/how-does-gldrawarrays-know-what-to-draw
// TODO: replace glut with x calls to create window and read keyboard
// https://linux.die.net/man/3/xbell
// TODO: sound
// /etc/modprobe.d/pc-speaker.conf
// https://www.alsa-project.org/alsa-doc/alsa-lib/examples.html
// br $029d
// first interrupt 0e5b / 0e5c
// https://jnz.dk/z80/um0080.pdf
