#include <math.h>
#include <Arduboy2.h>
#include <ArduboyTones.h>

#define FRAME_RATE 60
#define CX(x) (WIDTH / 2 + x)
#define CY(y) (HEIGHT / 2 + y)
#define RX (WIDTH/2 - 3)
#define RY (HEIGHT/2 - 3)
#define SMALL_R 4
#define DEG_360 6.283185307179586
#define DEG_30 0.5235987755982988
#define DEG_025 0.004363323129985824
#define MIN_DELTA 0.025
#define MAX_DELTA 0.2
#define MAX_BULLETS 20
#define MAX_ENEMIES 10
#define MIN_RESET (FRAME_RATE / 0.75)
#define INIT_RESET (FRAME_RATE / 0.225)
#define TICK (1.0/FRAME_RATE)

Arduboy2 arduboy;
ArduboyTones sound(arduboy.audio.enabled);

// game begins in 'reset' state, quickly enters 'playing', 'died' saves the high
// score, and gameOver just shows the "GAME OVER" text and waits for a button
// press.
enum State : uint8_t {
  State_playing,
  State_died,
  State_gameOver,
  State_reset
};

// stores a few game-related globals
struct Game {
  int diedCounter;
  float rotation;
  uint8_t state;
  int enemyCountdown;
  int enemyReset;
};

// player location (both absolute and as an angle), score, rate of "spinning"
struct Player {
  uint16_t score;
  uint16_t highScore;
  float deg;
  float degDelta;
  int x0;
  int y0;
  int xa;
  int ya;
  int xb;
  int yb;
};

// "active" bullets are drawn until they hit an enemy or reach the center.
// other info is bullet location, rotation, and distance along the path.
struct Bullet {
  bool isActive;
  float deg;
  float r;
  int x, y;
};

// very similar to Bullet, but a typedef just felt wrong!
struct Enemy {
  bool isActive;
  float deg;
  float r;
  int x, y;
};

Game game;
Player player;

Bullet bullets[MAX_BULLETS];

Enemy enemies[MAX_ENEMIES];

char text[10];

// arduboy init code and I left the fun arduboy additions like flashlight
// and systemButtons.  reset the game state, and check EEPROM for a high score.
void setup() {
  arduboy.boot();
  arduboy.blank();
  arduboy.setFrameRate(FRAME_RATE);

  // UP button -> light up screen
  arduboy.flashlight();

  // system control:
  // B_BUTTON -> blue LED
  // B_BUTTON + UP -> green LED, turn sound ON
  // B_BUTTON + DOWN -> red LED, turn sound OFF
  arduboy.systemButtons();
  arduboy.audio.begin();

  if ( checkEEPROM() ) {
    player.highScore = readEEPROM();
  }
  else {
    player.highScore = 0;
  }
  game.rotation = 0;

  reset();
  setPos();
  arduboy.initRandomSeed();
}

void loop() {
  // arduboy framerate handling
  if (!arduboy.nextFrame()) {
    return;
  }
  arduboy.pollButtons();
  // /arduboy

  arduboy.clear();
  mainLoop();
  arduboy.display();
}

void mainLoop() {
  // each game state calls out to one helper. died and reset both immediately
  // change the game state to another state
  if ( game.state == State_playing ) {
    gameLoop();
  }
  else if ( game.state == State_died ){
    died();
  }
  else if ( game.state == State_gameOver ){
    gameOverLoop();
  }
  else {
    reset();
  }
}

// reset the enemy generation timers, player score, enemies and bullets.
void reset() {
  game.enemyCountdown = MIN_RESET;
  game.enemyReset = INIT_RESET;
  game.state = State_playing;

  player.score = 0;
  player.degDelta = MIN_DELTA;

  for ( int i = 0; i < MAX_BULLETS ; i++ ) {
    bullets[i].isActive = false;
  }

  for ( int i = 0 ; i < MAX_ENEMIES ; i++ ) {
    enemies[i].isActive = false;
  }
}

// I broke this up into smaller helpers, but this is the bulk of the game here
void gameLoop() {
  drawPlayer();

  checkButtons();
  generateEnemy();
  drawBullets(true);
  drawEnemies(true);
  checkCollisions();
}

// the diedCounter prevents button presses from restarting for a couple seconds,
// and records the high score
void died() {
  game.diedCounter = (int)(FRAME_RATE / 0.75);
  game.state = State_gameOver;

  if ( player.score > player.highScore ) {
    player.highScore = player.score;
    writeEEPROM(player.score);
  }
}

