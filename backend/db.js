const fs = require('fs');
const path = require('path');
const Database = require('better-sqlite3');

const dbPath = process.env.DB_PATH || path.join(__dirname, 'data', 'iot_os.db');

// Ensure database directory exists
const dbDir = path.dirname(dbPath);
if (!fs.existsSync(dbDir)) {
  fs.mkdirSync(dbDir, { recursive: true });
}

const db = new Database(dbPath);

// Enable WAL journal mode & set synchronous to NORMAL for performance & SSD protection
db.pragma('journal_mode = WAL');
db.pragma('synchronous = NORMAL');
db.pragma('foreign_keys = ON');

// Initialize database schema
db.exec(`
  CREATE TABLE IF NOT EXISTS devices (
    mac TEXT PRIMARY KEY,
    status TEXT DEFAULT 'offline',
    last_seen INTEGER,
    config TEXT
  );

  CREATE TABLE IF NOT EXISTS telemetry (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mac TEXT NOT NULL,
    pin TEXT NOT NULL,
    value TEXT NOT NULL,
    timestamp INTEGER NOT NULL
  );

  CREATE TABLE IF NOT EXISTS rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    flow_id TEXT NOT NULL,
    source_mac TEXT NOT NULL,       -- Alphanumeric MAC or 'system' (time trigger)
    source_pin TEXT NOT NULL,       -- GPIO/pin name or HH:MM / 'sunrise' / 'sunset'
    operator TEXT NOT NULL,         -- '==', '!=', '>', '<', 'none'
    value TEXT NOT NULL,            -- Condition value (or code script for JS nodes)
    target_mac TEXT NOT NULL,
    target_pin TEXT NOT NULL,       -- Target pin name or JS node id
    action_value TEXT NOT NULL      -- Target state/angle
  );

  CREATE INDEX IF NOT EXISTS idx_telemetry_mac_ts ON telemetry (mac, timestamp DESC);
  CREATE INDEX IF NOT EXISTS idx_telemetry_ts ON telemetry (timestamp);
  CREATE INDEX IF NOT EXISTS idx_rules_trigger ON rules (source_mac, source_pin);
`);

// Prepared Statements for performance
const statements = {
  upsertDevice: db.prepare(`
    INSERT INTO devices (mac, status, last_seen)
    VALUES (?, ?, ?)
    ON CONFLICT(mac) DO UPDATE SET
      status = excluded.status,
      last_seen = excluded.last_seen
  `),
  updateDeviceConfig: db.prepare(`
    INSERT INTO devices (mac, config, last_seen)
    VALUES (?, ?, ?)
    ON CONFLICT(mac) DO UPDATE SET
      config = excluded.config,
      last_seen = excluded.last_seen
  `),
  getDeviceConfig: db.prepare(`
    SELECT config FROM devices WHERE mac = ?
  `),
  insertTelemetry: db.prepare(`
    INSERT INTO telemetry (mac, pin, value, timestamp)
    VALUES (?, ?, ?, ?)
  `),
  deleteOldTelemetry: db.prepare(`
    DELETE FROM telemetry WHERE timestamp < ?
  `),
  insertRule: db.prepare(`
    INSERT INTO rules (flow_id, source_mac, source_pin, operator, value, target_mac, target_pin, action_value)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
  `),
  clearRules: db.prepare(`
    DELETE FROM rules
  `),
  getRulesForTrigger: db.prepare(`
    SELECT * FROM rules WHERE source_mac = ? AND source_pin = ?
  `),
  getScheduledRules: db.prepare(`
    SELECT * FROM rules WHERE source_mac = 'system'
  `)
};

// SQLite Transaction for batch telemetry insert
const bulkInsertTelemetry = db.transaction((records) => {
  for (const record of records) {
    statements.insertTelemetry.run(record.mac, record.pin, record.value, record.timestamp);
  }
});

module.exports = {
  db,
  statements,
  bulkInsertTelemetry
};
