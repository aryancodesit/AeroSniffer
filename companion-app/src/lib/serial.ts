class SerialProtocol {
  port: any;
  reader: any;
  writer: any;
  keepReading: boolean;
  readPromise: any;
  onMessage: any;
  onDisconnect: any;
  encoderClosed: any;

  constructor() {
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.keepReading = true;
    this.readPromise = null;
    this.onMessage = null; // Callback for receiving complete messages
    this.onDisconnect = null; // Callback for disconnection
  }

  async connect(baudRate = 115200) {
    if (!("serial" in navigator)) {
      throw new Error("Web Serial API not supported in this browser.");
    }

    try {
      this.port = await (navigator as any).serial.requestPort();
      await this.port.open({ baudRate });

      this.keepReading = true;
      this.readPromise = this.readLoop();

      // Setup disconnect listener
      (navigator as any).serial.addEventListener("disconnect", (event: any) => {
        if (event.target === this.port) {
          this.disconnect();
          if (this.onDisconnect) this.onDisconnect();
        }
      });

      return true;
    } catch (err) {
      console.error("Serial connection error:", err);
      throw err;
    }
  }

  async disconnect() {
    this.keepReading = false;

    if (this.reader) {
      await this.reader.cancel();
      this.reader = null;
    }

    if (this.readPromise) {
      await this.readPromise;
      this.readPromise = null;
    }

    if (this.writer) {
      this.writer.releaseLock();
      this.writer = null;
    }

    if (this.port) {
      await this.port.close();
      this.port = null;
    }
  }

  async readLoop() {
    let buffer = "";
    const textDecoder = new TextDecoderStream();
    const readableStreamClosed = this.port.readable.pipeTo(textDecoder.writable);
    this.reader = textDecoder.readable.getReader();

    try {
      while (this.keepReading) {
        const { value, done } = await this.reader.read();
        if (done) break;

        buffer += value;

        let newlineIdx;
        while ((newlineIdx = buffer.indexOf("\n")) >= 0) {
          const line = buffer.slice(0, newlineIdx).trim();
          buffer = buffer.slice(newlineIdx + 1);

          if (line && this.onMessage) {
            this.parseMessage(line);
          }
        }
      }
    } catch (error) {
      console.error("Read error:", error);
    } finally {
      this.reader.releaseLock();
      await textDecoder.writable.close();
      await readableStreamClosed;
    }
  }

  parseMessage(line: any) {
    if (line.startsWith("RES:")) {
      try {
        const data = JSON.parse(line.substring(4));
        this.onMessage("RES", data);
      } catch (e) {
        console.error("Failed to parse RES JSON:", line);
      }
    } else if (line.startsWith("EVT:")) {
      try {
        const data = JSON.parse(line.substring(4));
        this.onMessage("EVT", data);
      } catch (e) {
        console.error("Failed to parse EVT JSON:", line);
      }
    }
  }

  async sendCommand(cmd: any) {
    if (!this.port || !this.port.writable) return;

    if (!this.writer) {
      const textEncoder = new TextEncoderStream();
      this.encoderClosed = textEncoder.readable.pipeTo(this.port.writable);
      this.writer = textEncoder.writable.getWriter();
    }

    await this.writer.write(`CMD:${cmd}\n`);
  }
}

export const serialAPI = new SerialProtocol();
