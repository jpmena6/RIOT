/*
 * Copyright (C) 2015 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include "cpu.h"
#include "board.h"

/**
 * @ingroup     cpu_kinetis_common_cpuinfo
 * @{
 *
 * @file
 * @brief       Implementation of Kinetis CPU information.
 *
 * @author      Joakim Gebart <joakim.gebart@eistec.se>
 */

/* Some older headers are missing this define, but according to their manuals
 * there still exist some read-only bits at the same location as the newer
 * Kinetis CPUs */
#ifndef SIM_SDID_DIEID_MASK
#define SIM_SDID_DIEID_MASK (0xF80u)
#endif
#ifndef SIM_SDID_DIEID_SHIFT
#define SIM_SDID_DIEID_SHIFT (7)
#endif

static void print_active_peripheral_clocks(void);

static void print_sim_sdid(void);

static void print_sim_uid(void);\

static void print_sim_fcfg(void);

/** @brief convert xxSIZE register binary values to memory size in bytes */
static inline unsigned int kinetis_size_reg_to_bytes(uint32_t xxsize)
{
    /*
     * verified in Octave:
     * >> pfsize = [0:15];
     * >> bitand((4 .* bitshift(1, floor(pfsize ./ 2)) .* (3 + (bitand(pfsize,1)))), bitcmp(7))
     * ans =
     *  8  16  24  32  48  64  96  128  192  256  384  512  768 1024 1536 2048
     */
    return ((1 << (xxsize / 2)) * (3 + (xxsize & 1))) & ~(1);
}

void print_cpu_info(void)
{
    puts("CPU info:");
    puts("=========");
    puts("");

    printf("SCB_CPUID: 0x%08" PRIx32 "\n", SCB->CPUID);

    print_sim_uid();
    print_sim_sdid();
    print_sim_fcfg();

    printf("Clocks:\n"
        "     F_CPU: %" PRIu32 "\n"
        "     F_SYS: %" PRIu32 "\n"
        "     F_BUS: %" PRIu32 "\n"
        " F_FLEXBUS: %" PRIu32 "\n"
        "   F_FLASH: %" PRIu32 "\n",
        SystemCoreClock, SystemSysClock, SystemBusClock, SystemFlexBusClock,
        SystemFlashClock);

    print_active_peripheral_clocks();
}

static void print_sim_uid(void)
{
    printf("  SIM_UID:");
#if SIM_UIDH_UID_MASK
    printf(" %08" PRIx32, SIM->UIDH);
#endif
#if SIM_UIDMH_UID_MASK
    printf(" %08" PRIx32, SIM->UIDMH);
#endif
#if SIM_UIDML_UID_MASK
    printf(" %08" PRIx32, SIM->UIDML);
#endif
#if SIM_UIDL_UID_MASK
    printf(" %08" PRIx32, SIM->UIDL);
#endif
    puts("");
}

