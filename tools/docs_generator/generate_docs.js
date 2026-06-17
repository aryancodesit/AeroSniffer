const fs = require('fs');
const path = require('path');
const pptxgen = require('pptxgenjs');
const docx = require('docx');
const { Document, Packer, Paragraph, TextRun, HeadingLevel, Table, TableRow, TableCell, BorderStyle, WidthType } = docx;

// Output paths
const docsDir = path.join(__dirname, '..', '..', 'docs');
if (!fs.existsSync(docsDir)) fs.mkdirSync(docsDir);

const pptxPath = path.join(docsDir, 'AeroSniffer_Presentation.pptx');
const docxPath = path.join(docsDir, 'AeroSniffer_Manual.docx');

// --- 1. Generate PPTX ---
async function generatePPTX() {
    console.log('Generating PPTX...');
    let pptx = new pptxgen();

    // Set Presentation properties
    pptx.author = 'AeroSniffer Developer';
    pptx.company = 'AeroSniffer Project';
    pptx.revision = '2';
    pptx.subject = 'AeroSniffer Documentation — DeskBuddy 2.0 Build';
    pptx.title = 'AeroSniffer - XIAO ESP32S3 Multi-Boot Desk Gadget';
    pptx.layout = 'LAYOUT_16x9';

    // Define Master Slides (Dark Tech Theme)
    pptx.defineSlideMaster({
        title: 'MASTER_SLIDE',
        background: { color: '1A1A1A' }, // Dark Slate/Black
        objects: [
            { rect: { x: 0, y: 0, w: '100%', h: 0.2, fill: { color: '00FFCC' } } }, // Cyan Top Accent
            { rect: { x: 0, y: 5.4, w: '100%', h: 0.2, fill: { color: '39FF14' } } } // Green Bottom Accent
        ],
        slideNumber: { x: '95%', y: '95%', fontFace: 'Courier New', fontSize: 10, color: 'FFFFFF' }
    });

    const TITLE_STYLE = { fontFace: 'Arial', fontSize: 32, color: '00FFCC', bold: true, align: 'center', y: 0.5 };
    const TEXT_STYLE = { fontFace: 'Arial', fontSize: 20, color: 'FFFFFF', align: 'left', y: 1.5, bullet: true };

    // Slide 1: Cover
    let s1 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s1.addText('AeroSniffer', { ...TITLE_STYLE, fontSize: 56, y: 1.8, color: '00FFCC' });
    s1.addText('XIAO ESP32S3 Multi-Boot Desk Gadget', { ...TEXT_STYLE, fontSize: 24, align: 'center', y: 3.0, bullet: false, color: 'FFFFFF' });
    s1.addText('DeskBuddy 2.0 Kit Build  |  v2.0', { fontFace: 'Courier New', fontSize: 16, color: '39FF14', align: 'center', y: 3.8 });
    s1.addText('By aryancodesit', { fontFace: 'Courier New', fontSize: 14, color: 'AAAAAA', align: 'center', y: 4.5 });

    // Slide 2: Overview
    let s2 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s2.addText('What is AeroSniffer?', TITLE_STYLE);
    s2.addText([
        { text: 'Three separate devices in one tiny XIAO ESP32S3 chip' },
        { text: 'Mode 1: The Companion — Interactive desk pet with animated eyes' },
        { text: 'Mode 2: Security Sentinel — 802.11 network monitor + web UI dashboard' },
        { text: 'Mode 3: Flight Radar — Live ADS-B tracker from OpenSky Network' },
        { text: 'Instant mode switching via 1.5s long-press on touch sensor' },
        { text: 'Built on FreeRTOS dual-core architecture (Core 0: data, Core 1: UI)' }
    ], { ...TEXT_STYLE, y: 1.8 });

    // Slide 3: Hardware Kit
    let s3 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s3.addText('Hardware: DeskBuddy 2.0 Kit', TITLE_STYLE);
    s3.addText('One purchase. Everything included. Rs 2,299.', { fontFace: 'Arial', fontSize: 18, color: '39FF14', align: 'center', y: 1.3 });
    let bomData = [
        [{ text: 'Component', options: { bold: true, color: '1A1A1A', fill: { color: '00FFCC' } } }, { text: 'Specification', options: { bold: true, color: '1A1A1A', fill: { color: '00FFCC' } } }],
        [{ text: 'Microcontroller', options: { color: 'FFFFFF' } }, { text: 'Seeed XIAO ESP32S3 (8MB Flash, 8MB PSRAM)', options: { color: 'FFFFFF' } }],
        [{ text: 'Display', options: { color: 'FFFFFF' } }, { text: '1.3" ST7789 240x240 Square IPS (SPI)', options: { color: 'FFFFFF' } }],
        [{ text: 'Touch Input', options: { color: 'FFFFFF' } }, { text: 'Red Capacitive Touch Switch Module', options: { color: 'FFFFFF' } }],
        [{ text: 'Battery', options: { color: 'FFFFFF' } }, { text: '3.7V LiPo (matched to enclosure)', options: { color: 'FFFFFF' } }],
        [{ text: 'Enclosure', options: { color: 'FFFFFF' } }, { text: '3D Printed Shell (pre-made)', options: { color: 'FFFFFF' } }],
        [{ text: 'Extras', options: { color: 'FFFFFF' } }, { text: 'On/Off switch + USB-C cable', options: { color: 'FFFFFF' } }]
    ];
    s3.addTable(bomData, { x: 1, y: 1.8, w: 8, fill: { color: '333333' }, fontSize: 16, border: { type: 'solid', color: '555555' } });

    // Slide 4: Wiring
    let s4 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s4.addText('Wiring — Only 10 Wires!', TITLE_STYLE);
    let wiringData = [
        [{ text: 'Module', options: { bold: true, color: '1A1A1A', fill: { color: '39FF14' } } }, { text: 'XIAO Pins', options: { bold: true, color: '1A1A1A', fill: { color: '39FF14' } } }, { text: 'GPIO #', options: { bold: true, color: '1A1A1A', fill: { color: '39FF14' } } }],
        [{ text: 'TFT MOSI', options: { color: 'FFFFFF' } }, { text: 'D10', options: { color: 'FFFFFF' } }, { text: 'GPIO 9', options: { color: 'FFFFFF' } }],
        [{ text: 'TFT SCLK', options: { color: 'FFFFFF' } }, { text: 'D8', options: { color: 'FFFFFF' } }, { text: 'GPIO 7', options: { color: 'FFFFFF' } }],
        [{ text: 'TFT CS', options: { color: 'FFFFFF' } }, { text: 'D3', options: { color: 'FFFFFF' } }, { text: 'GPIO 4', options: { color: 'FFFFFF' } }],
        [{ text: 'TFT DC', options: { color: 'FFFFFF' } }, { text: 'D2', options: { color: 'FFFFFF' } }, { text: 'GPIO 3', options: { color: 'FFFFFF' } }],
        [{ text: 'TFT RST', options: { color: 'FFFFFF' } }, { text: 'D9', options: { color: 'FFFFFF' } }, { text: 'GPIO 8', options: { color: 'FFFFFF' } }],
        [{ text: 'Touch SIG', options: { color: 'FFFFFF' } }, { text: 'D0', options: { color: 'FFFFFF' } }, { text: 'GPIO 1', options: { color: 'FFFFFF' } }],
        [{ text: 'VCC + GND', options: { color: 'AAAAAA' } }, { text: '3V3 + GND', options: { color: 'AAAAAA' } }, { text: '(x2 modules)', options: { color: 'AAAAAA' } }]
    ];
    s4.addTable(wiringData, { x: 0.8, y: 1.5, w: 8.4, fill: { color: '333333' }, fontSize: 16, border: { type: 'solid', color: '555555' } });

    // Slide 5: Mode 1
    let s5 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s5.addText('Mode 1: The Companion', TITLE_STYLE);
    s5.addText([
        { text: 'Interactive desktop pet with OLED-style animated eyes' },
        { text: 'Touch Sensor: Tap the roof to "pet" it — triggers happy expression' },
        { text: 'Emotions: Idle, Happy, Blink, Sleeping (auto-cycles)' },
        { text: 'Blush marks, glare dots, and smooth smile arcs' },
        { text: 'Face perfectly centered on the 240x240 square display' },
        { text: 'Long-press (1.5s) on touch sensor = switch to Mode 2' }
    ], { ...TEXT_STYLE, y: 1.8 });

    // Slide 6: Mode 2
    let s6 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s6.addText('Mode 2: Security Monitor', TITLE_STYLE);
    s6.addText([
        { text: 'Web-UI Hybrid Architecture (optimized for 1.3" screen)' },
        { text: 'ON SCREEN: Animated radar sweep + live PKT/s bar + stats' },
        { text: 'ON PHONE: Connect to "AeroSniffer-SEC" WiFi AP' },
        { text: 'Web UI at http://192.168.4.1 — start/stop scans, view packets' },
        { text: 'Promiscuous 802.11 capture with channel hopping' },
        { text: 'Deauth spike alerting (>10 frames in 5s = ALERT)' },
        { text: 'Companion desktop app (planned) for USB Serial control' }
    ], { ...TEXT_STYLE, y: 1.8 });

    // Slide 7: Mode 3
    let s7 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s7.addText('Mode 3: Flight Radar', TITLE_STYLE);
    s7.addText([
        { text: 'Live ADS-B flight tracker via OpenSky Network API' },
        { text: 'Connects to home WiFi (STA mode) — polls every 15s' },
        { text: 'Compact flight cards: callsign, altitude, speed, heading' },
        { text: 'Mini compass rose + radar dot-map on each card' },
        { text: 'Auto-scrolls through detected flights in your area' },
        { text: 'Customizable GPS bounding box in Config.h' }
    ], { ...TEXT_STYLE, y: 1.8 });

    // Slide 8: Enclosure
    let s8 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s8.addText('DeskBuddy 2.0 Enclosure', TITLE_STYLE);
    s8.addText([
        { text: 'Pre-made 3D-printed shell — included in the kit' },
        { text: 'Ultra-compact form factor (XIAO is 21mm x 17.5mm!)' },
        { text: 'Display window: pre-cut for 1.3" ST7789 module' },
        { text: 'Touch sensor: mounted under the roof for head-pats' },
        { text: 'USB-C slot: access port for charging and firmware updates' },
        { text: 'Battery compartment: sized for the included LiPo cell' },
        { text: 'Looks like a premium consumer product on your desk' }
    ], { ...TEXT_STYLE, y: 1.8 });

    // Slide 9: Software Setup
    let s9 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s9.addText('Software Setup', TITLE_STYLE);
    s9.addText([
        { text: 'IDE: Arduino IDE 2.x' },
        { text: 'Board: XIAO_ESP32S3 (esp32 by Espressif Systems v3.0.x+)' },
        { text: 'Settings: 8MB Flash, OPI PSRAM, USB CDC On Boot: Enabled' },
        { text: 'Required Libraries: TFT_eSPI (display) + ArduinoJson (API)' },
        { text: 'Only 2 libraries needed! (mic/DAC/IMU not wired = not needed)' }
    ], { ...TEXT_STYLE, y: 1.8 });

    // Slide 10: Configuration
    let s10 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s10.addText('Configuration — 2 Easy Steps', TITLE_STYLE);
    s10.addText([
        { text: '1. Copy TFT_eSPI_UserSetup.h → Arduino/libraries/TFT_eSPI/User_Setup.h' },
        { text: '   Pre-configured for ST7789 240x240 + XIAO SPI pins' },
        { text: '2. Edit Config.h:' },
        { text: '   Set WIFI_SSID and WIFI_PASSWORD' },
        { text: '   Set GPS bounding box (boundingbox.klokantech.com)' },
        { text: '   Hardware variant HW_DESKBUDDY_2 is already selected' }
    ], { ...TEXT_STYLE, y: 1.8 });

    // Slide 11: Flash & Test
    let s11 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s11.addText('Flash & Test', TITLE_STYLE);
    s11.addText([
        { text: 'Step 1: Upload SPIFFS data (airlines.db) via LittleFS plugin' },
        { text: 'Step 2: Open AeroSniffer.ino → Click Upload' },
        { text: 'Step 3: Boot splash appears → Mode 1 (Pet) loads' },
        { text: 'Step 4: Tap touch sensor → pet goes happy!' },
        { text: 'Step 5: Long-press (1.5s) → cycles through all 3 modes' },
        { text: 'If upload fails: Hold BOOT → Press RESET → Release BOOT' }
    ], { ...TEXT_STYLE, y: 1.8 });

    // Slide 12: Companion App (Future)
    let s12 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s12.addText('Companion App (Planned)', TITLE_STYLE);
    s12.addText([
        { text: 'Web app (Vercel) or desktop .exe — connects via USB Serial' },
        { text: 'Full network control panel when device is plugged in' },
        { text: 'Firmware update checker and OTA capability' },
        { text: 'Built-in how-to tutorial and payload library' },
        { text: 'Real-time packet visualization and export' }
    ], { ...TEXT_STYLE, y: 1.8 });

    // Slide 13: Architecture
    let s13 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s13.addText('Software Architecture', TITLE_STYLE);
    s13.addText([
        { text: 'FreeRTOS dual-core task routing:' },
        { text: '   Core 0: Background — WiFi, packet capture, API fetch' },
        { text: '   Core 1: UI Engine — TFT rendering, touch polling, state machine' },
        { text: 'Feature flags (#if HAS_MIC, HAS_DAC, etc.) for optional hardware' },
        { text: 'Dual hardware profiles: HW_DESKBUDDY_2 and HW_DEVKITC' },
        { text: 'Clean mode teardown/setup — no reboot needed between modes' }
    ], { ...TEXT_STYLE, y: 1.8 });

    // Slide 14: Questions
    let s14 = pptx.addSlide({ masterName: 'MASTER_SLIDE' });
    s14.addText('Questions?', { ...TITLE_STYLE, fontSize: 48, y: 2.0 });
    s14.addText('Enjoy your AeroSniffer!', { ...TEXT_STYLE, fontSize: 24, align: 'center', y: 3.2, bullet: false });
    s14.addText('github.com/aryancodesit/AeroSniffer', { fontFace: 'Courier New', fontSize: 16, color: '39FF14', align: 'center', y: 4.0 });
    s14.addText('Built on DeskBuddy 2.0 Kit  |  Rs 2,299  |  esclabs.in', { fontFace: 'Courier New', fontSize: 12, color: 'AAAAAA', align: 'center', y: 4.5 });

    await pptx.writeFile({ fileName: pptxPath });
    console.log(`Saved PPTX to ${pptxPath}`);
}

