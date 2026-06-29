# AeroSniffer Local REST API Reference

This document defines the REST API endpoints hosted on the AeroSniffer local Web Server (port 80). These endpoints are used by the local Single Page Application (SPA) and can be accessed by third-party integrations or local PC agents.

---

## 1. Summary of Endpoints

| Endpoint | Method | Authentication | Payload Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| `/api/status` | `GET` | None | JSON | Returns system states, uptime, heap, and packet telemetry. |
| `/api/devices` | `GET` | None | JSON | Returns the list of discovered Access Points and Client MACs. |
| `/api/threats` | `GET` | None | JSON | Returns active threat logs from the ring buffer. |
| `/api/settings` | `GET` | None | JSON | Returns current scan configurations and Evil Twin state. |
| `/api/settings` | `POST` | *Planned* | JSON | Updates SSID protection parameters and channel hopping dwell. |
| `/api/scan` | `POST` | *Planned* | JSON | Starts or stops the promiscuous packet sniffer task. |
| `/api/whitelist` | `POST` | *Planned* | JSON | Adds or removes devices from the Home Guard NVS whitelist. |

---

## 2. API Schema Details

### A. GET /api/status
Returns current system metrics, memory diagnostics, and sniffer telemetry.

* **Response Example**:
  ```json
  {
    "scanning": true,
    "pps": 142,
    "total": 42091,
    "beacons": 2491,
    "probes": 512,
    "deauths": 3,
    "channel": 6,
    "ap_stations": 2,
    "ap_ssid": "AeroSniffer-Test",
    "ap_ip": "192.168.4.1",
    "uptime_sec": 1820,
    "heap": 142091
  }
  ```
* **Property Definitions**:
  * `scanning`: (Boolean) True if Core 0 is currently sniffing packets.
  * `pps`: (Integer) Packets Per Second processed in the last measurement window.
  * `total`: (Integer) Cumulative count of 802.11 frames sniffed since boot.
  * `ap_stations`: (Integer) Number of client devices connected to the AeroSniffer access point.
  * `heap`: (Integer) Available free heap memory in bytes.

---

### B. GET /api/devices
Returns arrays of discovered wireless Access Points (SSIDs) and associated client devices.

* **Response Example**:
  ```json
  {
    "aps": [
      {
        "ssid": "HomeRouter_2G",
        "bssid": "00:11:22:33:44:55",
        "channel": 6,
        "rssi": -54
      },
      {
        "ssid": "CoffeeShop_Free",
        "bssid": "AA:BB:CC:DD:EE:FF",
        "channel": 11,
        "rssi": -78
      }
    ],
    "clients": [
      {
        "mac": "44:55:66:77:88:99",
        "ap_bssid": "00:11:22:33:44:55",
        "rssi": -62
      }
    ]
  }
  ```

---

### C. GET /api/threats
Retrieves logged alerts from the shared ring buffer.

* **Response Example**:
  ```json
  {
    "count": 1,
    "threats": [
      {
        "ts": 1284,
        "type": "deauth",
        "severity": 2,
        "desc": "Deauth flood spike (58 frames/s) detected",
        "src": "A4:C1:38:DE:AD:BF"
      }
    ]
  }
  ```
* **Property Definitions**:
  * `ts`: (Integer) Uptime timestamp in seconds when the threat was detected.
  * `type`: (String) Threat type classification (`deauth`, `eviltwin`, `intruder`).
  * `severity`: (Integer) Alert severity (`0 = INFO`, `1 = WARNING`, `2 = CRITICAL`).

---

### D. GET /api/settings
Returns the active configuration parameters and threat tracking state.

* **Response Example**:
  ```json
  {
    "eviltwin": {
      "ssid": "MyHomeNet",
      "bssid": "00:11:22:33:44:55",
      "detected": true
    },
    "scan": {
      "dwell": 180,
      "hopping": true,
      "fixed_ch": 6
    }
  }
  ```
