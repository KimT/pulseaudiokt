/***
    This file is part of PulseAudio.

    Copyright 2010 Kim Therkelsen

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulse/xmalloc.h>
#include <pulse/i18n.h>

#include <pulsecore/core.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/namereg.h>
#include <pulsecore/core-util.h>

#include "module-proxy-sink-symdef.h"

PA_MODULE_AUTHOR("Kim Therkelsen");
PA_MODULE_DESCRIPTION(_("Automatically load/unload module when master port changes."));
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(FALSE);
PA_MODULE_USAGE(
    "sink_name=<some name> master=<master sink> pa_master_ports=<comma separated list of master ports> module_to_load=<name of sink to load/unload> module_to_load_args=<the arguments for the sink module>");


static const char* const valid_modargs[] = {
    "sink_name",
    "master",
    "pa_master_ports",
    "module_to_load",
    "module_to_load_args",
    NULL,
};

struct userdata {
    pa_hook_slot *sink_port_changed_slot;

    pa_sink *master; /* Pointer to the master sink */
    pa_module *m; /* Pointer to the module that should be loaded */
    pa_modargs * ppaModuleArgs;

    unsigned short noOfSupportedPAMasterPorts;
    char  ** ppSupportedPAMasterPortNames;
    const char *sink_name;
    const char *module_to_load_name;
    const char *module_to_load_args;
};

static pa_bool_t is_current_sink_port_supported(struct userdata* u) {
    /* Check if the current master sink port is in the list of supported master port names */
    char *k;
    unsigned long p, j;
    short bPortFound = 0;
    k = u->master->active_port->name;

    for (p = 0; p < u->noOfSupportedPAMasterPorts; p++) {
        /*Compare strings*/
        if (strlen(k) != strlen(u->ppSupportedPAMasterPortNames[p])) {
            bPortFound = 0;
            continue;
        }
        bPortFound = 1;
        for (j = 0; j < strlen(k); j++)
        {
            if (k[j] != u->ppSupportedPAMasterPortNames[p][j]) {
                bPortFound = 0;
                break;
            }
        }
        if (bPortFound == 1)
            break;
    }

    if (!bPortFound) {
        pa_log_debug("The PA active master port (%s) is not in the list of supported master PA ports. The module will be/remain unloaded until the master port changes to one of the supported ports.", u->master->active_port->name);
        return FALSE;
    }
    else
        pa_log_debug("The module supports the selected port and will now be loaded.");

    return TRUE;
}

static void load_or_unload_sink(pa_core *c, struct userdata* u, pa_bool_t bForceRemove) {
    pa_sink *tmpSink;
    uint32_t idx;
    pa_bool_t bLoadSink = TRUE, bSinkLoaded = FALSE;

    pa_assert(c);
    pa_assert(u);

    /* Check if sink should be loaded */
    if (bForceRemove)
        bLoadSink = FALSE;
    else if (!is_current_sink_port_supported(u)) {
        pa_log_debug("Sink should not be loaded - master port is not in list.");
        bLoadSink = FALSE;
    }

    /* Check if sink is already loaded */
    if (u->m == NULL) /* The module has not been loaded (or at least not by this module-proxy-sink instance) */
        bSinkLoaded = FALSE;
    else { /* Check if the module is (still) loaded */
        for (tmpSink = pa_idxset_first(c->sinks, &idx); tmpSink; tmpSink = pa_idxset_next(c->sinks, &idx))
            if (tmpSink->module == u->m) {
                bSinkLoaded = TRUE;
                break;
            }
    }

    /* Unload sink if needed */
    if ((!bLoadSink) && bSinkLoaded) {
        pa_module_unload(c, u->m, TRUE);
        u->m = NULL;
    }

    /* Load sink if needed */
    if (bLoadSink && (!bSinkLoaded)) {
        u->m = pa_module_load(c, u->module_to_load_name, u->module_to_load_args);
        if (!u->m)
            pa_log_warn("Unable to load sink module.");
    }
}

