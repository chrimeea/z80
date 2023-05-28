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

REG8 memory[MAX16];
REG8 keyboard[] = {(REG8){.value=0x1F}, (REG8){.value=0x1F}, (REG8){.value=0x1F},
    (REG8){.value=0x1F}, (REG8){.value=0x1F}, (REG8){.value=0x1F}, (REG8){.value=0x1F},
    (REG8){.value=0x1F}};
const unsigned int memory_size = MAX16;
const unsigned int ula_t_states_per_line = 224;
long double time_start, state_duration = 0.00000035L;
unsigned long z80_t_states_all = 0, ula_t_states_all = 0;
unsigned int ula_draw_counter = 0;
bool running = true, z80_maskable_interrupt_flag = false;

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

bool register_is_bit(REG8 reg, int b) {
    return reg.byte_value & (1 << b);
}

void register_set_bit(REG8 reg, int b, bool value) {
    if (value) {
        reg.byte_value |= (1 << b);
    } else {
        reg.byte_value &= ~(1 << b);
    }
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

REG8 memory_read8(const REG16 reg) {
    return memory[reg.byte_value];
}

REG8 memory_read8_indexed(const REG16 reg16, const REG8 reg8) {
    REG16 alt;
    alt.byte_value = reg16.byte_value + reg8.value;
    return memory_read8(alt);
}

REG16 memory_read16(REG16 reg) {
    REG16 alt;
    alt.bytes.low = memory_read8(reg);
    reg.value += 1;
    alt.bytes.high = memory_read8(reg);
    return alt;
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
    //     register_set_bit(keyboard[0], 0, value);
    if (key == 'z' || key == 'Z') {
        register_set_bit(keyboard[0], 1, value);
    } else if (key == 'x' || key == 'X') {
        register_set_bit(keyboard[0], 2, value);
    } else if (key == 'c' || key == 'C') {
        register_set_bit(keyboard[0], 3, value);
    } else if (key == 'v' || key == 'V') {
        register_set_bit(keyboard[0], 4, value);
    } else if (key == 'a' || key == 'A') {
        register_set_bit(keyboard[1], 0, value);
    } else if (key == 's' || key == 'S') {
        register_set_bit(keyboard[1], 1, value);
    } else if (key == 'd' || key == 'D') {
        register_set_bit(keyboard[1], 2, value);
    } else if (key == 'f' || key == 'F') {
        register_set_bit(keyboard[1], 3, value);
    } else if (key == 'g' || key == 'G') {
        register_set_bit(keyboard[1], 4, value);
    } else if (key == 'q' || key == 'Q') {
        register_set_bit(keyboard[2], 0, value);
    } else if (key == 'w' || key == 'W') {
        register_set_bit(keyboard[2], 1, value);
    } else if (key == 'e' || key == 'E') {
        register_set_bit(keyboard[2], 2, value);
    } else if (key == 'r' || key == 'R') {
        register_set_bit(keyboard[2], 3, value);
    } else if (key == 't' || key == 'T') {
        register_set_bit(keyboard[2], 4, value);
    } else if (key == '1') {
        register_set_bit(keyboard[3], 0, value);
    } else if (key == '2') {
        register_set_bit(keyboard[3], 1, value);
    } else if (key == '3') {
        register_set_bit(keyboard[3], 2, value);
    } else if (key == '4') {
        register_set_bit(keyboard[3], 3, value);
    } else if (key == '5') {
        register_set_bit(keyboard[3], 4, value);
    } else if (key == '0') {
        register_set_bit(keyboard[4], 0, value);
    } else if (key == '9') {
        register_set_bit(keyboard[4], 1, value);
    } else if (key == '8') {
        register_set_bit(keyboard[4], 2, value);
    } else if (key == '7') {
        register_set_bit(keyboard[4], 3, value);
    } else if (key == '6') {
        register_set_bit(keyboard[4], 4, value);
    } else if (key == 'p' || key == 'P') {
        register_set_bit(keyboard[5], 0, value);
    } else if (key == 'o' || key == 'O') {
        register_set_bit(keyboard[5], 1, value);
    } else if (key == 'i' || key == 'I') {
        register_set_bit(keyboard[5], 2, value);
    } else if (key == 'u' || key == 'U') {
        register_set_bit(keyboard[5], 3, value);
    } else if (key == 'y' || key == 'Y') {
        register_set_bit(keyboard[5], 4, value);
    } else if (key == 13) {
        register_set_bit(keyboard[6], 0, value);
    } else if (key == 'l' || key == 'L') {
        register_set_bit(keyboard[6], 1, value);
    } else if (key == 'k' || key == 'K') {
        register_set_bit(keyboard[6], 2, value);
    } else if (key == 'j' || key == 'J') {
        register_set_bit(keyboard[6], 3, value);
    } else if (key == 'h' || key == 'H') {
        register_set_bit(keyboard[6], 4, value);
    } else if (key == ' ') {
        register_set_bit(keyboard[7], 0, value);
    // } else if (strcmp(key, "Shift_L") == 0 || strcmp(key, "Shift_R") == 0) {
    //     register_set_bit(keyboard[7], 1, value);
    } else if (key == 'm' || key == 'M') {
        register_set_bit(keyboard[7], 2, value);
    } else if (key == 'n' || key == 'N') {
        register_set_bit(keyboard[7], 3, value);
    } else if (key == 'b' || key == 'B') {
        register_set_bit(keyboard[7], 4, value);
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

unsigned int z80_run_one() {
    return 0;
}

void *z80_run(void *args) {
    while (running) {
        time_sync(&z80_t_states_all, z80_run_one());
    }
    return NULL;
}

void ula_point(const int x, const int y, const int c, const int b) {
}

void ula_draw_line(int i) {    
}

void ula_draw_screen_once() {
    glClear(GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();
}

void *ula_draw_screen(void *args) {
    while (running) {
        z80_maskable_interrupt_flag = true;
        ula_draw_screen_once();
        ula_draw_counter += 1;
        if (ula_draw_counter == 16) {
            ula_draw_counter = 0;
        }
    }
    return NULL;
}

int main(int argc, char** argv) {
    pthread_t z80_id, ula_id;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH);

    int width = 304;
    int height = 240;
    glutInitWindowSize(width, height);

    // int x = 200;
    // int y = 100;
    // glutInitWindowPosition(x, y);
    glutCreateWindow("Cristian Mocanu Z80");

    GLclampf Red = 0.0f, Green = 0.0f, Blue = 0.0f, Alpha = 0.0f;
    glClearColor(Red, Green, Blue, Alpha);

    // glutDisplayFunc(renderScene);
    glutKeyboardFunc(keyboard_press_down);
    glutKeyboardUpFunc(keyboard_press_up);
    time_start = time_in_seconds();
    pthread_create(&z80_id, NULL, z80_run, NULL);
    pthread_create(&ula_id, NULL, ula_draw_screen, NULL);
    glutMainLoop();
    running = false;
    return 0;
}