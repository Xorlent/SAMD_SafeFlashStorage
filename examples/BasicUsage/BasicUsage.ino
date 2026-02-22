/*
  FlashStorage Safe Basic Usage Example
  
  Demonstrates proper use of the FlashStorage library including:
  - Error checking on read/write operations
  - Handling uninitialized or corrupted flash data
  - Initializing with default values
  - Automatic write optimization
  
  This example stores a configuration structure in flash memory that
  persists across device power cycles.
  
  Compatible with: SAMD21 (M0/M0+) and SAMD51 (M4) boards
*/

#include <SAMD_SafeFlashStorage.h>

// Defines an example configuration structure we want to persist across reboots
typedef struct {
  uint32_t bootCount;      // Number of times device has booted
  uint16_t sensorInterval; // Sensor reading interval in seconds
  bool enableLogging;      // Enable/disable logging
  float calibrationValue;  // Sensor calibration constant
} Configuration;

// Create a flash storage instance for our configuration
// The name "configStore" and type Configuration are used to generate
// a unique hash to ensure only valid data is read back
FlashStorage(configStore, Configuration);

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 5000);  // Wait for serial
  
  Serial.println("=================================\n");
  Serial.println("FlashStorage Basic Usage Example");
  Serial.println("=================================\n");
  
  // Attempt to read existing configuration from flash
  Configuration config;
  bool validData = configStore.read(&config);
  
  if (validData) {
    // Valid data found - configuration was previously saved
    Serial.println("Valid configuration found in flash:");
    Serial.print("Boot count: ");
    Serial.println(config.bootCount);
    Serial.print("Sensor interval: ");
    Serial.print(config.sensorInterval);
    Serial.println(" seconds");
    Serial.print("Logging enabled: ");
    Serial.println(config.enableLogging ? "Yes" : "No");
    Serial.print("Calibration value: ");
    Serial.println(config.calibrationValue, 4);
    
    // Increment boot counter
    config.bootCount++;
    Serial.print("\n→ Incrementing boot count to ");
    Serial.println(config.bootCount);
    
  } else {
    // No valid data - first run, corrupted data, or structure changed
    Serial.println("No valid configuration found in flash");
    Serial.println("This can happen due to:");
    Serial.println("- First run after code upload");
    Serial.println("- Configuration data structure modified");
    Serial.println("- Data corruption (unlikely)");
    Serial.println("\n...Initializing with default values...");
    
    // Initialize configuration struct with default values
    config.bootCount = 1;
    config.sensorInterval = 60;  // 60 seconds
    config.enableLogging = true;
    config.calibrationValue = 1.0;
  }
  
  // Write configuration back to flash
  // Library skips write if data hasn't changed to preserve flash endurance
  Serial.println("\nWriting configuration to flash...");
  
  if (configStore.write(config)) {
    Serial.println("Configuration saved successfully");
  } else {
    Serial.println("ERROR: Failed to write configuration");
    Serial.println("Possible causes:");
    Serial.println("- Flash memory bounds exceeded");
    Serial.println("- Hardware flash controller error");
  }
  
  // Demonstrate read-back verification
  Serial.println("\nVerifying saved configuration...");
  Configuration verified;
  if (configStore.read(&verified)) {
    if (verified.bootCount == config.bootCount &&
        verified.sensorInterval == config.sensorInterval &&
        verified.enableLogging == config.enableLogging &&
        verified.calibrationValue == config.calibrationValue) {
      Serial.println("Verification successful - data integrity confirmed");
    } else {
      Serial.println("WARNING: Verification failed - data mismatch");
    }
  } else {
    Serial.println("ERROR: Failed to read back configuration");
  }
  
  // Example: Demonstrate updating a single value
  Serial.println("\n--- Flash Write Performance Test ---");
  
  // Toggle sensor interval to ensure we're making an actual change
  uint16_t newInterval = (config.sensorInterval == 60) ? 120 : 60;
  Serial.print("Changing sensor interval to ");
  Serial.print(newInterval);
  Serial.println(" seconds...");
  config.sensorInterval = newInterval;
  
  // Test 1: Measure time for actual write (data changed - will erase+write)
  Serial.println("\nTest 1: Write with changed data (forces erase+write)");
  unsigned long firstWriteStart = micros();
  bool firstWriteResult = configStore.write(config);
  unsigned long firstWriteTime = micros() - firstWriteStart;
  
  if (firstWriteResult) {
    Serial.println("Write succeeded");
    Serial.print("  Time taken: ");
    Serial.print(firstWriteTime);
    Serial.println(" µs");
  } else {
    Serial.println("ERROR: Failed to update configuration");
    return;  // Can't continue test
  }
  
  // Test 2: Write same data again (should be optimized away)
  Serial.println("\nTest 2: Write with identical data (should skip erase+write)");
  unsigned long secondWriteStart = micros();
  bool secondWriteResult = configStore.write(config);
  unsigned long secondWriteTime = micros() - secondWriteStart;
  
  if (secondWriteResult) {
    Serial.println("Write succeeded");
    Serial.print("  Time taken: ");
    Serial.print(secondWriteTime);
    Serial.println(" µs");
    
    // Compare the two write times
    Serial.println("\n--- Performance Comparison ---");
    Serial.print("First write (actual):    ");
    Serial.print(firstWriteTime);
    Serial.println(" µs");
    Serial.print("Second write (skipped):  ");
    Serial.print(secondWriteTime);
    Serial.println(" µs");
    Serial.print("Speedup:                 ");
    Serial.print((float)firstWriteTime / (float)secondWriteTime, 1);
    Serial.println("x faster");
    
    if (secondWriteTime < firstWriteTime / 3) {
      Serial.println("\nOPTIMIZATION CONFIRMED:");
      Serial.println("Second write was significantly faster, indicating the");
      Serial.println("library detected identical data and skipped flash erase+write.");
    } else {
      Serial.println("\nWarning: Both writes took similar time.");
      Serial.println("Optimization may not be working as expected.");
    }
  } else {
    Serial.println("ERROR: Failed second write");
  }
  
  Serial.println("\n=================================");
  Serial.println("EXAMPLE COMPLETE");
  Serial.println("\nReset the board to see boot count increment.");
}

void loop() {
  // In a real application, you might save configuration changes when required,
  // but avoid writing too frequently to preserve flash (only when necessary).
}