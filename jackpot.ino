#include <LiquidCrystal.h>
#include <EEPROM.h>

// ---------------- LCD PIN AND SHIELD SETTINGS ----------------
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// --------------------- KEYPAD SETTINGS -------------------------
#define KEYPAD_PIN A0

// Button codes
#define BTN_RIGHT   0
#define BTN_UP      1
#define BTN_DOWN    2
#define BTN_LEFT    3
#define BTN_SELECT  4
#define BTN_NONE    5

// ---------------------- GAME VARIABLES -----------------------
int jackpotCount = 0; 
int totalSpin    = 0;
int credits      = 1000; 
int bet          = 1;       
int highScore    = 1000;    

// ----------------------- EEPROM ADDRESSES ------------------------
const int jackpotAddress    = 0;
const int totalSpinAddress  = sizeof(int);
const int creditsAddress    = sizeof(int) * 2;
const int betAddress        = sizeof(int) * 3;
const int highScoreAddress  = sizeof(int) * 4;

// ------------------------- LED SETTINGS -----------------------------
#define LED_PIN 13
bool lightOn = false; 

// --------------------- SCREEN AND SCROLL FLAGS -------------
bool idleScreenShown = false;  // Is the idle screen being shown?

// ------------------ VARIABLES FOR SCROLLING -------------------
String scrollText = "";
int scrollIndex = 0;  
const unsigned long SCROLL_INTERVAL = 800; 
unsigned long lastScrollTime = 0;

// ------------------ FUNCTION PROTOTYPES ----------------------
int read_LCD_buttons();
void waitForRelease(int lastButton);

void saveAll();
void showMessage(const String &msg, unsigned long ms = 800);
void drawIdleScreen();
void updateScrolling();
void playGame();
void showMenu();
void setBet();
void showHighScore();
void resetAllData();
void toggleLight();
void showStats();

// LCD helper functions for printing:
// printStaticLine: Prints the given text starting at the specified column on the given row,
// padding it to fill 16 characters.
void printStaticLine(const String &text, int row, int col = 0);
// updateScrollingRow: Scrolls the text on the given row if its length exceeds 16 characters.
void updateScrollingRow(const String &text, int row, int &scrollIndex, unsigned long &lastTime);

// ------------------ FUNCTION DEFINITIONS -------------------

// read_LCD_buttons: Reads the analog value from the keypad and returns the corresponding button code.
int read_LCD_buttons() {
  int adc_key_in = analogRead(KEYPAD_PIN);
  if (adc_key_in > 1000) return BTN_NONE;   
  if (adc_key_in < 50)   return BTN_RIGHT;  
  if (adc_key_in < 200)  return BTN_UP;     
  if (adc_key_in < 400)  return BTN_DOWN;   
  if (adc_key_in < 600)  return BTN_LEFT;   
  if (adc_key_in < 800)  return BTN_SELECT; 
  return BTN_NONE;
}

// waitForRelease: Waits until the specified button is released (for debouncing).
void waitForRelease(int lastButton) {
  while (read_LCD_buttons() == lastButton) {
    delay(50);
  }
}

// saveAll: Saves all game variables to EEPROM.
void saveAll() {
  EEPROM.put(jackpotAddress, jackpotCount);
  EEPROM.put(totalSpinAddress, totalSpin);
  EEPROM.put(creditsAddress, credits);
  EEPROM.put(betAddress, bet);
  EEPROM.put(highScoreAddress, highScore);
}

// printStaticLine: Prints the given text starting at column 'col' on the specified row,
// padding the text so that a total of 16 characters are printed.
void printStaticLine(const String &text, int row, int col) {
  int available = 16 - col; // Number of characters available
  String padded = text;
  while (padded.length() < available) {
    padded += " ";
  }
  lcd.setCursor(col, row);
  lcd.print(padded);
}

// updateScrollingRow: Scrolls the text on the given row if its length exceeds 16 characters.
void updateScrollingRow(const String &text, int row, int &sIndex, unsigned long &lastTime) {
  int len = text.length();
  if (len <= 16) {
    printStaticLine(text, row, 0);
  } else {
    if (millis() - lastTime >= SCROLL_INTERVAL) {
      lastTime = millis();
      String displayStr = "";
      for (int i = 0; i < 16; i++) {
        displayStr += text.charAt((sIndex + i) % len);
      }
      lcd.setCursor(0, row);
      lcd.print(displayStr);
      sIndex = (sIndex + 1) % len;
    }
  }
}

// showMessage: Displays a message on the LCD for the specified duration.
// If the message is longer than 16 characters, it will scroll.
void showMessage(const String &msg, unsigned long ms) {
  lcd.clear();
  if (msg.length() > 16) {
    int scrollIdx = 0;
    unsigned long lastTime = millis();
    unsigned long start = millis();
    while (millis() - start < ms) {
      updateScrollingRow(msg, 0, scrollIdx, lastTime);
      delay(50);
    }
  } else {
    printStaticLine(msg, 0, 0);
    delay(ms);
  }
  idleScreenShown = false;
}

