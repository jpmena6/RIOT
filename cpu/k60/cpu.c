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
 * @ingroup     cpu_k60
 * @{
 *
 * @file
 * @brief       Implementation of K60 CPU initialization.
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

/** @brief Current core clock frequency */
uint32_t SystemCoreClock = DEFAULT_SYSTEM_CLOCK;
/** @brief Current system clock frequency */
uint32_t SystemSysClock = DEFAULT_SYSTEM_CLOCK;
/** @brief Current bus clock frequency */
uint32_t SystemBusClock = DEFAULT_SYSTEM_CLOCK;
/** @brief Current FlexBus clock frequency */
uint32_t SystemFlexBusClock = DEFAULT_SYSTEM_CLOCK;
/** @brief Current flash clock frequency */
uint32_t SystemFlashClock = DEFAULT_SYSTEM_CLOCK;
/** @brief Number of full PIT ticks in one microsecond. */
uint32_t PIT_ticks_per_usec = (DEFAULT_SYSTEM_CLOCK / 1000000ul);

/**
 * @brief Check the running CPU identification to find if we are running on the
 *        wrong hardware.
 */
static void check_running_cpu_revision(void);

/**
 * @brief Initialize the CPU, set IRQ priorities
 */
void cpu_init(void)
{
    /* initialize the Cortex-M core */
    cortexm_init();
    /* Check that we are running on the CPU that this code was built for */
    check_running_cpu_revision();
}

static void check_running_cpu_revision(void)
{
    /* Check that the running CPU revision matches the compiled revision */
    if (SCB->CPUID != K60_EXPECTED_CPUID) {
        uint32_t CPUID = SCB->CPUID; /* This is only to ease debugging, type
                   * "print /x CPUID" in gdb */
        uint32_t SILICON_REVISION = (SCB->CPUID & SCB_CPUID_REVISION_Msk) + 1;
        (void)CPUID; /* prevents compiler warnings about an unused variable. */
        (void)SILICON_REVISION;

        /* Running on the wrong CPU, the clock initialization is different
         * between silicon revision 1.x and 2.x (LSB of CPUID) */
        /* If you unexpectedly end up on this line when debugging:
         * Rebuild the code using the correct value for K60_CPU_REV */
        __asm__ volatile ("bkpt #99\n");

        while (1);
    }
}

void SystemCoreClockUpdate(void)
{
    /* Variable to store output clock frequency of the MCG module */
    uint32_t MCGOUT_clock;

    if ((MCG->C1 & MCG_C1_CLKS_MASK) == 0x0u) {
        /* Output of FLL or PLL is selected */
        if ((MCG->C6 & MCG_C6_PLLS_MASK) == 0x0u) {
            /* FLL is selected */
            if ((MCG->C1 & MCG_C1_IREFS_MASK) == 0x0u) {
                /* External reference clock is selected */
#if K60_CPU_REV == 1
                /* rev.1 silicon */
                if ((SIM->SOPT2 & SIM_SOPT2_MCGCLKSEL_MASK) == 0x0u) {
                    /* System oscillator drives MCG clock */
                    MCGOUT_clock = CPU_XTAL_CLK_HZ;
                }
                else {
                    /* RTC 32 kHz oscillator drives MCG clock */
                    MCGOUT_clock = CPU_XTAL32k_CLK_HZ;
                }

#else /* K60_CPU_REV */

                /* rev.2 silicon */
                if ((MCG->C7 & MCG_C7_OSCSEL_MASK) == 0x0u) {
                    /* System oscillator drives MCG clock */
                    MCGOUT_clock = CPU_XTAL_CLK_HZ;
                }
                else {
                    /* RTC 32 kHz oscillator drives MCG clock */
                    MCGOUT_clock = CPU_XTAL32k_CLK_HZ;
                }

#endif /* K60_CPU_REV */
                uint8_t divider = (uint8_t)(1u << ((MCG->C1 & MCG_C1_FRDIV_MASK) >> MCG_C1_FRDIV_SHIFT));
                /* Calculate the divided FLL reference clock */
                MCGOUT_clock /= divider;

                if ((MCG->C2 & MCG_C2_RANGE0_MASK) != 0x0u) {
                    /* If high range is enabled, additional 32 divider is active */
                    MCGOUT_clock /= 32u;
                }
            }
            else {
                /* The slow internal reference clock is selected */
                MCGOUT_clock = CPU_INT_SLOW_CLK_HZ;
            }

            /* Select correct multiplier to calculate the MCG output clock  */
            switch (MCG->C4 & (MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS_MASK)) {
                case (0x0u):
                    MCGOUT_clock *= 640u;
                    break;

                case (MCG_C4_DRST_DRS(0b01)): /* 0x20u */
                    MCGOUT_clock *= 1280u;
                    break;

                case (MCG_C4_DRST_DRS(0b10)): /* 0x40u */
                    MCGOUT_clock *= 1920u;
                    break;

                case (MCG_C4_DRST_DRS(0b11)): /* 0x60u */
                    MCGOUT_clock *= 2560u;
                    break;

                case (MCG_C4_DMX32_MASK): /* 0x80u */
                    MCGOUT_clock *= 732u;
                    break;

                case (MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0b01)): /* 0xA0u */
                    MCGOUT_clock *= 1464u;
                    break;

                case (MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0b10)): /* 0xC0u */
                    MCGOUT_clock *= 2197u;
                    break;

                case (MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0b11)): /* 0xE0u */
                    MCGOUT_clock *= 2929u;
                    break;

                default:
                    break;
            }
        }
        else {
            /* PLL is selected */
            /* Calculate the PLL reference clock */
            uint8_t divider = (1u + (MCG->C5 & MCG_C5_PRDIV0_MASK));
            MCGOUT_clock = (uint32_t)(CPU_XTAL_CLK_HZ / divider);
            /* Calculate the MCG output clock */
            divider = ((MCG->C6 & MCG_C6_VDIV0_MASK) + 24u);
            MCGOUT_clock *= divider;
        }
    }
    else if ((MCG->C1 & MCG_C1_CLKS_MASK) == MCG_C1_CLKS(0b01)) {   /* 0x40u */
        /* Internal reference clock is selected */
        if ((MCG->C2 & MCG_C2_IRCS_MASK) == 0x0u) {
            /* Slow internal reference clock selected */
            MCGOUT_clock = CPU_INT_SLOW_CLK_HZ;
        }
        else {
            /* Fast internal reference clock selected */
#if K60_CPU_REV == 1
            /* rev.1 silicon */
            MCGOUT_clock = CPU_INT_FAST_CLK_HZ;
#else /* K60_CPU_REV */
            /* rev.2 silicon */
            MCGOUT_clock = CPU_INT_FAST_CLK_HZ /
                           (1 << ((MCG->SC & MCG_SC_FCRDIV_MASK) >> MCG_SC_FCRDIV_SHIFT));
#endif /* K60_CPU_REV */
        }
    }
    else if ((MCG->C1 & MCG_C1_CLKS_MASK) == MCG_C1_CLKS(0b10)) {   /* 0x80u */
        /* External reference clock is selected */
#if K60_CPU_REV == 1
        /* rev.1 silicon */
        if ((SIM->SOPT2 & SIM_SOPT2_MCGCLKSEL_MASK) == 0x0u) {
            /* System oscillator drives MCG clock */
            MCGOUT_clock = CPU_XTAL_CLK_HZ;
        }
        else {
            /* RTC 32 kHz oscillator drives MCG clock */
            MCGOUT_clock = CPU_XTAL32k_CLK_HZ;
        }

#else /* K60_CPU_REV */

        /* rev.2 silicon */
        if ((MCG->C7 & MCG_C7_OSCSEL_MASK) == 0x0u) {
            /* System oscillator drives MCG clock */
            MCGOUT_clock = CPU_XTAL_CLK_HZ;
        }
        else {
            /* RTC 32 kHz oscillator drives MCG clock */
            MCGOUT_clock = CPU_XTAL32k_CLK_HZ;
        }

#endif /* K60_CPU_REV */
    }
    else {
        /* Reserved value */
        return;
    }

    /* Core clock and system clock use the same divider setting */
    SystemCoreClock = SystemSysClock = (MCGOUT_clock / (1u + ((SIM->CLKDIV1 & SIM_CLKDIV1_OUTDIV1_MASK)
                                        >> SIM_CLKDIV1_OUTDIV1_SHIFT)));
    SystemBusClock = (MCGOUT_clock / (1u + ((SIM->CLKDIV1 & SIM_CLKDIV1_OUTDIV2_MASK) >>
                                            SIM_CLKDIV1_OUTDIV2_SHIFT)));
    SystemFlexBusClock = (MCGOUT_clock / (1u + ((SIM->CLKDIV1 & SIM_CLKDIV1_OUTDIV3_MASK) >>
                                          SIM_CLKDIV1_OUTDIV3_SHIFT)));
    SystemFlashClock = (MCGOUT_clock / (1u + ((SIM->CLKDIV1 & SIM_CLKDIV1_OUTDIV4_MASK) >>
                                        SIM_CLKDIV1_OUTDIV4_SHIFT)));

    /* Module helper variables */
    if (SystemBusClock >= 1000000) {
        /* PIT module clock_delay_usec scale factor */
        PIT_ticks_per_usec = (SystemBusClock + 500000) / 1000000; /* Rounded to nearest integer */
    }
    else {
        /* less than 1 MHz clock frequency on the PIT module, round up. */
        PIT_ticks_per_usec = 1;
    }
}

