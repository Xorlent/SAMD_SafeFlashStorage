/*
  Copyright (c) 2015 Arduino LLC.  All right reserved.
  Written by Cristian Maglie, additional contributions by Xorlent

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef SAMD_SAFEFLASHSTORAGE_H
#define SAMD_SAFEFLASHSTORAGE_H

#pragma once

#include <Arduino.h>

// Platform validation - only SAMD architecture is supported
#if !defined(ARDUINO_ARCH_SAMD)
  #error "FlashStorage library only supports SAMD microcontrollers (SAMD21/SAMD51)"
#endif

// Concatenate after macro expansion (namespaced to avoid conflicts)
#define FLASHSTORAGE_PPCAT_NX(A, B) A ## B
#define FLASHSTORAGE_PPCAT(A, B) FLASHSTORAGE_PPCAT_NX(A, B)

// Compile-time string hash for variable identification
namespace FlashStorageInternal {
  constexpr uint16_t hash_combine(uint16_t hash, uint16_t value) {
    return hash ^ (value + 0x9e37 + (hash << 6) + (hash >> 2));
  }
  
  constexpr uint16_t hash_string(const char* str, size_t index = 0, uint16_t hash = 0x5A5A) {
    return (str[index] == '\0') ? hash : 
           hash_string(str, index + 1, hash_combine(hash, (uint16_t)str[index]));
  }
  
  constexpr uint16_t hash_variable(const char* name, size_t size) {
    return hash_combine(hash_string(name), (uint16_t)size);
  }
}

#if defined(__SAMD51__)
  #define Flash(name, size) \
  __attribute__((__aligned__(8192))) \
  static const uint8_t FLASHSTORAGE_PPCAT(_data,name)[(size+8191)/8192*8192] = { }; \
  FlashClass name(FLASHSTORAGE_PPCAT(_data,name), size);

#define FlashStorage(name, T) \
  __attribute__((__aligned__(8192))) \
  static const uint8_t FLASHSTORAGE_PPCAT(_data,name)[(sizeof(T)+4+8191)/8192*8192] = { }; \
  FlashStorageClass<T> name(FLASHSTORAGE_PPCAT(_data,name), FlashStorageInternal::hash_variable(#name, sizeof(T)));
#else
  #define Flash(name, size) \
  __attribute__((__aligned__(256))) \
  static const uint8_t FLASHSTORAGE_PPCAT(_data,name)[(size+255)/256*256] = { }; \
  FlashClass name(FLASHSTORAGE_PPCAT(_data,name), size);

#define FlashStorage(name, T) \
  __attribute__((__aligned__(256))) \
  static const uint8_t FLASHSTORAGE_PPCAT(_data,name)[(sizeof(T)+4+255)/256*256] = { }; \
  FlashStorageClass<T> name(FLASHSTORAGE_PPCAT(_data,name), FlashStorageInternal::hash_variable(#name, sizeof(T)));
#endif

// WARNING: FlashClass operations are NOT interrupt-safe and NOT thread-safe.
// - Do not call from interrupt service routines (ISRs)
// - Do not call concurrently from multiple threads/contexts
// - Flash operations take milliseconds and block execution
// - Use noInterrupts()/interrupts() or mutex if concurrent access is possible
class FlashClass {
public:
  FlashClass(const void *flash_addr = NULL, uint32_t size = 0);

  bool write(const void *data) { return write(flash_address, data, flash_size); }
  bool erase()                 { return erase(flash_address, flash_size);       }
  bool read(void *data)        { return read(flash_address, data, flash_size);  }

  bool write(const volatile void *flash_ptr, const void *data, uint32_t size);
  bool erase(const volatile void *flash_ptr, uint32_t size);
  bool read(const volatile void *flash_ptr, void *data, uint32_t size);

private:
  bool isWithinBounds(const volatile void *flash_ptr, uint32_t size) const;
  bool erase(const volatile void *flash_ptr);

  const uint32_t PAGE_SIZE, ROW_SIZE;
  const volatile void *flash_address;
  const uint32_t flash_size;
};

template<class T>
class FlashStorageClass {
private:
  struct StorageFormat {
    uint16_t id_hash;  // Hash of variable name + sizeof(T)
    T data;
    uint16_t checksum;
  };
  
  FlashClass flash;
  uint16_t variable_hash;
  
  // Calculate checksum for data validation
  static uint16_t calcChecksum(const uint8_t* ptr, size_t len) {
    uint16_t sum = 0xA5A5;  // Non-zero seed for better distribution
    for (size_t i = 0; i < len; i++) {
      sum += ptr[i];
      sum ^= (sum >> 8);  // Mix bits for better distribution
    }
    return sum;
  }

public:
  FlashStorageClass(const void *flash_addr, uint16_t var_hash) 
    : flash(flash_addr, sizeof(StorageFormat)), variable_hash(var_hash) { };

  // Write data into flash memory with checksum validation.
  // Compiler is able to optimize parameter copy.
  // Returns true on success, false on error.
  // Optimization: Skips erase+write if data hasn't changed (preserves flash endurance).
  inline bool write(T data) {
    StorageFormat pkg = {};  // Zero-initialize to eliminate padding bytes
    pkg.id_hash = variable_hash;
    pkg.data = data;
    pkg.checksum = calcChecksum((const uint8_t*)&pkg.data, sizeof(T));
    
    // Read existing data to check if write is necessary
    StorageFormat existing;
    if (flash.read(&existing)) {
      // Compare entire structure (including hash and checksum)
      if (memcmp(&pkg, &existing, sizeof(StorageFormat)) == 0) {
        return true;  // Data unchanged, skip erase+write to preserve flash endurance
      }
    }
    
    // Data changed or uninitialized, perform erase+write
    return flash.erase() && flash.write(&pkg);
  }

  // Read data from flash into variable with validation.
  // Returns true if valid data found, false if uninitialized or corrupted.
  inline bool read(T *data) {
    StorageFormat pkg;
    if (!flash.read(&pkg)) {
      return false;  // Read failed or out of bounds
    }
    
    // Validate variable hash (name + size)
    if (pkg.id_hash != variable_hash) {
      return false;  // Wrong variable, structure size changed, or uninitialized
    }
    
    // Validate checksum
    uint16_t expected = calcChecksum((const uint8_t*)&pkg.data, sizeof(T));
    if (pkg.checksum != expected) {
      return false;  // Corrupted data
    }
    
    *data = pkg.data;
    return true;
  }

  // Overloaded version of read.
  // Returns default-constructed T if validation fails.
  // Check return value of read(T*) version for explicit validation.
  inline T read() { T data; read(&data); return data; }
};

#endif // FLASHSTORAGE_H