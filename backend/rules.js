const vm = require('vm');
const { db, statements } = require('./db');
const { getSolarTimes } = require('./astronomy');

let activeMqttClient = null;

function initializeRulesEngine(mqttClient) {
  activeMqttClient = mqttClient;
  
  // Minute-by-minute scheduler for time and astronomical rules
  setInterval(() => {
    const now = new Date();
    const currentHourMin = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}`;
    const solar = getSolarTimes(now);
    
    try {
      const scheduledRules = statements.getScheduledRules.all();
      
      for (const rule of scheduledRules) {
        let triggerTime = rule.source_pin;
        
        if (triggerTime === 'sunrise') {
          triggerTime = solar.sunrise;
        } else if (triggerTime === 'sunset') {
          triggerTime = solar.sunset;
        }
        
        if (triggerTime === currentHourMin) {
          console.log(`[Rules] Scheduled Trigger! Time matches ${currentHourMin}. Sending action to device ${rule.target_mac}...`);
          sendControlAction(rule.target_mac, rule.target_pin, rule.action_value);
        }
      }
    } catch (err) {
      console.error('[Rules] Error in scheduled rules evaluation:', err);
    }
  }, 60000); // Check every 60s
  
  console.log('[Rules] Global rules engine scheduler initialized.');
}

function sendControlAction(mac, pin, value) {
  if (!activeMqttClient) return;
  const cleanedMac = mac.toUpperCase().replace(/[^A-Z0-9]/g, '');
  const cleanedPin = pin.replace('GPIO_', '').replace('ADC_', '');
  const topic = `cihaz/kontrol/${cleanedMac}/${cleanedPin}`;
  console.log(`[Rules] Sending control command -> Topic: ${topic}, Payload: ${value}`);
  activeMqttClient.publish(topic, String(value), { qos: 1, retain: false });
}

function evaluateGlobalRules(mac, pin, value) {
  try {
    const rules = statements.getRulesForTrigger.all(mac, pin);
    for (const rule of rules) {
      let isTriggered = false;
      
      if (rule.operator === '==') {
        isTriggered = (String(value) === String(rule.value));
      } else if (rule.operator === '!=') {
        isTriggered = (String(value) !== String(rule.value));
      } else if (rule.operator === '>') {
        isTriggered = (Number(value) > Number(rule.value));
      } else if (rule.operator === '<') {
        isTriggered = (Number(value) < Number(rule.value));
      } else if (rule.operator === 'none') {
        isTriggered = true;
      }
      
      if (isTriggered) {
        console.log(`[Rules] Triggered Global Rule: If ${mac}/${pin} ${rule.operator} ${rule.value} (Actual: ${value}) -> target: ${rule.target_mac}/${rule.target_pin} = ${rule.action_value}`);
        
        // Execute JS Sandbox Node if target starts with 'js_'
        if (rule.target_pin.startsWith('js_')) {
          runSandboxJsNode(rule.target_pin, rule.action_value, value, mac, pin);
        } else {
          sendControlAction(rule.target_mac, rule.target_pin, rule.action_value);
        }
      }
    }
  } catch (err) {
    console.error('[Rules] Error evaluating global rules:', err);
  }
}

function runSandboxJsNode(nodeId, scriptCode, inputValue, srcMac, srcPin) {
  const sandbox = {
    input: inputValue,
    mac: srcMac,
    pin: srcPin,
    output: null,
    console: {
      log: (...args) => console.log(`[JS Sandbox ${nodeId}]`, ...args)
    }
  };

  try {
    const context = vm.createContext(sandbox);
    const script = new vm.Script(scriptCode, { timeout: 100 }); // strict 100ms CPU timeout
    script.runInContext(context);
    
    // Evaluate downstream if output was populated
    if (sandbox.output !== null && sandbox.output !== undefined) {
      console.log(`[Rules] Sandbox output:`, sandbox.output);
      evaluateGlobalRules('system_js', nodeId, sandbox.output);
    }
  } catch (err) {
    console.error(`[Rules] Sandbox error on JS Node ${nodeId}:`, err);
  }
}

// Vue-Flow compiler
function compileFlow(flowData) {
  console.log('[Compiler] Compiling new Vue-Flow configuration...');
  
  const { nodes, edges } = flowData;
  if (!nodes || !edges) {
    throw new Error('Invalid flow data structure: nodes and edges are required');
  }

  // Node Map
  const nodeMap = {};
  for (const node of nodes) {
    nodeMap[node.id] = node;
  }

  // Clear previous rules in DB
  statements.clearRules.run();

  const localRulesByMac = {};

  const triggerNodes = nodes.filter(n => n.type === 'device_input' || n.type === 'time_trigger');

  for (const trigger of triggerNodes) {
    traverseDownstream(trigger, [], trigger, nodeMap, edges, localRulesByMac);
  }

  // Push local rules to corresponding devices
  for (const [mac, rules] of Object.entries(localRulesByMac)) {
    const compressedRules = rules.map(r => ({
      s: parseInt(r.source_pin.replace('GPIO_', '').replace('ADC_', '')),
      t: parseInt(r.target_pin.replace('GPIO_', '').replace('ADC_', '')),
      o: r.operator,
      v: r.value,
      a: r.action_value
    }));

    if (activeMqttClient) {
      const topic = `cihaz/config/${mac}`;
      const payload = JSON.stringify(compressedRules);
      console.log(`[Compiler] Pushing LOCAL rules to Device ${mac} -> Topic: ${topic}`);
      activeMqttClient.publish(topic, payload, { qos: 1, retain: true });
    }
  }

  return { success: true, localCount: Object.values(localRulesByMac).flat().length };
}

function traverseDownstream(node, pathConditions, startTrigger, nodeMap, edges, localRulesByMac) {
  const outgoingEdges = edges.filter(e => e.source === node.id);

  for (const edge of outgoingEdges) {
    const target = nodeMap[edge.target];
    if (!target) continue;

    if (target.type === 'condition') {
      const newConditions = [...pathConditions, {
        operator: target.data.op || '==',
        value: target.data.val || '0'
      }];
      traverseDownstream(target, newConditions, startTrigger, nodeMap, edges, localRulesByMac);
    }
    else if (target.type === 'custom_js') {
      const jsCode = target.data.code || '';
      saveGlobalRule('flow_compiler', startTrigger, pathConditions, 'system_js', target.id, jsCode);
      traverseDownstream(target, [], target, nodeMap, edges, localRulesByMac);
    }
    else if (target.type === 'device_output') {
      const targetMac = target.data.mac;
      const targetPin = target.data.pin;
      const actionValue = target.data.val || '1';

      const condition = pathConditions[0] || { operator: 'none', value: '' };

      if (startTrigger.type === 'device_input' && startTrigger.data.mac === targetMac) {
        // LOCAL Rule: Triggers and outputs are on same hardware
        const sourceMac = startTrigger.data.mac;
        const sourcePin = startTrigger.data.pin;

        if (!localRulesByMac[sourceMac]) {
          localRulesByMac[sourceMac] = [];
        }
        localRulesByMac[sourceMac].push({
          source_pin: sourcePin,
          operator: condition.operator,
          value: condition.value,
          target_pin: targetPin,
          action_value: actionValue
        });
      } else {
        // GLOBAL Rule
        saveGlobalRule('flow_compiler', startTrigger, pathConditions, targetMac, targetPin, actionValue);
      }
    }
  }
}

function saveGlobalRule(flowId, startTrigger, pathConditions, targetMac, targetPin, actionValue) {
  const condition = pathConditions[0] || { operator: 'none', value: '' };
  
  let sourceMac = 'system';
  let sourcePin = '';

  if (startTrigger.type === 'device_input') {
    sourceMac = startTrigger.data.mac;
    sourcePin = startTrigger.data.pin;
  } else if (startTrigger.type === 'time_trigger') {
    sourcePin = startTrigger.data.time;
  } else if (startTrigger.type === 'custom_js') {
    sourceMac = 'system_js';
    sourcePin = startTrigger.id;
  }

  statements.insertRule.run(
    flowId,
    sourceMac,
    sourcePin,
    condition.operator,
    condition.value,
    targetMac,
    targetPin,
    actionValue
  );
  
  console.log(`[Compiler] Saved GLOBAL rule: If ${sourceMac}/${sourcePin} ${condition.operator} ${condition.value} -> target: ${targetMac}/${targetPin} = ${actionValue.substring(0, 20)}`);
}

module.exports = {
  initializeRulesEngine,
  evaluateGlobalRules,
  compileFlow
};
