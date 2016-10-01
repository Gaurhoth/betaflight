/**
 ******************************************************************************
 * @addtogroup dRonin Targets
 * @{
 * @addtogroup BrainRE1 support files
 * @{
 *
 * @file       fpga_drv.c
 * @author     dRonin, http://dRonin.org/, Copyright (C) 2016
 * @brief      Driver for the RE1 custom FPGA
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Additional note on redistribution: The copyright and license notices above
 * must be maintained in each individual source file that is a derivative work
 * of this source file; otherwise redistribution is prohibited.
 */

#include "platform.h"

#if defined(USE_RE1_FPGA)

#include "fpga_drv.h"

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_rcc.h"

#include "common/maths.h"
#include "drivers/system.h"
#include "drivers/io.h"
#include "drivers/bus_spi.h"


/**
* RE1 Register Specification
* ##########################
*
* Address  Name   Type  Length  Default  Description
* 0x00     HWREV  R     1       NA       Hardware/FPGA revision
* 0x01     CFG    R/W   1       0x00     CFG[0]: Serial RX inverter
*                                                0: normal 1: invert
*                                        CFG[1]: MultiPort TX mode
*                                                0: output 1: bidirectional
*                                        CFG[2]: MultiPort TX inverter
*                                                0: normal 1: inverted
*                                        CFG[3]: MultiPort TX pullup/down
*                                                0: pullup / down disabled
*                                                1: pullup / down enabled
*                                        CFG[4]: MultiPort TX pullup/down
*                                                0: pull-down
*                                                1: pull-up
*                                        CFG[5]: LEDCFG 0: (status: blue, custom: green)
*                                                       0: (status: green, custom: blue)
*                                        CFG[6]: 0: BUZZER DC drive
*                                                1: BUZZER 4kHz drive
* 0x02     CTL    R/W   1       0x00     CTL[0]: BUZZER 0: off, 1: on
*                                        CTL[1]: LED custom: 0: off, 1: on
* 0x03     BLACK  R/W   1       XXX      OSD black level
* 0x04     WHITE  R/W   1       XXX      OSD white level
* 0x05     THR    R/W   1       XXX      OSD sync detect threshold
* 0x06     XCFG   R/W   1       0x00     OSD X axis configuration
*                                        XCFG[7:4] x-axis stretch
*                                        XCFG[3:0] x-axis offset
* 0x07     XCFG2  R/W   1       0x00     XCFG2[6]  : 0: normal 1: SBS3D mode
*                                        XCFG2[5:0]: right-eye x offset
* 0x08     IRCFG  R/W   1       0x00     IRCFG[0:3] IR Protocol
*                                        0x0: OFF / STM32 controlled
*                                        0x1: I-Lap / Trackmate
*                                        0x2: XX
*                                        0x3: XX
*                                        IRCFG[7:4] IR Power
* 0x09     IRDATA W     16      0x00     IR tranponder data
* 0x0F     LED    W     3072    0x00     WS2812B LED data
*/

enum re1fpga_register {
    RE1FPGA_REG_HWREV   = 0x00,
    RE1FPGA_REG_CFG     = 0x01,
    RE1FPGA_REG_CTL     = 0x02,
    RE1FPGA_REG_BLACK   = 0x03,
    RE1FPGA_REG_WHITE   = 0x04,
    RE1FPGA_REG_THR     = 0x05,
    RE1FPGA_REG_XCFG    = 0x06,
    RE1FPGA_REG_XCFG2   = 0x07,
    RE1FPGA_REG_IRCFG   = 0x08,
    RE1FPGA_REG_IRDATA  = 0x09,
    RE1FPGA_REG_LED     = 0x0F,
};

struct re1_shadow_reg {
    uint8_t reg_hwrev;
    uint8_t reg_cfg;
    uint8_t reg_ctl;
    uint8_t reg_black;
    uint8_t reg_white;
    uint8_t reg_thr;
    uint8_t reg_xcfg;
    uint8_t reg_xcfg2;
    uint8_t reg_ircfg;
};

