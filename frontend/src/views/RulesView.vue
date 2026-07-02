<template>
  <div class="h-[calc(100vh-140px)] flex flex-col space-y-4">
    <!-- Header Controls -->
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-3 flex-shrink-0">
      <div>
        <h2 class="text-xl font-extrabold text-white tracking-tight">Görsel Otomasyon Tasarımcısı</h2>
        <p class="text-xs text-slate-400">Mantıksal akış şeması çizerek cihazlar arası kuralları derleyin ve anında dağıtın</p>
      </div>

      <div class="flex gap-2">
        <!-- Add Node Dropdown -->
        <select 
          v-model="selectedNodeType" 
          class="bg-slate-800 text-xs text-white border border-slate-700 rounded-lg px-3 py-1.5 focus:outline-none"
        >
          <option value="device_input">Düğüm: Giriş Cihazı (Pin)</option>
          <option value="time_trigger">Düğüm: Zamanlayıcı (Saat/Gün)</option>
          <option value="condition">Düğüm: Mantıksal Karşılaştırma</option>
          <option value="custom_js">Düğüm: Sandbox Javascript Kodu</option>
          <option value="device_output">Düğüm: Çıkış Eylemi (Pin)</option>
        </select>
        
        <button 
          @click="addNode" 
          class="px-3 py-1.5 bg-slate-800 hover:bg-slate-700 active:scale-95 transition-all text-xs font-semibold border border-slate-700 rounded-lg text-white"
        >
          Düğüm Ekle
        </button>

        <button 
          @click="compileAndDeploy" 
          class="px-4 py-1.5 bg-blue-600 hover:bg-blue-500 active:scale-95 transition-all text-xs font-bold rounded-lg text-white shadow-lg shadow-blue-500/20"
        >
          Derle & Dağıt
        </button>
      </div>
    </div>

    <!-- Main Canvas Layout -->
    <div class="flex-grow flex gap-4 overflow-hidden min-h-0">
      <!-- Vue Flow Canvas -->
      <div class="flex-grow relative bg-[#090b11] border border-slate-800/80 rounded-2xl overflow-hidden shadow-2xl">
        <VueFlow 
          v-model:nodes="nodes" 
          v-model:edges="edges" 
          :default-edge-options="{ type: 'smoothstep', animated: true }"
          @edge-connect="onConnect"
          fit-view-on-init
          class="w-full h-full"
        >
          <!-- Grid background -->
          <Background pattern-color="#1e293b" :gap="16" :size="1.5" />
          
          <Controls position="bottom-left" class="bg-slate-900 border border-slate-800 text-white rounded-lg" />

          <!-- Custom Node: Device Input -->
          <template #node-device_input="{ id, data }">
            <div class="glass-card neon-border-blue w-60 rounded-xl overflow-hidden text-xs text-slate-200">
              <div class="bg-blue-600/10 px-3 py-2 border-b border-blue-500/20 flex items-center justify-between">
                <span class="font-bold text-blue-400">Giriş Cihazı</span>
                <button @click="deleteNode(id)" class="text-slate-500 hover:text-red-400"><Trash2 class="w-3.5 h-3.5" /></button>
              </div>
              <div class="p-3 space-y-2">
                <div>
                  <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Cihaz MAC</label>
                  <select v-model="data.mac" class="w-full bg-slate-900 border border-slate-800 rounded p-1.5 focus:outline-none">
                    <option v-for="d in store.devices" :key="d.mac" :value="d.mac">{{ formatMac(d.mac) }}</option>
                  </select>
                </div>
                <div>
                  <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Giriş Pini</label>
                  <select v-model="data.pin" class="w-full bg-slate-900 border border-slate-800 rounded p-1.5 focus:outline-none">
                    <option v-for="p in getPinsForDevice(data.mac)" :key="p" :value="`GPIO_${p}`">GPIO_{{ p }}</option>
                  </select>
                </div>
              </div>
              <Handle type="source" position="right" />
            </div>
          </template>

          <!-- Custom Node: Time Trigger -->
          <template #node-time_trigger="{ id, data }">
            <div class="glass-card neon-border-purple w-60 rounded-xl overflow-hidden text-xs text-slate-200">
              <div class="bg-purple-600/10 px-3 py-2 border-b border-purple-500/20 flex items-center justify-between">
                <span class="font-bold text-purple-400">Zamanlama Tetikleyici</span>
                <button @click="deleteNode(id)" class="text-slate-500 hover:text-red-400"><Trash2 class="w-3.5 h-3.5" /></button>
              </div>
              <div class="p-3 space-y-2">
                <div>
                  <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Tetik Zamanı</label>
                  <input 
                    type="text" 
                    v-model="data.time" 
                    placeholder="Örn: 08:30, sunrise, sunset" 
                    class="w-full bg-slate-900 border border-slate-800 rounded p-1.5 text-white font-mono placeholder-slate-600 focus:outline-none"
                  />
                </div>
              </div>
              <Handle type="source" position="right" />
            </div>
          </template>

          <!-- Custom Node: Condition -->
          <template #node-condition="{ id, data }">
            <div class="glass-card border-slate-700 w-52 rounded-xl overflow-hidden text-xs text-slate-200">
              <div class="bg-slate-800/40 px-3 py-2 border-b border-slate-700/60 flex items-center justify-between">
                <span class="font-bold text-slate-300">Mantıksal Koşul</span>
                <button @click="deleteNode(id)" class="text-slate-500 hover:text-red-400"><Trash2 class="w-3.5 h-3.5" /></button>
              </div>
              <div class="p-3 grid grid-cols-2 gap-2">
                <Handle type="target" position="left" />
                <div>
                  <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Karşılaştır</label>
                  <select v-model="data.op" class="w-full bg-slate-900 border border-slate-800 rounded p-1.5 focus:outline-none">
                    <option value="==">== (Eşit)</option>
                    <option value="!=">!= (Eşit Değil)</option>
                    <option value=">">> (Büyük)</option>
                    <option value="<">&lt; (Küçük)</option>
                  </select>
                </div>
                <div>
                  <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Değer</label>
                  <input 
                    type="text" 
                    v-model="data.val" 
                    class="w-full bg-slate-900 border border-slate-800 rounded p-1.5 text-white focus:outline-none"
                  />
                </div>
                <Handle type="source" position="right" />
              </div>
            </div>
          </template>

          <!-- Custom Node: Custom JS Code -->
          <template #node-custom_js="{ id, data }">
            <div class="glass-card border-purple-500/30 w-72 rounded-xl overflow-hidden text-xs text-slate-200 shadow-2xl">
              <div class="bg-purple-900/10 px-3 py-2 border-b border-purple-800/30 flex items-center justify-between">
                <span class="font-bold text-purple-400 flex items-center">
                  <Code class="w-3.5 h-3.5 mr-1" /> Sandbox Script
                </span>
                <button @click="deleteNode(id)" class="text-slate-500 hover:text-red-400"><Trash2 class="w-3.5 h-3.5" /></button>
              </div>
              <div class="p-3 space-y-2">
                <Handle type="target" position="left" />
                <div>
                  <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Javascript Kodu (100ms CPU Sınırı)</label>
                  <textarea 
                    v-model="data.code" 
                    rows="4" 
                    placeholder="// input, mac, pin değişkenleri hazırdır.&#10;if (input > 24) {&#10;  output = 1;&#10;} else {&#10;  output = 0;&#10;}" 
                    class="w-full bg-slate-900 border border-slate-800 rounded p-2 text-[10.5px] font-mono text-emerald-400 placeholder-slate-700 focus:outline-none resize-y"
                  ></textarea>
                </div>
                <Handle type="source" position="right" />
              </div>
            </div>
          </template>

          <!-- Custom Node: Device Output -->
          <template #node-device_output="{ id, data }">
            <div class="glass-card neon-border-green w-60 rounded-xl overflow-hidden text-xs text-slate-200">
              <div class="bg-emerald-600/10 px-3 py-2 border-b border-emerald-500/20 flex items-center justify-between">
                <span class="font-bold text-emerald-400">Çıkış Eylemi</span>
                <button @click="deleteNode(id)" class="text-slate-500 hover:text-red-400"><Trash2 class="w-3.5 h-3.5" /></button>
              </div>
              <div class="p-3 space-y-2">
                <Handle type="target" position="left" />
                <div>
                  <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Hedef Cihaz MAC</label>
                  <select v-model="data.mac" class="w-full bg-slate-900 border border-slate-800 rounded p-1.5 focus:outline-none">
                    <option v-for="d in store.devices" :key="d.mac" :value="d.mac">{{ formatMac(d.mac) }}</option>
                  </select>
                </div>
                <div>
                  <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Hedef Çıkış Pini</label>
                  <select v-model="data.pin" class="w-full bg-slate-900 border border-slate-800 rounded p-1.5 focus:outline-none">
                    <option v-for="p in getPinsForDevice(data.mac)" :key="p" :value="`GPIO_${p}`">GPIO_{{ p }}</option>
                  </select>
                </div>
                <div>
                  <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Eylem Değeri</label>
                  <input 
                    type="text" 
                    v-model="data.val" 
                    placeholder="Örn: 1, 0, 180" 
                    class="w-full bg-slate-900 border border-slate-800 rounded p-1.5 text-white font-mono focus:outline-none"
                  />
                </div>
              </div>
            </div>
          </template>
        </VueFlow>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { store } from '../store'
