/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/arinc/config.yaml).
 */
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
 */

#ifndef __ARINC_PORT_WRITER_GEN_H__
#define __ARINC_PORT_WRITER_GEN_H__

#include <memblocks.h>
    #include <arinc653/queueing.h>
    #include <arinc653/sampling.h>
    #include <port_info.h>

    #include <interfaces/message_handler_gen.h>


typedef struct ARINC_PORT_WRITER_state {
    PORT_DIRECTION_TYPE port_direction;
    MESSAGE_RANGE_TYPE q_port_max_nb_messages;
    MESSAGE_SIZE_TYPE port_max_message_size;
    NAME_TYPE port_name;
    int is_queuing_port;
    APEX_INTEGER port_id;
}ARINC_PORT_WRITER_state;

typedef struct {
    char instance_name[16];
    ARINC_PORT_WRITER_state state;
    struct {
            struct {
                message_handler ops;
            } portA;
    } in;
    struct {
    } out;
} ARINC_PORT_WRITER;



      ret_t arinc_receive_message(ARINC_PORT_WRITER *, const uint8_t *, size_t);




    void arinc_port_writer_init(ARINC_PORT_WRITER *);



#endif