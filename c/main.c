//gcc main.c -Ofast -lGLEW -lGLU -lGL -lglut -pthread -Wall

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

#define is_bit(I, B) (I & (1 << (B)))
#define register_is_bit(R, B) (is_bit(R.byte_value, B))
#define set_bit(I, B) (I |= (1 << (B)))
#define unset_bit(I, B) (I &= ~(1 << (B)))
#define set_or_unset_bit(I, B, V) (V ? set_bit(I, B) : unset_bit(I, B))
#define register_set_or_unset_bit(R, B, V) (set_or_unset_bit(R.byte_value, B, V))

typedef union {
    unsigned char byte_value;
    char value;
} REG8;

typedef union {
    struct {
        REG8 low;
        REG8 high;
    } bytes;
    unsigned short byte_value;
    short value;
} REG16;

typedef struct {
    GLfloat red;
    GLfloat green;
    GLfloat blue;
} RGB;

REG8 memory[MAX16];
REG8 keyboard[] = {(REG8){.value=0x1F}, (REG8){.value=0x1F}, (REG8){.value=0x1F},
    (REG8){.value=0x1F}, (REG8){.value=0x1F}, (REG8){.value=0x1F}, (REG8){.value=0x1F},
    (REG8){.value=0x1F}};
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
bool running;
bool z80_maskable_interrupt_flag, z80_nonmaskable_interrupt_flag;
bool z80_iff1, z80_iff2, z80_can_execute;
int z80_imode;

int system_little_endian() {
    int x = 1;
    return *(char*)&x;
}

long double time_in_seconds() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1000000000L;
}

void time_seconds_to_timespec(struct timespec *ts, long double s) {
    long double temp;
    ts->tv_nsec = modfl(s, &temp) * 1000000000;
    ts->tv_sec = temp;
}

void time_sync(unsigned long* t_states_all, unsigned int t_states) {
    *t_states_all += t_states;
    struct timespec ts;
    time_seconds_to_timespec(&ts, time_start + *t_states_all * state_duration - time_in_seconds());
    nanosleep(&ts, &ts);
}

void memory_load_rom(const char *filename) {
    int n, remaining = memory_size;
    REG8 *m = memory;
    FILE *f = fopen(filename, "rb");
    do {
        n = fread(m, 1, remaining, f);
        m += n;
        remaining -= n;
    } while (n != 0);
    fclose(f);
}

REG8 *memory_ref8(const REG16 reg) {
    return &memory[reg.byte_value];
}

REG8 memory_read8(const REG16 reg) {
    return *memory_ref8(reg);
}

void memory_write8(const REG16 reg, const REG8 alt) {
    *memory_ref8(reg) = alt;
}

REG8 *memory_ref8_indexed(const REG16 reg16, const REG8 reg8) {
    return &memory[reg16.byte_value + reg8.value];
}

REG8 memory_read8_indexed(const REG16 reg16, const REG8 reg8) {
    return *memory_ref8_indexed(reg16, reg8);
}

void memory_write8_indexed(const REG16 reg16, const REG8 reg8, REG8 alt) {
    *memory_ref8_indexed(reg16, reg8) = alt;
}

REG16 *memory_ref16(REG16 reg) {
    return (REG16 *)memory_ref8(reg);
}

REG16 memory_read16(REG16 reg) {
    return *memory_ref16(reg);
}

void memory_write16(REG16 reg, REG16 alt) {
    *memory_ref16(reg) = alt;
}

REG8 keyboard_read8(const REG16 reg) {
    REG8 alt;
    alt.byte_value = 0x1F;
    for (int i = 0; i < 8; i++) {
        if (!register_is_bit(reg.bytes.high, i)) {
            alt.byte_value &= keyboard[i].byte_value;
        }
    }
    return alt;
}

