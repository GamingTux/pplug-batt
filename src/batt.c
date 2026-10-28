/*============================================================================
Copyright (c) 2018-2025 Raspberry Pi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <string.h>
#include <glib/gi18n.h>

#include "plugin.h"

#include "batt_sys.h"
#include "batt.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#define SIM_INTERVAL 500
#define INTERVAL 5000

/* Battery states */
typedef enum
{
    STAT_UNKNOWN = -1,
    STAT_DISCHARGING = 0,
    STAT_CHARGING = 1,
    STAT_EXT_POWER = 2
} status_t;

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

conf_table_t conf_table[2] = {
    {CONF_TYPE_INT,  "batt_num", N_("Battery number to monitor"),   NULL,   "0" },
    {CONF_TYPE_NONE, NULL,       NULL,                              NULL,   NULL}
};

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static int init_measurement (PtBattPlugin *pt);
static int charge_level (PtBattPlugin *pt, status_t *status, int *tim);
static void draw_icon (PtBattPlugin *pt, int lev, float r, float g, float b, int powered);
static void append_battery_details (battery *b, GString *tip);
static void update_icon (PtBattPlugin *pt);
static gboolean timer_event (PtBattPlugin *pt);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/* Initialise measurements and check for a battery */

static int init_measurement (PtBattPlugin *pt)
{
    if (pt->simulate) return 1;

    pt->batt = battery_get (pt->batt_num);
    if (pt->batt) return 1;

    return 0;
}

/* Read current capacity, status and time remaining from battery */

static int charge_level (PtBattPlugin *pt, status_t *status, int *tim)
{
    if (pt->simulate)
    {
        static int level = 0;
       *tim = 30;
        if (level < 100) level +=5;
        else level = -100;
        if (level < 0)
        {
            *status = STAT_DISCHARGING;
            return (level * -1);
        }
        else if (level == 100) *status = STAT_EXT_POWER;
        else *status = STAT_CHARGING;
        return level;
    }
    *status = STAT_UNKNOWN;
    *tim = 0;
    battery *b = pt->batt;
    int mins;
    if (b)
    {
        battery_update (b);
        if (battery_is_charging (b))
        {
            if (b->state && (strcasecmp (b->state, "full") == 0 || strcasecmp (b->state, "not charging") == 0))
                *status = STAT_EXT_POWER;
            else if (b->state && strcasecmp (b->state, "charging") == 0)
                *status = STAT_CHARGING;
            else
                *status = STAT_EXT_POWER;
        }
        else *status = STAT_DISCHARGING;
        mins = b->seconds;
        mins /= 60;
        *tim = mins;
        return b->percentage;
    }
    else return -1;
}


/* Draw the icon in relevant colour and fill level */

static void draw_icon (PtBattPlugin *pt, int lev, float r, float g, float b, int powered)
{
    int h, w, f, ic; 

    // calculate dimensions based on icon size
    ic = wrap_icon_size (pt);
    w = ic < 36 ? 36 : ic;
    h = ((w * 10) / 36) * 2; // force it to be even
    if (h < 18) h = 18;
    if (h >= ic) h = ic - 2;

    // create and clear the drawing surface
    cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create (surface);
    cairo_set_source_rgba (cr, 0, 0, 0, 0);
    cairo_rectangle (cr, 0, 0, w, h);
    cairo_fill (cr);

    // draw base icon on surface
    cairo_set_source_rgb (cr, r, g, b);
    cairo_rectangle (cr, 4, 1, w - 10, 1);
    cairo_rectangle (cr, 3, 2, w - 8, 1);
    cairo_rectangle (cr, 3, h - 3, w - 8, 1);
    cairo_rectangle (cr, 4, h - 2, w - 10, 1);
    cairo_rectangle (cr, 2, 3, 2, h - 6);
    cairo_rectangle (cr, w - 6, 3, 2, h - 6);
    cairo_rectangle (cr, w - 4, (h >> 1) - 3, 2, 6);
    cairo_fill (cr);

    cairo_set_source_rgba (cr, r, g, b, 0.5);
    cairo_rectangle (cr, 3, 1, 1, 1);
    cairo_rectangle (cr, 2, 2, 1, 1);
    cairo_rectangle (cr, 2, h - 3, 1, 1);
    cairo_rectangle (cr, 3, h - 2, 1, 1);
    cairo_rectangle (cr, w - 6, 1, 1, 1);
    cairo_rectangle (cr, w - 5, 2, 1, 1);
    cairo_rectangle (cr, w - 5, h - 3, 1, 1);
    cairo_rectangle (cr, w - 6, h - 2, 1, 1);
    cairo_fill (cr);

    // fill the battery
    if (lev < 0) f = 0;
    else if (lev > 97) f = w - 12;
    else
    {
        f = (w - 12) * lev;
        f /= 97;
        if (f > w - 12) f = w - 12;
    }
    cairo_set_source_rgb (cr, r, g, b);
    cairo_rectangle (cr, 5, 4, f, h - 8);
    cairo_fill (cr);

    // show icons
    if (powered == 1 && pt->flash)
    {
        gdk_cairo_set_source_pixbuf (cr, pt->flash, (w >> 1) - 15, (h >> 1) - 16);
        cairo_paint (cr);
    }
    if (powered == 2 && pt->plug)
    {
        gdk_cairo_set_source_pixbuf (cr, pt->plug, (w >> 1) - 16, (h >> 1) - 16);
        cairo_paint (cr);
    }

    // create a pixbuf from the cairo surface
    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, w, h);

    // copy the pixbuf to the icon resource
    g_object_ref_sink (pt->tray_icon);
    gtk_image_set_from_pixbuf (GTK_IMAGE (pt->tray_icon), pixbuf);

    g_object_unref (pixbuf);
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
}

