/* Lighting Effects - A plug-in for GIMP
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LIGHTING_PREVIEW_H__
#define __LIGHTING_PREVIEW_H__

#define PREVIEW_WIDTH  200
#define PREVIEW_HEIGHT 200

typedef struct
{
  gint      x, y, w, h;
  GdkImage *image;
} BackBuffer;

/* Externally visible variables */

extern gint        lightx, lighty;
extern BackBuffer  backbuf;
extern gdouble    *xpostab, *ypostab;
extern gboolean    light_hit;
extern gboolean    left_button_pressed;

/* Externally visible functions */

void     draw_preview_image           (gboolean   recompute);
void     interactive_preview_callback (GtkWidget *widget);
gboolean preview_events               (GtkWidget *area,
                                       GdkEvent  *event);
void     update_light                 (gint       xpos,
                                       gint       ypos);

#endif  /* __LIGHTING_PREVIEW_H__ */
