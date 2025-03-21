// gcc main.c -Ofast -lGLEW -lGLU -lGL -lglut -pthread -lm -lasound -Wall
// SHIFT = SS; ALT = CS; ESC = SS + CS
// mkfifo load
// mkfifo save
// cat file.tzx > load
// cat save > file.tzx
// ./a.out file.rom [-o]
// ./a.out file.sna
// ./a.out file.tzx [-p0]

#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <GL/freeglut.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include <sys/resource.h>

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
#define SCREEN_ZOOM 2

#define TAPE_LOAD_EVENT 1
#define TAPE_SAVE_EVENT 2

#define RT_MAX 5

#define PCM_SAMPLE 48000
#define Z80_FREQ 3500000.0L

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

typedef struct TTASK
{
    unsigned long long t_states;
    void (*task)();
    struct TTASK *next;
} TASK;

typedef struct TREG8BLOCK
{
    unsigned char last_used, pulses_per_sample;
    int size, sync_size;
    unsigned short pilot_pulse, pilot_tone;
    unsigned short pause, zero_pulse, one_pulse;
    unsigned short *sync_pulse;
    REG8 *data;
    struct TREG8BLOCK *next;
} REG8BLOCK;

REG8 memory[MAX16];
RGB ula_screen[SCREEN_HEIGHT][SCREEN_WIDTH];
RGB ula_border[SCREEN_HEIGHT];
REG8 keyboard[] = {(REG8){.value = 0xFF}, (REG8){.value = 0xFF}, (REG8){.value = 0xFF},
                   (REG8){.value = 0xFF}, (REG8){.value = 0xFF}, (REG8){.value = 0xFF},
                   (REG8){.value = 0xFF}, (REG8){.value = 0xFF}};
RGB ula_colors[] = {(RGB){0.0f, 0.0f, 0.0f}, (RGB){0.0f, 0.0f, 0.9f},
                    (RGB){0.5f, 0.0f, 0.0f}, (RGB){0.4f, 0.0f, 0.4f},
                    (RGB){0.0f, 0.9f, 0.0f}, (RGB){0.0f, 0.4f, 0.4f},
                    (RGB){0.9f, 0.9f, 0.0f}, (RGB){0.9f, 0.9f, 0.9f}};
RGB ula_bright_colors[] = {(RGB){0.0f, 0.0f, 0.0f}, (RGB){0.0f, 0.0f, 1.0f},
                           (RGB){1.0f, 0.0f, 0.0f}, (RGB){0.5f, 0.0f, 0.5f},
                           (RGB){0.0f, 1.0f, 0.0f}, (RGB){0.0f, 1.0f, 1.0f},
                           (RGB){1.0f, 1.0f, 0.0f}, (RGB){1.0f, 1.0f, 1.0f}};
const unsigned int memory_size = MAX16;
long double time_start = 0.0L, state_duration = 1.0L / Z80_FREQ;
unsigned long long z80_t_states_all = 0;
unsigned int ula_draw_counter = 0, ula_line = 0, ula_state;
int ula_border_color;
bool sound_ear = false, sound_mic = false, sound_input = false;
REG16 ula_addr_bitmap, ula_addr_attrib;
REG16 z80_reg_bc, z80_reg_de, z80_reg_hl, z80_reg_af, z80_reg_pc, z80_reg_sp, z80_reg_ix, z80_reg_iy;
REG16 z80_reg_bc_2, z80_reg_de_2, z80_reg_hl_2, z80_reg_af_2;
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
bool z80_iff1, z80_iff2, z80_can_execute, z80_halt;
int z80_imode;
unsigned long long tape_save_t_states, tape_save_duration, tape_index, tape_break_index;
int tape_load_state, tape_save_state, tape_save_counter;
int tape_save_index, tape_save_buffer_size;
char *tape_save_buffer = NULL;
bool tape_save_mic;
REG8BLOCK *tape_block_head = NULL, *tape_block_last = NULL;
TASK rt_timeline[RT_MAX], rt_pending;
bool rt_is_pending = false;
int rt_size = 0;
snd_pcm_t *pcm_handle;
int pcm_states = Z80_FREQ / PCM_SAMPLE;
// int debug = 100;

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

// ===TIME===============================================

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

void time_sync()
{
    time_sleep_in_seconds(time_start + z80_t_states_all * state_duration - time_in_seconds());
}

// ===REGISTER===========================================

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

// ===FILE===============================================

bool file_has_extension(const char *filename, const char *ext)
{
    int i = strlen(filename);
    int j = strlen(ext);
    return (i > j && strcmp(filename + i - j, ext) == 0);
}

void file_load_rom(const char *filename)
{
    FILE *f = fopen(filename, "r");
    fread(memory, 1, memory_size, f);
    fclose(f);
}

void file_load_sna(const char *filename)
{
    FILE *f = fopen(filename, "r");
    fread(&z80_reg_i, 1, 1, f);
    fread(&z80_reg_hl_2, 2, 1, f);
    fread(&z80_reg_de_2, 2, 1, f);
    fread(&z80_reg_bc_2, 2, 1, f);
    fread(&z80_reg_af_2, 2, 1, f);
    fread(&z80_reg_hl, 2, 1, f);
    fread(&z80_reg_de, 2, 1, f);
    fread(&z80_reg_bc, 2, 1, f);
    fread(&z80_reg_iy, 2, 1, f);
    fread(&z80_reg_ix, 2, 1, f);
    fread(&z80_iff2, 1, 1, f);
    fread(&z80_reg_r, 1, 1, f);
    fread(&z80_reg_af, 2, 1, f);
    fread(&z80_reg_sp, 2, 1, f);
    fread(&z80_imode, 1, 1, f);
    fread(&ula_border_color, 1, 1, f);
    fread(memory + 0x4000, 1, 49152, f);
    fclose(f);
}

