/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "siod/siod.h"

#include "script-fu-types.h"

#include "script-fu-interface.h"
#include "script-fu-scripts.h"

#include "script-fu-intl.h"


#define RESPONSE_RESET         1
#define RESPONSE_ABOUT         2

#define TEXT_WIDTH           100
#define COLOR_SAMPLE_WIDTH    60
#define COLOR_SAMPLE_HEIGHT   15
#define SLIDER_WIDTH          80


typedef struct
{
  GtkWidget     *dialog;

  GtkWidget     *table;
  GtkWidget    **widgets;

  GtkWidget     *progress_label;
  GtkWidget     *progress_bar;

  GtkWidget     *about_dialog;

  gchar         *short_title;
  gchar         *title;
  gchar         *help_id;
  gchar         *last_command;
  gint           command_count;
  gint           consec_command_count;
} SFInterface;


/*
 *  Local Functions
 */

static void   script_fu_interface_quit      (SFScript             *script);

static void   script_fu_response            (GtkWidget            *widget,
                                             gint                  response_id,
                                             SFScript             *script);
static void   script_fu_ok                  (SFScript             *script);
static void   script_fu_reset               (SFScript             *script);
static void   script_fu_about               (SFScript             *script);

static void   script_fu_file_callback       (GtkWidget            *widget,
                                             SFFilename           *file);
static void   script_fu_combo_callback      (GtkWidget            *widget,
                                             SFOption             *option);
static void   script_fu_pattern_callback    (const gchar          *name,
                                             gint                  width,
                                             gint                  height,
                                             gint                  bytes,
                                             const guchar         *mask_data,
                                             gboolean              closing,
                                             gpointer              data);
static void   script_fu_gradient_callback   (const gchar          *name,
                                             gint                  width,
                                             const gdouble        *mask_data,
                                             gboolean              closing,
                                             gpointer              data);
static void   script_fu_font_callback       (gpointer              data,
                                             const gchar          *name,
                                             gboolean              closing);
static void   script_fu_palette_callback    (const gchar          *name,
                                             gboolean              closing,
                                             gpointer              data);
static void   script_fu_brush_callback      (const gchar          *name,
                                             gdouble               opacity,
                                             gint                  spacing,
                                             GimpLayerModeEffects  paint_mode,
                                             gint                  width,
                                             gint                  height,
                                             const guchar         *mask_data,
                                             gboolean              closing,
                                             gpointer              data);


/*
 *  Local variables
 */

static SFInterface *sf_interface = NULL;  /*  there can only be at most one
					      interactive interface  */


/*
 *  Function definitions
 */

void
script_fu_interface_report_cc (gchar *command)
{
  if (sf_interface == NULL)
    return;

  if (sf_interface->last_command &&
      strcmp (sf_interface->last_command, command) == 0)
    {
      gchar *new_command;

      sf_interface->command_count++;

      new_command = g_strdup_printf ("%s <%d>",
				     command, sf_interface->command_count);
      gtk_label_set_text (GTK_LABEL (sf_interface->progress_label), new_command);
      g_free (new_command);
    }
  else
    {
      sf_interface->command_count = 1;
      gtk_label_set_text (GTK_LABEL (sf_interface->progress_label), command);
      g_free (sf_interface->last_command);
      sf_interface->last_command = g_strdup (command);
    }

  while (gtk_main_iteration ());
}

