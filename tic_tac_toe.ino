/**
 * ============================================================================
 * TIC-TAC-TOE GAME CONTROLLER - ESP32 Arduino Project
 * 
 * AUTHOR: Krishna Gorai
 * IITM BS DEGREE ELECTRONICS - Diploma level
 * EMAIL: 25f1100001@es.study.iitm.ac.in
 * GITHUB: https://github.com/krishnagoraioficial
 * 
 * PROJECT DESCRIPTION:
 * Standalone 3x3 Tic-Tac-Toe game with OLED display, 9-button keypad,
 * and Serial communication interface for PC/Linux integration.
 * Features state machine architecture, debouncing, and win detection.
 * 
 * HARDWARE:
 * - ESP32 Development Board
 * - 128x64 SSD1306 OLED Display (I2C: 0x3C)
 * - 9 Push Buttons (3x3 Grid) + 2 Navigation Buttons
 * - Serial Monitor for move broadcasting
 * 
 * LAST UPDATED: February 2026
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// HARDWARE PIN MAPPING - ESP32 GPIO Configuration
// ============================================================================
// 3x3 Game Keypad Buttons mapped to physical grid (0-8 indexing)
// Layout: [0][1][2] TOP ROW
//         [3][4][5] MIDDLE ROW  
//         [6][7][8] BOTTOM ROW
const int GAME_BTN_PINS[9] = {13, 12, 14, 27, 26, 25, 33, 32, 15}; 

// Navigation Buttons (Carefully selected to avoid I2C/display conflicts)
const int BTN_NAV_SELECT = 4; // Green Button - Confirm/Start/Play Again
const int BTN_NAV_BACK = 5;   // Blue Button  - Menu/Exit Game

// OLED Display Configuration (I2C Interface)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ============================================================================
// GAME DISPLAY PARAMETERS
// ============================================================================
const int BOARD_X = 32;        // X-offset to center 3x3 grid (63px wide)
const int CELL_SIZE = 21;      // Pixel size per cell (63x63 total grid)
const int DEBOUNCE_DELAY = 250; // Debounce time in milliseconds

// ============================================================================
// STATE MACHINE DEFINITION
// ============================================================================
// Finite State Machine for clean game flow control
enum GameState {
  STATE_MENU,    // Main menu screen
  STATE_PLAYING, // Active gameplay
  STATE_GAMEOVER // Win/Draw results screen
};

// ============================================================================
// GLOBAL GAME STATE VARIABLES
// ============================================================================
GameState currentState = STATE_MENU;  // Current FSM state
char board[3][3];                     // 3x3 game board data structure
char currentPlayer = 'X';             // Active player ('X' or 'O')
int winner = 0;                       // 0=No winner, 1=X wins, 2=O wins, 3=Draw
int moveCount = 0;                    // Total moves (max 9 for draw detection)
unsigned long lastInputTime = 0;      // Timestamp for input debouncing

// ============================================================================
// FUNCTION PROTOTYPES - Forward Declarations
// ============================================================================
void drawGrid();           // Render empty game grid
void drawX(int row, int col);  // Draw 'X' symbol at position
void drawO(int row, int col);  // Draw 'O' symbol at position
void resetGame();          // Reset all game variables
int checkWin();            // Win condition checker
int getPressedButton();    // Scan keypad for input

/**
 * ============================================================================
 * ARDUINO SETUP FUNCTION
 * Initializes GPIO pins, OLED display, and game state
 * ============================================================================
 */
void setup() {
  Serial.begin(115200);
  Serial.println("=== TIC-TAC-TOE v1.0 STARTING ===");

  // === 1. GPIO INITIALIZATION ===
  // Configure all 9 game buttons + 2 nav buttons as INPUT_PULLUP
  for (int i = 0; i < 9; i++) {
    pinMode(GAME_BTN_PINS[i], INPUT_PULLUP);
  }
  pinMode(BTN_NAV_SELECT, INPUT_PULLUP);
  pinMode(BTN_NAV_BACK, INPUT_PULLUP);

  // === 2. OLED DISPLAY INITIALIZATION ===
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed - Check wiring!"));
    for(;;); // Halt execution
  }
  
  // Clear display and set default text color
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();
  
  // Initialize game in menu state
  resetGame();
  
  Serial.println("✓ Hardware initialized successfully");
}