void file_save_sna(const char *filename)
{
    FILE *f = fopen(filename, "w");
    fwrite(&z80_reg_i, 1, 1, f);
    fwrite(&z80_reg_hl_2, 2, 1, f);
    fwrite(&z80_reg_de_2, 2, 1, f);
    fwrite(&z80_reg_bc_2, 2, 1, f);
    fwrite(&z80_reg_af_2, 2, 1, f);
    fwrite(&z80_reg_hl, 2, 1, f);
    fwrite(&z80_reg_de, 2, 1, f);
    fwrite(&z80_reg_bc, 2, 1, f);
    fwrite(&z80_reg_iy, 2, 1, f);
    fwrite(&z80_reg_ix, 2, 1, f);
    fwrite(&z80_iff2, 1, 1, f);
    fwrite(&z80_reg_r, 1, 1, f);
    fwrite(&z80_reg_af, 2, 1, f);
    fwrite(&z80_reg_sp, 2, 1, f);
    fwrite(&z80_imode, 1, 1, f);
    fwrite(&ula_border_color, 1, 1, f);
    fwrite(memory + 0x4000, 1, 49152, f);
    fclose(f);
}

// ===MEMORY=============================================

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

// ======================================================

REG8 keyboard_read8(const REG16 reg)
{
    REG8 alt;
    alt.byte_value = 0xFF;
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
    }
    else if (!on && sound_ear)
    {
        sound_ear = false;
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
        if (!sound_input)
        {
            sound_ear_on_off(alt.byte_value & MAX4);
        }
    }
}

// ===Z80================================================

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
    z80_data_bus.byte_value = 0xFF;
    sound_mic = false;
    sound_ear_on_off(false);
}

void z80_memory_refresh()
{
    z80_reg_r.byte_value = ((z80_reg_r.byte_value + 1) & 0x7F) | (z80_reg_r.byte_value & MAX7);
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

int z80_execute_simple(REG8 reg)
{
    REG8 *alt;
    div_t qr;
    int mask = MASK_ALL;
    bool c, hc, hl;
    REG8 duplicate_a = z80_reg_af.bytes.high;
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

int z80_execute_cb(REG8 reg)
{
    bool hl;
    REG8 *alt = z80_decode_reg8(reg, 0, &hl);
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
}

int z80_execute_ed(REG8 reg)
{
    bool c, hl;
    div_t qr, qr_alt;
    int mask = MASK_ALL;
    REG8 *alt = NULL;
    REG8 duplicate_a = z80_reg_af.bytes.high;
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
    case 0x4C:
    case 0x54:
    case 0x5C:
    case 0x64:
    case 0x6C:
    case 0x74:
    case 0x7C:
        z80_reg_af.bytes.high.value = 0;
        register_sub8_with_flags(&z80_reg_af.bytes.high, duplicate_a, MASK_ALL);
        return 8;
    case 0x45: // RETN
    case 0x55:
    case 0x5D:
    case 0x65:
    case 0x6D:
    case 0x75:
    case 0x7D:
        z80_reg_pc = z80_pop16();
        z80_iff1 = z80_iff2;
        return 14;
    case 0x46: // IM 0
    case 0x4E:
    case 0x66:
    case 0x6E:
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
    case 0x76:
        z80_imode = 1;
        return 8;
    case 0x57: // LD A,I
        z80_reg_af.bytes.high = z80_reg_i;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_PV, z80_iff2);
        register_set_or_unset_flag(FLAG_HC | FLAG_N, false);
        return 9;
    case 0x5E: // IM 2
    case 0x7E:
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
    case 0x70: // IN 0,(C)
        register_set_flag_s_z_p(*alt, MASK_ALL);
        register_set_or_unset_flag(FLAG_HC | FLAG_N, false);
        return 12;
    case 0x71: // OUT (C),0
        port_write8(z80_reg_bc, (REG8){.value=0});
        return 12;
    case 0x77: // NOPD
    case 0x7F:
        return 8;
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
    case 0xCB:
    case 0xDD:
    case 0xED:
    case 0xFD:
    default:
        return 0; // fail
    }
}

