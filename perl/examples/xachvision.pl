#!/usr/bin/perl

# Once again, an effect of Xach's
# Created by Seth Burgess <sjburges@gimp.org>

use Gimp;
use Gimp::Fu;

register "xachvision",
         "Xach Survielence Camera/XachVision",
         "This makes an interlaced-looking machine vision type thing.",
         "Seth Burgess",
         "Seth Burgess <sjburges\@gimp.org>",
         "1999-02-28",
         "<Image>/Filters/Noise/Xach Vision",
         "RGB*, GRAY*",
         [
          [PF_COLOR,	"color",	"What Color to see the world in", [0, 255, 0]],
	  [PF_SLIDER,   "added_noise", "How much noise to add", 25, [0,255,5]]
         ],
         sub {
   my($img,$drawable,$color,$amt) =@_;

  eval { $img->undo_push_group_start };
	$oldbackground = gimp_palette_get_background();

	$midlayer = $drawable->gimp_layer_copy(1);
	$img->add_layer($midlayer, 0);

	$toplayer = $drawable->gimp_layer_copy(0);	
	$img->add_layer($toplayer, 0);

	gimp_palette_set_background($color);
	$toplayer->edit_fill();
	$toplayer->set_mode(COLOR_MODE);
	
	gimp_palette_set_background([0,0,0]);
	$drawable->edit_fill();

	$amt = $amt/255;
	$midlayer->plug_in_noisify(1,$amt, $amt, $amt, $amt);
	$midmask = $midlayer->create_mask(0);
	$img->add_layer_mask($midlayer,	$midmask);
	$midmask->plug_in_grid($img->height * 3, 3, 0, 0);
	$midmask->plug_in_gauss_iir(1.01, 1, 1);

	gimp_palette_set_background($oldbackground);
  eval { $img->undo_push_group_end };
	gimp_displays_flush();
	return();
};

exit main;

