#!/usr/bin/env python3
"""
╔══════════════════════════════════════════════════════════════╗
║   AeroSniffer — PC Desktop Agent                            ║
║   Monitors PC events → sends face commands to ESP32         ║
║   via USB Serial (115200 baud)                              ║
╠══════════════════════════════════════════════════════════════╣
║   Install:                                                  ║
║     pip install pyserial psutil pynput                      ║
║   Windows extra:  pip install pygetwindow                   ║
║   Linux extra:    sudo apt install xdotool                  ║
╚══════════════════════════════════════════════════════════════╝
"""

import serial
import serial.tools.list_ports
import psutil
import threading
import time
import sys
import os
import json
import re
import platform
import subprocess
from datetime import datetime

# ── Platform detection ─────────────────────────────────────────
IS_WIN   = sys.platform == "win32"
IS_MAC   = sys.platform == "darwin"
IS_LINUX = sys.platform.startswith("linux")

# ── Try importing pynput for keyboard monitoring ───────────────
try:
    from pynput import keyboard as kb
    PYNPUT_OK = True
except ImportError:
    PYNPUT_OK = False
    print("[WARN] pynput not installed — keyboard monitoring disabled")
    print("       Run: pip install pynput")

# ── Try platform-specific window libs ─────────────────────────
if IS_WIN:
    try:
        import ctypes
        import ctypes.wintypes
        WIN32_OK = True
    except ImportError:
        WIN32_OK = False
else:
    WIN32_OK = False

# ==============================================================
#  CONFIG  — edit this section to customise your experience
# ==============================================================
CONFIG = {
    # Serial settings
    "baud": 115200,
    "auto_detect": True,          # Auto-find ESP32 port
    "port": None,                 # Set manually if auto fails, e.g. "COM3" or "/dev/ttyACM0"

    # Timing (seconds)
    "idle_sleep_secs": 300,       # System idle → SLEEPING  (5 min)
    "typing_cooldown": 2.5,       # Secs after last keypress before "stopped typing"
    "cpu_panic_pct": 88,          # CPU % that triggers PANIC face
    "cpu_panic_secs": 6,          # Must be sustained for this long
    "status_update_hz": 2,        # How often to push status to ESP32 (per second)
    "window_poll_secs": 1.0,      # How often to check active window

    # App → Face mapping  (keyword in window title, case-insensitive)
    # Face choices: IDLE HAPPY SAD ANGRY SURPRISED THINKING
    #               TYPING SLEEPING CODING VIBING PANIC NOTIFICATION
    "app_faces": {
        # IDEs & Code Editors
        "visual studio code":  "CODING",
        "vs code":             "CODING",
        "pycharm":             "CODING",
        "arduino":             "CODING",
        "intellij":            "CODING",
        "android studio":      "CODING",
        "sublime text":        "CODING",
        "vim":                 "THINKING",
        "nvim":                "THINKING",
        "notepad++":           "CODING",

        # Terminals
        "terminal":            "CODING",
        "powershell":          "CODING",
        "cmd.exe":             "CODING",
        "bash":                "CODING",
        "command prompt":      "CODING",
        "windows terminal":    "CODING",
        "iterm":               "CODING",
        "hyper":               "CODING",

        # Video Calls
        "zoom":                "NOTIFICATION",
        "microsoft teams":     "THINKING",
        "google meet":         "THINKING",
        "webex":               "THINKING",
        "discord":             "HAPPY",
        "skype":               "NOTIFICATION",

        # Music / Media
        "spotify":             "VIBING",
        "youtube":             "HAPPY",
        "vlc":                 "HAPPY",
        "netflix":             "HAPPY",
        "prime video":         "HAPPY",
        "apple music":         "VIBING",
        "soundcloud":          "VIBING",

        # Gaming
        "steam":               "HAPPY",
        "epic games":          "HAPPY",
        "battle.net":          "HAPPY",
        "minecraft":           "SURPRISED",
        "roblox":              "HAPPY",

        # Email / Messaging
        "outlook":             "NOTIFICATION",
        "gmail":               "NOTIFICATION",
        "mail":                "NOTIFICATION",
        "slack":               "NOTIFICATION",
        "telegram":            "HAPPY",
        "whatsapp":            "HAPPY",

        # Browsers (default — will be overridden by typing detection)
        "google chrome":       "IDLE",
        "mozilla firefox":     "IDLE",
        "microsoft edge":      "IDLE",
        "safari":              "IDLE",
        "brave":               "IDLE",
        "opera":               "IDLE",

        # Productivity
        "notion":              "THINKING",
        "obsidian":            "THINKING",
        "figma":               "THINKING",
        "photoshop":           "THINKING",
        "blender":             "THINKING",
        "excel":               "THINKING",
        "word":                "TYPING",
        "powerpoint":          "THINKING",

        # File managers
        "explorer":            "IDLE",
        "finder":              "IDLE",
        "files":               "IDLE",
    },

    # Status messages shown on ESP32 screen for each face
    "face_status": {
        "IDLE":         "",
        "HAPPY":        "Let's go!",
        "SAD":          "Something wrong...",
        "ANGRY":        "Not happy...",
        "SURPRISED":    "Whoa!",
        "THINKING":     "Hmm...",
        "TYPING":       "Typing...",
        "SLEEPING":     "Zzz...",
        "CODING":       "{ coding... }",
        "VIBING":       "vibing~",
        "PANIC":        "!! CPU SPIKE !!",
        "NOTIFICATION": "You got mail!",
    },
}

