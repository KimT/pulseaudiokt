#ifndef foodbusutilhfoo
#define foodbusutilhfoo

/***
  This file is part of PulseAudio.

  Copyright 2006 Shams E. King

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include <dbus/dbus.h>

#include <pulsecore/llist.h>
#include <pulse/mainloop-api.h>
#include <pulse/proplist.h>

/* A wrap connection is not shared or refcounted, it is available in client side */
typedef struct pa_dbus_wrap_connection pa_dbus_wrap_connection;

pa_dbus_wrap_connection* pa_dbus_wrap_connection_new(pa_mainloop_api *mainloop, pa_bool_t use_rtclock, DBusBusType type, DBusError *error);
pa_dbus_wrap_connection* pa_dbus_wrap_connection_new_from_existing(
        pa_mainloop_api *mainloop,
        pa_bool_t use_rtclock,
        DBusConnection *conn);
void pa_dbus_wrap_connection_free(pa_dbus_wrap_connection* conn);

DBusConnection* pa_dbus_wrap_connection_get(pa_dbus_wrap_connection *conn);

int pa_dbus_add_matches(DBusConnection *c, DBusError *error, ...) PA_GCC_SENTINEL;
void pa_dbus_remove_matches(DBusConnection *c,  ...) PA_GCC_SENTINEL;

typedef struct pa_dbus_pending pa_dbus_pending;

struct pa_dbus_pending {
    DBusConnection *connection;
    DBusMessage *message;
    DBusPendingCall *pending;

    void *context_data;
    void *call_data;

    PA_LLIST_FIELDS(pa_dbus_pending);
};

pa_dbus_pending *pa_dbus_pending_new(DBusConnection *c, DBusMessage *m, DBusPendingCall *pending, void *context_data, void *call_data);
void pa_dbus_pending_free(pa_dbus_pending *p);

/* Sync up a list of pa_dbus_pending_call objects */
void pa_dbus_sync_pending_list(pa_dbus_pending **p);

/* Free up a list of pa_dbus_pending_call objects */
void pa_dbus_free_pending_list(pa_dbus_pending **p);

/* Sends an error message as the reply to the given message. */
void pa_dbus_send_error(
        DBusConnection *c,
        DBusMessage *in_reply_to,
        const char *name,
        const char *format, ...) PA_GCC_PRINTF_ATTR(4, 5);

void pa_dbus_send_empty_reply(DBusConnection *c, DBusMessage *in_reply_to);
void pa_dbus_send_basic_value_reply(DBusConnection *c, DBusMessage *in_reply_to, int type, void *data);
void pa_dbus_send_basic_variant_reply(DBusConnection *c, DBusMessage *in_reply_to, int type, void *data);
void pa_dbus_send_basic_array_variant_reply(
        DBusConnection *c,
        DBusMessage *in_reply_to,
        int item_type,
        void *array,
        unsigned n);
void pa_dbus_send_proplist_variant_reply(DBusConnection *c, DBusMessage *in_reply_to, pa_proplist *proplist);

void pa_dbus_append_basic_array(DBusMessageIter *iter, int item_type, const void *array, unsigned n);
void pa_dbus_append_basic_array_variant(DBusMessageIter *iter, int item_type, const void *array, unsigned n);
void pa_dbus_append_basic_variant(DBusMessageIter *iter, int type, void *data);
void pa_dbus_append_basic_variant_dict_entry(DBusMessageIter *dict_iter, const char *key, int type, void *data);
void pa_dbus_append_basic_array_variant_dict_entry(
        DBusMessageIter *dict_iter,
        const char *key,
        int item_type,
        const void *array,
        unsigned n);
void pa_dbus_append_proplist(DBusMessageIter *iter, pa_proplist *proplist);
void pa_dbus_append_proplist_variant(DBusMessageIter *iter, pa_proplist *proplist);
void pa_dbus_append_proplist_variant_dict_entry(DBusMessageIter *dict_iter, const char *key, pa_proplist *proplist);

/* Returns a new proplist that the caller has to free. If the proplist contains
 * invalid keys, an error reply is sent and NULL is returned. The iterator must
 * point to "a{say}" element. This function calls dbus_message_iter_next(iter)
 * before returning. */
pa_proplist *pa_dbus_get_proplist_arg(DBusConnection *c, DBusMessage *msg, DBusMessageIter *iter);

#endif
