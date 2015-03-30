/*  
 *  Copyright (C) 2015 Maxim Malkov, ISPRAS <malkov@ispras.ru> 
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __POK_PPC_IOPORTS_H__
#define __POK_PPC_IOPORTS_H__

#include <stdint.h>

static inline void outb(unsigned int port, uint8_t value)
{
    (void) port; (void) value;
}

static inline uint8_t inb(unsigned int port)
{
    (void) port;
    return 0;
}

static inline void outw(unsigned int port, uint16_t value)
{
    (void) port; (void) value;
}

static inline uint16_t inw(unsigned int port)
{
    (void) port;
    return 0;
}

static inline void outl(unsigned int port, uint32_t value)
{
    (void) port; (void) value;
}

static inline uint32_t inl(unsigned int port)
{
    (void) port;
    return 0;
}

#endif // __POK_PPC_IOPORTS_H__