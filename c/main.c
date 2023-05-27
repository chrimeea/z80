//g++ main.c -Ofast -lGLEW -lGLU -lGL -lglut -pthread -Wall

#include <GL/freeglut.h>
#include <stdio.h>
#include <pthread.h>

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
const int memory_size = MAX16;

void memory_load_rom(char *filename) {
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

REG8 memory_read8(REG16 reg) {
    return memory[reg.byte_value];
}

REG8 memory_read8_indexed(REG16 reg16, REG8 reg8) {
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

REG8 keyboard_read8(REG16 reg) {
    REG8 alt;
    return alt;
}

REG8 port_read8(REG16 reg) {
    if (reg.byte_value == 0xFE) {
        return keyboard_read8(reg);
    } else {
        return (REG8){.byte_value=0xFF};
    }
}

void port_write8(REG16 reg, REG8 alt) {
}

void *z80_run(void *args) {
    return NULL;
}

void ula_draw_screen(int value) {
    glClear(GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();
    glutTimerFunc(1000, ula_draw_screen, 0);
}

int main(int argc, char** argv) {
    pthread_t run_id;

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
    pthread_create(&run_id, NULL, z80_run, NULL);
    glutTimerFunc(100, ula_draw_screen, 0);
    glutMainLoop();

    return 0;
}