// drawIdleScreen: Draws the idle screen with game information on the top row and a fixed message on the bottom row.
void drawIdleScreen() {
  lcd.clear();
  scrollText = " Cr:" + String(credits) +
               " Spn:" + String(totalSpin) +
               " Win:" + String(jackpotCount) +
               " Bet:" + String(bet);
  printStaticLine("Sel=Spin L=Menu ", 1, 0);
  
  if (scrollText.length() <= 16) {
    printStaticLine(scrollText, 0, 0);
    idleScreenShown = true;
    return;
  }
  
  // Print the first 16 characters; updateScrolling() will handle scrolling.
  lcd.setCursor(0, 0);
  lcd.print(scrollText.substring(0, 16));
  scrollIndex = 0;
  lastScrollTime = millis();
  idleScreenShown = true;
}

// updateScrolling: Scrolls the idle screen text on the top row if necessary.
void updateScrolling() {
  int len = scrollText.length();
  if (len <= 16) return;
  if (millis() - lastScrollTime < SCROLL_INTERVAL) return;
  lastScrollTime = millis();
  String displayStr = "";
  for (int i = 0; i < 16; i++) {
    displayStr += scrollText.charAt((scrollIndex + i) % len);
  }
  lcd.setCursor(0, 0);
  lcd.print(displayStr);
  scrollIndex = (scrollIndex + 1) % len;
}

// playGame: Plays one round of the game (spin).
// During the spin animation, the reel values are formatted into a fixed 16-character string.
void playGame() {
  credits -= bet;
  lcd.clear();
  
  char spinBuffer[17];  // 16 characters + null terminator
  
  // Spin animation: Display random reel values 10 times.
  // Format string: "   %d   %d   %d    " produces exactly 16 characters.
  for (int i = 0; i < 10; i++){
    int r1 = random(1, 8);
    int r2 = random(1, 8);
    int r3 = random(1, 8);
    snprintf(spinBuffer, sizeof(spinBuffer), "   %d   %d   %d    ", r1, r2, r3);
    lcd.setCursor(0, 0);
    lcd.print(spinBuffer);
    delay(80);
  }
  
  // Final spin result:
  int num1 = random(1, 8);
  int num2 = random(1, 8);
  int num3 = random(1, 8);
  snprintf(spinBuffer, sizeof(spinBuffer), "   %d   %d   %d    ", num1, num2, num3);
  lcd.setCursor(0, 0);
  lcd.print(spinBuffer);
  
  if (num1 == num2 && num2 == num3) {
    printStaticLine("JACKPOT!!!", 1, 0);
    jackpotCount++;
    credits += (5 * bet);
  } else {
    // Display a losing message; here changed to "Maybe Next Time!".
    printStaticLine("Maybe Next Time!", 1, 0);
  }
  
  totalSpin++;
  if (credits > highScore) {
    highScore = credits;
  }
  delay(1200);
  idleScreenShown = false;
}

// showMenu: Displays the menu and handles menu navigation.
void showMenu() {
  const char* menuItems[5] = {
    "Set Bet",
    "High Score",
    "Reset",
    "Light",
    "Stats"
  };
  int menuCount = 5;
  int menuIndex = 0;
  
  // Separate scroll indices and timers for top and bottom rows.
  int topScrollIndex = 0;
  unsigned long topLastScrollTime = millis();
  int bottomScrollIndex = 0;
  unsigned long bottomLastScrollTime = millis();
  
  lcd.clear(); // Clear the screen once upon entering the menu.
  while (true) {
    // Top row: Menu item.
    String topText = menuItems[menuIndex];
    if (topText.length() > 16) {
      updateScrollingRow(topText, 0, topScrollIndex, topLastScrollTime);
    } else {
      printStaticLine(topText, 0, 0);
    }
    
    // Bottom row: Instruction.
    String bottomText = "UP/DN SEL=OK  L=Exit";
    if (bottomText.length() > 16) {
      updateScrollingRow(bottomText, 1, bottomScrollIndex, bottomLastScrollTime);
    } else {
      printStaticLine(bottomText, 1, 0);
    }
    
    int button = read_LCD_buttons();
    if (button == BTN_UP) {
      waitForRelease(BTN_UP);
      menuIndex--;
      if (menuIndex < 0) menuIndex = menuCount - 1;
      topScrollIndex = 0;
      topLastScrollTime = millis();
    } else if (button == BTN_DOWN) {
      waitForRelease(BTN_DOWN);
      menuIndex++;
      if (menuIndex >= menuCount) menuIndex = 0;
      topScrollIndex = 0;
      topLastScrollTime = millis();
    } else if (button == BTN_SELECT) {
      waitForRelease(BTN_SELECT);
      lcd.clear();
      switch (menuIndex) {
        case 0: setBet(); break;
        case 1: showHighScore(); break;
        case 2: resetAllData(); break;
        case 3: toggleLight(); break;
        case 4: showStats(); break;
      }
      lcd.clear();
      break; // Exit the menu.
    } else if (button == BTN_LEFT || button == BTN_RIGHT) {
      waitForRelease(button);
      lcd.clear();
      break;
    }
    delay(50);
  }
  idleScreenShown = false;
}

