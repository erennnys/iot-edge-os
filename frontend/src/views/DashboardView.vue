<template>
  <div class="space-y-6 pb-12">
    <!-- Title Section -->
    <div class="flex flex-col md:flex-row md:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-extrabold text-white tracking-tight">Sistem Paneli</h2>
        <p class="text-xs text-slate-400">Tüm gömülü cihazların ve donanım bileşenlerinin canlı izleme ekranı</p>
      </div>
      
      <button 
        @click="refreshDashboard" 
        class="self-start px-3.5 py-1.5 bg-slate-800 hover:bg-slate-700 active:scale-95 transition-all text-xs font-semibold rounded-lg flex items-center border border-slate-700"
      >
        <RefreshCw class="w-3.5 h-3.5 mr-2" :class="{'animate-spin': isRefreshing}" />
        Yenile
      </button>
    </div>

    <!-- Error/Loading states -->
    <div v-if="store.error" class="bg-red-900/20 border border-red-500/30 text-red-300 p-4 rounded-xl text-xs flex items-center">
      <AlertTriangle class="w-4 h-4 mr-2.5 flex-shrink-0" />
      {{ store.error }}
    </div>

    <div v-if="store.devices.length === 0 && !store.isLoading" class="glass-card p-12 text-center rounded-2xl max-w-lg mx-auto space-y-4">
      <div class="w-16 h-16 bg-slate-900/80 rounded-full flex items-center justify-center mx-auto text-slate-500 border border-slate-800">
        <Cpu class="w-8 h-8" />
      </div>
      <div class="space-y-1">
        <h3 class="text-sm font-semibold text-white">Kayıtlı Cihaz Bulunamadı</h3>
        <p class="text-xs text-slate-400">Yerel ağdaki cihazınızdan kayıt mesajı veya telemetry gönderildiğinde cihazınız burada otomatik olarak listelenecektir.</p>
      </div>
    </div>

    <!-- Devices Grid -->
    <div v-else class="grid grid-cols-1 lg:grid-cols-2 gap-6">
      <div 
        v-for="device in store.devices" 
        :key="device.mac" 
        class="glass-card rounded-2xl p-5 flex flex-col justify-between transition-all duration-300 hover:border-slate-700"
        :class="device.status === 'online' ? 'neon-border-blue' : 'opacity-60 border-red-900/30'"
      >
        <!-- Device Info Header -->
        <div class="flex items-start justify-between border-b border-slate-800 pb-4 mb-4">
          <div class="space-y-1">
            <div class="flex items-center space-x-2">
              <span class="font-mono text-sm font-bold text-white tracking-wider">{{ formatMac(device.mac) }}</span>
              <span 
                class="px-2 py-0.5 text-[9px] font-bold rounded-full uppercase tracking-wider"
                :class="device.status === 'online' ? 'bg-emerald-500/10 text-emerald-400' : 'bg-red-500/10 text-red-400'"
              >
                {{ device.status }}
              </span>
            </div>
            <p class="text-[10px] text-slate-400">Son Görülme: {{ formatTime(device.last_seen) }}</p>
          </div>
          <Cpu class="w-5 h-5 text-slate-400" />
        </div>

        <!-- Peripherals Controls / Display -->
        <div class="flex-grow space-y-4">
          <h4 class="text-[11px] font-semibold text-slate-400 uppercase tracking-widest">Bileşenler & Pinler</h4>
          
          <div v-if="!device.config || device.config.length === 0" class="text-center py-4">
            <p class="text-xs text-slate-500 italic">Bu cihaza atanmış donanım pini bulunmamaktadır.</p>
            <router-link to="/config" class="text-xs text-blue-400 hover:underline mt-1 inline-block">Pinleri Yapılandır</router-link>
          </div>

          <div v-else class="grid grid-cols-1 sm:grid-cols-2 gap-3">
            <div 
              v-for="pin in device.config" 
              :key="pin.p" 
              class="bg-slate-900/40 border border-slate-800/80 rounded-xl p-3 flex flex-col justify-between space-y-2.5"
            >
              <!-- Pin Header -->
              <div class="flex items-center justify-between">
                <span class="text-xs font-semibold text-white">Pin {{ pin.p }}</span>
                <span class="px-1.5 py-0.5 bg-slate-800 rounded text-[9px] font-mono text-slate-400">{{ pin.m }}</span>
              </div>

              <!-- Pin Control/Telemetry rendering based on mode -->
              <div class="flex-grow flex items-center justify-between">
                <!-- 1. Digital Output (DO) -->
                <div v-if="pin.m === 'DO'" class="w-full flex items-center justify-between">
                  <span class="text-[11px] text-slate-400">Röle/Çıkış</span>
                  <button 
                    @click="toggleDigital(device.mac, pin.p, device.state[`GPIO_${pin.p}`])"
                    class="relative inline-flex h-5 w-10 items-center rounded-full transition-all focus:outline-none"
                    :class="device.state[`GPIO_${pin.p}`] === '1' ? 'bg-blue-600' : 'bg-slate-800'"
                  >
                    <span 
                      class="inline-block h-3.5 w-3.5 transform rounded-full bg-white transition-all"
                      :class="device.state[`GPIO_${pin.p}`] === '1' ? 'translate-x-5.5' : 'translate-x-1'"
                    />
                  </button>
                </div>

                <!-- 2. PWM Duty Cycle -->
                <div v-else-if="pin.m === 'PWM'" class="w-full space-y-1">
                  <div class="flex justify-between text-[11px]">
                    <span class="text-slate-400">PWM Gücü</span>
                    <span class="text-white font-mono">{{ device.state[`PWM_${pin.p}`] || 0 }}%</span>
                  </div>
                  <input 
                    type="range" 
                    min="0" 
                    max="255" 
                    :value="device.state[`PWM_${pin.p}`] || 0" 
                    @change="e => updatePwm(device.mac, pin.p, e.target.value)"
                    class="w-full h-1 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-blue-500"
                  />
                </div>

                <!-- 3. Servo Motor (SRV) -->
                <div v-else-if="pin.m === 'SRV'" class="w-full space-y-1">
                  <div class="flex justify-between text-[11px]">
                    <span class="text-slate-400">Servo Açısı</span>
                    <span class="text-white font-mono">{{ device.state[`SRV_${pin.p}`] || 90 }}°</span>
                  </div>
                  <input 
                    type="range" 
                    min="0" 
                    max="180" 
                    :value="device.state[`SRV_${pin.p}`] || 90" 
                    @change="e => updateServo(device.mac, pin.p, e.target.value)"
                    class="w-full h-1 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-purple-500"
                  />
                </div>

                <!-- 4. Step Motor (STM) -->
                <div v-else-if="pin.m === 'STM'" class="w-full space-y-1.5">
                  <div class="flex justify-between text-[11px]">
                    <span class="text-slate-400">Adım Sürüşü</span>
                  </div>
                  <div class="flex gap-1.5">
                    <button @click="sendStepper(device.mac, pin.p, -100)" class="flex-grow py-1 bg-slate-800 hover:bg-slate-700 active:scale-95 text-[10px] rounded border border-slate-700 font-bold">-100</button>
                    <button @click="sendStepper(device.mac, pin.p, 100)" class="flex-grow py-1 bg-slate-800 hover:bg-slate-700 active:scale-95 text-[10px] rounded border border-slate-700 font-bold">+100</button>
                  </div>
                </div>

                <!-- 5. Digital Input (DI) -->
                <div v-else-if="pin.m === 'DI'" class="w-full flex items-center justify-between">
                  <span class="text-[11px] text-slate-400">Dijital Giriş</span>
                  <div class="flex items-center space-x-1.5">
                    <div class="w-2 h-2 rounded-full" :class="device.state[`GPIO_${pin.p}`] === '1' ? 'bg-emerald-500 shadow-md shadow-emerald-500/50' : 'bg-slate-700'"></div>
                    <span class="text-xs font-mono font-bold" :class="device.state[`GPIO_${pin.p}`] === '1' ? 'text-emerald-400' : 'text-slate-400'">
                      {{ device.state[`GPIO_${pin.p}`] === '1' ? 'AKTİF' : 'PASİF' }}
                    </span>
                  </div>
                </div>

                <!-- 6. Analog Input (AI) -->
                <div v-else-if="pin.m === 'AI'" class="w-full space-y-1">
                  <div class="flex justify-between text-[11px]">
                    <span class="text-slate-400">Analog Giriş</span>
                    <span class="text-white font-mono">{{ device.state[`ADC_${pin.p}`] || 0 }}</span>
                  </div>
                  <div class="w-full bg-slate-800 rounded-full h-1.5 overflow-hidden">
                    <div class="bg-blue-500 h-1.5 rounded-full" :style="{ width: ((device.state[`ADC_${pin.p}`] || 0) / 40.95) + '%' }"></div>
                  </div>
                </div>

                <!-- 7. DHT Sensor -->
                <div v-else-if="pin.m === 'DHT'" class="w-full flex items-center justify-around gap-1.5 text-center">
                  <div>
                    <span class="block text-[8px] text-slate-500 font-semibold uppercase">Sıcaklık</span>
                    <span class="text-xs font-bold text-emerald-400 font-mono">{{ device.state[`DHT_T_${pin.p}`] || '--' }}°C</span>
                  </div>
                  <div class="w-px h-6 bg-slate-800"></div>
                  <div>
                    <span class="block text-[8px] text-slate-500 font-semibold uppercase">Nem</span>
                    <span class="text-xs font-bold text-blue-400 font-mono">{{ device.state[`DHT_H_${pin.p}`] || '--' }}%</span>
                  </div>
                </div>

                <!-- 8. OneWire DS18B20 Temp Sensor -->
                <div v-else-if="pin.m === 'DS'" class="w-full flex items-center justify-between">
                  <span class="text-[11px] text-slate-400">DS18B20 Sıcaklık</span>
                  <span class="text-xs font-bold text-amber-400 font-mono">{{ device.state[`DS_T_${pin.p}`] || '--' }}°C</span>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { store } from '../store'
