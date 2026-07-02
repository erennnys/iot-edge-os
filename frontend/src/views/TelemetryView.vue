<template>
  <div class="space-y-6 pb-12">
    <!-- Title Section -->
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-extrabold text-white tracking-tight">Telemetri Veri Günlüğü</h2>
        <p class="text-xs text-slate-400">Tüm cihazlardan gelen ve SQLite veritabanına kaydedilen ham telemetri kayıtları</p>
      </div>

      <button 
        @click="loadLogs" 
        class="px-3.5 py-1.5 bg-slate-800 hover:bg-slate-700 active:scale-95 transition-all text-xs font-semibold rounded-lg flex items-center border border-slate-700 self-start"
      >
        <RefreshCw class="w-3.5 h-3.5 mr-2" :class="{'animate-spin': isRefreshing}" />
        Günlüğü Yenile
      </button>
    </div>

    <!-- Filters Bar -->
    <div class="glass-card p-4 rounded-xl flex flex-wrap gap-4 items-center">
      <div class="flex items-center space-x-2">
        <Filter class="w-4 h-4 text-slate-500" />
        <span class="text-xs font-bold text-slate-400 uppercase tracking-wider">Filtrele:</span>
      </div>

      <!-- Filter by Device -->
      <select 
        v-model="filterMac" 
        class="bg-slate-900 border border-slate-800 text-xs text-white rounded-lg px-3 py-1.5 focus:outline-none"
      >
        <option value="">Tüm Cihazlar</option>
        <option v-for="d in store.devices" :key="d.mac" :value="d.mac">{{ formatMac(d.mac) }}</option>
      </select>

      <!-- Filter by Pin -->
      <input 
        type="text" 
        v-model="filterPin" 
        placeholder="Pin adı (örn: GPIO_5, ADC_0)" 
        class="bg-slate-900 border border-slate-800 text-xs text-white rounded-lg px-3 py-1.5 focus:outline-none placeholder-slate-600"
      />
    </div>

    <!-- Telemetry Logs Table -->
    <div class="glass-card rounded-2xl overflow-hidden shadow-xl border border-slate-800/80">
      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="bg-slate-900/80 border-b border-slate-800 text-slate-400 uppercase tracking-widest text-[9.5px]">
              <th class="p-4 font-semibold">Tarih / Saat</th>
              <th class="p-4 font-semibold">Cihaz MAC</th>
              <th class="p-4 font-semibold">Bileşen / Pin</th>
              <th class="p-4 font-semibold">Gelen Veri</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-850">
            <tr v-if="filteredLogs.length === 0" class="text-center text-slate-500 italic">
              <td colspan="4" class="p-8">Eşleşen telemetri kaydı bulunmuyor.</td>
            </tr>
            <tr 
              v-else 
              v-for="log in filteredLogs" 
              :key="log.id"
              class="hover:bg-slate-800/20 transition-all"
            >
              <!-- Date Time -->
              <td class="p-4 text-slate-300 font-mono">{{ formatDateTime(log.timestamp) }}</td>
              
              <!-- MAC -->
              <td class="p-4 font-mono font-semibold text-slate-400">{{ formatMac(log.mac) }}</td>
              
              <!-- Pin -->
              <td class="p-4">
                <span class="px-2 py-0.5 bg-slate-850 border border-slate-800 text-[10.5px] rounded-md font-mono text-slate-300">
                  {{ log.pin }}
                </span>
              </td>
              
              <!-- Value -->
              <td class="p-4">
                <span class="font-bold text-white font-mono bg-blue-900/10 px-2 py-0.5 rounded border border-blue-500/10">
                  {{ log.value }}
                </span>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { store } from '../store'
import { RefreshCw, Filter } from 'lucide-vue-next'

const isRefreshing = ref(false)
const filterMac = ref('')
const filterPin = ref('')

onMounted(async () => {
  await store.fetchDevices()
  await loadLogs()
})

async function loadLogs() {
  isRefreshing.value = true
  await store.fetchTelemetry()
  setTimeout(() => isRefreshing.value = false, 500)
}

const filteredLogs = computed(() => {
  return store.telemetry.filter(log => {
    const matchMac = !filterMac.value || log.mac.toUpperCase() === filterMac.value.toUpperCase();
    const matchPin = !filterPin.value || log.pin.toLowerCase().includes(filterPin.value.toLowerCase());
    return matchMac && matchPin;
  });
})

function formatMac(mac) {
  if (!mac) return ''
  return mac.replace(/(.{2})/g, '$1:').slice(0, -1)
}

function formatDateTime(timestamp) {
  if (!timestamp) return ''
  const date = new Date(timestamp)
  const pad = (n) => String(n).padStart(2, '0');
  
  const day = pad(date.getDate());
  const month = pad(date.getMonth() + 1);
  const year = date.getFullYear();
  
  const hours = pad(date.getHours());
  const minutes = pad(date.getMinutes());
  const seconds = pad(date.getSeconds());
  
  return `${hours}:${minutes}:${seconds} - ${day}/${month}/${year}`;
}
</script>

<style>
.divide-slate-850 > tr {
  border-bottom: 1px solid rgba(30, 41, 59, 0.5);
}
</style>