/* Called from main context */
static pa_hook_result_t sink_port_changed_hook_cb(pa_core *c, pa_sink *s, struct userdata* u) {

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    load_or_unload_sink(c, u, FALSE);
    return PA_HOOK_OK;
}


int pa__init(pa_module*m) {
    struct userdata *u = NULL;
    const char *masterPortsArg;

    pa_assert(m);

    m->userdata = u = pa_xnew(struct userdata, 1);
    /* It is important to initialize variables at this point in case loading fails and pa__done is called before the variables have been truly initialized*/
    u->m = NULL;
    u->noOfSupportedPAMasterPorts = 0;
    u->ppSupportedPAMasterPortNames = NULL;
    u->sink_port_changed_slot = NULL;

    if (!(u->ppaModuleArgs = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    if (!(u->master = pa_namereg_get(m->core, pa_modargs_get_value(u->ppaModuleArgs, "master", NULL), PA_NAMEREG_SINK))) {
        pa_log("Master sink not found");
        goto fail;
    }

    if (!(u->sink_name = pa_modargs_get_value(u->ppaModuleArgs, "sink_name", NULL))) {
        pa_log("The sink name is missing.");
        goto fail;
    }

    if (!(u->module_to_load_name = pa_modargs_get_value(u->ppaModuleArgs, "module_to_load", NULL))) {
        pa_log("The name of the module to load is missing.");
        goto fail;
    }

    if (!(u->module_to_load_args = pa_modargs_get_value(u->ppaModuleArgs, "module_to_load_args", NULL)))
        pa_log("No module arguments specified. Assumes the module does not need arguments.");

    /* Parse the pa_master_ports argument [begin] */
    if (!(masterPortsArg = pa_modargs_get_value(u->ppaModuleArgs, "pa_master_ports", NULL))) {
        pa_log("The pa_master_ports argument is missing. The argument should contain a comma-separated list of master port names.");
        goto fail;
    }
    else {
        const char *state = NULL;
        char *k;
        unsigned long p, j;

        /* Figure out how many names there are in the comma-separated list and allocate memory*/
        p = 0;
        while ((k = pa_split(masterPortsArg, ",", &state))) {
            p++;
        }

        u->ppSupportedPAMasterPortNames = pa_xnew(typeof(char*), (unsigned) p);
        u->noOfSupportedPAMasterPorts = (unsigned short)p;
        p = 0;
        state = NULL;
        while ((k = pa_split(masterPortsArg, ",", &state))) {
            if (*k == 0) {
                pa_xfree(k);
                continue;
            }

            u->ppSupportedPAMasterPortNames[p++] = pa_xstrdup(k);
            pa_xfree(k);
        }

        for (j = 0; j < p; j++)
            pa_log_debug("Parsed PA master port: %s", u->ppSupportedPAMasterPortNames[j]);

    }
    /* Parse the pa_master_ports argument [end] */

    /* Subscribe to the PA_CORE_HOOK_SINK_PORT_CHANGED event */
    u->sink_port_changed_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PORT_CHANGED], PA_HOOK_NORMAL, (pa_hook_cb_t) sink_port_changed_hook_cb, u);//PA_HOOK_LATE? PA_HOOK_EARLY?

    load_or_unload_sink(m->core, u, FALSE);

    return 0;

fail:
    pa__done(m);
    return -1;
}

void pa__done(pa_module*m) {
    struct userdata *u = NULL;
    unsigned int c;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sink_port_changed_slot != NULL) {
        pa_hook_slot_free(u->sink_port_changed_slot);
        load_or_unload_sink(m->core, u, TRUE);
    }

    /* Free allocated PA master port name array */
    if ((u->ppSupportedPAMasterPortNames != NULL) && (u->noOfSupportedPAMasterPorts > 0)) {
        for (c = 0; c < u->noOfSupportedPAMasterPorts; c++)
            pa_xfree(u->ppSupportedPAMasterPortNames[c]);
        pa_xfree(u->ppSupportedPAMasterPortNames);
    }

    if (u->ppaModuleArgs)
        pa_modargs_free(u->ppaModuleArgs);

    pa_xfree(u);
}
