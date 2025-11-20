
// Cross-platform GLUT include
#if defined(__APPLE__)
  #include <GLUT/glut.h>
#else
  #include <GL/glut.h>
#endif

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <sstream>

// ---- Game State ----
struct Duck {
    float x, y;     // position
    float vx, vy;   // velocity
    float r;        // radius (hitbox & draw size)
    bool alive;
};

static int winW = 960, winH = 540;
static int origWinW = 960, origWinH = 540; // Store original window size
static bool isFullscreen = false;
static std::vector<Duck> ducks;
static int score = 0;
static int misses = 0;        // missed shots
static int lives = 3;         // lose a life when a duck escapes at top
static int wave = 1;
static bool gameOver = false;
static bool paused = false;
static int crossX = winW/2, crossY = winH/2; // mouse

// Utility random float in [a,b]
static float frand(float a, float b) { return a + (b - a) * (std::rand() / (float)RAND_MAX); }

static void resetGame() {
    ducks.clear();
    score = 0; misses = 0; lives = 3; wave = 1; gameOver = false; paused = false;
}

static void spawnDuck() {
    // Spawn near bottom with random side and velocity towards upper opposite
    Duck d{};
    d.r = frand(14.f, 22.f);
    d.alive = true;

    bool fromLeft = std::rand() % 2;
    d.x = fromLeft ? -40.f : (winW + 40.f);
    d.y = frand(40.f, 140.f);

    float speed = frand(120.f, 210.f) * (0.6f + 0.1f * wave); // scale with wave
    float angle = frand(0.25f, 0.6f); // radians upward
    d.vx = std::cos(angle) * speed * (fromLeft ? 1.f : -1.f);
    d.vy = std::sin(angle) * speed;

    ducks.push_back(d);
}

static void spawnWave(int n) {
    for (int i = 0; i < n; ++i) spawnDuck();
}

static void drawCircle(float cx, float cy, float r, int seg=24) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= seg; ++i) {
        float th = (2.f * 3.1415926f * i) / seg;
        glVertex2f(cx + std::cos(th)*r, cy + std::sin(th)*r);
    }
    glEnd();
}

static void drawDuck(const Duck& d) {
    // Body
    glColor3f(0.9f, 0.7f, 0.2f);
    drawCircle(d.x, d.y, d.r);
    // Head
    glColor3f(0.2f, 0.7f, 0.2f);
    drawCircle(d.x + (d.vx>0? d.r*0.9f : -d.r*0.9f), d.y + d.r*0.4f, d.r*0.55f);
    // Beak
    glColor3f(0.95f, 0.5f, 0.05f);
    glBegin(GL_TRIANGLES);
      float dir = (d.vx>0? 1.f : -1.f);
      glVertex2f(d.x + dir*(d.r*1.5f), d.y + d.r*0.45f);
      glVertex2f(d.x + dir*(d.r*2.1f), d.y + d.r*0.35f);
      glVertex2f(d.x + dir*(d.r*1.9f), d.y + d.r*0.55f);
    glEnd();
    // Wing
    glColor3f(0.4f, 0.3f, 0.2f);
    glBegin(GL_TRIANGLES);
      glVertex2f(d.x, d.y + d.r*0.2f);
      glVertex2f(d.x - d.r*0.8f, d.y);
      glVertex2f(d.x + d.r*0.3f, d.y - d.r*0.9f);
    glEnd();
}

static void drawText(float x, float y, const std::string& s, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (char c : s) glutBitmapCharacter(font, c);
}

static void drawHUD() {
    glColor3f(1,1,1);
    // Hint at the top
    drawText(10, winH - 20, "Press F to fullscreen");
    
    std::ostringstream ss;
    ss << "Score: " << score << "   Misses: " << misses << "   Lives: " << lives << "   Wave: " << wave;
    drawText(10, winH - 44, ss.str());
    if (paused) drawText(winW/2 - 40, winH - 44, "[PAUSED]");
}

static void drawGrass() {
    // Grass takes 40% of bottom space - single solid green shape
    float grassHeight = winH * 0.4f;
    float baseY = 0.f;  // Perfectly flat bottom at window bottom
    float topY = baseY + grassHeight;
    
    std::srand(42); // Seed for consistent pattern
    
    // Single uniform bright medium green (like the image)
    glColor3f(0.3f, 0.65f, 0.3f);
    glBegin(GL_POLYGON);
      // Perfectly flat bottom edge
      glVertex2f(0, baseY);
      glVertex2f(winW, baseY);
      
      // Create jagged, spiky top edge with irregular peaks and valleys
      float spikeFrequency = 0.15f;
      float spikeAmplitude = 20.f;
      
      // Draw jagged top from right to left
      for (float x = winW; x >= 0; x -= 1.5f) {
          // Combine multiple sine waves for irregular pattern
          float wave1 = std::sin(x * spikeFrequency) * spikeAmplitude;
          float wave2 = std::sin(x * spikeFrequency * 2.5f) * (spikeAmplitude * 0.6f);
          float wave3 = std::sin(x * spikeFrequency * 4.2f) * (spikeAmplitude * 0.3f);
          float randomVariation = frand(-10.f, 10.f);
          float spikeY = topY + wave1 + wave2 + wave3 + randomVariation;
          
          // Keep spikes within reasonable bounds
          if (spikeY < topY - 30.f) spikeY = topY - 30.f;
          if (spikeY > topY + 30.f) spikeY = topY + 30.f;
          glVertex2f(x, spikeY);
      }
    glEnd();
    
    std::srand((unsigned)std::time(nullptr)); // Reset seed
}