int z80_execute_dd_fd(REG8 reg, REG16 *other)
{
    bool c, hl;
    int mask = MASK_ALL;
    REG8 *alt;
    REG8 duplicate_a = z80_reg_af.bytes.high;
    REG16 *z80_bc_de___sp[] = {&z80_reg_bc, &z80_reg_de, other, &z80_reg_sp};
    while (reg.byte_value == 0xDD || reg.byte_value == 0xFD)
    {
        reg = z80_fetch_opcode();
    }
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
    case 0x24: // INC IXH
        register_add8_with_flags(&other->bytes.high, REG8_ONE, MASK_SZHVN);
        return 8;
    case 0x25: // DEC IXH
        register_sub8_with_flags(&other->bytes.high, REG8_ONE, MASK_SZHVN);
        return 8;
    case 0x26: // LD IXH,nn
        other->bytes.high = z80_next8();
        return 11;
    case 0x2A: // LD IX,(nn)
        *other = memory_read16(z80_next16());
        return 20;
    case 0x2B: // DEC IX
        register_sub16_with_flags(other, REG16_ONE, MASK_NONE);
        return 10;
    case 0x2C: // INC IXL
        register_add8_with_flags(&other->bytes.low, REG8_ONE, MASK_SZHVN);
        return 8;
    case 0x2D: // DEC IXL
        register_sub8_with_flags(&other->bytes.low, REG8_ONE, MASK_SZHVN);
        return 8;
    case 0x2E: // LD IXL,nn
        other->bytes.low = z80_next8();
        return 11;
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
    case 0x44: // LD r,IXH
    case 0x4C:
    case 0x54:
    case 0x5C:
    case 0x7C:
        *z80_decode_reg8(reg, 3, &hl) = other->bytes.high;
        return 8;
    case 0x45: //LD r,IXL
    case 0x4D:
    case 0x55:
    case 0x5D:
    case 0x7D:
        *z80_decode_reg8(reg, 3, &hl) = other->bytes.low;
        return 8;
    case 0x46: // LD r,(IX+d)
    case 0x4E:
    case 0x56:
    case 0x5E:
    case 0x66:
    case 0x6E:
    case 0x7E:
        *z80_decode_reg8(reg, 3, &hl) = memory_read8_indexed(*other, z80_next8());
        return 19;
    case 0x60: // LD IXH,r
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x67:
        other->bytes.high = *z80_decode_reg8(reg, 0, &hl);
        return 8;
    case 0x64: // LD IXH,IXH
        return 8;
    case 0x65: // LD IXH,IXL
        other->bytes.high = other->bytes.low;
        return 8;
    case 0x68: //LD IXL,r
    case 0x69:
    case 0x6A:
    case 0x6B:
    case 0x6F:
        other->bytes.low = *z80_decode_reg8(reg, 0, &hl);
        return 8;
    case 0x6C: //LD IXL,IXH
        other->bytes.low = other->bytes.high;
        return 8;
    case 0x6D: //LD IXL,IXL
        return 8;
    case 0x70: // LD (IX+d),r
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x77:
        alt = z80_decode_reg8(reg, 0, &hl);
        memory_write8_indexed(*other, z80_next8(), *alt);
        return 19;
    case 0x84: // ADD A,IXH
        register_add8_with_flags(&z80_reg_af.bytes.high, other->bytes.high, MASK_ALL);
        return 8;
    case 0x85: // ADD A,IXL
        register_add8_with_flags(&z80_reg_af.bytes.high, other->bytes.low, MASK_ALL);
        return 8;
    case 0x86: // ADD A,(IX+d)
        register_add8_with_flags(&z80_reg_af.bytes.high, memory_read8_indexed(*other, z80_next8()), MASK_ALL);
        return 19;
    case 0x8C: // ADC A,IXH
        c = register_is_flag(FLAG_C);
        register_add8_with_flags(&z80_reg_af.bytes.high, other->bytes.high, mask);
        if (c)
        {
            mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
            register_add8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
        }
        return 8;
    case 0x8D: // ADC A,IXL
        c = register_is_flag(FLAG_C);
        register_add8_with_flags(&z80_reg_af.bytes.high, other->bytes.low, mask);
        if (c)
        {
            mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
            register_add8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
        }
        return 8;
    case 0x8E: // ADC A,(IX+d)
        c = register_is_flag(FLAG_C);
        register_add8_with_flags(&z80_reg_af.bytes.high, memory_read8_indexed(*other, z80_next8()), mask);
        if (c)
        {
            mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
            register_add8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
        }
        return 19;
    case 0x94: // SUB A,IXH
        register_sub8_with_flags(&z80_reg_af.bytes.high, other->bytes.high, MASK_ALL);
        return 8;
    case 0x95: // SUB A,IXL
        register_sub8_with_flags(&z80_reg_af.bytes.high, other->bytes.low, MASK_ALL);
        return 8;
    case 0x96: // SUB (IX+d)
        register_sub8_with_flags(&z80_reg_af.bytes.high, memory_read8_indexed(*other, z80_next8()), MASK_ALL);
        return 19;
    case 0x9C: // SBC A,IXH
        c = register_is_flag(FLAG_C);
        register_sub8_with_flags(&z80_reg_af.bytes.high, other->bytes.high, mask);
        if (c)
        {
            mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
            register_sub8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
        }
        return 8;
    case 0x9D: // SBC A,IXL
        c = register_is_flag(FLAG_C);
        register_sub8_with_flags(&z80_reg_af.bytes.high, other->bytes.low, mask);
        if (c)
        {
            mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
            register_sub8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
        }
        return 8;
    case 0x9E: // SBC (IX+d)
        c = register_is_flag(FLAG_C);
        register_sub8_with_flags(&z80_reg_af.bytes.high, memory_read8_indexed(*other, z80_next8()), mask);
        if (c)
        {
            mask = MASK_ALL & ~(z80_reg_af.bytes.low.byte_value & MASK_HVNC);
            register_sub8_with_flags(&z80_reg_af.bytes.high, REG8_ONE, mask);
        }
        return 19;
    case 0xA4: // AND IXH
        z80_reg_af.bytes.high.byte_value &= other->bytes.high.byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_N | FLAG_C, false);
        register_set_or_unset_flag(FLAG_HC, true);
        return 8;
    case 0xA5: // AND IXL
        z80_reg_af.bytes.high.byte_value &= other->bytes.low.byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_N | FLAG_C, false);
        register_set_or_unset_flag(FLAG_HC, true);
        return 8;
    case 0xA6: // AND (IX+d)
        z80_reg_af.bytes.high.byte_value &= memory_read8_indexed(*other, z80_next8()).byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_N | FLAG_C, false);
        register_set_or_unset_flag(FLAG_HC, true);
        return 19;
    case 0xAC: // XOR IXH
        z80_reg_af.bytes.high.byte_value ^= other->bytes.high.byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_HC | FLAG_N | FLAG_C, false);
        return 8;
    case 0xAD: // XOR IXL
        z80_reg_af.bytes.high.byte_value ^= other->bytes.low.byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_HC | FLAG_N | FLAG_C, false);
        return 8;
    case 0xAE: // XOR (IX+d)
        z80_reg_af.bytes.high.byte_value ^= memory_read8_indexed(*other, z80_next8()).byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_HC | FLAG_N | FLAG_C, false);
        return 19;
    case 0xB4: // OR IXH
        z80_reg_af.bytes.high.byte_value |= other->bytes.high.byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_HC | FLAG_N | FLAG_C, false);
        return 8;
    case 0xB5: // OR IXL
        z80_reg_af.bytes.high.byte_value |= other->bytes.low.byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_HC | FLAG_N | FLAG_C, false);
        return 8;
    case 0xB6: // OR (IX+d)
        z80_reg_af.bytes.high.byte_value |= memory_read8_indexed(*other, z80_next8()).byte_value;
        register_set_flag_s_z_p(z80_reg_af.bytes.high, MASK_ALL);
        register_set_or_unset_flag(FLAG_HC | FLAG_N | FLAG_C, false);
        return 19;
    case 0xBC: // CP IXH
        register_sub8_with_flags(&duplicate_a, other->bytes.high, MASK_ALL);
        return 8;
    case 0xBD: // CP IXL
        register_sub8_with_flags(&duplicate_a, other->bytes.low, MASK_ALL);
        return 8;
    case 0xBE: // CP (IX+d)
        register_sub8_with_flags(&duplicate_a, memory_read8_indexed(*other, z80_next8()), MASK_ALL);
        return 19;
    case 0xCB: // DDCB
        alt = memory_ref8_indexed(*other, z80_next8());
        reg = z80_next8();
        switch (reg.byte_value)
        {
        case 0x00: // LD r,RLC (IX+d)
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x07:
            register_left8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX7));
            *z80_decode_reg8(reg, 0, &hl) = *alt;
            return 23;
        case 0x06: // RLC (IX+d)
            register_left8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX7));
            return 23;
        case 0x08: // LD r,RRC (IX+d)
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0F:
            register_right8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX0));
            *z80_decode_reg8(reg, 0, &hl) = *alt;
            return 23;
        case 0x0E: // RRC (IX+d)
            register_right8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX0));
            return 23;
        case 0x10: // LD r,RL (IX+d)
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x17:
            register_left8_with_flags(alt, MASK_ALL, register_is_flag(FLAG_C));
            *z80_decode_reg8(reg, 0, &hl) = *alt;
            return 23;
        case 0x16: // RL (IX+d)
            register_left8_with_flags(alt, MASK_ALL, register_is_flag(FLAG_C));
            return 23;
        case 0x18: // LD r,RR (IX+d)
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1F:
            register_right8_with_flags(alt, MASK_ALL, register_is_flag(FLAG_C));
            *z80_decode_reg8(reg, 0, &hl) = *alt;
            return 23;
        case 0x1E: // RR (IX+d)
            register_right8_with_flags(alt, MASK_ALL, register_is_flag(FLAG_C));
            return 23;
        case 0x20: // LD r,SLA (IX+d)
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x27:
            register_left8_with_flags(alt, MASK_ALL, false);
            *z80_decode_reg8(reg, 0, &hl) = *alt;
            return 23;
        case 0x26: // SLA (IX+d)
            register_left8_with_flags(alt, MASK_ALL, false);
            return 23;
        case 0x28: // LD r,SRA (IX+d)
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2F:
            register_right8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX7));
            *z80_decode_reg8(reg, 0, &hl) = *alt;
            return 23;
        case 0x2E: // SRA (IX+d)
            register_right8_with_flags(alt, MASK_ALL, register_is_bit(*alt, MAX7));
            return 23;
        case 0x30: // LD r,SLL (IX+d)
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x37:
            register_left8_with_flags(alt, MASK_ALL, true);
            *z80_decode_reg8(reg, 0, &hl) = *alt;
            return 23;
        case 0x36: // SLL (IX+d)
            register_left8_with_flags(alt, MASK_ALL, true);
            return 23;
        case 0x38: // LD r,SRL (IX+d)
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3F:
            register_right8_with_flags(alt, MASK_ALL, false);
            *z80_decode_reg8(reg, 0, &hl) = *alt;
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
        case 0x80: // LD r,RES b,(IX+d)
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8F:
        case 0x90:
        case 0x91:
        case 0x92:
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x97:
        case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9B:
        case 0x9C:
        case 0x9D:
        case 0x9F:
        case 0xA0:
        case 0xA1:
        case 0xA2:
        case 0xA3:
        case 0xA4:
        case 0xA5:
        case 0xA7:
        case 0xA8:
        case 0xA9:
        case 0xAA:
        case 0xAB:
        case 0xAC:
        case 0xAD:
        case 0xAF:
        case 0xB0:
        case 0xB1:
        case 0xB2:
        case 0xB3:
        case 0xB4:
        case 0xB5:
        case 0xB7:
        case 0xB8:
        case 0xB9:
        case 0xBA:
        case 0xBB:
        case 0xBC:
        case 0xBD:
        case 0xBF:
            register_set_or_unset_bit(*alt, 0x01 << (reg.byte_value >> 3 & 0x07), false);
            *z80_decode_reg8(reg, 0, &hl) = *alt;
            return 23;
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
        case 0xC0: // LD r,SET b,(IX+d)
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC4:
        case 0xC5:
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA:
        case 0xCB:
        case 0xCC:
        case 0xCD:
        case 0xCF:
        case 0xD0:
        case 0xD1:
        case 0xD2:
        case 0xD3:
        case 0xD4:
        case 0xD5:
        case 0xD7:
        case 0xD8:
        case 0xD9:
        case 0xDA:
        case 0xDB:
        case 0xDC:
        case 0xDD:
        case 0xDF:
        case 0xE0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE4:
        case 0xE5:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
        case 0xEC:
        case 0xED:
        case 0xEF:
        case 0xF0:
        case 0xF1:
        case 0xF2:
        case 0xF3:
        case 0xF4:
        case 0xF5:
        case 0xF7:
        case 0xF8:
        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFD:
        case 0xFF:
            register_set_or_unset_bit(*alt, 0x01 << (reg.byte_value >> 3 & 0x07), true);
            *z80_decode_reg8(reg, 0, &hl) = *alt;
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
    case 0xED:
        return 0; // fail
    case 0xF9: // LD SP,IX
        z80_reg_sp = *other;
        return 10;
    default:
        return z80_execute_simple(reg);
    }
}