# ==============================================================
#  UTILITIES
# ==============================================================
RESET  = "\033[0m"
CYAN   = "\033[96m"
GREEN  = "\033[92m"
YELLOW = "\033[93m"
RED    = "\033[91m"
DIM    = "\033[2m"
BOLD   = "\033[1m"

def log(msg, color=RESET):
    ts = datetime.now().strftime("%H:%M:%S")
    print(f"{DIM}[{ts}]{RESET} {color}{msg}{RESET}")

def log_face(face, reason):
    colors = {
        "IDLE":"","HAPPY":GREEN,"SAD":CYAN,"ANGRY":RED,
        "SURPRISED":YELLOW,"THINKING":CYAN,"TYPING":GREEN,
        "SLEEPING":DIM,"CODING":GREEN,"VIBING":CYAN,
        "PANIC":RED,"NOTIFICATION":YELLOW
    }
    col = colors.get(face, "")
    log(f"  face → {col}{BOLD}{face:14}{RESET}  {DIM}({reason}){RESET}")

# ==============================================================
#  SERIAL MANAGER  — auto-connect, reconnect, send commands
# ==============================================================
class SerialManager:
    def __init__(self):
        self.ser        = None
        self.connected  = False
        self.lock       = threading.Lock()
        self._stop      = False
        self._thread    = threading.Thread(target=self._keep_alive, daemon=True)
        self._thread.start()

    def _find_port(self):
        """Auto-detect the XIAO ESP32S3 serial port."""
        # Keywords that appear in ESP32S3 port descriptions
        keywords = ["xiao", "esp32", "ch340", "ch9102", "cp210", "ftdi", "usb serial", "cdc"]
        ports = serial.tools.list_ports.comports()
        for p in ports:
            desc = (p.description or "").lower()
            hw   = (p.hwid or "").lower()
            if any(k in desc or k in hw for k in keywords):
                log(f"Found ESP32: {p.device}  ({p.description})", GREEN)
                return p.device
        # Fallback: pick any USB serial
        for p in ports:
            if "usb" in (p.description or "").lower():
                log(f"Trying USB port: {p.device}", YELLOW)
                return p.device
        return None

    def _connect(self):
        port = CONFIG["port"] or self._find_port()
        if not port:
            log("No ESP32 found. Plug in USB-C and retry...", RED)
            return False
        try:
            self.ser = serial.Serial(port, CONFIG["baud"], timeout=0.1)
            time.sleep(1.5)  # Give ESP32 time to reset
            self.connected = True
            log(f"Connected → {port}  @{CONFIG['baud']} baud", GREEN)
            return True
        except serial.SerialException as e:
            log(f"Serial error: {e}", RED)
            return False

    def _keep_alive(self):
        """Background thread: reconnect if disconnected."""
        while not self._stop:
            if not self.connected:
                self._connect()
            else:
                # Check connection is still alive
                try:
                    if self.ser and self.ser.in_waiting > 0:
                        data = self.ser.readline().decode("utf-8", errors="ignore").strip()
                        if data:
                            log(f"  ESP32 → {data}", DIM)
                except (serial.SerialException, OSError):
                    log("Serial disconnected. Reconnecting...", YELLOW)
                    self.connected = False
                    if self.ser:
                        try: self.ser.close()
                        except: pass
            time.sleep(2)

    def send(self, command: str):
        """Thread-safe send a command string + newline to ESP32."""
        if not self.connected or not self.ser:
            return False
        try:
            with self.lock:
                self.ser.write((command + "\n").encode("utf-8"))
                self.ser.flush()
            return True
        except (serial.SerialException, OSError):
            self.connected = False
            return False

    def stop(self):
        self._stop = True
        if self.ser:
            try: self.ser.close()
            except: pass