static void drawCrosshair() {
    glColor3f(1,1,1);
    glBegin(GL_LINES);
      glVertex2f(crossX-12, crossY); glVertex2f(crossX+12, crossY);
      glVertex2f(crossX, crossY-12); glVertex2f(crossX, crossY+12);
    glEnd();
    glBegin(GL_LINE_LOOP);
      for (int i=0;i<24;++i){
        float th = i * (2.f*3.1415926f/24.f);
        glVertex2f(crossX + std::cos(th)*14.f, crossY + std::sin(th)*14.f);
      }
    glEnd();
}

static void display() {
    glClearColor(0.22f, 0.42f, 0.85f, 1.f); // sky
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw grass at the bottom
    drawGrass();

    // Ducks
    for (const auto& d : ducks) if (d.alive) drawDuck(d);

    drawHUD();
    drawCrosshair();

    // Game over overlay
    if (gameOver) {
        glColor3f(0,0,0);
        glBegin(GL_QUADS);
          glVertex2f(0,0); glVertex2f(winW,0); glVertex2f(winW,winH); glVertex2f(0,winH);
        glEnd();
        glColor3f(1,1,1);
        drawText(winW/2 - 70, winH/2 + 10, "GAME OVER", GLUT_BITMAP_HELVETICA_18);
        std::ostringstream s; s << "Final score: " << score << "  (press R to restart)";
        drawText(winW/2 - 120, winH/2 - 14, s.str());
    }

    glutSwapBuffers();
}

static void update(int ms) {
    if (!paused && !gameOver) {
        const float dt = ms / 1000.f;
        for (auto &d : ducks) if (d.alive) {
            d.x += d.vx * dt;

            // Smooth bobbing motion
            float bobSpeed = 2.0f;
            float bobAmount = 20.0f;
            d.y += d.vy * dt + std::sin(glutGet(GLUT_ELAPSED_TIME) * 0.002f + (&d - &ducks[0])) * 0.3f;

            // Keep vertical velocity steady (so ducks float instead of falling)
            d.vy = bobAmount * std::sin(glutGet(GLUT_ELAPSED_TIME) * 0.001f);

            // bounce off side walls for fun
            if (d.x < -60 && d.vx < 0) d.vx *= -1;
            if (d.x > winW + 60 && d.vx > 0) d.vx *= -1;

            // escaped at top
            if (d.y > winH + 40) {
                d.alive = false;
                lives -= 1;
                if (lives <= 0) gameOver = true;
            }
        }

        // Remove dead ducks when all cleared -> next wave
        bool anyAlive = false;
        for (const auto &d : ducks) if (d.alive) { anyAlive = true; break; }
        if (!anyAlive) {
            wave += 1;
            spawnWave(std::min(3 + wave, 12));
        }
    }

    glutPostRedisplay();
    glutTimerFunc(ms, update, ms);
}

static void reshape(int w, int h) {
    winW = w; winH = h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void shootAt(int sx, int sy) {
    if (gameOver) return;
    bool hit = false;
    for (auto &d : ducks) if (d.alive) {
        float dx = sx - d.x;
        float dy = sy - d.y;
        float dist2 = dx*dx + dy*dy;
        float R = d.r * 1.0f; // hitbox scale
        if (dist2 <= R*R) { d.alive = false; score += 10; hit = true; }
    }
    if (!hit) misses += 1;
}

static void mouse(int button, int state, int x, int y) {
    // GLUT gives y from top; our ortho is bottom-left origin
    int oy = winH - y;
    crossX = x; crossY = oy;
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        shootAt(x, oy);
    }
}

static void motion(int x, int y) {
    crossX = x; crossY = winH - y;
}

static void toggleFullscreen() {
    isFullscreen = !isFullscreen;
    if (isFullscreen) {
        // Store original window size before going fullscreen
        origWinW = winW;
        origWinH = winH;
        glutFullScreen();
    } else {
        // Restore original window size
        glutReshapeWindow(origWinW, origWinH);
        glutPositionWindow(100, 100); // Position window on screen
    }
}

static void keyboard(unsigned char key, int, int) {
    switch (key) {
        case 27: std::exit(0); break; // ESC
        case 'f': case 'F': toggleFullscreen(); break;
        case 'p': case 'P': paused = !paused; break;
        case 'r': case 'R': resetGame(); spawnWave(4); break;
        default: break;
    }
}

int main(int argc, char** argv) {
    std::srand((unsigned)std::time(nullptr));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Duck Shooter — freeglut");
    
    // Hide the system mouse cursor - only show crosshair
    glutSetCursor(GLUT_CURSOR_NONE);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutPassiveMotionFunc(motion);
    glutKeyboardFunc(keyboard);

    reshape(winW, winH);

    resetGame();
    spawnWave(4);

    glutTimerFunc(16, update, 16); // ~60 FPS
    glutMainLoop();
    return 0;
}