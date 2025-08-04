#include "ota.h"

void initOTA(){
  // Only initialize OTA if WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    if(DEBUG_SERIAL) Serial.println("OTA disabled - no WiFi connection");
    return;
  }
  
  // CRITICAL: Disable ESP-NOW immediately to prevent interference
  // This is especially important for leader nodes that constantly broadcast
  if(DEBUG_SERIAL) Serial.println("Disabling ESP-NOW for OTA preparation");
  esp_now_deinit();
  WiFi.mode(WIFI_STA); // Ensure we stay in STA mode for OTA
  
  // Create unique hostname using the node's token
  String hostname = "NeoNode-" + String(myToken, HEX);
  ArduinoOTA.setHostname(hostname.c_str());
  
  // Set password to match what Arduino IDE is sending
  ArduinoOTA.setPassword("neopixel123");
  
  // Set port (default 3232 is fine for most cases)
  ArduinoOTA.setPort(3232);
  
  setOTACallbacks();
  ArduinoOTA.begin();
  
  if(DEBUG_SERIAL) {
    Serial.printf("OTA initialized: %s.local (IP: %s) - ESP-NOW disabled\n", 
      hostname.c_str(), WiFi.localIP().toString().c_str());
  }
}

void setOTACallbacks(){
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    
    if(DEBUG_SERIAL) Serial.println("OTA Start updating " + type);
    
    // Turn off all LEDs immediately and keep them dark during entire upload
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    
    // Show OTA status on LCD only
    canvas.fillSprite(TFT_BLACK);
    canvas.fillRect(0, 0, M5.Lcd.width(), 40, TFT_BLUE);
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(10, 10);
    canvas.print("OTA UPDATE");
    canvas.setTextSize(1);
    canvas.setCursor(10, 50);
    canvas.print("Updating " + type + "...");
    canvas.setCursor(10, 65);
    canvas.print("ESP-NOW disabled");
    canvas.setCursor(10, 80);
    canvas.print("LEDs off for stability");
    canvas.pushSprite(0, 0);
  });

  ArduinoOTA.onEnd([]() {
    if(DEBUG_SERIAL) Serial.println("\nOTA Update Complete!");
    
    // Success indication - solid green
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    
    // Show success on LCD
    canvas.fillSprite(TFT_BLACK);
    canvas.fillRect(0, 0, M5.Lcd.width(), 40, TFT_GREEN);
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(10, 10);
    canvas.print("OTA SUCCESS");
    canvas.setTextSize(1);
    canvas.setCursor(10, 50);
    canvas.print("Rebooting...");
    canvas.setCursor(10, 65);
    canvas.print("Will restart normally");
    canvas.pushSprite(0, 0);
    
    delay(2000); // Show success for 2 seconds before reboot
    // ESP will automatically restart after OTA completion
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    // Update progress every 2000ms to minimize interference with upload
    if (now - lastUpdate > 2000) {
      lastUpdate = now;
      
      unsigned int percent = (progress / (total / 100));
      if(DEBUG_SERIAL) {
        Serial.printf("OTA Progress: %u%% (%u/%u bytes)\n", percent, progress, total);
      }
      
      // NO LED updates during upload - keep them dark for maximum stability
      
      // Update LCD progress - simplified and less frequent
      canvas.fillSprite(TFT_BLACK);
      canvas.fillRect(0, 0, M5.Lcd.width(), 40, TFT_BLUE);
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_WHITE);
      canvas.setCursor(10, 10);
      canvas.print("OTA UPDATE");
      canvas.setTextSize(1);
      canvas.setCursor(10, 50);
      canvas.printf("Progress: %u%%", percent);
      
      // Progress bar on LCD
      int barWidth = M5.Lcd.width() - 20;
      int barProgress = (percent * barWidth) / 100;
      canvas.drawRect(10, 70, barWidth, 10, TFT_WHITE);
      canvas.fillRect(10, 70, barProgress, 10, TFT_WHITE);
      
      // Show status
      canvas.setCursor(10, 85);
      canvas.print("LEDs: OFF for stability");
      canvas.setCursor(10, 100);
      canvas.print("ESP-NOW: disabled");
      
      canvas.pushSprite(0, 0);
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    String errorMsg = "Unknown error";
    
    if(DEBUG_SERIAL) {
      Serial.printf("OTA Error[%u]: ", error);
      
      if (error == OTA_AUTH_ERROR) {
        errorMsg = "Auth Failed";
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        errorMsg = "Begin Failed";
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        errorMsg = "Connect Failed";
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        errorMsg = "Receive Failed";
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        errorMsg = "End Failed";
        Serial.println("End Failed");
      }
    }
    
    // Error indication - solid red
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    
    // Show error on LCD
    canvas.fillSprite(TFT_BLACK);
    canvas.fillRect(0, 0, M5.Lcd.width(), 40, TFT_RED);
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(10, 10);
    canvas.print("OTA ERROR");
    canvas.setTextSize(1);
    canvas.setCursor(10, 50);
    canvas.print(errorMsg);
    canvas.setCursor(10, 65);
    canvas.print("Will restart in 5s");
    canvas.pushSprite(0, 0);
    
    delay(5000); // Show error for 5 seconds
    
    // Restart the device to recover from OTA error
    if(DEBUG_SERIAL) Serial.println("Restarting after OTA error...");
    ESP.restart();
  });
}

bool isESPNowActive(){
  // Check if ESP-NOW is currently active
  // This is a simple check - in practice you might track this with a global variable
  return esp_now_is_peer_exist(NULL); // Returns true if any peers exist
}

void handleOTA(){
  // Only handle OTA if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    // Warn if ESP-NOW is still active (shouldn't happen with our changes)
    if (isESPNowActive() && DEBUG_SERIAL) {
      Serial.println("Warning: ESP-NOW still active during OTA handling");
    }
    ArduinoOTA.handle();
  }
}