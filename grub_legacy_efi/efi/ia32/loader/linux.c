/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006  Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <grub/misc.h>
#include <grub/types.h>
#include <grub/cpu/linux.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/misc.h>
#include <clanton/asset.h>
#include <clanton/clanton.h>
#include <clanton/early_uart.h>
#include <clanton/flash.h>
#include <clanton/iarom.h>
#include <clanton/imr.h>
#include <clanton/mfh.h>
#include <clanton/perf_metrics.h>
#include <clanton/sbh.h>
#include <clanton/test_module.h>

#include "switch.h"
#include <shared.h>

#include "graphics.h"
#include "i386-elf.h"

#define NEXT_MEMORY_DESCRIPTOR(desc, size)      \
  ((grub_efi_memory_descriptor_t *) ((char *) (desc) + (size)))

#define PTR_HI(x) ((grub_uint32_t) ((unsigned long long)((unsigned long)(x)) >> 32))

#ifndef SECTOR_SIZE
#define SECTOR_SIZE 0x200
#endif /* defined(SECTOR_SIZE) */
#ifndef SECTOR_BITS
#define SECTOR_BITS 9
#endif /* defined(SECTOR_BITS) */

#define PAGE_SIZE 0x1000

#define GRUB_MULTIBOOT_ADDR		0x100000

static unsigned long linux_mem_size;
static int loaded;
static void *real_mode_mem;
static void *prot_mode_mem;
static void *initrd_mem;
static grub_efi_uintn_t real_mode_pages;
static grub_efi_uintn_t prot_mode_pages;
static grub_efi_uintn_t initrd_pages;
static grub_efi_guid_t graphics_output_guid = GRUB_EFI_GRAPHICS_OUTPUT_GUID;
static grub_uint32_t mmio_base;

/* If modules are in SPI/Flash, follow MFH path.
   Else follow file system path.  */
unsigned short int grub_cln_linux_spi = 0;
unsigned short int grub_cln_initrd_spi = 0;

/* The Clanton Secure Boot Header.  */
static struct grub_cln_sbh cln_sbh;
static grub_uint32_t sbh_len = 0;

#define grub_file_size()    filemax

static inline grub_size_t
page_align (grub_size_t size)
{
  return (size + (1 << 12) - 1) & (~((1 << 12) - 1));
}

static void
free_pages (void)
{
  if (real_mode_mem)
    {
      grub_efi_free_pages ((grub_addr_t) real_mode_mem, real_mode_pages);
      real_mode_mem = 0;
    }

  if (prot_mode_mem)
    {
      grub_efi_free_pages ((grub_addr_t) prot_mode_mem, prot_mode_pages);
      prot_mode_mem = 0;
    }

  if (initrd_mem)
    {
      grub_efi_free_pages ((grub_addr_t) initrd_mem, initrd_pages);
      initrd_mem = 0;
    }

  if (mmap_buf)
    {
      grub_efi_free_pages ((grub_addr_t) mmap_buf, mmap_pages);
      mmap_buf = 0;
    }
}

/* Allocate pages for the real mode code and the protected mode code
   for linux as well as a memory map buffer.  */
static int
allocate_pages (grub_size_t real_size, grub_size_t prot_size)
{
  grub_efi_uintn_t desc_size;
  grub_efi_memory_descriptor_t *mmap_end;
  grub_efi_memory_descriptor_t *desc;
  grub_efi_physical_address_t addr;

  /* Make sure that each size is aligned to a page boundary.  */
  real_size = page_align (real_size + SECTOR_SIZE);
  prot_size = page_align (prot_size);

  grub_dprintf ("linux", "real_size = %x, prot_size = %x, mmap_size = %x\n",
		(unsigned int) real_size, (unsigned int) prot_size,
		(unsigned int) mmap_size);

  /* Calculate the number of pages; Combine the real mode code with
     the memory map buffer for simplicity.  */
  real_mode_pages = (real_size >> 12);
  prot_mode_pages = (prot_size >> 12);

  /* Initialize the memory pointers with NULL for convenience.  */
  real_mode_mem = 0;
  prot_mode_mem = 0;
  initrd_mem = 0;

  if (grub_efi_get_memory_map (0, &desc_size, 0) <= 0)
    {
      grub_printf ("cannot get memory map");
      errnum = ERR_BOOT_FAILURE;
      return 0;
    }

  addr = 0;
  mmap_end = NEXT_MEMORY_DESCRIPTOR (mmap_buf, mmap_size);
  /* First, find free pages for the real mode code
     and the memory map buffer.  */
  for (desc = mmap_buf;
       desc < mmap_end;
       desc = NEXT_MEMORY_DESCRIPTOR (desc, desc_size))
    {
      if (desc->type == GRUB_EFI_CONVENTIONAL_MEMORY
	  && desc->num_pages >= real_mode_pages)
	{
	  grub_efi_physical_address_t physical_end;

          physical_end = desc->physical_start + (desc->num_pages << 12);

          grub_dprintf ("linux", "physical_start = %x, physical_end = %x\n",
                        (unsigned) desc->physical_start,
                        (unsigned) physical_end);
          addr = physical_end - real_size;
          if (addr < 0x10000)
            continue;

          grub_dprintf ("linux", "trying to allocate %u pages at %x\n",
                        (unsigned) real_mode_pages, (unsigned) addr);
          real_mode_mem = grub_efi_allocate_pages (addr, real_mode_pages);
          if (! real_mode_mem)
            {
              grub_printf ("cannot allocate pages");
              errnum = ERR_WONT_FIT;
              goto fail;
            }

          desc->num_pages -= real_mode_pages;
          break;
	}
    }

  if (! real_mode_mem)
    {
      grub_printf ("cannot allocate real mode pages");
      errnum = ERR_WONT_FIT;
      goto fail;
    }

  /* Next, find free pages for the protected mode code.  */
  /* XXX what happens if anything is using this address?  */
  prot_mode_mem = grub_efi_allocate_pages
                  (GRUB_LINUX_BZIMAGE_ADDR, prot_mode_pages);
  if (! prot_mode_mem)
    {
      grub_printf ("Cannot allocate pages for VMLINUZ");
      errnum = ERR_WONT_FIT;
      goto fail;
    }

  return 1;

 fail:
  free_pages ();
  return 0;
}

