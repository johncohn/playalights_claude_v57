/*
  NeoPixel Controller - Modular Version with OTA
  22 patterns with robust networking, audio, UI, and OTA updates
  Refactored into manageable modules
  
  ** SERIAL DEBUG OUTPUT SIGNIFICANTLY REDUCED **
  Use DEBUG_SERIAL, DEBUG_HEARTBEAT, DEBUG_BPM in config.h to control output
*/

#include "config.h"
#include "patterns.h" 
#include "networking.h"
#include "audio.h"
#include "ui.h"
#include "ota.h"
#include "version.h"

// ── Global Variable Definitions ───────────────────────────────────────────────
Mode      currentMode      = AUTO;
FsmState  fsmState         = FOLLOWER;
uint8_t   styleIdx         = 0;
bool      freezeActive     = false;

CRGB      leds[NUM_LEDS];
LGFX_Sprite canvas(&M5.Lcd);
Preferences prefs;

// ── Global Brightness Control ────────────────────────────────────────────────
uint8_t   globalBrightnessScale = 64;  // 25% of max brightness (64/255) - LOCAL CONTROL

// ── Control Arrays ────────────────────────────────────────────────────────────
uint8_t speedVals[MODE_COUNT][22], brightVals[MODE_COUNT][22],
        ssensVals[MODE_COUNT][22], bsensVals[MODE_COUNT][22],
        vsensVals[MODE_COUNT][22], decayVals[MODE_COUNT][22],
        timeVals[MODE_COUNT][22];

