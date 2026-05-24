export default function Tutorial() {
  return (
    <div className="card" style={{ maxWidth: '800px', margin: '0 auto' }}>
      <h2>📖 AeroSniffer Tutorial</h2>
      <p style={{marginTop: '20px', color: 'var(--text-muted)'}}>
        Documentation and guides will go here.
      </p>
      
      <div style={{marginTop: '30px'}}>
        <h3>Quick Start</h3>
        <ul style={{marginLeft: '20px', marginTop: '10px', display: 'flex', flexDirection: 'column', gap: '10px'}}>
          <li><strong>Mode 1 (Companion):</strong> Tap the roof to interact.</li>
          <li><strong>Mode 2 (Security):</strong> Plug into USB and connect via the Dashboard.</li>
          <li><strong>Mode 3 (Aviation):</strong> Connects to WiFi to track flights overhead.</li>
          <li><strong>Switching Modes:</strong> Long-press the touch sensor for 1.5 seconds.</li>
        </ul>
      </div>
    </div>
  );
}