int z80_execute(REG8 reg)
{
    if (z80_halt)
    {
        reg.byte_value = 0x00;
        z80_reg_pc.byte_value--;
    }
    switch (reg.byte_value)
    {
    case 0xCB: // CB
        return z80_execute_cb(z80_fetch_opcode());
    case 0xDD: // DD
        return z80_execute_dd_fd(z80_fetch_opcode(), &z80_reg_ix);
    case 0xFD: // FD
        return z80_execute_dd_fd(z80_fetch_opcode(), &z80_reg_iy);
    case 0xED: // ED
        return z80_execute_ed(z80_fetch_opcode());
    default:
        return z80_execute_simple(reg);
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
    z80_memory_refresh();
    switch (z80_imode)
    {
    case 0:
        // TODO: wait 2 cycles for interrupting device to write to data_bus
        return z80_execute(z80_data_bus);
    case 1:
        z80_push16(z80_reg_pc);
        z80_reg_pc.byte_value = 0x38;
        return 13;
    case 2:
        z80_memory_refresh();
        z80_memory_refresh();
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
        // if (z80_reg_pc.byte_value == 0x87a3 && debug == 100)
        // {
        //    debug = 0;
        // }
        // if (debug < 100)
        // {
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

// ===RT=================================================

void rt_advance_head()
{
    int i;
    if (rt_size > 0)
    {
        for (i = 1; i < rt_size; i++)
        {
            rt_timeline[i - 1] = rt_timeline[i];
        }
        rt_size--;
    }
}

bool rt_add_pending_task(TASK task)
{
    if (rt_is_pending)
    {
        return false;
    }
    else
    {
        rt_pending = task;
        rt_is_pending = true;
        return true;
    }
}

unsigned long long rt_next_t_states()
{
    if (rt_size == 0)
    {
        return z80_t_states_all + 1;
    }
    else if (rt_size == 1)
    {
        return rt_timeline[0].t_states + 1;
    }
    else
    {
        return rt_timeline[1].t_states;
    }
}

bool rt_add_task(TASK task)
{
    int i;
    TASK aux;
    if (rt_size < RT_MAX)
    {
        rt_timeline[rt_size] = task;
        rt_size++;
        for (i = rt_size - 2; i >= 0; i--)
        {
            if (rt_timeline[i].t_states > rt_timeline[i + 1].t_states)
            {
                aux = rt_timeline[i];
                rt_timeline[i] = rt_timeline[i + 1];
                rt_timeline[i + 1] = aux;
            }
        }
        return true;
    }
    else
    {
        return false;
    }
}

void *rt_run(void *args)
{
	running = true;
	time_start = time_in_seconds();
    while (running)
    {
		z80_t_states_all = (rt_size == 0 ? z80_t_states_all : rt_timeline[0].t_states);
        time_sync();
        if (rt_is_pending)
        {
            rt_add_task(rt_pending);
            rt_is_pending = false;
        }
        if (rt_size > 0 && z80_t_states_all >= rt_timeline[0].t_states)
        {
            rt_timeline[0].task();
            rt_advance_head();
        }
    }
    return NULL;
}

// ===ULA================================================

void ula_point(const int x, const int y, const int c, const bool b)
{
    RGB color = (b ? ula_bright_colors[c] : ula_colors[c]);
    ula_screen[y][x] = color;
}

int ula_draw_line()
{
    int i, j, x = 0, y = ula_line;
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
    else if (y < SCREEN_HEIGHT)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            ula_point(j, y, ula_border_color, false);
        }
    }
    else
    {
        z80_maskable_interrupt_flag = true;
        ula_addr_bitmap.byte_value = 0x4000;
        ula_line = 0;
        ula_draw_counter = (ula_draw_counter + 1) % 16;
        return 8 * 224; // vertical retrace
    }
    ula_line++;
    return 224;
}

void ula_run()
{
    rt_add_task((TASK){.t_states = z80_t_states_all + ula_draw_line(), .task = ula_run});
}

// ===SOUND==============================================

void pcm_run()
{
	unsigned char frame = sound_ear * 128;
	int err = snd_pcm_writei(pcm_handle, &frame, 1);
    if (err == -EPIPE)
    {
        snd_pcm_prepare(pcm_handle);
    }
    else if (err == -ESTRPIPE)
    {
        while ((err = snd_pcm_resume(pcm_handle)) == -EAGAIN)
        {
            time_sleep_in_seconds(1.0);
		}
        if (err < 0)
        {
            snd_pcm_prepare(pcm_handle);
        }
    }
    rt_add_task((TASK){.t_states = z80_t_states_all + pcm_states, .task = pcm_run});
}

int pcm_config()
{
	int i = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (i == 0)
	{
		i = snd_pcm_set_params(pcm_handle,
				  SND_PCM_FORMAT_U8,
				  SND_PCM_ACCESS_RW_INTERLEAVED,
				  1,
				  PCM_SAMPLE,
				  1,
				  100000);
	}
	return i;
}
// ===TAPE===============================================

int tape_play_block()
{
    int i, j, k, n, b, v, s = 0;
    REG8BLOCK *block = tape_block_last;
    if (block != NULL)
    {
        for (i = 0; i < block->pilot_tone; i++)
        {
            s++;
            if (tape_load_state + 1 == s)
            {
                tape_load_state = s;
                sound_ear_on_off(!sound_ear);
                return block->pilot_pulse;
            }
        }
        for (i = 0; i < block->sync_size; i++)
        {
            s++;
            if (tape_load_state + 1 == s)
            {
                tape_load_state = s;
                sound_ear_on_off(!sound_ear);
                return block->sync_pulse[i];
            }
        }
        for (i = 0; i < block->size; i++)
        {
            b = MAX7;
            n = (i == block->size - 1 ? block->last_used : 8);
            for (j = 0; j < n; j++)
            {
                v = (register_is_bit(block->data[i], b) ? block->one_pulse : block->zero_pulse);
                for (k = 0; k < block->pulses_per_sample; k++)
                {
                    s++;
                    if (tape_load_state + 1 == s)
                    {
                        tape_load_state = s;
                        sound_ear_on_off(!sound_ear);
                        return v;
                    }
                }
                b >>= 1;
            }
        }
        if ((block->pause > 0)
            || (block->pause == 0 && tape_block_last->next == NULL))
        {
            s++;
            if (tape_load_state + 1 == s)
            {
                tape_load_state = s;
                if (!sound_ear)
                {
                    sound_ear_on_off(true);
                    return 1 / (1000 * state_duration);
                }
            }
            s++;
            if (tape_load_state + 1 == s)
            {
                tape_load_state = 0;
                sound_ear_on_off(false);
                tape_block_last = tape_block_last->next;
                return block->pause / (1000 * state_duration);
            }
        }
        else if (block->pause == 0)
        {
            tape_load_state = 0;
            tape_block_last = tape_block_last->next;
            return 0;
        }
    }
    return -1;
}

REG8BLOCK *tape_allocate(int size, int sync_size)
{
    REG8BLOCK *b = calloc(1, sizeof(REG8BLOCK));
    b->size = size;
    b->data = calloc(1, size);
    b->sync_size = sync_size;
    b->sync_pulse = malloc(sync_size * sizeof(short));
    b->next = NULL;
    if (tape_block_head == NULL)
    {
        tape_block_head = b;
    }
    else
    {
        tape_block_last->next = b;
    }
    tape_block_last = b;
    return b;
}

void tape_default_timings(REG8BLOCK *block)
{
    block->pilot_pulse = 2168;
    block->sync_pulse[0] = 667;
    block->sync_pulse[1] = 735;
    block->zero_pulse = 855;
    block->one_pulse = 1710;
    block->pulses_per_sample = 2;
    block->last_used = 8;
    block->pilot_tone = (block->data[0].byte_value < 0x80 ? 8063 : 3223);
}

bool tape_read(int fd, void *buffer, int size, bool rec)
{
    int i = 0, j;
    char *b = buffer;
    while (i < size && running)
    {
        j = read(fd, &b[i], size - i);
        if (j == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                time_sleep_in_seconds(0.1);
            }
            else
            {
                return false;
            }
        }
        else if (j == 0)
        {
            return false;
        }
        else
        {
            i += j;
            if (rec)
            {
                tape_index += j;
            }
        }
    }
    return true;
}

