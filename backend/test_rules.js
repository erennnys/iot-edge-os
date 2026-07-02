const { db } = require('./db');
const { getSolarTimes } = require('./astronomy');
const { compileFlow, initializeRulesEngine, evaluateGlobalRules } = require('./rules');

async function runTest() {
  console.log('[Test] Starting astronomy & rules engine test suite...');

  // Test 1: Astronomy calculations
  const solar = getSolarTimes(new Date());
  console.log(`[Test 1] Solar Times for Istanbul: Sunrise=${solar.sunrise}, Sunset=${solar.sunset}`);
  if (!solar.sunrise || !solar.sunset) {
    throw new Error('Test 1 Failed: Sunrise/Sunset calculations returned invalid values');
  }
  console.log('[Test 1] Astronomy calculations PASSED.');

  // Test 2: Flow Compilation (Local and Global partition)
  // Mock flow structure:
  // Node 1: Input Pin 5 on Device A (112233445566)
  // Node 2: Condition "== 0"
  // Node 3: Local Output Pin 4 on Device A (112233445566) -> Action 1
  // Node 4: Condition "== 1"
  // Node 5: Global Output Pin 12 on Device B (AABBCCDDEEFF) -> Action 0
  
  const mockFlow = {
    nodes: [
      { id: 'n1', type: 'device_input', data: { mac: '112233445566', pin: 'GPIO_5' } },
      { id: 'n2', type: 'condition', data: { op: '==', val: '0' } },
      { id: 'n3', type: 'device_output', data: { mac: '112233445566', pin: 'GPIO_4', val: '1' } },
      { id: 'n4', type: 'condition', data: { op: '==', val: '1' } },
      { id: 'n5', type: 'device_output', data: { mac: 'AABBCCDDEEFF', pin: 'GPIO_12', val: '0' } }
    ],
    edges: [
      { source: 'n1', target: 'n2' },
      { source: 'n2', target: 'n3' },
      { source: 'n1', target: 'n4' },
      { source: 'n4', target: 'n5' }
    ]
  };

  // Mock MQTT Client for rule compilation
  const publishedMessages = [];
  const mockMqttClient = {
    publish: (topic, payload, options) => {
      publishedMessages.push({ topic, payload: payload.toString() });
      console.log(`[Mock MQTT] Published to ${topic}: ${payload}`);
    }
  };

  initializeRulesEngine(mockMqttClient);

  console.log('[Test 2] Compiling mock flow diagram...');
  const result = compileFlow(mockFlow);
  console.log(`[Test 2] Compile result local count: ${result.localCount}`);

  // Local rule should be published to MQTT config topic
  const localConfigMsg = publishedMessages.find(m => m.topic === 'cihaz/config/112233445566');
  if (!localConfigMsg) {
    throw new Error('Test 2 Failed: Local rules not published to cihaz/config/112233445566');
  }
  
  const parsedLocalRules = JSON.parse(localConfigMsg.payload);
  console.log('[Test 2] Published local rules details:', parsedLocalRules);
  // Expected local rules: [{"s":5, "t":4, "o":"==", "v":"0", "a":"1"}]
  if (parsedLocalRules[0].s !== 5 || parsedLocalRules[0].t !== 4 || parsedLocalRules[0].o !== '==') {
    throw new Error('Test 2 Failed: Local rules payload content is incorrect');
  }

  // Global rule should be saved in SQLite rules table
  const globalRulesCount = db.prepare('SELECT COUNT(*) as count FROM rules').get().count;
  console.log(`[Test 2] SQLite Global Rules count: ${globalRulesCount} (Expected: 1)`);
  if (globalRulesCount !== 1) {
    throw new Error('Test 2 Failed: Global rule not saved in SQLite');
  }

  const savedRule = db.prepare('SELECT * FROM rules LIMIT 1').get();
  console.log('[Test 2] Saved global rule details:', savedRule);
  if (savedRule.source_mac !== '112233445566' || savedRule.target_mac !== 'AABBCCDDEEFF' || savedRule.action_value !== '0') {
    throw new Error('Test 2 Failed: Saved global rule data is incorrect');
  }
  console.log('[Test 2] Flow compilation and partition PASSED.');

  // Test 3: Global Rule Evaluation
  console.log('\n[Test 3] Simulating telemetry update that triggers global rule...');
  // Device A GPIO_5 reports '1' -> triggers output on Device B GPIO_12 = '0'
  publishedMessages.length = 0; // Clear published buffer
  
  evaluateGlobalRules('112233445566', 'GPIO_5', '1');
  
  console.log(`[Test 3] Messages published after telemetry: ${publishedMessages.length}`);
  const controlMsg = publishedMessages.find(m => m.topic === 'cihaz/kontrol/AABBCCDDEEFF/12');
  if (!controlMsg || controlMsg.payload !== '0') {
    throw new Error('Test 3 Failed: Global rule trigger did not send control command');
  }
  console.log('[Test 3] Global Rule Trigger Evaluation PASSED.');

  console.log('\n[Test] ALL TESTS PASSED SUCCESSFULLY! Rules compiler & engine is flawless.');
  process.exit(0);
}

runTest().catch(err => {
  console.error('[Test] Test suite failed:', err);
  process.exit(1);
});
