# MISSION_CONTROL_ARCHITECTURE.md

# AeroSniffer Mission Control Architecture

Version: 1.0
Status: Approved Architecture
Owner: AeroSniffer Project
Last Updated: 2026-06-16

---

# Purpose

Mission Control is the long-term dashboard architecture for AeroSniffer.

Its purpose is to provide a unified interface for:

* Companion Intelligence
* Security Intelligence
* Aviation Intelligence
* Device Analytics
* System Observability

Mission Control is not a replacement for the ESP32 device or the local Security Portal.

It is the third layer of the AeroSniffer ecosystem.

---

# Core Philosophy

AeroSniffer is divided into three distinct layers:

## Device = Observe

Hardware:

* XIAO ESP32S3
* ST7789 Display
* Capacitive Touch Input
* WiFi Radio
* ADS-B / Aviation Services

Responsibilities:

* Packet Capture
* Threat Detection
* Home Guard Presence Tracking
* Aviation Data Collection
* Emotion Engine
* User Interaction

The device observes and reacts.

The device does not perform heavy analytics.

---

## Portal = Act

Local Security Portal

Accessed via:

```text
192.168.4.1
```

Responsibilities:

* Security Controls
* Home Guard Controls
* Device Configuration
* Threat Management
* Local Diagnostics

The portal performs actions.

The portal must remain fully functional offline.

---

## Mission Control = Analyze

Web Dashboard

Accessed via:

```text
aero-sniffer.vercel.app
```

Responsibilities:

* Historical Analysis
* Long-Term Trends
* Visualizations
* Device Intelligence
* Threat Intelligence
* Aviation Insights

Mission Control analyzes information.

Mission Control should never become the primary control surface.

---

# Architecture Diagram

```text
                ┌────────────────────┐
                │  AeroSniffer ESP32 │
                │      Observe       │
                └──────────┬─────────┘
                           │
                 WiFi / Serial Data
                           │
        ┌──────────────────┴──────────────────┐
        │                                     │
        ▼                                     ▼

┌───────────────────┐              ┌───────────────────┐
│   Local Portal    │              │  Mission Control  │
│       Act         │              │      Analyze      │
└───────────────────┘              └───────────────────┘
```

---

# Mission Control Goals

Mission Control exists to answer:

## Companion

```text
How is my companion behaving?
```

## Security

```text
What is happening on my network?
```

## Aviation

```text
What aircraft activity has been observed?
```

## System

```text
How is AeroSniffer performing?
```

---

# Route Structure

## /

Overview Dashboard

System-wide summary.

Contains:

* Device Status
* Current Mode
* Threat Summary
* Device Summary
* Aviation Summary
* Recent Activity

Purpose:

Executive overview.

---

## /companion

Companion Intelligence Dashboard

Contains:

* Current Emotion
* Face State
* Emotion Timeline
* Interaction Statistics
* Companion Events
* Activity Metrics

Purpose:

Understand companion behavior.

---

## /security

Security Dashboard

Contains:

### Overview

* Packet Statistics
* Threat Counts
* Scan Status
* Channel Information

### Devices

* Trusted Devices
* Familiar Devices
* Unknown Devices
* Presence Status

### Threats

* Unknown Device Alerts
* Randomized MAC Alerts
* Trusted Device Returned
* Deauthentication Events

### Home Guard

* Presence Tracking
* Device Trust Management
* Occupancy Awareness

Purpose:

Network intelligence and awareness.

---

## /aviation

Aviation Dashboard

Contains:

### Flight Overview

* Aircraft Count
* Flight Statistics
* Coverage Area

### Aircraft Table

* Callsign
* Altitude
* Speed
* Heading
* Coordinates

### Analytics

* Most Seen Aircraft
* Most Seen Airlines
* Flight Activity

Purpose:

Aviation awareness and tracking.

---

## /analytics

Analytics Dashboard

Contains:

### Threat Analytics

* Threat Frequency
* Threat Types
* Threat Timeline

### Device Analytics

* Device Growth
* Trusted Ratio
* Presence History

### System Analytics

* Uptime
* Memory Usage
* Mode Usage

Purpose:

Long-term analysis.

---

# Data Sources

Mission Control must support multiple transports.

## V1

Serial

```text
ESP32
↓
Web Serial API
↓
Mission Control
```

Primary transport.

---

## V2

WiFi HTTP API

```text
ESP32
↓
REST API
↓
Mission Control
```

Optional future transport.

---

## V3

Cloud Synchronization

```text
ESP32
↓
Cloud
↓
Mission Control
```

Future roadmap.

---

# Data Model Principles

Mission Control must consume structured data only.

Examples:

## Status

* Current Mode
* Heap
* Uptime
* Scan State

## Threats

* Severity
* Type
* Timestamp
* Source

## Devices

* MAC
* Name
* Trust State
* Presence State

## Aviation

* Callsign
* Altitude
* Speed
* Heading

---

# Design Principles

Mission Control must preserve:

* Cyberpunk Aesthetic
* Pixel Identity
* Retro Terminal Feel
* Neon Color System

Mission Control must not become:

* Generic Admin Dashboard
* Bootstrap Panel
* Corporate SaaS Interface

AeroSniffer's identity is part of the product.

---

# Non Goals

Mission Control is NOT:

* A replacement for the local portal
* A cloud-first application
* A remote attack platform
* A device management suite

Mission Control exists for visibility and understanding.

---

# Future Expansion

Potential future modules:

## Fleet View

Multiple AeroSniffer devices.

## Historical Database

Persistent event storage.

## Companion Intelligence

Emotion pattern analysis.

## Security Reports

Automated report generation.

## Aviation Heatmaps

Long-term aircraft visualization.

---

# Success Criteria

Mission Control is considered successful when:

* Users can understand device status at a glance.
* Security activity is easy to visualize.
* Companion activity feels alive and personal.
* Aviation activity is easy to explore.
* Analytics provide useful insights.
* The dashboard scales without architectural redesign.

---

# Architecture Decision

The AeroSniffer ecosystem is permanently organized as:

```text
Device
   = Observe

Portal
   = Act

Mission Control
   = Analyze
```

All future features should respect this separation of responsibilities.
