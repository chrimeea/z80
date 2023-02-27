//g++ main.c -Ofast -lGLEW -lGLU -lGL -lglut -pthread -Wall

#include <GL/freeglut.h>
#include <stdio.h>
#include <pthread.h>

void *run(void *args) {
    return NULL;
}

void renderScene(int value) {
    glClear(GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();
    glutTimerFunc(1000, renderScene, 0);
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
    int win = glutCreateWindow("Tutorial 01");
    printf("window id: %d\n", win);

    GLclampf Red = 0.0f, Green = 0.0f, Blue = 0.0f, Alpha = 0.0f;
    glClearColor(Red, Green, Blue, Alpha);

    // glutDisplayFunc(renderScene);
    pthread_create(&run_id, NULL, run, NULL);
    glutTimerFunc(100, renderScene, 0);
    glutMainLoop();

    return 0;
}