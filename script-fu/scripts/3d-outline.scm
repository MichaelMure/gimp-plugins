; 3d-outlined-patterned-shadowed-and-bump-mapped-logo :)
; creates outlined border of a text with patterns
;
; The GIMP -- an image manipulation program
; Copyright (C) 1995 Spencer Kimball and Peter Mattis
;
; 3d-outline creates outlined border of a text with patterns
; Copyright (C) 1998 Hrvoje Horvat
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

(define (apply-3d-outline-logo-effect img
				      logo-layer
				      text-pattern
				      outline-blur-radius
				      shadow-blur-radius
				      bump-map-blur-radius
				      noninteractive
				      s-offset-x
				      s-offset-y)
  (let* ((width (car (gimp-drawable-width logo-layer)))
         (height (car (gimp-drawable-height logo-layer)))
         (bg-layer (car (gimp-layer-new img width height
					RGB-IMAGE "Background" 100 NORMAL-MODE)))
         (pattern (car (gimp-layer-new img width height
				       RGBA-IMAGE "Pattern" 100 NORMAL-MODE)))
         (layer2)
         (layer3)
         (pattern-mask)
         (floating-sel))

    (gimp-context-push)

    (gimp-selection-none img)
    (script-fu-util-image-resize-from-layer img logo-layer)
    (gimp-image-add-layer img pattern 1)
    (gimp-image-add-layer img bg-layer 2)
    (gimp-context-set-background '(255 255 255))
    (gimp-edit-fill bg-layer BACKGROUND-FILL)
    (gimp-edit-clear pattern)
    (gimp-layer-set-lock-alpha logo-layer TRUE)
    (gimp-context-set-foreground '(0 0 0))
    (gimp-edit-fill logo-layer FOREGROUND-FILL)
    (gimp-layer-set-lock-alpha logo-layer FALSE)
    (plug-in-gauss-iir 1 img logo-layer outline-blur-radius TRUE TRUE)

    (gimp-drawable-set-visible pattern FALSE)
    (set! layer2 (car (gimp-image-merge-visible-layers img CLIP-TO-IMAGE)))
    (plug-in-edge 1 img layer2 2 1 0)
    (set! layer3 (car (gimp-layer-copy layer2 TRUE)))
    (gimp-image-add-layer img layer3 2)
    (plug-in-gauss-iir 1 img layer2 bump-map-blur-radius TRUE TRUE)

    (gimp-selection-all img)
    (gimp-context-set-pattern text-pattern)
    (gimp-edit-bucket-fill pattern
			   PATTERN-BUCKET-FILL NORMAL-MODE 100 0 FALSE 0 0)
    (plug-in-bump-map noninteractive img pattern layer2
		      110.0 45.0 4 0 0 0 0 TRUE FALSE 0)

    (set! pattern-mask (car (gimp-layer-create-mask pattern ADD-ALPHA-MASK)))
    (gimp-layer-add-mask pattern pattern-mask)

    (gimp-selection-all img)
    (gimp-edit-copy layer3)
    (set! floating-sel (car (gimp-edit-paste pattern-mask FALSE)))
    (gimp-floating-sel-anchor floating-sel)

    (gimp-layer-remove-mask pattern MASK-APPLY)
    (gimp-invert layer3)
    (plug-in-gauss-iir 1 img layer3 shadow-blur-radius TRUE TRUE)

    (gimp-drawable-offset layer3 0 1 s-offset-x s-offset-y)

    (gimp-drawable-set-visible layer2 FALSE)
    (gimp-drawable-set-visible pattern TRUE)
    ;;(set! final (car (gimp-image-flatten img)))

    (gimp-context-pop)))

(define (script-fu-3d-outline-logo-alpha img
					 logo-layer
					 text-pattern
					 outline-blur-radius
					 shadow-blur-radius
					 bump-map-blur-radius
					 noninteractive
					 s-offset-x
					 s-offset-y)
  (begin
    (gimp-image-undo-group-start img)
    (apply-3d-outline-logo-effect img logo-layer text-pattern
				  outline-blur-radius shadow-blur-radius
				  bump-map-blur-radius noninteractive
				  s-offset-x s-offset-y)
    (gimp-image-undo-group-end img)
    (gimp-displays-flush)))

(script-fu-register "script-fu-3d-outline-logo-alpha"
                    _"3D _Outline..."
                    "Creates outlined texts with drop shadow"
                    "Hrvoje Horvat (hhorvat@open.hr)"
                    "Hrvoje Horvat"
                    "07 April, 1998"
                    "RGBA"
                    SF-IMAGE       "Image"               0
                    SF-DRAWABLE    "Drawable"            0
		    SF-PATTERN    _"Pattern"             "Parque #1"
                    SF-ADJUSTMENT _"Outline blur radius" '(5 1 200 1 10 0 1)
                    SF-ADJUSTMENT _"Shadow blur radius"  '(10 1 200 1 10 0 1)
                    SF-ADJUSTMENT _"Bumpmap (alpha layer) blur radius" '(5 1 200 1 10 0 1)
		    SF-TOGGLE     _"Default bumpmap settings" TRUE
		    SF-ADJUSTMENT _"Shadow X offset"     '(0 0 200 1 5 0 1)
                    SF-ADJUSTMENT _"Shadow Y offset"     '(0 0 200 1 5 0 1))

(script-fu-menu-register "script-fu-3d-outline-logo-alpha"
			 "<Image>/Filters/Alpha to Logo")


(define (script-fu-3d-outline-logo text-pattern
				   text
				   size
				   font
				   outline-blur-radius
				   shadow-blur-radius
				   bump-map-blur-radius
				   noninteractive
				   s-offset-x
				   s-offset-y)
  (let* ((img (car (gimp-image-new 256 256 RGB)))
         (text-layer (car (gimp-text-fontname img -1 0 0 text 30 TRUE size PIXELS font))))
    (gimp-image-undo-disable img)
    (apply-3d-outline-logo-effect img text-layer text-pattern
				  outline-blur-radius shadow-blur-radius
				  bump-map-blur-radius noninteractive
				  s-offset-x s-offset-y)
    (gimp-image-undo-enable img)
    (gimp-display-new img)))

(script-fu-register "script-fu-3d-outline-logo"
                    _"3D _Outline..."
                    "Creates outlined texts with drop shadow"
                    "Hrvoje Horvat (hhorvat@open.hr)"
                    "Hrvoje Horvat"
                    "07 April, 1998"
                    ""
		    SF-PATTERN    _"Pattern"             "Parque #1"
                    SF-STRING     _"Text"                "The Gimp"
                    SF-ADJUSTMENT _"Font size (pixels)"  '(100 2 1000 1 10 0 1)
                    SF-FONT       _"Font"                "RoostHeavy"
                    SF-ADJUSTMENT _"Outline blur radius" '(5 1 200 1 10 0 1)
                    SF-ADJUSTMENT _"Shadow blur radius"  '(10 1 200 1 10 0 1)
                    SF-ADJUSTMENT _"Bumpmap (alpha layer) blur radius" '(5 1 200 1 10 0 1)
		    SF-TOGGLE     _"Default bumpmap settings" TRUE
		    SF-ADJUSTMENT _"Shadow X offset"     '(0 0 200 1 5 0 1)
                    SF-ADJUSTMENT _"Shadow Y offset"     '(0 0 200 1 5 0 1))

(script-fu-menu-register "script-fu-3d-outline-logo"
			 "<Toolbox>/Xtns/Logos")
