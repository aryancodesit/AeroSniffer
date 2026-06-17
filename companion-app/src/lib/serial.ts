/* eslint-disable @typescript-eslint/no-explicit-any */
class SerialProtocol {
  port: any;
  reader: any;
  writer: any;
  keepReading: boolean;
  readPromise: any;
  listeners: Set<(type: string, data: any) => void>;
  connectListeners: Set<() => void>;
  disconnectListeners: Set<() => void>;
  encoderClosed: any;

  constructor() {
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.keepReading = true;
    this.readPromise = null;
    this.listeners = new Set();
    this.connectListeners = new Set();
    this.disconnectListeners = new Set();
  }

  // Modern Subscription API
  addListener(cb: (type: string, data: any) => void) {
    this.listeners.add(cb);
    return () => {
      this.listeners.delete(cb);
    };
  }

  addConnectListener(cb: () => void) {
    this.connectListeners.add(cb);
    return () => {
      this.connectListeners.delete(cb);
    };
  }

  addDisconnectListener(cb: () => void) {
    this.disconnectListeners.add(cb);
    return () => {
      this.disconnectListeners.delete(cb);
    };
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

      // Trigger connect listeners
      this.connectListeners.forEach((cb) => cb());

      // Setup disconnect listener
      (navigator as any).serial.addEventListener("disconnect", (event: any) => {
        if (event.target === this.port) {
          this.disconnect();
          this.disconnectListeners.forEach((cb) => cb());
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
      try {
        await this.port.close();
      } catch (e) {
        // ignore
      }
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

          if (line) {
            this.parseMessage(line);
          }
        }
      }
    } catch (error) {
      console.error("Read error:", error);
    } finally {
      this.reader.releaseLock();
      try {
        await textDecoder.writable.close();
      } catch (e) {
        // stream already closing
      }
      try {
        await readableStreamClosed;
      } catch (e) {
        // stream already closed
      }
    }
  }

  parseMessage(line: any) {
    if (line.startsWith("RES:")) {
      try {
        const jsonStr = line.substring(4);
        const data = JSON.parse(jsonStr);
        // Flight pipeline instrumentation
        if (data.flights !== undefined) {
          console.log(`[WEB] Received aircraft=${Array.isArray(data.flights) ? data.flights.length : 0}`, data.flights);
        }
        this.listeners.forEach((cb) => cb("RES", data));
      } catch (e) {
        console.error("[WEB] Failed to parse RES JSON:", line.substring(0, 200), e);
      }
    } else if (line.startsWith("EVT:")) {
      try {
        const data = JSON.parse(line.substring(4));
        this.listeners.forEach((cb) => cb("EVT", data));
      } catch (e) {
        console.error("Failed to parse EVT JSON:", line);
      }
    }
  }

  async sendCommand(cmd: any, raw = false) {
    if (!this.port || !this.port.writable) return;

    if (!this.writer) {
      const textEncoder = new TextEncoderStream();
      this.encoderClosed = textEncoder.readable.pipeTo(this.port.writable);
      this.writer = textEncoder.writable.getWriter();
    }

    const payload = raw ? `${cmd}\n` : `CMD:${cmd}\n`;
    await this.writer.write(payload);
  }
}

export const serialAPI = new SerialProtocol();
