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
import sqlite3
import shutil
import tempfile
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
    # Face choices: IDLE HAPPY EXCITED SLEEPY THINKING SAD_ERROR ALERT_WARNING LOVE_BONDING STARTUP_BOOT SURPRISED
    "app_faces": {
        # IDEs & Code Editors
        "visual studio code":  "THINKING",
        "vs code":             "THINKING",
        "pycharm":             "THINKING",
        "arduino":             "THINKING",
        "intellij":            "THINKING",
        "android studio":      "THINKING",
        "sublime text":        "THINKING",
        "vim":                 "THINKING",
        "nvim":                "THINKING",
        "notepad++":           "THINKING",

        # Terminals
        "terminal":            "THINKING",
        "powershell":          "THINKING",
        "cmd.exe":             "THINKING",
        "bash":                "THINKING",
        "command prompt":      "THINKING",
        "windows terminal":    "THINKING",
        "iterm":               "THINKING",
        "hyper":               "THINKING",

        # Video Calls
        "zoom":                "ALERT_WARNING",
        "microsoft teams":     "THINKING",
        "google meet":         "THINKING",
        "webex":               "THINKING",
        "discord":             "HAPPY",
        "skype":               "ALERT_WARNING",

        # Music / Media
        "spotify":             "LOVE_BONDING",
        "youtube":             "HAPPY",
        "vlc":                 "HAPPY",
        "netflix":             "HAPPY",
        "prime video":         "HAPPY",
        "apple music":         "LOVE_BONDING",
        "soundcloud":          "LOVE_BONDING",

        # Gaming
        "steam":               "EXCITED",
        "epic games":          "EXCITED",
        "battle.net":          "EXCITED",
        "minecraft":           "SURPRISED",
        "roblox":              "EXCITED",

        # Email / Messaging
        "outlook":             "ALERT_WARNING",
        "gmail":               "ALERT_WARNING",
        "mail":                "ALERT_WARNING",
        "slack":               "ALERT_WARNING",
        "telegram":            "HAPPY",
        "whatsapp":            "HAPPY",

        # Browsers (default — will be overridden by typing detection)
        "google chrome":       "IDLE",
        "mozilla firefox":     "IDLE",
        "microsoft edge":      "IDLE",
        "safari":              "IDLE",
        "brave":               "IDLE",
        "opera":               "IDLE",

        # Social & Web
        "github":              "LOVE_BONDING",
        "linkedin":            "THINKING",
        "reddit":              "SURPRISED",
        "chatgpt":             "LOVE_BONDING",
        "stackoverflow":       "SAD_ERROR",

        # Productivity & Docs
        "notion":              "THINKING",
        "obsidian":            "THINKING",
        "figma":               "THINKING",
        "photoshop":           "THINKING",
        "blender":             "THINKING",
        "excel":               "THINKING",
        "word":                "THINKING",
        "powerpoint":          "THINKING",
        "pdf":                 "THINKING",
        "acrobat":             "THINKING",

        # File managers
        "explorer":            "IDLE",
        "finder":              "IDLE",
        "files":               "IDLE",
    },

    # Status messages shown on ESP32 screen for each face
    "face_status": {
        "IDLE":          "",
        "HAPPY":         "Let's go!",
        "EXCITED":       "Wooo!",
        "SLEEPY":        "Zzz...",
        "THINKING":      "Hmm...",
        "SAD_ERROR":     "Something wrong...",
        "ALERT_WARNING": "Warning!",
        "LOVE_BONDING":  "<3",
        "STARTUP_BOOT":  "Booting...",
        "SURPRISED":     "Whoa!",
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
        "IDLE":"","HAPPY":GREEN,"EXCITED":YELLOW,"SLEEPY":DIM,
        "THINKING":CYAN,"SAD_ERROR":RED,"ALERT_WARNING":YELLOW,
        "LOVE_BONDING":CYAN,"STARTUP_BOOT":GREEN,"SURPRISED":YELLOW
    }
    col = colors.get(face, "")
    log(f"  face -> {col}{BOLD}{face:14}{RESET}  {DIM}({reason}){RESET}")