// ── Network Variables ─────────────────────────────────────────────────────────
uint8_t  broadcastAddress[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
uint32_t masterSeq         = 0, chunkMask      = 0, lastRecvMillis = 0;
uint32_t electionStart     = 0, electionEnd    = 0;
uint32_t myToken           = 0, highestTokenSeen = 0, myDelay = 0;
bool     electionBroadcasted = false;
uint32_t lastTokenBroadcast  = 0, lastHeartbeat      = 0, missedFrameCount   = 0;

// ── Audio Variables ───────────────────────────────────────────────────────────
float   soundMin      = 1.0f, soundMax      = 0.0f, musicLevel = 0.0f;
bool    prevAbove     = false;
uint32_t beatTimes[50];
uint8_t  beatCount    = 0;
uint32_t lastBpmMillis= 0;
bool    audioDetected = true;

// ── Watchdog Variables ────────────────────────────────────────────────────────
uint32_t lastWatchdogUpdate = 0;
const uint32_t WATCHDOG_TIMEOUT = 30000;  // 30 seconds

// ── System State Variables ────────────────────────────────────────────────────
uint32_t lastSystemCheck = 0;
const uint32_t SYSTEM_CHECK_INTERVAL = 5000; // Check system health every 5 seconds

void feedWatchdog() {
  lastWatchdogUpdate = millis();
}

void checkWatchdog() {
  uint32_t now = millis();
  if (now - lastWatchdogUpdate > WATCHDOG_TIMEOUT) {
    if(DEBUG_SERIAL) Serial.println("WATCHDOG: System appears hung - restarting...");
    ESP.restart();
  }
}

void checkSystemHealth() {
  uint32_t now = millis();
  if (now - lastSystemCheck < SYSTEM_CHECK_INTERVAL) return;
  lastSystemCheck = now;
  
  // Check ESP-NOW peer status (but not if in OTA mode)
  if (!isInOTAMode() && !esp_now_is_peer_exist(broadcastAddress)) {
    if(DEBUG_SERIAL) Serial.println("HEALTH: ESP-NOW broadcast peer missing - re-adding");
    esp_now_peer_info_t peer={};
    memcpy(peer.peer_addr, broadcastAddress, 6);
    peer.channel = 0; 
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
  
  // Check if ESP-NOW needs reinitializing after OTA mode exit
  static bool wasInOTAMode = false;
  bool currentlyInOTA = isInOTAMode();
  if (wasInOTAMode && !currentlyInOTA) {
    // Just exited OTA mode - reinitialize networking
    if(DEBUG_SERIAL) Serial.println("HEALTH: Reinitializing networking after OTA mode");
    delay(100); // Brief pause for stability
    initNetworking();
  }
  wasInOTAMode = currentlyInOTA;
  
  // Check if we're stuck in a bad state
  static FsmState lastFsmState = FOLLOWER;
  static uint32_t stateChangeTime = now;
  static int stuckCounter = 0;
  
  if (fsmState != lastFsmState) {
    lastFsmState = fsmState;
    stateChangeTime = now;
    stuckCounter = 0;
  } else {
    // Check if we've been in the same state too long
    if (now - stateChangeTime > 60000) { // 1 minute in same state
      stuckCounter++;
      if (stuckCounter > 3) {
        if(DEBUG_SERIAL) Serial.printf("HEALTH: Stuck in %s state - forcing election\n", 
          fsmState == LEADER ? "LEADER" : fsmState == FOLLOWER ? "FOLLOWER" : "ELECT");
        
        // Force a new election to unstick the system
        fsmState = ELECT;
        electionStart = now;
        electionEnd = now + ELECTION_TIMEOUT;
        highestTokenSeen = myToken;
        myDelay = random(0, ELECTION_JITTER);
        electionBroadcasted = false;
        missedFrameCount = 0;
        chunkMask = 0;
        stuckCounter = 0;
      }
    }
  }
  
  // Check for memory issues
  if (ESP.getFreeHeap() < 10000) { // Less than 10KB free
    if(DEBUG_SERIAL) Serial.printf("HEALTH: Low memory - %d bytes free\n", ESP.getFreeHeap());
  }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup(){
  Serial.begin(115200);
  
  // Initialize watchdog
  feedWatchdog();
  
  // Show startup info
  if(DEBUG_SERIAL) {
    Serial.println("=====================================");
    Serial.printf("NeoPixel Controller v%s\n", FIRMWARE_VERSION);
    Serial.printf("Build: %s %s\n", BUILD_DATE, BUILD_TIME);
    Serial.println("=====================================");
  }
  
  // Initialize M5 hardware
  M5.begin();
  feedWatchdog();
  
  // Initialize all modules
  loadControls();
  feedWatchdog();
  initUI();
  feedWatchdog();
  initAudio();
  feedWatchdog();
  initNetworking();
  feedWatchdog();
  
  // Initialize OTA (must be after networking for WiFi)
  initOTA();
  feedWatchdog();
  
  // Initialize FastLED
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
         .setCorrection(TypicalLEDStrip);
  // Start with default local brightness (25%)
  FastLED.setBrightness(globalBrightnessScale);
  feedWatchdog();

  // Initialize timing and state
  randomSeed(micros());
  lastRecvMillis     = millis();
  lastTokenBroadcast = millis();
  lastHeartbeat      = millis();
  lastSystemCheck    = millis();
  missedFrameCount   = 0;
  
  // Ensure we start in a clean state
  fsmState = FOLLOWER;
  currentMode = AUTO;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  
  if(DEBUG_SERIAL) {
    Serial.printf("NeoPixel Controller v%s initialized - 22 patterns ready!\n", FIRMWARE_VERSION);
    Serial.printf("Ready for OTA updates at: NeoNode-%06X.local\n", myToken);
    Serial.println("Watchdog timer enabled (30s timeout)");
    Serial.printf("Local brightness: %d/255 (%.1f%%) - each node controls its own\n", 
      globalBrightnessScale, (globalBrightnessScale * 100.0f) / 255.0f);
    Serial.println("Button A: Short press = local brightness cycle, Long press = OFF/ON");
    Serial.println("Button B: Pattern freeze/advance (AUTO-LEADER mode only)");
    Serial.println("Button C: Manual sync reset (force resynchronization)");
    Serial.println("ESP-NOW mesh network active - WiFi optional for OTA");
  }
  feedWatchdog();
}

// ── Main Loop ─────────────────────────────────────────────────────────────────
void loop(){
  // Feed watchdog at start of every loop
  feedWatchdog();
  
  M5.update();
  
  // Handle OTA updates (highest priority, but only if WiFi connected)
  handleOTA();
  
  // Handle user input
  handleButtons();
  
  if(currentMode == OFF) {
    // OFF mode - minimal processing for battery savings
    drawUI();   // Show OFF status on dimmed display
    delay(200); // Longer delay to save power
    return;     // Skip all LED/networking processing
  }
  
  // AUTO mode - full functionality
  handleNetworking(); // This handles WiFi transitions gracefully
  drawUI();
  updateBPM();
  
  // Periodic system health checks
  checkSystemHealth();
  
  // Check watchdog (will restart if hung)
  checkWatchdog();
}