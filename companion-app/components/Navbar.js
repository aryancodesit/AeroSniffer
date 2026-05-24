import Link from 'next/link';

export default function Navbar() {
  return (
    <nav style={styles.nav}>
      <div style={styles.logo}>
        <Link href="/">
          AeroSniffer
        </Link>
      </div>
      <div style={styles.links}>
        <Link href="/connect" style={styles.link}>Dashboard</Link>
        <Link href="/payloads" style={styles.link}>Payloads</Link>
        <Link href="/tutorial" style={styles.link}>Tutorial</Link>
        <Link href="/updates" style={styles.link}>Updates</Link>
      </div>
    </nav>
  );
}

const styles = {
  nav: {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: '15px 30px',
    backgroundColor: 'var(--bg-elevated)',
    borderBottom: '1px solid var(--border)',
  },
  logo: {
    fontFamily: 'var(--font-heading)',
    fontSize: '1.5rem',
    fontWeight: 'bold',
  },
  links: {
    display: 'flex',
    gap: '20px',
  },
  link: {
    fontFamily: 'var(--font-heading)',
    fontSize: '1rem',
    textTransform: 'uppercase',
  }
};