/* Append optional battery telemetry attributes if provided by the kernel driver */

static void append_battery_details (battery *b, GString *tip)
{
    if (!b) return;

    /* Power and current */
    if (b->power_now > 0 && b->current_now > 0)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Power : %0.2f W (%0.2f A)"),
            b->power_now / 1000.0, b->current_now / 1000.0);
    }
    else if (b->power_now > 0)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Power : %0.2f W"), b->power_now / 1000.0);
    }
    else if (b->current_now > 0)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Current : %0.2f A"), b->current_now / 1000.0);
    }

    /* Voltage and design range */
    if (b->voltage_now > 0)
    {
        g_string_append (tip, "\n");
        if (b->voltage_min_design > 0 && b->voltage_max_design > 0)
            g_string_append_printf (tip, _("Voltage : %0.2f V (%0.1f-%0.1f V)"),
                b->voltage_now / 1000.0, b->voltage_min_design / 1000.0, b->voltage_max_design / 1000.0);
        else
            g_string_append_printf (tip, _("Voltage : %0.2f V"), b->voltage_now / 1000.0);
    }

    /* Energy (Wh) and/or Charge capacity (mAh) */
    if (b->energy_now >= 0 && b->energy_full > 0 && b->charge_now >= 0 && b->charge_full > 0)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Energy : %0.1f / %0.1f Wh (%d / %d mAh)"),
            b->energy_now / 1000.0, b->energy_full / 1000.0, b->charge_now, b->charge_full);
    }
    else if (b->energy_now >= 0 && b->energy_full > 0)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Energy : %0.1f / %0.1f Wh"),
            b->energy_now / 1000.0, b->energy_full / 1000.0);
    }
    else if (b->charge_now >= 0 && b->charge_full > 0)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Capacity : %d / %d mAh"), b->charge_now, b->charge_full);
    }

    /* Health & State of Health (%) */
    if (b->health && b->health_pct > 0)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Health : %s (%d%%)"), _(b->health), b->health_pct);
    }
    else if (b->health)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Health : %s"), _(b->health));
    }
    else if (b->health_pct > 0)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Health : %d%%"), b->health_pct);
    }

    /* Cycle count */
    if (b->cycle_count >= 0)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Cycle count : %d"), b->cycle_count);
    }

    /* Charge control end threshold (if active < 100%) */
    if (b->charge_threshold > 0 && b->charge_threshold < 100)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Charge limit : %d%%"), b->charge_threshold);
    }

    /* Temperature */
    if (b->temp_deci_c != G_MININT)
    {
        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Temperature : %0.1f °C"), b->temp_deci_c / 10.0);
    }

    /* Manufacturer / Model / Technology */
    if (b->manufacturer || b->model_name || b->technology)
    {
        GString *mdl = g_string_new (NULL);
        if (b->manufacturer && b->model_name)
            g_string_append_printf (mdl, "%s %s", b->manufacturer, b->model_name);
        else if (b->manufacturer)
            g_string_append (mdl, b->manufacturer);
        else if (b->model_name)
            g_string_append (mdl, b->model_name);

        if (b->technology)
        {
            if (mdl->len > 0)
                g_string_append_printf (mdl, " (%s)", b->technology);
            else
                g_string_append (mdl, b->technology);
        }

        g_string_append (tip, "\n");
        g_string_append_printf (tip, _("Model : %s"), mdl->str);
        g_string_free (mdl, TRUE);
    }
}