void
script_fu_interface (SFScript *script)
{
  GtkWidget    *dlg;
  GtkWidget    *menu;
  GtkWidget    *vbox;
  GtkWidget    *vbox2;
  GtkSizeGroup *group;
  GSList       *list;
  gchar        *tmp;
  gint          i;

  static gboolean gtk_initted = FALSE;

  /*  Simply return if there is already an interface. This is an
      ugly workaround for the fact that we can not process two
      scripts at a time.  */
  if (sf_interface != NULL)
    {
      gchar *message =
        g_strdup_printf ("%s\n\n%s",
                         _("Script-Fu cannot process two scripts "
                           "at the same time."),
                         _("You are already running the \"%s\" script."));

      g_message (message, sf_interface->short_title);
      g_free (message);

      return;
    }

  g_return_if_fail (script != NULL);

  if (!gtk_initted)
    {
      INIT_I18N();

      gimp_ui_init ("script-fu", TRUE);

      gtk_initted = TRUE;
    }

  sf_interface = g_new0 (SFInterface, 1);
  sf_interface->widgets = g_new0 (GtkWidget *, script->num_args);

  /* strip the first part of the menupath if it contains _("/Script-Fu/") */
  tmp = strstr (gettext (script->menu_path), _("/Script-Fu/"));
  if (tmp)
    sf_interface->short_title = g_strdup (tmp + strlen (_("/Script-Fu/")));
  else
    sf_interface->short_title = g_strdup (gettext (script->menu_path));

  /* strip mnemonics from the menupath */
  tmp = gimp_strip_uline (sf_interface->short_title);
  g_free (sf_interface->short_title);
  sf_interface->short_title = tmp;

  tmp = strstr (sf_interface->short_title, "...");
  if (tmp)
    *tmp = '\0';

  sf_interface->title = g_strdup_printf (_("Script-Fu: %s"),
                                         sf_interface->short_title);

  sf_interface->help_id = g_strdup (script->script_name);

  sf_interface->dialog = dlg =
    gimp_dialog_new (sf_interface->title, "script-fu",
                     NULL, 0,
                     gimp_standard_help_func, sf_interface->help_id,

                     GTK_STOCK_ABOUT,  GTK_RESPONSE_HELP,
                     GIMP_STOCK_RESET, RESPONSE_RESET,
                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                     GTK_STOCK_OK,     GTK_RESPONSE_OK,

                     NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (sf_interface->dialog),
                                           GTK_RESPONSE_HELP,
                                           RESPONSE_RESET,
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  g_signal_connect (dlg, "response",
                    G_CALLBACK (script_fu_response),
                    script);

  g_signal_connect_swapped (dlg, "destroy",
                            G_CALLBACK (script_fu_interface_quit),
                            script);

  gtk_window_set_resizable (GTK_WINDOW (dlg), TRUE);

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  /*  The argument table  */
  if (script->image_based)
    sf_interface->table = gtk_table_new (script->num_args - 1, 3, FALSE);
  else
    sf_interface->table = gtk_table_new (script->num_args + 1, 3, FALSE);

  gtk_table_set_col_spacings (GTK_TABLE (sf_interface->table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (sf_interface->table), 6);
  gtk_box_pack_start (GTK_BOX (vbox), sf_interface->table, FALSE, FALSE, 0);
  gtk_widget_show (sf_interface->table);

  group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  for (i = script->image_based ? 2 : 0; i < script->num_args; i++)
    {
      GtkWidget *widget       = NULL;
      GtkObject *adj;
      gchar     *label_text;
      gfloat     label_yalign = 0.5;
      gint      *ID_ptr       = NULL;
      gint       row          = i;
      gboolean   left_align     = FALSE;

      if (script->image_based)
        row -= 2;

      /*  we add a colon after the label;
          some languages want an extra space here  */
      label_text =  g_strdup_printf (_("%s:"),
                                     gettext (script->arg_labels[i]));

      switch (script->arg_types[i])
	{
	case SF_IMAGE:
	case SF_DRAWABLE:
	case SF_LAYER:
	case SF_CHANNEL:
	  switch (script->arg_types[i])
	    {
	    case SF_IMAGE:
              widget = gimp_image_combo_box_new (NULL, NULL);
              ID_ptr = &script->arg_values[i].sfa_image;
	      break;

	    case SF_DRAWABLE:
              widget = gimp_drawable_combo_box_new (NULL, NULL);
              ID_ptr = &script->arg_values[i].sfa_drawable;
              break;

	    case SF_LAYER:
              widget = gimp_layer_combo_box_new (NULL, NULL);
              ID_ptr = &script->arg_values[i].sfa_layer;
	      break;

	    case SF_CHANNEL:
              widget = gimp_channel_combo_box_new (NULL, NULL);
              ID_ptr = &script->arg_values[i].sfa_channel;
	      break;

	    default:
	      menu = NULL;
	      break;
	    }

          gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (widget), *ID_ptr,
                                      G_CALLBACK (gimp_int_combo_box_get_active),
                                      ID_ptr);
	  break;

	case SF_COLOR:
          left_align = TRUE;
	  widget = gimp_color_button_new (_("Script-Fu Color Selection"),
                                          COLOR_SAMPLE_WIDTH,
                                          COLOR_SAMPLE_HEIGHT,
                                          &script->arg_values[i].sfa_color,
                                          GIMP_COLOR_AREA_FLAT);

          gimp_color_button_set_update (GIMP_COLOR_BUTTON (widget), TRUE);

	  g_signal_connect (widget, "color-changed",
			    G_CALLBACK (gimp_color_button_get_color),
			    &script->arg_values[i].sfa_color);
	  break;

	case SF_TOGGLE:
	  g_free (label_text);
	  label_text = NULL;
	  widget =
            gtk_check_button_new_with_mnemonic (gettext (script->arg_labels[i]));
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				       script->arg_values[i].sfa_toggle);

	  g_signal_connect (widget, "toggled",
			    G_CALLBACK (gimp_toggle_button_update),
			    &script->arg_values[i].sfa_toggle);
	  break;

	case SF_VALUE:
	case SF_STRING:
	  widget = gtk_entry_new ();
	  gtk_widget_set_size_request (widget, TEXT_WIDTH, -1);
          gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);

	  gtk_entry_set_text (GTK_ENTRY (widget),
                              script->arg_values[i].sfa_value);
	  break;

        case SF_TEXT:
          {
            GtkWidget     *view;
            GtkTextBuffer *buffer;

            widget = gtk_scrolled_window_new (NULL, NULL);
            gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget),
                                                 GTK_SHADOW_IN);
            gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
                                            GTK_POLICY_AUTOMATIC,
                                            GTK_POLICY_AUTOMATIC);
            gtk_widget_set_size_request (widget, TEXT_WIDTH, -1);

            view = gtk_text_view_new ();
            gtk_container_add (GTK_CONTAINER (widget), view);
            gtk_widget_show (view);

            buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
            gtk_text_view_set_editable (GTK_TEXT_VIEW (view), TRUE);

            gtk_text_buffer_set_text (buffer,
                                      script->arg_values[i].sfa_value, -1);

            label_yalign = 0.0;
          }
          break;

	case SF_ADJUSTMENT:
	  switch (script->arg_defaults[i].sfa_adjustment.type)
	    {
	    case SF_SLIDER:
	      script->arg_values[i].sfa_adjustment.adj = (GtkAdjustment *)
		gimp_scale_entry_new (GTK_TABLE (sf_interface->table),
                                      0, row,
				      label_text, SLIDER_WIDTH, -1,
				      script->arg_values[i].sfa_adjustment.value,
				      script->arg_defaults[i].sfa_adjustment.lower,
				      script->arg_defaults[i].sfa_adjustment.upper,
				      script->arg_defaults[i].sfa_adjustment.step,
				      script->arg_defaults[i].sfa_adjustment.page,
				      script->arg_defaults[i].sfa_adjustment.digits,
				      TRUE, 0.0, 0.0,
				      NULL, NULL);
              gtk_entry_set_activates_default (GIMP_SCALE_ENTRY_SPINBUTTON (script->arg_values[i].sfa_adjustment.adj), TRUE);
	      break;

	    case SF_SPINNER:
              left_align = TRUE;
              widget =
                gimp_spin_button_new (&adj,
                                      script->arg_values[i].sfa_adjustment.value,
                                      script->arg_defaults[i].sfa_adjustment.lower,
                                      script->arg_defaults[i].sfa_adjustment.upper,
                                      script->arg_defaults[i].sfa_adjustment.step,
                                      script->arg_defaults[i].sfa_adjustment.page,
                                      0, 0,
                                      script->arg_defaults[i].sfa_adjustment.digits);
              gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
              script->arg_values[i].sfa_adjustment.adj = GTK_ADJUSTMENT (adj);
	      break;
	    }

          g_signal_connect (script->arg_values[i].sfa_adjustment.adj,
                            "value-changed",
                            G_CALLBACK (gimp_double_adjustment_update),
                            &script->arg_values[i].sfa_adjustment.value);
	  break;

	case SF_FILENAME:
	case SF_DIRNAME:
          if (script->arg_types[i] == SF_FILENAME)
            widget = gtk_file_chooser_button_new (_("Script-Fu File Selection"),
                                                  GTK_FILE_CHOOSER_ACTION_OPEN);
          else
            widget = gtk_file_chooser_button_new (_("Script-Fu Folder Selection"),
                                                  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
          if (script->arg_values[i].sfa_file.filename)
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget),
                                           script->arg_values[i].sfa_file.filename);

	  g_signal_connect (widget, "selection-changed",
                            G_CALLBACK (script_fu_file_callback),
                            &script->arg_values[i].sfa_file);
	  break;

	case SF_FONT:
	  widget = gimp_font_select_button_new (_("Script-Fu Font Selection"),
                                                script->arg_values[i].sfa_font);
          g_signal_connect_swapped (widget, "font-set",
                                    G_CALLBACK (script_fu_font_callback),
                                    &script->arg_values[i].sfa_font);
	  break;

	case SF_PALETTE:
	  widget = gimp_palette_select_widget_new (_("Script-Fu Palette Selection"),
                                                   script->arg_values[i].sfa_palette,
                                                   script_fu_palette_callback,
                                                   &script->arg_values[i].sfa_palette);
	  break;

	case SF_PATTERN:
          left_align = TRUE;
	  widget = gimp_pattern_select_widget_new (_("Script-fu Pattern Selection"),
                                                   script->arg_values[i].sfa_pattern,
                                                   script_fu_pattern_callback,
                                                   &script->arg_values[i].sfa_pattern);
	  break;
	case SF_GRADIENT:
          left_align = TRUE;
	  widget = gimp_gradient_select_widget_new (_("Script-Fu Gradient Selection"),
                                                    script->arg_values[i].sfa_gradient,
                                                    script_fu_gradient_callback,
                                                    &script->arg_values[i].sfa_gradient);
	  break;

	case SF_BRUSH:
          left_align = TRUE;
	  widget = gimp_brush_select_widget_new (_("Script-Fu Brush Selection"),
                                                 script->arg_values[i].sfa_brush.name,
                                                 script->arg_values[i].sfa_brush.opacity,
                                                 script->arg_values[i].sfa_brush.spacing,
                                                 script->arg_values[i].sfa_brush.paint_mode,
                                                 script_fu_brush_callback,
                                                 &script->arg_values[i].sfa_brush);
	  break;

	case SF_OPTION:
	  widget = gtk_combo_box_new_text ();
	  for (list = script->arg_defaults[i].sfa_option.list;
	       list;
	       list = g_slist_next (list))
	    {
              gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
                                         gettext ((const gchar *) list->data));
	    }

          gtk_combo_box_set_active (GTK_COMBO_BOX (widget),
                                    script->arg_values[i].sfa_option.history);

          g_signal_connect (widget, "changed",
                            G_CALLBACK (script_fu_combo_callback),
                            &script->arg_values[i].sfa_option);
	  break;

	case SF_ENUM:
	  widget = gimp_enum_combo_box_new (g_type_from_name (script->arg_defaults[i].sfa_enum.type_name));

          gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (widget),
                                         script->arg_values[i].sfa_enum.history);

          g_signal_connect (widget, "changed",
                            G_CALLBACK (gimp_int_combo_box_get_active),
                            &script->arg_values[i].sfa_enum.history);
          break;
	}

      if (widget)
        {
          if (label_text)
            {
              gimp_table_attach_aligned (GTK_TABLE (sf_interface->table),
                                         0, row,
                                         label_text, 0.0, label_yalign,
                                         widget, 2, left_align);
              g_free (label_text);
            }
          else
            {
              gtk_table_attach (GTK_TABLE (sf_interface->table),
                                widget, 0, 3, row, row + 1,
                                GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
              gtk_widget_show (widget);
            }

          if (left_align)
            gtk_size_group_add_widget (group, widget);
        }

      sf_interface->widgets[i] = widget;
    }

  g_object_unref (group);

  /* the script progress bar */
  vbox2 = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_end (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);
  gtk_widget_show (vbox2);

  sf_interface->progress_bar = gimp_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox2), sf_interface->progress_bar,
                      FALSE, FALSE, 0);
  gtk_widget_show (sf_interface->progress_bar);

  sf_interface->progress_label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (sf_interface->progress_label), 0.0, 0.5);
  gimp_label_set_attributes (GTK_LABEL (sf_interface->progress_label),
                             PANGO_ATTR_STYLE, PANGO_STYLE_ITALIC,
                             -1);
  gtk_box_pack_start (GTK_BOX (vbox2), sf_interface->progress_label,
                      FALSE, FALSE, 0);
  gtk_widget_show (sf_interface->progress_label);

  gtk_widget_show (dlg);

  gtk_main ();
}