/**
 * ============================================================================
 * MAIN LOOP - STATE MACHINE IMPLEMENTATION
 * Handles all game states with debounced input processing
 * ============================================================================
 */
void loop() {
  // === INPUT DEBOUNCING ===
  // Prevent multiple rapid inputs (250ms cooldown)
  if (millis() - lastInputTime < DEBOUNCE_DELAY) return;

  switch (currentState) {
    
    // =================================================
    // STATE 1: MAIN MENU SCREEN
    // =================================================
    case STATE_MENU:
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(30, 10);
      display.println("TIC-TAC-TOE");
      
      display.setCursor(10, 40);
      display.print("Press GREEN to Start");
      display.display();

      // Transition to gameplay on GREEN button press
      if (digitalRead(BTN_NAV_SELECT) == LOW) {
        resetGame();
        currentState = STATE_PLAYING;
        
        // Render initial empty board
        display.clearDisplay();
        drawGrid();
        display.display();
        lastInputTime = millis();
      }
      break;

    // =================================================
    // STATE 2: ACTIVE GAMEPLAY
    // =================================================
    case STATE_PLAYING: 
    { 
      // Scan 3x3 keypad for pressed button (0-8)
      int btnIndex = getPressedButton();

      if (btnIndex != -1) { 
        int row = btnIndex / 3;  // Row calculation (0,1,2)
        int col = btnIndex % 3;  // Column calculation (0,1,2)

        // VALIDATE: Check if cell is empty
        if (board[row][col] == ' ') {
          // === STEP 1: UPDATE GAME DATA ===
          board[row][col] = currentPlayer;
          moveCount++;

          // === STEP 2: RENDER MOVE ON DISPLAY ===
          if (currentPlayer == 'X') drawX(row, col);
          else drawO(row, col);
          display.display();

          // === STEP 3: BROADCAST MOVE TO EXTERNAL SYSTEM ===
          // Serial protocol for PC/Linux integration
          sendMoveToPC(currentPlayer, row, col); 

          // === STEP 4: WIN/DRAW DETECTION ===
          int result = checkWin();
          if (result != 0) {
             winner = result;
             Serial.print("RESULT:"); 
             Serial.println(winner == 1 ? "X" : "O");
             delay(500);  // Brief pause for visibility
             currentState = STATE_GAMEOVER;
          } else if (moveCount >= 9) {
             winner = 3;
             Serial.println("RESULT:DRAW");
             delay(500);
             currentState = STATE_GAMEOVER;
          } else {
             // Switch players
             currentPlayer = (currentPlayer == 'X') ? 'O' : 'X';
          }
        }
        lastInputTime = millis();
      }

      // === BACK TO MENU ===
      if (digitalRead(BTN_NAV_BACK) == LOW) {
        currentState = STATE_MENU;
        lastInputTime = millis();
      }
      break;
    } // End STATE_PLAYING scope

    // =================================================
    // STATE 3: GAME OVER SCREEN
    // =================================================
    case STATE_GAMEOVER:
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(15, 10);
      
      if (winner == 1) display.println("X WINS!");
      else if (winner == 2) display.println("O WINS!");
      else display.println("DRAW!");

      display.setTextSize(1);
      display.setCursor(10, 45);
      display.println("Green: Play Again");
      display.display();

      // Return to menu on GREEN button
      if (digitalRead(BTN_NAV_SELECT) == LOW) {
        currentState = STATE_MENU;
        lastInputTime = millis();
      }
      break;
  }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Scans 9 game buttons and returns pressed index (0-8) or -1 (none)
 */
int getPressedButton() {
  for (int i = 0; i < 9; i++) {
    if (digitalRead(GAME_BTN_PINS[i]) == LOW) {
      return i;
    }
  }
  return -1;
}

/**
 * Reset all game variables to initial state
 * Clears board, resets player, move counter, and winner
 */
void resetGame() {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      board[i][j] = ' ';  // Empty cell marker
    }
  }
  currentPlayer = 'X';
  winner = 0;
  moveCount = 0;
}

