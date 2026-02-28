# SAMD Safe FlashStorage Library for Arduino

An enhanced version of the original cmaglie/FlashStorage library, this is a safe version of the library for storing data structures in flash memory on SAMD21 and SAMD51 microcontrollers. Data persists across power cycles and resets, making it ideal for configuration storage, calibration values, and application state.

## Features

- **Type-safe storage** - Store any C/C++ struct or built-in types (int, float, bool, char, etc)
- **Automatic data validation** - Built-in checksums and data validity checking
- **Write optimization** - Skips no-change writes to preserve flash endurance
- **Error handling** - Comprehensive transaction validation with return values
- **Corruption detection** - Detects uninitialized, corrupted, or incompatible data
- **Easy to use** - Simple read/write API with one-line declarations
- **Flash-friendly** - Respects hardware alignment and erase boundaries

## Why This Library?

This is an enhanced version of the original FlashStorage library with safety improvements:

- **Comprehensive bounds checking** to prevent memory corruption
- **Prevent integer overflow** in address calculations
- **Fixed SAMD51 cache handling** per device errata
- **Enhanced error handling** to ensure safe use

## Compatibility

- SAMD21 and SAMD51-based boards

## Installation

### Arduino IDE

1. Download this repository as a ZIP file
2. In Arduino IDE: **Sketch** → **Include Library** → **Add .ZIP Library...**
3. Select the downloaded ZIP file
4. Restart Arduino IDE

### Manual Installation

1. Clone or download this repository
2. Copy the `flashstorage` folder to your Arduino libraries directory:
   - **macOS**: `~/Documents/Arduino/libraries/`
   - **Windows**: `C:\Users\[username]\Documents\Arduino\libraries\`
   - **Linux**: `~/Arduino/libraries/`
3. Restart Arduino IDE

## Basic Usage

```cpp
#include "SAMD_SafeFlashStorage.h"

// Create a flash storage instance for an int
FlashStorage(bootCounter, int);

void setup() {
  Serial.begin(9600);
  
  int count;
  
  // Read from flash (returns false if no valid data)
  if (bootCounter.read(&count)) {
    // Valid data found - increment it
    count++;
    Serial.print("Boot count: ");
    Serial.println(count);
  } else {
    // No valid data - this is the first run
    count = 1;
    Serial.println("First boot!");
  }
  
  // Write to flash
  bootCounter.write(count);
}
```

## Complete Example

See [examples/BasicUsage/BasicUsage.ino](examples/BasicUsage/BasicUsage.ino) for a complete working example that demonstrates:

- Reading existing configuration or initializing defaults
- Writing configuration to flash
- Verifying data integrity
- No-change write optimization
- Persistence via a "Boot" counter

## API Reference

### Creating a Storage Instance

```cpp
FlashStorage(name, DataType);
```

- **name**: Unique identifier for this storage (must be unique across your sketch)
- **DataType**: Your struct or POD type (plain old data - no pointers, virtual functions, or dynamic types)

### Writing Data

```cpp
bool write(DataType data);
```

Writes data to flash memory. Returns `true` on success, `false` on error.

**Note:** Automatically skips write if data hasn't changed to preserve flash.

**Example:**
```cpp
int myValue = 42;
if (myStorage.write(myValue)) {
  Serial.println("Saved!");
} else {
  Serial.println("Write error.");
}
```

### Reading Data

```cpp
bool read(DataType *data);
```

Reads data from flash memory. Returns `true` if valid data found, `false` if uninitialized/corrupted.

**Example:**
```cpp
int myValue;
if (myStorage.read(&myValue)) {
  // Valid data found - use myValue
  Serial.println(myValue);
} else {
  // No valid data - we must manually set a default
  // (the pointer version doesn't do this automatically)
  myValue = 0;
}
```

### Alternative Read (with default)

```cpp
DataType read();
```

Returns the stored value, or a default value if read fails.

**What happens on failure:**
- `int` returns `0`
- `float` returns `0.0`
- `bool` returns `false`
- Structs return with all fields set to zero

**Important:** This version doesn't tell you if the read failed. Use the pointer version `read(&variable)` if you need to know whether valid data was found.

**Example:**
```cpp
int myValue = myStorage.read();
// If read failed, myValue will be 0
// If read succeeded, myValue will contain the stored value
// You can't tell which happened!
```

## Best Practices

### 1. Always Check Return Values

```cpp
// Good
if (configStore.write(config)) {
  Serial.println("Saved successfully");
} else {
  Serial.println("Save failed!");
}
```

### 2. Initialize Defaults When Read Fails

```cpp
Configuration config;
if (!configStore.read(&config)) {
  // First run or corrupted data - set defaults
  config.bootCount = 1;
  config.sensorInterval = 60;
  config.enableLogging = true;
  config.calibrationValue = 1.0;
}
```

### 3. Avoid Frequent Writes

Flash memory has limited write cycles (~10,000-100,000 erase cycles per block)

```cpp
// ***** BAD ***** - Writes constantly:
void loop() {
  config.counter++;
  configStore.write(config);  // This will eventually wear out device flash
  delay(100);
}
```

**Note:** The library automatically skips writes when data hasn't changed.  Regardless, avoid calling `write()` unnecessarily.

### 4. Avoid Pointers and Dynamic Types

```cpp
// ***** GOOD ***** - Plain data types only:
typedef struct {
  uint32_t id;
  float values[10];     // Fixed-size arrays OK
  char name[32];        // Fixed-size strings OK
} ValidConfig;

