const mqtt = require('mqtt');
const db = require('./db');
const { statements } = db;
const TelemetryBuffer = require('./buffer');
const { startCronJobs } = require('./cron');
const { initializeRulesEngine, evaluateGlobalRules, compileFlow } = require('./rules');
require('dotenv').config();

const mqttBrokerUrl = process.env.MQTT_BROKER_URL || 'mqtt://localhost:1883';
const flushIntervalMs = process.env.TELEMETRY_FLUSH_INTERVAL_MS || 300000;
const maxBufferSize = process.env.TELEMETRY_MAX_BUFFER_SIZE || 1000;

console.log('[App] Starting IoT OS backend...');

// Instantiate the RAM Buffer
const buffer = new TelemetryBuffer(flushIntervalMs, maxBufferSize);

// Start 03:00 maintenance cron
startCronJobs();

// Connect to Mosquitto Broker
const client = mqtt.connect(mqttBrokerUrl, {
  clientId: 'iot_backend_main_' + Math.random().toString(16).substring(2, 10),
  clean: true,
  keepalive: 60
});

client.on('connect', () => {
  console.log(`[MQTT] Connected to Broker at ${mqttBrokerUrl}`);
  
  // Subscribe to registry, LWT status, telemetry updates, and Vue-Flow uploads
  client.subscribe(['cihaz/kayit/+', 'cihaz/durum/+', 'cihaz/rapor/+', 'cihaz/flow'], (err) => {
    if (err) {
      console.error('[MQTT] Subscription error:', err);
    } else {
      console.log('[MQTT] Subscribed to topics: cihaz/kayit/+, cihaz/durum/+, cihaz/rapor/+, cihaz/flow');
    }
  });

  // Initialize Global Rules Engine & Scheduler
  initializeRulesEngine(client);
});

client.on('message', (topic, message) => {
  const payloadStr = message.toString().trim();

  // 1. Vue-Flow Configuration compiler endpoint
  if (topic === 'cihaz/flow') {
    try {
      const flowData = JSON.parse(payloadStr);
      const compileResult = compileFlow(flowData);
      console.log(`[Compiler] Flow compilation successful. Local rules pushed: ${compileResult.localCount}`);
    } catch (err) {
      console.error('[Compiler] Failed to parse/compile Vue-Flow payload:', err);
    }
    return;
  }

  const parts = topic.split('/');
  if (parts.length < 3) return;
  
  const type = parts[1];
  const mac = parts[2].toUpperCase().replace(/[^A-Z0-9]/g, ''); // Standard clean MAC
  const now = Date.now();
  
  try {
    if (type === 'kayit') {
      console.log(`[Device] Register: Cihaz ${mac} online. Payload: "${payloadStr}"`);
      statements.upsertDevice.run(mac, 'online', now);
    } else if (type === 'durum') {
      const status = payloadStr.toLowerCase() === 'offline' ? 'offline' : 'online';
      console.log(`[Device] LWT Status: Cihaz ${mac} marked "${status}"`);
      statements.upsertDevice.run(mac, status, now);
    } else if (type === 'rapor') {
      let data;
      try {
        data = JSON.parse(payloadStr);
      } catch (err) {
        // Fallback to simple string-to-value wrapper if message is not JSON
        data = { "value": payloadStr };
      }
      
      // Buffer each telemetry key-value pair and run automation triggers
      for (const [pin, val] of Object.entries(data)) {
        buffer.add(mac, pin, val, now);
        
        // Evaluate global rule connections
        evaluateGlobalRules(mac, pin, val);
      }
      
      // Update last seen status to online
      statements.upsertDevice.run(mac, 'online', now);
    }
  } catch (err) {
    console.error(`[App] Error handling message on topic "${topic}":`, err);
  }
});

client.on('error', (err) => {
  console.error('[MQTT] Connection error:', err);
});

// Express.js REST API and Static File Server
const express = require('express');
const path = require('path');
const fs = require('fs');
const app = express();
const port = process.env.PORT || 3000;

app.use(express.json());

