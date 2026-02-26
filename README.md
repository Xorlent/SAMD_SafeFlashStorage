# Minimal RGB LED Library for the WS2812B

The simplest, most lightweight WS2812B RGB LED library for Arduino-ESP32 version 3.3 and higher.  For single RGB control, this library **saves over 76KB of program space** compared to a very popular RGB LED control library.

## Features

- **Ultra-minimal** - Uses only ESP32's hardware RMT peripheral and standard C libraries
- **Simple API** - Just two functions: `begin()` and `set()`
- **Hardware-based timing** - No CPU overhead for bit timing
- **8 predefined colors** - Common colors built-in

## Hardware Connection

Connect your WS2812B LED to your ESP32:

```
WS2812B Pin  →  ESP32
-----------     -----
VDD (5V)     →  5V
VSS (GND)    →  GND
DIN          →  Any GPIO pin (e.g., GPIO 35)
```

## Installation

1. Download or clone this repository
2. Move the `ESP32_WS2812B` folder to your Arduino libraries folder:
   - **macOS**: `~/Documents/Arduino/libraries/`
   - **Windows**: `Documents\Arduino\libraries\`
   - **Linux**: `~/Arduino/libraries/`
3. Restart Arduino IDE

## Usage

```cpp
#include "WS2812B.h"

#define LED_PIN 5  // GPIO pin for WS2812B data (DIN)

WS2812B led;

void setup() {
  // Initialize with GPIO pin number
  led.begin(LED_PIN);
}

void loop() {
  // Flash red with a 1 second interval
  led.set("red", 255); // brightness value optional (default 255)
  delay(1000);
  led.set("black");
  delay(1000);
}
```

## API Reference

### `bool begin(uint8_t pin)`

Initialize the WS2812B controller on the specified GPIO pin.

- **Parameters**: `pin` - GPIO pin number (e.g., 5)
- **Returns**: `true` on success, `false` on failure

### `void set(const char* color, uint8_t brightness = 255)`

Set the LED color and brightness.

- **Parameters**:
  - `color` - Color name (see below)
  - `brightness` - Brightness level from 1-255 (255 = full brightness)
    - Ignored for "black" (always off)
- **Returns**: None

### Supported Colors

| Color String | RGB Values | Alternative |
|-------------|------------|-------------|
| `"black"`   | Off        | -           |
| `"white"`   | 255,255,255| -           |
| `"red"`     | 255,0,0    | `"R"`       |
| `"green"`   | 0,255,0    | `"G"`       |
| `"blue"`    | 0,0,255    | `"B"`       |
| `"purple"`  | 128,0,128  | -           |
| `"yellow"`  | 255,255,0  | -           |
| `"orange"`  | 255,165,0  | -           |

'
## Thread Safety

This library is designed for **single-threaded use** (typical Arduino `setup()`/`loop()` pattern). If you need to control the LED from multiple FreeRTOS tasks, protect access with a mutex:

```cpp
#include "WS2812B.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

WS2812B led;
SemaphoreHandle_t ledMutex;

void setup() {
  led.begin(5);
  ledMutex = xSemaphoreCreateMutex();
}

void task1(void* param) {
  while(1) {
    xSemaphoreTake(ledMutex, portMAX_DELAY);
    led.set("red", 255);
    xSemaphoreGive(ledMutex);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void task2(void* param) {
  while(1) {
    xSemaphoreTake(ledMutex, portMAX_DELAY);
    led.set("blue", 128);
    xSemaphoreGive(ledMutex);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
```

## Requirements

- **ESP32 board** (any variant with the RMT peripheral)
- **Arduino-ESP32 core 3.3 or higher**
- **Single WS2812B LED**

## Troubleshooting

**LED doesn't light up:**
- Check power connections (WS2812B needs both 5V and GND)
- Verify data pin connection
- Try a different GPIO pin (avoid strapping pins like GPIO 0, 2, 12, 15)

**LED shows wrong colors:**
- Some WS2812B variants use RGB instead of GRB - check your LED's datasheet
Simply change the order of the g, r, b in WS2812B.cpp here:
```cpp
    // WS2812B expects GRB order
    uint8_t led_data[3] = {g, r, b};
```

**Compilation errors:**
- Ensure the Arduino-ESP32 board library is version 3.3 or higher

## License

This library is released under the MIT License. See the LICENSE file for details.
