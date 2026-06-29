# Home Guard

## Purpose

Home Guard converts discovered devices into known entities.

## Device States

### Unknown

Newly observed device.

### Familiar

Observed multiple times.

### Trusted

Explicitly approved by user.

## Presence States

### Present

Seen recently.

### Away

Not observed recently.

## Events

- Trusted Device Returned
- New Unknown Device
- Familiar Device Detected

## Goals

- Presence awareness
- Home occupancy awareness
- Trusted device tracking

## Non Goals

- Cloud analytics
- Device fingerprinting
- Vendor databases
- Geolocation
