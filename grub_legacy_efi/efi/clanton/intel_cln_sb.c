/*
 * Copyright(c) 2013 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Contact Information:
 * Intel Corporation
 */

#include <netboot/linux-asm-io.h>
#include <stage2/shared.h>
#include "intel_cln_sb.h"

#define INTEL_CLN_SB_CMD_ADDR	(0x000000D0)
#define INTEL_CLN_SB_DATA_ADDR	(0x000000D4)

#define INTEL_CLN_SB_MCR_SHIFT	(24)
#define INTEL_CLN_SB_PORT_SHIFT	(16)
#define INTEL_CLN_SB_REG_SHIFT	(8)
#define INTEL_CLN_SB_BYTEEN	(0xF0)	/* enable all 32 bits */

/* PCI config space reg definitions */
#define PCI_VENDOR_ID		(0x00)
#define PCI_DEVICE_ID		(0x02)
#define PCI_CLASS_DEVICE	(0x0A)

struct sb_pci_dev {
  unsigned int bus;
  unsigned int dev_fn;
};

static struct sb_pci_dev sb_pcidev;

/*
 * Functions for accessing PCI configuration space with type 1 accesses
 */
#define CONFIG_CMD(bus, dev_fn, where) \
  (0x80000000 | (bus << 16) | (dev_fn << 8) | (where & ~3))

static void
pci_read_config_byte(struct sb_pci_dev *sb_pcidev,
                     unsigned int where, unsigned char *value)
{
  outl(CONFIG_CMD(sb_pcidev->bus,sb_pcidev->dev_fn, where), 0xCF8);
  *value = inb(0xCFC + (where&3));
}

static void
pci_read_config_word (struct sb_pci_dev *sb_pcidev,
                      unsigned int where, unsigned short *value)
{
  outl(CONFIG_CMD(sb_pcidev->bus,sb_pcidev->dev_fn,where), 0xCF8);
  *value = inw(0xCFC + (where&2));
}

static void
pci_read_config_dword (struct sb_pci_dev *sb_pcidev,
                       unsigned int where, unsigned int *value)
{
  outl(CONFIG_CMD(sb_pcidev->bus,sb_pcidev->dev_fn, where), 0xCF8);
  *value = inl(0xCFC);
}

static void
pci_write_config_byte (struct sb_pci_dev *sb_pcidev,
                       unsigned int where, unsigned char value)
{
  outl(CONFIG_CMD(sb_pcidev->bus,sb_pcidev->dev_fn, where), 0xCF8);
  outb(value, 0xCFC + (where&3));
}

static void
pci_write_config_word (struct sb_pci_dev *sb_pcidev,
                       unsigned int where, unsigned short value)
{
  outl(CONFIG_CMD(sb_pcidev->bus,sb_pcidev->dev_fn, where), 0xCF8);
  outw(value, 0xCFC + (where&2));
}

static void
pci_write_config_dword (struct sb_pci_dev *sb_pcidev, unsigned int where,
                        unsigned int value)
{
  outl(CONFIG_CMD(sb_pcidev->bus,sb_pcidev->dev_fn, where), 0xCF8);
  outl(value, 0xCFC);
}

/**
 * intel_cln_sb_read_reg
 *
 * @param cln_sb_id: Sideband identifier
 * @param command: Command to send to destination identifier
 * @param reg: Target register w/r to cln_sb_id
 * @return nothing
 *
 * Utility function to allow thread-safe read of side-band
 * command - can be different read op-code types - which is why we don't
 * hard-code this value directly into msg
 */
void
intel_cln_sb_read_reg(cln_sb_id id, grub_uint8_t cmd, grub_uint8_t reg,
                      grub_uint32_t *data)
{
  grub_uint32_t msg = (cmd << INTEL_CLN_SB_MCR_SHIFT) |
      ((id << INTEL_CLN_SB_PORT_SHIFT) & 0xFF0000)|
      ((reg << INTEL_CLN_SB_REG_SHIFT) & 0xFF00)|
      INTEL_CLN_SB_BYTEEN;

  if(data == NULL)
    return;

  pci_write_config_dword(&sb_pcidev, INTEL_CLN_SB_CMD_ADDR, msg);
  pci_read_config_dword(&sb_pcidev, INTEL_CLN_SB_DATA_ADDR, data);

}

/**
 * intel_cln_sb_write_reg
 *
 * @param cln_sb_id: Sideband identifier
 * @param command: Command to send to destination identifier
 * @param reg: Target register w/r to cln_sb_id
 * @return nothing
 *
 * Utility function to allow thread-safe write of side-band
 */
void
intel_cln_sb_write_reg(cln_sb_id id, grub_uint8_t cmd, grub_uint8_t reg,
                       grub_uint32_t data)
{
  grub_uint32_t msg = (cmd << INTEL_CLN_SB_MCR_SHIFT) |
      ((id << INTEL_CLN_SB_PORT_SHIFT) & 0xFF0000)|
      ((reg << INTEL_CLN_SB_REG_SHIFT) & 0xFF00)|
      INTEL_CLN_SB_BYTEEN;

  pci_write_config_dword(&sb_pcidev, INTEL_CLN_SB_DATA_ADDR, data);
  pci_write_config_dword(&sb_pcidev, INTEL_CLN_SB_CMD_ADDR, msg);
}


/* Clanton hardware */
#define PCI_VENDOR_ID_INTEL		(0x8086)
#define PCI_DEVICE_ID_CLANTON_SB	(0x0958)

/**
 * sb_probe
 *
 * @param dev: the PCI device matching
 * @param id: entry in the match table
 * @return 0
 *
 * Callback from PCI layer when dev/vendor ids match.
 * Sets up necessary resources
 */
int
intel_cln_sb_probe(void)
{
  int found = 0;
  grub_uint16_t class;
  grub_uint16_t device, vendor;
  grub_uint8_t type;

  sb_pcidev.bus = 0;
  for (sb_pcidev.dev_fn = 0; sb_pcidev.dev_fn < 0xFF; sb_pcidev.dev_fn++)
    {
      /* Only probe function 0 on single fn devices */
      pci_read_config_word(&sb_pcidev, PCI_CLASS_DEVICE, &class);

      if (class == 0xffff)
        continue;

    pci_read_config_word(&sb_pcidev, PCI_VENDOR_ID, &vendor);
    pci_read_config_word(&sb_pcidev, PCI_DEVICE_ID, &device);

    /* Do early PCI UART init */
    if(vendor == PCI_VENDOR_ID_INTEL)
      {
        if (device == PCI_DEVICE_ID_CLANTON_SB)
          {
            /* Found */
            found = 1;
            grub_printf("%s b/d/f 0x%04x/0x%04x scan vendor 0x%04x device 0x%04x\n",
                        __func__, sb_pcidev.bus, sb_pcidev.dev_fn, vendor, device);
            break;
          }
      }
    }

  if(found == 0)
    {
      grub_printf("Unable to init side-band!\n");
      return -1;
    }

  grub_printf("Intel Clanton side-band driver registered\n");


  return 0;
}