static void
script_fu_interface_quit (SFScript *script)
{
  gint i;

  g_return_if_fail (script != NULL);
  g_return_if_fail (sf_interface != NULL);

  g_free (sf_interface->short_title);
  g_free (sf_interface->title);
  g_free (sf_interface->help_id);

  if (sf_interface->about_dialog)
    gtk_widget_destroy (sf_interface->about_dialog);

  for (i = 0; i < script->num_args; i++)
    switch (script->arg_types[i])
      {
      case SF_FONT:
  	gimp_font_select_button_close_popup
          (GIMP_FONT_SELECT_BUTTON (sf_interface->widgets[i]));
	break;

      case SF_PALETTE:
  	gimp_palette_select_widget_close (sf_interface->widgets[i]);
	break;

      case SF_PATTERN:
  	gimp_pattern_select_widget_close (sf_interface->widgets[i]);
	break;

      case SF_GRADIENT:
  	gimp_gradient_select_widget_close (sf_interface->widgets[i]);
	break;

      case SF_BRUSH:
  	gimp_brush_select_widget_close (sf_interface->widgets[i]);
	break;

      default:
	break;
      }

  g_free (sf_interface->widgets);
  g_free (sf_interface->last_command);

  g_free (sf_interface);
  sf_interface = NULL;

  /*
   *  We do not call gtk_main_quit() earlier to reduce the possibility
   *  that script_fu_script_proc() is called from gimp_extension_process()
   *  while we are not finished with the current script. This sucks!
   */

  gtk_main_quit ();
}

