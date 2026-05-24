export default function Payloads() {
  return (
    <div className="card" style={{ maxWidth: '800px', margin: '0 auto' }}>
      <h2>🎯 Payload Library</h2>
      <p style={{marginTop: '20px', color: 'var(--text-muted)'}}>
        Pre-configured scan workflows. (Connect device in Dashboard first)
      </p>
      
      <div style={{display: 'grid', gap: '15px', marginTop: '30px'}}>
        <div style={styles.payloadCard}>
          <h3>Quick Scan</h3>
          <p style={{color: 'var(--text-muted)'}}>30-second sweep of all channels.</p>
          <button className="btn btn-primary" style={{marginTop: '10px'}} disabled>Run Payload</button>
        </div>
        <div style={styles.payloadCard}>
          <h3>Deauth Watch</h3>
          <p style={{color: 'var(--text-muted)'}}>Monitor for deauthentication attacks.</p>
          <button className="btn btn-primary" style={{marginTop: '10px'}} disabled>Run Payload</button>
        </div>
        <div style={styles.payloadCard}>
          <h3>AP Census</h3>
          <p style={{color: 'var(--text-muted)'}}>Count all visible access points.</p>
          <button className="btn btn-primary" style={{marginTop: '10px'}} disabled>Run Payload</button>
        </div>
      </div>
    </div>
  );
}

const styles = {
  payloadCard: {
    padding: '20px',
    backgroundColor: 'var(--bg-deep)',
    border: '1px solid var(--border)',
    borderRadius: '4px',
  }
};
