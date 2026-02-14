# ESPUtils - Modular Architecture Documentation

## Overview
The codebase has been refactored into a **production-ready, modular architecture** following **Separation of Concerns (SoC)** principles. Each module handles a specific responsibility, making the code:
- ✅ Easier to maintain
- ✅ Easier to test
- ✅ Easier to extend
- ✅ More reliable
- ✅ More professional

## File Structure

```
ESPUtils_ESP8266/
├── ESPUtils_ESP8266.ino          # Main entry point (setup & loop only)
└── src/                          # Source code organized by SoC
    ├── config/
    │   └── Config.h              # Configuration constants & structures
    ├── utils/
    │   ├── Utils.h               # Utility declarations
    │   └── Utils.cpp             # Utility implementations
    ├── storage/
    │   ├── StorageManager.h      # Storage declarations
    │   └── StorageManager.cpp    # File system operations
    ├── auth/
    │   ├── AuthManager.h         # Authentication declarations
    │   └── AuthManager.cpp       # User authentication logic
    ├── network/
    │   ├── WirelessNetworkManager.h    # Network declarations
    │   └── WirelessNetworkManager.cpp  # WiFi/AP & TCP/UDP
    ├── hardware/
    │   ├── GPIOManager.h         # GPIO declarations
    │   ├── GPIOManager.cpp       # GPIO control logic
    │   ├── IRManager.h           # IR declarations
    │   └── IRManager.cpp         # IR capture & transmission
    └── handlers/
        ├── RequestHandler.h      # Request handler declarations
        └── RequestHandler.cpp    # Request routing logic
```

## Module Descriptions

### Folder Organization

The codebase follows **Separation of Concerns (SoC)** with a clean folder structure:

- **`src/config/`** - Configuration constants and data structures
- **`src/utils/`** - Utility functions (LED, serial, conversions)
- **`src/storage/`** - File system and persistent storage operations
- **`src/auth/`** - User authentication and credential management
- **`src/network/`** - Wireless networking (WiFi/AP) and communication
- **`src/hardware/`** - Hardware interfaces (GPIO, IR)
- **`src/handlers/`** - Request parsing and command routing

Each folder contains the header (`.h`) and implementation (`.cpp`) files for that concern.

### 1. **Config Module** (`src/config/`)
Centralized configuration management. Contains:
- Device settings (name, password, pins)
- Network ports
- Timing parameters
- File paths
- Data structures (UserConfig, WirelessConfig, GPIOConfig)
- Response messages

**Benefits**: Easy to modify settings, no magic numbers in code

### 2. **Utils Module** (`src/utils/`)
Low-level utility functions used across the application:
- `printSerial()` - Serial output with enable/disable support
- `ledPulse()` - Visual feedback via LED
- `getUInt64FromHex()` - Hex string conversion
- `initLED()` / `setLED()` - LED control

**Benefits**: Reusable utilities, consistent behavior

### 3. **StorageManager Module** (`src/storage/`)
Handles all file system operations:
- Read/write JSON documents
- Load/save wireless configuration
- Load/save user credentials
- File management (delete, exists, format)

**Benefits**: Abstracted storage layer, easy to switch storage backends

### 4. **AuthManager Module** (`src/auth/`)
User authentication and credential management:
- Authenticate users
- Update credentials
- Reset to default
- Persistent credential storage

**Benefits**: Secure authentication layer, centralized credential management

### 5. **WirelessNetworkManager Module** (`src/network/`)
Network communication and wireless management:
- WiFi/AP mode initialization
- TCP socket server
- UDP datagram handling
- Wireless configuration updates
- Connection status monitoring

**Benefits**: Clean networking abstraction, easy to extend protocols

### 6. **GPIOManager Module** (`src/hardware/`)
GPIO pin control and configuration:
- Apply GPIO settings
- Get GPIO status
- Persistent pin configuration
- Factory reset detection

**Benefits**: Hardware abstraction, configuration persistence

### 7. **IRManager Module** (`src/hardware/`)
Infrared remote control functionality:
- IR signal capture
- IR signal transmission
- Support for multiple protocols
- Raw data handling

**Benefits**: Clean IR interface, protocol abstraction

### 8. **RequestHandler Module** (`src/handlers/`)
Request routing and command handling:
- Parse incoming requests
- Route to appropriate handlers
- Authentication verification
- Response generation

**Benefits**: Single entry point for all commands, easy to add new commands

### 9. **ESPUtils_ESP8266.ino**
Main application file - clean and minimal:
- `setup()` - Initialize all modules
- `setupOTA()` - Configure OTA updates
- `loop()` - Event processing

**Benefits**: Clear application flow, easy to understand

## Key Improvements

### Before Refactoring ❌
- 917 lines in single monolithic file
- Mixed concerns throughout
- Hardcoded constants
- Difficult to test
- Hard to maintain
- Poor code organization

### After Refactoring ✅
- Modular design (~150 lines per module)
- Clear separation of concerns
- Centralized configuration
- Easy to test individual modules
- Professional code structure
- Scalable architecture

## Design Patterns Used

1. **Separation of Concerns**: Each module has a single, well-defined responsibility
2. **Static Classes**: Stateless utility classes with static methods
3. **Namespace Organization**: Config and ResponseMsg namespaces
4. **Dependency Injection**: Modules receive dependencies as parameters
5. **Single Responsibility Principle**: Each class/module does one thing well

## How to Add New Features

### Adding a New Command
1. Add handler method in `RequestHandler.cpp`
2. Add route in `handleRequest()` method
3. Implement business logic using existing modules

### Adding New Hardware
1. Add pin configuration to `Config.h`
2. Create new manager module if needed (e.g., SensorManager)
3. Initialize in `setup()` function

### Changing Configuration
1. Edit values in `Config.h`
2. No code changes needed elsewhere

## Code Quality Improvements

- ✅ Const correctness
- ✅ Proper error handling
- ✅ Documentation comments
- ✅ Consistent naming conventions
- ✅ Memory management
- ✅ No code duplication
- ✅ Type safety
- ✅ Namespace usage

## Building the Project

This project requires the same libraries as before:
- ArduinoJson 7.x
- IRremoteESP8266 2.8.4+
- ESP8266/ESP32 board support

All `.cpp` and `.h` files will be automatically compiled by Arduino IDE when building the `.ino` file.

## Migration Notes

The refactored code maintains **full backward compatibility** with the existing Android application and API. All request/response formats remain identical.

## Performance

- **Same memory footprint**: Modular code compiles to similar binary size
- **No runtime overhead**: Static classes with inline optimizations
- **Better maintainability**: Worth any minimal overhead (if any)

## Testing

Individual modules can now be tested separately:
```cpp
// Example: Test StorageManager
WirelessConfig config;
bool success = StorageManager::loadWirelessConfig(config);
```

## Future Enhancements

With this modular architecture, you can easily add:
- MQTT support (new MQTTManager module)
- Web interface (new WebServerManager module)
- Sensor integration (new SensorManager module)
- Logging system (extend Utils module)
- Configuration web portal
- REST API layer

---

**Version**: 2.0.0 (Production-Ready Modular Architecture)
**Author**: Refactored for production use
**Date**: February 2026