static void
script_fu_file_callback (GtkWidget  *widget,
                         SFFilename *file)
{
  if (file->filename)
    g_free (file->filename);

  file->filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
}

static void
script_fu_combo_callback (GtkWidget *widget,
                          SFOption  *option)
{
  option->history = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
}

static void
script_fu_string_update (gchar       **dest,
                         const gchar  *src)
{
  if (*dest)
    g_free (*dest);

  *dest = g_strdup (src);
}

static void
script_fu_pattern_callback (const gchar  *name,
                            gint          width,
                            gint          height,
                            gint          bytes,
                            const guchar *mask_data,
                            gboolean      closing,
                            gpointer      data)
{
  script_fu_string_update (data, name);
}

static void
script_fu_gradient_callback (const gchar   *name,
                             gint           width,
                             const gdouble *mask_data,
                             gboolean       closing,
                             gpointer       data)
{
  script_fu_string_update (data, name);
}

static void
script_fu_font_callback (gpointer     data,
                         const gchar *name,
                         gboolean     closing)
{
  script_fu_string_update (data, name);
}

static void
script_fu_palette_callback (const gchar *name,
                            gboolean     closing,
                            gpointer     data)
{
  script_fu_string_update (data, name);
}

static void
script_fu_brush_callback (const gchar          *name,
                          gdouble               opacity,
                          gint                  spacing,
                          GimpLayerModeEffects  paint_mode,
                          gint                  width,
                          gint                  height,
                          const guchar         *mask_data,
                          gboolean              closing,
                          gpointer              data)
{
  SFBrush *brush = data;

  g_free (brush->name);

  brush->name       = g_strdup (name);
  brush->opacity    = opacity;
  brush->spacing    = spacing;
  brush->paint_mode = paint_mode;
}