/* Read the current charge state and update the icon accordingly */

static void update_icon (PtBattPlugin *pt)
{
    int capacity, time;
    status_t status;
    float ftime;

    if (!pt->timer)
    {
        gtk_widget_hide (pt->plugin);
        return;
    }

    // read the charge status
    capacity = charge_level (pt, &status, &time);
    if (status == STAT_UNKNOWN) return;
    ftime = time / 60.0;

    GString *tip = g_string_new (NULL);

    // fill the battery symbol and create the tooltip
    if (status == STAT_CHARGING)
    {
        if (time <= 0)
            g_string_append_printf (tip, _("Charging : %d%%"), capacity);
        else if (time < 90)
            g_string_append_printf (tip, _("Charging : %d%%\nTime remaining : %d minutes"), capacity, time);
        else
            g_string_append_printf (tip, _("Charging : %d%%\nTime remaining : %0.1f hours"), capacity, ftime);
        draw_icon (pt, capacity, 0.95, 0.64, 0, 1);
    }
    else if (status == STAT_EXT_POWER)
    {
        g_string_append_printf (tip, _("Charged : %d%%\nOn external power"), capacity);
        draw_icon (pt, capacity, 0, 0.85, 0, 2);
    }
    else
    {
        if (time <= 0)
            g_string_append_printf (tip, _("Discharging : %d%%"), capacity);
        else if (time < 90)
            g_string_append_printf (tip, _("Discharging : %d%%\nTime remaining : %d minutes"), capacity, time);
        else
            g_string_append_printf (tip, _("Discharging : %d%%\nTime remaining : %0.1f hours"), capacity, ftime);
        if (capacity <= 20) draw_icon (pt, capacity, 1, 0, 0, 0);
        else draw_icon (pt, capacity, 0, 0.85, 0, 0);
    }

    append_battery_details (pt->batt, tip);

    // set the tooltip
    gtk_widget_set_tooltip_text (pt->tray_icon, tip->str);
    g_string_free (tip, TRUE);
    gtk_widget_show_all (pt->plugin);
}

static gboolean timer_event (PtBattPlugin *pt)
{
    update_icon (pt);
    return TRUE;
}

/*----------------------------------------------------------------------------*/
/* wf-panel plugin functions                                                  */
/*----------------------------------------------------------------------------*/

/* Handler for system config changed message from panel */
void batt_update_display (PtBattPlugin *pt)
{
    update_icon (pt);
}

void batt_set_values (PtBattPlugin *pt)
{
    conf_table[0].value = (void *) &pt->batt_num;
}

/* Handler for battery number update from variable watcher */
void batt_set_num (PtBattPlugin *pt)
{
    if (pt->timer) g_source_remove (pt->timer);
    if (init_measurement (pt))
    {
        pt->timer = g_timeout_add (pt->simulate ? SIM_INTERVAL : INTERVAL, (GSourceFunc) timer_event, (gpointer) pt);
        update_icon (pt);
    }
    else pt->timer = 0;
}

void batt_init (PtBattPlugin *pt)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    /* Allocate icon as a child of top level */
    pt->tray_icon = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (pt->plugin), pt->tray_icon);

    /* Set up button */
    wrap_add_longpress (pt->gesture, pt->plugin, NULL, NULL);

    /* Load the symbols */
    pt->plug = gdk_pixbuf_new_from_file (PACKAGE_DATA_DIR "/images/plug.png", NULL);
    pt->flash = gdk_pixbuf_new_from_file (PACKAGE_DATA_DIR "/images/flash.png", NULL);

    if (getenv ("PLUGIN_SIMBAT")) pt->simulate = TRUE;
    else pt->simulate = FALSE;

    /* Start timed events to monitor status */
    batt_set_num (pt);
}

void batt_destructor (gpointer user_data)
{
    PtBattPlugin *pt = (PtBattPlugin *) user_data;

    wrap_free_gesture (pt->gesture);

    /* Disconnect the timer */
    if (pt->timer) g_source_remove (pt->timer);

    if (pt->batt) battery_free (pt->batt);
    if (pt->plug) g_object_unref (pt->plug);
    if (pt->flash) g_object_unref (pt->flash);

    g_free (pt);
}

/* End of file */
/*----------------------------------------------------------------------------*/
