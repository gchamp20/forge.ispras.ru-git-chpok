/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 */

//TODO move define to config.h or delete

#include "syspart_config.h"

#ifdef SYS_NEEDS_DRIVER_NE2000

#  include <libc/stdio.h>
#  include <libc/string.h>
#  include <pci.h>
//#  include <arch/x86/ioports.h>
#  include <middleware/port.h>

//TODO: should be 2**16?
#  define ETH_DATA_MAXLEN	65536
#  define ETH_DATA_MINLEN	46
#  define ETH_MAC_LEN		6

#  define RECV_BUF_SZ		20

#  define NE2000_ETH_DATA_MINLEN 64

typedef struct
{
    char data[ETH_DATA_MAXLEN];
} pok_packet_t;

typedef struct
{
    s_pci_dev  pci;
    unsigned int  addr;
    uint8_t       mac[6];
    void (*packet_received_callback)(const char *, size_t);
} s_ne2000_dev;

typedef struct
{
    uint8_t status;
    uint8_t next;
    uint16_t size;
} __attribute__ ((packed)) s_ne2000_header;

/* CR: Command Register */
#  define NE2000_CR	0x00	 /* Register address */
#  define NE2000_CR_STP	(1 << 0) /* This bit is the STOP command. When it is
                                    set, no packets will be received or
                                    transmitted. POWER UP=1. */
#  define NE2000_CR_STA	(1 << 1) /* The STA bit controls nothing. It only
                                    reflects the value written to this bit.
                                    POWER UP=0. */
#  define NE2000_CR_TXP	(1 << 2) /* This bit must be set to transmit a packet.
                                    It is internally reset either after the
                                    transmission is completed or aborted. */
#  define NE2000_CR_RD0	(1 << 3) /* See RD2-0 in the datasheet */
#  define NE2000_CR_RD1	(1 << 4) /* See RD2-0 in the datasheet */
#  define NE2000_CR_RD2	(1 << 5) /* See RD2-0 in the datasheet */
#  define NE2000_CR_PS0	(1 << 6) /* See PS0 in the datasheet */
#  define NE2000_CR_PS1	(1 << 7) /* See PS1 in the datasheet */

/* ISR: Interrupt Status Register */
#  define NE2000_ISR	0x07	 /* Register address */

#  define NE2000_ISR_PRX	(1 << 0) /* This bit indicates packet received with no
                                            errors. */
#  define NE2000_ISR_PTX	(1 << 1) /* This bit indicates packet transmitted with
                                            no errors. */
#  define NE2000_ISR_RXE	(1 << 2) /* This bit is set when a packet received
                                            with one or more of the following errors:
                                            - CRC error
                                            - Frame alignment error
                                            - Missed packet */
#  define NE2000_ISR_TXE	(1 << 3) /* Transmit error bit is set when a packet
                                            transmission is aborted due to excessive
                                            collisions. */
#  define NE2000_ISR_OVW	(1 << 4) /* This bit is set when the receive buffer
                                            has been exhausted. */
#  define NE2000_ISR_CNT	(1 << 5) /* Set when MSB of one or more of the network
                                            tally counters has been set. */
#  define NE2000_ISR_RDC	(1 << 6) /* Set when remote DMA operation has been
                                            completed. */
#  define NE2000_ISR_RST (1 << 7) /* This bit is set when NIC enters reset state
                                     and is cleared when a start command is
                                     issued to the CR. It is also set when
                                     receive buffer overflows and is cleared
                                     when one or more packets have been read from
                                     the buffer. */

/* DCR: Data Configuration Register */
#  define NE2000_DCR	0x0E	 /* Register address */
#  define NE2000_DCR_WTS	(1 << 0) /* Word Transfer Select
0: byte-wide DMA transfer
1: word-wide DMA transfer */
#  define NE2000_DCR_BOS	(1 << 1) /* Byte Order Select
0: MS byte placed on MD15-8 and LS byte on MD7-0. (32xxx,80x86)
1: MS byte placed on MD7-0 and LS byte on MD15-8. (680x0) */
#  define NE2000_DCR_LAS	(1 << 2) /* This bit must be set to zero. NIC only supports dual 16-bit DMA
                                            mode. POWER UP =1 */