/* do some funky stuff, then boot linux */
void
linux_boot (void)
{
  grub_printf ("zImage is not supported under EFI.\n");
  for (;;);
}

#ifndef __x86_64__
struct {
    unsigned short limit;
    unsigned int base;
} __attribute__ ((packed)) 
  gdt_addr = { 0x800, 0x94000 },
  idt_addr = { 0, 0 };

unsigned short init_gdt[] = {
  /* gdt[0]: dummy */
  0, 0, 0, 0,

  /* gdt[1]: unused */
  0, 0, 0, 0,

  /* gdt[2]: code */
  0xFFFF,         /* 4Gb - (0x100000*0x1000 = 4Gb) */
  0x0000,         /* base address=0 */
  0x9A00,         /* code read/exec */
  0x00CF,         /* granularity=4096, 386 (+5th nibble of limit) */

  /* gdt[3]: data */
  0xFFFF,         /* 4Gb - (0x100000*0x1000 = 4Gb) */
  0x0000,         /* base address=0 */
  0x9200,         /* data read/write */
  0x00CF,         /* granularity=4096, 386 (+5th nibble of limit) */
};
#endif


static void
test_print(grub_efi_system_table_t *sys_table)
{
	sys_table->con_out->output_string(sys_table->con_out, L"Memos\r\n");
}

void 
multi_boot (int start, int mb_info)
{
  /* memos is modified so that it doesn't need any information
   * from mb_info. So mb_info is not used now */
  grub_efi_uintn_t map_key;
  grub_efi_uintn_t desc_size;
  grub_efi_uint32_t desc_version;

  if (grub_efi_get_memory_map (&map_key, &desc_size, &desc_version) <= 0)
  {
    grub_printf ("Cannot get memory map\n");
    errnum = ERR_BOOT_FAILURE;
    return;
  }

  grub_printf ("Entry point is %p\n", start);

  grub_printf("ExitBootServices...\n");
  if (! grub_efi_exit_boot_services (map_key))
  {
    grub_printf ("cannot exit boot services");
    errnum = ERR_BOOT_FAILURE;
    return;
  }

  asm volatile ("cli"); 

  /* set gdt, actually we don't need this... */
  grub_memset((void *)gdt_addr.base, gdt_addr.limit, 0);
  grub_memcpy((void *)gdt_addr.base, init_gdt, sizeof (init_gdt));
  asm volatile ("lgdt %0"::"m" (gdt_addr) );

  asm volatile ("movl %0, %%ebx\n\t" /* Pass multiboot info struct pointer */
                "movl %1, %%ecx\n\t"
                "jmp *%%ecx\n\t"     /* Jump to kernel entry point */
                :
                :"m" (mb_info), "m" (start));
}

