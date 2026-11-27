from PIL import Image
import os
import numpy as np

base = r'C:\Users\georg\OneDrive\Documents\DoorOffice\resources'

# 1) Load source
src = os.path.join(base, 'door_logo.png')
img = Image.open(src).convert('RGB')
arr = np.array(img)  # HxWx3 RGB
print('loaded:', img.size)

# 2) Tight crop around the door: treat non-dark pixels as content.
# Background is near-black (all channels small); the door/knob is brighter.
content = arr.max(axis=2) > 40
ys, xs = np.nonzero(content)
x0, x1 = xs.min(), xs.max()
y0, y1 = ys.min(), ys.max()
print('door bbox:', x0, y0, x1, y1, 'size', (x1 - x0 + 1, y1 - y0 + 1))

# 3) Small padding (6px), clamp to bounds
pad = 6
x0 = max(x0 - pad, 0); y0 = max(y0 - pad, 0)
x1 = min(x1 + pad, arr.shape[1] - 1); y1 = min(y1 + pad, arr.shape[0] - 1)
crop = img.crop((x0, y0, x1 + 1, y1 + 1))
print('cropped size:', crop.size)

# 4) Make pure black background transparent -> RGBA
rgba = crop.convert('RGBA')
np_arr = np.array(rgba)
rgb = np_arr[:, :, :3]
alpha = np_arr[:, :, 3]
black = (rgb == [0, 0, 0]).all(axis=2)
alpha[black] = 0
np_arr[:, :, 3] = alpha
clean = Image.fromarray(np_arr, 'RGBA')

clean_path = os.path.join(base, 'door_logo_clean.png')
clean.save(clean_path, optimize=True)
print('saved', clean_path, clean.size)

# 5) Multi-size Windows icon (16, 32, 48, 64, 128, 256)
# Preserve the door's aspect ratio and center it on a transparent square canvas.
sizes = [16, 32, 48, 64, 128, 256]
def make_square(im, s):
    ratio = min(s / im.width, s / im.height)
    nw, nh = max(1, round(im.width * ratio)), max(1, round(im.height * ratio))
    resized = im.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    canvas.paste(resized, ((s - nw) // 2, (s - nh) // 2), resized)
    return canvas


imgs = [make_square(clean, s) for s in sizes]
ico_path = os.path.join(base, 'DoorOffice.ico')
imgs[-1].save(ico_path, format='ICO', sizes=[(s, s) for s in sizes])
print('saved', ico_path, 'sizes:', sizes)
print('DONE')
