/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * The GIMP Help plug-in
 * Copyright (C) 1999-2004 Sven Neumann <sven@gimp.org>
 *                         Michael Natterer <mitch@gimp.org>
 *                         Henrik Brix Andersen <brix@gimp.org>
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

#include <string.h>  /*  strlen  */

#include <glib.h>

#include "libgimp/gimp.h"

#include "domain.h"

#include "libgimp/stdplugins-intl.h"


/*  defines  */

#define GIMP_HELP_EXT_NAME        "extension_gimp_help"
#define GIMP_HELP_TEMP_EXT_NAME   "extension_gimp_help_temp"

#define GIMP_HELP_PREFIX          "help"
#define GIMP_HELP_ENV_URI         "GIMP2_HELP_URI"

#define GIMP_HELP_DEFAULT_LOCALE  "C"
#define GIMP_HELP_DEFAULT_ID      "gimp-main"


typedef struct
{
  gchar *procedure;
  gchar *help_domain;
  gchar *help_locale;
  gchar *help_id;
} IdleHelp;


/*  forward declarations  */

static void     query             (void);
static void     run               (const gchar      *name,
                                   gint              nparams,
                                   const GimpParam  *param,
                                   gint             *nreturn_vals,
                                   GimpParam       **return_vals);

static void     temp_proc_install (void);
static void     temp_proc_run     (const gchar      *name,
                                   gint              nparams,
                                   const GimpParam  *param,
                                   gint             *nreturn_vals,
                                   GimpParam       **return_vals);

static void     load_help         (const gchar      *procedure,
                                   const gchar      *help_domain,
                                   const gchar      *help_locale,
                                   const gchar      *help_id);
static gboolean load_help_idle    (gpointer          data);


/*  local variables  */

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};


MAIN ()

static void
query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,       "num_domain_names", "" },
    { GIMP_PDB_STRINGARRAY, "domain_names",     "" },
    { GIMP_PDB_INT32,       "num_domain_uris",  "" },
    { GIMP_PDB_STRINGARRAY, "domain_uris",      "" }
  };

  gimp_install_procedure (GIMP_HELP_EXT_NAME,
                          "", /* FIXME */
                          "", /* FIXME */
                          "Sven Neumann <sven@gimp.org>, "
			  "Michael Natterer <mitch@gimp.org>, "
                          "Henrik Brix Andersen <brix@gimp.org>",
                          "Sven Neumann, Michael Natterer & Henrik Brix Andersen",
                          "1999-2004",
                          NULL,
                          "",
                          GIMP_EXTENSION,
                          G_N_ELEMENTS (args), 0,
                          args, NULL);
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[1];
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  const gchar       *default_env_domain_uri;
  gchar             *default_domain_uri;

  INIT_I18N ();

  /*  set default values  */
  default_env_domain_uri = g_getenv (GIMP_HELP_ENV_URI);

  if (default_env_domain_uri)
    {
      default_domain_uri = g_strdup (default_env_domain_uri);
    }
  else
    {
      gchar *help_root = g_build_filename (gimp_data_directory (),
                                           GIMP_HELP_PREFIX,
                                           NULL);

      default_domain_uri = g_filename_to_uri (help_root, NULL, NULL);

      g_free (help_root);
    }

  /*  make sure all the arguments are there  */
  if (nparams == 4)
    {
      gint    num_domain_names = param[0].data.d_int32;
      gchar **domain_names     = param[1].data.d_stringarray;
      gint    num_domain_uris  = param[2].data.d_int32;
      gchar **domain_uris      = param[3].data.d_stringarray;

      if (num_domain_names == num_domain_uris)
        {
          gint i;

          domain_register (GIMP_HELP_DEFAULT_DOMAIN, default_domain_uri);

          for (i = 0; i < num_domain_names; i++)
            {
              domain_register (domain_names[i], domain_uris[i]);
            }
        }
      else
        {
          g_printerr ("help: number of names doesn't match number of URIs.\n");

          status = GIMP_PDB_CALLING_ERROR;
        }
    }
  else
    {
      g_printerr ("help: wrong number of arguments in procedure call.\n");

      status = GIMP_PDB_CALLING_ERROR;
    }

  g_free (default_domain_uri);

  if (status == GIMP_PDB_SUCCESS)
    {
      GMainLoop *loop;

      temp_proc_install ();

      gimp_extension_ack ();
      gimp_extension_enable ();

      loop = g_main_loop_new (NULL, FALSE);
      g_main_loop_run (loop);
      g_main_loop_unref (loop);
    }

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  *nreturn_vals = 1;
  *return_vals  = values;
}