void
big_linux_boot (void)
{
  struct linux_kernel_params *params;
  struct grub_linux_kernel_header *lh;
  grub_efi_uintn_t map_key;
  grub_efi_uintn_t desc_size;
  grub_efi_uint32_t desc_version;
  int e820_nr_map;
  int i;

  params = real_mode_mem;

  graphics_set_kernel_params (params);

  if (grub_efi_get_memory_map (&map_key, &desc_size, &desc_version) <= 0)
    {
      grub_printf ("cannot get memory map");
      errnum = ERR_BOOT_FAILURE;
      return;
    }

  /* Pass e820 memmap. */
  e820_map_from_efi_map ((struct e820_entry *) params->e820_map, &e820_nr_map,
			 mmap_buf, desc_size, mmap_size);
  params->e820_nr_map = e820_nr_map;

  grub_dprintf(__func__,"got to ExitBootServices...\n");

  if (! grub_efi_exit_boot_services (map_key))
    {
      grub_printf ("cannot exit boot services");
      errnum = ERR_BOOT_FAILURE;
      return;
    }
    
  /* Note that no boot services are available from here.  */

  lh = &params->hdr;
  /* Pass EFI parameters.  */
  if (grub_le_to_cpu16 (lh->version) >= 0x0206) {
    params->version_0206.efi_mem_desc_size = desc_size;
    params->version_0206.efi_mem_desc_version = desc_version;
    params->version_0206.efi_mmap = (grub_uint32_t) (unsigned long) mmap_buf;
    params->version_0206.efi_mmap_size = mmap_size;
  } else if (grub_le_to_cpu16 (lh->version) >= 0x0204) {
    params->version_0204.efi_mem_desc_size = desc_size;
    params->version_0204.efi_mem_desc_version = desc_version;
    params->version_0204.efi_mmap = (grub_uint32_t) (unsigned long) mmap_buf;
    params->version_0204.efi_mmap_size = mmap_size;
  } else /* dunno */ {
    params->dunno.efi_mem_desc_size = desc_size;
    params->dunno.efi_mem_desc_version = desc_version;
    params->dunno.efi_mmap = (grub_uint32_t) (unsigned long) mmap_buf;
    params->dunno.efi_mmap_size = mmap_size;
    params->dunno.efi_mmap_hi = PTR_HI(mmap_buf);
  }

#ifdef __x86_64__
  /* copy our real mode transition code to 0x700 */
  memcpy ((void *) 0x700, switch_image, switch_size);
  asm volatile ( "mov $0x700, %%rdi" : :);

  /* Pass parameters.  */
  asm volatile ("mov %0, %%rsi" : : "m" (real_mode_mem));
  asm volatile ("movl %0, %%ebx" : : "m" (params->hdr.code32_start));

  /* Enter Linux, switch from 64-bit long mode
   * to 32-bit protect mode, this code end address
   * must not exceed 0x1000, because linux kernel bootstrap
   * code will flush this area
   */
  asm volatile ( "jmp *%%rdi" : :);
#else

  asm volatile ( "cli" : : );

  grub_memset((void *)gdt_addr.base, gdt_addr.limit, 0);
  grub_memcpy((void *)gdt_addr.base, init_gdt, sizeof (init_gdt));

  /* This is the very last stage we can timestamp.
     Do it and append performance metrics to the linux command line.  */
  grub_cln_event_append ("jmp_code32");
  grub_strncat ((char *) real_mode_mem + 0x1000,
                 grub_cln_event_get_metrics (),
                 GRUB_LINUX_CL_END_OFFSET - GRUB_LINUX_CL_OFFSET + 1);

  if (0) {
    /* copy our real mode transition code to 0x7C00 */
    memcpy ((void *) 0x7C00, switch_image, switch_size);
    asm volatile ( "mov $0x7C00, %%ebx" : : );
    asm volatile ( "jmp *%%ebx" : : );
  } else {

    /* load descriptor table pointers */
    // asm volatile ( "lidt %0" : : "m" (idt_addr) );
    asm volatile ( "lgdt %0" : : "m" (gdt_addr) );

    /*
     * ebx := 0  (%%TBD - do not know why, yet)
     * ecx := kernel entry point
     * esi := address of boot sector and setup data
     */

    asm volatile ( "movl %0, %%esi" : : "m" (real_mode_mem) );
    asm volatile ( "movl %0, %%ecx" : : "m" (params->hdr.code32_start) );
    asm volatile ( "xorl %%ebx, %%ebx" : : );

    /*
     * Jump to kernel entry point.
     */

    asm volatile ( "jmp *%%ecx" : : );
  }
#endif

  /* Never reach here.  */
  for (;;);
}

/*
   Lexer to detect early Quark's UART MMIO token.
   If token is found, expand it with the actual MMIO address.
 */
static void
cln_detect_early_uart (char **cmdline)
{
  char *p = NULL;
  char *start = *cmdline;
  grub_uint32_t mmio;
  grub_uint32_t offs = 0;

  p = strstr(*cmdline, QUARK_UART_MMIO_TOKEN);
  *cmdline = start;

  if (NULL == p)
    {
      /* Nothing to do.  */
      return;
    }

  mmio = (grub_uint32_t)cln_early_uart_init();
  if (!mmio)
    {
      grub_printf("%s: couldn't find device. Skipping..\n", __func__);
      return;
    }

  if (grub_cln_debug)
    grub_printf("%s: MMIO addr @ 0x%x\n", __func__, mmio);

  offs = grub_sprintf(p, "0x%x", mmio);
  grub_sprintf(p + offs, "%s", p + sizeof(QUARK_UART_MMIO_TOKEN) - 1);
}

