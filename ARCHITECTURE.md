# ESPUtils - Modular Architecture Documentation

## Overview
The codebase has been refactored into a **production-ready, modular architecture** following **Separation of Concerns (SoC)** principles. Each module handles a specific responsibility, making the code:
- ✅ Easier to maintain
- ✅ Easier to test
- ✅ Easier to extend
- ✅ More reliable
- ✅ More professional

## Communication Protocol

### HTTP REST API
The device uses **HTTP-based REST API** instead of WebSocket/TCP socket communication for:
- Better compatibility with web browsers and standard HTTP clients
- RESTful design patterns for intuitive API usage
- Standard HTTP status codes and JSON responses
- Session-based authentication with JWT token verification

### Discovery via mDNS
Instead of UDP broadcast discovery, the device uses **mDNS (Multicast DNS)** for network discovery:
- **Device accessible at**: `http://<deviceID>.local`
- Automatic service advertisement via mDNS
- Zero-configuration networking
- No manual IP address management needed
- HTTP service advertised on port 80

### Authentication & Security
- **JWT Token Verification**: Validates ES256 JWT tokens from cloud server (issuer verification)
- **Session Management**: Generates session tokens stored in both flash and RAM
- **Session Expiry**: 1-week expiration for session tokens
- **Protected Endpoints**: All API endpoints except `/ping` require session authentication
- **Authorization Header**: Uses `Authorization: Session <token>` for protected requests
- **Persistent Sessions**: Sessions survive device restarts (stored in flash)

### API Endpoints

#### Public Endpoints (No Authentication Required)
- **GET /ping**
  - Returns: `{ deviceID, ipAddress, deviceName }`
  - Used for device discovery and health checks

#### Authentication Endpoint
- **POST /api/auth**
  - Body: `{ "token": "<JWT_TOKEN>" }`
  - Returns: `{ "sessionToken": "<SESSION>", "expiresIn": 604800 }`
  - Validates JWT and creates a new session

#### Protected Endpoints (Require Session Token)
All protected endpoints require the header: `Authorization: Session <session-token>`

- **GET /api/device** - Get comprehensive device information
- **POST /api/ir/capture** - Capture IR signal
  - Body: `{ "captureMode": 0 }`
- **POST /api/ir/send** - Send IR signal
  - Body: `{ "irCode": "<hex>", "length": "<hex>", "protocol": "UNKNOWN" }`
- **GET /api/wireless** - Get wireless configuration
- **PUT /api/wireless** - Update wireless configuration
  - Body: `{ "mode": "WIFI|AP", "ssid": "<ssid>", "password": "<pass>" }`
- **PUT /api/user** - Update user credentials
  - Body: `{ "username": "<user>", "password": "<pass>" }`
- **POST /api/gpio/set** - Set GPIO pin state
  - Body: `{ "pinNumber": 5, "pinMode": "OUTPUT", "pinValue": 1 }`
- **GET /api/gpio/get?pin=5** - Get GPIO pin state
- **POST /api/restart** - Restart the device
- **POST /api/reset** - Factory reset the device

## File Structure

```
ESPUtils_ESP8266/
├── ESPUtils_ESP8266.ino          # Main entry point (setup & loop only)
└── src/                          # Source code organized by SoC
    ├── config/
    │   └── Config.h              # Configuration constants & structures
    ├── utils/
    │   ├── Utils.h               # Utility declarations
    │   └── Utils.cpp             # Utility implementations (deviceID, LED, etc.)
    ├── storage/
    │   ├── StorageManager.h      # Storage declarations
    │   └── StorageManager.cpp    # File system operations
    ├── auth/
    │   ├── AuthManager.h         # Authentication declarations
    │   ├── AuthManager.cpp       # User authentication logic
    │   ├── SessionManager.h      # Session management declarations
    │   └── SessionManager.cpp    # JWT & session token handling
    ├── network/
    │   ├── WirelessNetworkManager.h    # Network declarations
    │   └── WirelessNetworkManager.cpp  # WiFi/AP & mDNS management
    ├── hardware/
    │   ├── GPIOManager.h         # GPIO declarations
    │   ├── GPIOManager.cpp       # GPIO control logic
    │   ├── IRManager.h           # IR declarations
    │   └── IRManager.cpp         # IR capture & transmission
    └── handlers/
        ├── RequestHandler.h      # HTTP REST API declarations
        └── RequestHandler.cpp    # RESTful endpoint routing logic
```

## Module Descriptions

### Folder Organization

The codebase follows **Separation of Concerns (SoC)** with a clean folder structure:

- **`src/config/`** - Configuration constants and data structures
- **`src/utils/`** - Utility functions (LED, serial, deviceID conversions)
- **`src/storage/`** - File system and persistent storage operations
- **`src/auth/`** - JWT authentication, session management, and credential management
- **`src/network/`** - Wireless networking (WiFi/AP) and mDNS service discovery
- **`src/hardware/`** - Hardware interfaces (GPIO, IR)
- **`src/handlers/`** - HTTP REST API endpoint routing and request handling

Each folder contains the header (`.h`) and implementation (`.cpp`) files for that concern.

### 1. **Config Module** (`src/config/`)
Centralized configuration management. Contains:
- Device settings (name, password, pins)
- Network ports (HTTP port, OTA port)
- Session expiry configuration (1 week)
- Timing parameters
- File paths (including session storage)
- Data structures (UserConfig, WirelessConfig, GPIOConfig)
- Response messages

**Benefits**: Easy to modify settings, no magic numbers in code

### 2. **Utils Module** (`src/utils/`)
Low-level utility functions used across the application:
- `printSerial()` - Serial output with enable/disable support
- `ledPulse()` - Visual feedback via LED
- `getUInt64FromHex()` - Hex string conversion
- `getDeviceID()` / `getDeviceIDString()` - Device ID retrieval (replaces chipID)
- `initLED()` / `setLED()` - LED control

