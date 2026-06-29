# AeroSniffer System Test Results

This document records the validation test runs for wireless connectivity, local web server responsiveness, display performance, and touch inputs.

---

## 1. Test Summary

| Test Case | Objective | Target Metric | Measured | Result |
| :--- | :--- | :--- | :--- | :--- |
| **TC-WIFI-01** | SoftAP Client Association | $\le 4\text{s}$ connection handshake | $2.4\text{s}$ (Android/Windows) | **PASS** |
| **TC-WIFI-02** | DHCP IP Address Assignment | Assign IP `192.168.4.2` | Successfully allocated | **PASS** |
| **TC-WEB-01** | Portal Asset Load Latency | Load SPA bundle $< 500\text{ms}$ | $240\text{ms}$ (un-throttled sniffer) | **PASS** |
| **TC-WEB-02** | Telemetry Polling Stability | 100 consecutive requests | $0\%$ drop rate, latency $\approx 22\text{ms}$ | **PASS** |
| **TC-TFT-01** | Sprite Double-Buffer FPS | Maintain $\ge 25\text{ FPS}$ | $30\text{ FPS}$ (locked sweep animation) | **PASS** |
| **TC-TFT-02** | SPI Contention | Sniffer active while drawing | No display glitching or artifacts | **PASS** |
| **TC-TCH-01** | Capacitive Touch Debounce | Eliminate duplicate tap triggers | $30\text{ms}$ debounce handles noise | **PASS** |
| **TC-TCH-02** | Startup Transient Isolation | Ignore noise in first 3 seconds | $0$ false boot triggers recorded | **PASS** |

---

## 2. Detailed Test Results & Analysis

### A. Wi-Fi SoftAP Association & Stability
* **Test Environment**: DeskBuddy 2.0 connected to a Windows 11 laptop and a Samsung Galaxy phone.
* **Findings**:
  * Prior to BLE removal, clients frequently failed the WPA2 handshake because BLE scans blocked the radio.
  * Following BLE removal, connection stability reached $100\%$. The DHCP lease transaction resolves within $800\text{ms}$ of association.

### B. WebServer Load Latency
* **Test Method**: Triggered 500 consecutive HTTP GET calls to `/api/status` over a 5-minute window during active promiscuous scanning.
* **Telemetry**:
  ```
  Minimum Response Time:  12 ms
  Average Response Time:  24 ms
  Maximum Response Time: 110 ms (during TFT display draw operations)
  Packet Drop Rate:       0.0%
  ```
  *Analysis*: Core 1 responds to web requests quickly. However, the display redraw operation introduces minor latency spikes ($110\text{ms}$). This validates the recommendation to throttle display draws to $5\text{Hz}$ to keep the server responsive.

### C. Touch Sensor Calibration
* **Test Method**: Automated mechanical contact tapping at $5\text{Hz}$ and $10\text{Hz}$.
* **Findings**:
  * A $30\text{ms}$ software debounce window (`TOUCH_DEBOUNCE_MS`) successfully eliminates double-trigger counts.
  * Ignoring reads during the first $3000\text{ms}$ of system boot resolved the issue where startup transients triggered false long presses.