// Serve static assets of Vue 3 Frontend production build
const distPath = process.env.NODE_ENV === 'production'
  ? path.join(__dirname, 'frontend/dist')
  : path.join(__dirname, '../frontend/dist');

app.use(express.static(distPath));

// REST APIs
// 1. Get List of Devices and Statuses (hydrated with latest pin states)
app.get('/api/devices', (req, res) => {
  try {
    const rows = db.db.prepare('SELECT * FROM devices').all();
    const pinStatesStmt = db.db.prepare(`
      SELECT pin, value FROM (
        SELECT pin, value FROM telemetry WHERE mac = ? ORDER BY timestamp DESC
      ) GROUP BY pin
    `);

    const devices = rows.map(r => {
      const states = {};
      const pinStates = pinStatesStmt.all(r.mac);
      for (const row of pinStates) {
        states[row.pin] = row.value;
      }

      return {
        mac: r.mac,
        status: r.status,
        last_seen: r.last_seen,
        config: r.config ? JSON.parse(r.config) : null,
        state: states
      };
    });
    res.json(devices);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// 2. Post dynamic configuration to a device (pushes pins over MQTT and saves to SQLite)
app.post('/api/config/:mac', (req, res) => {
  const mac = req.params.mac.toUpperCase().replace(/[^A-Z0-9]/g, '');
  const pinConfig = req.body; // array of pin setups
  try {
    const payloadStr = JSON.stringify(pinConfig);
    client.publish(`cihaz/config/${mac}`, payloadStr, { qos: 1, retain: true });
    statements.updateDeviceConfig.run(mac, payloadStr, Date.now());
    res.json({ success: true });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// 2b. Direct Pin control action over HTTP (publishes to cihaz/kontrol topic)
app.post('/api/control/:mac/:pin', (req, res) => {
  const mac = req.params.mac.toUpperCase().replace(/[^A-Z0-9]/g, '');
  const pin = req.params.pin;
  const { value } = req.body;
  try {
    client.publish(`cihaz/kontrol/${mac}/${pin}`, String(value), { qos: 1, retain: false });
    res.json({ success: true });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// 3. Query telemetry records
app.get('/api/telemetry', (req, res) => {
  const limit = req.query.limit ? parseInt(req.query.limit) : 100;
  try {
    const rows = db.db.prepare('SELECT * FROM telemetry ORDER BY timestamp DESC LIMIT ?').all(limit);
    res.json(rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// 4. Post Vue-Flow diagram config (runs partition compiler)
app.post('/api/flow', (req, res) => {
  try {
    const flowData = req.body;
    const compileResult = compileFlow(flowData);
    
    // Save raw flow config locally for persistence
    const dataDir = path.join(__dirname, 'data');
    if (!fs.existsSync(dataDir)) {
      fs.mkdirSync(dataDir, { recursive: true });
    }
    fs.writeFileSync(path.join(dataDir, 'flow.json'), JSON.stringify(flowData, null, 2));
    
    res.json({ success: true, localCount: compileResult.localCount });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// 5. Get saved Vue-Flow config diagram
app.get('/api/flow', (req, res) => {
  const flowPath = path.join(__dirname, 'data', 'flow.json');
  if (fs.existsSync(flowPath)) {
    res.sendFile(flowPath);
  } else {
    res.json({ nodes: [], edges: [] });
  }
});

// Fallback to index.html for SPA (Single Page Application) routing
app.get('*', (req, res) => {
  res.sendFile(path.join(distPath, 'index.html'));
});

const httpServer = app.listen(port, () => {
  console.log(`[HTTP] Express server hosting dashboard on port ${port}`);
});

// Graceful exit handler
async function shutdown() {
  console.log('\n[App] Process terminating. Reclaiming resources...');
  
  if (httpServer) {
    console.log('[HTTP] Closing server...');
    httpServer.close();
  }

  if (client && client.connected) {
    console.log('[MQTT] Disconnecting client...');
    client.end();
  }
  
  console.log('[Buffer] Performing final flush...');
  await buffer.close();
  
  console.log('[DB] Closing database connection...');
  db.db.close();
  
  console.log('[App] Exit safe.');
  process.exit(0);
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
