# NeoPixel Controller System

A sophisticated distributed LED art installation system using M5StickC Plus2 microcontrollers to create synchronized light shows across multiple nodes. Each node controls 334 WS2812B NeoPixel LEDs with advanced networking, audio reactivity, and individual user control.

## Current Status: Version 1.0.x (Auto-Incrementing)

**Last Updated**: January 2025  
**Active Nodes**: ~10 devices  
**Network**: Works with or without WiFi (ESP-NOW mesh priority)

## System Architecture

### Hardware Configuration
- **Controllers**: M5StickC Plus2 ESP32 devices
- **LEDs**: 334 WS2812B NeoPixels per node (`NUM_LEDS = 334`)
- **Communication**: ESP-NOW for pattern synchronization + WiFi for OTA updates
- **Audio**: Built-in microphone for beat detection and music reactivity
- **Power**: Each node independently powered
- **Network**: Multi-WiFi support (Barn, GMA-WIFI_Access_Point, phone hotspot)

### Distributed Leadership Model
- **AUTO Mode**: Nodes automatically elect a leader using MAC-based token algorithm
- **Leader Node**: Generates patterns, detects audio, broadcasts synchronized data at full brightness
- **Follower Nodes**: Receive and display leader's patterns with perfect synchronization
- **Automatic Failover**: If leader goes offline, remaining nodes elect new leader
- **Conflict Resolution**: Higher MAC-based tokens automatically become leader
- **Offline Operation**: ESP-NOW mesh works perfectly without WiFi

### Individual Brightness Control
- **Local Control**: Each node controls its own brightness independently
- **5 Brightness Levels**: 12%, 25% (default), 50%, 75%, 100%
- **Music Synchronization**: Leader's audio reactivity baked into LED colors, then each node applies local brightness
- **OFF Mode**: Complete sleep with dimmed display for battery savings

## User Interface

### Button Controls
- **Button A Short Press**: Cycle through brightness levels (LOCAL 12% → 25% → 50% → 75% → 100%)
- **Button A Long Press (2s)**: Enter/exit OFF mode
- **Button B**: Pattern freeze/advance (only works when node is leader)

### LCD Display
- **Color-coded Status**: Orange (Leader), Green (Follower), Purple (Election), Black (OFF)
- **Brightness Display**: Shows current local brightness level (e.g., "LOCAL 50%")
- **Pattern Info**: Current pattern name or "Following Leader"
- **Connection Status**: ".236" (WiFi IP) or "LOCAL" (offline mode) in bottom right
- **Version Display**: "v1.0.5" in bottom left corner
- **Freeze Indicator**: [F] shown when patterns are frozen

## Pattern System

### 22 LED Patterns
**Basic**: Rainbow, Chase, Juggle, Rainbow+Glitter, Confetti, BPM, Fire, Color Wheel, Random  
**Advanced**: Pulse Wave, Meteor Shower, Color Spiral, Plasma Field, Sparkle Storm, Aurora Waves  
**Organic**: Organic Flow, Wave Collapse, Color Drift, Liquid Rainbow, Sine Breath, Fractal Noise, Rainbow Strobe

### Pattern Generation
- **Leader Only**: Generates patterns with audio reactivity baked into LED colors
- **Music Mode**: When audio detected, `effectMusic()` scales LED colors by `musicLevel` (dramatic nearly-off to full-bright response)
- **Background Mode**: Normal patterns when no audio detected
- **Timed Advance**: Automatic pattern cycling every few seconds (configurable)
- **Full Brightness Broadcast**: Leader sends 100% brightness data, each node applies local scaling

## Audio Reactivity

### Music Detection (Leader Only)
- **BPM Analysis**: 5-second rolling window for beat detection
- **Threshold Detection**: `musicLevel > 0.6f` triggers beat events
- **Adaptive Levels**: Moving averages for sound min/max provide full dynamic range
- **Synchronized Response**: All nodes react identically to leader's audio analysis

### Implementation
- **Color Scaling**: Music reactivity applied via `leds[i].nscale8(musicScale)` in `effectMusic()`
- **Network Distribution**: Music-scaled colors transmitted at full brightness
- **Local Brightness**: Each node applies its brightness percentage to received data
- **Dramatic Response**: Audio scaling ranges from ~12% (quiet) to 100% (loud beats)

## Networking Protocol

### ESP-NOW Communication (Primary)
- **Message Types**: RAW data (0x00), Token broadcasts (0x01)
- **Chunked Transmission**: LED data split into 75-LED chunks for reliability
- **Token System**: MAC-based tokens for leader election and heartbeats
- **Robust Failover**: 3-strike timeout system with automatic re-election
- **Offline Priority**: Works perfectly without WiFi, mesh-first design

### WiFi Management (Secondary - OTA Only)
- **Multi-Network Support**: Tries multiple WiFi networks automatically
- **Non-Blocking**: WiFi operations never interfere with ESP-NOW mesh
- **Background Scanning**: WiFi checked every 2 minutes, status every 10 seconds
- **Clean Transitions**: Handles WiFi connect/disconnect gracefully
- **OTA Ready**: When WiFi available, enables over-the-air updates

