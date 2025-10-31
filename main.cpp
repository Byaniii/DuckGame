#include <GL/glut.h>

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    
    glColor3f(0.0, 1.0, 0.0);
    glBegin(GL_LINES);
        glVertex2f(-0.5, -0.5);
        glVertex2f( 0.5,  0.5);
        glVertex2f(-0.5,  0.5);
        glVertex2f( 0.5, -0.5);
    glEnd();

    glPointSize(6.0f);
    glBegin(GL_POINTS);
        glColor3f(1.0, 1.0, 1.0); glVertex2f(0.0,  0.3); 
        glColor3f(0.0, 1.0, 0.0); glVertex2f(-0.3, 0.0); 
        glColor3f(0.0, 0.0, 1.0); glVertex2f(0.3,  0.0); 
        glColor3f(1.0, 0.0, 1.0); glVertex2f(0.0, -0.3); 
    glEnd();

    glFlush(); 
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize(500, 500);
    glutCreateWindow("Basic Rendering");
    glutDisplayFunc(display);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glutMainLoop();
    return 0;
}