static void
script_fu_response (GtkWidget *widget,
                    gint       response_id,
                    SFScript  *script)
{
  if (! GTK_WIDGET_SENSITIVE (GTK_DIALOG (sf_interface->dialog)->action_area))
    return;

  switch (response_id)
    {
    case GTK_RESPONSE_HELP:
      script_fu_about (script);
      break;

    case RESPONSE_RESET:
      script_fu_reset (script);
      break;

    case GTK_RESPONSE_OK:
      gtk_widget_set_sensitive (sf_interface->table, FALSE);
      gtk_widget_set_sensitive (GTK_DIALOG (sf_interface->dialog)->action_area,
                                FALSE);

      script_fu_ok (script);
      /* fallthru */

    default:
      gtk_widget_destroy (sf_interface->dialog);
      break;
    }
}

static void
script_fu_ok (SFScript *script)
{
  gchar   *escaped;
  GString *s;
  gchar   *command;
  gchar    buffer[G_ASCII_DTOSTR_BUF_SIZE];
  gint     i;

  s = g_string_new ("(");
  g_string_append (s, script->script_name);

  for (i = 0; i < script->num_args; i++)
    {
      SFArgValue *arg_value = &script->arg_values[i];
      GtkWidget  *widget    = sf_interface->widgets[i];

      g_string_append_c (s, ' ');

      switch (script->arg_types[i])
        {
        case SF_IMAGE:
        case SF_DRAWABLE:
        case SF_LAYER:
        case SF_CHANNEL:
          g_string_append_printf (s, "%d", arg_value->sfa_image);
          break;

        case SF_COLOR:
          {
            guchar r, g, b;

            gimp_rgb_get_uchar (&arg_value->sfa_color, &r, &g, &b);
            g_string_append_printf (s, "'(%d %d %d)",
                                    (gint) r, (gint) g, (gint) b);
          }
          break;

        case SF_TOGGLE:
          g_string_append (s, arg_value->sfa_toggle ? "TRUE" : "FALSE");
          break;

        case SF_VALUE:
          g_free (arg_value->sfa_value);
          arg_value->sfa_value =
            g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));

          g_string_append (s, arg_value->sfa_value);
          break;

        case SF_STRING:
          g_free (arg_value->sfa_value);
          arg_value->sfa_value =
            g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));

          escaped = g_strescape (arg_value->sfa_value, NULL);
          g_string_append_printf (s, "\"%s\"", escaped);
          g_free (escaped);
          break;

        case SF_TEXT:
          {
            GtkWidget     *view;
            GtkTextBuffer *buffer;
            GtkTextIter    start, end;

            view = gtk_bin_get_child (GTK_BIN (widget));
            buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

            gtk_text_buffer_get_start_iter (buffer, &start);
            gtk_text_buffer_get_end_iter (buffer, &end);

            g_free (arg_value->sfa_value);
            arg_value->sfa_value = gtk_text_buffer_get_text (buffer,
                                                             &start, &end,
                                                             FALSE);

            escaped = g_strescape (arg_value->sfa_value, NULL);
            g_string_append_printf (s, "\"%s\"", escaped);
            g_free (escaped);
          }
          break;

        case SF_ADJUSTMENT:
          g_ascii_dtostr (buffer, sizeof (buffer),
                          arg_value->sfa_adjustment.value);
          g_string_append (s, buffer);
          break;

        case SF_FILENAME:
        case SF_DIRNAME:
          escaped = g_strescape (arg_value->sfa_file.filename, NULL);
          g_string_append_printf (s, "\"%s\"", escaped);
          g_free (escaped);
          break;

        case SF_FONT:
          g_string_append_printf (s, "\"%s\"", arg_value->sfa_font);
          break;

        case SF_PALETTE:
          g_string_append_printf (s, "\"%s\"", arg_value->sfa_palette);
          break;

        case SF_PATTERN:
          g_string_append_printf (s, "\"%s\"", arg_value->sfa_pattern);
          break;

        case SF_GRADIENT:
          g_string_append_printf (s, "\"%s\"", arg_value->sfa_gradient);
          break;

        case SF_BRUSH:
          g_ascii_dtostr (buffer, sizeof (buffer),
                          arg_value->sfa_brush.opacity);

          g_string_append_printf (s, "'(\"%s\" %s %d %d)",
                                  arg_value->sfa_brush.name,
                                  buffer,
                                  arg_value->sfa_brush.spacing,
                                  arg_value->sfa_brush.paint_mode);
          break;

        case SF_OPTION:
          g_string_append_printf (s, "%d", arg_value->sfa_option.history);
          break;

        case SF_ENUM:
          g_string_append_printf (s, "%d", arg_value->sfa_enum.history);
          break;
        }
    }

  g_string_append_c (s, ')');

  command = g_string_free (s, FALSE);

  /*  run the command through the interpreter  */
  if (repl_c_string (command, 0, 0, 1) != 0)
    script_fu_error_msg (command);

  g_free (command);
}