### Network Resilience
- **Heartbeat Monitoring**: 1.5-second timeout detection
- **State Recovery**: Automatic LED blanking and state reset on leadership changes
- **Chunk Validation**: Complete frame assembly before display
- **Health Monitoring**: System checks prevent stuck states

## Development & Deployment

### Modular Codebase
- **config.h**: Hardware/network configuration, debug controls
- **main.ino**: Setup and main loop coordination with version display
- **patterns.cpp/.h**: All 22 LED pattern implementations with proper brightness handling
- **networking.cpp/.h**: ESP-NOW communication, leader election, WiFi management
- **audio.cpp/.h**: Microphone processing and BPM detection
- **ui.cpp/.h**: LCD display and button handling with version display
- **ota.cpp/.h**: Over-the-air update functionality with ESP-NOW conflict resolution
- **version.h**: Auto-generated version information

### Smart Deployment System
- **nodes.txt File**: Maintains list of node IP addresses
- **Auto-Discovery**: Network scanning for responsive devices
- **Smart Caching**: Only recompiles when source files change
- **Version Management**: Auto-incrementing version numbers with each compile
- **Retry Logic**: Up to 3 attempts per failed node with detailed reporting
- **Parallel Deployment**: OTA updates to multiple nodes simultaneously
- **Compile-Only Mode**: Fast incremental builds with version bumping, no deployment needed

### Deployment Options
1. **Deploy** (compile if needed + upload with retries) [DEFAULT]
2. **Force recompile and deploy** (clean cache + full compile + upload)
3. **Upload only** (skip compile, use existing build)
4. **Compile only** (increment version + fast build, no upload)
5. **Clean cache** (remove all cached builds)

## Debug Controls
```cpp
#define DEBUG_SERIAL    1     // Essential messages only
#define DEBUG_HEARTBEAT 0     // Disable heartbeat spam
#define DEBUG_BPM       0     // Disable BPM detection spam
```

## Current Node Configuration
- **Active Nodes**: ~10 devices on network 192.168.0.x
- **Known IPs**: 140, 141, 148, 150, 180, 113, 122, 236, 238, etc.
- **OTA Password**: "neopixel123"
- **Default Brightness**: 25% (64/255) per node
- **WiFi Networks**: Barn, GMA-WIFI_Access_Point, phone hotspot

## Key Features Working ✅
- ✅ Distributed leader election with automatic failover
- ✅ 22 diverse LED patterns with smooth transitions
- ✅ Synchronized music reactivity across all nodes (dramatic response restored)
- ✅ Individual brightness control (12%-100% + OFF)
- ✅ Robust ESP-NOW networking with chunk validation
- ✅ OTA deployment system with retry logic and version management
- ✅ Audio-responsive brightness scaling with full dynamic range
- ✅ Comprehensive LCD UI with status indicators and version display
- ✅ Watchdog timer and error recovery systems
- ✅ Multi-WiFi support with clean offline operation
- ✅ Fast incremental compilation with smart caching

## Recent Major Fixes
- **Offline Operation**: ESP-NOW mesh works perfectly without WiFi, no blocking delays
- **Audio Response**: Restored dramatic nearly-off to full-bright music reactivity
- **WiFi Transitions**: Clean handling of WiFi connect/disconnect without ESP-NOW interference
- **Deployment Reliability**: Retry logic for failed uploads, better leader node OTA handling
- **Version Management**: Auto-incrementing version display on all nodes
- **Build Performance**: Smart caching for fast incremental compiles

## Usage Workflow

### Development
```bash
# Fast development iteration
./deploy_nodes.sh    # Choose option 4: Compile only
# → Fast incremental build, version bump, ready for deployment

# When ready to deploy
./deploy_nodes.sh    # Choose option 3: Upload only  
# → Uses cached build, deploys with retry logic
```

### Operation
1. **Power On**: Nodes connect to WiFi (if available) and enter AUTO mode at 25% brightness
2. **Auto-Sync**: Nodes elect leader and begin synchronized pattern display
3. **User Control**: Button A cycles brightness, Button B controls patterns (leader only) 
4. **Music Mode**: Leader's microphone drives synchronized audio reactivity across all nodes
5. **Offline Ready**: Full functionality without WiFi, mesh network self-manages

## System Requirements
- **Arduino IDE** with ESP32 support
- **M5StickC Plus2** board definitions
- **FastLED library** (3.10.1+)
- **M5Unified library** (0.2.7+)
- **ESP32 Arduino Core** (3.2.1+)

## Network Architecture
The system prioritizes ESP-NOW mesh networking for real-time LED synchronization, with WiFi as an optional secondary network for OTA updates and remote management. This ensures perfect performance whether in your shop with WiFi or out in the woods at Burning Man.

**Local mesh network handles**: Pattern synchronization, leader election, audio reactivity  
**WiFi network handles**: OTA updates, remote configuration, version management

This architecture provides the reliability of a local mesh with the convenience of WiFi connectivity when available.