# ==============================================================
#  SERIAL MANAGER  — auto-connect, reconnect, send commands
# ==============================================================
class SerialManager:
    def __init__(self):
        self.ser        = None
        self.connected  = False
        self.needs_resend = False
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
            log(f"Connected -> {port}  @{CONFIG['baud']} baud", GREEN)
            return True
        except serial.SerialException as e:
            log(f"Serial error: {e}", RED)
            return False

    def _keep_alive(self):
        """Background thread: reconnect if disconnected."""
        while not self._stop:
            if not self.connected:
                if self._connect():
                    self.needs_resend = True
            else:
                # Check connection is still alive
                try:
                    if self.ser and self.ser.in_waiting > 0:
                        data = self.ser.readline().decode("utf-8", errors="ignore").strip()
                        if data:
                            log(f"  ESP32 -> {data}", DIM)
                            self.needs_resend = True
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

    @property
    def battery_status(self) -> str:
        """Return BAT_LOW, BAT_CHARGING, BAT_FULL or empty string."""
        if not hasattr(psutil, "sensors_battery"):
            return ""
        bat = psutil.sensors_battery()
        if not bat:
            return ""
        if bat.power_plugged:
            if bat.percent >= 99:
                return "BAT_FULL"
            return "BAT_CHARGING"
        elif bat.percent < 20:
            return "BAT_LOW"
        return ""