// --- 2. Generate DOCX ---
async function generateDOCX() {
    console.log('Generating DOCX...');

    const doc = new Document({
        sections: [{
            properties: {},
            children: [
                new Paragraph({
                    text: "AeroSniffer User Manual",
                    heading: HeadingLevel.TITLE,
                    alignment: docx.AlignmentType.CENTER,
                }),
                new Paragraph({
                    text: "XIAO ESP32S3 Multi-Boot Desk Gadget — DeskBuddy 2.0 Build",
                    heading: HeadingLevel.HEADING_1,
                    alignment: docx.AlignmentType.CENTER,
                }),
                new Paragraph({ text: "" }),
                
                // Section 1: Introduction
                new Paragraph({ text: "1. Introduction", heading: HeadingLevel.HEADING_2 }),
                new Paragraph({
                    text: "AeroSniffer treats the XIAO ESP32S3 like a mini operating system with three completely separate identities. Long-press the capacitive touch sensor (1.5 seconds) to switch between a living desktop companion, a wireless security monitor, and a real-time flight radar — all running inside a DeskBuddy 2.0 enclosure."
                }),
                new Paragraph({ text: "" }),

                // Section 2: Hardware
                new Paragraph({ text: "2. Hardware — DeskBuddy 2.0 Kit (Rs 2,299)", heading: HeadingLevel.HEADING_2 }),
                new Paragraph({ text: "Purchase from: https://www.esclabs.in/product/deskbuddy-2-0-kit/" }),
                new Paragraph({ text: "The kit includes everything you need. No extra purchases required:" }),
                new Paragraph({ text: "- Seeed Studio XIAO ESP32S3 (8MB Flash, 8MB PSRAM, WiFi + BLE 5.0)", bullet: { level: 0 } }),
                new Paragraph({ text: "- 1.3 inch ST7789 IPS Display (240x240 square, SPI interface)", bullet: { level: 0 } }),
                new Paragraph({ text: "- Red Capacitive Touch Switch Module (digital output, active LOW)", bullet: { level: 0 } }),
                new Paragraph({ text: "- 3.7V LiPo Battery (matched to enclosure)", bullet: { level: 0 } }),
                new Paragraph({ text: "- On/Off Switch", bullet: { level: 0 } }),
                new Paragraph({ text: "- 3D Printed Enclosure (pre-made, professionally finished)", bullet: { level: 0 } }),
                new Paragraph({ text: "- USB-C Cable (data + power)", bullet: { level: 0 } }),
                new Paragraph({ text: "" }),

                // Section 3: Wiring
                new Paragraph({ text: "3. Wiring Guide (10 Wires Total)", heading: HeadingLevel.HEADING_2 }),
                new Paragraph({ text: "Connect the modules to the XIAO ESP32S3 pins as follows:" }),
                new Paragraph({ text: "" }),
                new Paragraph({ text: "ST7789 Display (SPI):", bold: true }),
                new Paragraph({ text: "  MOSI = D10 (GPIO 9)" }),
                new Paragraph({ text: "  SCLK = D8  (GPIO 7)" }),
                new Paragraph({ text: "  CS   = D3  (GPIO 4)" }),
                new Paragraph({ text: "  DC   = D2  (GPIO 3)" }),
                new Paragraph({ text: "  RST  = D9  (GPIO 8)" }),
                new Paragraph({ text: "  VCC  = 3V3,  GND = GND" }),
                new Paragraph({ text: "  BL   = Not connected (always-on on carrier board)" }),
                new Paragraph({ text: "" }),
                new Paragraph({ text: "Capacitive Touch Module:", bold: true }),
                new Paragraph({ text: "  SIG  = D0  (GPIO 1)  — active LOW when touched" }),
                new Paragraph({ text: "  VCC  = 3V3,  GND = GND" }),
                new Paragraph({ text: "" }),
                new Paragraph({ text: "Touch sensor placement: Mount under the roof of the 3D-printed shell. Capacitive sensing works through thin plastic (2-3mm)." }),
                new Paragraph({ text: "" }),

                // Section 4: Software Setup
                new Paragraph({ text: "4. Software Setup", heading: HeadingLevel.HEADING_2 }),
                new Paragraph({ text: "1. Install Arduino IDE 2.x from https://www.arduino.cc/en/software" }),
                new Paragraph({ text: "2. Add ESP32 board support URL in Preferences: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json" }),
                new Paragraph({ text: "3. Install 'esp32 by Espressif Systems' v3.0.x+ from Boards Manager." }),
                new Paragraph({ text: "4. Select board: XIAO_ESP32S3. Enable 'USB CDC On Boot'." }),
                new Paragraph({ text: "5. Install libraries: TFT_eSPI (by Bodmer), ArduinoJson (by Benoit Blanchon)." }),
                new Paragraph({ text: "6. Copy AeroSniffer/TFT_eSPI_UserSetup.h to the TFT_eSPI library folder (User_Setup.h). It is pre-configured for ST7789 240x240 with XIAO SPI pins." }),
                new Paragraph({ text: "7. Edit AeroSniffer/Config.h to set your WiFi credentials and GPS bounding box." }),
                new Paragraph({ text: "8. Upload SPIFFS data using the LittleFS plugin (airlines.db for Mode 3)." }),
                new Paragraph({ text: "9. Compile and Flash AeroSniffer.ino." }),
                new Paragraph({ text: "" }),

                // Section 5: Operating Manual
                new Paragraph({ text: "5. Operating Manual", heading: HeadingLevel.HEADING_2 }),
                new Paragraph({ text: "Touch interaction:", bold: true }),
                new Paragraph({ text: "  Short tap (< 1.5s) = Context action (pet pat in Mode 1)" }),
                new Paragraph({ text: "  Long press (>= 1.5s) = Switch to next mode" }),
                new Paragraph({ text: "" }),
                new Paragraph({ text: "Mode 1 — The Companion: Animated desk pet with expressive eyes. Tap the touch sensor to trigger the happy face. The pet blinks automatically and cycles through idle expressions." }),
                new Paragraph({ text: "" }),
                new Paragraph({ text: "Mode 2 — Security Sentinel: The 1.3 inch screen shows an animated radar sweep with live network statistics (PKT/s, beacons, probes, events). For full control, connect your phone to the 'AeroSniffer-SEC' WiFi network and open http://192.168.4.1 in a browser. The web UI lets you start/stop scans and view detailed network data." }),
                new Paragraph({ text: "" }),
                new Paragraph({ text: "Mode 3 — Flight Radar: Connects to your home WiFi and polls the OpenSky Network API every 15 seconds. Displays compact flight cards with callsign, altitude, ground speed, heading, and a mini compass rose + radar map." }),
                new Paragraph({ text: "" }),

                // Section 6: Companion App
                new Paragraph({ text: "6. Companion App (Planned)", heading: HeadingLevel.HEADING_2 }),
                new Paragraph({ text: "A web application (deployed on Vercel) or desktop executable is planned for Mode 2 control when the device is connected via USB. Features will include:" }),
                new Paragraph({ text: "- Full network control panel via USB Serial", bullet: { level: 0 } }),
                new Paragraph({ text: "- Firmware update checker and OTA capability", bullet: { level: 0 } }),
                new Paragraph({ text: "- Built-in how-to tutorial and payload library", bullet: { level: 0 } }),
                new Paragraph({ text: "- Real-time packet visualization and PCAP export", bullet: { level: 0 } }),
                new Paragraph({ text: "" }),

                // Section 7: Troubleshooting
                new Paragraph({ text: "7. Troubleshooting", heading: HeadingLevel.HEADING_2 }),
                new Paragraph({ text: "White screen: Check TFT_eSPI User_Setup.h was correctly replaced. Verify SPI wiring." }),
                new Paragraph({ text: "Upload fails: Put XIAO in download mode — Hold BOOT, press RESET, release BOOT. Ensure USB cable supports data." }),
                new Paragraph({ text: "No serial output: Enable 'USB CDC On Boot' in Arduino IDE board settings." }),
                new Paragraph({ text: "Touch not working: Verify D0 (GPIO 1) is wired to the SIG/OUT pin of the touch module." }),
                new Paragraph({ text: "WiFi FAILED in Mode 3: Check SSID/password are correct and router is 2.4GHz." }),
            ],
        }],
    });

    const buffer = await Packer.toBuffer(doc);
    fs.writeFileSync(docxPath, buffer);
    console.log(`Saved DOCX to ${docxPath}`);
}

async function main() {
    try {
        await generatePPTX();
        await generateDOCX();
        console.log('Success!');
    } catch (err) {
        console.error('Error generating docs:', err);
    }
}

main();
