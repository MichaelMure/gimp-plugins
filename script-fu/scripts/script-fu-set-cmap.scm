; Set Colormap v1.1  September 29, 2004
; by Kevin Cozens <kcozens@interlog.com>
;
; Change the colourmap of an image to the colours in a specified palette.
; Included is tiny-fu-make-cmap-array (available for use in scripts) which
; returns an INT8ARRAY containing the colours from a specified palette.
; This array can be used as the cmap argument for gimp-image-set-cmap.

; The GIMP -- an image manipulation program
; Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

(define (tiny-fu-make-cmap-array palette)
  (let (
       (num-colours)
       (colour)
       (cmap)
       (i 0)
       )

    (set! num-colours (car (gimp-palette-get-info palette)))
    (set! cmap (cons-array (* num-colours 3) 'byte))

    (while (< i num-colours)
      (set! colour (caddr (gimp-palette-entry-get-color palette i)))
      (aset cmap (* i 3) (car colour))
      (aset cmap (+ (* i 3) 1) (cadr colour))
      (aset cmap (+ (* i 3) 2) (caddr colour))
      (set! i (+ i 1))
    )

    cmap
  )
)

(define (tiny-fu-set-cmap img drawable palette)
  (gimp-image-set-cmap img
                       (* (car (gimp-palette-get-info palette)) 3)
                       (tiny-fu-make-cmap-array palette))
  (gimp-displays-flush)
)

(tiny-fu-register "tiny-fu-set-cmap"
    _"<Image>/Tiny-Fu/Utils/Set Colormap"
    "Change the colourmap of an image to the colours in a specified palette."
    "Kevin Cozens <kcozens@interlog.com>"
    "Kevin Cozens"
    "September 29, 2004"
    "INDEXED*"
    SF-IMAGE     "Image"    0
    SF-DRAWABLE  "Drawable" 0
    SF-PALETTE  _"Palette"  "Default"
)
