#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include "3dstypes.h"
#include "3dsinterface.h"
#include "3dslodepng.h"
#include "3dspreview.h"

#define SCREEN_WIDTH    400
#define SCREEN_HEIGHT   240

#define BOXART_X        16
#define BOXART_Y        20
#define BOXART_WIDTH    150
#define BOXART_HEIGHT   200

#define SNAP_X          184
#define SNAP_Y          32
#define SNAP_WIDTH      200
#define SNAP_HEIGHT     175

typedef struct
{
    unsigned char   *pixels;    // RGBA, as decoded by lodepng
    unsigned        width;
    unsigned        height;
} SPreviewImage;

static bool previewActive = false;
static SPreviewImage titleImage = { NULL, 0, 0 };
static char previewShown[_MAX_PATH];


static bool previewLoad(SPreviewImage *image, const char *folder, const char *romName)
{
    char path[_MAX_PATH];
    snprintf(path, _MAX_PATH, "%s/%s/%s.png", impl3dsPreviewDir, folder, romName);

    image->pixels = NULL;
    if (lodepng_decode32_file(&image->pixels, &image->width, &image->height, path))
    {
        free(image->pixels);
        image->pixels = NULL;
        return false;
    }
    return true;
}


static void previewFree(SPreviewImage *image)
{
    free(image->pixels);
    image->pixels = NULL;
}


//---------------------------------------------------------
// Draws an image into the top screen's framebuffer (which is
// stored rotated: one 240-pixel column per x), centred in the
// given slot and scaled down to fit it if it's larger.
// Transparent pixels are blended against black.
//---------------------------------------------------------
static void previewDraw(u32 *fb, SPreviewImage *image, int slotX, int slotY, int slotWidth, int slotHeight)
{
    if (!image->pixels || !image->width || !image->height)
        return;

    int width = image->width;
    int height = image->height;
    if (width > slotWidth || height > slotHeight)
    {
        if (image->width * slotHeight > image->height * slotWidth)
        {
            width = slotWidth;
            height = image->height * slotWidth / image->width;
        }
        else
        {
            height = slotHeight;
            width = image->width * slotHeight / image->height;
        }
        if (width < 1) width = 1;
        if (height < 1) height = 1;
    }

    int left = slotX + (slotWidth - width) / 2;
    int top = slotY + (slotHeight - height) / 2;
    for (int y = 0; y < height; y++)
    {
        u8 *row = image->pixels + (y * image->height / height) * image->width * 4;
        for (int x = 0; x < width; x++)
        {
            u8 *p = row + (x * image->width / width) * 4;
            u32 a = p[3];
            u32 r = p[0] * a / 255;
            u32 g = p[1] * a / 255;
            u32 b = p[2] * a / 255;
            fb[(left + x) * SCREEN_HEIGHT + (SCREEN_HEIGHT - 1 - (top + y))] = (r << 24) | (g << 16) | (b << 8) | 0xff;
        }
    }
}


void preview3dsBegin()
{
    previewActive = true;
    previewShown[0] = 0;

    if (lodepng_decode32_file(&titleImage.pixels, &titleImage.width, &titleImage.height, impl3dsTitleImage)
        || titleImage.width != SCREEN_WIDTH || titleImage.height != SCREEN_HEIGHT)
    {
        previewFree(&titleImage);
    }

    // Draw straight into the displayed framebuffer: the menu
    // swaps both screens' buffers whenever it redraws the
    // bottom screen.
    gfxSetDoubleBuffering(GFX_TOP, false);
}


void preview3dsShow(const char *romFileName)
{
    if (!previewActive)
        return;

    // The ROM name without its extension.
    char romName[_MAX_PATH] = "";
    if (romFileName)
    {
        strncpy(romName, romFileName, _MAX_PATH - 1);
        romName[_MAX_PATH - 1] = 0;
        char *dot = strrchr(romName, '.');
        if (dot)
            *dot = 0;
    }
    if (strcmp(romName, previewShown) == 0 && previewShown[0])
        return;

    SPreviewImage boxart = { NULL, 0, 0 };
    SPreviewImage snap = { NULL, 0, 0 };
    bool found = false;
    if (romName[0])
    {
        found |= previewLoad(&boxart, "boxart", romName);
        found |= previewLoad(&snap, "snaps", romName);
    }

    u32 *fb = (u32 *) gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    if (found)
    {
        memset(fb, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
        previewDraw(fb, &boxart, BOXART_X, BOXART_Y, BOXART_WIDTH, BOXART_HEIGHT);
        previewDraw(fb, &snap, SNAP_X, SNAP_Y, SNAP_WIDTH, SNAP_HEIGHT);
    }
    else if (titleImage.pixels)
    {
        previewDraw(fb, &titleImage, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
    else
    {
        memset(fb, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    }
    GSPGPU_FlushDataCache(fb, SCREEN_WIDTH * SCREEN_HEIGHT * 4);

    previewFree(&boxart);
    previewFree(&snap);

    // An empty name stands for the title image.
    strncpy(previewShown, found ? romName : "", _MAX_PATH);
}


void preview3dsEnd()
{
    previewActive = false;
    previewFree(&titleImage);
    gfxSetDoubleBuffering(GFX_TOP, true);
}
