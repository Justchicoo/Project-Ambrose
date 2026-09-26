# Project Ambrose by Imjustchico
# Drives the user's own client: the Ambrose launcher starts it, the window is found by class and size, text and keys are posted as window messages so the machine stays usable, a press borrows the cursor and the foreground for about a second and raises the window above anything covering the point it presses, because the client's interface drops mouse messages while its window is not the active one and hit-tests a press against the real cursor, and says whether it got them, and frames come from the composited window surface so a covered window still reads, with a blank frame, which the client gives while it swaps what it draws, tried again a few times before a step fails on it.
import contextlib
import ctypes
import os
import re
import subprocess
import threading
import time

from . import screens
from .errors import StepFailed
from .logtail import LogTail

WINDOW_CLASS = "Wizard Graphical Client"
PROGRAM = "WizardGraphicalClient.exe"
FRAME_ATTEMPTS = 5
FRAME_RETRY_SECONDS = 0.5
LOG_NAME = "WizardClient.log"
NO_WINDOW = 0x08000000
SW_SHOWNOACTIVATE = 4
PW_RENDERFULLCONTENT = 2
SMTO_ABORTIFHUNG = 0x0002
MK_LBUTTON = 0x0001
DWMWA_EXTENDED_FRAME_BOUNDS = 9
MODIFIER_KEYS = (0x10, 0x11, 0x12, 0x5B, 0x5C)
MOUSE_BUTTONS = (0x01, 0x02, 0x04)
EXTENDED_KEYS = (0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2D, 0x2E)
IDLE_TIMEOUT = 60
INSTALL_LINE = re.compile(r"^launcher: install (.*) \((.*)\)$")
COMMAND_LINE = re.compile(r"^launcher: (.*WizardGraphicalClient\.exe.*)$")


def prepare_process():
    ctypes.windll.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))


def modifiers_held():
    import win32api

    return any(win32api.GetAsyncKeyState(key) & 0x8000 for key in MODIFIER_KEYS)


def buttons_held():
    import win32api

    return any(win32api.GetAsyncKeyState(button) & 0x8000 for button in MOUSE_BUTTONS)


def wait_until_released(held, what, timeout=IDLE_TIMEOUT):
    deadline = time.monotonic() + timeout
    while held():
        if time.monotonic() > deadline:
            raise StepFailed(f"{what} stayed held for {timeout}s, so the driver sent nothing to the client")
        time.sleep(0.05)


def windows_of(pid):
    import win32gui
    import win32process

    found = []

    def visit(handle, _unused):
        if win32process.GetWindowThreadProcessId(handle)[1] == pid and win32gui.GetClassName(handle) == WINDOW_CLASS:
            found.append(handle)
        return True

    win32gui.EnumWindows(visit, None)
    return found


def force_foreground(handle, timeout=2.0):
    import win32api
    import win32con
    import win32gui
    import win32process

    user32 = ctypes.windll.user32
    if win32gui.GetForegroundWindow() == handle:
        return True
    previous = win32gui.GetForegroundWindow()
    mine = win32api.GetCurrentThreadId()
    other = win32process.GetWindowThreadProcessId(previous)[0] if previous else 0
    attached = False
    try:
        if other and other != mine:
            attached = bool(user32.AttachThreadInput(mine, other, True))
        win32gui.SetWindowPos(handle, win32con.HWND_TOP, 0, 0, 0, 0, win32con.SWP_NOMOVE | win32con.SWP_NOSIZE)
        user32.SetForegroundWindow(handle)
        if win32gui.GetForegroundWindow() != handle:
            user32.SwitchToThisWindow(handle, True)
    finally:
        if attached:
            user32.AttachThreadInput(mine, other, False)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if win32gui.GetForegroundWindow() == handle:
            return True
        time.sleep(0.05)
    return False