int
grub_load_multiboot (char *kernel, char *arg)
{
  unsigned char buffer[MULTIBOOT_SEARCH];
  int len, i;
  extern int cur_addr;
  unsigned long flags = 0;
  grub_size_t prot_size = 0;
  entry_func real_entry_addr = 0;
  Elf32_Ehdr *elf;

  if (!grub_open(kernel)) {
    grub_printf("Failed to open kernel image\n");
    goto fail1;
  }

  /* we only need the image header for now */
  if (!(len = grub_read (buffer, MULTIBOOT_SEARCH)) || len < 32) {
    errnum = ERR_EXEC_FORMAT;
    grub_printf ("Failed to read image header\n");
    goto fail;
  }

  /* search multiboot flags */
  for (i = 0; i < len; i++) {
    if (MULTIBOOT_FOUND ((int) (buffer + i), len - i)) {
      flags = ((struct multiboot_header *) (buffer + i))->flags;
      if (flags & MULTIBOOT_UNSUPPORTED) {
        errnum = ERR_BOOT_FEATURES;
        goto fail;
      }
      break;
    }
  }

  elf = (Elf32_Ehdr *) buffer;

  if (len > sizeof (Elf32_Ehdr) && BOOTABLE_I386_ELF ((*elf))) {
    /* record entry_addr so that we can jump to it later*/
    entry_addr = (entry_func) (elf->e_entry);

    if (entry_addr < (entry_func) 0x100000) {
      errnum = ERR_BELOW_1MB;
      goto fail;
    }

    if (elf->e_phoff == 0 || elf->e_phnum == 0 ||
        ((elf->e_phoff + (elf->e_phentsize * elf->e_phnum)) >= len)) {
      errnum = ERR_EXEC_FORMAT;
      goto fail;
    }
  } else {
    grub_printf ("Image corrupted or not bootable\n");
    errnum = ERR_EXEC_FORMAT;
    goto fail;
  } 

  /* allocate memory for elf loading */

  /**
   * grub_cln_asset_size is useful for SPI support
   * prot_size = grub_cln_asset_size (GRUB_CLN_ASSET_KERNEL);
   * grub_size_t real_size = 0;
   * if (! allocate_pages (real_size , prot_size))
   *   goto fail;
   */

  /* grub_file_size () returns filemax set by grub_open */
  prot_size = grub_file_size ();
  prot_mode_pages = (prot_size >> 12);
  real_mode_pages = 0;
  real_mode_mem = prot_mode_mem = 0;

  /**
   * Now, we try to allocate memory for the multiboot kernel. For simplicity, we
   * will assume it starts at GRUB_MULTIBOOT_ADDR (1MB by default).
   */
  prot_mode_mem = grub_efi_allocate_pages (GRUB_MULTIBOOT_ADDR, prot_mode_pages);
  if (! prot_mode_mem) {
      grub_printf ("Cannot allocate pages for multiboot kernel\n");
      errnum = ERR_WONT_FIT;
      free_pages ();
      goto fail;
  }

  grub_printf ("Memory allocated: At %p of %u bytes\n", prot_mode_mem, prot_size);

  /* Can we skip this? Donno why we need imr...	*/
#if 0
  errnum = intel_cln_imr_setup(IMR_RANGE_BZIMAGE, (grub_addr_t)prot_mode_mem, prot_size);
  if (errnum != ERR_NONE) {
    grub_printf("IMR boot params setup failed!\n");
    goto fail;
  }
#endif

  /* init bss */
  grub_memset (prot_mode_mem, 0, prot_size);

  /* start loading elf */
  Elf32_Phdr *phdr;
  unsigned filesiz = 0, memsiz = 0;

  /* Reset cur_addr */
  cur_addr = 0;

  phdr = (Elf32_Phdr *) (elf->e_phoff + ((int) buffer));
  //grub_printf ("e_phnum is %u\n", elf->e_phnum);

  for (i = 0; i < elf->e_phnum; i++) {
    if (phdr[i].p_type == PT_LOAD) {
      /* locate the section in file */
      grub_seek (phdr[i].p_offset);
      filesiz = phdr[i].p_filesz;
      memsiz = phdr[i].p_memsz;

#if 0
      /* record the load address of the first section
       * so that we can calculate following sections'
       * real physical address */
      grub_printf("first_loaded_sect_phy_addr is %p\n", first_loaded_sect_phy_addr);
      if (first_loaded_sect_phy_addr == 0xFFFFFFFF)
        first_loaded_sect_phy_addr = phdr[i].p_paddr;
      real_phy_addr = load_phy_to_real_phy(phdr[i].p_paddr, first_loaded_sect_phy_addr, prot_size);
      if (real_phy_addr == 0xFFFFFFFF || real_phy_addr < 0x100000) {
        /* real_phy_addr is out of range */
        errnum = ERR_BELOW_1MB;
        goto fail;
      }
      grub_printf("loading No.%u section, real_phy_addr is %p,\
          load_phy is %p, memsiz is %x, filesiz is %x\n",
          loaded, real_phy_addr, phdr[i].p_paddr, memsiz, filesiz);

      /* No virtual memory here, we have to find 
       * the physical addresss of entry on our own */
      if ((unsigned) entry_addr >= phdr[i].p_vaddr
          && (unsigned) entry_addr < phdr[i].p_vaddr + memsiz) {
        unsigned offset_in_sect = (unsigned) entry_addr - phdr[i].p_vaddr;
        unsigned entry_load_phy = phdr[i].p_paddr + offset_in_sect;
        real_entry_addr = (entry_func) load_phy_to_real_phy(entry_load_phy, 
            first_loaded_sect_phy_addr, prot_size);
        grub_printf("entry_addr is %p, real_entry_addr is %p\n",
            entry_addr, real_entry_addr);
      }

      if (filesiz > memsiz)
        filesiz = memsiz;


      /* finally, we can load the section now */
      if (grub_read((char *)real_phy_addr, filesiz) == filesiz) {
        if (memsiz > filesiz)
          memset ((char *) real_phy_addr, 0, memsiz - filesiz);
      }
#endif

      grub_printf("load_phy is %p, virt_addr is %p, memsiz is %x, filesiz is %x\n",
          phdr[i].p_paddr, phdr[i].p_vaddr, memsiz, filesiz);
      /* finally, we can load the section now */
      if (grub_read ((char *)phdr[i].p_paddr, filesiz) == filesiz) {
        if (memsiz > filesiz) {
          grub_memset ((char *) phdr[i].p_paddr + filesiz, 0, memsiz - filesiz);
        }

        /* Update cur_addr */
        if ((phdr[i].p_paddr + memsiz) > cur_addr) {
          cur_addr = phdr[i].p_paddr + memsiz;
        }
      } else {
        grub_printf ("Reading ELF failed!\n");
        break;
      }
    }
  }

  /* find the mmaped uart base address */
  mmio_base = (grub_uint32_t)cln_early_uart_init();
  grub_printf ("mmapped uart base address is 0x%x\n", mmio_base);

  /* fill the multiboot info structure */
  mbi.cmdline = (int) arg;
  mbi.mods_count = 0;
  mbi.mods_addr = 0;
  mbi.boot_device = (current_drive << 24) | current_partition;
  mbi.flags &= ~(MB_INFO_MODS | MB_INFO_AOUT_SYMS | MB_INFO_ELF_SHDR);
  mbi.syms.a.tabsize = 0;
  mbi.syms.a.strsize = 0;
  mbi.syms.a.addr = 0;
  mbi.syms.a.pad = 0;

  /* Let's skip the elf symbols for now. */
  mbi.syms.e.num = elf->e_shnum;
  mbi.syms.e.size = elf->e_shentsize;
  mbi.syms.e.shndx = elf->e_shstrndx;

fail:
  grub_close ();

fail1:
  if (errnum != ERR_NONE) {
    loaded = 0;
  }
  return errnum ? KERNEL_TYPE_NONE : KERNEL_TYPE_MULTIBOOT;
}