static void
script_fu_reset (SFScript *script)
{
  gint i;

  for (i = 0; i < script->num_args; i++)
    {
      GtkWidget *widget = sf_interface->widgets[i];

      switch (script->arg_types[i])
        {
        case SF_IMAGE:
        case SF_DRAWABLE:
        case SF_LAYER:
        case SF_CHANNEL:
          break;

        case SF_COLOR:
          gimp_color_button_set_color (GIMP_COLOR_BUTTON (widget),
                                       &script->arg_defaults[i].sfa_color);
	break;

        case SF_TOGGLE:
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
                                        script->arg_defaults[i].sfa_toggle);
	break;

        case SF_VALUE:
        case SF_STRING:
          gtk_entry_set_text (GTK_ENTRY (widget),
                              script->arg_defaults[i].sfa_value);
          break;

        case SF_TEXT:
          {
            GtkWidget     *view;
            GtkTextBuffer *buffer;

            g_free (script->arg_values[i].sfa_value);
            script->arg_values[i].sfa_value =
              g_strdup (script->arg_defaults[i].sfa_value);

            view = gtk_bin_get_child (GTK_BIN (widget));
            buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

            gtk_text_buffer_set_text (buffer,
                                      script->arg_values[i].sfa_value, -1);
          }
          break;

        case SF_ADJUSTMENT:
          gtk_adjustment_set_value (script->arg_values[i].sfa_adjustment.adj,
                                    script->arg_defaults[i].sfa_adjustment.value);
          break;

        case SF_FILENAME:
        case SF_DIRNAME:
          gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget),
                                         script->arg_defaults[i].sfa_file.filename);
          break;

        case SF_FONT:
          gimp_font_select_button_set_font_name
            (GIMP_FONT_SELECT_BUTTON (widget),
             script->arg_defaults[i].sfa_font);
          break;

        case SF_PALETTE:
          gimp_palette_select_widget_set (widget,
                                          script->arg_defaults[i].sfa_palette);
          break;

        case SF_PATTERN:
          gimp_pattern_select_widget_set (widget,
                                          script->arg_defaults[i].sfa_pattern);
          break;

        case SF_GRADIENT:
          gimp_gradient_select_widget_set (widget,
                                           script->arg_defaults[i].sfa_gradient);
          break;

        case SF_BRUSH:
          gimp_brush_select_widget_set (widget,
                                        script->arg_defaults[i].sfa_brush.name,
                                        script->arg_defaults[i].sfa_brush.opacity,
                                        script->arg_defaults[i].sfa_brush.spacing,
                                        script->arg_defaults[i].sfa_brush.paint_mode);
          break;

        case SF_OPTION:
          gtk_combo_box_set_active (GTK_COMBO_BOX (widget),
                                    script->arg_defaults[i].sfa_option.history);
          break;

        case SF_ENUM:
          gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (widget),
                                         script->arg_defaults[i].sfa_enum.history);
          break;
        }
    }
}