import { 
  RefreshCw, 
  AlertTriangle, 
  Cpu, 
  Sliders, 
  ToggleLeft 
} from 'lucide-vue-next'

const isRefreshing = ref(false)
let pollingTimer = null

onMounted(async () => {
  await store.fetchDevices()
  
  // Dynamic refresh polling every 3 seconds
  pollingTimer = setInterval(async () => {
    await store.fetchDevices()
  }, 3000)
})

onUnmounted(() => {
  if (pollingTimer) clearInterval(pollingTimer)
})

async function refreshDashboard() {
  isRefreshing.value = true
  await store.fetchDevices()
  setTimeout(() => isRefreshing.value = false, 500)
}

function formatMac(mac) {
  if (!mac) return ''
  return mac.replace(/(.{2})/g, '$1:').slice(0, -1)
}

function formatTime(timestamp) {
  if (!timestamp) return 'Bilinmiyor'
  const date = new Date(timestamp)
  return date.toLocaleTimeString()
}

// Controller events
function toggleDigital(mac, pin, currentValue) {
  const newValue = currentValue === '1' ? '0' : '1'
  store.controlPin(mac, pin, newValue)
  // Optimistically set status
  const dev = store.devices.find(d => d.mac === mac)
  if (dev) {
    dev.state[`GPIO_${pin}`] = newValue
  }
}

function updatePwm(mac, pin, value) {
  store.controlPin(mac, pin, value)
  const dev = store.devices.find(d => d.mac === mac)
  if (dev) {
    dev.state[`PWM_${pin}`] = value
  }
}

function updateServo(mac, pin, value) {
  store.controlPin(mac, pin, value)
  const dev = store.devices.find(d => d.mac === mac)
  if (dev) {
    dev.state[`SRV_${pin}`] = value
  }
}

function sendStepper(mac, pin, steps) {
  store.controlPin(mac, pin, steps)
}
</script>
