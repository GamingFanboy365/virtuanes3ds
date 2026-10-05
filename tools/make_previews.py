#!/usr/bin/env python3
#
# Makes the box art and screenshot previews for VirtuaNES's ROM list at the
# sizes the 3DS shows them: box art fits inside 150x200, screenshots are
# 200x175. Images that size load fastest on the 3DS and look better than
# ones the 3DS scales itself.
#
# Takes folders of images, for example the Named_Boxarts and Named_Snaps (or
# Named_Titles) folders of a libretro-thumbnails pack
# (https://github.com/libretro-thumbnails/Nintendo_-_Nintendo_Entertainment_System),
# and writes OUT/boxart and OUT/snaps. Copy those two folders to
# /3ds/virtuanes_3ds/ on the SD card.
#
# With --roms, only the images for the ROMs in that folder are made, and
# they're named after the ROM files, which is how VirtuaNES finds them. ROMs
# whose names don't match an image exactly (Tennis.nes against "Tennis
# (Japan, USA).png") are matched by title, ignoring the bracketed tags.
#
# Needs Pillow: pip install pillow
#
#   tools/make_previews.py --boxarts Named_Boxarts --snaps Named_Snaps \
#       --roms /path/to/roms --out previews
#
import argparse
import os
import re
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit('This needs Pillow: pip install pillow')

BOXART_SIZE = (150, 200)
SNAP_SIZE = (200, 175)
ROM_EXTENSIONS = ('.nes', '.fds', '.unf', '.unif', '.nsf')
IMAGE_EXTENSIONS = ('.png', '.jpg', '.jpeg', '.bmp', '.gif', '.webp')

# Regions preferred when a ROM's title matches several images.
REGION_ORDER = ('USA', 'World', 'Europe', 'Japan')


def libretro_name(name):
    """libretro-thumbnails replace these characters in file names."""
    return re.sub(r'[&*/:`<>?\\|"]', '_', name)


def title_key(name):
    """The title without bracketed tags, lowercased, letters and digits only."""
    name = re.sub(r'\([^)]*\)|\[[^\]]*\]', '', name)
    return re.sub(r'[^a-z0-9]+', '', name.lower())


def list_images(folder):
    images = {}
    if folder:
        for entry in os.listdir(folder):
            name, ext = os.path.splitext(entry)
            if ext.lower() in IMAGE_EXTENSIONS:
                images[name] = os.path.join(folder, entry)
    return images


def find_image(images, rom_name):
    """Returns (path, exact) for a ROM name, or (None, False)."""
    for name in (rom_name, libretro_name(rom_name)):
        if name in images:
            return images[name], True
    key = title_key(rom_name)
    if not key:
        return None, False
    candidates = sorted(n for n in images if title_key(n) == key)
    if not candidates:
        return None, False
    for region in REGION_ORDER:
        for name in candidates:
            if region in name:
                return images[name], False
    return images[candidates[0]], False


def load_rgb(path):
    image = Image.open(path)
    if image.mode in ('RGBA', 'LA') or (image.mode == 'P' and 'transparency' in image.info):
        # Transparent pixels show black on the 3DS too.
        image = image.convert('RGBA')
        background = Image.new('RGB', image.size, (0, 0, 0))
        background.paste(image, mask=image.getchannel('A'))
        return background
    return image.convert('RGB')


def make_boxart(source, destination):
    image = load_rgb(source)
    width, height = image.size
    scale = min(BOXART_SIZE[0] / width, BOXART_SIZE[1] / height)
    if scale < 1:
        size = (max(1, round(width * scale)), max(1, round(height * scale)))
        image = image.resize(size, Image.LANCZOS)
    image.save(destination, optimize=True)


def make_snap(source, destination, keep_aspect):
    image = load_rgb(source)
    if keep_aspect:
        image.thumbnail(SNAP_SIZE, Image.LANCZOS)
    else:
        image = image.resize(SNAP_SIZE, Image.LANCZOS)
    image.save(destination, optimize=True)


def main():
    parser = argparse.ArgumentParser(
        description='Makes box art and screenshot previews for VirtuaNES for 3DS.')
    parser.add_argument('--boxarts', help='folder of box art images')
    parser.add_argument('--snaps', help='folder of screenshots (or title screens)')
    parser.add_argument('--roms', help='only make previews for the ROMs in this folder, named after them')
    parser.add_argument('--out', default='previews',
                        help='output folder; copy its boxart and snaps folders to '
                             '/3ds/virtuanes_3ds/ on the SD card (default: previews)')
    parser.add_argument('--keep-snap-aspect', action='store_true',
                        help='fit screenshots inside 200x175 instead of filling it')
    args = parser.parse_args()
    if not args.boxarts and not args.snaps:
        parser.error('give --boxarts, --snaps or both')

    boxarts = list_images(args.boxarts)
    snaps = list_images(args.snaps)

    # (output name, box art path, snap path)
    jobs = []
    if args.roms:
        roms = sorted(os.path.splitext(f)[0] for f in os.listdir(args.roms)
                      if os.path.splitext(f)[1].lower() in ROM_EXTENSIONS)
        for rom in roms:
            boxart, exact_boxart = find_image(boxarts, rom)
            snap, exact_snap = find_image(snaps, rom)
            matched = [os.path.splitext(os.path.basename(p))[0]
                       for p, exact in ((boxart, exact_boxart), (snap, exact_snap))
                       if p and not exact]
            note = ' (matched "%s")' % matched[0] if matched else ''
            if not boxart and not snap:
                print('no images: %s' % rom)
                continue
            print('%s%s' % (rom, note))
            jobs.append((rom, boxart, snap))
    else:
        for name in sorted(set(boxarts) | set(snaps)):
            jobs.append((name, boxarts.get(name), snaps.get(name)))

    os.makedirs(os.path.join(args.out, 'boxart'), exist_ok=True)
    os.makedirs(os.path.join(args.out, 'snaps'), exist_ok=True)
    made = [0, 0]
    for name, boxart, snap in jobs:
        if boxart:
            make_boxart(boxart, os.path.join(args.out, 'boxart', name + '.png'))
            made[0] += 1
        if snap:
            make_snap(snap, os.path.join(args.out, 'snaps', name + '.png'), args.keep_snap_aspect)
            made[1] += 1

    print('%d box art and %d screenshots in %s; copy its boxart and snaps folders '
          'to /3ds/virtuanes_3ds/ on the SD card.' % (made[0], made[1], args.out))


if __name__ == '__main__':
    main()
