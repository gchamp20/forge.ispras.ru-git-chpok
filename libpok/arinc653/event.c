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

#include <config.h>

#include "event.h"

#include <arinc653/types.h>
#include <arinc653/event.h>
#include <types.h>
#include <utils.h>

#include <kernel_shared_data.h>
#include <core/assert_os.h>

#include <string.h>
#include "arinc_config.h"
#include "arinc_process_queue.h"

#include "map_error.h"

static unsigned nevents_used = 0;
static inline EVENT_ID_TYPE index_to_id(unsigned index)
{
    // Avoid 0 value.
    return index + 1;
}

static inline unsigned id_to_index(EVENT_ID_TYPE id)
{
    return id - 1;
}


/* Find event by name (in UPPERCASE). Returns NULL if not found. */
static struct arinc_event* find_event(const char* name)
{
   for (unsigned i = 0; i < nevents_used; i++)
   {
      struct arinc_event* event = &arinc_events[i];
      if(strncasecmp(event->event_name, name, MAX_NAME_LENGTH) == 0)
         return event;
   }

   return NULL;
}


void CREATE_EVENT (EVENT_NAME_TYPE EVENT_NAME,
                   EVENT_ID_TYPE *EVENT_ID,
                   RETURN_CODE_TYPE *RETURN_CODE)
{
   if(kshd->partition_mode == POK_PARTITION_MODE_NORMAL) {
      // Cannot create buffer in NORMAL mode
      *RETURN_CODE = INVALID_MODE;
      return;
   }

   if(find_event(EVENT_NAME) != NULL) {
      // Event with given name already exists.
      *RETURN_CODE = NO_ACTION;
      return;
   }

   if(nevents_used == arinc_config_nevents) {
      // Too many events
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   struct arinc_event* event = &arinc_events[nevents_used];

   memcpy(event->event_name, EVENT_NAME, MAX_NAME_LENGTH);
   msection_init(&event->section);
   msection_wq_init(&event->process_queue);
   event->event_state = DOWN;

   *EVENT_ID = index_to_id(nevents_used);

   nevents_used++;

   *RETURN_CODE = NO_ERROR;
}

void SET_EVENT (EVENT_ID_TYPE EVENT_ID,
                RETURN_CODE_TYPE *RETURN_CODE)
{
    unsigned index = id_to_index(EVENT_ID);
   if (index >= nevents_used) {
      // Incorrect event identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   struct arinc_event* event = &arinc_events[index];

   msection_enter(&event->section);

   event->event_state = UP;

   if(msection_wq_notify(&event->section, &event->process_queue, TRUE)
      == EOK) {
      // There are processes waiting for event.
      // We are already woken up them, so just cleanup the process queue.
      pok_thread_id_t t = event->process_queue.first;

      do {
         msection_wq_del(&event->process_queue, t);

         t = event->process_queue.first;
      } while(t != JET_THREAD_ID_NONE);
   }

   msection_leave(&event->section);
   *RETURN_CODE = NO_ERROR;
}

void RESET_EVENT (EVENT_ID_TYPE EVENT_ID,
                  RETURN_CODE_TYPE *RETURN_CODE)
{
    unsigned index = id_to_index(EVENT_ID);
   if (index >= nevents_used) {
      // Incorrect event identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   struct arinc_event* event = &arinc_events[index];

   msection_enter(&event->section);
   event->event_state = DOWN;
   msection_leave(&event->section);

   *RETURN_CODE = NO_ERROR;
}

void WAIT_EVENT (EVENT_ID_TYPE EVENT_ID,
                 SYSTEM_TIME_TYPE TIME_OUT,
                 RETURN_CODE_TYPE *RETURN_CODE)
{
    unsigned index = id_to_index(EVENT_ID);
   if (index >= nevents_used) {
      // Incorrect event identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   struct arinc_event* event = &arinc_events[index];

   msection_enter(&event->section);

   if(event->event_state == UP) {
      // Event is already ready.
      *RETURN_CODE = NO_ERROR;
   }
   else if(TIME_OUT == 0)
   {
      // Event is DOWN but waiting is not requested.
      *RETURN_CODE = NOT_AVAILABLE;
   }
   else {
      // Event is DOWN and waiting is *requested* by the caller.
      // (whether waiting is *allowed* will be checked by the kernel.)
      pok_thread_id_t t = kshd->current_thread_id;

      /*
       * ARINC 1-4 explicitely says, that:
       * 
       * When processes are waiting on an event and another process sets
       * the event to up, then all the processes waiting on the event
       * become ready to run and are released on a priority followed by
       * FIFO (when priorities are equal) basis.
       */
      arinc_process_queue_add_common(&event->process_queue, PRIORITY);

      jet_ret_t core_ret = msection_wait(&event->section, TIME_OUT);

      MAP_ERROR_BEGIN(core_ret)
         MAP_ERROR(EOK, NO_ERROR);
         MAP_ERROR(JET_INVALID_MODE, INVALID_MODE);
         MAP_ERROR(ETIMEDOUT, TIMED_OUT);
         MAP_ERROR_CANCELLED();
         MAP_ERROR_DEFAULT();
      MAP_ERROR_END()

      switch(core_ret)
      {
      case EOK:
         // Event become UP.
         break;
      case JET_INVALID_MODE: // Waiting is not allowed
      case JET_CANCELLED: // Thread has been STOP()-ed or [IPPC] server thread has been cancelled.
      case ETIMEDOUT: // Timeout
         msection_wq_del(&event->process_queue, t);
         break;
      default:
         assert_os(FALSE);
      }
   }
   msection_leave(&event->section);
}

void GET_EVENT_ID (EVENT_NAME_TYPE EVENT_NAME,
                   EVENT_ID_TYPE *EVENT_ID,
                   RETURN_CODE_TYPE *RETURN_CODE)
{

   struct arinc_event* event = find_event(EVENT_NAME);
   if(event == NULL) {
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   *EVENT_ID = index_to_id(event - arinc_events);
   *RETURN_CODE = NO_ERROR;
}

void GET_EVENT_STATUS (EVENT_ID_TYPE EVENT_ID,
                       EVENT_STATUS_TYPE *EVENT_STATUS,
                       RETURN_CODE_TYPE *RETURN_CODE)
{
    unsigned index = id_to_index(EVENT_ID);
   if (index >= nevents_used) {
      // Incorrect event identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   struct arinc_event* event = &arinc_events[index];

   msection_enter(&event->section);
   EVENT_STATUS->EVENT_STATE = event->event_state;
   EVENT_STATUS->WAITING_PROCESSES = msection_wq_size(&event->section,
      &event->process_queue);

   msection_leave(&event->section);

   *RETURN_CODE = NO_ERROR;
}
