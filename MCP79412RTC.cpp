/*----------------------------------------------------------------------*
 * MCP79412RTC.cpp - Arduino library for the Microchip MCP79412         *
 * Real-Time Clock. This library is intended for use with the Arduino   *
 * Time.h library, http://www.arduino.cc/playground/Code/Time           *
 *                                                                      *
 * This library is a drop-in replacement for the DS1307RTC.h library    *
 * by Michael Margolis that is supplied with the Arduino Time library   *
 * above. To change from using a DS1307 RTC to an MCP79412 RTC, it is   *
 * only necessary to change the #include statement to include this      *
 * library instead of DS1307RTC.h.                                      *
 *                                                                      *
 * In addition, this library implements functions to support the        *
 * additional features of the MCP79412.                                 *
 *                                                                      *
 * Jack Christensen 29Jul2012                                           *
 *                                                                      *
 * This work is licensed under the Creative Commons Attribution-        *
 * ShareAlike 3.0 Unported License. To view a copy of this license,     *
 * visit http://creativecommons.org/licenses/by-sa/3.0/ or send a       *
 * letter to Creative Commons, 171 Second Street, Suite 300,            *
 * San Francisco, California, 94105, USA.                               *
 *----------------------------------------------------------------------*/ 

#include <Wire.h>
#include "MCP79412RTC.h"

/*----------------------------------------------------------------------*
 * Constructor.                                                         *
 *----------------------------------------------------------------------*/
MCP79412RTC::MCP79412RTC()
{
    Wire.begin();
}
  
/*----------------------------------------------------------------------*
 * Read the current time from the RTC and return it as a time_t value.  *
 *----------------------------------------------------------------------*/
time_t MCP79412RTC::get()
{
    tmElements_t tm;
    
    read(tm);
    return(makeTime(tm));
}

/*----------------------------------------------------------------------*
 * Set the RTC to the given time_t value.                               *
 *----------------------------------------------------------------------*/
void MCP79412RTC::set(time_t t)
{
    tmElements_t tm;

    breakTime(t, tm);
    write(tm); 
}

/*----------------------------------------------------------------------*
 * Read the current time from the RTC and return it in a tmElements_t   *
 * structure.                                                           *
 *----------------------------------------------------------------------*/
void MCP79412RTC::read(tmElements_t &tm)
{
    Wire.beginTransmission(RTC_ADDR);
    i2cWrite(TIME_REG);
    Wire.endTransmission();

    //request 7 bytes (secs, min, hr, dow, date, mth, yr)
    Wire.requestFrom(RTC_ADDR, tmNbrFields);
    tm.Second = bcd2dec(i2cRead() & ~_BV(ST));   
    tm.Minute = bcd2dec(i2cRead());
    tm.Hour = bcd2dec(i2cRead() & ~_BV(HR1224));    //assumes 24hr clock
    tm.Wday = bcd2dec(i2cRead() & ~(_BV(OSCON) | _BV(VBAT) | _BV(VBATEN)) );    //mask off OSCON, VBAT, VBATEN bits
    tm.Day = bcd2dec(i2cRead());
    tm.Month = bcd2dec(i2cRead() & ~_BV(LP));       //mask off the leap year bit
    tm.Year = y2kYearToTm(bcd2dec(i2cRead()));
}

/*----------------------------------------------------------------------*
 * Set the RTC's time from a tmElements_t structure.                    *
 *----------------------------------------------------------------------*/
void MCP79412RTC::write(tmElements_t &tm)
{
    Wire.beginTransmission(RTC_ADDR);
    i2cWrite(TIME_REG);
    i2cWrite(0x00);                              //stops the oscillator (Bit 7, ST == 0)   
    i2cWrite(dec2bcd(tm.Minute));
    i2cWrite(dec2bcd(tm.Hour));                  //sets 24 hour format (Bit 6 == 0)
    i2cWrite(dec2bcd(tm.Wday) | _BV(VBATEN));    //enable battery backup operation
    i2cWrite(dec2bcd(tm.Day));
    i2cWrite(dec2bcd(tm.Month));
    i2cWrite(dec2bcd(tmYearToY2k(tm.Year))); 
    Wire.endTransmission();  

    Wire.beginTransmission(RTC_ADDR);
    i2cWrite(TIME_REG);
    i2cWrite(dec2bcd(tm.Second) | _BV(ST));    //set the seconds and start the oscillator (Bit 7, ST == 1)
    Wire.endTransmission();  
}

/*----------------------------------------------------------------------*
 * Write a single byte to RTC RAM.                                      *
 * Valid address range is 0x00 - 0x5F, no checking.                     *
 *----------------------------------------------------------------------*/
void MCP79412RTC::ramWrite(byte addr, byte value)
{
    ramWrite(addr, &value, 1);
}

/*----------------------------------------------------------------------*
 * Write multiple bytes to RTC RAM.                                     *
 * Valid address range is 0x00 - 0x5F, no checking.                     *
 * Number of bytes (nBytes) must be between 1 and 31 (Wire library      *
 * has a limitation of 31 bytes).                                       *
 *----------------------------------------------------------------------*/
