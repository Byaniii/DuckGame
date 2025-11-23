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

struct Duck {
    float x, y;
    float vx, vy;
    float r;
    bool alive;
};

static int windowWidth = 960, windowHeight = 540;
static int originalWindowWidth = 960, originalWindowHeight = 540;
static bool isFullScreenMode = false;
static std::vector<Duck> duckList;
static int playerScore = 0;
static int missedShots = 0;
static int playerLives = 3;
static int currentWave = 1;
static bool isGameOver = false;
static bool isPaused = false;
static int crosshairX = windowWidth/2, crosshairY = windowHeight/2;

// ---- Scene Objects ----
static float cloudPositionOffset = 0.0f;
static float cloudMovementSpeed = 20.0f;

static float sunScaleFactor = 1.0f;
static float sunCenterX, sunCenterY;
static float sunRotationAngle = 0.0f;   

// ---- VBOs for Grass and Sun ----
static GLuint grassBufferObject = 0;
static GLuint sunDiskBufferObject = 0;
static GLuint sunRaysBufferObject = 0;
static int grassVertexTotal = 0;
static int sunDiskVertexTotal = 0;
static int sunRaysVertexTotal = 0;

// ---- VBO Initialization ----
static void initGrassVBO() {
    std::vector<float> verts;
    float grassHeight = windowHeight * 0.4f;
    float baseY = 0.f;
    float topY = baseY + grassHeight;

    verts.push_back(0); verts.push_back(baseY);
    verts.push_back(windowWidth); verts.push_back(baseY);

    float spikeFreq = 0.15f;
    float spikeAmp = 22.f;

    for (float x = windowWidth; x >= 0; x -= 1.25f) {
        float y = topY +
                  std::sin(x * spikeFreq) * spikeAmp +
                  std::sin(x * spikeFreq * 2.5f) * (spikeAmp * 0.6f);
        verts.push_back(x);
        verts.push_back(y);
    }

    grassVertexTotal = verts.size() / 2;
    glGenBuffers(1, &grassBufferObject);
    glBindBuffer(GL_ARRAY_BUFFER, grassBufferObject);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void initSunDiskVBO() {
    std::vector<float> verts;
    int seg = 64;
    verts.push_back(0); verts.push_back(0);
    float r = 90.f;

    for (int i = 0; i <= seg; i++) {
        float th = (2.f * 3.1415926f * i) / seg;
        verts.push_back(std::cos(th) * r);
        verts.push_back(std::sin(th) * r);
    }

    sunDiskVertexTotal = verts.size() / 2;
    glGenBuffers(1, &sunDiskBufferObject);
    glBindBuffer(GL_ARRAY_BUFFER, sunDiskBufferObject);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void initSunRaysVBO() {
    std::vector<float> verts;
    for (int i = 0; i < 12; i++) {
        float th = (2.f * 3.1415926f * i) / 12.f;
        float x1 = std::cos(th) * 100;
        float y1 = std::sin(th) * 100;
        float x2 = std::cos(th) * 140;
        float y2 = std::sin(th) * 140;
        verts.push_back(x1); verts.push_back(y1);
        verts.push_back(x2); verts.push_back(y2);
    }
    sunRaysVertexTotal = verts.size() / 2;
    glGenBuffers(1, &sunRaysBufferObject);
    glBindBuffer(GL_ARRAY_BUFFER, sunRaysBufferObject);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Utility random float
static float frand(float a, float b) { return a + (b - a) * (std::rand() / (float)RAND_MAX); }

// ------------------------------------------------------------
// GAME LOGIC
// ------------------------------------------------------------

static void resetGame() {
    duckList.clear();
    playerScore = 0; missedShots = 0; playerLives = 3; currentWave = 1; isGameOver = false; isPaused = false;
}

static void spawnDuck() {
    Duck d{};
    d.r = frand(14.f, 22.f);
    d.alive = true;

    bool fromLeft = std::rand() % 2;
    d.x = fromLeft ? -40.f : (windowWidth + 40.f);

    float grassTop = windowHeight * 0.4f;
    d.y = frand(grassTop * 0.2f, grassTop * 0.8f);

    float speed = frand(120.f, 210.f) * (0.6f + 0.1f * currentWave);
    float angle = frand(0.25f, 0.6f);
    d.vx = std::cos(angle) * speed * (fromLeft ? 1.f : -1.f);
    d.vy = std::sin(angle) * speed;

    duckList.push_back(d);
}

static void spawnWave(int n) {
    for (int i = 0; i < n; ++i) spawnDuck();
}


static void drawCircle(float cx, float cy, float r, int seg=64) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= seg; ++i) {
        float th = (2.f * 3.1415926f * i) / seg;
        glVertex2f(cx + std::cos(th)*r, cy + std::sin(th)*r);
    }
    glEnd();
}

// ---- Smooth Cloud ----
static void drawCloud(float cx, float cy, float s) {
    glColor3f(1,1,1);
    glPushMatrix();
    glTranslatef(cx, cy, 0);
    glScalef(s, s, 1);

    drawCircle(-25,   0, 22);
    drawCircle(  0,  12, 28);
    drawCircle( 28,  10, 22);
    drawCircle( 10, -10, 20);

    glPopMatrix();
}

// ---- Rotating Full-Circle Sun ----
static void drawSun() {
    if (sunDiskBufferObject == 0) initSunDiskVBO();
    if (sunRaysBufferObject == 0) initSunRaysVBO();

    glPushMatrix();
    glTranslatef(sunCenterX, sunCenterY, 0);
    glScalef(sunScaleFactor, sunScaleFactor, 1);
    glRotatef(sunRotationAngle, 0, 0, 1);

    glColor3f(1.0f, 0.78f, 0.0f);
    glBindBuffer(GL_ARRAY_BUFFER, sunDiskBufferObject);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, 0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, sunDiskVertexTotal);

    glBindBuffer(GL_ARRAY_BUFFER, sunRaysBufferObject);
    glVertexPointer(2, GL_FLOAT, 0, 0);
    glDrawArrays(GL_LINES, 0, sunRaysVertexTotal);

    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glPopMatrix();
}

