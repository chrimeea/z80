//gcc main.c -Ofast -lGLEW -lGLU -lGL -lglut -pthread -Wall

#include <GL/freeglut.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

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

void keyboard_press(const char *key, const bool value) {
    if (strcmp(key, "Caps_Lock") == 0) {
        register_set_bit(keyboard[0], 0, value);
    } else if (strcasecmp(key, "z") == 0) {
        register_set_bit(keyboard[0], 1, value);
    } else if (strcasecmp(key, "x") == 0) {
        register_set_bit(keyboard[0], 2, value);
    } else if (strcasecmp(key, "c") == 0) {
        register_set_bit(keyboard[0], 3, value);
    } else if (strcasecmp(key, "v") == 0) {
        register_set_bit(keyboard[0], 4, value);
    } else if (strcasecmp(key, "a") == 0) {
        register_set_bit(keyboard[1], 0, value);
    } else if (strcasecmp(key, "s") == 0) {
        register_set_bit(keyboard[1], 1, value);
    } else if (strcasecmp(key, "d") == 0) {
        register_set_bit(keyboard[1], 2, value);
    } else if (strcasecmp(key, "f") == 0) {
        register_set_bit(keyboard[1], 3, value);
    } else if (strcasecmp(key, "g") == 0) {
        register_set_bit(keyboard[1], 4, value);
    } else if (strcasecmp(key, "q") == 0) {
        register_set_bit(keyboard[2], 0, value);
    } else if (strcasecmp(key, "w") == 0) {
        register_set_bit(keyboard[2], 1, value);
    } else if (strcasecmp(key, "e") == 0) {
        register_set_bit(keyboard[2], 2, value);
    } else if (strcasecmp(key, "r") == 0) {
        register_set_bit(keyboard[2], 3, value);
    } else if (strcasecmp(key, "t") == 0) {
        register_set_bit(keyboard[2], 4, value);
    } else if (strcmp(key, "1") == 0) {
        register_set_bit(keyboard[3], 0, value);
    } else if (strcmp(key, "2") == 0) {
        register_set_bit(keyboard[3], 1, value);
    } else if (strcmp(key, "3") == 0) {
        register_set_bit(keyboard[3], 2, value);
    } else if (strcmp(key, "4") == 0) {
        register_set_bit(keyboard[3], 3, value);
    } else if (strcmp(key, "5") == 0) {
        register_set_bit(keyboard[3], 4, value);
    } else if (strcmp(key, "0") == 0) {
        register_set_bit(keyboard[4], 0, value);
    } else if (strcmp(key, "9") == 0) {
        register_set_bit(keyboard[4], 1, value);
    } else if (strcmp(key, "8") == 0) {
        register_set_bit(keyboard[4], 2, value);
    } else if (strcmp(key, "7") == 0) {
        register_set_bit(keyboard[4], 3, value);
    } else if (strcmp(key, "6") == 0) {
        register_set_bit(keyboard[4], 4, value);
    } else if (strcasecmp(key, "p") == 0) {
        register_set_bit(keyboard[5], 0, value);
    } else if (strcasecmp(key, "o") == 0) {
        register_set_bit(keyboard[5], 1, value);
    } else if (strcasecmp(key, "i") == 0) {
        register_set_bit(keyboard[5], 2, value);
    } else if (strcasecmp(key, "u") == 0) {
        register_set_bit(keyboard[5], 3, value);
    } else if (strcasecmp(key, "y") == 0) {
        register_set_bit(keyboard[5], 4, value);
    } else if (strcmp(key, "Return") == 0 || strcmp(key, "KP_Enter") == 0) {
        register_set_bit(keyboard[6], 0, value);
    } else if (strcasecmp(key, "l") == 0) {
        register_set_bit(keyboard[6], 1, value);
    } else if (strcasecmp(key, "k") == 0) {
        register_set_bit(keyboard[6], 2, value);
    } else if (strcasecmp(key, "j") == 0) {
        register_set_bit(keyboard[6], 3, value);
    } else if (strcasecmp(key, "h") == 0) {
        register_set_bit(keyboard[6], 4, value);
    } else if (strcmp(key, "space") == 0) {
        register_set_bit(keyboard[7], 0, value);
    } else if (strcmp(key, "Shift_L") == 0 || strcmp(key, "Shift_R") == 0) {
        register_set_bit(keyboard[7], 1, value);
    } else if (strcasecmp(key, "m") == 0) {
        register_set_bit(keyboard[7], 2, value);
    } else if (strcasecmp(key, "n") == 0) {
        register_set_bit(keyboard[7], 3, value);
    } else if (strcasecmp(key, "b") == 0) {
        register_set_bit(keyboard[7], 4, value);
    }
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

void *z80_run(void *args) {
    return NULL;
}

void ula_draw_screen(const int value) {
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