# ==============================================================
#  MOOD ENGINE  — Advanced character state
# ==============================================================
class MoodEngine:
    def __init__(self, serial_mgr: SerialManager, agent):
        self.serial      = serial_mgr
        self.agent       = agent
        self.happiness   = 50.0
        self.energy      = 80.0
        self.focus       = 50.0
        
        self.last_face   = None
        self.last_app    = ""
        self.last_status = None
        self.last_update = time.time()
        self.last_particle_time = 0
        
        # Priority Event variables
        self.event_type     = None      # e.g. "notif", "battery", "app_change", "typing"
        self.event_priority = 0         # 1 to 5
        self.event_end      = 0.0       # timestamp when it expires
        self.event_face     = "IDLE"
        self.event_status   = ""
        
        # State tracking for battery transitions
        self.last_power_plugged = None
        self.last_battery_percent = None
        self.last_cpu_panic = False
        self.last_short_app = ""

    def trigger_event(self, event_type: str, priority: int, duration: float, face: str, status: str):
        """Trigger an event if it has higher or equal priority to the current active event, or if current has expired."""
        now = time.time()
        if priority >= self.event_priority or now >= self.event_end:
            self.event_type = event_type
            self.event_priority = priority
            self.event_end = now + duration
            self.event_face = face
            self.event_status = status
            
            # Print to local agent console table format
            time_str = datetime.now().strftime("%H:%M:%S")
            print(f"  {time_str:8}  {face:14}  {status[:20]:20}  {event_type.upper()}")

    def update(self, window_title: str, typing: bool, sys_mon: SystemMonitor):
        now = time.time()
        dt = now - self.last_update
        self.last_update = now

        if self.serial.needs_resend:
            self.last_face   = None
            self.last_status = None
            self.last_app    = ""
            self.serial.needs_resend = False

        # ── Decay & Recovery over time ─────────────────────
        if sys_mon.is_idle:
            self.energy -= dt * 0.5
            self.focus -= dt * 1.0
        else:
            self.energy -= dt * 0.1
            
        if typing:
            self.focus += dt * 2.0
            self.energy += dt * 0.5
            
        # ── App-specific mood modifiers ────────────────────
        short_app = self._short_app_name(window_title)
        title_lower = window_title.lower()
        
        is_coding = "code" in title_lower or "terminal" in title_lower or "pycharm" in title_lower or "github" in title_lower
        is_gaming = "steam" in title_lower or "epic" in title_lower or "minecraft" in title_lower
        is_music = "spotify" in title_lower or "music" in title_lower
        
        if is_coding:
            self.focus += dt * 1.0
        if is_gaming:
            self.happiness += dt * 1.0
            self.focus -= dt * 0.5
        if is_music:
            self.happiness += dt * 0.5
            
        # ── Battery & CPU stats impact ──────────────────────
        if hasattr(psutil, "sensors_battery"):
            bat = psutil.sensors_battery()
            if bat:
                plugged = bat.power_plugged
                percent = bat.percent
                
                # Check for changes in plug status (Priority 4)
                if self.last_power_plugged is not None and plugged != self.last_power_plugged:
                    if plugged:
                        self.trigger_event("charging", 4, 10.0, "EXCITED", "Charger Connected")
                    else:
                        self.trigger_event("battery", 4, 10.0, "SURPRISED", "Battery Power")
                
                # Check for full / low transitions (Priority 4)
                elif self.last_battery_percent is not None:
                    if percent >= 99 and self.last_battery_percent < 99 and plugged:
                        self.trigger_event("battery_full", 4, 10.0, "HAPPY", "Fully Charged!")
                    elif percent < 20 and self.last_battery_percent >= 20:
                        self.trigger_event("battery_low", 4, 10.0, "SAD_ERROR", f"Battery Low! ({percent}%)")
                
                self.last_power_plugged = plugged
                self.last_battery_percent = percent
                
                # Apply continuous stat decay/buff
                if not plugged and percent < 20:
                    self.energy -= dt * 5.0
                    self.happiness -= dt * 2.0
                elif plugged:
                    self.energy += dt * 5.0
            
        if sys_mon.cpu_panic:
            self.energy -= dt * 2.0
            self.happiness -= dt * 2.0
            if not self.last_cpu_panic:
                self.trigger_event("cpu_panic", 4, 8.0, "ALERT_WARNING", "CPU Hot!")
            self.last_cpu_panic = True
        else:
            self.last_cpu_panic = False

        # App focus changes (Priority 3)
        if short_app != self.last_short_app and short_app != "":
            app_face = None
            for keyword, face in CONFIG["app_faces"].items():
                if keyword in title_lower:
                    app_face = face
                    break
            face = app_face if app_face else "HAPPY" if is_gaming else "IDLE"
            self.trigger_event("app_change", 3, 8.0, face, f"Using {short_app}")
            self.last_short_app = short_app

        # Active typing (Priority 2)
        if typing:
            self.trigger_event("typing", 2, 3.0, "HAPPY", "Typing...")

        # Clamp stats 0-100
        self.happiness = max(0, min(100, self.happiness))
        self.energy = max(0, min(100, self.energy))
        self.focus = max(0, min(100, self.focus))

        # ── Trigger Particles ──────────────────────────────
        if now - self.last_particle_time > 2.0:
            if is_music:
                self.serial.send("PARTICLE:MUSIC")
                self.last_particle_time = now
            elif sys_mon.cpu_panic:
                self.serial.send("PARTICLE:SWEAT")
                self.last_particle_time = now
            elif self.focus > 90 and typing:
                self.serial.send("PARTICLE:HEART")
                self.last_particle_time = now

        # ── Determine Face and Status based on Active Event ─
        if now < self.event_end:
            new_face = self.event_face
            status = self.event_status
            reason = f"event: {self.event_type}"
        else:
            # Clear expired events
            self.event_type = None
            self.event_priority = 0
            
            # Priority 1: Default / Idle state showing Time & Weather!
            time_str = datetime.now().strftime("%I:%M %p")
            if self.agent.weather_info:
                status = f"{time_str} | {self.agent.weather_info}"
            else:
                status = time_str
                
            # Determine base face when idle
            if self.energy < 20 or sys_mon.is_idle:
                new_face = "SLEEPY"
                reason = "idle sleep"
            elif self.focus > 80:
                new_face = "THINKING"
                reason = "idle focus"
            else:
                new_face = "IDLE"
                reason = "idle default"

        # ── Send to ESP32 ──────────────────────────────────
        if new_face != self.last_face:
            log(f"  stats -> H:{self.happiness:.0f} E:{self.energy:.0f} F:{self.focus:.0f}", DIM)
            log_face(new_face, reason)
            self.serial.send(f"FACE:{new_face}")
            self.last_face = new_face
            
        if status != self.last_status:
            self.serial.send(f"STATUS:{status}")
            self.last_status = status
            
        if short_app != self.last_app:
            self.serial.send(f"APP:{short_app}")
            self.last_app = short_app

        return True

    def _short_app_name(self, title: str) -> str:
        for keyword in CONFIG["app_faces"]:
            if keyword in title:
                return keyword.split()[0].title()
        words = title.replace("-", " ").replace("|", " ").split()
        for w in words:
            if len(w) > 2 and not w.lower() in {"the","and","for","with","from"}:
                return w[:14].title()
        return ""


