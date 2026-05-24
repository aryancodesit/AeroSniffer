import "./globals.css";
import Navbar from "@/components/Navbar";

export const metadata = {
  title: "AeroSniffer Companion",
  description: "Web interface for AeroSniffer Mode 2 Security Monitor",
};

export default function RootLayout({ children }) {
  return (
    <html lang="en">
      <body>
        <Navbar />
        <main style={{ flex: 1, padding: "20px" }}>
          {children}
        </main>
      </body>
    </html>
  );
}