// ***** BAD ***** - Dynamic/complex types:
typedef struct {
  String name;          // Dynamic allocation - DON'T USE
  float *values;        // Pointer - DON'T USE
  std::vector<int> v;   // STL containers - DON'T USE
} InvalidConfig;
```

### 7. Thread/Interrupt Safety

Flash operations are **NOT** interrupt-safe:

```cpp
// If writing from multiple contexts, protect access:
void saveConfig() {
  noInterrupts();
  configStore.write(config);
  interrupts();
}

// Never call from ISR:
void ISR_handler() {
  configStore.write(config);  // DON'T DO THIS!
}
```

## Memory Considerations

### Flash Allocation

The library allocates flash memory aligned to page boundaries. Page size varies based on total device NVS.  The following are typical values for most development boards:

- **Typical SAMD21**: 256-byte pages
- **Typical SAMD51**: 8192-byte pages

A small struct like:
```cpp
struct Config {
  uint32_t value1;  // 4 bytes
  uint16_t value2;  // 2 bytes
};  // 6 bytes + 4 bytes overhead = 10 bytes actual
```

Will allocate at least one full page.  This is a hardware limitation, as flash can only be erased in full pages.

### Structure Size Limits

- Maximum practical size: **~8KB per structure**
- Multiple small structures are more efficient than one large structure
- Padding/alignment may increase actual size

Check your structure size:
```cpp
Serial.print("Config size: ");
Serial.println(sizeof(Configuration));
```

## Troubleshooting

### "FlashStorage library only supports SAMD microcontrollers"

**Cause:** Trying to compile for an unsupported platform (AVR, ESP32, etc.)

**Solution:** This library only works on SAMD21 and SAMD51 boards.

### Write/Read Returns False

**Possible causes:**
1. **First time running** - No data written yet (normal)
3. **Bounds check failure** - Very large structures
4. **Flash corruption** - Rare hardware issue

**Solutions:**
- Always initialize defaults when `read()` returns false
- After changing struct, expect old data to be invalid
- Check structure size with `sizeof()`

### Data Not Persisting

**Possible causes:**
1. **Write not called** - Check if `write()` is actually executing
2. **Upload erases flash** - Some bootloaders may erase all user NVS pages
3. **Power loss during write** - Flash write interrupted

**Solutions:**
- Verify `write()` returns `true`
- Check serial output for write confirmation
- Ensure stable power during writes

## Advanced Topics

### Multiple Storage Instances

You can store multiple independent structures:

```cpp
typedef struct {
  uint8_t brightness;
  uint8_t volume;
} UserSettings;

typedef struct {
  float offset;
  float scale;
} SensorCalibration;

FlashStorage(settings, UserSettings);
FlashStorage(calibration, SensorCalibration);

void setup() {
  UserSettings s;
  SensorCalibration c;
  
  // Always check if reads succeeded
  if (!settings.read(&s)) {
    // No valid data - initialize defaults
    s.brightness = 128;
    s.volume = 50;
  }
  
  if (!calibration.read(&c)) {
    // No valid data - initialize defaults
    c.offset = 0.0;
    c.scale = 1.0;
  }
  
  // Each has independent storage and validation
}
```

### Data Validation

The library automatically validates:
- **Structure identity** - Hash of name + size
- **Data integrity** - Checksum of data bytes

This protects against:
- Reading wrong variable
- Structure size changes
- Bit corruption
- Uninitialized flash

### Limitations

**Hash Collisions:** The library uses a 16-bit hash to identify FlashStorage instances.  With numerous instances in a project (unlikely), hash collisions could occur.  To minimize this risk:
- Use unique, descriptive variable names
- Limit the total number of FlashStorage instances per project (<10 is best)

## Performance

### Write Performance

Typical write times (erasing + writing ~256 bytes on SAMD21):
- **First write (erase+write)**: 500-2000 µs
- **Optimized write (unchanged data)**: 20-100 µs

## Credits

- This library is based on the original FlashStorage library: Arduino LLC / Cristian Maglie
