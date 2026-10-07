#ifndef _3DSPREVIEW_H_
#define _3DSPREVIEW_H_

//---------------------------------------------------------
// Box art and screenshot previews on the top screen for the
// ROM highlighted in the ROM menu.
//
// The images are <impl3dsPreviewDir>/boxart/<ROM name>.png
// (shown in a 176x200 slot on the left, which fits NES
// boxes at 150x200 as well as landscape Famicom and square
// Famicom Disk System covers) and
// <impl3dsPreviewDir>/snaps/<ROM name>.png (200x175 slot on
// the right), where <ROM name> is the ROM's file name
// without its extension. Images of that size or smaller are
// drawn as they are; larger ones are scaled down to fit.
// A game with only one of the two images has it centred.
// Without either image, the top screen shows the title
// image.
//---------------------------------------------------------

// Call before showing the ROM menu.
void preview3dsBegin();

// Shows the previews for romFileName, or the title image
// for NULL.
void preview3dsShow(const char *romFileName);

// Call after leaving the ROM menu.
void preview3dsEnd();

#endif