static volatile struct re1_shadow_reg shadow_reg;
static IO_t re1FPGACsPin = IO_NONE;
static IO_t re1FPGACdonePin = IO_NONE;
static IO_t re1FPGACresetPin = IO_NONE;
static IO_t re1FPGAResetPin = IO_NONE;


static int32_t RE1FPGA_WriteReg(uint8_t reg, uint8_t data, uint8_t mask);
static int32_t RE1FPGA_WriteRegDirect(enum re1fpga_register reg, uint8_t data);
static uint8_t RE1FPGA_ReadReg(uint8_t reg);
static void update_shadow_regs();
int32_t RE1FPGA_SetLEDs(const uint8_t * led_data, uint16_t n_leds);
int32_t RE1FPGA_SetIRData(const uint8_t * ir_data, uint8_t n_bytes);


/**
 * @brief Initialize the driver
 * @return 0 for success
 */
int32_t RE1FPGA_Init(bool load_config)
{

    re1FPGACsPin = IOGetByTag(IO_TAG(RE1FPGA_CS_PIN));
    IOInit(re1FPGACsPin, OWNER_OSD, RESOURCE_SPI_CS, 0);
    IOConfigGPIO(re1FPGACsPin, SPI_IO_CS_CFG);

    spiSetDivisor(RE1FPGA_SPI_INSTANCE, RE1FPGA_SPI_DIVISOR);

    if (load_config) {
        /* Configure the CDONE and CRESETB pins */
        re1FPGACdonePin = IOGetByTag(IO_TAG(RE1FPGA_CDONE_PIN));
        IOInit(re1FPGACdonePin, OWNER_OSD, RESOURCE_INPUT, 0);
        IOConfigGPIO(re1FPGACdonePin, IOCFG_IPU_25);

        re1FPGACresetPin = IOGetByTag(IO_TAG(RE1FPGA_CRESET_PIN));
        IOInit(re1FPGACresetPin, OWNER_OSD, RESOURCE_OUTPUT, 0);
        IOConfigGPIO(re1FPGACresetPin, IOCFG_OUT_PP);

        // CRESETB low for 1ms
        IOLo(re1FPGACresetPin);
        delay(1);
        IOHi(re1FPGACresetPin);

        for (int i=0; i<100; i++) {
            if (IORead(re1FPGACdonePin))
                break;
            delay(1);
        }

        if (!IORead(re1FPGACdonePin))
            return -2;
    }

    /* Configure 16MHz clock output to FPGA */
    IOConfigGPIOAF(IOGetByTag(IO_TAG(RE1FPGA_CLOCK_PIN)), IOCFG_AF_PP, GPIO_AF_MCO);
    RCC_MCO1Config(RCC_MCO1Source_HSE, RCC_MCO1Div_1);
    RCC_MCO1Cmd(ENABLE);

    /* Configure reset pin */
    re1FPGAResetPin = IOGetByTag(IO_TAG(RE1FPGA_RESET_PIN));
    IOInit(re1FPGAResetPin, OWNER_OSD, RESOURCE_OUTPUT, 0);
    IOConfigGPIO(re1FPGAResetPin, IO_CONFIG(GPIO_Mode_OUT, GPIO_Speed_2MHz, GPIO_OType_PP, GPIO_PuPd_DOWN));

    // Give the PLL some time to stabilize
    delay(10);

    // Reset the FPGA
    IOHi(re1FPGAResetPin);
    delay(1);
    IOLo(re1FPGAResetPin);
    delay(1);

    // Initialize registers
    RE1FPGA_WriteRegDirect(RE1FPGA_REG_CFG, 0x00);
    RE1FPGA_WriteRegDirect(RE1FPGA_REG_CTL, 0x00);
    RE1FPGA_WriteRegDirect(RE1FPGA_REG_BLACK, 35);
    RE1FPGA_WriteRegDirect(RE1FPGA_REG_WHITE, 110);
    RE1FPGA_WriteRegDirect(RE1FPGA_REG_THR, 120);
    RE1FPGA_WriteRegDirect(RE1FPGA_REG_XCFG, 0x08);
    RE1FPGA_WriteRegDirect(RE1FPGA_REG_XCFG2, 0x10);
    RE1FPGA_WriteRegDirect(RE1FPGA_REG_IRCFG, 0x00);

    // Read all registers into shadow regsiters
    update_shadow_regs();

    return 0;
}