/**
 * WIN CONDITION CHECKER
 * Returns: 1=X wins, 2=O wins, 0=no winner yet
 * Checks all 8 possible winning combinations
 */
int checkWin() {
  // === ROWS ===
  for (int i = 0; i < 3; i++) {
    if (board[i][0] != ' ' && board[i][0] == board[i][1] && board[i][1] == board[i][2]) 
      return (board[i][0] == 'X') ? 1 : 2;
  }
  
  // === COLUMNS ===
  for (int i = 0; i < 3; i++) {
    if (board[0][i] != ' ' && board[0][i] == board[1][i] && board[1][i] == board[2][i]) 
      return (board[0][i] == 'X') ? 1 : 2;
  }
  
  // === DIAGONALS ===
  if (board[0][0] != ' ' && board[0][0] == board[1][1] && board[1][1] == board[2][2]) 
    return (board[0][0] == 'X') ? 1 : 2;
    
  if (board[0][2] != ' ' && board[0][2] == board[1][1] && board[1][1] == board[2][0]) 
    return (board[0][2] == 'X') ? 1 : 2;

  return 0; // No winner yet
}

// ============================================================================
// GRAPHICS RENDERING FUNCTIONS
// ============================================================================

/**
 * Draw 3x3 grid lines on OLED display
 * Creates visual boundary for game cells
 */
void drawGrid() {
  // Vertical grid lines (2 lines)
  display.drawLine(BOARD_X + CELL_SIZE, 0, BOARD_X + CELL_SIZE, 63, WHITE);
  display.drawLine(BOARD_X + CELL_SIZE*2, 0, BOARD_X + CELL_SIZE*2, 63, WHITE);
  
  // Horizontal grid lines (2 lines)
  display.drawLine(BOARD_X, CELL_SIZE, BOARD_X + CELL_SIZE*3, CELL_SIZE, WHITE);
  display.drawLine(BOARD_X, CELL_SIZE*2, BOARD_X + CELL_SIZE*3, CELL_SIZE*2, WHITE);
}

/**
 * Render 'X' symbol at specified board position
 * Uses two crossing lines with proper margins
 */
void drawX(int row, int col) {
  int x = BOARD_X + (col * CELL_SIZE);
  int y = (row * CELL_SIZE);
  
  // Two diagonal lines forming 'X'
  display.drawLine(x + 5, y + 5, x + CELL_SIZE - 5, y + CELL_SIZE - 5, WHITE);
  display.drawLine(x + CELL_SIZE - 5, y + 5, x + 5, y + CELL_SIZE - 5, WHITE);
}

/**
 * Render 'O' symbol at specified board position
 * Centered circle with proper scaling
 */
void drawO(int row, int col) {
  int x = BOARD_X + (col * CELL_SIZE) + (CELL_SIZE / 2);
  int y = (row * CELL_SIZE) + (CELL_SIZE / 2);
  display.drawCircle(x, y, (CELL_SIZE / 2) - 5, WHITE);
}

/**
 * SERIAL COMMUNICATION PROTOCOL
 * Broadcasts moves to PC/Linux host for external processing
 * Format: "MOVE:X,1,2" or "RESULT:X"
 */
void sendMoveToPC(char player, int row, int col) {
  Serial.print("MOVE:");
  Serial.print(player);
  Serial.print(",");
  Serial.print(row);
  Serial.print(",");
  Serial.println(col);
}