# ==============================================================
#  ACTIVE WINDOW DETECTION  — cross-platform
# ==============================================================
class WindowMonitor:
    def get_active_title(self) -> str:
        """Return the active window title as a lowercase string."""
        try:
            if IS_WIN:
                return self._get_win()
            elif IS_MAC:
                return self._get_mac()
            elif IS_LINUX:
                return self._get_linux()
        except Exception:
            pass
        return ""

    def _get_win(self) -> str:
        user32 = ctypes.windll.user32
        hwnd = user32.GetForegroundWindow()
        length = user32.GetWindowTextLengthW(hwnd)
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buf, length + 1)
        return buf.value.lower()

    def _get_mac(self) -> str:
        script = 'tell application "System Events" to get name of first application process whose frontmost is true'
        result = subprocess.run(
            ["osascript", "-e", script],
            capture_output=True, text=True, timeout=2
        )
        title_script = 'tell application "System Events" to get title of front window of (first application process whose frontmost is true)'
        title_result = subprocess.run(
            ["osascript", "-e", title_script],
            capture_output=True, text=True, timeout=2
        )
        app  = result.stdout.strip().lower()
        title = title_result.stdout.strip().lower()
        return f"{app} {title}"

    def _get_linux(self) -> str:
        wid = subprocess.run(
            ["xdotool", "getactivewindow"],
            capture_output=True, text=True, timeout=2
        )
        if wid.returncode != 0:
            return ""
        window_id = wid.stdout.strip()
        name = subprocess.run(
            ["xdotool", "getwindowname", window_id],
            capture_output=True, text=True, timeout=2
        )
        cls = subprocess.run(
            ["xdotool", "getwindowclassname", window_id],
            capture_output=True, text=True, timeout=2
        )
        return f"{name.stdout.strip()} {cls.stdout.strip()}".lower()

# ==============================================================
#  KEYBOARD MONITOR  — tracks typing activity
# ==============================================================
class KeyboardMonitor:
    def __init__(self):
        self.last_keypress = 0.0
        self.is_typing     = False
        self._listener     = None

    def start(self):
        if not PYNPUT_OK:
            return
        def on_press(key):
            self.last_keypress = time.time()
        self._listener = kb.Listener(on_press=on_press, daemon=True)
        self._listener.start()
        log("Keyboard monitor active", GREEN)

    def stop(self):
        if self._listener:
            try: self._listener.stop()
            except: pass

    @property
    def typing(self) -> bool:
        return (time.time() - self.last_keypress) < CONFIG["typing_cooldown"]