static void print_active_peripheral_clocks(void);

void print_cpu_info(void)
{
    puts("CPU info:");
    puts("=========");
    puts("");

    printf("SCB_CPUID: 0x%08" PRIx32 "\n", SCB->CPUID);
    printf("  SIM_UID: %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 "\n",
        SIM->UIDH, SIM->UIDMH, SIM->UIDML, SIM->UIDL);
    printf(" SIM_SDID: %08" PRIx32 "\n", SIM->SDID);
    printf(" |- REVID: %5x\n", (unsigned int)((SIM->SDID & SIM_SDID_REVID_MASK) >> SIM_SDID_REVID_SHIFT));
    printf(" |- FAMID: %7x   (", (unsigned int)((SIM->SDID & SIM_SDID_FAMID_MASK) >> SIM_SDID_FAMID_SHIFT));
    switch ((SIM->SDID & SIM_SDID_FAMID_MASK) >> SIM_SDID_FAMID_SHIFT) {
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
            printf("K50, K52");
            break;
        case 7:
            printf("K51, K53");
            break;
        default:
            printf("Unknown");
            break;
    }
    puts(")");

    printf(" '- PINID: %8x  (", (unsigned int)((SIM->SDID & SIM_SDID_PINID_MASK) >> SIM_SDID_PINID_SHIFT));
    switch ((SIM->SDID & SIM_SDID_PINID_MASK) >> SIM_SDID_PINID_SHIFT) {
        case 2:
            printf("32");
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
            printf("81");
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
    puts("-pin)");

    printf(" SIM_FCFG: %08" PRIx32 " %08" PRIx32 "\n", SIM->FCFG1, SIM->FCFG2);
    printf(" '- Flash size: ");
    switch ((SIM->FCFG1 & SIM_FCFG1_PFSIZE_MASK) >> SIM_FCFG1_PFSIZE_SHIFT) {
        case 7:
            printf("128");
            break;
        case 9:
            printf("256");
            break;
        case 11:
            printf("512");
            break;
        case 15:
            if ((SIM->FCFG2 & SIM_FCFG2_PFLSH_MASK) == 0){
                printf("256");
            }
            else {
                printf("512");
            }
            break;
        default:
            puts("(Unknown)");
            break;
    }
    puts(" KiB");

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

/** @} */