// ---- Duck drawing ----
static void drawDuck(const Duck& d) {
    glColor3f(0.9f, 0.7f, 0.2f);
    drawCircle(d.x, d.y, d.r);

    glColor3f(0.2f, 0.7f, 0.2f);
    drawCircle(d.x + (d.vx>0? d.r*0.9f : -d.r*0.9f), d.y + d.r*0.4f, d.r*0.55f);

    glColor3f(0.95f, 0.5f, 0.05f);
    glBegin(GL_TRIANGLES);
        float dir = (d.vx>0? 1.f : -1.f);
        glVertex2f(d.x + dir*(d.r*1.5f), d.y + d.r*0.45f);
        glVertex2f(d.x + dir*(d.r*2.0f), d.y + d.r*0.35f);
        glVertex2f(d.x + dir*(d.r*1.8f), d.y + d.r*0.55f);
    glEnd();

    glColor3f(0.4f, 0.3f, 0.2f);
    glBegin(GL_TRIANGLES);
        glVertex2f(d.x, d.y + d.r*0.2f);
        glVertex2f(d.x - d.r*0.8f, d.y);
        glVertex2f(d.x + d.r*0.3f, d.y - d.r*0.9f);
    glEnd();
}

// ---- HUD ----
static void drawText(float x, float y, const std::string& s, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (char c : s) glutBitmapCharacter(font, c);
}

static void drawHUD() {
    glColor3f(1,1,1);
    drawText(10, windowHeight - 20, "Press F to fullscreen");

    std::ostringstream ss;
    ss << "Score: " << playerScore << "   Misses: " << missedShots << "   Lives: " << playerLives << "   Wave: " << currentWave;
    drawText(10, windowHeight - 44, ss.str());
}

// ---- Grass ----
static void drawGrass() {
    if (grassBufferObject == 0) initGrassVBO();
    glColor3f(0.3f, 0.65f, 0.3f);
    glBindBuffer(GL_ARRAY_BUFFER, grassBufferObject);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, 0);
    glDrawArrays(GL_POLYGON, 0, grassVertexTotal);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ------------------------------------------------------------
// DISPLAY
// ------------------------------------------------------------

static void display() {
    glClearColor(0.22f, 0.42f, 0.85f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Clouds
    glPushMatrix();
    glTranslatef(cloudPositionOffset, 0, 0);
    drawCloud(150, windowHeight - 140, 1.3f);
    drawCloud(380, windowHeight - 115, 1.1f);
    drawCloud(600, windowHeight - 135, 1.4f);
    // Additional clouds
    drawCloud(80,  windowHeight - 160, 1.1f);
    drawCloud(450, windowHeight - 180, 1.6f);
    drawCloud(780, windowHeight - 150, 1.3f);
    glPopMatrix();

    // Sun (background)
    drawSun();

    // Grass hides half of the sun
    drawGrass();

    // Ducks
    for (const auto& d : duckList)
        if (d.alive) drawDuck(d);

    drawHUD();

    // Crosshair
    glColor3f(1,1,1);
    glBegin(GL_LINES);
        glVertex2f(crosshairX-12, crosshairY); glVertex2f(crosshairX+12, crosshairY);
        glVertex2f(crosshairX, crosshairY-12); glVertex2f(crosshairX, crosshairY+12);
    glEnd();

    // Circle outline for gun crosshair
    glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 32; i++) {
            float th = i * (2.f * 3.1415926f / 32.f);
            glVertex2f(crosshairX + std::cos(th) * 14.f,
                       crosshairY + std::sin(th) * 14.f);
        }
    glEnd();

    glutSwapBuffers();
}

// ------------------------------------------------------------
// UPDATE
// ------------------------------------------------------------