# ==============================================================
#  SYSTEM MONITOR  — CPU, idle time
# ==============================================================
class SystemMonitor:
    def __init__(self):
        self._cpu_high_since = 0.0
        self._idle_since     = time.time()
        self._last_mouse_pos = self._get_mouse_pos()

    def _get_mouse_pos(self):
        try:
            if IS_WIN:
                pt = ctypes.wintypes.POINT()
                ctypes.windll.user32.GetCursorPos(ctypes.byref(pt))
                return (pt.x, pt.y)
        except: pass
        return (0, 0)

    @property
    def cpu_pct(self) -> float:
        return psutil.cpu_percent(interval=None)

    @property
    def ram_pct(self) -> float:
        return psutil.virtual_memory().percent

    @property
    def cpu_panic(self) -> bool:
        """True if CPU has been above threshold for sustained period."""
        pct = self.cpu_pct
        if pct >= CONFIG["cpu_panic_pct"]:
            if self._cpu_high_since == 0:
                self._cpu_high_since = time.time()
            return (time.time() - self._cpu_high_since) >= CONFIG["cpu_panic_secs"]
        else:
            self._cpu_high_since = 0
            return False

    def update_idle(self, keyboard_active: bool, window_changed: bool):
        """Update idle timer; reset on any activity."""
        mouse = self._get_mouse_pos()
        moved = (mouse != self._last_mouse_pos)
        self._last_mouse_pos = mouse
        if keyboard_active or window_changed or moved:
            self._idle_since = time.time()

    @property
    def idle_seconds(self) -> float:
        return time.time() - self._idle_since

    @property
    def is_idle(self) -> bool:
        return self.idle_seconds >= CONFIG["idle_sleep_secs"]

# ==============================================================
#  FACE CONTROLLER  — maps events → face commands
# ==============================================================
class FaceController:
    def __init__(self, serial_mgr: SerialManager):
        self.serial      = serial_mgr
        self.face        = "IDLE"
        self.last_face   = None
        self.last_window = ""
        self.last_app    = ""

    def _match_app_face(self, title: str) -> str:
        """Find the best matching face for the active window title."""
        for keyword, face in CONFIG["app_faces"].items():
            if keyword in title:
                return face
        return "IDLE"

    def update(self, window_title: str, typing: bool, sys_mon: SystemMonitor):
        """Decide current face based on all inputs. Returns True if changed."""

        # ── Priority 1: CPU panic ──────────────────────────────
        if sys_mon.cpu_panic:
            new_face = "PANIC"
            reason   = f"CPU {sys_mon.cpu_pct:.0f}%"

        # ── Priority 2: System idle ────────────────────────────
        elif sys_mon.is_idle:
            new_face = "SLEEPING"
            reason   = f"idle {sys_mon.idle_seconds/60:.1f}min"

        # ── Priority 3: Typing (overrides app face) ────────────
        elif typing:
            app_face = self._match_app_face(window_title)
            # If it's a coding app, show CODING even while typing
            new_face = "CODING" if app_face == "CODING" else "TYPING"
            reason   = "keyboard active"

        # ── Priority 4: App / window ───────────────────────────
        else:
            new_face = self._match_app_face(window_title)
            reason   = f'window: "{window_title[:30]}"' if window_title else "no window"

        # ── Extract short app name ─────────────────────────────
        short_app = self._short_app_name(window_title)

        # ── Send only if changed ───────────────────────────────
        changed = False
        if new_face != self.last_face:
            log_face(new_face, reason)
            status = CONFIG["face_status"].get(new_face, "")
            self.serial.send(f"FACE:{new_face}")
            if status:
                self.serial.send(f"STATUS:{status}")
            self.last_face = new_face
            self.face = new_face
            changed = True

        if short_app != self.last_app:
            self.serial.send(f"APP:{short_app}")
            self.last_app = short_app

        return changed

    def _short_app_name(self, title: str) -> str:
        """Extract a short recognisable app name from window title."""
        for keyword in CONFIG["app_faces"]:
            if keyword in title:
                return keyword.split()[0].title()
        # Try to grab first meaningful word
        words = title.replace("-", " ").replace("|", " ").split()
        for w in words:
            if len(w) > 2 and not w.lower() in {"the","and","for","with","from"}:
                return w[:14].title()
        return ""

# ==============================================================
#  NOTIFICATION WATCHER  — watches process list for alert apps
# ==============================================================
class NotificationWatcher:
    """Detects notification-related processes popping up."""

    ALERT_PROCS = [
        "toast", "notification", "alert", "popup",
        "teams", "slack", "telegram", "signal",
        "whatsapp", "discord"
    ]

    def __init__(self, serial_mgr: SerialManager):
        self.serial = serial_mgr
        self._known = set()

    def check(self):
        """Check for new notification processes."""
        current = {p.name().lower() for p in psutil.process_iter(["name"])}
        new_procs = current - self._known
        self._known = current

        for proc in new_procs:
            for keyword in self.ALERT_PROCS:
                if keyword in proc:
                    log(f"Notification process: {proc}", YELLOW)
                    # Don't override — just announce
                    break