static void
temp_proc_install (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_STRING, "procedure",   "The procedure of the browser to use" },
    { GIMP_PDB_STRING, "help_domain", "Help domain to use" },
    { GIMP_PDB_STRING, "help_locale", "Language to use"    },
    { GIMP_PDB_STRING, "help_id",     "Help ID to open"    }
  };

  gimp_install_temp_proc (GIMP_HELP_TEMP_EXT_NAME,
                          "DON'T USE THIS ONE",
                          "(Temporary procedure)",
			  "Sven Neumann <sven@gimp.org>, "
			  "Michael Natterer <mitch@gimp.org>"
                          "Henrik Brix Andersen <brix@gimp.org",
			  "Sven Neumann, Michael Natterer & Henrik Brix Andersen",
			  "1999-2004",
                          NULL,
                          "",
                          GIMP_TEMPORARY,
                          G_N_ELEMENTS (args), 0,
                          args, NULL,
                          temp_proc_run);
}

static void
temp_proc_run (const gchar      *name,
               gint              nparams,
               const GimpParam  *param,
               gint             *nreturn_vals,
               GimpParam       **return_vals)
{
  static GimpParam   values[1];
  GimpPDBStatusType  status      = GIMP_PDB_SUCCESS;
  const gchar       *procedure   = ""; /* FIXME */
  const gchar       *help_domain = GIMP_HELP_DEFAULT_DOMAIN;
  const gchar       *help_locale = GIMP_HELP_DEFAULT_LOCALE;
  const gchar       *help_id     = GIMP_HELP_DEFAULT_ID;

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  /*  make sure all the arguments are there  */
  if (nparams == 4)
    {
      if (param[0].data.d_string && strlen (param[0].data.d_string))
        procedure = param[0].data.d_string;

      if (param[1].data.d_string && strlen (param[1].data.d_string))
        help_domain = param[1].data.d_string;

      if (param[2].data.d_string && strlen (param[2].data.d_string))
        help_locale = param[2].data.d_string;

      if (param[3].data.d_string && strlen (param[3].data.d_string))
        help_id = param[3].data.d_string;
    }

  load_help (procedure, help_domain, help_locale, help_id);
}

static void
load_help (const gchar *procedure,
           const gchar *help_domain,
           const gchar *help_locale,
           const gchar *help_id)
{
  IdleHelp *idle_help;

  idle_help = g_new0 (IdleHelp, 1);

  idle_help->procedure   = g_strdup (procedure);
  idle_help->help_domain = g_strdup (help_domain);
  idle_help->help_locale = g_strdup (help_locale);
  idle_help->help_id     = g_strdup (help_id);

  g_idle_add (load_help_idle, idle_help);
}

static gboolean
load_help_idle (gpointer data)
{
  IdleHelp   *idle_help;
  HelpDomain *domain;
  gchar      *full_uri;

  idle_help = (IdleHelp *) data;

  g_printerr ("help: got a request for %s %s %s\n",
              idle_help->help_domain,
              idle_help->help_locale,
              idle_help->help_id);

  domain = domain_lookup (idle_help->help_domain);

  if (domain)
    {
      full_uri = domain_map (domain,
                             idle_help->help_locale,
                             idle_help->help_id);

      if (full_uri)
        {
          GimpParam *return_vals;
          gint       n_return_vals;

          g_printerr ("help: calling '%s' for '%s'\n",
                      idle_help->procedure, full_uri);

          return_vals = gimp_run_procedure (idle_help->procedure,
                                            &n_return_vals,
                                            GIMP_PDB_STRING, full_uri,
                                            GIMP_PDB_END);

          gimp_destroy_params (return_vals, n_return_vals);

          g_free (full_uri);
        }
    }

  g_free (idle_help->procedure);
  g_free (idle_help->help_domain);
  g_free (idle_help->help_locale);
  g_free (idle_help->help_id);
  g_free (idle_help);

  return FALSE;
}