void MCP79412RTC::ramWrite(byte addr, byte *values, byte nBytes)
{
    Wire.beginTransmission(RTC_ADDR);
    i2cWrite(addr);
    for (byte i=0; i<nBytes; i++) i2cWrite(values[i]);
    Wire.endTransmission();  
}

/*----------------------------------------------------------------------*
 * Read a single byte from RTC RAM.                                     *
 * Valid address range is 0x00 - 0x5F, no checking.                     *
 *----------------------------------------------------------------------*/
byte MCP79412RTC::ramRead(byte addr)
{
    byte value;
    
    ramRead(addr, &value, 1);
    return value;
}

/*----------------------------------------------------------------------*
 * Read multiple bytes from RTC RAM.                                    *
 * Valid address range is 0x00 - 0x5F, no checking.                     *
 * Number of bytes (nBytes) must be between 1 and 31 (Wire library      *
 * has a limitation of 31 bytes).                                       *
 *----------------------------------------------------------------------*/
void MCP79412RTC::ramRead(byte addr, byte *values, byte nBytes)
{
    Wire.beginTransmission(RTC_ADDR);
    i2cWrite(addr);
    Wire.endTransmission();  
    Wire.requestFrom( (uint8_t)RTC_ADDR, nBytes );
    for (byte i=0; i<nBytes; i++) values[i] = i2cRead();
}

/*----------------------------------------------------------------------*
 * Write a single byte to Static RAM.                                   *
 * Address (addr) is constrained to the range (0, 63).                  *
 *----------------------------------------------------------------------*/
void MCP79412RTC::sramWrite(byte addr, byte value)
{
    ramWrite( (addr & (SRAM_SIZE - 1) ) + SRAM_START_ADDR, &value, 1 );
}

/*----------------------------------------------------------------------*
 * Write multiple bytes to Static RAM.                                  *
 * Address (addr) is constrained to the range (0, 63).                  *
 * Number of bytes (nBytes) must be between 1 and 31 (Wire library      *
 * has a limitation of 31 bytes).                                       *
 * Invalid values for nBytes, or combinations of addr and nBytes        *
 * that would result in addressing past the last byte of SRAM will      *
 * result in no action.                                                 *
 *----------------------------------------------------------------------*/
void MCP79412RTC::sramWrite(byte addr, byte *values, byte nBytes)
{
    if (nBytes >= 1 && nBytes <= I2C_BYTE_LIMIT && (addr + nBytes) <= SRAM_SIZE) {
        ramWrite( (addr & (SRAM_SIZE - 1) ) + SRAM_START_ADDR, values, nBytes );
    }
}

/*----------------------------------------------------------------------*
 * Read a single byte from Static RAM.                                  *
 * Address (addr) is constrained to the range (0, 63).                  *
 *----------------------------------------------------------------------*/
byte MCP79412RTC::sramRead(byte addr)
{
    byte value;
    
    ramRead( (addr & (SRAM_SIZE - 1) ) + SRAM_START_ADDR, &value, 1 );
    return value;
}

/*----------------------------------------------------------------------*
 * Read multiple bytes from Static RAM.                                 *
 * Address (addr) is constrained to the range (0, 63).                  *
 * Number of bytes (nBytes) must be between 1 and 31 (Wire library      *
 * has a limitation of 31 bytes).                                       *
 * Invalid values for addr or nBytes, or combinations of addr and       *
 * nBytes that would result in addressing past the last byte of SRAM    *
 * result in no action.                                                 *
 *----------------------------------------------------------------------*/
void MCP79412RTC::sramRead(byte addr, byte *values, byte nBytes)
{
    if (nBytes >= 1 && nBytes <= I2C_BYTE_LIMIT && (addr + nBytes) <= SRAM_SIZE) {
        ramRead((addr & (SRAM_SIZE - 1) ) + SRAM_START_ADDR, values, nBytes);
    }
}

/*----------------------------------------------------------------------*
 * Write a single byte to EEPROM.                                       *
 * Address (addr) is constrained to the range (0, 127).                 *
 * Can't leverage page write function because a write can't start       *
 * mid-page.                                                            *
 *----------------------------------------------------------------------*/
void MCP79412RTC::eepromWrite(byte addr, byte value)
{
        Wire.beginTransmission(EEPROM_ADDR);
        i2cWrite( addr & (EEPROM_SIZE - 1) );
        i2cWrite(value);
        Wire.endTransmission();
        eepromWait();
}

/*----------------------------------------------------------------------*
 * Write a page (or less) to EEPROM. An EEPROM page is 8 bytes.         *
 * Address (addr) should be a page start address (0, 8, ..., 120), but  *
 * is ruthlessly coerced into a valid value.                            *
 * Number of bytes (nBytes) must be between 1 and 8, other values       *
 * result in no action.                                                 *
 *----------------------------------------------------------------------*/
