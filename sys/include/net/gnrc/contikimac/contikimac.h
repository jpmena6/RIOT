/*
 * Copyright (C) 2017 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_gnrc_contikimac ContikiMAC compatible MAC layer
 * @ingroup     net_gnrc
 * @brief       Duty cycling MAC protocol for low power communication over IEEE 802.15.4 networks
 *
 * @see Dunkels, A. (2011). The contikimac radio duty cycling protocol. http://soda.swedish-ict.se/5128/1/contikimac-report.pdf
 * @see Michel, M., & Quoitin, B. (2014). Technical report: ContikiMAC vs X-MAC performance analysis. arXiv preprint arXiv:1404.3589. https://arxiv.org/abs/1404.3589
 *
 * # Summary
 *
 * ContikiMAC is a duty cycling MAC layer protocol which uses strobing (repeated
 * transmissions) of actual data packets to ensure that sleeping nodes will
 * receive the transmission as they wake up. This is similar to the X-MAC
 * protocol, except X-MAC uses special strobe frames to signal incoming data
 * instead of the actual data frame.
 *
 * \note This implementation supports the ContikiMAC fast sleep optimization.
 *
 * \todo Add support for the ContikiMAC phase lock optimization
 *
 * \todo Add support for the ContikiMAC burst optimization (frame pending field)
 *
 * # Algorithm description
 *
 * ContikiMAC nodes wake the radio at a constant interval, e.g. every 125 ms,
 * and performs a number of CCA checks to check the radio medium for energy. If
 * one of the CCA checks report that the medium is busy, the radio is switched
 * to listening mode and waiting for incoming packets. If the CCA checks all
 * report channel idle, the radio is put to sleep.
 * When listening, if no packet is received before a timeout is hit, the radio
 * is put back to sleep.
 * If a packet is received correctly, the radio is put back to sleep immediately.
 *
 * ## Optimizations
 *
 * The MAC layer can use knowledge of the algorithm behavior to further reduce
 * the power usage.
 *
 * ### Fast sleep
 *
 * If a CCA check signals channel busy, but further CCA checks do not detect any
 * silence after the time it takes to transmit the longest possible packet, the
 * MAC layer will assume that the detected radio energy is noise from some other
 * source, e.g. WiFi networks or microwave ovens, and put the radio back to sleep.
 *
 * If a CCA check signals channel busy, and then some silence is detected, but
 * there is no reported incoming reception from the radio after the strobe
 * interval has passed, the radio is put back to sleep.
 *
 * ### Phase lock
 *
 * \note Not yet implemented in gnrc_contikimac
 *
 * If a unicast transmission succeeds with a correctly received Ack packet, the
 * sender can record the time of the last transmission start. The next time a
 * unicast packet is directed to the same destination, the registered phase will
 * be used as a reference, and the tranmission will be started right before the
 * receiver is expected to wake up.
 *
 * ### Burst transmission
 *
 * \note Not yet implemented in gnrc_contikimac
 *
 * If a sender has more data than fits in a single frame, a special flag is used
 * to tell the receiver to keep the radio turned on and listening after the
 * current frame has been received. For IEEE 802.15.4, the Frame Pending flag in
 * the Frame Control Field is used for this purpose. This optimization increases
 * network throughput if used correctly. The 6lowpan fragmentation module
 * automatically tells the MAC layer to set this flag on any fragmented packets,
 * but it can also be used from the application layer if the application knows
 * that there will be more data immediately after the current.
 *
 * # Implementation details
 *
 * This section gives some extra information regarding this specific
 * implementation.
 *
 * The timing is handled by the xtimer system, this means that the platform
 * needs to use a low power timer for xtimer in order to use the low power modes
 * of the MCU. At the time of writing (2017-09-16), most boards' default
 * configurations are using high precision timers for xtimer (one notable
 * exception is the frdm-kw41z board), but many boards can be easily
 * reconfigured to use low power timers via board.h. Check your CPU manual for
 * the proper timer to use.
 *
 * All radio state switching, CCA checks, etc. are called via the netdev get/set
 * API. This makes the code platform independent, but the radio drivers need to
 * support the options used by the implementation. The options required for full
 * functionality are:
 *
 * - NETOPT_PRELOADING, for loading the TX frame once and transmitting many times
 * - NETOPT_STATE_TX, for triggering retransmission
 * - NETOPT_STATE_STANDBY, for radio low power CCA checking
 * - NETOPT_STATE_SLEEP, for radio low power mode
 * - NETOPT_STATE_IDLE, for radio RX listen
 * - NETOPT_CSMA, to disable hardware CSMA, not required if the radio does not perform CSMA
 * - NETOPT_RETRANS, to disable automatic retransmissions)
 * - NETOPT_TX_END_IRQ, to be alerted about end of TX)
 * - NETOPT_RX_START_IRQ, to be alerted about incoming frames)
 * - NETOPT_RX_END_IRQ, to be alerted about incoming frames)
 * - NETOPT_IS_CHANNEL_CLR, for performing CCA checks
 *
 * Additionally, the radio must allow the same frame to be transmitted multiple
 * times. The implementation will switch the radio to standby before any TX
 * preloading, to avoid corrupting the TX buffer with incoming RX packets on
 * single buffered devices, e.g. at86rf2xx. The device driver must allow
 * multiple calls to NETOPT_STATE_TX after a single preload, for retransmissions
 * while strobing.
 * NETOPT_IS_CHANNEL_CLR, netdev->send(), and NETOPT_STATE_TX are called while
 * the radio is in NETOPT_STATE_STANDBY.
 *
 * ## Fast sleep
 *
 * During a wake up, the fast sleep implementation will perform additional
 * periodic CCA checks after the first energy detection on the channel. The
 * periodic CCA checks continue until either an idle channel is detected, or a
 * timeout occurs. The timeout must be greater than the time it takes to
 * transmit the longest possible frame, or else it may time out before seeing
 * the end of a packet if the first ED occurred right at the beginning of the
 * frame. After silence is detected, the radio is switched to listening state,
 * and a new timeout is set. If an RX begin event occurs before the timeout, the
 * timeout is incremented to the length of the longest frame, to allow for the
 * complete reception. If the timeout is hit, the radio is put back to sleep. No
 * further CCA checks are performed after switching the radio to listening state,
 * to avoid interfering with the frame reception.
 *
 * # ContikiMAC Timing constraints
 *
 * In order to ensure reliable transmissions while duty cycling the receiver,
 * there are some constraints on the timings for the ContikiMAC algorithm.
 *
 * These constraints are also given in [dunkels11], but written with an implicit
 * \f$n_c = 2\f$.
 *
 * To reliably detect Ack packets:
 *
 * \f[T_a + T_d < T_i\f]
 *
 * To reliably detect incoming packets during CCA cycles:
 *
 * \f[T_i < (n_c - 1) \cdot T_c + n_c \cdot T_r)\f]
 *
 * and
 *
 * \f[(T_c + 2 T_r) < T_s\f]
 *
 * The variables in the above conditions are described below:
 *
 * \f$T_a\f$ is the time between reception end and Ack TX begin.
 *
 * \f$T_d\f$ is the time it takes for the transceiver to receive the Ack packet.
 *
 * \f$T_i\f$ is the time between the end of transmission, and the start of retransmission
 *
 * \f$T_c\f$ is the time between CCA checks during CCA cycles
 *
 * \f$n_c\f$ is the maximum number of CCA checks to perform during the CCA cycle
 *
 * \f$T_r\f$ is the time it takes to perform one CCA check
 *
 * \f$T_s\f$ is the time it takes to transmit the shortest allowed frame
 *
 * The constraint on \f$T_s\f$ yields a minimum packet length in bytes:
 *
 * \f[T_s = n_s \cdot T_b \Leftrightarrow n_s = \frac{T_s}{T_b}\f]
 *
 * where \f$n_s\f$ is the number of bytes in the shortest packet, and \f$T_b\f$
 * is the time it takes to transmit one byte.
 *
 * For packets shorter than \f$n_s\f$ bytes, extra padding must be added to ensure
 * reliable transmission, or else the packet may fall between two CCA checks and
 * remain undetected.
 *
 * From the above equations it can be seen that using \f$n_c > 2\f$ relaxes the
 * constraint on minimum packet length, making it possible to eliminate the extra
 * frame padding completely, at the cost of additional CCA checks.
 *
 * ## Fast sleep
 *
 * For fast sleep, some additional timing information is needed.
 *
 *
 *
 * \f[\f]
 *
 * ## Timing parameters for O-QPSK 250 kbit/s
 *
 * O-QPSK 250 kbit/s is the most widely used mode for 802.15.4 radios in the 2.4 GHz band.
 *
 * \f[T_a = 12~\mathrm{symbols} = 192~\mathrm{\mu{}s}\f]
 *
 * Specified by the standard (AIFS = macSifsPeriod = aTurnaroundTime)
 *
 * \f[T_d = 5 + 1 + 5~\mathrm{bytes} = 352~\mathrm{\mu{}s}\f]
 *
 * A standard Ack packet is 5 bytes long, the preamble and start-of-frame
 * delimiter (SFD) is 5 bytes, and the PHY header (PHR) is 1 byte.
 *
 * \f[T_r = 8~\mathrm{symbols} = 128~\mathrm{\mu{}s}\f]
 *
 * Specified by the standard (aCcaTime)
 *
 * \f[T_b = 2~\mathrm{symbols} = 32~\mathrm{\mu{}s}\f]
 *
 * Specified by the standard (4 bits per symbol)
 *
 * Additionally, the hardware may have some timing constraints as well. For
 * example, the at86rf2xx transceiver has a fixed Ack timeout (when using
 * hardware Ack reception) of 54 symbols (\f$864~\mathrm{\mu{}s}\f$), this means
 * that the configuration must satisfy \f$T_i > 864~\mathrm{\mu{}s}\f$ if
 * using a at86rf2xx transceiver.
 *
 * Due to CPU processing constraints, there are lower limits on all timings. For
 * example, the reception and CCA check results need to be processed by the CPU
 * and passed to the ContikiMAC thread, which may not be an insignificant time
 * depending on the CPU speed and the radio interface bus speed (SPI, UART etc.).
 *
 * # Configuring timing parameters
 *
 * The timing parameters are set using an instance of contikimac_params_t.
 * Some parameters can be automatically derived from the other parameters. The
 * constants that must be configured manually for a minimum working configuration
 * are:
 *
 * - \f$n_s\f$, minimum allowed frame size
 * - \f$T_a + T_d\f$, Ack wait timeout
 * - \f$T_c\f$, the time between consecutive CCA checks
 * - \f$T_w\f$, the time between wake ups
 *
 * ## Derived parameters
 *
 * The parameters below can be derived from the parameters manually configured
 * in the previous section.
 *
 * - \f$n_c\f$, the maximum number of CCA checks in each wake up
 * - \f$T_l\f$, the transmission time for the longest possible packet
 *
 * @{
 *
 * @file
 * @brief       Interface definition for the ContikiMAC layer
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#ifndef NET_GNRC_CONTIKIMAC_CONTIKIMAC_H
#define NET_GNRC_CONTIKIMAC_CONTIKIMAC_H

#include "kernel_types.h"
#include "net/gnrc/netdev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a network interface with ContikiMAC
 *
 * The initialization starts a new thread that connects to the given netdev
 * device and starts a link layer event loop.
 *
 * @param[in] stack         stack for the control thread
 * @param[in] stacksize     size of *stack*
 * @param[in] priority      priority for the thread
 * @param[in] name          name of the thread
 * @param[in] dev           netdev device, needs to be already initialized
 *
 * @return                  PID of thread on success
 * @return                  -EINVAL if creation of thread fails
 * @return                  -ENODEV if *dev* is invalid
 */
kernel_pid_t gnrc_contikimac_init(char *stack, int stacksize, char priority,
                             const char *name, gnrc_netdev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LWMAC_LWMAC_H */
/** @} */