static void print_sim_sdid(void)
{
    uint32_t dieid = ((SIM->SDID & SIM_SDID_DIEID_MASK) >> SIM_SDID_DIEID_SHIFT);
    uint32_t revid = ((SIM->SDID & SIM_SDID_REVID_MASK) >> SIM_SDID_REVID_SHIFT);
    uint32_t pinid = ((SIM->SDID & SIM_SDID_PINID_MASK) >> SIM_SDID_PINID_SHIFT);

    printf(" SIM_SDID:    %08" PRIx32 "\n", SIM->SDID);

#if SIM_SDID_FAMILYID_MASK
    /* Newer CPUs added the FAMILYID, SUBFAMID, etc. fields */
    uint32_t familyid = ((SIM->SDID & SIM_SDID_FAMILYID_MASK) >> SIM_SDID_FAMILYID_SHIFT);
    printf(" |- FAMILYID: %x\n", familyid)
#endif
#if SIM_SDID_SUBFAMID_MASK
    uint32_t subfamid = ((SIM->SDID & SIM_SDID_SUBFAMID_MASK) >> SIM_SDID_SUBFAMID_SHIFT);
    printf(" |- SUBFAMID:  %x\n", familyid)
#endif
#if SIM_SDID_SERIESID_MASK
    uint32_t seriesid = ((SIM->SDID & SIM_SDID_SERIESID_MASK) >> SIM_SDID_SERIESID_SHIFT);
    printf(" |- SERIESID:   %x\n", seriesid)
#endif
#if SIM_SDID_SRAMSIZE_MASK
    uint32_t sramsize = ((SIM->SDID & SIM_SDID_SRAMSIZE_MASK) >> SIM_SDID_SRAMSIZE_SHIFT);
    printf(" |- SRAMSIZE:    %x\n", sramsize)
#endif
    uint32_t famid = ((SIM->SDID & SIM_SDID_FAMID_MASK) >> SIM_SDID_FAMID_SHIFT);
    /* Older CPUs do not have the FAMILYID, SUBFAMID, etc. fields */
    printf(" |---- REVID:     %" PRIx32 "\n", revid);
    /* DIEID does not end on an even hex digit boundary */
    printf(" |---- DIEID:      %" PRIx32 " (0x%02" PRIx32 ")\n",
        (dieid << (SIM_SDID_DIEID_SHIFT % 4)),
        dieid);
    printf(" |---- FAMID:       %" PRIx32 "\n", famid);
    printf(" '---- PINID:        %" PRIx32 "\n", pinid);

    printf("Family: ");
#if SIM_SDID_SERIESID_MASK && SIM_SDID_SUBFAMID_MASK && SIM_SDID_SERIESID_MASK
    switch (seriesid) {
        case 0:
            putchar('K');
            break;
        case 1:
            putchar('L');
            break;
        case 5:
            putchar('W');
            break;
        case 6:
            putchar('V');
            break;
    }
    printf("%" PRId32 "%" PRId32 "\n", familyid, subfamid);
#else
    switch (famid) {
        case 0:
            printf("K10");
            break;
        case 1:
            printf("K20");
            break;
        case 2:
            printf("K30");
            break;
        case 3:
            printf("K40");
            break;
        case 4:
            printf("K60");
            break;
        case 5:
            printf("K70");
            break;
        case 6:
            printf("K50 or K52");
            break;
        case 7:
            printf("K51 or K53");
            break;
        default:
            printf("Unknown");
            break;
    }
    puts("");
#endif

    printf("Pin count: ");
    switch (pinid) {
        case 0:
            printf("16");
            break;
        case 1:
            printf("24");
            break;
        case 2:
            printf("32");
            break;
        case 3:
            printf("36");
            break;
        case 4:
            printf("48");
            break;
        case 5:
            printf("64");
            break;
        case 6:
            printf("80");
            break;
        case 7:
            printf("81 or 121");
            break;
        case 8:
            printf("100");
            break;
        case 9:
            printf("121");
            break;
        case 10:
            printf("144");
            break;
        case 11:
            printf("(Custom/WLCSP)");
            break;
        case 12:
            printf("196");
            break;
        case 14:
            printf("256");
            break;
        default:
            puts("(Unknown)");
            break;
    }
    puts("");

    printf("SRAM size: ");
#if SIM_SDID_SRAMSIZE_MASK
    if (sramsize >= 1) {
        printf("%d KiB\n", (1 << (sramsize - 1)));
    }
    else {
        puts("512 Bytes");
    }
#endif
#if SIM_SOPT1_RAMSIZE_MASK
    uint32_t ramsize = ((SIM->SOPT1 & SIM_SOPT1_RAMSIZE_MASK) >> SIM_SOPT1_RAMSIZE_SHIFT);
    if (ramsize > 0 && ramsize < 12) {
        printf("%d KiB\n", kinetis_size_reg_to_bytes(ramsize) * 2);
    }
    else {
        puts("(Unknown)");
    }

#endif
}