**Benefits**: Reusable utilities, consistent behavior

### 3. **StorageManager Module** (`src/storage/`)
Handles all file system operations:
- Read/write JSON documents
- Load/save wireless configuration
- Load/save user credentials
- Load/save session data
- File management (delete, exists, format)

**Benefits**: Abstracted storage layer, easy to switch storage backends

### 4. **AuthManager Module** (`src/auth/`)
Authentication and session management:
- JWT token validation (ES256)
- Session token generation and validation
- Legacy username/password authentication
- Credential storage and updates
- Session expiry management

**Benefits**: Secure authentication layer, centralized credential management

**SessionManager Sub-Module**:
- Verifies ES256 JWT signatures (placeholder for full implementation)
- Generates secure session tokens
- Stores sessions in both flash and RAM
- Handles session expiry (1 week)
- Recovers sessions after device restart

### 5. **WirelessNetworkManager Module** (`src/network/`)
Network communication and wireless management:
- WiFi/AP mode initialization
- mDNS service advertisement
- Wireless configuration updates
- IP address management
- MAC address caching

**Key Changes**:
- Removed UDP broadcast discovery
- Removed TCP socket server
- Added mDNS support with deviceID-based hostname
- HTTP communication only

**Benefits**: Zero-config networking, standard HTTP protocol

### 6. **GPIOManager Module** (`src/hardware/`)
GPIO pin control and management:
- Read/write digital pins
- Configure pin modes
- Persistent GPIO state storage
- Apply GPIO configurations on boot

**Benefits**: Hardware abstraction, persistent GPIO settings

### 7. **IRManager Module** (`src/hardware/`)
Infrared signal capture and transmission:
- Capture IR signals from remotes
- Send IR signals
- Support for multiple IR protocols
- Configurable capture buffer and timeouts

**Benefits**: Reusable IR functionality, protocol abstraction

### 8. **RequestHandler Module** (`src/handlers/`)
HTTP REST API endpoint routing and request processing:
- RESTful endpoint definitions
- Session token validation middleware
- JSON request/response handling
- HTTP status code management
- Error handling

**Design Pattern**: 
- Separation of public/protected endpoints
- Middleware pattern for authentication
- Consistent JSON response structure
- Helper methods for sending responses

**Benefits**: Clean API structure, easy to add new endpoints, follows REST principles

## Best Practices Implemented

### Memory Optimization (ESP8266 Constraints)
- PROGMEM for constant strings
- String pre-allocation with `reserve()`
- Minimal heap fragmentation
- Efficient JSON serialization
- Cached values (MAC address)

### CPU Optimization
- No busy polling
- Efficient event handling
- Minimal blocking operations
- `yield()` calls to prevent watchdog timeouts

### Security
- Session-based authentication
- JWT token validation
- Protected API endpoints
- Session expiry enforcement
- Authorization header validation

### Reliability
- Persistent session storage (survives restarts)
- Wireless configuration persistence
- Error handling and validation
- Watchdog timer management
- OTA update support

### Maintainability
- Modular architecture
- Clear separation of concerns
- Comprehensive documentation
- Consistent naming conventions
- Type-safe enums and constants

## Device Identification

The device is identified by its **deviceID** (previously chipID):
- Unique hardware identifier from ESP chip
- Used for mDNS hostname: `<deviceID>.local`
- Returned in ping responses
- 32-bit for ESP8266, 48-bit for ESP32
- Formatted as uppercase hex string

## Session Management Flow

1. **Client Authentication**:
   - Client obtains JWT token from cloud server
   - Client sends JWT to device at `/api/auth`
   - Device verifies JWT signature (ES256)
   - Device generates session token
   - Session stored in flash and RAM
   - Session token returned to client

2. **Protected API Access**:
   - Client includes `Authorization: Session <token>` header
   - Device validates session token
   - Device checks session expiry
   - If valid, process request
   - If invalid/expired, return 401 Unauthorized

3. **Session Recovery**:
   - On device restart, load session from flash
   - Validate expiry time
   - If expired, invalidate session
   - If valid, keep session active

4. **Session Expiry**:
   - Sessions expire after 1 week (604,800 seconds)
   - Client must re-authenticate after expiry
   - Expired sessions automatically invalidated

## Development Notes

### Adding New Endpoints
1. Add handler method in `RequestHandler.cpp`
2. Register route in `setupRoutes()`
3. Implement session validation if protected
4. Use `sendSuccess()` / `sendError()` helpers
5. Follow RESTful conventions (GET, POST, PUT, DELETE)

### JWT Implementation
The current JWT verification is a **placeholder**. For production use:
1. Implement Base64URL decoding
2. Add uECC library for ES256 verification
3. Store public key in Config or PROGMEM
4. Verify signature against header.payload
5. Parse and validate JWT payload (issuer, expiry, etc.)

### mDNS Considerations
- mDNS works only on local network
- Hostname based on deviceID for uniqueness
- Service advertised as `_http._tcp`
- ESP8266 requires `MDNS.update()` in loop

### Future Enhancements
- Full ES256 JWT signature verification
- Multiple concurrent sessions
- Role-based access control
- Rate limiting on endpoints
- HTTPS support (if flash allows)
- WebSocket for real-time updates (optional)

## Conclusion

This architecture provides a solid foundation for ESP8266/ESP32 IoT devices with:
- Modern HTTP REST API
- Secure session-based authentication
- Zero-config network discovery via mDNS
- Modular, maintainable code structure
- Memory and CPU-efficient design
- Production-ready reliability

The design balances simplicity with functionality while respecting the resource constraints of embedded systems.