// setBet: Allows the user to set the bet amount.
void setBet() {
  int topScrollIndex = 0;
  unsigned long topLastScrollTime = millis();
  int bottomScrollIndex = 0;
  unsigned long bottomLastScrollTime = millis();
  
  lcd.clear();
  
  while (true) {
    // Top row: "Set Bet: <bet>"
    String topText = "Set Bet: " + String(bet);
    if (topText.length() > 16) {
      updateScrollingRow(topText, 0, topScrollIndex, topLastScrollTime);
    } else {
      printStaticLine(topText, 0, 0);
    }
    
    // Bottom row: "UP/DN=+-1 Sel=Exit"
    String bottomText = "UP/DN=+-1 Sel=Exit";
    if (bottomText.length() > 16) {
      updateScrollingRow(bottomText, 1, bottomScrollIndex, bottomLastScrollTime);
    } else {
      printStaticLine(bottomText, 1, 0);
    }
    
    int button = read_LCD_buttons();
    if (button == BTN_UP) {
      waitForRelease(BTN_UP);
      bet++;
      if (bet > 5) bet = 5;
    } else if (button == BTN_DOWN) {
      waitForRelease(BTN_DOWN);
      bet--;
      if (bet < 1) bet = 1;
    } else if (button == BTN_SELECT || button == BTN_LEFT || button == BTN_RIGHT) {
      waitForRelease(button);
      saveAll();
      lcd.clear();
      break;
    }
    delay(50);
  }
}

// showHighScore: Displays the high score.
void showHighScore() {
  lcd.clear();
  printStaticLine("High Score:", 0, 0);
  printStaticLine(String(highScore), 1, 0);
  delay(2000);
}

// resetAllData: Resets all game data if confirmed.
void resetAllData() {
  lcd.clear();
  printStaticLine("Reset? Sel=Yes", 0, 0);
  printStaticLine("Other=No", 1, 0);
  delay(200);
  
  while (true) {
    int b = read_LCD_buttons();
    if (b == BTN_SELECT) {
      waitForRelease(BTN_SELECT);
      jackpotCount = 0;
      totalSpin = 0;
      credits = 1000;
      bet = 1;
      highScore = 1000;
      saveAll();
      showMessage("Data Reset!", 1000);
      break;
    } else if (b == BTN_UP || b == BTN_DOWN || b == BTN_LEFT || b == BTN_RIGHT || b == BTN_NONE) {
      waitForRelease(b);
      showMessage("Canceled", 800);
      break;
    }
    delay(50);
  }
}

// toggleLight: Toggles the LED state.
void toggleLight() {
  lightOn = !lightOn;
  digitalWrite(LED_PIN, lightOn ? HIGH : LOW);
  if (lightOn)
    showMessage("Light ON", 600);
  else
    showMessage("Light OFF", 600);
}

// showStats: Displays game statistics.
void showStats() {
  lcd.clear();
  float ratio = 0.0;
  if (totalSpin > 0) ratio = (float)jackpotCount / totalSpin * 100.0;
  
  printStaticLine("Spin:" + String(totalSpin) + " Jp:" + String(jackpotCount), 0, 0);
  printStaticLine("Rate:" + String(ratio, 1) + "%", 1, 0);
  delay(2000);
}

void setup() {
  lcd.begin(16, 2);
  randomSeed(analogRead(A0));
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  EEPROM.get(jackpotAddress, jackpotCount);
  EEPROM.get(totalSpinAddress, totalSpin);
  EEPROM.get(creditsAddress, credits);
  EEPROM.get(betAddress, bet);
  EEPROM.get(highScoreAddress, highScore);
  
  lcd.clear();
  printStaticLine("Loaded Data", 0, 0);
  printStaticLine("Cr:" + String(credits) + " HS:" + String(highScore), 1, 0);
  delay(1500);
}

void loop() {
  if (!idleScreenShown) {
    drawIdleScreen();
  }
  
  if (idleScreenShown) {
    updateScrolling();
  }
  
  int button = read_LCD_buttons();
  if (button == BTN_NONE) {
    delay(100);
    return;
  }
  
  switch (button) {
    case BTN_SELECT:
      waitForRelease(BTN_SELECT);
      playGame();
      break;
    case BTN_LEFT:
      waitForRelease(BTN_LEFT);
      showMenu();
      break;
    case BTN_RIGHT:
      waitForRelease(BTN_RIGHT);
      saveAll();
      showMessage("Data Saved!", 800);
      break;
    case BTN_UP:
    case BTN_DOWN:
      waitForRelease(button);
      showMessage("No Action", 600);
      break;
  }
  delay(100);
}
