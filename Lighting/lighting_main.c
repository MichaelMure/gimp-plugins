/* Lighting Effects 0.2.2 -- image filter plug-in for The Gimp program
 *
 * Copyright (C) 1996-98 Tom Bech
 * Copyright (C) 1996-98 Federico Mena Quintero
 *
 * E-mail: tomb@gimp.org (Tom) or quartic@gimp.org (Federico)
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

#include <libgimp/gimp.h>

#include <gck/gck.h>

#include "lighting_apply.h"
#include "lighting_image.h"
#include "lighting_main.h"
#include "lighting_preview.h"
#include "lighting_shade.h"
#include "lighting_ui.h"

#include "libgimp/stdplugins-intl.h"

LightingValues mapvals;

/******************/
/* Implementation */
/******************/

/*************************************/
/* Set parameters to standard values */
/*************************************/

static void
set_default_settings (void)
{
  gimp_vector3_set (&mapvals.viewpoint,   0.5, 0.5, 0.25);
  gimp_vector3_set (&mapvals.planenormal, 0.0, 0.0, 1.0);
  
  gimp_vector3_set (&mapvals.lightsource.position,   1.0,  0.0, 1.0);
  gimp_vector3_set (&mapvals.lightsource.direction, -1.0, -1.0, 1.0);

  gimp_rgba_set (&mapvals.lightsource.color, 1.0, 1.0, 1.0, 1.0);
  mapvals.lightsource.intensity = 1.0;
  mapvals.lightsource.type      = POINT_LIGHT;

  mapvals.material.ambient_int  =  0.3;
  mapvals.material.diffuse_int  =  1.0;
  mapvals.material.diffuse_ref  =  0.4;
  mapvals.material.specular_ref =  0.6;
  mapvals.material.highlight    = 27.0;

  mapvals.pixel_treshold      = 0.25;
  mapvals.max_depth           =  3.0;
  mapvals.preview_zoom_factor =  1.0;

  mapvals.bumpmaptype = 0;
  mapvals.bumpmin     = 0.0;
  mapvals.bumpmax     = 0.1;

  mapvals.antialiasing           = FALSE;
  mapvals.create_new_image       = FALSE;
  mapvals.transparent_background = FALSE;
  mapvals.bump_mapped            = FALSE;
  mapvals.env_mapped             = FALSE;
  mapvals.ref_mapped             = FALSE;
  mapvals.previewquality         = FALSE;
  mapvals.interactive_preview    = TRUE;

  mapvals.bumpmap_id = -1;
  mapvals.envmap_id  = -1;
}

static void
check_drawables (void)
{
  if (mapvals.bumpmap_id != -1 &&
      gimp_drawable_image_id (mapvals.bumpmap_id) == -1)
    {
      mapvals.bump_mapped = FALSE;
      mapvals.bumpmap_id  = -1;
    }

  if (mapvals.envmap_id != -1 &&
      gimp_drawable_image_id (mapvals.envmap_id) == -1)
    {
      mapvals.env_mapped = FALSE;
      mapvals.envmap_id  = -1;
    }

  if (mapvals.bump_mapped)
    {
      if (!gimp_drawable_is_gray (mapvals.bumpmap_id) || 
	  (gimp_drawable_width (mapvals.drawable_id) !=
	   gimp_drawable_width (mapvals.bumpmap_id)) ||
	  (gimp_drawable_height (mapvals.drawable_id) !=
	   gimp_drawable_height (mapvals.bumpmap_id)))
        {
          mapvals.bump_mapped = FALSE;
          mapvals.bumpmap_id  = -1;
        }
    }

  if (mapvals.env_mapped)
    {
      if (gimp_drawable_is_gray (mapvals.envmap_id) || 
          gimp_drawable_has_alpha (mapvals.envmap_id))
        {
          mapvals.bump_mapped = FALSE;
          mapvals.bumpmap_id  = -1;
        }
    }
}

