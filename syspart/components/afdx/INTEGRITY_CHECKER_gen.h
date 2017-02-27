/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/afdx/config.yaml).
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

#ifndef __INTEGRITY_CHECKER_GEN_H__
#define __INTEGRITY_CHECKER_GEN_H__

    #include <types.h>

    #include <interfaces/time_message_handler_gen.h>

    #include <interfaces/time_message_handler_gen.h>

typedef struct INTEGRITY_CHECKER_state {
    uint8_t last_in_seq_number;
    uint8_t network_card;
    pok_bool_t first_message_received;
}INTEGRITY_CHECKER_state;

typedef struct {
    INTEGRITY_CHECKER_state state;
    struct {
            struct {
                time_message_handler ops;
            } portA;
    } in;
    struct {
            struct {
                time_message_handler *ops;
                self_t *owner;
            } portB;
    } out;
} INTEGRITY_CHECKER;



      ret_t integrity_checker_receive_packet(INTEGRITY_CHECKER *, const char *, size_t, SYSTEM_TIME_TYPE);

      ret_t INTEGRITY_CHECKER_call_portB_handle(INTEGRITY_CHECKER *, const char *, size_t, SYSTEM_TIME_TYPE);



    void integrity_checker_init(INTEGRITY_CHECKER *);



#endif
