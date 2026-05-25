export default function Tutorial() {
  return (
    <div className="card" style={{ maxWidth: '800px', margin: '0 auto', padding: '30px' }}>
      <h2 style={{ borderBottom: '1px solid var(--border)', paddingBottom: '15px' }}>📖 AeroSniffer User Manual</h2>
      <p style={{marginTop: '20px', color: 'var(--text-muted)'}}>
        Welcome to the DeskBuddy 2.0 AeroSniffer. Here is everything you need to know about operating the three different hardware modes.
      </p>
      
      <div style={{marginTop: '30px'}}>
        <h3 style={{color: 'var(--accent-green)'}}>🐾 Mode 1: The Companion</h3>
        <p style={{marginTop: '10px', lineHeight: '1.6'}}>
          This is the default mode when you power on the device. It displays animated eyes that look around and blink automatically.
        </p>
        <ul style={{marginLeft: '20px', marginTop: '10px', display: 'flex', flexDirection: 'column', gap: '8px'}}>
          <li><strong>Interaction:</strong> Give the roof of the gadget a quick tap. The eyes will react happily!</li>
          <li><strong>Screensaver:</strong> If you leave the gadget alone for 1 minute, it will enter screensaver mode, fetching the live time and weather via Wi-Fi and displaying it in a slideshow.</li>
        </ul>
      </div>

      <div style={{marginTop: '30px'}}>
        <h3 style={{color: 'var(--accent-green)'}}>🛡️ Mode 2: Security Monitor</h3>
        <p style={{marginTop: '10px', lineHeight: '1.6'}}>
          This mode acts as a passive 802.11 network sniffer. It jumps across Wi-Fi channels to map out local access points and detect Deauthentication attacks.
        </p>
        <ul style={{marginLeft: '20px', marginTop: '10px', display: 'flex', flexDirection: 'column', gap: '8px'}}>
          <li><strong>Standalone:</strong> The 1.3" display will show a green sweeping radar and basic packet statistics.</li>
          <li><strong>Web Control:</strong> While in this mode, plug the device into your laptop and navigate to the <strong>Connect</strong> tab to see full network details, lock the channel hopper, and configure the device.</li>
        </ul>
      </div>

      <div style={{marginTop: '30px'}}>
        <h3 style={{color: 'var(--accent-green)'}}>✈️ Mode 3: Flight Radar</h3>
        <p style={{marginTop: '10px', lineHeight: '1.6'}}>
          The aviation mode connects to your home Wi-Fi and tracks actual airplanes flying overhead using the OpenSky Network.
        </p>
        <ul style={{marginLeft: '20px', marginTop: '10px', display: 'flex', flexDirection: 'column', gap: '8px'}}>
          <li><strong>Flight Cards:</strong> The screen automatically scrolls through nearby planes, showing their callsign, altitude, and speed.</li>
          <li><strong>Mini-Radar:</strong> A compass shows the plane's heading, and a small radar dot shows its physical location relative to your city.</li>
        </ul>
      </div>

      <div style={{marginTop: '30px', padding: '20px', backgroundColor: 'var(--bg-deep)', borderRadius: '8px', border: '1px solid var(--border)'}}>
        <h3 style={{color: 'var(--text-primary)'}}>⚙️ How to Switch Modes</h3>
        <p style={{marginTop: '10px', lineHeight: '1.6', color: 'var(--text-muted)'}}>
          Place your finger on the red capacitive touch sensor (hidden under the roof of the 3D-printed case) and <strong>hold it for 1.5 seconds</strong>. The screen will display a mode-switch alert, and the gadget will boot into the next mode sequentially (1 &rarr; 2 &rarr; 3 &rarr; 1).
        </p>
      </div>
    </div>
  );
}