# ==============================================================
#  NOTIFICATION WATCHER  — watches process list & notifications
# ==============================================================
class NotificationWatcher:
    def __init__(self, mood_engine: MoodEngine):
        self.mood = mood_engine
        self.last_check = 0.0
        self.max_order = 0
        self.last_unread_counts = {}
        self.db_path = os.path.expandvars(r'%LOCALAPPDATA%\Microsoft\Windows\Notifications\wpndatabase.db')
        
        # Initialize max_order on startup so we only catch NEW notifications
        if IS_WIN and os.path.exists(self.db_path):
            try:
                self.max_order = self._get_max_order()
                log(f"Notification DB sniffer initialized (Base Order: {self.max_order})", GREEN)
            except Exception as e:
                log(f"Notification DB init failed: {e}", YELLOW)

    def _get_max_order(self) -> int:
        if not os.path.exists(self.db_path):
            return 0
        temp_dir = tempfile.gettempdir()
        temp_copy = os.path.join(temp_dir, "wpn_init_poll.db")
        shutil.copy2(self.db_path, temp_copy)
        conn = sqlite3.connect(temp_copy)
        cursor = conn.cursor()
        cursor.execute("SELECT MAX([Order]) FROM Notification;")
        val = cursor.fetchone()[0] or 0
        conn.close()
        try: os.remove(temp_copy)
        except: pass
        return val

    def check_window_titles(self):
        """Scan open windows to find background messaging apps with unread notification counters."""
        unreads = []
        if not IS_WIN:
            return unreads
            
        def enum_cb(hwnd, lParam):
            if ctypes.windll.user32.IsWindowVisible(hwnd):
                length = ctypes.windll.user32.GetWindowTextLengthW(hwnd)
                if length > 0:
                    buff = ctypes.create_unicode_buffer(length + 1)
                    ctypes.windll.user32.GetWindowTextW(hwnd, buff, length + 1)
                    title = buff.value
                    if title:
                        # Match pattern like (1) WhatsApp, (3) Discord, Gmail (2)
                        match = re.search(r'\((\d+)\)', title)
                        if match:
                            count = match.group(1)
                            t_lower = title.lower()
                            app = None
                            if "discord" in t_lower: app = "Discord"
                            elif "whatsapp" in t_lower: app = "WhatsApp"
                            elif "slack" in t_lower: app = "Slack"
                            elif "teams" in t_lower: app = "Teams"
                            elif "telegram" in t_lower: app = "Telegram"
                            elif "gmail" in t_lower: app = "Gmail"
                            elif "outlook" in t_lower: app = "Outlook"
                            
                            if app:
                                unreads.append((app, int(count)))
            return True
            
        try:
            EnumWindows = ctypes.windll.user32.EnumWindows
            EnumWindowsProc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_int, ctypes.c_int)
            EnumWindows(EnumWindowsProc(enum_cb), 0)
        except Exception as e:
            pass
            
        return unreads

    def check(self):
        now = time.time()
        # Check notifications every 2.5 seconds to limit disk/CPU impact
        if now - self.last_check < 2.5:
            return
        self.last_check = now
        
        # 1. Check Window Titles for unread badges
        try:
            unreads = self.check_window_titles()
            for app, count in unreads:
                last = self.last_unread_counts.get(app, 0)
                if count > last:
                    msg = f"{app}: {count} Unread Msg"
                    face = "HAPPY" if app in ["Discord", "WhatsApp", "Telegram"] else "ALERT_WARNING"
                    self.mood.trigger_event("notif", 5, 12.0, face, msg)
                self.last_unread_counts[app] = count
                
            # Clear apps that no longer have unread counts
            for app in list(self.last_unread_counts.keys()):
                if not any(a == app for a, c in unreads):
                    self.last_unread_counts[app] = 0
        except Exception as e:
            pass

        # 2. Check SQLite wpndatabase.db for new toast alerts
        if IS_WIN and os.path.exists(self.db_path):
            temp_dir = tempfile.gettempdir()
            temp_copy = os.path.join(temp_dir, "wpn_check.db")
            try:
                shutil.copy2(self.db_path, temp_copy)
                conn = sqlite3.connect(temp_copy)
                cursor = conn.cursor()
                
                # Query apps mapping
                cursor.execute("SELECT RecordId, PrimaryId FROM NotificationHandler;")
                handlers = {r[0]: r[1] for r in cursor.fetchall()}
                
                # Query new toasts
                cursor.execute(
                    "SELECT [Order], HandlerId, Payload FROM Notification "
                    "WHERE [Order] > ? AND Type = 'toast' ORDER BY [Order] ASC;",
                    (self.max_order,)
                )
                rows = cursor.fetchall()
                
                for r in rows:
                    order, handler_id, payload = r
                    self.max_order = max(self.max_order, order)
                    
                    app_id = handlers.get(handler_id, "Unknown")
                    app_clean = app_id.split('.')[-1].split('!')[-1].replace("ToastHandler", "").replace("App", "").title()
                    
                    payload_str = ""
                    if isinstance(payload, bytes):
                        payload_str = payload.decode('utf-8', errors='ignore')
                    else:
                        payload_str = str(payload)
                    
                    texts = re.findall(r'<text[^>]*>(.*?)</text>', payload_str)
                    if texts:
                        title = texts[0].strip()
                        body = texts[1].strip() if len(texts) > 1 else ""
                        
                        if body:
                            msg = f"{app_clean}: {title}: {body}"
                        else:
                            msg = f"{app_clean}: {title}"
                            
                        msg = msg[:39]  # fit 40 char ESP32 screen limit
                        
                        face = "ALERT_WARNING"
                        app_lower = app_clean.lower()
                        if any(k in app_lower for k in ["discord", "whatsapp", "telegram", "slack", "teams"]):
                            face = "HAPPY"
                        elif "spotify" in app_lower:
                            face = "LOVE_BONDING"
                            
                        self.mood.trigger_event("notif", 5, 12.0, face, msg)
                        break  # handle one per check cycle to prevent spamming
                        
                conn.close()
                try: os.remove(temp_copy)
                except: pass
            except Exception as e:
                pass