import { VueFlow, useVueFlow, Handle } from '@vue-flow/core'
import { Background } from '@vue-flow/background'
import { Controls } from '@vue-flow/controls'
import { Trash2, Code } from 'lucide-vue-next'

import '@vue-flow/core/dist/style.css'
import '@vue-flow/core/dist/theme-default.css'

const selectedNodeType = ref('device_input')
const { addEdges, removeNodes } = useVueFlow()

const nodes = ref([])
const edges = ref([])

onMounted(async () => {
  await store.fetchDevices()
  await store.fetchFlow()
  
  if (store.flow && store.flow.nodes && store.flow.nodes.length > 0) {
    nodes.value = store.flow.nodes
    edges.value = store.flow.edges
  } else {
    // Add default template input and output node to guide the user
    nodes.value = [
      {
        id: 'input_1',
        type: 'device_input',
        position: { x: 50, y: 150 },
        data: { mac: store.devices[0]?.mac || '', pin: '' }
      },
      {
        id: 'output_1',
        type: 'device_output',
        position: { x: 450, y: 150 },
        data: { mac: store.devices[0]?.mac || '', pin: '', val: '1' }
      }
    ]
  }
})

function getPinsForDevice(mac) {
  const dev = store.devices.find(d => d.mac === mac)
  if (!dev || !dev.config) return []
  // return only pin numbers
  return dev.config.map(p => p.p)
}

