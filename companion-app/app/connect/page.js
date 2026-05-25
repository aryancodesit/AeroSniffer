"use client";

import { useState, useEffect, useRef } from 'react';
import { serialAPI } from '@/lib/serial';

export default function Dashboard() {
  const [connected, setConnected] = useState(false);
  const [deviceInfo, setDeviceInfo] = useState(null);
  const [status, setStatus] = useState({
    scanning: false,
    ch: 1,
    pps: 0,
    total: 0,
    beacons: 0,
    probes: 0,
    deauths: 0,
    hopping: true,
    aps: 0
  });
  const [events, setEvents] = useState([]);
  const [apList, setApList] = useState([]);
  const [errorMsg, setErrorMsg] = useState("");
  
  const [settings, setSettings] = useState({
    ssid: "",
    pass: "",
    lamin: 19.8,
    lomin: 85.0,
    lamax: 21.0,
    lomax: 86.8,
    c_col: "#00FF00",
    w_col: "#FF0000"
  });
  const [settingsStatus, setSettingsStatus] = useState("");
  
  const eventsEndRef = useRef(null);

  // Auto-scroll event log
  useEffect(() => {
    eventsEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [events]);

  useEffect(() => {
    serialAPI.onMessage = (type, data) => {
      if (type === 'RES') {
        if (data.action === 'scan_start') setStatus(s => ({ ...s, scanning: true }));
        if (data.action === 'scan_stop') setStatus(s => ({ ...s, scanning: false }));
        if (data.pps !== undefined) {
          // Status update
          setStatus(data);
        }
        if (data.fw) {
          setDeviceInfo(data);
        }
        if (data.aps && Array.isArray(data.aps)) {
          setApList(data.aps);
        }
        if (data.ssid !== undefined) {
          setSettings(s => ({
            ...s,
            ssid: data.ssid,
            lamin: data.lamin,
            lomin: data.lomin,
            lamax: data.lamax,
            lomax: data.lomax,
            c_col: data.c_col || "#00FF00",
            w_col: data.w_col || "#FF0000"
          }));
        }
        if (data.action === 'set_wifi' || data.action === 'set_bbox' || data.action === 'set_color') {
          setSettingsStatus("Settings saved to device!");
          setTimeout(() => setSettingsStatus(""), 3000);
        }
      } else if (type === 'EVT') {
        setEvents(prev => [...prev.slice(-49), { time: new Date().toLocaleTimeString(), ...data }]);
      }
    };

    serialAPI.onDisconnect = () => {
      setConnected(false);
      setDeviceInfo(null);
    };

    // Polling loop when connected
    const pollInterval = setInterval(() => {
      if (connected && status.scanning) {
        serialAPI.sendCommand("STATUS");
      }
    }, 1000);

    const apInterval = setInterval(() => {
      if (connected && status.scanning) {
        serialAPI.sendCommand("GET_APS");
      }
    }, 5000);

    return () => {
      clearInterval(pollInterval);
      clearInterval(apInterval);
      serialAPI.onMessage = null;
      serialAPI.onDisconnect = null;
    };
  }, [connected, status.scanning]);

  const connectDevice = async () => {
    try {
      setErrorMsg("");
      await serialAPI.connect();
      setConnected(true);
      await serialAPI.sendCommand("PING");
      setTimeout(() => serialAPI.sendCommand("STATUS"), 500);
      setTimeout(() => serialAPI.sendCommand("GET_CFG"), 1000);
    } catch (e) {
      setErrorMsg(e.message || "Failed to connect.");
    }
  };

  const disconnectDevice = async () => {
    await serialAPI.disconnect();
    setConnected(false);
  };

  const startScan = () => serialAPI.sendCommand("SCAN_START");
  const stopScan = () => serialAPI.sendCommand("SCAN_STOP");
  const toggleHop = () => serialAPI.sendCommand(status.hopping ? "HOP_OFF" : "HOP_ON");
  const setChannel = (e) => serialAPI.sendCommand(`SET_CH:${e.target.value}`);
  const resetStats = () => {
    serialAPI.sendCommand("RESET_STATS");
    setEvents([]);
    setApList([]);
  };

  const saveSettings = () => {
    serialAPI.sendCommand(`SET_WIFI:${settings.ssid}:${settings.pass}`);
    setTimeout(() => {
      serialAPI.sendCommand(`SET_BBOX:${settings.lamin}:${settings.lomin}:${settings.lamax}:${settings.lomax}`);
    }, 200);
    setTimeout(() => {
      serialAPI.sendCommand(`SET_COLOR:${settings.c_col}:${settings.w_col}`);
    }, 400);
  };

  const handleSettingChange = (e) => {
    const { name, value } = e.target;
    setSettings(s => ({ ...s, [name]: value }));
  };

  if (!connected) {
    return (
      <div style={styles.centerContainer}>
        <div className="card" style={{ textAlign: 'center', maxWidth: '400px', width: '100%' }}>
          <h2>Connect Device</h2>
          <p style={{ margin: '20px 0', color: 'var(--text-muted)' }}>
            Ensure your DeskBuddy is plugged into a USB port and running Mode 2.
          </p>
          <button className="btn btn-primary" onClick={connectDevice} style={{ width: '100%' }}>
            Request USB Port
          </button>
          {errorMsg && <p style={{ color: 'var(--accent-red)', marginTop: '15px' }}>{errorMsg}</p>}
        </div>
      </div>
    );
  }

  return (
    <div style={styles.dashboard}>
      {/* Top Bar */}
      <div className="card" style={styles.topbar}>
        <div style={{display: 'flex', alignItems: 'center', gap: '15px'}}>
          <div style={styles.dot}></div>
          <span>Connected: {deviceInfo?.hw === 'deskbuddy2' ? 'XIAO ESP32S3' : 'ESP32'}</span>
          <span style={{color: 'var(--text-muted)'}}>| FW: v{deviceInfo?.fw}</span>
        </div>
        <button className="btn btn-danger" onClick={disconnectDevice} style={{padding: '5px 15px'}}>Disconnect</button>
      </div>

      <div style={styles.grid}>
        {/* Left Column: Controls & Stats */}
        <div style={{display: 'flex', flexDirection: 'column', gap: '20px'}}>
          <div className="card">
            <h3 style={{marginBottom: '15px'}}>Scanner Controls</h3>
            <div style={{display: 'flex', gap: '10px', marginBottom: '15px'}}>
              <button className="btn btn-primary" onClick={startScan} disabled={status.scanning} style={{flex: 1}}>
                ▶ Start
              </button>
              <button className="btn btn-danger" onClick={stopScan} disabled={!status.scanning} style={{flex: 1}}>
                ■ Stop
              </button>
            </div>
            <div style={{display: 'flex', gap: '10px', marginBottom: '15px'}}>
              <select 
                value={status.hopping ? "auto" : status.ch} 
                onChange={setChannel}
                style={styles.select}
                disabled={!status.scanning}
              >
                <option value="auto">Auto-Hop</option>
                {[...Array(13)].map((_, i) => <option key={i+1} value={i+1}>CH {i+1}</option>)}
              </select>
              <button className="btn" onClick={toggleHop} disabled={!status.scanning} style={{flex: 1}}>
                {status.hopping ? "Lock CH" : "Resume Hop"}
              </button>
            </div>
            <button className="btn" onClick={resetStats} style={{width: '100%', borderColor: 'var(--text-muted)', color: 'var(--text-muted)'}}>
              Reset Counters
            </button>
          </div>

          <div className="card" style={{ flex: 1 }}>
            <h3 style={{marginBottom: '15px'}}>Live Stats</h3>
            <div style={{display: 'flex', flexDirection: 'column', gap: '10px'}}>
              <StatRow label="Status" value={status.scanning ? "SCANNING" : "IDLE"} highlight={status.scanning ? 'var(--accent-green)' : ''} />
              <StatRow label="Channel" value={status.hopping ? `${status.ch} (Hop)` : status.ch} />
              <StatRow label="PKT/s" value={status.pps} highlight={status.pps > 100 ? 'var(--accent-cyan)' : ''} />
              <StatRow label="Total Packets" value={status.total.toLocaleString()} />
              <div style={{height: '1px', backgroundColor: 'var(--border)', margin: '5px 0'}}></div>
              <StatRow label="Beacons" value={status.beacons.toLocaleString()} />
              <StatRow label="Probes" value={status.probes.toLocaleString()} />
              <StatRow 
                label="Deauths" 
                value={status.deauths.toLocaleString()} 
                highlight={status.deauths > 0 ? 'var(--accent-red)' : ''} 
                className={status.deauths > 0 ? 'alert-pulse' : ''}
              />
              <StatRow label="AP Count" value={status.aps} />
            </div>
          </div>

          <div className="card">
            <h3 style={{marginBottom: '15px'}}>Device Settings</h3>
            <div style={{display: 'flex', flexDirection: 'column', gap: '10px'}}>
              <div>
                <label style={styles.label}>WiFi SSID</label>
                <input type="text" name="ssid" value={settings.ssid} onChange={handleSettingChange} style={styles.input} />
              </div>
              <div>
                <label style={styles.label}>WiFi Password</label>
                <input type="password" name="pass" value={settings.pass} onChange={handleSettingChange} placeholder="(unchanged)" style={styles.input} />
              </div>
              <div>
                <label style={styles.label}>Radar Box (Lat Min/Max)</label>
                <div style={{display: 'flex', gap: '5px'}}>
                  <input type="number" step="0.1" name="lamin" value={settings.lamin} onChange={handleSettingChange} style={styles.input} />
                  <input type="number" step="0.1" name="lamax" value={settings.lamax} onChange={handleSettingChange} style={styles.input} />
                </div>
              </div>
              <div>
                <label style={styles.label}>Radar Box (Lon Min/Max)</label>
                <div style={{display: 'flex', gap: '5px'}}>
                  <input type="number" step="0.1" name="lomin" value={settings.lomin} onChange={handleSettingChange} style={styles.input} />
                  <input type="number" step="0.1" name="lomax" value={settings.lomax} onChange={handleSettingChange} style={styles.input} />
                </div>
              </div>
              <div style={{display: 'flex', gap: '10px'}}>
                <div style={{flex: 1}}>
                  <label style={styles.label}>Clock Color</label>
                  <input type="color" name="c_col" value={settings.c_col} onChange={handleSettingChange} style={styles.colorInput} />
                </div>
                <div style={{flex: 1}}>
                  <label style={styles.label}>Weather Color</label>
                  <input type="color" name="w_col" value={settings.w_col} onChange={handleSettingChange} style={styles.colorInput} />
                </div>
              </div>
              <button className="btn btn-primary" onClick={saveSettings} style={{marginTop: '10px'}}>Save to ESP32</button>
              {settingsStatus && <p style={{color: 'var(--accent-green)', fontSize: '0.8rem', textAlign: 'center'}}>{settingsStatus}</p>}
            </div>
          </div>
        </div>

        {/* Right Column: Feeds */}
        <div style={{display: 'flex', flexDirection: 'column', gap: '20px'}}>
          <div className="card" style={{flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column'}}>
            <h3 style={{marginBottom: '15px'}}>Access Points (Top 20)</h3>
            <div style={styles.tableContainer}>
              <table style={styles.table}>
                <thead>
                  <tr>
                    <th>SSID</th>
                    <th>BSSID</th>
                    <th>CH</th>
                    <th>RSSI</th>
                  </tr>
                </thead>
                <tbody>
                  {apList.map((ap, i) => (
                    <tr key={i}>
                      <td>{ap.ssid || '<hidden>'}</td>
                      <td style={{color: 'var(--text-muted)'}}>{ap.bssid}</td>
                      <td>{ap.ch}</td>
                      <td style={{color: ap.rssi > -60 ? 'var(--accent-green)' : ap.rssi > -80 ? 'var(--accent-yellow)' : 'var(--text-muted)'}}>
                        {ap.rssi} dBm
                      </td>
                    </tr>
                  ))}
                  {apList.length === 0 && (
                    <tr><td colSpan={4} style={{textAlign: 'center', color: 'var(--text-muted)'}}>No APs detected yet.</td></tr>
                  )}
                </tbody>
              </table>
            </div>
          </div>

          <div className="card" style={{flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column'}}>
            <h3 style={{marginBottom: '15px'}}>Event Log</h3>
            <div style={styles.eventLog}>
              {events.map((evt, i) => (
                <div key={i} style={styles.eventItem}>
                  <span style={{color: 'var(--text-muted)', fontSize: '0.8rem'}}>[{evt.time}]</span>{' '}
                  {evt.type === 'deauth_alert' ? (
                    <span style={{color: 'var(--accent-red)', fontWeight: 'bold'}}>⚠️ DEAUTH SPIKE DETECTED ({evt.count} frames)</span>
                  ) : (
                    <span>{JSON.stringify(evt)}</span>
                  )}
                </div>
              ))}
              {events.length === 0 && (
                <div style={{color: 'var(--text-muted)', fontStyle: 'italic'}}>Waiting for events...</div>
              )}
              <div ref={eventsEndRef} />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

const StatRow = ({ label, value, highlight, className }) => (
  <div style={{display: 'flex', justifyContent: 'space-between', padding: '5px 0'}} className={className}>
    <span style={{color: 'var(--text-muted)'}}>{label}</span>
    <span style={{color: highlight || 'var(--text-primary)', fontWeight: 'bold'}}>{value}</span>
  </div>
);

const styles = {
  centerContainer: {
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
    minHeight: '80vh',
  },
  dashboard: {
    display: 'flex',
    flexDirection: 'column',
    gap: '20px',
    height: '100%',
  },
  topbar: {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: '15px 20px',
  },
  dot: {
    width: '12px',
    height: '12px',
    backgroundColor: 'var(--accent-green)',
    borderRadius: '50%',
    boxShadow: '0 0 10px var(--accent-green)',
  },
  grid: {
    display: 'grid',
    gridTemplateColumns: '300px 1fr',
    gap: '20px',
    flex: 1,
  },
  select: {
    backgroundColor: 'var(--bg-elevated)',
    color: 'var(--text-primary)',
    border: '1px solid var(--border)',
    padding: '10px',
    borderRadius: '4px',
    fontFamily: 'var(--font-body)',
    flex: 1,
  },
  tableContainer: {
    overflowY: 'auto',
    flex: 1,
  },
  table: {
    width: '100%',
    borderCollapse: 'collapse',
    textAlign: 'left',
  },
  label: {
    display: 'block',
    fontSize: '0.8rem',
    color: 'var(--text-muted)',
    marginBottom: '2px',
  },
  input: {
    width: '100%',
    backgroundColor: 'var(--bg-elevated)',
    color: 'var(--text-primary)',
    border: '1px solid var(--border)',
    padding: '8px',
    borderRadius: '4px',
    fontFamily: 'var(--font-body)',
  },
  colorInput: {
    width: '100%',
    height: '36px',
    padding: '0',
    border: 'none',
    borderRadius: '4px',
    cursor: 'pointer',
    backgroundColor: 'transparent'
  },
  eventLog: {
    flex: 1,
    overflowY: 'auto',
    backgroundColor: 'var(--bg-deep)',
    border: '1px solid var(--border)',
    borderRadius: '4px',
    padding: '10px',
    fontFamily: 'var(--font-body)',
    fontSize: '0.9rem',
    display: 'flex',
    flexDirection: 'column',
    gap: '5px',
  },
  eventItem: {
    borderBottom: '1px solid rgba(255,255,255,0.05)',
    paddingBottom: '5px',
  }
};