int
grub_load_linux (char *kernel, char *arg)
{
  struct grub_linux_kernel_header *lh;
  struct linux_kernel_params *params;
  static struct linux_kernel_params params_buf;
  grub_uint8_t setup_sects = 0;
  grub_size_t real_size = 0, prot_size = 0, img_size = 0;
  grub_uint32_t code32_start_offs = 0x0, prot_mode_offs = 0x0;;
  grub_ssize_t len = 0;

	grub_printf ("linux.c::grub_load_kernel start\n");
	if (kernel != NULL)
		grub_printf ("kernel path is %s\n", kernel);

  sbh_len = 0;

  __cln_test_setup(arg);

  /* In SPI/Flash mode, file system path to kernel is not required.  */
  if (! grub_cln_linux_spi && kernel == NULL)
    {
      errnum = ERR_BAD_FILENAME;
      grub_printf ("no kernel specified");
      goto fail1;
    }

  if (grub_cln_secure)
    {
      grub_printf ("grub_cln_secure is true\n");

       if (! grub_cln_fetch_sbh (GRUB_CLN_ASSET_KERNEL_CSBH, kernel, &cln_sbh))
         {
           if(ERR_FILE_NOT_FOUND == errnum
               && ! grub_cln_linux_spi)
             errnum = ERR_SGN_FILE_NOT_FOUND;
           goto fail1;
         }
       sbh_len = cln_sbh.security_header.header_len;
    }

	grub_printf ("grub_cln_asset_opening...\n");
  if (! grub_cln_asset_open (GRUB_CLN_ASSET_KERNEL, kernel))
    goto fail1;

	grub_printf ("grub_cln_asset_reading...\n");
	/* why this is called params_buf */
	/* I think not only parameters are loaded into this buffer */
	/* added by Tom */
  if (grub_cln_asset_read (GRUB_CLN_ASSET_KERNEL, (grub_uint8_t *) &params_buf, sizeof (params_buf))
      != sizeof (params_buf))
    {
      errnum = ERR_EXEC_FORMAT;
      grub_close();
      grub_printf ("cannot read the linux header");
      goto fail;
    }

	grub_printf ("Done with grub_cln_asset_read\n");
  lh = &params_buf.hdr;

  if (lh->boot_flag != grub_cpu_to_le16 (0xaa55))
    {
      errnum = ERR_EXEC_FORMAT;
      grub_close();
      grub_printf ("invalid magic number: %x", lh->boot_flag);
      goto fail;
    }

  /* EFI support is quite new, so reject old versions.  */
  if (lh->header != grub_cpu_to_le32 (GRUB_LINUX_MAGIC_SIGNATURE)
      || grub_le_to_cpu16 (lh->version) < 0x0203)
    {
      grub_close();
      errnum = ERR_EXEC_FORMAT;
      grub_printf ("too old version");
      goto fail;
    }

  /* I'm not sure how to support zImage on EFI.  */
  if (! (lh->loadflags & GRUB_LINUX_FLAG_BIG_KERNEL))
    {
      errnum = ERR_EXEC_FORMAT;
      grub_printf ("zImage is not supported");
      goto fail;
    }

  setup_sects = lh->setup_sects;

  img_size = grub_cln_asset_size (GRUB_CLN_ASSET_KERNEL);

	/* check this! The size of real mode part of kernel */
	/* added by Tom */
  real_size = 0x1000 + grub_strlen(arg);

  /* Grub allocates distinct memory regions for the so-called "real mode" and
     "protected mode" portions of bzImage.  The linux entry point is located
     at the base address of the protected mode section
     (GRUB_LINUX_BZIMAGE_ADDR).
     In order to perform signature verification, Clanton secure boot requires
     the whole bzImage to be allocated contiguously, with its CSBH prepended.
     Consequently, the signed image is placed in a contiguous region starting
     from GRUB_LINUX_BZIMAGE_ADDR, and the entry point offset is updated
     accordingly.  */
  if (grub_cln_secure)
    {
      prot_size = img_size + sbh_len;
      code32_start_offs += (setup_sects << SECTOR_BITS) + SECTOR_SIZE
                           + sbh_len;
	}

  else
    prot_size = img_size - (setup_sects << SECTOR_BITS) - SECTOR_SIZE;

  if (! allocate_pages (real_size, prot_size))
    goto fail;

  errnum = intel_cln_imr_setup(IMR_RANGE_BOOT, (grub_addr_t)real_mode_mem, real_size); 	
  if(errnum != ERR_NONE)
    {
	grub_printf("IMR boot params setup failed !\n");
	goto fail;
    }

  errnum = intel_cln_imr_setup(IMR_RANGE_BZIMAGE, GRUB_LINUX_BZIMAGE_ADDR, prot_size);
  if(errnum != ERR_NONE)
    { 
	grub_printf("IMR bzimage setup failed !\n");
	goto fail;
    }

 __cln_test_imr();

 /* XXX Linux assumes that only elilo can boot Linux on EFI!!!  */
  lh->type_of_loader = 0x50;

  lh->cmd_line_ptr = (grub_uint32_t) (unsigned long) real_mode_mem + 0x1000;

  lh->heap_end_ptr = LINUX_HEAP_END_OFFSET;
  lh->loadflags |= LINUX_FLAG_CAN_USE_HEAP;

  lh->ramdisk_image = 0;
  lh->ramdisk_size = 0;

  grub_memset(real_mode_mem, 0, real_size);

  params = (struct linux_kernel_params *) real_mode_mem;

  grub_memmove(&params->hdr, lh, 0x202 + lh->jump_off - 0x1f1);

  /* Update offset for protected mode code entry point.  Clanton secure boot
     requires real mode code to be allocated contiguously to protected mode
     section.  Hence the kernel entry point must be shifted accordingly.  */
  params->hdr.code32_start += code32_start_offs;

  params->cl_magic = GRUB_LINUX_CL_MAGIC;
  params->cl_offset = 0x1000;

  /* These are not needed to be precise, because Linux uses these values
     only to raise an error when the decompression code cannot find good
     space.  */
  params->ext_mem = ((32 * 0x100000) >> 10);
  params->alt_mem = ((32 * 0x100000) >> 10);

  /* No APM on EFI.  */
  params->apm_version = 0;
  params->apm_code_segment = 0;
  params->apm_entry = 0;
  params->apm_16bit_code_segment = 0;
  params->apm_data_segment = 0;
  params->apm_flags = 0;
  params->apm_code_len = 0;
  params->apm_data_len = 0;

  /* XXX is there any way to use SpeedStep on EFI?  */
  params->ist_signature = 0;
  params->ist_command = 0;
  params->ist_event = 0;
  params->ist_perf_level = 0;

  /* Let the kernel probe the information.  */
  grub_memset (params->hd0_drive_info, 0, sizeof (params->hd0_drive_info));
  grub_memset (params->hd1_drive_info, 0, sizeof (params->hd1_drive_info));

  /* No MCA on EFI.  */
  params->rom_config_len = 0;

  if (grub_le_to_cpu16 (lh->version) >= 0x0206) {
    grub_memcpy(&params->version_0204.efi_signature, "EL32", 4);
    params->version_0206.efi_system_table = \
                        (grub_uint32_t) (unsigned long) grub_efi_system_table;
  } else if (grub_le_to_cpu16 (lh->version) >= 0x0204) {
    grub_memcpy(&params->version_0204.efi_signature, "EFIL", 4);
    params->version_0204.efi_system_table = \
                        (grub_uint32_t) (unsigned long) grub_efi_system_table;
  } else /* dunno */ {
    params->dunno.efi_signature = GRUB_LINUX_EFI_SIGNATURE_X64;
    params->dunno.efi_system_table = \
                        (grub_uint32_t) (unsigned long) grub_efi_system_table;
    params->dunno.efi_system_table_hi = PTR_HI(grub_efi_system_table);
  }
  /* The other EFI parameters are filled when booting.  */

  /* No EDD */
  params->eddbuf_entries = 0;
  params->edd_mbr_sig_buf_entries = 0;

  /* Dump MFH layout.conf contents */
  grub_cln_dump_layout ();

  /* XXX there is no way to know if the kernel really supports EFI.  */
  grub_printf ("[Linux-EFI%s, setup=0x%x, size=0x%x]\n",
               grub_cln_linux_spi ? " SPI" : "",
               (unsigned int) real_size, (unsigned int) prot_size);

  /* Check the mem= option to limit memory used for initrd.  */
  {
    char *mem;

    mem = grub_strstr (arg, "mem=");
    if (mem)
      {
	char *value = mem + 4;

	safe_parse_maxulong (&value, &linux_mem_size);
	switch (errnum)
	  {
	  case ERR_NUMBER_OVERFLOW:
	    /* If an overflow occurs, use the maximum address for
	       initrd instead. This is good, because MAXINT is
	       greater than LINUX_INITRD_MAX_ADDRESS.  */
	    linux_mem_size = LINUX_INITRD_MAX_ADDRESS;
	    errnum = ERR_NONE;
	    break;

	  case ERR_NONE:
	    {
	      int shift = 0;

	      switch (grub_tolower (*value))
		{
		case 'g':
		  shift += 10;
		case 'm':
		  shift += 10;
		case 'k':
		  shift += 10;
		default:
		  break;
		}

	      /* Check an overflow.  */
	      if (linux_mem_size > (~0UL >> shift))
		linux_mem_size = 0;
	      else
		linux_mem_size <<= shift;
	    }
	    break;

	  default:
	    linux_mem_size = 0;
	    errnum = ERR_NONE;
	    break;
	  }
      }
    else
      linux_mem_size = 0;
  }

	grub_printf("Before set up uart\n");
	grub_printf("arg is \"%s\"\n", arg);

  /* Expand Intel Quark's UART MMIO address if requested  */
  cln_detect_early_uart(&arg);

	grub_printf("After set up uart\n");
	grub_printf("arg is \"%s\"\n", arg);

  /* Skip the path to the kernel only if in file system mode.  */
  grub_stpcpy ((char *) real_mode_mem + 0x1000,
               grub_cln_linux_spi ? arg : skip_to (0, arg));

  /* If Clanton secure boot path, copy over CSBH + bzImage into "protected
     mode" section.  */
  if (grub_cln_secure)
    {
      grub_cln_asset_seek (0);
      prot_size -= sbh_len;
      grub_memcpy (prot_mode_mem, &cln_sbh, sizeof (cln_sbh));
      prot_mode_offs += sbh_len;
    }
  else
    grub_cln_asset_seek ((setup_sects << SECTOR_BITS) + SECTOR_SIZE);

  len = prot_size;
  if (grub_cln_asset_read (GRUB_CLN_ASSET_KERNEL,
                         (grub_uint8_t *) prot_mode_mem + prot_mode_offs,
                         len) != len)
    {
      errnum = ERR_EXEC_FORMAT;
      grub_printf ("Couldn't read file");
      goto fail;
    }

  /* Verify the kernel signature.  */
  if (grub_cln_secure &&
      ! grub_cln_verify_asset_signature ((grub_uint8_t *) prot_mode_mem))
    {
      errnum = ERR_CLN_VERIFICATION;
    }

  if (errnum == ERR_NONE)
    {
      loaded = 1;
    }

 fail:

  grub_close ();

 fail1:

  if (errnum != ERR_NONE)
    {
      loaded = 0;
    }
  return errnum ? KERNEL_TYPE_NONE : KERNEL_TYPE_BIG_LINUX;
}

