/*
 *                               POK header
 *
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team
 *
 * Created by julien on Mon May 18 18:44:51 2009
 */

/**
 * @file
 * @author Laurent
 * @brief  RTL8029 driver
 * @date   PFE GISTR 2010
 */

#include "rtl8029.h"
#ifdef POK_NEEDS_RTL8029

#define DRV_NAME "ne2k-pci"

//#include <middleware/port.h>
#include <pci.h>
#include <ioports.h>
#include <net/network.h>

//TODO
# define outb_inverse(a,b) outb((b), (a))

// global since there is no way to get device data otherwise...
static s_ne2000_dev dev;

/*
 * We *always* assume page 0 to be selected.
 * Two exceptions: initialization and polling.
 * Therefore, each time we need to switch to page 1,
 * the card is switched to page 0 again when we're done...
 */
#define NE2000_SELECT_PAGE(dev, page)                                   \
  outb_inverse((inb((dev)->addr + NE2000_CR) &                          \
        ~(NE2000_CR_PS0 | NE2000_CR_PS1)) | ((page) << 6), (dev)->addr)

static int ne2000_write(
        const s_ne2000_dev *dev,
        const void         *buf,
        unsigned short     count,
        unsigned short     offset)
{
  const char* p = NULL;
  int ret = count;

  // Sets RD2 (abort/complete remote DMA)
  outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD0 | NE2000_CR_RD1)) |
       NE2000_CR_RD2, (dev)->addr);

  /* These two registers set the start address of remote DMA. */
  outb_inverse(offset, dev->addr + NE2000_RSAR0);
  outb_inverse(offset >> 8, dev->addr + NE2000_RSAR1);

  /* These two registers set the data byte counts of remote DMA. */
  outb_inverse(count, dev->addr + NE2000_RBCR0);
  outb_inverse(count >> 8, dev->addr + NE2000_RBCR1);

  // Sets RD1 (remote write)
  outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD0 | NE2000_CR_RD2)) |
       NE2000_CR_RD1, (dev)->addr);

  for (p = buf; count > 0; count--, p++) {
    outb_inverse(*p, dev->addr + NE2000_DMA_PORT);
  }

  return (ret);
}

static int ne2000_read_header(
        const s_ne2000_dev *dev,
        s_ne2000_header    *hdr,
        unsigned short      offset)
{
    int count = sizeof(s_ne2000_header);
    // Sets RD2 (abort/complete remote DMA)
    outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD0 | NE2000_CR_RD1)) |
            NE2000_CR_RD2, (dev)->addr);

    /* These two registers set the start address of remote DMA. */
    outb_inverse(offset, dev->addr + NE2000_RSAR0);
    outb_inverse(offset >> 8, dev->addr + NE2000_RSAR1);

    /* These two registers set the data byte counts of remote DMA. */
    outb_inverse(count, dev->addr + NE2000_RBCR0);
    outb_inverse(count >> 8, dev->addr + NE2000_RBCR1);

    // Sets RD0 (remote read)
    outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD1 | NE2000_CR_RD2)) |
            NE2000_CR_RD0, (dev)->addr);
    hdr->status = inb(dev->addr + NE2000_DMA_PORT);
    hdr->next   = inb(dev->addr + NE2000_DMA_PORT);
    hdr->size   = inb(dev->addr + NE2000_DMA_PORT);
    hdr->size  |= inb(dev->addr + NE2000_DMA_PORT) << 8;

    return count;
}
static int ne2000_read(
        const s_ne2000_dev *dev,
        void               *buf,
        unsigned short      count,
        unsigned short      offset)
{
  char* p = NULL;
  int ret = count;

  // Sets RD2 (abort/complete remote DMA)
  outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD0 | NE2000_CR_RD1)) |
       NE2000_CR_RD2, (dev)->addr);

  /* These two registers set the start address of remote DMA. */
  outb_inverse(offset, dev->addr + NE2000_RSAR0);
  outb_inverse(offset >> 8, dev->addr + NE2000_RSAR1);

  /* These two registers set the data byte counts of remote DMA. */
  outb_inverse(count, dev->addr + NE2000_RBCR0);
  outb_inverse(count >> 8, dev->addr + NE2000_RBCR1);

  // Sets RD0 (remote read)
  outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD1 | NE2000_CR_RD2)) |
       NE2000_CR_RD0, (dev)->addr);

  for (p = buf; count > 0; count--, p++) {
    *p = inb(dev->addr + NE2000_DMA_PORT);
  }

  return ret;
}


static void set_packet_received_callback(void (*f)(const char *, size_t))
{
    dev.packet_received_callback = f;
}


/**
 *  @brief Polls rtl8029 device.
 *
 *  Watches for events, typically for receiving queued packets.
 */