void keyboard_press(unsigned char key, const bool value) {
    // if (strcmp(key, "Caps_Lock") == 0) {
    //     register_set_or_unset_bit(keyboard[0], 0, value);
    if (key == 'z' || key == 'Z') {
        register_set_or_unset_bit(keyboard[0], 1, value);
    } else if (key == 'x' || key == 'X') {
        register_set_or_unset_bit(keyboard[0], 2, value);
    } else if (key == 'c' || key == 'C') {
        register_set_or_unset_bit(keyboard[0], 3, value);
    } else if (key == 'v' || key == 'V') {
        register_set_or_unset_bit(keyboard[0], 4, value);
    } else if (key == 'a' || key == 'A') {
        register_set_or_unset_bit(keyboard[1], 0, value);
    } else if (key == 's' || key == 'S') {
        register_set_or_unset_bit(keyboard[1], 1, value);
    } else if (key == 'd' || key == 'D') {
        register_set_or_unset_bit(keyboard[1], 2, value);
    } else if (key == 'f' || key == 'F') {
        register_set_or_unset_bit(keyboard[1], 3, value);
    } else if (key == 'g' || key == 'G') {
        register_set_or_unset_bit(keyboard[1], 4, value);
    } else if (key == 'q' || key == 'Q') {
        register_set_or_unset_bit(keyboard[2], 0, value);
    } else if (key == 'w' || key == 'W') {
        register_set_or_unset_bit(keyboard[2], 1, value);
    } else if (key == 'e' || key == 'E') {
        register_set_or_unset_bit(keyboard[2], 2, value);
    } else if (key == 'r' || key == 'R') {
        register_set_or_unset_bit(keyboard[2], 3, value);
    } else if (key == 't' || key == 'T') {
        register_set_or_unset_bit(keyboard[2], 4, value);
    } else if (key == '1') {
        register_set_or_unset_bit(keyboard[3], 0, value);
    } else if (key == '2') {
        register_set_or_unset_bit(keyboard[3], 1, value);
    } else if (key == '3') {
        register_set_or_unset_bit(keyboard[3], 2, value);
    } else if (key == '4') {
        register_set_or_unset_bit(keyboard[3], 3, value);
    } else if (key == '5') {
        register_set_or_unset_bit(keyboard[3], 4, value);
    } else if (key == '0') {
        register_set_or_unset_bit(keyboard[4], 0, value);
    } else if (key == '9') {
        register_set_or_unset_bit(keyboard[4], 1, value);
    } else if (key == '8') {
        register_set_or_unset_bit(keyboard[4], 2, value);
    } else if (key == '7') {
        register_set_or_unset_bit(keyboard[4], 3, value);
    } else if (key == '6') {
        register_set_or_unset_bit(keyboard[4], 4, value);
    } else if (key == 'p' || key == 'P') {
        register_set_or_unset_bit(keyboard[5], 0, value);
    } else if (key == 'o' || key == 'O') {
        register_set_or_unset_bit(keyboard[5], 1, value);
    } else if (key == 'i' || key == 'I') {
        register_set_or_unset_bit(keyboard[5], 2, value);
    } else if (key == 'u' || key == 'U') {
        register_set_or_unset_bit(keyboard[5], 3, value);
    } else if (key == 'y' || key == 'Y') {
        register_set_or_unset_bit(keyboard[5], 4, value);
    } else if (key == 13) {
        register_set_or_unset_bit(keyboard[6], 0, value);
    } else if (key == 'l' || key == 'L') {
        register_set_or_unset_bit(keyboard[6], 1, value);
    } else if (key == 'k' || key == 'K') {
        register_set_or_unset_bit(keyboard[6], 2, value);
    } else if (key == 'j' || key == 'J') {
        register_set_or_unset_bit(keyboard[6], 3, value);
    } else if (key == 'h' || key == 'H') {
        register_set_or_unset_bit(keyboard[6], 4, value);
    } else if (key == ' ') {
        register_set_or_unset_bit(keyboard[7], 0, value);
    // } else if (strcmp(key, "Shift_L") == 0 || strcmp(key, "Shift_R") == 0) {
    //     register_set_or_unset_bit(keyboard[7], 1, value);
    } else if (key == 'm' || key == 'M') {
        register_set_or_unset_bit(keyboard[7], 2, value);
    } else if (key == 'n' || key == 'N') {
        register_set_or_unset_bit(keyboard[7], 3, value);
    } else if (key == 'b' || key == 'B') {
        register_set_or_unset_bit(keyboard[7], 4, value);
    }
}

void keyboard_press_down(unsigned char key, int x, int y) {
    keyboard_press(key, true);
}

void keyboard_press_up(unsigned char key, int x, int y) {
    keyboard_press(key, false);
}

REG8 port_read8(const REG16 reg) {
    if (reg.byte_value == 0xFE) {
        return keyboard_read8(reg);
    } else {
        return (REG8){.byte_value = 0xFF};
    }
}

void port_write8(const REG16 reg, const REG8 alt) {
}