static void
script_fu_about (SFScript *script)
{
  GtkWidget     *dialog = sf_interface->about_dialog;
  GtkWidget     *vbox;
  GtkWidget     *label;
  GtkWidget     *scrolled_window;
  GtkWidget     *table;
  GtkWidget     *text_view;
  GtkTextBuffer *text_buffer;

  if (! dialog)
    {
      gchar *title = g_strdup_printf (_("About %s"), sf_interface->title);

      sf_interface->about_dialog = dialog =
        gimp_dialog_new (title, "script-fu-about",
                         sf_interface->dialog, 0,
                         gimp_standard_help_func, sf_interface->help_id,

                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,

                         NULL);
      g_free (title);

      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);

      g_signal_connect (dialog, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&sf_interface->about_dialog);

      vbox = gtk_vbox_new (FALSE, 12);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox,
			  TRUE, TRUE, 0);
      gtk_widget_show (vbox);

      /* the name */
      label = gtk_label_new (script->script_name);
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      gtk_widget_show (label);

      /* the help display */
      scrolled_window = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
      gtk_widget_show (scrolled_window);

      text_buffer = gtk_text_buffer_new (NULL);
      text_view = gtk_text_view_new_with_buffer (text_buffer);
      g_object_unref (text_buffer);

      gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), GTK_WRAP_WORD);
      gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text_view), 3);
      gtk_text_view_set_right_margin (GTK_TEXT_VIEW (text_view), 3);
      gtk_widget_set_size_request (text_view, 240, 120);
      gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);
      gtk_widget_show (text_view);

      gtk_text_buffer_set_text (text_buffer, script->help, -1);

      /* author, copyright, etc. */
      table = gtk_table_new (2, 4, FALSE);
      gtk_table_set_col_spacings (GTK_TABLE (table), 6);
      gtk_table_set_row_spacings (GTK_TABLE (table), 6);
      gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
      gtk_widget_show (table);

      label = gtk_label_new (script->author);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
      gimp_table_attach_aligned (GTK_TABLE (table), 0, 0,
				 _("Author:"), 0.0, 0.0,
				 label, 1, FALSE);

      label = gtk_label_new (script->copyright);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
      gimp_table_attach_aligned (GTK_TABLE (table), 0, 1,
				 _("Copyright:"), 0.0, 0.0,
				 label, 1, FALSE);

      label = gtk_label_new (script->date);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
      gimp_table_attach_aligned (GTK_TABLE (table), 0, 2,
				 _("Date:"), 0.0, 0.0,
				 label, 1, FALSE);

      if (strlen (script->img_types) > 0)
	{
	  label = gtk_label_new (script->img_types);
          gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	  gimp_table_attach_aligned (GTK_TABLE (table), 0, 3,
				     _("Image Types:"), 0.0, 0.0,
				     label, 1, FALSE);
	}
    }

  gtk_window_present (GTK_WINDOW (dialog));

  /*  move focus from the text view to the Close button  */
  gtk_widget_child_focus (dialog, GTK_DIR_TAB_FORWARD);
}
