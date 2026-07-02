const { bulkInsertTelemetry } = require('./db');

class TelemetryBuffer {
  constructor(flushIntervalMs = 300000, maxBufferSize = 1000) {
    this.buffer = [];
    this.flushIntervalMs = Number(flushIntervalMs);
    this.maxBufferSize = Number(maxBufferSize);
    this.timer = null;
    this.writeLock = Promise.resolve(); // Simple Mutex chain

    this.startTimer();
  }

  add(mac, pin, value, timestamp) {
    this.buffer.push({ 
      mac, 
      pin, 
      value: String(value), 
      timestamp: timestamp || Date.now() 
    });
    
    if (this.buffer.length >= this.maxBufferSize) {
      console.log(`[Buffer] Buffer limit reached (${this.buffer.length}). Triggering early flush.`);
      this.flush();
    }
  }

  startTimer() {
    this.timer = setInterval(() => {
      if (this.buffer.length > 0) {
        console.log(`[Buffer] Interval reached. Flushing ${this.buffer.length} records.`);
        this.flush();
      }
    }, this.flushIntervalMs);
  }

  async flush() {
    if (this.buffer.length === 0) return this.writeLock;

    const recordsToInsert = [...this.buffer];
    this.buffer = []; // Clear buffer synchronously to handle immediate synchronous add calls

    // Chain onto the writeLock to prevent concurrent database writes
    this.writeLock = this.writeLock.then(async () => {
      try {
        const start = Date.now();
        bulkInsertTelemetry(recordsToInsert);
        const duration = Date.now() - start;
        console.log(`[Buffer] Successfully flushed ${recordsToInsert.length} records to DB in ${duration}ms.`);
      } catch (err) {
        console.error('[Buffer] Error writing bulk telemetry to SQLite:', err);
        // Prepend failed records back to buffer to retry
        this.buffer = [...recordsToInsert, ...this.buffer];
      }
    }).catch(err => {
      console.error('[Buffer] Unexpected error in write lock queue:', err);
    });

    return this.writeLock;
  }

  close() {
    if (this.timer) {
      clearInterval(this.timer);
    }
    return this.flush(); // Synchronous final flush on termination
  }
}

module.exports = TelemetryBuffer;