// show GAME OVER and once the diedCounter reaches zero, allow a button press to
// start a new game
void gameOverLoop() {
  drawPlayer();
  drawBullets(false);
  drawEnemies(false);

  arduboy.setCursor(37, 28);
  arduboy.print(F("GAME OVER"));

  if ( game.diedCounter <= 0 && arduboy.buttonsState() ) {
    game.state = State_reset;
  }
  else if ( game.diedCounter > 0 ) {
    game.diedCounter -= 1;
  }
}

// this check is a way to have some confidence that the EEPROM was under this
// game's control last time it was accessed.
bool checkEEPROM() {
  if ( EEPROM.read(0) != 'T' ) return false;
  if ( EEPROM.read(1) != 'M' ) return false;
  if ( EEPROM.read(2) != 'P' ) return false;
  if ( EEPROM.read(3) != 'S' ) return false;
  if ( EEPROM.read(4) != 'T' ) return false;
  return true;
}

// the score is 16 bits (0..65536), so read out two bytes and add em
uint16_t readEEPROM() {
  uint8_t low = EEPROM.read(5);
  uint8_t high = EEPROM.read(6);
  return (uint16_t)low + (uint16_t)(high << 8);
}

// break score up into two bytes and save to EEPROM
void writeEEPROM(uint16_t score) {
  uint8_t low = score;
  uint8_t high = score >> 8;
  EEPROM.write(0, 'T');
  EEPROM.write(1, 'M');
  EEPROM.write(2, 'P');
  EEPROM.write(3, 'S');
  EEPROM.write(4, 'T');
  EEPROM.write(5, low);
  EEPROM.write(6, high);
}

// LEFT and RIGHT move the ship, and there's a "speedup" effect, see updatePos
// for details on that.  B button fires.
void checkButtons() {
  if ( arduboy.pressed(LEFT_BUTTON) ) {
    updatePos(-1);
  }
  else if ( arduboy.pressed(RIGHT_BUTTON) ) {
    updatePos(1);
  }
  else {
    player.degDelta = MIN_DELTA;
  }

  if ( arduboy.justPressed(B_BUTTON) ) {
    fire();
  }
}

// memoize the player position based on current angle.  This is a way to avoid
// the sin/cos calculations *every* game loop.
void setPos() {
  player.x0 = 0.5 + RX * cos(player.deg);
  player.y0 = 0.5 + RY * sin(player.deg);
  player.xa = player.x0 + SMALL_R * cos(player.deg + DEG_30);
  player.ya = player.y0 + SMALL_R * sin(player.deg + DEG_30);
  player.xb = player.x0 + SMALL_R * cos(player.deg - DEG_30);
  player.yb = player.y0 + SMALL_R * sin(player.deg - DEG_30);
}

// if the player is spinning, `degDelta` is increased slightly so that the
// movement accelerates the longer you hold down LEFT/RIGHT (up to a maximum)
void updatePos(float delta) {
  player.deg += delta * player.degDelta;
  player.degDelta = min(MAX_DELTA, player.degDelta + 0.005);

  setPos();
}

// find an inactive bullet and activate it
bool fire() {
  uint8_t bulletIndex = MAX_BULLETS;

  for ( int i = 0; i < MAX_BULLETS ; i++ ) {
    if ( !bullets[i].isActive ) {
      bulletIndex = i;
      break;
    }
  }

  if ( bulletIndex == MAX_BULLETS ) {
    return false;
  }

  bullets[bulletIndex].deg = player.deg;
  bullets[bulletIndex].r = 1;
  bullets[bulletIndex].isActive = true;
  sound.tone(1000, 50, 500, 50);
  return true;
}

// when the enemyCountdown reaches 0 generate an enemy and reset the timer. the
// countdown decreases, so that enemies are generated faster and faster.
void generateEnemy() {
  if ( --game.enemyCountdown == 0 ) {
    createEnemy();
    game.enemyCountdown = game.enemyReset;
    game.enemyReset = max(MIN_RESET, game.enemyReset - 10);
  }
}

// find an inactive enemy and activate it, very similar to bullets
bool createEnemy() {
  uint8_t enemyIndex = MAX_ENEMIES;

  for ( int i = 0; i < MAX_ENEMIES ; i++ ) {
    if ( !enemies[i].isActive ) {
      enemyIndex = i;
      break;
    }
  }

  if ( enemyIndex == MAX_ENEMIES ) {
    return false;
  }

  enemies[enemyIndex].deg = (float)random(360) * 0.017453292519943295;
  enemies[enemyIndex].r = 0;
  enemies[enemyIndex].isActive = true;
  return true;
}