/**
 * @brief Claim the SPI bus for the communications and select this chip
 * @return 0 if successful, -1 for invalid device, -2 if unable to claim bus
 */
static int32_t RE1FPGA_ClaimBus()
{
    IOLo(re1FPGACsPin);

    return 0;
}

/**
 * @brief Release the SPI bus for the communications and end the transaction
 * @return 0 if successful
 */
static int32_t RE1FPGA_ReleaseBus()
{
    IOHi(re1FPGACsPin);

    return 0;
}

/**
 * @brief Writes one byte to the RE1FPGA register
 * \param[in] reg Register address
 * \param[in] data Byte to write
 * @returns 0 when success
 */
static int32_t RE1FPGA_WriteReg(enum re1fpga_register reg, uint8_t data, uint8_t mask)
{
    volatile uint8_t* cur_reg;

    switch (reg) {
    case RE1FPGA_REG_HWREV:
        return -2;
    case RE1FPGA_REG_CFG:
        cur_reg = &shadow_reg.reg_cfg;
        break;
    case RE1FPGA_REG_CTL:
        cur_reg = &shadow_reg.reg_ctl;
        break;
    case RE1FPGA_REG_BLACK:
        cur_reg = &shadow_reg.reg_black;
        break;
    case RE1FPGA_REG_WHITE:
        cur_reg = &shadow_reg.reg_white;
        break;
    case RE1FPGA_REG_THR:
        cur_reg = &shadow_reg.reg_thr;
        break;
    case RE1FPGA_REG_XCFG:
        cur_reg = &shadow_reg.reg_xcfg;
        break;
    case RE1FPGA_REG_XCFG2:
        cur_reg = &shadow_reg.reg_xcfg2;
        break;
    case RE1FPGA_REG_IRCFG:
        cur_reg = &shadow_reg.reg_ircfg;
        break;

    default:
    case RE1FPGA_REG_LED:
        return -2;
    }

    uint8_t new_data = (*cur_reg & ~mask) | (data & mask);


    if (new_data == *cur_reg) {
        return 0;
    }

    *cur_reg = new_data;

    return RE1FPGA_WriteRegDirect(reg, new_data);
}

static int32_t RE1FPGA_WriteRegDirect(enum re1fpga_register reg, uint8_t data)
{

    if (RE1FPGA_ClaimBus() != 0)
        return -3;

    spiTransferByte(RE1FPGA_SPI_INSTANCE, 0x7f & reg);
    spiTransferByte(RE1FPGA_SPI_INSTANCE, data);

    RE1FPGA_ReleaseBus();

    return 0;
}

/**
 * @brief Read a register from RE1FPGA
 * @returns The register value
 * @param reg[in] Register address to be read
 */
static uint8_t RE1FPGA_ReadReg(enum re1fpga_register reg)
{
    uint8_t data;
    uint8_t out = 0x00;

    RE1FPGA_ClaimBus();

    spiTransferByte(RE1FPGA_SPI_INSTANCE, 0x80 | reg);
    spiTransfer(RE1FPGA_SPI_INSTANCE, &data, &out, 1);   // receive response

    RE1FPGA_ReleaseBus();

    return data;
}


/**
 * @brief Read all registers and update shadow registers
 */
static void update_shadow_regs(void)
{
    shadow_reg.reg_hwrev = RE1FPGA_ReadReg(RE1FPGA_REG_HWREV);
    shadow_reg.reg_cfg = RE1FPGA_ReadReg(RE1FPGA_REG_CFG);
    shadow_reg.reg_ctl = RE1FPGA_ReadReg(RE1FPGA_REG_CTL);
    shadow_reg.reg_black = RE1FPGA_ReadReg(RE1FPGA_REG_BLACK);
    shadow_reg.reg_white = RE1FPGA_ReadReg(RE1FPGA_REG_WHITE);
    shadow_reg.reg_thr = RE1FPGA_ReadReg(RE1FPGA_REG_THR);
    shadow_reg.reg_xcfg = RE1FPGA_ReadReg(RE1FPGA_REG_XCFG);
    shadow_reg.reg_xcfg2 = RE1FPGA_ReadReg(RE1FPGA_REG_XCFG2);
    shadow_reg.reg_ircfg = RE1FPGA_ReadReg(RE1FPGA_REG_IRCFG);
}