void MCP79412RTC::eepromWrite(byte addr, byte *values, byte nBytes)
{
    if (nBytes >= 1 && nBytes <= EEPROM_PAGE_SIZE) {
        Wire.beginTransmission(EEPROM_ADDR);
        i2cWrite( addr & ~(EEPROM_PAGE_SIZE - 1) & (EEPROM_SIZE - 1) );
        for (byte i=0; i<nBytes; i++) i2cWrite(values[i]);
        Wire.endTransmission();
        eepromWait();
    }
}

/*----------------------------------------------------------------------*
 * Read a single byte from EEPROM.                                      *
 * Address (addr) is constrained to the range (0, 127).                  *
 *----------------------------------------------------------------------*/
byte MCP79412RTC::eepromRead(byte addr)
{
    byte value;

    eepromRead( addr & (EEPROM_SIZE - 1), &value, 1 );    
    return value;
}

/*----------------------------------------------------------------------*
 * Read multiple bytes from EEPROM.                                     *
 * Address (addr) is constrained to the range (0, 127).                 *
 * Number of bytes (nBytes) must be between 1 and 31 (Wire library      *
 * has a limitation of 31 bytes).                                       *
 * Invalid values for addr or nBytes, or combinations of addr and       *
 * nBytes that would result in addressing past the last byte of EEPROM  *
 * result in no action.                                                 *
 *----------------------------------------------------------------------*/
void MCP79412RTC::eepromRead(byte addr, byte *values, byte nBytes)
{
    if (nBytes >= 1 && nBytes <= I2C_BYTE_LIMIT && (addr + nBytes) <= EEPROM_SIZE) {
        Wire.beginTransmission(EEPROM_ADDR);
        i2cWrite( addr & (EEPROM_SIZE - 1) );
        Wire.endTransmission();  
        Wire.requestFrom( (uint8_t)EEPROM_ADDR, nBytes );
        for (byte i=0; i<nBytes; i++) values[i] = i2cRead();
    }
}

/*----------------------------------------------------------------------*
 * Wait for EEPROM write to complete.                                   *
 *----------------------------------------------------------------------*/
byte MCP79412RTC::eepromWait(void)
{
    byte waitCount = 0;
    byte txStatus;
    
    do
    {
        ++waitCount;
        Wire.beginTransmission(EEPROM_ADDR);
        i2cWrite(0);
        txStatus = Wire.endTransmission();
        
    } while (txStatus != 0);
        
    return waitCount;
}

/*----------------------------------------------------------------------*
 * Read the calibration register.                                       *
 * The calibration value is not a twos-complement number. The MSB is    *
 * the sign bit, and the 7 LSBs are an unsigned number, so we convert   *
 * it and return it to the caller as a regular twos-complement integer. *
 *----------------------------------------------------------------------*/
int MCP79412RTC::calibRead(void)
{
    byte val = ramRead(CALIB_REG);
    
    if ( val & 0x80 ) return -(val & 0x7F);
    else return val;   
}

/*----------------------------------------------------------------------*
 * Write the calibration register.                                      *
 * Calibration value must be between -127 and 127, others result        *
 * in no action. See note above on the format of the calibration value. *
 *----------------------------------------------------------------------*/
void MCP79412RTC::calibWrite(int value)
{
    byte calibVal;
    
    if (value >= -127 && value <= 127) {
        calibVal = abs(value);
        if (value < 0) calibVal += 128;
        ramWrite(CALIB_REG, calibVal);
    }
}

/*----------------------------------------------------------------------*
 * Read the unique ID.                                                  *
 * Caller must provide an 8-byte array to contain the results.          *
 *----------------------------------------------------------------------*/
void MCP79412RTC::idRead(byte *uniqueID)
{
    Wire.beginTransmission(EEPROM_ADDR);
    i2cWrite(UNIQUE_ID_ADDR);
    Wire.endTransmission();  
    Wire.requestFrom( EEPROM_ADDR, UNIQUE_ID_SIZE );
    for (byte i=0; i<UNIQUE_ID_SIZE; i++) uniqueID[i] = i2cRead();
}

/*----------------------------------------------------------------------*
 * Check to see if the RTC's oscillator is started (ST bit in seconds   *
 * register). Returns true if started.                                  *
 *----------------------------------------------------------------------*/
boolean MCP79412RTC::oscStarted(void)
{
    Wire.beginTransmission(RTC_ADDR);
    i2cWrite(TIME_REG);
    Wire.endTransmission();

    //request just the seconds register
    Wire.requestFrom(RTC_ADDR, 1);
    return i2cRead() & _BV(ST);
}

/*----------------------------------------------------------------------*
 * Decimal-to-BCD conversion                                            *
 *----------------------------------------------------------------------*/
uint8_t MCP79412RTC::dec2bcd(uint8_t num)
{
    return ((num / 10 * 16) + (num % 10));
}

/*----------------------------------------------------------------------*
 * BCD-to-Decimal conversion                                            *
 *----------------------------------------------------------------------*/
uint8_t MCP79412RTC::bcd2dec(uint8_t num)
{
    return ((num / 16 * 10) + (num % 16));
}

MCP79412RTC RTC = MCP79412RTC();    //instantiate an RTC object