static void print_sim_fcfg(void)
{
    printf(" SIM_FCFG:   %08" PRIx32 " %08" PRIx32 "\n", SIM->FCFG1, SIM->FCFG2);
#if SIM_FCFG1_NVMSIZE_MASK
    uint32_t nvmsize = ((SIM->FCFG1 & SIM_FCFG1_NVMSIZE_MASK) >> SIM_FCFG1_NVMSIZE_SHIFT);
    printf(" |- NVMSIZE: %" PRIx32 "\n", nvmsize);
#endif
#if SIM_FCFG1_PFSIZE_MASK
    uint32_t pfsize = ((SIM->FCFG1 & SIM_FCFG1_PFSIZE_MASK) >> SIM_FCFG1_PFSIZE_SHIFT);
    printf(" |-  PFSIZE:  %" PRIx32 "\n", pfsize);
#endif
#if SIM_FCFG1_EESIZE_MASK
    uint32_t eesize = ((SIM->FCFG1 & SIM_FCFG1_EESIZE_MASK) >> SIM_FCFG1_EESIZE_SHIFT);
    printf(" |-  EESIZE:    %" PRIx32 "\n", eesize);
#endif
#if SIM_FCFG1_DEPART_MASK
    uint32_t depart = ((SIM->FCFG1 & SIM_FCFG1_DEPART_MASK) >> SIM_FCFG1_DEPART_SHIFT);
    printf(" '-  DEPART:      %" PRIx32 "\n", depart);
#endif

    printf("Flash size: ");
#if SIM_FCFG1_PFSIZE_MASK
    printf("%d KiB\n", kinetis_size_reg_to_bytes(pfsize) * 4);
#else
    puts("Unknown!");
#endif

    printf("FlexNVM size: ");
#if SIM_FCFG1_NVMSIZE_MASK
    if (nvmsize == 0) {
        printf("0\n");
    }
    else {
        printf("%d KiB\n", kinetis_size_reg_to_bytes(nvmsize) * 4);
    }
#else
    puts("Unknown!");
#endif

    printf("EEPROM size: ");
#if SIM_FCFG1_EESIZE_MASK
    if (eesize < 10 || eesize == 15) {
        printf("%d Bytes\n", (16384 >> eesize));
    }
    else {
        puts("(Unknown)");
    }
#else
    puts("Unknown!");
#endif
}


