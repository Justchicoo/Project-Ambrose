# Project Ambrose by Imjustchico
# Rebuilds the reference crops from a live client: it runs a scenario with every screen wait turned into a wait for the screen to change after the press and then settle, and writes each crop for the current key, but keeps a crop it already has when the new one does not look like it and leaves the new one beside it to be judged, because a scenario that walked onto the wrong screen would otherwise replace a good crop with a picture of the wrong one.
import os
import time

from . import screens
from .engine import Engine
from .errors import StepFailed
from .run import Run

SETTLED = 0.995
SETTLE_POLL = 0.4
SETTLE_NEEDED = 2
CROP_SUFFIX = ".png"
CANDIDATE_SUFFIX = ".candidate.png"


class ReferenceCapture(Engine):
    def __init__(self, *arguments, replace=False, **keywords):
        super().__init__(*arguments, **keywords)
        self.replace = replace
        self.written = []
        self.before_press = None

    def act_click(self, step):
        try:
            self.before_press = self.client.frame()
        except Exception:
            self.before_press = None
        return super().act_click(step)

    def on_screen(self, name):
        return True

    def act_wait_screen(self, step):
        picture = None
        said = "there is no crop to match yet, so the driver waited for the screen to settle"
        if not self.store.references.missing_crops(self.store.folder, step["screens"]):
            try:
                said = super().act_wait_screen(step)
                picture = self.current
            except StepFailed as error:
                said = f"no crop matched, so the driver waited for the screen to settle instead: {error}"
        if picture is None:
            picture, settled = self.settle(step["screens"], step["timeout"])
            self.current = picture
            said = f"{said} ({settled})"
        self.before_press = None
        return said + "; " + "; ".join(self.write(name, picture) for name in step["screens"])

    def crops_of(self, picture, names):
        return [picture.crop(self.store.references.crop_of(name)) for name in names]

    def steady(self, first, second):
        return all(screens.compare(one, other, tolerance=self.store.references.tolerance)["fraction"] >= SETTLED
                   for one, other in zip(first, second))

    def settle(self, names, timeout):
        deadline = time.monotonic() + timeout
        held = 0
        previous = None
        moved = self.before_press is None
        while True:
            if not self.client.alive():
                raise StepFailed("the client stopped while the driver waited for the screen to settle")
            picture = self.client.frame()
            crops = self.crops_of(picture, names)
            if not moved:
                moved = not self.steady(self.crops_of(self.before_press, names), crops)
            elif previous is not None:
                held = held + 1 if self.steady(previous, crops) else 0
                if held >= SETTLE_NEEDED:
                    return picture, "the crop stopped changing"
            previous = crops
            if time.monotonic() > deadline:
                if not moved:
                    raise StepFailed(f"the crop of {'/'.join(names)} never changed after the press within {timeout}s")
                return picture, (f"the crop of {'/'.join(names)} still moved after {timeout}s, so this is the frame at the end "
                                 "of that wait and the crop may hold a moment of an animation")
            time.sleep(SETTLE_POLL)

    def write(self, name, picture):
        crop = picture.crop(self.store.references.crop_of(name))
        path = self.store.references.crop_file(self.store.folder, name)
        before = None
        if os.path.isfile(path):
            try:
                before = screens.compare(crop, screens.load_png(path), tolerance=self.store.references.tolerance)
            except Exception:
                before = None
        size = f"{crop.width}x{crop.height}"
        kept = before is not None and before["fraction"] < self.store.references.fraction and not self.replace
        if kept:
            candidate = path[: -len(CROP_SUFFIX)] + CANDIDATE_SUFFIX
            screens.save_png(crop, candidate)
            self.written.append({"screen": name, "file": candidate, "size": size, "replaced": False,
                                 "against_the_old_crop": before})
            return (f"kept the crop of {name}, because only {before['fraction']} of the new one looks like it, which is "
                    f"what a scenario that walked onto the wrong screen leaves behind; the new one is beside it as "
                    f"{os.path.basename(candidate)}, and --replace takes it")
        screens.save_png(crop, path)
        self.store.loaded[name] = crop
        self.written.append({"screen": name, "file": path, "size": size, "replaced": True,
                             "against_the_old_crop": before})
        return f"wrote {name} ({size})" + (f", {before['fraction']} of it matching the old crop" if before else "")


class CaptureRun(Run):
    def make_engine(self, client, server, store, variables, databases):
        return ReferenceCapture(self.scenario, client, server, store, self.shots, variables, databases,
                                replace=bool(self.options.get("replace")))

    def extra(self, engine):
        return {"references_written": engine.written}
