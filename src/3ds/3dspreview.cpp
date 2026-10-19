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

// The box art slot is wide enough for landscape Famicom
// covers; NES boxes (150x200) are limited by its height.
#define BOXART_X        8
#define BOXART_Y        20
#define BOXART_WIDTH    176
#define BOXART_HEIGHT   200

#define SNAP_X          192
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


//---------------------------------------------------------
// "Donkey Kong (USA) [!]" -> "Donkey Kong": the title without
// the bracketed region and dump tags, so one image can serve
// every version of a game.
//---------------------------------------------------------
static void previewStripTags(const char *name, char *title, size_t size)
{
    size_t n = 0;
    int depth = 0;
    for (const char *p = name; *p && n + 1 < size; p++)
    {
        if (*p == '(' || *p == '[')
            depth++;
        else if ((*p == ')' || *p == ']') && depth > 0)
            depth--;
        else if (depth == 0)
            title[n++] = *p;
    }
    while (n > 0 && title[n - 1] == ' ')
        n--;
    title[n] = 0;
}


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


unsigned preview3dsDecodeTitleImage(unsigned char **pixels, unsigned *width, unsigned *height)
{
    // The app's folder first, where the .3dsx looks too, then the
    // current directory (the SD card root for the CIA).
    const char *name = strrchr(impl3dsTitleImage, '/');
    name = name ? name + 1 : impl3dsTitleImage;
    char path[_MAX_PATH];
    snprintf(path, _MAX_PATH, "%s/%s", impl3dsPreviewDir, name);

    *pixels = NULL;
    unsigned error = lodepng_decode32_file(pixels, width, height, path);
    if (error)
    {
        free(*pixels);
        *pixels = NULL;
        error = lodepng_decode32_file(pixels, width, height, impl3dsTitleImage);
    }
    return error;
}


void preview3dsBegin()
{
    previewActive = true;
    previewShown[0] = 0;

    if (preview3dsDecodeTitleImage(&titleImage.pixels, &titleImage.width, &titleImage.height)
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
        // An image named exactly like the ROM, or else after its
        // title without tags: Donkey Kong.png for Donkey Kong (USA).nes.
        char title[_MAX_PATH];
        previewStripTags(romName, title, _MAX_PATH);
        bool tryTitle = title[0] && strcmp(title, romName) != 0;

        found |= previewLoad(&boxart, "boxart", romName) ||
                 (tryTitle && previewLoad(&boxart, "boxart", title));
        found |= previewLoad(&snap, "snaps", romName) ||
                 (tryTitle && previewLoad(&snap, "snaps", title));
    }

    u32 *fb = (u32 *) gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    if (found)
    {
        memset(fb, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
        if (boxart.pixels && snap.pixels)
        {
            previewDraw(fb, &boxart, BOXART_X, BOXART_Y, BOXART_WIDTH, BOXART_HEIGHT);
            previewDraw(fb, &snap, SNAP_X, SNAP_Y, SNAP_WIDTH, SNAP_HEIGHT);
        }
        else if (boxart.pixels)
            previewDraw(fb, &boxart, (SCREEN_WIDTH - BOXART_WIDTH) / 2, BOXART_Y, BOXART_WIDTH, BOXART_HEIGHT);
        else
            previewDraw(fb, &snap, (SCREEN_WIDTH - SNAP_WIDTH) / 2, SNAP_Y, SNAP_WIDTH, SNAP_HEIGHT);
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