#  define NE2000_DCR_LS	(1 << 3) /* Loopback Select
0: Loopback mode selected. Bits 1 and 2 of the TCR must also be
programmed for Loopback operation.
1: Normal Operation */
#  define NE2000_DCR_ARM	(1 << 4) /* Auto-initialize Remote
0: Send Packet Command not executed.
1: Send Packet Command executed.
This bit is set when the receive buffer */
#  define NE2000_DCR_FT0	(1 << 5) /* FIFO threshold select bit 0. */
#  define NE2000_DCR_FT1	(1 << 6) /* FIFO threshold select bit 1. */

/* RCR: Receive Configuration Register */
#  define NE2000_RCR	0x0C	 /* Register address */
#  define NE2000_RCR_AB	(1 << 2) /* If AB=1, packets with broadcast destination
                                    address are accepted.
                                    If AB=0, packets with broadcast destination
                                    address are rejected.*/

#  define NE2000_RCR_AR (1) /*If AR=1, packets with length fewer than 64 bytes
                              are accepted.
                              If AR=0, packets with length fewer than 64 bytes
                              are rejected. */

/* RSR: Receive Status Register */
#  define NE2000_RSR	0x0C	 /* Register address */
#  define NE2000_RSR_PRX	(1 << 0) /* This bit indicates packet received with
                                            no errors.*/

/* IMR: Interrupt Mask Register */
#  define NE2000_IMR	0x0F	 /* Register address */

/* PSTART: Page Start Register */
#  define NE2000_PSTART	0x01	 /* Register address */

/* PSTOP: Page Stop Register */
#  define NE2000_PSTOP	0x02	 /* Register address */

/* BNRY: Boundary Register */
#  define NE2000_BNRY	0x03	 /* Register address */

/* TPSR: Transmit Page Start Register */
#  define NE2000_TPSR	0x04	 /* Register address */

/* TCR: Transmit Configuration Register */
#  define NE2000_TCR	0x0D	 /* Register address */

/* CURR: Current Page Register */
#  define NE2000_CURR	0x07	 /* Register address */

/* TBCR0,1: Transmit Byte Count Registers */
#  define NE2000_TBCR0	0x05	 /* Register address */
#  define NE2000_TBCR1	0x06	 /* Register address */


/* RSAR0,1: Remote Start Address Registers */
#  define NE2000_RSAR0	0x08	 /* Register address */
#  define NE2000_RSAR1	0x09	 /* Register address */

/* RBCR0,1: Remote Byte Count Registers */
#  define NE2000_RBCR0	0x0A	 /* Register address */
#  define NE2000_RBCR1	0x0B	 /* Register address */

/* PAR0-5: Physical Address Registers */
#  define NE2000_PAR0	0x01	 /* Register address */
#  define NE2000_PAR1	0x02	 /* Register address */
#  define NE2000_PAR2	0x03	 /* Register address */
#  define NE2000_PAR3	0x04	 /* Register address */
#  define NE2000_PAR4	0x05	 /* Register address */
#  define NE2000_PAR5	0x06	 /* Register address */


/* MAR0-7: Multicast Address Register */
#  define NE2000_MAR	0x08	 /* Register address */

/* Remote DMA Port */
#  define NE2000_DMA_PORT 0x10	 /* Register address */


#  define NE2000_MEMSZ	128	// device memory size in 256 bytes block
#  define NE2000_TXBUF	64	// tx buffer offset in 256 bytes block
#  define NE2000_TXBUFSZ 8	// tx buffer size in 256 bytes block
#  define NE2000_RXBUF	(NE2000_TXBUF + NE2000_TXBUFSZ)	// rx buf offset
// in 256 bytes block
#  define NE2000_RXBUFSZ (NE2000_MEMSZ - NE2000_TXBUFSZ) // rx buf size
// in 256 bytes block
//

#endif /*SYS_NEEDS_DRIVER_P3041*/