void rtl8029_polling ()
{
    unsigned char state; // ISR state
    s_ne2000_header  ne2000_hdr;     // ne2000 packet header
    unsigned short   offset;         // dma offset
    unsigned char    start, end;     // pointers for the ring buffer
    pok_packet_t     recv_packet;


    NE2000_SELECT_PAGE(&dev, 0);

    // do we have an interrupt flag set?
    if ((state = inb(dev.addr + NE2000_ISR)) == 0)
        goto POLL_END;

    if (!state & NE2000_ISR_PRX)
        goto POLL_END;

    if ((inb(dev.addr + NE2000_RSR) & NE2000_RSR_PRX) == 0)
        goto POLL_END;

    /* This register is used to prevent overwrite of the receive buffer ring.
       It is typically used as a pointer indicating the last receive buffer
       page the host has read.*/
    unsigned tmp = inb(dev.addr + NE2000_BNRY);
    start = tmp + 1;

    /* This register points to the page address of the first receive
       buffer page to be used for a packet reception. */
    NE2000_SELECT_PAGE(&dev, 1);
    end = inb(dev.addr + NE2000_CURR);
    NE2000_SELECT_PAGE(&dev, 0);

    if ((end % NE2000_MEMSZ) == (start % NE2000_MEMSZ))
        goto POLL_END;

    /* et on decapsule! */

    offset = start << 8;
    offset += ne2000_read_header(&dev, &ne2000_hdr, offset);

    ne2000_read(&dev, &recv_packet,
            ne2000_hdr.size - sizeof(s_ne2000_header), offset);

    // update the BNRY register... almost forgot that
    outb_inverse(ne2000_hdr.next > NE2000_MEMSZ ?
            NE2000_RXBUF - 1 : ne2000_hdr.next - 1, dev.addr + NE2000_BNRY);

    dev.packet_received_callback((const char *)&recv_packet, ne2000_hdr.size - sizeof(s_ne2000_header));

    outb_inverse(NE2000_ISR_PRX, dev.addr + NE2000_ISR); // Clear PRX flag

POLL_END:
    //Clear flags
    if (state & NE2000_ISR_PTX) outb_inverse(NE2000_ISR_PTX, dev.addr + NE2000_ISR);
    if (state & NE2000_ISR_RXE) outb_inverse(NE2000_ISR_RXE, dev.addr + NE2000_ISR);
    if (state & NE2000_ISR_TXE) outb_inverse(NE2000_ISR_TXE, dev.addr + NE2000_ISR);
    if (state & NE2000_ISR_OVW) outb_inverse(NE2000_ISR_OVW, dev.addr + NE2000_ISR);
    if (state & NE2000_ISR_CNT) outb_inverse(NE2000_ISR_CNT, dev.addr + NE2000_ISR);
    if (state & NE2000_ISR_RST) outb_inverse(NE2000_ISR_RST, dev.addr + NE2000_ISR);
}


/**
 *  @brief Initializes rtl8029 device.
 *
 *  Seeks and registers PCI interface, set configuration and fills the
 *  dev structure.
 */
static int rtl8029_init (struct pci_device *pci_dev)
{
    printf("rtl8029 probe called\n");
 // dev.pci.vendorid = 0x10ec;
 // dev.pci.deviceid = 0x8029;
 // dev.pci.io_range = 0x10;

  dev.addr = pci_dev->ioaddr;
  printf("bar0 addr = 0x%x\n", dev.addr);

  unsigned char	i = 0;
  unsigned char	buf[6 * 2]; // used for MAC address

  NE2000_SELECT_PAGE(&dev, 0);

  /* This bit is the STOP command. When it is set, no packets will be
     received or transmitted. POWER UP=1. */
  outb_inverse(NE2000_CR_STP, dev.addr + NE2000_CR);

  // Sets several options... Read the datasheet!
  outb_inverse(0x00, dev.addr + NE2000_TCR);
  outb_inverse(NE2000_RCR_AB, dev.addr + NE2000_RCR);
  outb_inverse(NE2000_DCR_LS | NE2000_DCR_FT1, dev.addr + NE2000_DCR);

  /* The Page Start register sets the start page address
     of the receive buffer ring. */
  outb_inverse(NE2000_RXBUF, dev.addr + NE2000_PSTART);
  /* The Page Stop register sets the stop page address
     of the receive buffer ring. */
  outb_inverse(NE2000_MEMSZ, dev.addr + NE2000_PSTOP);
  /* This register is used to prevent overwrite of the receive buffer ring.
     It is typically used as a pointer indicating the last receive buffer
     page the host has read. */
  outb_inverse(NE2000_RXBUF, dev.addr + NE2000_BNRY);

  /* These two registers set the data byte counts of remote DMA. */
  outb_inverse(0, dev.addr + NE2000_RBCR0);
  outb_inverse(0, dev.addr + NE2000_RBCR1);

  NE2000_SELECT_PAGE(&dev, 1);

  /* This register points to the page address of the first receive buffer
     page to be used for a packet reception. */
  outb_inverse(NE2000_RXBUF + 1, dev.addr + NE2000_CURR);

  // Init mac address
  /* Here's something I do not understand... Section 6.2.2 of the datasheet
     says bytes 00H-05H of the PROM corresponds to the Ethernet ID. But it
     looks like each byte of the MAC address is written twice...
     Therefore I read 2 * sizeof(mac) and select one of the two bytes
     corresponding to the MAC... Weird... Really... */
  ne2000_read(&dev, buf, 6 * 2, 0);
  for (i = 0; i < 6; i++) {
    dev.mac[i] = buf[i * 2];
    printf("%x:", dev.mac[i]);
  }
  printf("\n");

  /* These registers contain my Ethernet node address and are used to compare
    the destination address of incoming packets for acceptation or rejection.*/
  outb_inverse(dev.mac[0], dev.addr + NE2000_PAR0);
  outb_inverse(dev.mac[1], dev.addr + NE2000_PAR1);
  outb_inverse(dev.mac[2], dev.addr + NE2000_PAR2);
  outb_inverse(dev.mac[3], dev.addr + NE2000_PAR3);
  outb_inverse(dev.mac[4], dev.addr + NE2000_PAR4);
  outb_inverse(dev.mac[5], dev.addr + NE2000_PAR5);

  NE2000_SELECT_PAGE(&dev, 0);

  // Start command
  outb_inverse(NE2000_CR_STA, dev.addr + NE2000_CR);

  // Reactivating interrupts
  /* ISR register must be cleared after power up. */
  outb_inverse(0xFF, dev.addr + NE2000_ISR);
  /* All bits correspond to the bits in the ISR register. POWER UP=all 0s.
     Setting individual bits will enable the corresponding interrupts. */
  /* Since POK use polling, ALL interrupts are disabled */
  outb_inverse(0x00, dev.addr + NE2000_IMR);

  for (i = 0; i < 20; i++) /* TODO: random constant */
  {
    dev.recv_buf[i].len = 0;
    dev.recv_buf[i].off = 0;
  }

  return TRUE;
}