/**
 * @brief Get the Hardware
 */
uint8_t RE1FPGA_GetHWRevision()
{
    return shadow_reg.reg_hwrev;
}


/**
 * @brief Set programmable LED (WS2812B) colors
 */
int32_t RE1FPGA_SetLEDs(const uint8_t * led_data, uint16_t n_leds)
{
    if (RE1FPGA_ClaimBus() != 0)
        return -1;

    n_leds = MIN(n_leds, 1024);

    spiTransferByte(RE1FPGA_SPI_INSTANCE, 0x7f & RE1FPGA_REG_LED);
    spiTransfer(RE1FPGA_SPI_INSTANCE, NULL, led_data, 3 * n_leds);

    RE1FPGA_ReleaseBus();

    return 0;
}

/**
 * @brief Set programmable LED (WS2812B) color
 */
#define LED_BLOCK_SIZE 16
int32_t RE1FPGA_SetLEDColor(uint16_t n_leds, uint8_t red, uint8_t green, uint8_t blue)
{

    uint8_t LED_DATA[LED_BLOCK_SIZE * 3];

    n_leds = MIN(n_leds, 1024);

    if (RE1FPGA_ClaimBus() != 0)
        return -1;

    for (int i=0; i<LED_BLOCK_SIZE; i++) {
        LED_DATA[3 * i] = green;
        LED_DATA[3 * i + 1] = red;
        LED_DATA[3 * i + 2] = blue;
    }

    spiTransferByte(RE1FPGA_SPI_INSTANCE, 0x7f & RE1FPGA_REG_LED);

    for (int i=0; i<n_leds/LED_BLOCK_SIZE; i++) {
        spiTransfer(RE1FPGA_SPI_INSTANCE, NULL, LED_DATA, 3 * LED_BLOCK_SIZE);
    }

    if (n_leds % LED_BLOCK_SIZE != 0) {
        spiTransfer(RE1FPGA_SPI_INSTANCE, NULL, LED_DATA, 3 * (n_leds % LED_BLOCK_SIZE));
    }

    RE1FPGA_ReleaseBus();

    return 0;
}

/**
 * @brief Set the protocol for the IR transmitter
 */
int32_t RE1FPGA_SetIRProtocol(enum re1fpga_ir_protocols ir_protocol)
{
    uint8_t value = 0;

    switch (ir_protocol) {
    case RE1FPGA_IR_PROTOCOL_OFF:
        value = 0;
        break;
    case RE1FPGA_IR_PROTOCOL_ILAP:
    case RE1FPGA_IR_PROTOCOL_TRACKMATE:
        /* They use the same basic protocol */
        value = 0x01;
        break;
    }

    return RE1FPGA_WriteReg(RE1FPGA_REG_IRCFG, value, 0x0F);
}

/**
 * @brief Set IR emitter data
 */
int32_t RE1FPGA_SetIRData(const uint8_t * ir_data, uint8_t n_bytes)
{
    if (n_bytes > 16)
        return - 1;

    if (RE1FPGA_ClaimBus() != 0)
        return -2;


    spiTransferByte(RE1FPGA_SPI_INSTANCE, 0x7f & RE1FPGA_REG_IRDATA);
    spiTransfer(RE1FPGA_SPI_INSTANCE, NULL, ir_data, n_bytes);

    RE1FPGA_ReleaseBus();

    return 0;
}

/**
 * @brief Enable / disable the serial RX inverter
 */