void tape_read_block_10(int fd)
{
    int pause = 0, size = 0;
    tape_read(fd, &pause, 2, true);
    tape_read(fd, &size, 2, true);
    REG8BLOCK *b = tape_allocate(size, 2);
    b->pause = pause;
    tape_read(fd, b->data, b->size, true);
    tape_default_timings(b);
}

void tape_read_block_11(int fd)
{
    REG8BLOCK block;
    unsigned short sync_pulse[2];
    tape_read(fd, &block.pilot_pulse, 2, true);
    tape_read(fd, &sync_pulse[0], 2, true);
    tape_read(fd, &sync_pulse[1], 2, true);
    tape_read(fd, &block.zero_pulse, 2, true);
    tape_read(fd, &block.one_pulse, 2, true);
    tape_read(fd, &block.pilot_tone, 2, true);
    tape_read(fd, &block.last_used, 1, true);
    tape_read(fd, &block.pause, 2, true);
    tape_read(fd, &block.size, 3, true);
    REG8BLOCK *b = tape_allocate(block.size, 2);
    b->pilot_pulse = block.pilot_pulse;
    b->sync_pulse[0] = sync_pulse[0];
    b->sync_pulse[1] = sync_pulse[1];
    b->zero_pulse = block.zero_pulse;
    b->one_pulse = block.one_pulse;
    b->pilot_tone = block.pilot_tone;
    b->last_used = block.last_used;
    b->pulses_per_sample = 2;
    b->pause = block.pause;
    tape_read(fd, b->data, b->size, true);
}

