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

#include <config.h>

#include <errno.h>
#include <core/debug.h>
#include "cons.h"
#include <bsp_common.h>
#include "space.h"

#include "devtree.h"

pok_bsp_t pok_bsp = {
    .ccsrbar_size = 0x1000000ULL,
    .ccsrbar_base = 0xE0000000ULL,
    .ccsrbar_base_phys = 0xFE0000000ULL,
    .dcfg_offset = 0xE0000UL,
    .serial0_regs_offset = 0x4500ULL,
    .serial1_regs_offset = 0x4600ULL,
    .timebase_freq = 400000000,
    .pci_bridge = {
        .cfg_addr = 0xe0008000,
        .cfg_data = 0xe0008004,
        .iorange =  0xe1000000
    }
};

extern char _end[];

/* Initialize bsp-related functionality. Called from entry.S. */
void pok_bsp_init (void)
{
   pok_cons_init ();

   //devtree_dummy_dump();
   if ((uintptr_t) _end > 0x4000000ULL)
       pok_fatal("Kernel size is more than 64 megabytes");
}

void pok_bsp_get_info(void *addr) {
    pok_bsp_t *data = addr;
    *data = pok_bsp;
}