int
grub_load_initrd (char *initrd)
{
  grub_ssize_t size;
  grub_addr_t addr_min, addr_max;
  grub_addr_t addr;
  grub_efi_uintn_t map_key;
  grub_efi_memory_descriptor_t *mmap_end;
  grub_efi_memory_descriptor_t *desc;
  grub_efi_memory_descriptor_t tdesc;
  grub_efi_uintn_t desc_size;
  grub_efi_uint32_t desc_version;
  struct linux_kernel_params *params;

  sbh_len = 0;

  /* In SPI/Flash mode, file system path to initrd is not required.  */
  if (! grub_cln_initrd_spi && initrd == NULL)
    {
      errnum = ERR_BAD_FILENAME;
      grub_printf ("No module specified");
      goto fail1;
    }

  if (! loaded)
    {
      errnum = ERR_UNRECOGNIZED;
      grub_printf ("You need to load the kernel first.");
      goto fail1;
    }


  if (grub_cln_secure)
    {
      if (! grub_cln_fetch_sbh (GRUB_CLN_ASSET_INITRD_CSBH, initrd, &cln_sbh))
        {
          if (ERR_FILE_NOT_FOUND == errnum
              && ! grub_cln_initrd_spi)
            errnum = ERR_SGN_FILE_NOT_FOUND;
          goto fail1;
        }
      sbh_len = cln_sbh.security_header.header_len;
    }

  if (! grub_cln_asset_open (GRUB_CLN_ASSET_INITRD, initrd))
    goto fail1;

  size = grub_cln_asset_size (GRUB_CLN_ASSET_INITRD);

  /* If Clanton secure boot, make room for CSBH (dedicated page).  */
  if (grub_cln_secure)
    size += PAGE_SIZE;

  initrd_pages = (page_align (size) >> 12);

  params = (struct linux_kernel_params *) real_mode_mem;
  grub_dprintf(__func__, "initrd_pages: %lu\n", initrd_pages);

  addr_max = grub_cpu_to_le32 (params->hdr.initrd_addr_max);
  if (linux_mem_size != 0 && linux_mem_size < addr_max)
    addr_max = linux_mem_size;
  addr_max &= ~((1 << 12)-1);

  /* Linux 2.3.xx has a bug in the memory range check, so avoid
     the last page.
     Linux 2.2.xx has a bug in the memory range check, which is
     worse than that of Linux 2.3.xx, so avoid the last 64kb.  */
  //addr_max -= 0x10000;

  /* Usually, the compression ratio is about 50%.  */
  addr_min = (grub_addr_t) prot_mode_mem + ((prot_mode_pages * 3) << 12);
  grub_dprintf(__func__, "prot_mode_mem=%p prot_mode_pages=%lu\n", prot_mode_mem, prot_mode_pages);

  /* Find the highest address to put the initrd.  */
  if (grub_efi_get_memory_map (&map_key, &desc_size, &desc_version) <= 0)
    {
      grub_printf ("cannot get memory map");
      errnum = ERR_BOOT_FAILURE;
      goto fail;
    }

  mmap_end = NEXT_MEMORY_DESCRIPTOR (mmap_buf, mmap_size);
  addr = 0;
  for (desc = mmap_buf;
       desc < mmap_end;
       desc = NEXT_MEMORY_DESCRIPTOR (desc, desc_size))
    {
      if (desc->type != GRUB_EFI_CONVENTIONAL_MEMORY)
        continue;
      memcpy(&tdesc, desc, sizeof (tdesc));
      if (tdesc.physical_start < addr_min
              && tdesc.num_pages > ((addr_min - tdesc.physical_start) >> 12))
        {
          tdesc.num_pages -= ((addr_min - tdesc.physical_start) >> 12);
          tdesc.physical_start = addr_min;
        }

      grub_dprintf(__func__, "desc = {type=%d,ps=0x%llx,vs=0x%llx,sz=%llu,attr=%llu}\n", desc->type, (unsigned long long)desc->physical_start, (unsigned long long)desc->virtual_start, (unsigned long long)desc->num_pages, (unsigned long long)desc->attribute);
      if (tdesc.physical_start >= addr_min
	  && tdesc.physical_start + page_align (size) <= addr_max
	  && tdesc.num_pages >= initrd_pages)
	{
	  grub_efi_physical_address_t physical_end;

	  physical_end = tdesc.physical_start + (tdesc.num_pages << 12);
	  if (physical_end > addr_max)
	    physical_end = addr_max;

	  if (physical_end <= 0x7fffffffUL && physical_end > addr)
	    addr = physical_end - page_align (size);
	}
    }

  if (addr == 0)
    {
      errnum = ERR_UNRECOGNIZED;
      grub_printf ("no free pages available");
      goto fail;
    }

  initrd_mem = grub_efi_allocate_pages (addr, initrd_pages);
  if (! initrd_mem)
    {
      grub_printf ("cannot allocate pages: %x@%x", (unsigned) initrd_pages,
                   (unsigned) addr);
      errnum = ERR_WONT_FIT;
      goto fail;
    }

  /*
     Clanton secure boot requires the CSBH to be prepended to the Initrd.
     Since Linux requires the Initrd to be page-aligned, do the following:
     1. copy Initrd to the 2nd allocated page
     2. copy the CSBH to the 1st allocated page at an offset such that
        CSBH and Initrd are adjacent
   */
  if (grub_cln_secure)
    {
      size -= PAGE_SIZE;
      grub_memcpy ((grub_uint8_t *) initrd_mem + PAGE_SIZE - sbh_len,
                   &cln_sbh, sizeof (cln_sbh));
      initrd_mem = (grub_uint8_t *) initrd_mem + PAGE_SIZE;
      addr += PAGE_SIZE;
    }

  if (grub_cln_asset_read (GRUB_CLN_ASSET_INITRD, initrd_mem, size) != size)
    {
      errnum = ERR_EXEC_FORMAT;
      grub_printf ("Couldn't read file");
      goto fail;
    }

  grub_printf ("[Initrd%s, addr=0x%x, size=0x%x]\n",
               grub_cln_initrd_spi ? " SPI" : "",
               (unsigned int) addr, (unsigned int) size);

  if (grub_cln_secure)
    {
      /* Verify the initrd signature.  */
      if (! grub_cln_verify_asset_signature ((grub_uint8_t *)
                                             initrd_mem - sbh_len))
        {
          errnum = ERR_CLN_VERIFICATION;
          goto fail;
        }

      /* Free up page allocated to CSBH.  */
      grub_efi_free_pages ((grub_addr_t)
                           ((grub_uint8_t *) initrd_mem - PAGE_SIZE), 1);
    }

  params->hdr.ramdisk_image = addr;
  params->hdr.ramdisk_size = size;

 fail:
  grub_close ();
 fail1:
  return !errnum;
}