void tape_read_block_12(int fd)
{
    REG8BLOCK *b = tape_allocate(0, 0);
    tape_read(fd, &b->pilot_pulse, 2, true);
    tape_read(fd, &b->pilot_tone, 2, true);
}

void tape_read_block_13(int fd)
{
    unsigned char sync_size;
    tape_read(fd, &sync_size, 1, true);
    REG8BLOCK *b = tape_allocate(0, sync_size);
    tape_read(fd, b->sync_pulse, sync_size * 2, true);
}

void tape_read_block_14(int fd)
{
    REG8BLOCK block;
    tape_read(fd, &block.zero_pulse, 2, true);
    tape_read(fd, &block.one_pulse, 2, true);
    tape_read(fd, &block.last_used, 1, true);
    tape_read(fd, &block.pause, 2, true);
    tape_read(fd, &block.size, 3, true);
    REG8BLOCK *b = tape_allocate(block.size, 0);
    b->zero_pulse = block.zero_pulse;
    b->one_pulse = block.one_pulse;
    b->last_used = block.last_used;
    b->pulses_per_sample = 2;
    b->pause = block.pause;
    tape_read(fd, b->data, b->size, true);
}

void tape_read_block_15(int fd)
{
    unsigned char used;
    unsigned short states_per_sample, pause;
    int size = 0;
    tape_read(fd, &states_per_sample, 2, true);
    tape_read(fd, &pause, 2, true);
    tape_read(fd, &used, 1, true);
    tape_read(fd, &size, 3, true);
    REG8BLOCK *b = tape_allocate(ceil(size / 8.0), 0);
    tape_read(fd, b->data, b->size, true);
    b->zero_pulse = b->one_pulse = states_per_sample;
    b->last_used = used;
    b->pulses_per_sample = 1;
}

void tape_read_block_20(int fd, int index)
{
    unsigned short pause;
    tape_read(fd, &pause, 2, true);
    if (pause > 0)
    {
        REG8BLOCK *b = tape_allocate(0, 0);
        b->pause = pause;
    }
    else
    {
        tape_break_index++;
        if (tape_break_index == index)
        {
            while (tape_read(fd, &pause, 1, false));
        }
    }
}

void tape_read_block_21(int fd)
{
    unsigned char size;
    char text[256];
    tape_read(fd, &size, 1, true);
    tape_read(fd, text, size, true);
    text[size] = 0;
    printf("%s\n", text);
}

void tape_read_block_30(int fd)
{
    unsigned char size;
    char buffer[256];
    tape_read(fd, &size, 1, true);
    tape_read(fd, buffer, size, true);
    buffer[size] = 0;
    printf("%s\n", buffer);
}

void tape_read_block_32(int fd)
{
    unsigned short size;
    unsigned char n, l, t;
    int i;
    char buffer[256];
    char *types[] = {"Title", "Publisher", "Author", "Year",
        "Language", "Type", "Price", "Protection", "Origin",
        "Comment", ""};
    tape_read(fd, &size, 2, true);
    tape_read(fd, &n, 1, true);
    for (i = 0; i < n; i++)
    {
        tape_read(fd, &t, 1, true);
        tape_read(fd, &l, 1, true);
        tape_read(fd, buffer, l, true);
        buffer[l] = 0;
        if (t == 0xFF)
        {
            t = 9;
        }
        else if (t > 8)
        {
            t = 10;
        }
        printf("%s: %s\n", types[t], buffer);
    }
}

void tape_read_block_33(int fd)
{
    unsigned char n, t, id, info;
    int i;
    tape_read(fd, &n, 1, true);
    for (i = 0; i < n; i++)
    {
        tape_read(fd, &t, 1, true);
        tape_read(fd, &id, 1, true);
        tape_read(fd, &info, 1, true);
        printf("Hardware ID: %02x, Info: %02x\n", id, info);
    }
}

