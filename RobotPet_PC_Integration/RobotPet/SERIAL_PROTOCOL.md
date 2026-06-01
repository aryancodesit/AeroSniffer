# AeroSniffer Serial Protocol Reference

## Physical connection
```
XIAO ESP32S3  ←USB-C→  PC
115200 baud, 8N1, no flow control
```

---

## PC → ESP32  (commands you send)

| Command | Effect | Example |
|---------|--------|---------|
| `FACE:<name>\n` | Set face expression | `FACE:HAPPY\n` |
| `STATUS:<text>\n` | Set status bar text (max 38 chars) | `STATUS:{ coding... }\n` |
| `APP:<name>\n` | Set app label (max 30 chars) | `APP:VSCode\n` |
| `TELEMETRY:cpu=<n>,ram=<n>\n` | Feed system stats | `TELEMETRY:cpu=42,ram=68\n` |
| `PING\n` | Health check | `PING\n` |

### Valid face names
```
IDLE          Default neutral face
HAPPY         Curved-up eyes, cheek blush, smile
SAD           Droopy eyes, tears, frown
ANGRY         Slitted red eyes, zigzag mouth
SURPRISED     Wide yellow eyes, open mouth
THINKING      One squint, one open, animated dots
TYPING        Forward squint, cursor blink
SLEEPING      Closed eyes, Zzz animation
CODING        Screen-style eyes, green colour
VIBING        Wavy pink eyes, music note
PANIC         X eyes, red screen flash
NOTIFICATION  Blinking pink eyes, ! indicator
```

---

## ESP32 → PC  (responses you receive)

| Response | When |
|----------|------|
| `{"ready":1,"mode":"pet"}` | On boot / mode entry |
| `{"pong":1}` | Reply to PING |

---

## Quick test (without the full agent)

**Using Arduino Serial Monitor:**
1. Open Serial Monitor at 115200 baud
2. Type and send:  `FACE:PANIC`
3. Watch the face change

**Using Python one-liner:**
```python
import serial, time
s = serial.Serial("COM3", 115200)   # change port
time.sleep(1.5)
s.write(b"FACE:HAPPY\n")
```

**Using screen (Linux/Mac):**
```bash
screen /dev/ttyACM0 115200
# then type: FACE:VIBING  and press Enter
# Ctrl+A then K to exit
```

---

## Event priority order (in pc_agent.py)

```
1. CPU > 88% sustained 6s   →  PANIC
2. System idle > 5 min      →  SLEEPING
3. Keyboard active           →  TYPING  (or CODING if in IDE)
4. Active window title       →  mapped via CONFIG["app_faces"]
5. Default                   →  IDLE
```

You can add any app to `CONFIG["app_faces"]` in `pc_agent.py`.
The key is a **substring of the window title** (lowercase).

---

## Run commands

```bash
# Normal operation
python pc_agent.py

# Specify port manually
python pc_agent.py --port COM3
python pc_agent.py --port /dev/ttyACM0

# List all serial ports
python pc_agent.py --list

# Test mode — cycles every face with 3s delay
python pc_agent.py --test
```

---

## Adding custom triggers

Open `pc_agent.py` and edit `CONFIG["app_faces"]`:

```python
"app_faces": {
    # Add your app here — the key is a substring of the window title
    "your_app_name":  "HAPPY",
    "another_app":    "CODING",
    ...
}
```

You can also add your own events by editing the `FaceController.update()` method.

For example, to trigger `NOTIFICATION` when a specific file appears:
```python
# Inside FaceController.update() add before the final else:
elif os.path.exists("/tmp/my_notification_trigger"):
    new_face = "NOTIFICATION"
    reason   = "custom trigger file"
```