function formatMac(mac) {
  if (!mac) return ''
  return mac.replace(/(.{2})/g, '$1:').slice(0, -1)
}

let nodeCounter = 2
function addNode() {
  const type = selectedNodeType.value
  const id = `node_${type}_${nodeCounter++}`
  
  // Default configurations
  const data = { mac: store.devices[0]?.mac || '', pin: '' }
  if (type === 'time_trigger') {
    data.time = '08:00'
  } else if (type === 'condition') {
    data.op = '=='
    data.val = '1'
  } else if (type === 'custom_js') {
    data.code = `// input: gelen tetik degeri\nif (input == 1) {\n  output = 1;\n} else {\n  output = 0;\n}`
  } else if (type === 'device_output') {
    data.val = '1'
  }

  nodes.value.push({
    id,
    type,
    position: { x: 200, y: 200 },
    data
  })
}

function deleteNode(id) {
  removeNodes([id])
}

function onConnect(connection) {
  const edge = {
    id: `e-${connection.source}-${connection.target}`,
    source: connection.source,
    target: connection.target,
    sourceHandle: connection.sourceHandle,
    targetHandle: connection.targetHandle,
    animated: true,
    type: 'smoothstep'
  }
  edges.value.push(edge)
}

async function compileAndDeploy() {
  const flowData = {
    nodes: nodes.value.map(n => ({
      id: n.id,
      type: n.type,
      position: n.position,
      data: n.data
    })),
    edges: edges.value.map(e => ({
      id: e.id,
      source: e.source,
      target: e.target
    }))
  }

  const result = await store.saveFlow(flowData)
  if (result.success) {
    alert(`Otomasyon akış şeması başarıyla derlendi ve kartlara yüklendi!\nDağıtılan lokal kural sayısı: ${result.localCount}`);
  } else {
    alert(`Derleme Hatası: ${result.error}`);
  }
}
</script>