static pok_bool_t send_frame_gather(const pok_network_sg_list_t *sg_list,
                                    size_t sg_list_len,
                                    pok_network_buffer_callback_t callback,
                                    void *callback_arg)
{
    if (sg_list_len != 1) {
        printf("In NE2k driver real gathering is not supported yet");
        return FALSE;
    }
    printf("send_frame_gather ... \n");

    char *buf = sg_list[0].buffer;
    size_t buf_len = sg_list[0].size;

    unsigned char state; // ISR state

    ne2000_write(&dev, buf, buf_len, NE2000_TXBUF * 256);

    do {
        state = inb(dev.addr + NE2000_ISR);
    } while ((state & NE2000_ISR_RDC) != NE2000_ISR_RDC);

    /* This register sets the start page address of
       the packet to the transmitted. */
    outb_inverse(NE2000_TXBUF, dev.addr + NE2000_TPSR); //?

    /* These two registers set the byte counts of
       the packet to be transmitted. */
    outb_inverse(buf_len, dev.addr + NE2000_TBCR0);
    outb_inverse(buf_len >> 8, dev.addr + NE2000_TBCR1);

    /* This bit must be set to transmit a packet. */
    outb_inverse(inb(dev.addr + NE2000_CR) | NE2000_CR_TXP,
            dev.addr + NE2000_CR);

    outb_inverse(NE2000_ISR_RDC, dev.addr + NE2000_ISR); // Clear RDC bit

    //XXX: Currently NE2K driver ignores reclaim_*
    callback(callback_arg);

    return TRUE;
}

static pok_bool_t send_frame(char *buffer,
                             size_t size,
                             pok_network_buffer_callback_t callback,
                             void *callback_arg)
{
    printf("send_frame\n");
    pok_network_sg_list_t sg_list[1] = {{.buffer=buffer, .size=size}};
    return send_frame_gather(sg_list, 1, callback, callback_arg);
}

static void reclaim_send_buffers(void)
{
}

static void reclaim_receive_buffers(void)
{
    rtl8029_polling();
}

static void flush_send(void)
{
    printf("flush_send\n");
}

static const pok_network_driver_ops_t driver_ops = {
    .send_frame = send_frame,
    .send_frame_gather = send_frame_gather,
    .set_packet_received_callback = set_packet_received_callback,
    .reclaim_send_buffers = reclaim_send_buffers,
    .reclaim_receive_buffers = reclaim_receive_buffers,
    .flush_send = flush_send,
};

pok_network_driver_device_t pok_network_ne2000_device = {
    .ops = &driver_ops,
    .mac = dev.mac
};


static const struct pci_device_id ne2k_pci_tbl[] = {
    { 0x10ec, 0x8029},
    //Here may be others cards from ne2k family
};

struct pci_driver ne2k_driver = {
    .name     = DRV_NAME,
    .probe    = rtl8029_init,
    .id_table = ne2k_pci_tbl
};

#endif