class Client:
    def __init__(self, launcher, run_folder, host, port, window, client_dir=None, locale=None, log=None,
                 install=None, revision=None, character=None):
        self.launcher = launcher
        self.run_folder = run_folder
        self.host = host
        self.port = int(port)
        self.window = window
        self.client_dir = client_dir
        self.locale = locale
        self.character = character
        self.launcher_process = None
        self.launcher_output = os.path.join(os.path.dirname(run_folder), "launcher.txt")
        self.launcher_log = LogTail(self.launcher_output)
        self.log = LogTail(log or os.path.join(run_folder, LOG_NAME), encoding="latin-1")
        self.pid = None
        self.handle = None
        self.install = install
        self.revision = revision
        self.command = None
        self.previous_foreground = 0
        self.frame_source = None
        self._output = None

    @property
    def pids(self):
        launcher = self.launcher_process.pid if self.launcher_process is not None else None
        return [pid for pid in (self.pid, launcher) if pid]

    def arguments(self):
        wanted = [self.launcher, "--host", self.host, "--port", str(self.port),
                  "--window", f"{self.window[0]}x{self.window[1]}", "--run-dir", self.run_folder, "--wait"]
        if self.client_dir:
            wanted += ["--client", self.client_dir]
        if self.locale:
            wanted += ["--locale", self.locale]
        if self.character:
            wanted += ["--character", self.character]
        return wanted

    def start(self, timeout=180):
        import win32gui

        os.makedirs(os.path.dirname(self.launcher_output), exist_ok=True)
        self.previous_foreground = win32gui.GetForegroundWindow()
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = SW_SHOWNOACTIVATE
        self._output = open(self.launcher_output, "wb")
        self.launcher_process = subprocess.Popen(self.arguments(), stdout=self._output, stderr=subprocess.STDOUT,
                                                 stdin=subprocess.DEVNULL, startupinfo=startup, creationflags=NO_WINDOW)
        self.pid = self.find_process(timeout)
        return f"the launcher started the client from {self.install} ({self.revision}) as process {self.pid}"

    def describe(self):
        self.launcher_log.poll()
        for line in self.launcher_log.lines:
            install = INSTALL_LINE.match(line.strip())
            if install:
                self.install, self.revision = install.group(1), install.group(2)
            command = COMMAND_LINE.match(line.strip())
            if command:
                self.command = command.group(1)
        return self.command

    def launcher_alive(self):
        return self.launcher_process is not None and self.launcher_process.poll() is None

    def find_process(self, timeout=180):
        import psutil

        deadline = time.monotonic() + timeout
        parent = psutil.Process(self.launcher_process.pid)
        while time.monotonic() < deadline:
            for child in parent.children(recursive=True):
                with contextlib.suppress(psutil.Error):
                    if child.name().lower() == PROGRAM.lower():
                        return child.pid
            if not self.launcher_alive():
                lines = " ".join(line.strip() for line in self.launcher_log.lines[-4:])
                raise StepFailed(f"the launcher stopped before the client appeared: {lines}")
            time.sleep(0.2)
        raise StepFailed(f"the launcher did not start {PROGRAM} within {timeout}s")

    def alive(self):
        import psutil

        if not self.pid:
            return False
        try:
            return psutil.Process(self.pid).status() != psutil.STATUS_ZOMBIE
        except psutil.Error:
            return False

    def find_window(self, timeout=180):
        import win32gui

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if not self.alive():
                raise StepFailed("the client stopped before its window appeared")
            visible = [handle for handle in windows_of(self.pid) if win32gui.IsWindowVisible(handle)]
            if visible:
                self.handle = visible[0]
                _left, _top, width, height = win32gui.GetClientRect(self.handle)
                if (width, height) != tuple(self.window):
                    raise StepFailed(f"the client area is {width}x{height} and the references were measured at "
                                     f"{self.window[0]}x{self.window[1]}, so every press would land in the wrong place")
                return self.handle
            time.sleep(0.2)
        raise StepFailed(f"the client window did not appear within {timeout}s")

    def is_foreground(self):
        import win32gui

        return bool(self.handle) and win32gui.GetForegroundWindow() == self.handle

    def to_background(self):
        import win32con
        import win32gui

        win32gui.SetWindowPos(self.handle, win32con.HWND_BOTTOM, 0, 0, 0, 0,
                              win32con.SWP_NOMOVE | win32con.SWP_NOSIZE | win32con.SWP_NOACTIVATE)
        previous = self.previous_foreground
        if self.is_foreground() and previous and previous != self.handle and win32gui.IsWindow(previous):
            with contextlib.suppress(Exception):
                win32gui.SetForegroundWindow(previous)
        return "the client is at the bottom and " + ("still holds" if self.is_foreground() else "no longer holds") + " the foreground"

    def client_box(self, origin):
        import win32gui

        left, top = win32gui.ClientToScreen(self.handle, (0, 0))
        _x, _y, width, height = win32gui.GetClientRect(self.handle)
        return (left - origin[0], top - origin[1], left - origin[0] + width, top - origin[1] + height)

    def frame(self, attempts=FRAME_ATTEMPTS):
        import win32gui

        for attempt in range(1, attempts + 1):
            if win32gui.IsIconic(self.handle):
                raise StepFailed("the client window is minimized, so it draws nothing")
            try:
                picture = self.frame_from_surface()
                self.frame_source = "the composited window surface"
                return picture
            except Exception as error:
                try:
                    picture = self.frame_from_printwindow()
                    self.frame_source = f"PrintWindow, because the window surface gave nothing: {error}"
                    return picture
                except StepFailed:
                    if attempt == attempts:
                        raise
                    time.sleep(FRAME_RETRY_SECONDS)

    def frame_from_surface(self, timeout=5):
        import ctypes.wintypes

        import numpy
        from windows_capture import WindowsCapture

        taken = {}
        done = threading.Event()
        session = WindowsCapture(cursor_capture=False, draw_border=False, window_hwnd=self.handle)

        @session.event
        def on_frame_arrived(frame, control):
            if "buffer" not in taken:
                array = numpy.array(frame.frame_buffer, copy=True)
                taken["buffer"] = array.tobytes()
                taken["size"] = (array.shape[1], array.shape[0])
            done.set()
            control.stop()

        @session.event
        def on_closed():
            done.set()

        control = session.start_free_threaded()
        done.wait(timeout)
        with contextlib.suppress(Exception):
            control.stop()
        with contextlib.suppress(Exception):
            control.wait()
        if "buffer" not in taken:
            raise StepFailed("no frame arrived from the window surface")
        width, height = taken["size"]
        bounds = ctypes.wintypes.RECT()
        ctypes.windll.dwmapi.DwmGetWindowAttribute(self.handle, DWMWA_EXTENDED_FRAME_BOUNDS, ctypes.byref(bounds),
                                                   ctypes.sizeof(bounds))
        if (bounds.right - bounds.left, bounds.bottom - bounds.top) != (width, height):
            raise StepFailed(f"the frame is {width}x{height} and the window is "
                             f"{bounds.right - bounds.left}x{bounds.bottom - bounds.top}")
        whole = screens.from_bgra(taken["buffer"], width, height)
        return whole.crop(self.client_box((bounds.left, bounds.top)))

    def frame_from_printwindow(self):
        import win32gui
        import win32ui

        left, top, right, bottom = win32gui.GetWindowRect(self.handle)
        width, height = right - left, bottom - top
        device = win32gui.GetWindowDC(self.handle)
        source = win32ui.CreateDCFromHandle(device)
        memory = source.CreateCompatibleDC()
        bitmap = win32ui.CreateBitmap()
        bitmap.CreateCompatibleBitmap(source, width, height)
        memory.SelectObject(bitmap)
        try:
            if not ctypes.windll.user32.PrintWindow(self.handle, memory.GetSafeHdc(), PW_RENDERFULLCONTENT):
                raise StepFailed("PrintWindow copied nothing out of the window")
            bits = bitmap.GetBitmapBits(True)
        finally:
            win32gui.DeleteObject(bitmap.GetHandle())
            memory.DeleteDC()
            source.DeleteDC()
            win32gui.ReleaseDC(self.handle, device)
        whole = screens.from_bgra(bits, width, height)
        picture = whole.crop(self.client_box((left, top)))
        if screens.is_blank(picture):
            raise StepFailed("PrintWindow returned a blank frame")
        return picture

    def screenshot(self, path, picture=None):
        picture = self.frame() if picture is None else picture
        screens.save_png(picture, path)
        return picture

    def post_char(self, code):
        import win32api
        import win32con
        import win32gui

        wait_until_released(modifiers_held, "a modifier key")
        key = win32api.VkKeyScan(chr(code)) & 0xFF if 32 <= code < 127 else {8: 0x08, 9: 0x09, 13: 0x0D, 27: 0x1B}.get(code, 0)
        scan = win32api.MapVirtualKey(key, 0) if key else 0
        win32gui.PostMessage(self.handle, win32con.WM_CHAR, code, 1 | (scan << 16))

    def type(self, text, delay=0.02):
        for character in text:
            if ord(character) >= 127:
                raise StepFailed("the client's message loop is ANSI, so the driver types ASCII only")
            self.post_char(ord(character))
            time.sleep(delay)

    def key(self, virtual_key, hold=0.05):
        import win32api
        import win32con
        import win32gui

        if virtual_key == win32con.VK_RETURN:
            raise StepFailed("send Enter as character 13: a VK_RETURN key-up with Alt held switches the client to fullscreen")
        wait_until_released(modifiers_held, "a modifier key")
        scan = win32api.MapVirtualKey(virtual_key, 0)
        extended = 1 << 24 if virtual_key in EXTENDED_KEYS else 0
        win32gui.PostMessage(self.handle, win32con.WM_KEYDOWN, virtual_key, 1 | (scan << 16) | extended)
        time.sleep(hold)
        win32gui.PostMessage(self.handle, win32con.WM_KEYUP, virtual_key, 1 | (scan << 16) | extended | (3 << 30))

    @contextlib.contextmanager
    def cursor_at(self, x, y):
        import win32api
        import win32gui

        saved = win32api.GetCursorPos()
        win32api.SetCursorPos(win32gui.ClientToScreen(self.handle, (x, y)))
        try:
            yield
        finally:
            win32api.SetCursorPos(saved)

    @contextlib.contextmanager
    def activated(self):
        import win32con
        import win32gui

        previous = win32gui.GetForegroundWindow()
        got = True if previous == self.handle else force_foreground(self.handle)
        win32gui.SetWindowPos(self.handle, win32con.HWND_TOP, 0, 0, 0, 0, win32con.SWP_NOMOVE | win32con.SWP_NOSIZE)
        try:
            yield got
        finally:
            if previous and previous != self.handle and win32gui.IsWindow(previous):
                force_foreground(previous)
            win32gui.SetWindowPos(self.handle, win32con.HWND_BOTTOM, 0, 0, 0, 0,
                                  win32con.SWP_NOMOVE | win32con.SWP_NOSIZE | win32con.SWP_NOACTIVATE)

    def click(self, x, y, dwell=0.35):
        import win32con
        import win32gui

        wait_until_released(modifiers_held, "a modifier key")
        wait_until_released(buttons_held, "a mouse button")
        position = (y << 16) | (x & 0xFFFF)

        def send(message, wparam):
            win32gui.SendMessageTimeout(self.handle, message, wparam, position, SMTO_ABORTIFHUNG, 3000)

        with self.activated() as active, self.cursor_at(x, y):
            send(win32con.WM_MOUSEMOVE, 0)
            time.sleep(dwell)
            send(win32con.WM_LBUTTONDOWN, MK_LBUTTON)
            time.sleep(dwell)
            send(win32con.WM_LBUTTONUP, 0)
            time.sleep(0.2)
        return f"{x},{y} after {dwell:.2f}s with the window " + ("active" if active else "NOT active"), active

    def close(self, timeout=60, force=False):
        import psutil
        import win32con
        import win32gui

        from .netguard import kill_tree

        if not self.alive():
            return self._stop_launcher("the client had already stopped")
        if force:
            kill_tree(self.pid)
            return self._stop_launcher("the client was ended without its own quit path, which would have opened a page "
                                       "outside the machine from the screen it was left on")
        if self.handle and win32gui.IsWindow(self.handle):
            win32gui.PostMessage(self.handle, win32con.WM_SYSCOMMAND, win32con.SC_CLOSE, 0)
        try:
            code = psutil.Process(self.pid).wait(timeout)
            how = f"closed cleanly with {code}"
        except psutil.TimeoutExpired:
            kill_tree(self.pid)
            how = "killed, because it did not close in time"
        except psutil.Error:
            how = "gone"
        return self._stop_launcher(f"the client was {how}")

    def _stop_launcher(self, said):
        from .netguard import kill_tree

        if self.launcher_process is not None:
            try:
                self.launcher_process.wait(30)
            except subprocess.TimeoutExpired:
                kill_tree(self.launcher_process.pid)
                said += ", and the launcher was killed"
            else:
                said += f", and the launcher exited with {self.launcher_process.returncode}"
        if self._output:
            self._output.close()
            self._output = None
        self.describe()
        return said
