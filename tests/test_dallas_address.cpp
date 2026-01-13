/**
 * Test program to verify getDallasAddress() function behavior
 * 
 * This test verifies that the Dallas DS18B20 address conversion
 * works correctly and produces the expected hex string output.
 */

#include <Arduino.h>

// Mock the DeviceAddress type (it's just uint8_t array)
typedef uint8_t DeviceAddress[8];

// Include the fixed getDallasAddress function
static const char hexchars[] PROGMEM = "0123456789ABCDEF";

char* getDallasAddress(DeviceAddress deviceAddress)
{
  static char dest[17]; // 8 bytes * 2 chars + 1 null
  
  for (uint8_t i = 0; i < 8; i++)
  {
    uint8_t b = deviceAddress[i];
    dest[i*2]   = pgm_read_byte(&hexchars[b >> 4]);
    dest[i*2+1] = pgm_read_byte(&hexchars[b & 0x0F]);
  }
  dest[16] = '\0';
  return dest;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("\n\n=== Dallas Address Conversion Test ===\n");
  
  // Test Case 1: Standard Dallas address
  DeviceAddress addr1 = {0x28, 0xFF, 0x64, 0x1E, 0x82, 0x16, 0xC3, 0xA1};
  const char* expected1 = "28FF641E8216C3A1";
  const char* result1 = getDallasAddress(addr1);
  
  Serial.print("Test 1: ");
  Serial.print("Expected: ");
  Serial.println(expected1);
  Serial.print("        Got:      ");
  Serial.println(result1);
  Serial.print("        Status:   ");
  Serial.println(strcmp(result1, expected1) == 0 ? "PASS ✓" : "FAIL ✗");
  Serial.println();
  
  // Test Case 2: Address with leading zeros
  DeviceAddress addr2 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  const char* expected2 = "0102030405060708";
  const char* result2 = getDallasAddress(addr2);
  
  Serial.print("Test 2: ");
  Serial.print("Expected: ");
  Serial.println(expected2);
  Serial.print("        Got:      ");
  Serial.println(result2);
  Serial.print("        Status:   ");
  Serial.println(strcmp(result2, expected2) == 0 ? "PASS ✓" : "FAIL ✗");
  Serial.println();
  
  // Test Case 3: All zeros
  DeviceAddress addr3 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const char* expected3 = "0000000000000000";
  const char* result3 = getDallasAddress(addr3);
  
  Serial.print("Test 3: ");
  Serial.print("Expected: ");
  Serial.println(expected3);
  Serial.print("        Got:      ");
  Serial.println(result3);
  Serial.print("        Status:   ");
  Serial.println(strcmp(result3, expected3) == 0 ? "PASS ✓" : "FAIL ✗");
  Serial.println();
  
  // Test Case 4: All FFs
  DeviceAddress addr4 = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  const char* expected4 = "FFFFFFFFFFFFFFFF";
  const char* result4 = getDallasAddress(addr4);
  
  Serial.print("Test 4: ");
  Serial.print("Expected: ");
  Serial.println(expected4);
  Serial.print("        Got:      ");
  Serial.println(result4);
  Serial.print("        Status:   ");
  Serial.println(strcmp(result4, expected4) == 0 ? "PASS ✓" : "FAIL ✗");
  Serial.println();
  
  // Test Case 5: Verify length
  Serial.print("Test 5: Length check - ");
  Serial.print("Expected: 16, Got: ");
  Serial.print(strlen(result4));
  Serial.print(" - Status: ");
  Serial.println(strlen(result4) == 16 ? "PASS ✓" : "FAIL ✗");
  Serial.println();
  
  // Test Case 6: Verify no buffer overflow
  DeviceAddress addr6 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
  const char* result6 = getDallasAddress(addr6);
  Serial.print("Test 6: Buffer integrity - ");
  Serial.print("Result: ");
  Serial.print(result6);
  Serial.print(" - Status: ");
  Serial.println(strlen(result6) == 16 && result6[16] == '\0' ? "PASS ✓" : "FAIL ✗");
  Serial.println();
  
  Serial.println("=== All Tests Complete ===");
}

void loop() {
  // Nothing to do
}

/*
 * Expected Output:
 * 
 * === Dallas Address Conversion Test ===
 * 
 * Test 1: Expected: 28FF641E8216C3A1
 *         Got:      28FF641E8216C3A1
 *         Status:   PASS ✓
 * 
 * Test 2: Expected: 0102030405060708
 *         Got:      0102030405060708
 *         Status:   PASS ✓
 * 
 * Test 3: Expected: 0000000000000000
 *         Got:      0000000000000000
 *         Status:   PASS ✓
 * 
 * Test 4: Expected: FFFFFFFFFFFFFFFF
 *         Got:      FFFFFFFFFFFFFFFF
 *         Status:   PASS ✓
 * 
 * Test 5: Length check - Expected: 16, Got: 16 - Status: PASS ✓
 * 
 * Test 6: Buffer integrity - Result: AABBCCDDEEFF1122 - Status: PASS ✓
 * 
 * === All Tests Complete ===
 */
