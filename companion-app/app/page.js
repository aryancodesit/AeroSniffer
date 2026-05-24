import Link from 'next/link';

export default function Home() {
  return (
    <div style={styles.container}>
      <h1 style={styles.title}>AeroSniffer</h1>
      <p style={styles.subtitle}>XIAO ESP32S3 Multi-Boot Desk Gadget</p>
      
      <div style={styles.grid}>
        <div className="card" style={styles.card}>
          <h2>🔌 Connect & Scan</h2>
          <p>Launch the web-based Security Monitor dashboard.</p>
          <Link href="/connect" className="btn btn-primary" style={{marginTop: '15px', display: 'inline-block'}}>Launch Dashboard</Link>
        </div>

        <div className="card" style={styles.card}>
          <h2>🎯 Payloads</h2>
          <p>Library of pre-configured scans and automated workflows.</p>
          <Link href="/payloads" className="btn" style={{marginTop: '15px', display: 'inline-block'}}>View Payloads</Link>
        </div>

        <div className="card" style={styles.card}>
          <h2>📖 Tutorial</h2>
          <p>Learn how to use all three modes of AeroSniffer.</p>
          <Link href="/tutorial" className="btn" style={{marginTop: '15px', display: 'inline-block'}}>Read Guide</Link>
        </div>

        <div className="card" style={styles.card}>
          <h2>🔄 Updates</h2>
          <p>Check for firmware updates and flash via Web Serial.</p>
          <Link href="/updates" className="btn" style={{marginTop: '15px', display: 'inline-block'}}>Check Updates</Link>
        </div>
      </div>

      <div style={styles.footer}>
        <p>Ensure your device is plugged in via USB-C and set to Mode 2 (Security).</p>
        <p style={{fontSize: '0.8rem', color: 'var(--text-muted)', marginTop: '10px'}}>Supported browsers: Chrome, Edge, Opera (requires Web Serial API)</p>
      </div>
    </div>
  );
}

const styles = {
  container: {
    maxWidth: '1000px',
    margin: '0 auto',
    padding: '40px 20px',
    textAlign: 'center',
  },
  title: {
    fontSize: '4rem',
    marginBottom: '10px',
    textShadow: '0 0 20px rgba(0, 255, 204, 0.3)',
  },
  subtitle: {
    fontSize: '1.2rem',
    color: 'var(--text-muted)',
    marginBottom: '50px',
    fontFamily: 'var(--font-heading)',
  },
  grid: {
    display: 'grid',
    gridTemplateColumns: 'repeat(auto-fit, minmax(300px, 1fr))',
    gap: '20px',
    marginBottom: '50px',
  },
  card: {
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    justifyContent: 'center',
    padding: '30px',
    minHeight: '200px',
  },
  footer: {
    padding: '20px',
    borderTop: '1px solid var(--border)',
    color: 'var(--accent-cyan)',
  }
};