int32_t RE1FPGA_SerialRxInvert(bool invert)
{
    uint8_t data;
    if (invert) {
        data = 0x01;
    }
    else {
        data = 0x00;
    }
    return RE1FPGA_WriteReg(RE1FPGA_REG_CFG, data, 0x01);
}

/**
 * @brief Set MultiPort TX pin mode
 */
int32_t RE1FPGA_MPTxPinMode(bool bidrectional, bool invert)
{
    uint8_t data = 0;

    data |= bidrectional << 1;
    data |= invert << 2;

    return RE1FPGA_WriteReg(RE1FPGA_REG_CFG, data, 0x06);
}

/**
 * @brief Set MultiPort TX pull-up / pull-down resistor
 */
int32_t RE1FPGA_MPTxPinPullUpDown(bool enable, bool pullup)
{
    uint8_t data = 0;

    data |= enable << 3;
    data |= pullup << 4;

    return RE1FPGA_WriteReg(RE1FPGA_REG_CFG, data, 0x18);
}

/**
 * @brief Set buzzer type
 */
int32_t RE1FPGA_SetBuzzerType(enum re1fpga_buzzer_types type)
{
    uint8_t data;
    if (type == RE1FPGA_BUZZER_DC) {
        data = 0;
    }
    else {
        data = 255;
    }
    return RE1FPGA_WriteReg(RE1FPGA_REG_CFG, data, 0x40);
}


/**
 * @brief Enable / disable buzzer
 */
int32_t RE1FPGA_Buzzer(bool enable)
{
    uint8_t data = enable;

    return RE1FPGA_WriteReg(RE1FPGA_REG_CTL, data, 0x01);
}

/**
 * @brief Toggle buzzer
 */
int32_t RE1FPGA_BuzzerToggle()
{
    uint8_t data = shadow_reg.reg_ctl ^ 0x01;
    return RE1FPGA_WriteReg(RE1FPGA_REG_CTL, data, 0x01);
}

/**
 * @brief Set the notification LED colors
 */
int32_t RE1FPGA_SetNotificationLedColor(enum re1fpga_led_colors led_colors)
{
    uint8_t value;

    if (led_colors == RE1FPGA_STATUS_BLUE_CUSTOM_GREEN)
        value = 0;
    else
        value = 255;

    return RE1FPGA_WriteReg(RE1FPGA_REG_CFG, value, 0x20);
}

/**
 * @brief Set OSD black and white levels
 */
void RE1FPGA_SetBwLevels(uint8_t black, uint8_t white)
{
    RE1FPGA_WriteReg(RE1FPGA_REG_BLACK, black, 0xFF);
    RE1FPGA_WriteReg(RE1FPGA_REG_WHITE, white, 0xFF);
}

/**
 * @brief Set the threshold for the video sync pulse detector
 */
int32_t RE1FPGA_SetSyncThreshold(uint8_t threshold)
{
    return RE1FPGA_WriteReg(RE1FPGA_REG_THR, threshold, 0xFF);
}

/**
 * @brief Set OSD x offset
 */
void RE1FPGA_SetXOffset(int8_t x_offset)
{
    //x_offset += 5;
    if (x_offset >= 7)
        x_offset = 7;
    if (x_offset < -8)
        x_offset = -8;

    uint8_t value = 8 + x_offset;
    RE1FPGA_WriteReg(RE1FPGA_REG_XCFG, value, 0x0F);
}

/**
 * @brief Set OSD x scale
 */
void RE1FPGA_SetXScale(uint8_t x_scale)
{
    x_scale = (x_scale & 0x0F) << 4;
    RE1FPGA_WriteReg(RE1FPGA_REG_XCFG, x_scale, 0xF0);
}

/**
 * @brief Set 3D mode configuration
 */
void RE1FPGA_Set3DConfig(bool enabled, uint8_t x_shift_right)
{
    uint8_t cfg;
    uint8_t enabled_data = enabled ? 1 : 0;

    cfg = (enabled_data << 6) | (x_shift_right & 0x3F);
    RE1FPGA_WriteReg(RE1FPGA_REG_XCFG2, cfg, 0xFF);
}

#endif /* defined(USE_RE1_FPGA) */