static void
query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,    "run_mode",              "Interactive (0), non-interactive (1)" },
    { GIMP_PDB_IMAGE,    "image",                 "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable",              "Input drawable" },
    { GIMP_PDB_DRAWABLE, "bumpdrawable",          "Bumpmap drawable (set to 0 if disabled)" },
    { GIMP_PDB_DRAWABLE, "envdrawable",           "Environmentmap drawable (set to 0 if disabled)" },
    { GIMP_PDB_INT32,    "dobumpmap",             "Enable bumpmapping (TRUE/FALSE)" },
    { GIMP_PDB_INT32,    "doenvmap",              "Enable envmapping (TRUE/FALSE)" },
    { GIMP_PDB_INT32,    "bumpmaptype",           "Type of mapping (0=linear,1=log, 2=sinusoidal, 3=spherical)" },
    { GIMP_PDB_INT32,    "lighttype",             "Type of lightsource (0=point,1=directional,3=spot,4=none)" },
    { GIMP_PDB_COLOR,    "lightcolor",            "Lightsource color (r,g,b)" },
    { GIMP_PDB_FLOAT,    "lightposition_x",       "Lightsource position (x,y,z)" },
    { GIMP_PDB_FLOAT,    "lightposition_y",       "Lightsource position (x,y,z)" },
    { GIMP_PDB_FLOAT,    "lightposition_z",       "Lightsource position (x,y,z)" },
    { GIMP_PDB_FLOAT,    "lightdirection_x",      "Lightsource direction [x,y,z]" },
    { GIMP_PDB_FLOAT,    "lightdirection_y",      "Lightsource direction [x,y,z]" },
    { GIMP_PDB_FLOAT,    "lightdirection_z",      "Lightsource direction [x,y,z]" },
    { GIMP_PDB_FLOAT,    "ambient_intensity",     "Material ambient intensity (0..1)" },
    { GIMP_PDB_FLOAT,    "diffuse_intensity",     "Material diffuse intensity (0..1)" },
    { GIMP_PDB_FLOAT,    "diffuse_reflectivity",  "Material diffuse reflectivity (0..1)" },
    { GIMP_PDB_FLOAT,    "specular_reflectivity", "Material specular reflectivity (0..1)" },
    { GIMP_PDB_FLOAT,    "highlight",             "Material highlight (0..->), note: it's expotential" },
    { GIMP_PDB_INT32,    "antialiasing",          "Apply antialiasing (TRUE/FALSE)" },
    { GIMP_PDB_INT32,    "newimage",              "Create a new image (TRUE/FALSE)" },
    { GIMP_PDB_INT32,    "transparentbackground", "Make background transparent (TRUE/FALSE)" }
  };

  gimp_install_procedure ("plug_in_lighting",
			  "Apply various lighting effects to an image",
			  "No help yet",
			  "Tom Bech & Federico Mena Quintero",
			  "Tom Bech & Federico Mena Quintero",
			  "Version 0.2.0, March 15 1998",
			  N_("<Image>/Filters/Light Effects/Lighting Effects..."),
			  "RGB*",
			  GIMP_PLUGIN,
			  G_N_ELEMENTS (args), 0,
			  args, NULL);
}

static void
run (gchar      *name,
     gint        nparams,
     GimpParam  *param,
     gint       *nreturn_vals,
     GimpParam **return_vals)
{
  static GimpParam   values[1];
  GimpDrawable      *drawable;
  GimpRunMode    run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;

  run_mode = param[0].data.d_int32;

  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      INIT_I18N_UI ();
    }
  else
    {
      INIT_I18N ();
    }

  *nreturn_vals = 1;
  *return_vals = values;

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  /* Set default values */
  /* ================== */

  set_default_settings ();

  /* Possibly retrieve data */
  /* ====================== */

  gimp_get_data ("plug_in_lighting", &mapvals);

  /* Get the specified drawable */
  /* ========================== */

  drawable = gimp_drawable_get (param[2].data.d_drawable);

  mapvals.drawable_id = drawable->drawable_id;

  check_drawables ();

  if (status == GIMP_PDB_SUCCESS)
    {
      /* Make sure that the drawable is RGBA or RGB color */
      /* ================================================ */

      if (gimp_drawable_is_rgb (drawable->drawable_id))
	{
	  /* Set the tile cache size */
          /* ======================= */

	  gimp_tile_cache_ntiles (TILE_CACHE_SIZE);
          
          switch (run_mode)
            {
              case GIMP_RUN_INTERACTIVE:
                if (main_dialog (drawable))
		  {
		    compute_image ();

		    gimp_set_data ("plug_in_lighting",
				   &mapvals, sizeof (LightingValues));
		    gimp_displays_flush ();
		  }
              break;

              case GIMP_RUN_WITH_LAST_VALS:
                image_setup (drawable, FALSE);
                compute_image ();
		gimp_displays_flush ();
                break;

              case GIMP_RUN_NONINTERACTIVE:
                if (nparams != 24)
		  {
		    status = GIMP_PDB_CALLING_ERROR;
		  }
                else
                  {
                    mapvals.bumpmap_id              = param[3].data.d_drawable;
                    mapvals.envmap_id               = param[4].data.d_drawable;
                    mapvals.bump_mapped             = (gint) param[5].data.d_int32;
                    mapvals.env_mapped              = (gint) param[6].data.d_int32;
                    mapvals.bumpmaptype             = (gint) param[7].data.d_int32;
                    mapvals.lightsource.type        = (LightType) param[8].data.d_int32;
		    mapvals.lightsource.color       = param[9].data.d_color;
                    mapvals.lightsource.position.x  = param[10].data.d_float;
                    mapvals.lightsource.position.y  = param[11].data.d_float;
                    mapvals.lightsource.position.z  = param[12].data.d_float;
                    mapvals.lightsource.direction.x = param[13].data.d_float;
                    mapvals.lightsource.direction.y = param[14].data.d_float;
                    mapvals.lightsource.direction.z = param[15].data.d_float;
                    mapvals.material.ambient_int    = param[16].data.d_float;
                    mapvals.material.diffuse_int    = param[17].data.d_float;
                    mapvals.material.diffuse_ref    = param[18].data.d_float;
                    mapvals.material.specular_ref   = param[19].data.d_float;
                    mapvals.material.highlight      = param[20].data.d_float;
                    mapvals.antialiasing            = (gint) param[21].data.d_int32;
                    mapvals.create_new_image        = (gint) param[22].data.d_int32;
                    mapvals.transparent_background  = (gint) param[23].data.d_int32;

                    check_drawables ();
                    image_setup (drawable, FALSE);
                    compute_image ();
                  }
              default:
                break;
            }
        }
      else
        status = GIMP_PDB_EXECUTION_ERROR;
    }

  values[0].data.d_status = status;
  gimp_drawable_detach (drawable);

  g_free (xpostab);
  g_free (ypostab);
}

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

MAIN ()
