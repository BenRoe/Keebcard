#include "settings.h"
#include "Tetris.h"

#define SSD1306_DATA 0x40

#define BOARD_WIDTH 8
#define BOARD_HEIGHT 32

#define STARTING_PIECE_WIDTH 4
#define STARTING_PIECE_HEIGHT 34

#define RENDER_SMALL false

// cut down on features since digispark has a bootloader
// #define DIGISPARK

// TODO these will not spawn at the same heights, but rotate (hopefully) correctly
// about their axis. if I move them to all spawn at the same height, they will
// lose that axis rotation. I need to reorder so that the first or last
// pixel is always the lowest pixel, so that I may use that in my program
// to spawn them all at the same height
const Shape Tetris::shapes[28] PROGMEM = {
  { { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } } }, // square 1
  { { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } } }, // square 2
  { { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } } }, // square 3
  { { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } } }, // square 4

  { { { -1, -1 }, { -1, 0 }, { 0, 0 }, { 1, 0 } } }, // J 1
  { { { 0, -1 }, { 0, 0 }, { 0, 1 }, { 1, 1 } } }, // J 2
  { { { -1, 0 }, { 0, 0 }, { 1, 0 }, { 1, -1 } } }, // J 3
  { { { -1, -1 }, { 0, -1 }, { 0, 0 }, { 0, 1 } } }, // J 4

  { { { -1, 0 }, { 0, 0 }, { 1, 0 }, { 1, 1 } } }, // L 1
  { { { 0, 1 }, { 0, 0 }, { 0, -1 }, { 1, -1 } } }, // L 2
  { { { -1, -1 }, { -1, 0 }, { 0, 0 }, { 1, 0 } } }, // L 3
  { { { -1, 1 }, { 0, 1 }, { 0, 0 }, { 0, -1 } } }, // L 4

  { { { -1, 0 }, { 0, 0 }, { 0, -1 }, { 1, -1 } } }, // S 1
  { { { 0, -1 }, { 0, 0 }, { 1, 0 }, { 1, 1 } } }, // S 2
  { { { -1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 } } }, // S 3
  { { { -1, -1 }, { -1, 0 }, { 0, 0 }, { 0, 1 } } }, // S 4

  { { { -1, 0 }, { 0, 0 }, { 0, -1 }, { 1, 0 } } }, // T 1
  { { { 0, -1 }, { 0, 0 }, { 0, 1 }, { 1, 0 } } }, // T 2
  { { { -1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 0 } } }, // T 3
  { { { -1, 0 }, { 0, -1 }, { 0, 0 }, { 0, 1 } } }, // T 4

  { { { -1, -1 }, { 0, -1 }, { 0, 0 }, { 1, 0 } } }, // Z 1
  { { { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, -1 } } }, // Z 2
  { { { -1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 } } }, // Z 3
  { { { -1, 1 }, { -1, 0 }, { 0, 0 }, { 0, -1 } } }, // Z 4

  { { { -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 } } }, // I 1
  { { { 0, 2 }, { 0, 1 }, { 0, 0 }, { 0, -1 } } }, // I 2
  { { { -2, 1 }, { -1, 1 }, { 0, 1 }, { 1, 1 } } }, // I 3
  { { { -1, -2 }, { -1, -1 }, { -1, 0 }, { -1, 1 } } }, // I 4
};

// static, so initialized to 0s anyways
uint8_t Tetris::board[32] = {
  0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00,
};
// initialized elsewhere instead of reading from progmem here
Shape Tetris::shape = { { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } } };
// square 1
// initialized elsewhere
static uint8_t Tetris::shapeIndex = 1;

const uint8_t Tetris::numShapes = sizeof(Tetris::shapes) / sizeof(Tetris::shapes[0]); // 28

// TODO use 3 + 800 / (16 + x*x) to determine required frames. recalculate on increase in score? or just use as function

Tetris::Tetris(SSD1306Device* _oled) {
  oled = _oled;
  position = { STARTING_PIECE_WIDTH, STARTING_PIECE_HEIGHT };
}

uint8_t Tetris::run() {
  main();
  end();
  return score*10;
}

