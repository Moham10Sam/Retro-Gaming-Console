/*
  Arduino Uno Retro Console (4 Games)
  Hardware: 128x64 I2C OLED, Analog Joystick (with click), 2 Buttons
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

// ------------------- PINS -------------------
#define PIN_JOY_X   A1
#define PIN_JOY_Y   A0
#define PIN_JOY_BTN 2
#define PIN_BTN_A   3
#define PIN_BTN_B   4

// ------------------- INPUT -------------------
struct Input {
  int jx, jy;
  bool btnA, btnB, btnJ;
  bool btnA_pressed, btnB_pressed, btnJ_pressed;
};
Input in;

// ------------------- SYSTEM -------------------
enum SysMode { MODE_MENU, MODE_INGAME, MODE_GAMEOVER };
SysMode sysMode = MODE_MENU;

typedef struct {
  const char* name;
  void (*init)();
  void (*loop)();
  void (*input)(const Input&);
} Game;

void snake_init();  void snake_loop();  void snake_input(const Input&);
void pong_init();   void pong_loop();   void pong_input(const Input&);
void breakout_init(); void breakout_loop(); void breakout_input(const Input&);
void spaceinvaders_init(); void spaceinvaders_loop(); void spaceinvaders_input(const Input&);

Game games[] = {
  {"Snake",         snake_init,  snake_loop,  snake_input},
  {"Pong",          pong_init,   pong_loop,   pong_input},
  {"Breakout",      breakout_init, breakout_loop, breakout_input},
  {"Space Invaders",spaceinvaders_init, spaceinvaders_loop, spaceinvaders_input},
};
const uint8_t NUM_GAMES = sizeof(games) / sizeof(Game);

uint8_t selectedGame = 0;
uint8_t activeGame = 0;
unsigned long lastScrollTime = 0;

// ------------------- CALIBRATION -------------------
int jxCenter = 512;
int jyCenter = 512;
const int DEADZONE = 40;

// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_JOY_BTN, INPUT_PULLUP);
  pinMode(PIN_BTN_A,   INPUT_PULLUP);
  pinMode(PIN_BTN_B,   INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  randomSeed(analogRead(A3));

  long sumX = 0, sumY = 0;
  const uint8_t SAMPLES = 20;
  for (uint8_t i = 0; i < SAMPLES; i++) {
    sumX += analogRead(PIN_JOY_X);
    sumY += analogRead(PIN_JOY_Y);
    delay(5);
  }
  jxCenter = sumX / SAMPLES;
  jyCenter = sumY / SAMPLES;

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED FAIL - LOW MEMORY"));
    while (1) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(100);
    }
  }
  Serial.println(F("OLED OK"));
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

// ------------------- MAIN LOOP -------------------
void loop() {
  readInputs();
  switch (sysMode) {
    case MODE_MENU:     runMenu();     break;
    case MODE_INGAME:   runInGame();   break;
    case MODE_GAMEOVER: runGameOver(); break;
  }
  delay(16);
}

// ------------------- INPUT HANDLER -------------------
void readInputs() {
  int rawX = analogRead(PIN_JOY_X) - jxCenter;
  int rawY = analogRead(PIN_JOY_Y) - jyCenter;

  in.jx = (abs(rawX) < DEADZONE) ? 0 : rawX;
  in.jy = (abs(rawY) < DEADZONE) ? 0 : -rawY;

  bool rawA = digitalRead(PIN_BTN_A) == LOW;
  bool rawB = digitalRead(PIN_BTN_B) == LOW;
  bool rawJ = digitalRead(PIN_JOY_BTN) == LOW;

  in.btnA_pressed = (rawA && !in.btnA);
  in.btnB_pressed = (rawB && !in.btnB);
  in.btnJ_pressed = (rawJ && !in.btnJ);

  in.btnA = rawA;
  in.btnB = rawB;
  in.btnJ = rawJ;
}

// ------------------- MENU -------------------
void runMenu() {
  if (millis() - lastScrollTime > 150) {
    if (in.jy > 200) { selectedGame = (selectedGame + 1) % NUM_GAMES; lastScrollTime = millis(); }
    if (in.jy < -200) { selectedGame = (selectedGame + NUM_GAMES - 1) % NUM_GAMES; lastScrollTime = millis(); }
  }

  display.clearDisplay();
  display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(22, 2);
  display.print(F("RETRO CONSOLE"));

  display.setTextColor(SSD1306_WHITE);
  for (uint8_t i = 0; i < NUM_GAMES; i++) {
    uint8_t y = 14 + i * 11;
    if (y > 54) break;
    if (i == selectedGame) {
      display.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(8, y);
      display.print(F("> "));
    } else {
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(8, y);
      display.print(F("  "));
    }
    display.print(games[i].name);
  }

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 56);
  display.print(F("A:Play   J:Menu"));
  display.display();

  if (in.btnA_pressed) {
    activeGame = selectedGame;
    sysMode = MODE_INGAME;
    games[activeGame].init();
  }
}

// ------------------- IN-GAME -------------------
void runInGame() {
  if (in.btnJ_pressed) { sysMode = MODE_MENU; return; }
  games[activeGame].input(in);
  games[activeGame].loop();
}

// ------------------- GAME OVER -------------------
void runGameOver() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(16, 18);
  display.print(F("GAME OVER"));
  display.setTextSize(1);
  display.setCursor(8, 44);
  display.print(F("A:Restart  J:Menu"));
  display.display();

  if (in.btnA_pressed) {
    sysMode = MODE_INGAME;
    games[activeGame].init();
  }
  if (in.btnJ_pressed) {
    sysMode = MODE_MENU;
  }
}

// ------------------- HELPER -------------------
void setGameOver() { sysMode = MODE_GAMEOVER; }

// ============================================================
//  GAME 1: SNAKE (REDUCED TO 40 SEGMENTS TO SAVE RAM)
// ============================================================
#define SNAKE_MAX 40
uint8_t sx[SNAKE_MAX], sy[SNAKE_MAX];
uint8_t s_len;
int8_t s_dir;
uint8_t s_fx, s_fy;
uint16_t s_delay;
unsigned long s_timer;
bool s_over;

void snake_init() {
  s_len = 3;
  sx[0] = 64; sy[0] = 32;
  sx[1] = 62; sy[1] = 32;
  sx[2] = 60; sy[2] = 32;
  s_dir = 1;
  s_delay = 140;
  s_timer = millis();
  s_over = false;
  s_fx = random(4, 124);
  s_fy = random(4, 60);
}

void snake_input(const Input& i) {
  if (i.jy < -200 && s_dir != 2) s_dir = 0;
  if (i.jx > 200  && s_dir != 3) s_dir = 1;
  if (i.jy > 200  && s_dir != 0) s_dir = 2;
  if (i.jx < -200 && s_dir != 1) s_dir = 3;
}

void snake_loop() {
  if (s_over) { setGameOver(); return; }

  uint16_t d = s_delay;
  if (in.btnA) d = d / 2;

  if (millis() - s_timer < d) {
    display.clearDisplay();
    display.fillRect(s_fx-1, s_fy-1, 3, 3, SSD1306_WHITE);
    for (uint8_t i = 0; i < s_len; i++)
      display.fillRect(sx[i]-1, sy[i]-1, 3, 3, SSD1306_WHITE);
    display.display();
    return;
  }
  s_timer = millis();

  for (uint8_t i = s_len; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
  if (s_dir == 0) sy[0]--;
  if (s_dir == 1) sx[0]++;
  if (s_dir == 2) sy[0]++;
  if (s_dir == 3) sx[0]--;

  if (sx[0] < 2 || sx[0] > 125 || sy[0] < 2 || sy[0] > 61) { s_over = true; return; }
  for (uint8_t i = 1; i < s_len; i++)
    if (sx[0] == sx[i] && sy[0] == sy[i]) { s_over = true; return; }

  if (abs((int16_t)sx[0] - (int16_t)s_fx) < 4 && abs((int16_t)sy[0] - (int16_t)s_fy) < 4) {
    if (s_len < SNAKE_MAX) s_len++;
    s_delay = s_delay * 95 / 100;
    if (s_delay < 45) s_delay = 45;
    s_fx = random(4, 124); s_fy = random(4, 60);
  }

  display.clearDisplay();
  display.fillRect(s_fx-1, s_fy-1, 3, 3, SSD1306_WHITE);
  for (uint8_t i = 0; i < s_len; i++)
    display.fillRect(sx[i]-1, sy[i]-1, 3, 3, SSD1306_WHITE);
  display.display();
}

// ============================================================
//  GAME 2: PONG
// ============================================================
float pg_bx, pg_by, pg_dx, pg_dy;
float pg_px, pg_aix;
uint8_t pg_pw = 22;
uint8_t pg_br = 2;
bool pg_wait;
bool pg_over;

void pong_init() {
  pg_bx = 64; pg_by = 32;
  pg_px = 53; pg_aix = 53;
  pg_wait = true;
  pg_over = false;
}

void pong_input(const Input& i) {
  pg_px += i.jx / 220.0;
  if (pg_px < 0) pg_px = 0;
  if (pg_px > 128 - pg_pw) pg_px = 128 - pg_pw;
  if (in.btnA_pressed && pg_wait) {
    pg_wait = false;
    pg_dx = (random(0, 2) == 0 ? 1.2 : -1.2);
    pg_dy = -1.6;
  }
}

void pong_loop() {
  if (pg_over) { setGameOver(); return; }

  float target = pg_bx - pg_pw / 2.0;
  pg_aix += (target - pg_aix) * 0.12;
  if (pg_aix < 0) pg_aix = 0;
  if (pg_aix > 128 - pg_pw) pg_aix = 128 - pg_pw;

  if (!pg_wait) {
    pg_bx += pg_dx; pg_by += pg_dy;
    if (pg_bx <= pg_br) { pg_bx = pg_br; pg_dx = abs(pg_dx); }
    if (pg_bx >= 128 - pg_br) { pg_bx = 128 - pg_br; pg_dx = -abs(pg_dx); }

    if (pg_by <= 6 && pg_dy < 0)
      if (pg_bx >= pg_aix && pg_bx <= pg_aix + pg_pw) pg_dy = abs(pg_dy);

    if (pg_by >= 58 - pg_br && pg_dy > 0)
      if (pg_bx >= pg_px && pg_bx <= pg_px + pg_pw) {
        pg_dy = -abs(pg_dy);
        pg_dx += (pg_bx - (pg_px + pg_pw/2.0)) * 0.04;
        if (pg_dx > 2.8) pg_dx = 2.8;
        if (pg_dx < -2.8) pg_dx = -2.8;
      }

    if (pg_by > 64) { pg_over = true; return; }
    if (pg_by < 0) { pg_wait = true; pg_bx = 64; pg_by = 32; }
  } else {
    pg_bx = pg_px + pg_pw / 2;
    pg_by = 57;
  }

  display.clearDisplay();
  display.fillRect((int8_t)pg_aix, 2, pg_pw, 2, SSD1306_WHITE);
  display.fillRect((int8_t)pg_px, 58, pg_pw, 2, SSD1306_WHITE);
  display.fillCircle((int8_t)pg_bx, (int8_t)pg_by, pg_br, SSD1306_WHITE);
  for (uint8_t i = 0; i < 64; i += 4) display.drawPixel(64, i, SSD1306_WHITE);
  display.display();
}

// ============================================================
//  GAME 3: BREAKOUT
// ============================================================
float br_bx, br_by, br_dx, br_dy;
float br_px;
uint8_t br_pw = 22;
uint8_t br_ph = 3;
uint8_t br_py = 58;
uint8_t br_br = 2;
float br_spd = 1.0;
unsigned long br_spdTimer;
bool br_wait;
bool br_over;

void breakout_init() {
  br_bx = 64; br_by = 45;
  br_px = 53;
  br_dx = 1.3; br_dy = -1.5;
  br_spd = 1.0;
  br_spdTimer = millis();
  br_wait = true;
  br_over = false;
}

void breakout_input(const Input& i) {
  br_px += i.jx / 200.0;
  if (br_px < 0) br_px = 0;
  if (br_px > 128 - br_pw) br_px = 128 - br_pw;
  if (in.btnA_pressed && br_wait) {
    br_wait = false;
    br_dy = -1.5;
  }
}

void breakout_loop() {
  if (br_over) { setGameOver(); return; }

  if (!br_wait) {
    if (millis() - br_spdTimer > 5000) {
      br_spd *= 1.06;
      if (br_spd > 2.5) br_spd = 2.5;
      br_spdTimer = millis();
    }
    br_bx += br_dx * br_spd;
    br_by += br_dy * br_spd;

    if (br_bx <= br_br) { br_bx = br_br; br_dx = abs(br_dx); }
    if (br_bx >= 128 - br_br) { br_bx = 128 - br_br; br_dx = -abs(br_dx); }
    if (br_by <= br_br) { br_by = br_br; br_dy = abs(br_dy); }

    if (br_dy > 0) {
      if (br_by + br_br >= br_py && br_by - br_br <= br_py + br_ph) {
        if (br_bx + br_br >= br_px && br_bx - br_br <= br_px + br_pw) {
          br_dy = -abs(br_dy);
          float hit = (br_bx - (br_px + br_pw/2.0)) / (br_pw/2.0);
          br_dx += hit * 0.6;
          if (br_dx > 2.5) br_dx = 2.5;
          if (br_dx < -2.5) br_dx = -2.5;
        }
      }
    }
    if (br_by > 64 + br_br) { br_over = true; return; }
  } else {
    br_bx = br_px + br_pw / 2;
    br_by = br_py - br_br - 1;
  }

  display.clearDisplay();
  display.fillCircle((int8_t)br_bx, (int8_t)br_by, br_br, SSD1306_WHITE);
  display.fillRect((int8_t)br_px, br_py, br_pw, br_ph, SSD1306_WHITE);
  display.display();
}

// ============================================================
//  GAME 4: SPACE INVADERS (OPTIMIZED RAM)
// ============================================================
#define SI_COLS       6
#define SI_ROWS       3
#define SI_ALIEN_W    8
#define SI_ALIEN_H    6
#define SI_SPACING_X  14
#define SI_SPACING_Y  10

static uint8_t  si_playerX;
static bool     si_bulletActive;
static uint8_t  si_bulletX, si_bulletY;
static uint8_t  si_alienAlive[SI_COLS * SI_ROWS];
static uint8_t  si_alienX, si_alienY;
static int8_t   si_alienDir;
static uint8_t  si_aliensLeft;
static uint16_t si_alienInterval;
static unsigned long si_alienTimer;
static uint8_t  si_animFrame;
static uint8_t  si_bombX[2];
static uint8_t  si_bombY[2];
static bool     si_bombActive[2];
static unsigned long si_bombTimer;
static uint8_t  si_bunkerHealth[3];
static bool     si_ufoActive;
static int8_t   si_ufoX;
static unsigned long si_ufoNext;
static uint16_t si_score;
static bool     si_playerDead;

void spaceinvaders_init() {
  si_playerX = 60;
  si_bulletActive = false;
  for (uint8_t i = 0; i < SI_COLS * SI_ROWS; i++) si_alienAlive[i] = 1;
  si_alienX = 10;
  si_alienY = 8;
  si_alienDir = 1;
  si_aliensLeft = SI_COLS * SI_ROWS;
  si_alienInterval = 700;
  si_alienTimer = millis();
  si_animFrame = 0;
  si_bombActive[0] = si_bombActive[1] = false;
  si_bombTimer = millis();
  si_bunkerHealth[0] = si_bunkerHealth[1] = si_bunkerHealth[2] = 3;
  si_ufoActive = false;
  si_ufoNext = millis() + random(4000, 12000);
  si_score = 0;
  si_playerDead = false;
}

void spaceinvaders_input(const Input& i) {
  if (si_playerDead) return;
  if (i.jx < 0 && si_playerX > 0)       si_playerX -= 2;
  if (i.jx > 0 && si_playerX < 120)     si_playerX += 2;
  if (i.btnA_pressed && !si_bulletActive) {
    si_bulletActive = true;
    si_bulletX = si_playerX + 3;
    si_bulletY = 54;
  }
}

void spaceinvaders_loop() {
  if (si_playerDead) { setGameOver(); return; }
  unsigned long now = millis();

  // ---- Move aliens ----
  if (now - si_alienTimer > si_alienInterval) {
    si_alienTimer = now;
    si_animFrame ^= 1;

    uint8_t formationRight = 0;
    uint8_t formationLeft  = 127;
    uint8_t formationBottom = 0;

    for (uint8_t i = 0; i < SI_COLS * SI_ROWS; i++) {
      if (!si_alienAlive[i]) continue;
      uint8_t col = i % SI_COLS;
      uint8_t row = i / SI_COLS;
      uint8_t ax = si_alienX + col * SI_SPACING_X;
      uint8_t ay = si_alienY + row * SI_SPACING_Y;
      if (ax < formationLeft)  formationLeft = ax;
      if (ax + SI_ALIEN_W > formationRight) formationRight = ax + SI_ALIEN_W;
      if (ay + SI_ALIEN_H > formationBottom) formationBottom = ay + SI_ALIEN_H;
    }

    bool drop = false;
    if (si_alienDir > 0 && formationRight >= 124) drop = true;
    else if (si_alienDir < 0 && formationLeft <= 2) drop = true;

    if (drop) {
      si_alienDir = -si_alienDir;
      si_alienY += 4;
      if (formationBottom + 4 >= 56) { si_playerDead = true; return; }
    } else {
      si_alienX += si_alienDir * 2;
    }
  }

  // ---- Alien bombs ----
  if (now - si_bombTimer > 600) {
    si_bombTimer = now;
    for (uint8_t b = 0; b < 2; b++) {
      if (si_bombActive[b]) continue;
      uint8_t col = random(0, SI_COLS);
      int8_t targetRow = -1;
      for (int8_t row = SI_ROWS - 1; row >= 0; row--) {
        if (si_alienAlive[row * SI_COLS + col]) { targetRow = row; break; }
      }
      if (targetRow >= 0) {
        si_bombX[b] = si_alienX + col * SI_SPACING_X + 4;
        si_bombY[b] = si_alienY + targetRow * SI_SPACING_Y + SI_ALIEN_H;
        si_bombActive[b] = true;
        break;
      }
    }
  }

  // ---- Move projectiles ----
  if (si_bulletActive) {
    si_bulletY -= 3;
    if (si_bulletY < 2) si_bulletActive = false;
  }
  for (uint8_t b = 0; b < 2; b++) {
    if (si_bombActive[b]) {
      si_bombY[b] += 2;
      if (si_bombY[b] > 64) si_bombActive[b] = false;
    }
  }

  // ---- UFO ----
  if (!si_ufoActive && now > si_ufoNext) {
    si_ufoActive = true;
    si_ufoX = 128;
  }
  if (si_ufoActive) {
    si_ufoX -= 1;
    if (si_ufoX < -12) {
      si_ufoActive = false;
      si_ufoNext = now + random(8000, 20000);
    }
  }

  // ---- Collisions: player bullet vs aliens ----
  if (si_bulletActive) {
    for (uint8_t i = 0; i < SI_COLS * SI_ROWS; i++) {
      if (!si_alienAlive[i]) continue;
      uint8_t col = i % SI_COLS;
      uint8_t row = i / SI_COLS;
      uint8_t ax = si_alienX + col * SI_SPACING_X;
      uint8_t ay = si_alienY + row * SI_SPACING_Y;
      if (si_bulletX >= ax && si_bulletX <= ax + SI_ALIEN_W &&
          si_bulletY >= ay && si_bulletY <= ay + SI_ALIEN_H) {
        si_alienAlive[i] = 0;
        si_aliensLeft--;
        si_bulletActive = false;
        si_score += 10;
        if (si_alienInterval > 120) si_alienInterval -= 20;
        break;
      }
    }
  }

  // ---- Bullet vs UFO ----
  if (si_bulletActive && si_ufoActive) {
    if (si_bulletX >= si_ufoX && si_bulletX <= si_ufoX + 10 &&
        si_bulletY <= 6) {
      si_ufoActive = false;
      si_ufoNext = now + random(8000, 20000);
      si_bulletActive = false;
      si_score += 50;
    }
  }

  // ---- Bullet vs bunkers ----
  if (si_bulletActive) {
    for (uint8_t b = 0; b < 3; b++) {
      if (si_bunkerHealth[b] == 0) continue;
      uint8_t bx = 18 + b * 38;
      if (si_bulletX >= bx && si_bulletX <= bx + 12 &&
          si_bulletY >= 46 && si_bulletY <= 50) {
        si_bunkerHealth[b]--;
        si_bulletActive = false;
        break;
      }
    }
  }

  // ---- Bombs vs player ----
  for (uint8_t b = 0; b < 2; b++) {
    if (!si_bombActive[b]) continue;
    if (si_bombX[b] >= si_playerX && si_bombX[b] <= si_playerX + 8 &&
        si_bombY[b] >= 56 && si_bombY[b] <= 62) {
      si_playerDead = true; return;
    }
  }

  // ---- Bombs vs bunkers ----
  for (uint8_t b = 0; b < 2; b++) {
    if (!si_bombActive[b]) continue;
    for (uint8_t k = 0; k < 3; k++) {
      if (si_bunkerHealth[k] == 0) continue;
      uint8_t bx = 18 + k * 38;
      if (si_bombX[b] >= bx && si_bombX[b] <= bx + 12 &&
          si_bombY[b] >= 46 && si_bombY[b] <= 50) {
        si_bunkerHealth[k]--;
        si_bombActive[b] = false;
        break;
      }
    }
  }

  // ---- Bombs vs bullet (cancel) ----
  if (si_bulletActive) {
    for (uint8_t b = 0; b < 2; b++) {
      if (!si_bombActive[b]) continue;
      if (si_bulletX >= si_bombX[b] - 1 && si_bulletX <= si_bombX[b] + 1 &&
          abs((int8_t)si_bulletY - (int8_t)si_bombY[b]) < 4) {
        si_bulletActive = false;
        si_bombActive[b] = false;
        break;
      }
    }
  }

  // ---- Wave clear ----
  if (si_aliensLeft == 0) {
    for (uint8_t i = 0; i < SI_COLS * SI_ROWS; i++) si_alienAlive[i] = 1;
    si_aliensLeft = SI_COLS * SI_ROWS;
    si_alienX = 10;
    si_alienY = 8;
    si_alienDir = 1;
    if (si_alienInterval > 120) si_alienInterval -= 60;
    si_bombActive[0] = si_bombActive[1] = false;
    si_bulletActive = false;
  }

  // ==================== DRAW ====================
  display.clearDisplay();

  // Score
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("SC:"));
  display.print(si_score);

  // UFO
  if (si_ufoActive) {
    display.fillRect(si_ufoX, 2, 10, 3, SSD1306_WHITE);
    display.drawPixel(si_ufoX + 2, 1, SSD1306_WHITE);
    display.drawPixel(si_ufoX + 7, 1, SSD1306_WHITE);
  }

  // Aliens
  for (uint8_t i = 0; i < SI_COLS * SI_ROWS; i++) {
    if (!si_alienAlive[i]) continue;
    uint8_t col = i % SI_COLS;
    uint8_t row = i / SI_COLS;
    uint8_t ax = si_alienX + col * SI_SPACING_X;
    uint8_t ay = si_alienY + row * SI_SPACING_Y;

    display.fillRect(ax + 2, ay, 4, 2, SSD1306_WHITE);
    display.drawPixel(ax + 1, ay + 1, SSD1306_WHITE);
    display.drawPixel(ax + 6, ay + 1, SSD1306_WHITE);

    if (si_animFrame == 0) {
      display.drawPixel(ax + 1, ay + 2, SSD1306_WHITE);
      display.drawPixel(ax + 6, ay + 2, SSD1306_WHITE);
      display.drawPixel(ax + 2, ay + 3, SSD1306_WHITE);
      display.drawPixel(ax + 5, ay + 3, SSD1306_WHITE);
    } else {
      display.drawPixel(ax + 2, ay + 2, SSD1306_WHITE);
      display.drawPixel(ax + 5, ay + 2, SSD1306_WHITE);
      display.drawPixel(ax + 1, ay + 3, SSD1306_WHITE);
      display.drawPixel(ax + 6, ay + 3, SSD1306_WHITE);
    }
  }

  // Bunkers
  for (uint8_t b = 0; b < 3; b++) {
    if (si_bunkerHealth[b] == 0) continue;
    uint8_t bx = 18 + b * 38;
    uint8_t bw = si_bunkerHealth[b] * 4;
    display.fillRect(bx + (12 - bw) / 2, 46, bw, 4, SSD1306_WHITE);
  }

  // Player bullet
  if (si_bulletActive)
    display.drawLine(si_bulletX, si_bulletY, si_bulletX, si_bulletY - 2, SSD1306_WHITE);

  // Alien bombs
  for (uint8_t b = 0; b < 2; b++)
    if (si_bombActive[b])
      display.drawLine(si_bombX[b], si_bombY[b], si_bombX[b], si_bombY[b] + 1, SSD1306_WHITE);

  // Player ship
  display.fillRect(si_playerX, 58, 8, 2, SSD1306_WHITE);
  display.drawLine(si_playerX + 2, 56, si_playerX + 5, 56, SSD1306_WHITE);
  display.drawPixel(si_playerX + 3, 55, SSD1306_WHITE);

  display.display();
}