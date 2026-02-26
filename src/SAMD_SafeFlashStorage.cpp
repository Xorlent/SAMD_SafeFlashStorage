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

#include "SAMD_SafeFlashStorage.h"
#include <stdint.h>

static const uint32_t pageSizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };

FlashClass::FlashClass(const void *flash_addr, uint32_t size) :
  PAGE_SIZE(pageSizes[NVMCTRL->PARAM.bit.PSZ]),
#if defined(__SAMD51__)
  ROW_SIZE((PAGE_SIZE * NVMCTRL->PARAM.bit.NVMP) / 64),
#else
  ROW_SIZE(PAGE_SIZE * 4),
#endif
  flash_address((volatile void *)flash_addr),
  flash_size(size)
{
}

static inline uint32_t read_unaligned_uint32(const void *data)
{
  union {
    uint32_t u32;
    uint8_t u8[4];
  } res;
  const uint8_t *d = (const uint8_t *)data;
  res.u8[0] = d[0];
  res.u8[1] = d[1];
  res.u8[2] = d[2];
  res.u8[3] = d[3];
  return res.u32;
}

bool FlashClass::isWithinBounds(const volatile void *flash_ptr, uint32_t size) const
{
  // If no bounds set (flash_size == 0), allow any operation
  if (flash_size == 0 || flash_address == NULL) {
    return true;
  }
  
  uintptr_t op_start = (uintptr_t)flash_ptr;
  
  // Check for integer overflow before addition
  if (op_start > UINTPTR_MAX - size) {
    return false;  // Would overflow
  }
  
  uintptr_t op_end = op_start + size;
  uintptr_t bound_start = (uintptr_t)flash_address;
  uintptr_t bound_end = bound_start + flash_size;
  
  // Check if operation is fully within allocated bounds
  return (op_start >= bound_start && op_end <= bound_end);
}

#if defined(__SAMD51__)
// Invalidate all CMCC cache entries if CMCC cache is enabled.
static void invalidate_CMCC_cache()
{
  if (CMCC->SR.bit.CSTS) {
    CMCC->CTRL.bit.CEN = 0;
    while (CMCC->SR.bit.CSTS) {}  // Wait for cache to disable
    CMCC->MAINT0.bit.INVALL = 1;
    // Cache remains disabled during invalidation
    CMCC->CTRL.bit.CEN = 1;
    while (!CMCC->SR.bit.CSTS) {}  // Wait for cache to re-enable
  }
}
#endif

bool FlashClass::write(const volatile void *flash_ptr, const void *data, uint32_t size)
{
  // Bounds check before writing
  if (!isWithinBounds(flash_ptr, size)) {
    return false;
  }
  
  // Calculate data boundaries
  size = (size + 3) / 4;
  volatile uint32_t *dst_addr = (volatile uint32_t *)flash_ptr;
  const uint8_t *src_addr = (uint8_t *)data;

  // Disable automatic page write
#if defined(__SAMD51__)
  NVMCTRL->CTRLA.bit.WMODE = 0;
  while (NVMCTRL->STATUS.bit.READY == 0) { }
  // Disable NVMCTRL cache while writing, per SAMD51 errata.
  bool original_CACHEDIS0 = NVMCTRL->CTRLA.bit.CACHEDIS0;
  bool original_CACHEDIS1 = NVMCTRL->CTRLA.bit.CACHEDIS1;
  NVMCTRL->CTRLA.bit.CACHEDIS0 = true;
  NVMCTRL->CTRLA.bit.CACHEDIS1 = true;
#else
  NVMCTRL->CTRLB.bit.MANW = 1;
#endif

  // Do writes in pages
  while (size) {
    // Execute "PBC" Page Buffer Clear
#if defined(__SAMD51__)
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_PBC;
    while (NVMCTRL->INTFLAG.bit.DONE == 0) { }
#else
    NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_PBC;
    while (NVMCTRL->INTFLAG.bit.READY == 0) { }
#endif

    // Fill page buffer
    uint32_t i;
    for (i=0; i<(PAGE_SIZE/4) && size; i++) {
      *dst_addr = read_unaligned_uint32(src_addr);
      src_addr += 4;
      dst_addr++;
      size--;
    }

    // Execute "WP" Write Page
#if defined(__SAMD51__)
    NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_WP;
    while (NVMCTRL->INTFLAG.bit.DONE == 0) { }
    invalidate_CMCC_cache();
#else
    NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_WP;
    while (NVMCTRL->INTFLAG.bit.READY == 0) { }
#endif
  }
  
#if defined(__SAMD51__)
  // Restore original NVMCTRL cache settings after all writes complete
  NVMCTRL->CTRLA.bit.CACHEDIS0 = original_CACHEDIS0;
  NVMCTRL->CTRLA.bit.CACHEDIS1 = original_CACHEDIS1;
  // Memory barrier to ensure flash write completion before subsequent reads
  __DSB();
  __ISB();
#endif
  
  return true;
}

bool FlashClass::erase(const volatile void *flash_ptr, uint32_t size)
{
  // Bounds check before erasing
  if (!isWithinBounds(flash_ptr, size)) {
    return false;
  }
  
  const uint8_t *ptr = (const uint8_t *)flash_ptr;
  while (size > ROW_SIZE) {
    if (!erase(ptr)) {
      return false;  // Erase failed - out of bounds
    }
    ptr += ROW_SIZE;
    size -= ROW_SIZE;
  }
  // Erase remaining partial or full row if any data remains
  if (size > 0) {
    return erase(ptr);
  }
  return true;
}

bool FlashClass::erase(const volatile void *flash_ptr)
{
  // Check that flash_ptr is row-aligned and within reasonable flash range
  // Note: We don't check against flash_size here because erasing requires
  // erasing full rows, which may be larger than the data being stored
  uintptr_t addr = (uintptr_t)flash_ptr;
  
  // Verify row alignment
  if (addr % ROW_SIZE != 0) {
    return false;
  }
  
  // Verify it's in flash memory range by reading actual device flash size
  // NVMP = # of NVM pages
  // For SAMD21: NVMP * PAGE_SIZE = total flash
  // For SAMD51: NVMP represents blocks of 64 rows, so total flash = NVMP * PAGE_SIZE * 64
#if defined(__SAMD51__)
  uint32_t flash_size_bytes = NVMCTRL->PARAM.bit.NVMP * PAGE_SIZE * 64;
#else
  uint32_t flash_size_bytes = NVMCTRL->PARAM.bit.NVMP * PAGE_SIZE;
#endif
  if (addr >= flash_size_bytes) {
    return false;
  }
  
#if defined(__SAMD51__)
  NVMCTRL->ADDR.reg = ((uint32_t)flash_ptr);
  NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_EB;
  while (!NVMCTRL->INTFLAG.bit.DONE) { }
  invalidate_CMCC_cache();
  // Memory barrier to ensure erase completion
  __DSB();
  __ISB();
#else
  NVMCTRL->ADDR.reg = ((uint32_t)flash_ptr) / 2;
  NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_ER;
  while (!NVMCTRL->INTFLAG.bit.READY) { }
#endif
  
  return true;
}

bool FlashClass::read(const volatile void *flash_ptr, void *data, uint32_t size)
{
  // Bounds check before reading
  if (!isWithinBounds(flash_ptr, size)) {
    return false;
  }
  
#if defined(__SAMD51__)
  // Ensure all flash operations complete before reading
  __DSB();
#endif
  
  memcpy(data, (const void *)flash_ptr, size);
  
#if defined(__SAMD51__)
  // Memory barrier after read to ensure data integrity
  __DSB();
#endif
  
  return true;
}