static void print_active_peripheral_clocks(void)
{
    puts("Active peripheral clocks:");
    /* Use the following one-liner to generate the code: */
    /*
     cat cpu/k60/include/MK60DZ10.h cpu/k60/include/MK60D10.h | \
     sed -n -e 's,.*SIM_SCGC\([0-9]\)_\([^_]*\)_MASK.*,SIM_SCGC\1 \2,p' | \
     LC_ALL=C sort -k 2 | uniq | awk -F' ' \
     '{ printf "#if "$1"_"$2"_MASK\n    if ("$1" & "$1"_"$2"_MASK) {\n        puts(\""$2"\");\n    }\n#endif\n";}'
     */
#if SIM_SCGC6_ADC0_MASK
    if (SIM_SCGC6 & SIM_SCGC6_ADC0_MASK) {
        puts("ADC0");
    }
#endif
#if SIM_SCGC3_ADC1_MASK
    if (SIM_SCGC3 & SIM_SCGC3_ADC1_MASK) {
        puts("ADC1");
    }
#endif
#if SIM_SCGC4_CMP_MASK
    if (SIM_SCGC4 & SIM_SCGC4_CMP_MASK) {
        puts("CMP");
    }
#endif
#if SIM_SCGC4_CMT_MASK
    if (SIM_SCGC4 & SIM_SCGC4_CMT_MASK) {
        puts("CMT");
    }
#endif
#if SIM_SCGC6_CRC_MASK
    if (SIM_SCGC6 & SIM_SCGC6_CRC_MASK) {
        puts("CRC");
    }
#endif
#if SIM_SCGC2_DAC0_MASK
    if (SIM_SCGC2 & SIM_SCGC2_DAC0_MASK) {
        puts("DAC0");
    }
#endif
#if SIM_SCGC2_DAC1_MASK
    if (SIM_SCGC2 & SIM_SCGC2_DAC1_MASK) {
        puts("DAC1");
    }
#endif
#if SIM_SCGC7_DMA_MASK
    if (SIM_SCGC7 & SIM_SCGC7_DMA_MASK) {
        puts("DMA");
    }
#endif
#if SIM_SCGC6_DMAMUX_MASK
    if (SIM_SCGC6 & SIM_SCGC6_DMAMUX_MASK) {
        puts("DMAMUX");
    }
#endif
#if SIM_SCGC6_DSPI0_MASK
    if (SIM_SCGC6 & SIM_SCGC6_DSPI0_MASK) {
        puts("DSPI0");
    }
#endif
#if SIM_SCGC2_ENET_MASK
    if (SIM_SCGC2 & SIM_SCGC2_ENET_MASK) {
        puts("ENET");
    }
#endif
#if SIM_SCGC4_EWM_MASK
    if (SIM_SCGC4 & SIM_SCGC4_EWM_MASK) {
        puts("EWM");
    }
#endif
#if SIM_SCGC7_FLEXBUS_MASK
    if (SIM_SCGC7 & SIM_SCGC7_FLEXBUS_MASK) {
        puts("FLEXBUS");
    }
#endif
#if SIM_SCGC6_FLEXCAN0_MASK
    if (SIM_SCGC6 & SIM_SCGC6_FLEXCAN0_MASK) {
        puts("FLEXCAN0");
    }
#endif
#if SIM_SCGC3_FLEXCAN1_MASK
    if (SIM_SCGC3 & SIM_SCGC3_FLEXCAN1_MASK) {
        puts("FLEXCAN1");
    }
#endif
#if SIM_SCGC6_FTFL_MASK
    if (SIM_SCGC6 & SIM_SCGC6_FTFL_MASK) {
        puts("FTFL");
    }
#endif
#if SIM_SCGC6_FTM0_MASK
    if (SIM_SCGC6 & SIM_SCGC6_FTM0_MASK) {
        puts("FTM0");
    }
#endif
#if SIM_SCGC6_FTM1_MASK
    if (SIM_SCGC6 & SIM_SCGC6_FTM1_MASK) {
        puts("FTM1");
    }
#endif
#if SIM_SCGC3_FTM2_MASK
    if (SIM_SCGC3 & SIM_SCGC3_FTM2_MASK) {
        puts("FTM2");
    }
#endif
#if SIM_SCGC4_I2C0_MASK
    if (SIM_SCGC4 & SIM_SCGC4_I2C0_MASK) {
        puts("I2C0");
    }
#endif
#if SIM_SCGC4_I2C1_MASK
    if (SIM_SCGC4 & SIM_SCGC4_I2C1_MASK) {
        puts("I2C1");
    }
#endif
#if SIM_SCGC6_I2S_MASK
    if (SIM_SCGC6 & SIM_SCGC6_I2S_MASK) {
        puts("I2S");
    }
#endif
#if SIM_SCGC4_LLWU_MASK
    if (SIM_SCGC4 & SIM_SCGC4_LLWU_MASK) {
        puts("LLWU");
    }
#endif
#if SIM_SCGC5_LPTIMER_MASK
    if (SIM_SCGC5 & SIM_SCGC5_LPTIMER_MASK) {
        puts("LPTIMER");
    }
#endif
#if SIM_SCGC7_MPU_MASK
    if (SIM_SCGC7 & SIM_SCGC7_MPU_MASK) {
        puts("MPU");
    }
#endif
#if SIM_SCGC6_PDB_MASK
    if (SIM_SCGC6 & SIM_SCGC6_PDB_MASK) {
        puts("PDB");
    }
#endif
#if SIM_SCGC6_PIT_MASK
    if (SIM_SCGC6 & SIM_SCGC6_PIT_MASK) {
        puts("PIT");
    }
#endif
#if SIM_SCGC5_PORTA_MASK
    if (SIM_SCGC5 & SIM_SCGC5_PORTA_MASK) {
        puts("PORTA");
    }
#endif
#if SIM_SCGC5_PORTB_MASK
    if (SIM_SCGC5 & SIM_SCGC5_PORTB_MASK) {
        puts("PORTB");
    }
#endif
#if SIM_SCGC5_PORTC_MASK
    if (SIM_SCGC5 & SIM_SCGC5_PORTC_MASK) {
        puts("PORTC");
    }
#endif
#if SIM_SCGC5_PORTD_MASK
    if (SIM_SCGC5 & SIM_SCGC5_PORTD_MASK) {
        puts("PORTD");
    }
#endif
#if SIM_SCGC5_PORTE_MASK
    if (SIM_SCGC5 & SIM_SCGC5_PORTE_MASK) {
        puts("PORTE");
    }
#endif
#if SIM_SCGC5_REGFILE_MASK
    if (SIM_SCGC5 & SIM_SCGC5_REGFILE_MASK) {
        puts("REGFILE");
    }
#endif
#if SIM_SCGC3_RNGA_MASK
    if (SIM_SCGC3 & SIM_SCGC3_RNGA_MASK) {
        puts("RNGA");
    }
#endif
#if SIM_SCGC6_RNGA_MASK
    if (SIM_SCGC6 & SIM_SCGC6_RNGA_MASK) {
        puts("RNGA");
    }
#endif
#if SIM_SCGC3_RNGB_MASK
    if (SIM_SCGC3 & SIM_SCGC3_RNGB_MASK) {
        puts("RNGB");
    }
#endif
#if SIM_SCGC6_RTC_MASK
    if (SIM_SCGC6 & SIM_SCGC6_RTC_MASK) {
        puts("RTC");
    }
#endif
#if SIM_SCGC3_SDHC_MASK
    if (SIM_SCGC3 & SIM_SCGC3_SDHC_MASK) {
        puts("SDHC");
    }
#endif
#if SIM_SCGC6_SPI0_MASK
    if (SIM_SCGC6 & SIM_SCGC6_SPI0_MASK) {
        puts("SPI0");
    }
#endif
#if SIM_SCGC6_SPI1_MASK
    if (SIM_SCGC6 & SIM_SCGC6_SPI1_MASK) {
        puts("SPI1");
    }
#endif
#if SIM_SCGC3_SPI2_MASK
    if (SIM_SCGC3 & SIM_SCGC3_SPI2_MASK) {
        puts("SPI2");
    }
#endif
#if SIM_SCGC5_TSI_MASK
    if (SIM_SCGC5 & SIM_SCGC5_TSI_MASK) {
        puts("TSI");
    }
#endif
#if SIM_SCGC4_UART0_MASK
    if (SIM_SCGC4 & SIM_SCGC4_UART0_MASK) {
        puts("UART0");
    }
#endif
#if SIM_SCGC4_UART1_MASK
    if (SIM_SCGC4 & SIM_SCGC4_UART1_MASK) {
        puts("UART1");
    }
#endif
#if SIM_SCGC4_UART2_MASK
    if (SIM_SCGC4 & SIM_SCGC4_UART2_MASK) {
        puts("UART2");
    }
#endif
#if SIM_SCGC4_UART3_MASK
    if (SIM_SCGC4 & SIM_SCGC4_UART3_MASK) {
        puts("UART3");
    }
#endif
#if SIM_SCGC1_UART4_MASK
    if (SIM_SCGC1 & SIM_SCGC1_UART4_MASK) {
        puts("UART4");
    }
#endif
#if SIM_SCGC1_UART5_MASK
    if (SIM_SCGC1 & SIM_SCGC1_UART5_MASK) {
        puts("UART5");
    }
#endif
#if SIM_SCGC6_USBDCD_MASK
    if (SIM_SCGC6 & SIM_SCGC6_USBDCD_MASK) {
        puts("USBDCD");
    }
#endif
#if SIM_SCGC4_USBOTG_MASK
    if (SIM_SCGC4 & SIM_SCGC4_USBOTG_MASK) {
        puts("USBOTG");
    }
#endif
#if SIM_SCGC4_VREF_MASK
    if (SIM_SCGC4 & SIM_SCGC4_VREF_MASK) {
        puts("VREF");
    }
#endif
}