void tape_read_block_35(int fd)
{
    unsigned short l;
    char buffer[11];
    char *info;
    tape_read(fd, buffer, 10, true);
    printf("%s\n", buffer);
    buffer[10] = 0;
    tape_read(fd, &l, 2, true);
    info = malloc(l);
    tape_read(fd, info, l, true);
    free(info);
}

void tape_read_block_5A(int fd)
{
    char buffer[9];
    tape_read(fd, buffer, 9, true);
}

bool tape_wait(int fd, int event)
{
    int r = 0;
    struct pollfd p = {.fd = fd};
    if (event == TAPE_LOAD_EVENT)
    {
        p.events = POLLIN;
    }
    else if (event == TAPE_SAVE_EVENT)
    {
        p.events = POLLOUT;
    }
    while (r == 0 && running)
    {
        r = poll(&p, 1, 100);
    }
    if (r > 0 && !(p.revents & POLLERR))
    {
        if (event == TAPE_LOAD_EVENT)
        {
            return (p.revents & POLLIN);
        }
        else if (event == TAPE_SAVE_EVENT)
        {
            return (p.revents & POLLOUT);
        }
    }
    return false;
}

void tape_close()
{
    tape_block_last = NULL;
    while (tape_block_head != NULL)
    {
        REG8BLOCK *p = tape_block_head;
        tape_block_head = tape_block_head->next;
        if (p->size > 0)
        {
            free(p->data);
        }
        if (p->sync_size > 0)
        {
            free(p->sync_pulse);
        }
        free(p);
    }
}

bool tape_header(int fd)
{
    REG8 block[10];
    return (tape_read(fd, block, 10, true)
        && strncmp("ZXTape!\x1A", (const char *)block, 8) == 0
        && block[8].byte_value == 1);
}

void tape_load_tzx(int fd, int index)
{
    char id;
    tape_index = 0;
    tape_break_index = 0;
    if (tape_header(fd))
    {
        while (tape_read(fd, &id, 1, true) && running)
        {
            switch (id)
            {
            case 0x10:
                tape_read_block_10(fd);
                break;
            case 0x11:
                tape_read_block_11(fd);
                break;
            case 0x12:
                tape_read_block_12(fd);
                break;
            case 0x13:
                tape_read_block_13(fd);
                break;
            case 0x14:
                tape_read_block_14(fd);
                break;
            case 0x15:
                tape_read_block_15(fd);
                break;
            case 0x20:
                tape_read_block_20(fd, index);
                break;
            case 0x21:
                tape_read_block_21(fd);
                break;
            case 0x22:
                break;
            case 0x30:
                tape_read_block_30(fd);
                break;
            case 0x32:
                tape_read_block_32(fd);
                break;
            case 0x33:
                tape_read_block_33(fd);
                break;
            case 0x35:
                tape_read_block_35(fd);
                break;
            case 0x5A:
                tape_read_block_5A(fd);
                break;
            default:
                printf("Unknown block type %02x\n", id);
                return;
            }
        }
    }
}

void tape_play_run()
{
    sound_input = true;
    int s;
    do
    {
        s = tape_play_block();
    }
    while (s == 0 && running);
    if (s > 0)
    {
        rt_add_task((TASK){.t_states = z80_t_states_all + s, .task = tape_play_run});
    }
    else
    {
        sound_input = false;
    }
}

void write_tzx_header(int fd)
{
    char buffer[10] = "ZXTape!";
    buffer[7] = 0x1A;
    buffer[8] = 1;
    buffer[9] = 20;
    write(fd, buffer, 10);
}

void write_tzx_block_10(int fd, char *data, unsigned short size)
{
    char buffer[5];
    buffer[0] = 0x10;
    *(unsigned short *) &buffer[1] = 0x3E8;
    *(unsigned short *) &buffer[3] = size;
    write(fd, buffer, 5);
    write(fd, data, size);
}

void tape_record(int counter, unsigned long long duration)
{
    int i;
    if (duration == 2168 && tape_save_state == 0)
    {
        if (counter == 8063)
        {
            tape_save_state = 1;
            return;
        }
        else if (counter == 3222)
        {
            i = *(unsigned short *) &tape_save_buffer[12] + 2;
            if (i > tape_save_buffer_size)
            {
                tape_save_buffer = realloc(tape_save_buffer, i);
            }
            tape_save_buffer_size = i;
            tape_save_state = 1;
            return;
        }
    }
    if (tape_save_state == 1 && counter == 1
        && duration == 667)
    {
        tape_save_state++;
        return;
    }
    if (tape_save_state == 2 && counter == 1
        && duration == 735)
    {
        tape_save_state++;
        return;
    }
    if (tape_save_state == 3)
    {
        if (duration == 855 || duration == 852
            || duration == 856 || duration == 854)
        {
            for (i = 0; i < counter / 2; i++)
            {
                tape_save_buffer[tape_save_index / 8] <<= 1;
                tape_save_index++;
            }
            if (tape_save_index / 8 == tape_save_buffer_size)
            {
                tape_save_state++;
            }
            return;
        }
        else if (duration == 1710 || duration == 1711
            || duration == 1707 || duration == 1709)
        {
            for (i = 0; i < counter / 2; i++)
            {
                tape_save_buffer[tape_save_index / 8] <<= 1;
                tape_save_buffer[tape_save_index / 8] |= MAX0;
                tape_save_index++;
            }
            if (tape_save_index / 8 == tape_save_buffer_size)
            {
                tape_save_state++;
            }
            return;
        }
        else
        {
            tape_save_state++;
            return;
        }
    }
    else if (tape_save_state == 4)
    {
        return;
    }
    tape_save_state = 0;
    tape_save_index = 0;
}