void z80_reset() {
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

void z80_memory_refresh() {
    z80_reg_r.byte_value = (z80_reg_r.byte_value + 1) % MAX7;
}

REG8 z80_next8() {
    REG8 v = memory_read8(z80_reg_pc);
    z80_reg_pc.byte_value++;
    return v;
}

REG16 z80_next16() {
    REG16 v = memory_read16(z80_reg_pc);
    z80_reg_pc.byte_value += 2;
    return v;
}

void z80_push16(const REG16 reg) {
    z80_reg_sp.byte_value -= 2;
    memory_write16(z80_reg_sp, reg);
}

REG16 z80_pop16() {
    REG16 v = memory_read16(z80_reg_sp);
    z80_reg_sp.byte_value += 2;
    return v;
}

REG8 z80_fetch_opcode() {
    z80_memory_refresh();
    return z80_next8();
}

unsigned int z80_decode_reg8(REG8 reg, int pos, unsigned int t, REG8 *result) {
    REG8 *a[] = {&z80_reg_bc.bytes.high, &z80_reg_bc.bytes.low,
        &z80_reg_de.bytes.high, &z80_reg_de.bytes.low,
        &z80_reg_hl.bytes.high, &z80_reg_hl.bytes.low,
        memory_ref8(z80_reg_hl), &z80_reg_af.bytes.high};
    int i = reg.byte_value >> pos & 0x07;
    result = a[i];
    return i == 0x06 ? t : 0;
}

void z80_decode_reg16(REG16 reg, int pos, REG16 *result) {
    REG16 *a[] = {&z80_reg_bc, &z80_reg_de, &z80_reg_hl, &z80_reg_sp};
    result = a[reg.byte_value >> pos & 0x03];
}

unsigned int z80_execute(REG8 reg) {
    switch (reg.byte_value) {
        case 0x00: //NOP
        return 4;
        case 0x01: //LD dd,nn
        case 0x11:
        case 0x21:
        case 0x31:
        return 10;
    }
    return 0;
}

unsigned int z80_nonmaskable_interrupt() {
    z80_iff2 = z80_iff1;
    z80_iff1 = false;
    z80_push16(z80_reg_pc);
    z80_reg_pc.byte_value = 0x66;
    return 11;
}

unsigned int z80_maskable_interrupt() {
    z80_iff1 = false;
    z80_iff2 = false;
    switch (z80_imode) {
        case 0:
        //TODO: wait 2 cycles for interrupting device to write to data_bus
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
        return 0; //fail
    }
}

unsigned int z80_run_one() {
    if (z80_nonmaskable_interrupt_flag) {
        z80_nonmaskable_interrupt_flag = false;
        return z80_nonmaskable_interrupt();
    } else if (z80_maskable_interrupt_flag) {
        z80_maskable_interrupt_flag = false;
        if (z80_iff1) {
            return z80_maskable_interrupt();
        }
    } else if (z80_can_execute) {
        return z80_execute(z80_fetch_opcode());
    }
    return 0;
}

void *z80_run(void *args) {
    while (running) {
        time_sync(&z80_t_states_all, z80_run_one());
    }
    return NULL;
}

void ula_point(const int x, const int y, const int c, const bool b) {
    RGB color = b ? ula_bright_colors[c] : ula_colors[c];
    glColor3f(color.red, color.green, color.blue);
    glBegin(GL_POINTS);
    glVertex2f((x + 48.0f) / 304.0f, (y + 48.0f) / 288.0f);
    glEnd();
}

unsigned int ula_draw_line(int y) {
    if (y > 63 && y < 256) {
        int x = 0;
        ula_addr_attrib.byte_value = 0x5800 + y / 8 * 32;
        for (int i = 0; i < 32; i ++) {
            REG8 reg_bitmap = memory_read8(ula_addr_bitmap);
            REG8 reg_attrib = memory_read8(ula_addr_attrib);
            int ink = reg_attrib.byte_value & 7;
            int paper = reg_attrib.byte_value >> 3 & 7;
            bool flash = register_is_bit(reg_attrib, 7);
            if (flash && ula_draw_counter == 0) {
                int temp = ink;
                ink = paper;
                paper = temp;
            }
            bool brightness = register_is_bit(reg_attrib, 6);
            for (int j = 0; j < 8; j++) {
                ula_point(x + j, y, register_is_bit(reg_bitmap, 7 - j) ? ink : paper, brightness);
            }
            ula_addr_bitmap.byte_value++;
            ula_addr_attrib.byte_value++;
            x += 8;
        }
        y++;
        register_set_or_unset_bit(ula_addr_bitmap, 5, is_bit(y, 3));
        register_set_or_unset_bit(ula_addr_bitmap, 6, is_bit(y, 4));
        register_set_or_unset_bit(ula_addr_bitmap, 7, is_bit(y, 5));
        register_set_or_unset_bit(ula_addr_bitmap, 8, is_bit(y, 0));
        register_set_or_unset_bit(ula_addr_bitmap, 9, is_bit(y, 1));
        register_set_or_unset_bit(ula_addr_bitmap, 10, is_bit(y, 2));
        register_set_or_unset_bit(ula_addr_bitmap, 11, is_bit(y, 6));
        register_set_or_unset_bit(ula_addr_bitmap, 12, is_bit(y, 7));
    }
    return 224;
}

void ula_draw_screen_once() {
    ula_addr_bitmap.byte_value = 0x4000;
    ula_addr_attrib.byte_value = 0;
    for (int i = 0; i < 312; i++) {
        time_sync(&ula_t_states_all, ula_draw_line(i));
    }
    glFlush();
}

void *ula_draw_screen(void *args) {
    while (running) {
        z80_maskable_interrupt_flag = true;
        ula_draw_screen_once();
        ula_draw_counter = (ula_draw_counter + 1) % 16;
    }
    return NULL;
}

int main(int argc, char** argv) {
    pthread_t z80_id, ula_id;
    if (system_little_endian()) {
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
        if (argc == 2) {
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

//TODO: bright colors
//TODO: keyboard caps lock and shift
//TODO: border, UART, sound, tape
//TODO: debugger
