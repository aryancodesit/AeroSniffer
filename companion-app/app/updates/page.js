export default function Updates() {
  return (
    <div className="card" style={{ maxWidth: '800px', margin: '0 auto', textAlign: 'center' }}>
      <h2>🔄 Firmware Updates</h2>
      <p style={{marginTop: '20px', color: 'var(--text-muted)'}}>
        Check for the latest AeroSniffer firmware releases.
      </p>
      
      <div style={{marginTop: '40px', padding: '30px', backgroundColor: 'var(--bg-deep)', borderRadius: '8px', border: '1px solid var(--border)'}}>
        <h3>Current Version: v2.0</h3>
        <p style={{marginTop: '10px', color: 'var(--accent-green)'}}>You are up to date!</p>
        
        <button className="btn btn-primary" style={{marginTop: '20px'}} disabled>Check for Updates</button>
      </div>

      <p style={{marginTop: '30px', fontSize: '0.9rem', color: 'var(--text-muted)'}}>
        Future versions will support flashing directly from the browser using Web Serial.
      </p>
    </div>
  );
}