void tape_listen()
{
    unsigned long long duration;
    if (tape_save_state != -1)
    {
        if (tape_save_mic != sound_mic)
        {
            duration = z80_t_states_all - tape_save_t_states;
            if (duration > tape_save_duration - 5
                && duration < tape_save_duration + 5)
            {
                tape_save_counter++;
            }
            else
            {
                tape_record(tape_save_counter, tape_save_duration);
                tape_save_duration = duration;
                tape_save_counter = 1;
            }
            tape_save_t_states = z80_t_states_all;
            tape_save_mic = sound_mic;
        }
        rt_add_task((TASK){.t_states = rt_next_t_states(), .task = tape_listen});
    }
}

void *tape_run_save(void *args)
{
    int fd;
    while (running)
    {
        fd = open("save", O_WRONLY | O_NONBLOCK);
        if (fd == -1)
        {
            time_sleep_in_seconds(0.1);
        }
        else
        {
            tape_save_state = 0;
            tape_save_t_states = 0;
            tape_save_counter = 0;
            tape_save_duration = 0;
            tape_save_index = 0;
            tape_save_mic = false;
            tape_save_buffer_size = 19;
            tape_save_buffer = realloc(tape_save_buffer, tape_save_buffer_size);
            rt_add_pending_task((TASK){.t_states = z80_t_states_all, .task = tape_listen});
            write_tzx_header(fd);
            while (tape_wait(fd, TAPE_SAVE_EVENT))
            {
                if (tape_save_state != 4)
                {
                    time_sleep_in_seconds(0.1);
                }
                else
                {
                    write_tzx_block_10(fd, tape_save_buffer, tape_save_buffer_size);
                    tape_save_state = 0;
                    tape_save_index = 0;
                }
            }
            tape_save_state = -1;
            free(tape_save_buffer);
            tape_save_buffer = NULL;
            close(fd);
        }
    };
    return NULL;
}

void *tape_run_load(void *args)
{
    int fd;
    while (running)
    {
        fd = open("load", O_RDONLY | O_NONBLOCK);
        if (fd != -1)
        {
            if (tape_wait(fd, TAPE_LOAD_EVENT))
            {
                tape_close();
                tape_load_tzx(fd, 1);
                tape_block_last = tape_block_head;
                tape_load_state = 0;
                rt_add_pending_task((TASK){.t_states = z80_t_states_all, .task = tape_play_run});
            }
            close(fd);
        }
        else
        {
            break;
        }
    }
    tape_close();
    return NULL;
}

// ======================================================

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

void z80_run()
{
    rt_add_task((TASK){.t_states = z80_t_states_all + z80_run_one(), z80_run});
}

void window_show(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE);
    glutInitWindowSize(SCREEN_WIDTH * SCREEN_ZOOM, SCREEN_HEIGHT * SCREEN_ZOOM);

    // glutInitWindowPosition(200, 100);
    glutCreateWindow("Cristian Mocanu Z80");
    glPointSize(SCREEN_ZOOM);
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
}

int main(int argc, char **argv)
{
    pthread_t rt_id, tape_load_id, tape_save_id;
    int fd, index, pcm_ok;
    char *buffer;
    pthread_attr_t a;
    struct sched_param p = {.sched_priority = 10};
    if (system_little_endian())
    {
		z80_reset();
        if (argc >= 2)
        {
            if (file_has_extension(argv[1], ".rom"))
            {
                file_load_rom(argv[1]);
            }
            else if (file_has_extension(argv[1], ".sna"))
            {
                file_load_sna(argv[1]);
                z80_reg_pc = z80_pop16();
                z80_iff1 = z80_iff2;
            }
            else if (file_has_extension(argv[1], ".tzx"))
            {
                fd = open(argv[1], O_RDONLY);
                if (fd != -1)
                {
                    if (argc == 3 && argv[2][0] == '-' && argv[2][1] == 'p')
                    {
                        index = atoi(&argv[2][2]);
                        if (index == 0)
                        {
                            buffer = malloc(strlen(argv[1]) + 13);
                            sprintf(buffer, "cat \"%s\" > load", argv[1]);
                            system(buffer);
                            free(buffer);
                        }
                        else
                        {
                            tape_load_tzx(fd, index);
                            tape_index++;
                            buffer = malloc(strlen(argv[1]) + (floor(log10(tape_index)) + 1) + 40);
                            sprintf(buffer, "{ head -c 10 \"%1$s\"; tail -c +%2$lld \"%1$s\"; } > load", argv[1], tape_index);
                            system(buffer);
                            free(buffer);
                        }
                    }
                    else
                    {
                        tape_load_tzx(fd, 0);
                    }
                    tape_close();
                    close(fd);
                }
                return 0;
            }
            else
            {
                printf("Unkown file format %s\n", argv[1]);
                return 1;
            }
        }
        pcm_ok = pcm_config();
        window_show(argc, argv);
        rt_add_task((TASK){.t_states = 0, ula_run});
        rt_add_task((TASK){.t_states = 0, z80_run});
        if (pcm_ok == 0)
        {
			rt_add_task((TASK){.t_states = z80_t_states_all + pcm_states, pcm_run});
		}
		pthread_attr_init(&a);
		pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(&a, SCHED_FIFO);
		pthread_attr_setschedparam(&a, &p);
        if (pthread_create(&rt_id, &a, rt_run, NULL) != 0)
        {
			pthread_create(&rt_id, NULL, rt_run, NULL);
		}
        pthread_attr_destroy(&a);
        pthread_create(&tape_load_id, NULL, tape_run_load, NULL);
        pthread_create(&tape_save_id, NULL, tape_run_save, NULL);
        glutDisplayFunc(draw_screen);
        glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
        glutMainLoop();
        running = false;
        if (pcm_ok == 0)
        {
			snd_pcm_close(pcm_handle);
		}
        pthread_join(rt_id, NULL);
        pthread_join(tape_load_id, NULL);
        pthread_join(tape_save_id, NULL);
		if (argc == 3 && argv[2][0] == '-' && argv[2][1] == 'o')
        {
			z80_push16(z80_reg_pc);
			file_save_sna("out.sna");
		}
    }
    return 0;
}

// TODO: ula task each 4 states and horizontal retrace
// TODO: uart
// TODO: use pixel shader with drawArrays
// https://stackoverflow.com/questions/19102180/how-does-gldrawarrays-know-what-to-draw
// TODO: replace glut with x calls to create window and read keyboard
