const fs = require('fs');
const path = require('path');

// Set dummy env variables
process.env.DB_PATH = path.join(__dirname, 'data', 'test_iot.db');
process.env.TELEMETRY_FLUSH_INTERVAL_MS = '1500'; // 1.5 seconds for test
process.env.TELEMETRY_MAX_BUFFER_SIZE = '5'; // Flush at 5 items

// Clean previous test database
if (fs.existsSync(process.env.DB_PATH)) {
  fs.unlinkSync(process.env.DB_PATH);
}
if (fs.existsSync(`${process.env.DB_PATH}-wal`)) {
  fs.unlinkSync(`${process.env.DB_PATH}-wal`);
}

const { db } = require('./db');
const TelemetryBuffer = require('./buffer');

async function runTest() {
  console.log('[Test] Starting TelemetryBuffer unit tests...');
  const buffer = new TelemetryBuffer(process.env.TELEMETRY_FLUSH_INTERVAL_MS, process.env.TELEMETRY_MAX_BUFFER_SIZE);
  
  // Test 1: Periodic flushing with 3 items (below threshold 5)
  console.log('[Test 1] Adding 3 records (should flush after 1.5s interval)...');
  buffer.add('AA11BB22CC33', 'D1', '1');
  buffer.add('AA11BB22CC33', 'A0', '512');
  buffer.add('AA11BB22CC33', 'temp', '24.5');
  
  // Verify buffer state
  console.log(`[Test 1] Buffer size currently: ${buffer.buffer.length} (Expected: 3)`);
  
  // Wait for 2 seconds to let the interval flush execute
  await new Promise(resolve => setTimeout(resolve, 2000));
  
  let count = db.prepare('SELECT COUNT(*) as count FROM telemetry').get().count;
  console.log(`[Test 1] DB Row Count after interval: ${count} (Expected: 3)`);
  if (count !== 3) throw new Error('Test 1 Failed: Records not flushed on interval');
  
  // Test 2: Threshold flushing (exceeding max size 5)
  console.log('\n[Test 2] Adding 6 records rapidly (should trigger immediate overflow flush)...');
  for (let i = 0; i < 6; i++) {
    buffer.add('112233445566', `GPIO_${i}`, i * 10);
  }
  
  // Buffer size should be 1 (since 5 were flushed immediately and 1 remained)
  console.log(`[Test 2] Buffer size after overflow: ${buffer.buffer.length} (Expected: 1)`);
  
  // Yield to allow the async flush promise to resolve
  await new Promise(resolve => setImmediate(resolve));
  
  count = db.prepare('SELECT COUNT(*) as count FROM telemetry').get().count;
  console.log(`[Test 2] DB Row Count after overflow: ${count} (Expected: 8, which is 3 + 5)`);
  if (count !== 8) throw new Error('Test 2 Failed: Immediate overflow flush did not insert 5 records');

  // Test 3: Mutex lock concurrency test
  console.log('\n[Test 3] Simulating concurrent flush operations...');
  buffer.add('555555555555', 'L1', 'active');
  buffer.add('555555555555', 'L2', 'inactive');
  
  // Run concurrent flushes
  const flushPromise1 = buffer.flush();
  const flushPromise2 = buffer.flush();
  
  await Promise.all([flushPromise1, flushPromise2]);
  
  count = db.prepare('SELECT COUNT(*) as count FROM telemetry').get().count;
  console.log(`[Test 3] DB Row Count after concurrent flushes: ${count} (Expected: 11, which is 8 + 2 + 1 remaining from Test 2)`);
  if (count !== 11) throw new Error('Test 3 Failed: Concurrency issue occurred');
  
  // Clean up
  await buffer.close();
  db.close();
  
  // Remove test databases
  fs.unlinkSync(process.env.DB_PATH);
  if (fs.existsSync(`${process.env.DB_PATH}-wal`)) {
    fs.unlinkSync(`${process.env.DB_PATH}-wal`);
  }
  if (fs.existsSync(`${process.env.DB_PATH}-shm`)) {
    fs.unlinkSync(`${process.env.DB_PATH}-shm`);
  }
  
  console.log('\n[Test] ALL TESTS PASSED SUCCESSFULLY! Buffer is bulletproof.');
}

runTest().catch(err => {
  console.error('[Test] Test suite failed:', err);
  process.exit(1);
});