void Tetris::main() {
  while (true) {
    uint32_t t1 = millis();
    checkInputs();
    // if the piece is in free fall
    if(!checkCollision({ 0, -1})) {
      checkInputs();
      movePiece();
      renderBoard();
      checkInputs();
    } else {
      if (position.y >= BOARD_HEIGHT) break;
      // re-add piece back in its new final resting place
      addOrRemovePiece(true);
      // write once more to get rid of a nasty double buffer bug introduced when I stopped rendering the full screen
      // pieces will bounce if they hit but don't complete a row
      renderBoard(false, false);
      // check to see if any rows are full and need to be culled
      // needs to be done before we spawn a new piece, once again, due to rendering tricks
      // its so much faster though, totally worth it
      checkForFullRows();
      // more double buffer bugs when you complete full rows
      renderBoard(false, false);
      // spawn a new piece
      spawnNewPiece();
      checkInputs();
    }

    uint32_t delta = millis() - t1;
    // 60fps lol
    if (delta < 16) delay(16 - delta);
    checkInputs();
  }
}

void Tetris::checkInputs() {
  buttonFlags = buttonFlags |
                ((digitalRead(LEFT_BUTTON) == LOW) << LEFT_BUTTON) |
                ((digitalRead(RIGHT_BUTTON) == LOW) << RIGHT_BUTTON) |
                ((digitalRead(MIDDLE_BUTTON) == LOW) << MIDDLE_BUTTON);
}

// TODO coop multithread
void Tetris::movePiece() {
  position.y--;
#ifndef DIGISPARK
  if ((buttonFlags & 0x01 << LEFT_BUTTON) && !checkCollision({ -1, 0 })) {
    --position.x;
  } else if ((buttonFlags & 0x01 << MIDDLE_BUTTON) && !checkCollision({ 1, 0 })) {
    ++position.x;
  }
  if ((buttonFlags & 0x01 << RIGHT_BUTTON)) {
    rotatePiece();
  }
  buttonFlags = 0;
#endif
}

void Tetris::end() {
  // reset board in case
  memset(board, 0x00, 32);
}

void Tetris::spawnNewPiece() {
  assignRandomShape();
  // I organized the shapes so that the first pixel is always one of the leftmost,
  // and the last is always one of the rightmost. if I'm going with random positions
  // I can use that here to dynamically change my bounds
  position.x = STARTING_PIECE_WIDTH;
  position.y = STARTING_PIECE_HEIGHT;
}

void Tetris::assignRandomShape() {
  // static uint8_t index = 0;
  // assignShape(index++); // (rand()) % numShapes
  // assignShape(((uint8_t)rand()) % numShapes);
  // assignShape(8);
  assignShape(((uint8_t) rand() & 0b00000111 - 1) << 2);
}

void Tetris::assignShape(uint8_t index) {
  shapeIndex = index;
  // load shape from progmem into ram since we need it everywhere
  memcpy_P(&shape, &shapes[shapeIndex], sizeof(Shape));
}

// really
void Tetris::checkForFullRows() {
  uint8_t extraScore = 0;
  uint8_t y = (int8_t)(position.y) - 3 < 0 ? 0 : position.y - 3;
  while(y < BOARD_HEIGHT) {
    if (board[y] == 0xFF) {
      extraScore++;
      for (uint8_t yy = y; yy < BOARD_HEIGHT - 1; yy++) {
        board[yy] = board[yy+1];
      }

      // doesn't work sporadically, not sure why. seems to be when 2 or more lines finish
      // memmove((void*)(board + y), (void*)(board + y + 1), 1);
      // could do if extraScore == 1 since we only have to do this once -
      // the first time, when there might be stuff in the top row that
      // gets duplicated - but meh
      board[BOARD_HEIGHT-1] = 0x00;
      // we could move this out of the loop but it creates a nice effect
      renderBoard(true, false);
    } else {
      ++y;
    }
  }

  // got a double buffer bug when rows are cleared. trying this
  renderBoard(false, false);
  if (extraScore % 2 == 1) {
    // double render bug, I think
    // this doesn't have to be renderBoard(true) because it can only be 1 or 3, in which case our envelope catches it
  } else if (extraScore == 4) {
    // extra score for getting a yahtzee or whatever
    extraScore++;
  }

  score += extraScore;
}