# ==============================================================
#  MAIN AGENT LOOP
# ==============================================================
class PCAgent:
    def __init__(self):
        log(f"\n{BOLD}{'-'*54}", CYAN)
        log(f"  AeroSniffer PC Agent  ({platform.system()} {platform.release()})", CYAN)
        log(f"{'-'*54}{RESET}\n")

        self.serial     = SerialManager()
        self.window_mon = WindowMonitor()
        self.kbd_mon    = KeyboardMonitor()
        self.sys_mon    = SystemMonitor()
        
        self.weather_info = ""
        self.mood_engine = MoodEngine(self.serial, self)
        self.notif_mon  = NotificationWatcher(self.mood_engine)
        self._running   = False

        # Start keyboard listener
        self.kbd_mon.start()
        
        # Start Weather Fetcher daemon thread
        self.weather_thread = threading.Thread(target=self._fetch_weather_loop, daemon=True)
        self.weather_thread.start()

    def _fetch_weather_loop(self):
        """Fetch weather from wttr.in every 20 minutes in a non-blocking thread."""
        import urllib.request
        log("Weather fetcher thread started.", GREEN)
        while True:
            try:
                url = "https://wttr.in/Nalco,Angul?format=%c%t"
                req = urllib.request.Request(url, headers={'User-Agent': 'curl'})
                with urllib.request.urlopen(req, timeout=5) as response:
                    raw = response.read().decode('utf-8').strip()
                    clean = raw.replace('+', ' ').strip()
                    if clean and len(clean) < 15:
                        self.weather_info = clean
                        log(f"Weather updated (Nalco, Angul): {clean}", GREEN)
            except Exception as e:
                pass
            time.sleep(1200)

    def run(self):
        self._running = True
        last_window_check = 0.0
        last_cpu_sample   = 0.0
        window_title      = ""
        prev_window       = ""

        log("Agent running. Press Ctrl+C to stop.\n")

        # Print header table
        print(f"  {'TIME':8}  {'FACE':14}  {'STATUS':20}  TRIGGER")
        print(f"  {'-'*8}  {'-'*14}  {'-'*20}  {'-'*20}")

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
                self.mood_engine.update(
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
        faces = ["IDLE","HAPPY","EXCITED","SLEEPY","THINKING",
                 "SAD_ERROR","ALERT_WARNING","LOVE_BONDING",
                 "STARTUP_BOOT","SURPRISED"]
        for face in faces:
            log(f"  -> {face}", CYAN)
            mgr.send(f"FACE:{face}")
            mgr.send(f"STATUS:{CONFIG['face_status'].get(face,'')}")
            time.sleep(3)
        mgr.send("FACE:IDLE")
        mgr.stop()
        log("Test complete.", GREEN)
        sys.exit(0)

    PCAgent().run()