// this draws the player *and* the game board, including the spinning lines.
void drawPlayer() {
  arduboy.drawLine(CX(player.x0), CY(player.y0), CX(player.xa), CY(player.ya), 1);
  arduboy.drawLine(CX(player.x0), CY(player.y0), CX(player.xb), CY(player.yb), 1);
  arduboy.drawCircle(CX(0), CY(0), SMALL_R, 1);

  int x0;
  int y0;
  int x1;
  int y1;
  for ( float deg = game.rotation ; deg < DEG_360 ; deg += DEG_30 ) {
    int x0 = CX(0.5 + RX * cos(deg));
    int y0 = CY(0.5 + RY * sin(deg));
    int x1 = CX(0.5 + SMALL_R * cos(deg));
    int y1 = CY(0.5 + SMALL_R * sin(deg));
    arduboy.drawLine(x0, y0, x1, y1, 1);
  }
  game.rotation = (game.rotation + DEG_025);
  while ( game.rotation > DEG_30 ) {
    game.rotation -= DEG_30;
  }

  arduboy.setCursor(0, 54);
  arduboy.print(player.score);
  if ( player.highScore > 0 ) {
    sprintf(text, "%u", player.highScore);
    uint8_t len = strlen(text);
    arduboy.setCursor(128 - 6 * len, 54);
    arduboy.print(text);
  }
}

// draw all the active bullets.  the `update` argument is `true` in the main
// game loop, but `false` during game over - so the bullets are still drawn, but
// they don't move.
void drawBullets(bool update) {
  for ( int bulletIndex = 0 ; bulletIndex < MAX_BULLETS ; bulletIndex++ ) {
    Bullet *bullet = &bullets[bulletIndex];
    if ( !bullet->isActive ) { continue; }

    if ( update ) {
      bullet->r -= TICK;
      bullet->x = CX(0.5 + RX * bullet->r * cos(bullet->deg));
      bullet->y = CY(0.5 + RY * bullet->r * sin(bullet->deg));

      float dist = pow(bullet->x - CX(0), 2) + pow(bullet->y - CY(0), 2);

      if ( dist < SMALL_R * SMALL_R ) {
        bullet->isActive = false;
      }
    }

    arduboy.drawPixel(bullet->x, bullet->y, 1);
  }
}

// draw all the active enemies.  very similar to the bullet drawing logic, but
// enemies move out from the center instead of in, and the player is "dead" when
// the enemy reaches the edge of the screen.
void drawEnemies(bool update) {
  for ( int enemyIndex = 0 ; enemyIndex < MAX_ENEMIES ; enemyIndex++ ) {
    Enemy *enemy = &enemies[enemyIndex];
    if ( !enemy->isActive ) { continue; }

    if ( update ) {
      enemy->r += 0.4 * TICK;
      enemy->x = CX(0.5 + RX * enemy->r * cos(enemy->deg));
      enemy->y = CY(0.5 + RY * enemy->r * sin(enemy->deg));

      if ( enemy->x <= 1 || enemy->x >= WIDTH - 2 || enemy->y <= 1 || enemy->y >= HEIGHT - 2 ) {
        game.state = State_died;
      }
    }

    arduboy.drawPixel(enemy->x + 1, enemy->y, 1);
    arduboy.drawPixel(enemy->x - 1, enemy->y, 1);
    arduboy.drawPixel(enemy->x, enemy->y + 1, 1);
    arduboy.drawPixel(enemy->x, enemy->y - 1, 1);
  }
}

// if a bullet is near an enemy, deactivate both projectiles
void checkCollisions() {
  for ( int enemyIndex = 0 ; enemyIndex < MAX_ENEMIES ; enemyIndex++ ) {
    Enemy *enemy = &enemies[enemyIndex];
    if ( !enemy->isActive ) { continue; }

    for ( int bulletIndex = 0 ; bulletIndex < MAX_BULLETS ; bulletIndex++ ) {
      Bullet *bullet = &bullets[bulletIndex];
      if ( !bullet->isActive ) { continue; }

      int bulletX = bullet->x;
      int bulletY = bullet->y;
      int enemyX = enemy->x;
      int enemyY = enemy->y;
      if ( abs(enemyX - bulletX) <= 2 && abs(enemyY - bulletY) <= 2 ) {
        bullet->isActive = false;
        enemy->isActive = false;
        player.score += 1;
        sound.tone(100+random(200), 10, 100+random(200), 10, 100+random(200), 10);
      }
    }
  }
}