void Tetris::rotatePiece() {
  // we need to check the collision before we can see if the shape is good
  // so we save the old id just in case
  const uint8_t oldIndex = shapeIndex;

  // shapes are arranged in groups of 4, 0-3 being the first
  // we want to stay in the group we are in while advancing one place
  assignShape((shapeIndex & 0b11111100) | ((shapeIndex+1) & 0b00000011));

  if (checkCollision({ 0, 0 })) {
    assignShape(oldIndex);
  }
}

// two times we check collision: 1. to know if a brick hits another brick
// 2. to know if a rotation would fail.
// for #1 use a modifier of -1 to check underneath pixels.
// #2 just use 0
bool Tetris::checkCollision(Position delta) {
  for (uint8_t i = 0; i < PIXELS_IN_SHAPE; i++) {
    Position pixel = shape.pixels[i];
    Position newPosition = { position.x + pixel.x + delta.x, position.y + pixel.y + delta.y };

    // if we've hit the bottom it always collides
    // most collision checks are done with a delta - they are checking
    // if the piece moves to a location whether it collides or not.
    // the only exception is rotation checks, which just want to check
    // if the new rotated piece is occupying the same space as something.
    // hence < 0 instead of <= 0 for the 0 checks
    if (((int8_t)newPosition.y < 0) || ((int8_t)newPosition.x < 0) || (newPosition.x >= BOARD_WIDTH)) {
      return true;
    }

    // pixels above the play field do not collide. they will also buffer overflow lol
    if (newPosition.y < BOARD_HEIGHT) {
      uint8_t row = board[newPosition.y];

      // check underneath for blocks
      if (row & (0x01 << newPosition.x)) {
        return true;
      }
    }
  }

  return false;
}

// this method should not be called when a piece would interstect
void Tetris::addOrRemovePiece(bool add) {
  for (uint8_t i = 0; i < PIXELS_IN_SHAPE; i++) {
    Position pixel = shape.pixels[i];
    Position newPosition = { position.x + pixel.x, position.y + pixel.y };

    // check bounds, continue rendering the rest of the piece if we fail though
    if (newPosition.y > BOARD_HEIGHT || (int8_t)newPosition.y < 0) continue;
    if (newPosition.x > BOARD_WIDTH || (int8_t)newPosition.x < 0) continue;

    uint8_t* row = &board[(newPosition.y)];
    if (add) {
      *row |= 0x01 << (newPosition.x);
    } else {
      *row &= ~(0x01 << (newPosition.x));
    }
  }
}

// only call this function when the piece is in a good place
void Tetris::renderBoard(bool wholeScreen, bool removePiece) {
  // add piece to board
  addOrRemovePiece(true);
  wholeScreen = false;
  // reset to bottom left

  // no way in hell am I importing std
  // 6 works to get rid of double buffer problems, but 4 might as well
  uint8_t yMin = wholeScreen ? 0 : ((int8_t)(position.y) - 6 < 0 ? 0 : position.y - 6);
  uint8_t yMax = wholeScreen ? BOARD_HEIGHT : (position.y + 6 > BOARD_HEIGHT ? BOARD_HEIGHT : position.y + 6);

  oled->setCursor(yMin*4,0);

  for (uint8_t x = 0; x < (RENDER_SMALL ? 1 : BOARD_WIDTH/2); x++) {
    for (uint8_t y = yMin; y < yMax; y++) {
      // proceeding upwards, grab pixels 2x1 and write them to the screen
      uint8_t row = board[y];
      bool pixel1 = row & 0x01 << (x * 2 + 1);
      bool pixel2 = row & 0x01 << (x * 2);

      uint8_t block = pixel1 ? 0xF0 : 0x00;
      block |= pixel2 ? 0x0F : 0x00;

      // let the screen know there's data comin
      oled->startData();

      for (uint8_t i = 0; i < (RENDER_SMALL ? 1 : 4); i++){
        oled->sendData((RENDER_SMALL ? row : block));
      }

      oled->endData();
    }

    oled->setCursor(yMin*4,x+1);
  }
  if (RENDER_SMALL) delay(200);
  if (removePiece) addOrRemovePiece(false);

  // debugging
  oled->setCursor(112,0);
  oled->print(shapeIndex);
  oled->switchFrame();
}