# ==============================================================
#  MAIN AGENT LOOP
# ==============================================================
class PCAgent:
    def __init__(self):
        log(f"\n{BOLD}{'─'*54}", CYAN)
        log(f"  AeroSniffer PC Agent  ({platform.system()} {platform.release()})", CYAN)
        log(f"{'─'*54}{RESET}\n")

        self.serial     = SerialManager()
        self.window_mon = WindowMonitor()
        self.kbd_mon    = KeyboardMonitor()
        self.sys_mon    = SystemMonitor()
        self.face_ctrl  = FaceController(self.serial)
        self.notif_mon  = NotificationWatcher(self.serial)
        self._running   = False

        # Start keyboard listener
        self.kbd_mon.start()

    def run(self):
        self._running = True
        last_window_check = 0.0
        last_cpu_sample   = 0.0
        window_title      = ""
        prev_window       = ""

        log("Agent running. Press Ctrl+C to stop.\n")

        # Print header table
        print(f"  {'TIME':8}  {'FACE':14}  {'STATUS':20}  TRIGGER")
        print(f"  {'─'*8}  {'─'*14}  {'─'*20}  {'─'*20}")

        try:
            while self._running:
                now = time.time()

                # ── Poll active window ─────────────────────────
                if now - last_window_check >= CONFIG["window_poll_secs"]:
                    last_window_check = now
                    try:
                        window_title = self.window_mon.get_active_title()
                    except Exception:
                        window_title = ""
                    window_changed = (window_title != prev_window)
                    prev_window    = window_title

                    # Update idle tracker
                    self.sys_mon.update_idle(
                        self.kbd_mon.typing,
                        window_changed
                    )
                else:
                    window_changed = False

                # ── Update face ────────────────────────────────
                self.face_ctrl.update(
                    window_title,
                    self.kbd_mon.typing,
                    self.sys_mon
                )

                # ── Push CPU/RAM telemetry every 5s ──────────
                if now - last_cpu_sample >= 5.0:
                    last_cpu_sample = now
                    cpu = psutil.cpu_percent(interval=0.1)
                    ram = psutil.virtual_memory().percent
                    self.serial.send(f"TELEMETRY:cpu={cpu:.0f},ram={ram:.0f}")

                # ── Check notifications ────────────────────────
                self.notif_mon.check()

                time.sleep(1.0 / CONFIG["status_update_hz"])

        except KeyboardInterrupt:
            log("\nStopping...", YELLOW)
        finally:
            self.stop()

    def stop(self):
        self._running = False
        self.kbd_mon.stop()
        self.serial.send("FACE:IDLE")
        time.sleep(0.3)
        self.serial.stop()
        log("Agent stopped.", DIM)


# ==============================================================
#  ENTRY POINT
# ==============================================================
if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="AeroSniffer PC Agent — sends face commands to ESP32"
    )
    parser.add_argument("--port",  help="Serial port (e.g. COM3, /dev/ttyACM0)")
    parser.add_argument("--list",  action="store_true", help="List available serial ports and exit")
    parser.add_argument("--test",  action="store_true", help="Test mode: cycle through all faces")
    args = parser.parse_args()

    if args.list:
        print("\nAvailable serial ports:")
        for p in serial.tools.list_ports.comports():
            print(f"  {p.device:20}  {p.description}")
        sys.exit(0)

    if args.port:
        CONFIG["port"] = args.port

    if args.test:
        # Test mode: send each face in sequence so you can see them all
        log("TEST MODE — cycling all faces", YELLOW)
        mgr = SerialManager()
        time.sleep(2)
        faces = ["IDLE","HAPPY","SAD","ANGRY","SURPRISED","THINKING",
                 "TYPING","SLEEPING","CODING","VIBING","PANIC","NOTIFICATION"]
        for face in faces:
            log(f"  → {face}", CYAN)
            mgr.send(f"FACE:{face}")
            mgr.send(f"STATUS:{CONFIG['face_status'].get(face,'')}")
            time.sleep(3)
        mgr.send("FACE:IDLE")
        mgr.stop()
        log("Test complete.", GREEN)
        sys.exit(0)

    PCAgent().run()
