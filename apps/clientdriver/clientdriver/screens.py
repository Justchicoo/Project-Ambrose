# Project Ambrose by Imjustchico
# Frames of the client's window as plain RGB pixels, and the match that decides which screen is on it: the fraction of pixels within a tolerance of a reference crop, because the mean difference alone calls the same dialog over a loaded scene a stranger.
import os

DEFAULT_TOLERANCE = 32
DEFAULT_FRACTION = 0.65
CHANGE_FRACTION = 0.95
CHANGE_STEP = 4
BLANK_LOW = 16
BLANK_HIGH = 239


class Bitmap:
    __slots__ = ("width", "height", "data")

    def __init__(self, width, height, data):
        if width <= 0 or height <= 0:
            raise ValueError(f"a frame of {width}x{height} holds no pixels")
        if len(data) != width * height * 3:
            raise ValueError(f"a {width}x{height} frame needs {width * height * 3} bytes, not {len(data)}")
        self.width = width
        self.height = height
        self.data = bytes(data)

    @property
    def size(self):
        return (self.width, self.height)

    def crop(self, box):
        left, top, right, bottom = box
        if not 0 <= left < right <= self.width or not 0 <= top < bottom <= self.height:
            raise ValueError(f"the crop {tuple(box)} does not lie inside a {self.width}x{self.height} frame")
        rows = []
        for y in range(top, bottom):
            start = (y * self.width + left) * 3
            rows.append(self.data[start:start + (right - left) * 3])
        return Bitmap(right - left, bottom - top, b"".join(rows))

    def pixel(self, x, y):
        at = (y * self.width + x) * 3
        return (self.data[at], self.data[at + 1], self.data[at + 2])


def solid(width, height, color):
    return Bitmap(width, height, bytes(color) * (width * height))


def from_bgra(buffer, width, height, stride=None):
    stride = stride if stride is not None else width * 4
    if len(buffer) < stride * height:
        raise ValueError(f"a {width}x{height} frame of {stride}-byte rows needs {stride * height} bytes, not {len(buffer)}")
    view = memoryview(buffer)
    if stride == width * 4:
        packed = bytearray(view[:stride * height])
    else:
        packed = bytearray()
        for y in range(height):
            packed += view[y * stride:y * stride + width * 4]
    del packed[3::4]
    blue = packed[0::3]
    packed[0::3] = packed[2::3]
    packed[2::3] = blue
    return Bitmap(width, height, bytes(packed))


def compare(first, second, tolerance=DEFAULT_TOLERANCE, step=1):
    if first.size != second.size:
        raise ValueError(f"a {first.width}x{first.height} frame cannot be compared with a {second.width}x{second.height} one")
    left = first.data
    right = second.data
    width = first.width
    off = 0
    counted = 0
    difference = 0
    for y in range(0, first.height, step):
        row = y * width * 3
        for x in range(0, width, step):
            at = row + x * 3
            red = abs(left[at] - right[at])
            green = abs(left[at + 1] - right[at + 1])
            blue = abs(left[at + 2] - right[at + 2])
            difference += red + green + blue
            counted += 1
            if red > tolerance or green > tolerance or blue > tolerance:
                off += 1
    return {"fraction": round(1.0 - off / counted, 3), "mean": round(difference / (counted * 3), 1), "pixels": counted}


def changed(first, second, tolerance=DEFAULT_TOLERANCE, fraction=CHANGE_FRACTION):
    if first is None or second is None or first.size != second.size:
        return True, None
    scored = compare(first, second, tolerance=tolerance, step=CHANGE_STEP)
    return scored["fraction"] < fraction, scored


def is_blank(bitmap, step=CHANGE_STEP):
    lowest = 255
    highest = 0
    for y in range(0, bitmap.height, step):
        row = y * bitmap.width * 3
        for x in range(0, bitmap.width, step):
            at = row + x * 3
            for channel in (bitmap.data[at], bitmap.data[at + 1], bitmap.data[at + 2]):
                lowest = min(lowest, channel)
                highest = max(highest, channel)
    return highest < BLANK_LOW or lowest > BLANK_HIGH


class Store:
    def __init__(self, references, folder, loader=None):
        self.references = references
        self.folder = folder
        self.loader = loader or load_png
        self.loaded = {}

    def reference(self, name):
        if name not in self.loaded:
            self.loaded[name] = self.loader(self.references.crop_file(self.folder, name))
        return self.loaded[name]

    def score(self, frame, name):
        crop = frame.crop(self.references.crop_of(name))
        return compare(crop, self.reference(name), tolerance=self.references.tolerance)

    def identify(self, frame, names):
        scored = {name: self.score(frame, name) for name in names}
        best = max(scored, key=lambda name: scored[name]["fraction"])
        return (best if scored[best]["fraction"] >= self.references.fraction else None), scored


def load_png(path):
    from PIL import Image

    with Image.open(path) as opened:
        converted = opened.convert("RGB")
        return Bitmap(converted.width, converted.height, converted.tobytes())


def save_png(bitmap, path):
    from PIL import Image

    directory = os.path.dirname(path)
    if directory:
        os.makedirs(directory, exist_ok=True)
    Image.frombytes("RGB", bitmap.size, bitmap.data).save(path)
    return path