static void update(int ms) {
    if (!isPaused && !isGameOver) {
        float dt = ms / 1000.f;

        cloudPositionOffset += cloudMovementSpeed * dt;
        if (cloudPositionOffset > windowWidth + 200) cloudPositionOffset = -200;

        for (auto &d : duckList) if (d.alive) {
            d.x += d.vx * dt;
            d.y += d.vy * dt;

            if (d.x < -60 && d.vx < 0) d.vx *= -1;
            if (d.x > windowWidth + 60 && d.vx > 0) d.vx *= -1;

            if (d.y > windowHeight + 40) {
                d.alive = false;
                playerLives--;
                if (playerLives <= 0) isGameOver = true;
            }
        }

        bool alive = false;
        for (auto &d : duckList) if (d.alive) alive = true;

        if (!alive) {
            currentWave++;
            spawnWave(std::min(3 + currentWave, 12));
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 16);
}

// ------------------------------------------------------------
// RESHAPE
// ------------------------------------------------------------

static void reshape(int w, int h) {
    windowWidth = w;
    windowHeight = h;

    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW); 
    glLoadIdentity();

    sunCenterX = windowWidth * 0.5f;
    sunCenterY = windowHeight * 0.4f - 20;

    // --- IMPORTANT FIX ---
    // Force VBO rebuild on resize / fullscreen toggle
    grassBufferObject = 0;
    sunDiskBufferObject = 0;
    sunRaysBufferObject = 0;
}

// ------------------------------------------------------------
// SHOOTING
// ------------------------------------------------------------

static void shootAt(int sx, int sy) {
    if (isGameOver) return;

    bool hit = false;
    for (auto &d : duckList) if (d.alive) {
        float dx = sx - d.x;
        float dy = sy - d.y;
        if (dx*dx + dy*dy <= d.r*d.r) {
            d.alive = false;
            playerScore += 10;
            hit = true;
        }
    }
    if (!hit) missedShots++;
}

// ------------------------------------------------------------
// MOUSE
// ------------------------------------------------------------

static void mouse(int button, int state, int x, int y) {
    int oy = windowHeight - y;
    crosshairX = x;
    crosshairY = oy;

    // Scroll wheel scales sun
    if (button == 3 && state == GLUT_DOWN) sunScaleFactor += 0.1f;
    if (button == 4 && state == GLUT_DOWN) {
        sunScaleFactor -= 0.1f;
        if (sunScaleFactor < 0.3f) sunScaleFactor = 0.3f;
    }

    // RIGHT CLICK ROTATES SUN
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        sunRotationAngle += 15.0f;        // rotate by 15°
        if (sunRotationAngle >= 360.0f)   // wrap angle
            sunRotationAngle -= 360.0f;
    }

    // LEFT CLICK SHOOTS
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
        shootAt(x, oy);
}

// ------------------------------------------------------------
// PASSIVE MOUSE MOTION (for crosshair tracking)
// ------------------------------------------------------------
static void motion(int x, int y) {
    crosshairX = x;
    crosshairY = windowHeight - y;
}
// ------------------------------------------------------------
// KEYBOARD
// ------------------------------------------------------------

static void toggleFullscreen() {
    isFullScreenMode = !isFullScreenMode;
    if (isFullScreenMode) {
        originalWindowWidth = windowWidth;
        originalWindowHeight = windowHeight;
        glutFullScreen();
    } else {
        glutReshapeWindow(originalWindowWidth, originalWindowHeight);
        glutPositionWindow(100, 100);
    }
}

static void keyboard(unsigned char key, int, int) {
    switch (key) {
        case 27: exit(0); break;
        case 'f': case 'F': toggleFullscreen(); break;
        case 'p': case 'P': isPaused = !isPaused; break;
        case 'r': case 'R': resetGame(); spawnWave(4); break;
        case 'w':
        case 'W':
            sunScaleFactor += 0.1f;
            break;
        case 's':
        case 'S':
            sunScaleFactor -= 0.1f;
            if (sunScaleFactor < 0.3f) sunScaleFactor = 0.3f;
            break;
        default:
            break;
    }
}

// GLUT special keys handler for UP/DOWN arrows
static void specialKeys(int key, int x, int y) {
    if (key == GLUT_KEY_UP) {
        sunScaleFactor += 0.1f;
    }
    if (key == GLUT_KEY_DOWN) {
        sunScaleFactor -= 0.1f;
        if (sunScaleFactor < 0.3f) sunScaleFactor = 0.3f;
    }
}

// ------------------------------------------------------------
// MAIN
// ------------------------------------------------------------

int main(int argc, char** argv) {
    std::srand((unsigned)std::time(nullptr));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Duck Shooter — freeglut");
    glutSetCursor(GLUT_CURSOR_NONE);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutPassiveMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);

    reshape(windowWidth, windowHeight);
    resetGame();
    spawnWave(4);

    glutTimerFunc(16, update, 16);
    glutMainLoop();

    return 0;
}