<template>
  <div class="space-y-6 pb-12">
    <div>
      <h2 class="text-xl font-extrabold text-white tracking-tight">Cihaz Donanım Yapılandırması</h2>
      <p class="text-xs text-slate-400">Gömülü cihazların pin rollerini dinamik olarak tanımlayın ve LittleFS /pins.json ayarlarını güncelleyin</p>
    </div>

    <div class="grid grid-cols-1 lg:grid-cols-3 gap-6">
      <!-- Devices list panel -->
      <div class="space-y-4">
        <h3 class="text-xs font-bold text-slate-400 uppercase tracking-widest">Cihaz Seçimi</h3>
        
        <div v-if="store.devices.length === 0" class="glass-card p-6 text-center text-xs text-slate-500 rounded-xl">
          Aktif cihaz bulunmuyor.
        </div>
        
        <div 
          v-for="device in store.devices" 
          :key="device.mac"
          @click="selectDevice(device)"
          class="glass-card p-4 rounded-xl cursor-pointer transition-all duration-200 border"
          :class="selectedDevice?.mac === device.mac ? 'border-blue-500 bg-blue-500/5' : 'border-slate-800/80 hover:border-slate-700'"
        >
          <div class="flex items-center justify-between">
            <span class="font-mono text-xs font-bold text-white">{{ formatMac(device.mac) }}</span>
            <span class="text-[10px] px-1.5 py-0.5 rounded bg-slate-800 text-slate-400">
              {{ device.config?.length || 0 }} Pin
            </span>
          </div>
          <div class="flex justify-between items-center mt-2 text-[10px]">
            <span :class="device.status === 'online' ? 'text-emerald-400' : 'text-red-400'">● {{ device.status }}</span>
            <span class="text-slate-500">Board: {{ detectBoard(device.mac) }}</span>
          </div>
        </div>
      </div>

      <!-- Config form panel -->
      <div class="lg:col-span-2 space-y-4">
        <h3 class="text-xs font-bold text-slate-400 uppercase tracking-widest">Pin Haritası Düzenleyici</h3>
        
        <div v-if="!selectedDevice" class="glass-card p-12 text-center text-xs text-slate-500 rounded-2xl">
          <Sliders class="w-8 h-8 mx-auto text-slate-600 mb-2" />
          Lütfen yapılandırmak için soldan bir cihaz seçin.
        </div>

        <div v-else class="glass-card rounded-2xl p-6 space-y-6">
          <!-- Device Info banner -->
          <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-3 bg-slate-900/40 p-4 rounded-xl border border-slate-800/60">
            <div>
              <h4 class="text-xs font-bold text-white">Seçili Cihaz: {{ formatMac(selectedDevice.mac) }}</h4>
              <p class="text-[10px] text-slate-400 mt-0.5">Tespit edilen profil: <strong class="text-slate-300">{{ detectedProfile }}</strong></p>
            </div>
            <div class="flex items-center space-x-2">
              <span class="text-[10px] text-slate-400">Model:</span>
              <select v-model="boardProfile" class="bg-slate-800 text-xs text-white border border-slate-700 rounded px-2 py-1 focus:outline-none">
                <option value="esp32">ESP32 Dual-Core (Max 40 Pins)</option>
                <option value="esp8266">ESP8266 L106 (Max 17 Pins)</option>
                <option value="esp01">ESP-01/S Module (Max 4 Pins)</option>
              </select>
            </div>
          </div>

          <!-- Pins List Editor -->
          <div class="space-y-3">
            <div class="flex justify-between items-center">
              <h5 class="text-[11px] font-bold text-slate-400 uppercase tracking-widest">Aktif Pin Haritaları</h5>
              <button 
                @click="addNewPin" 
                class="px-2.5 py-1 bg-blue-600 hover:bg-blue-500 active:scale-95 transition-all text-[11px] font-bold rounded text-white flex items-center shadow-md shadow-blue-600/10"
              >
                <Plus class="w-3.5 h-3.5 mr-1" /> Pin Ekle
              </button>
            </div>

            <!-- Validation/Safety warnings -->
            <div v-if="safetyWarnings.length > 0" class="space-y-1.5">
              <div 
                v-for="warn in safetyWarnings" 
                :key="warn" 
                class="bg-amber-900/20 border border-amber-500/30 text-amber-300 px-3 py-2 rounded-lg text-[10.5px] flex items-start"
              >
                <AlertTriangle class="w-3.5 h-3.5 mr-2 mt-0.5 flex-shrink-0" />
                <span>{{ warn }}</span>
              </div>
            </div>

            <!-- Configured Pins List -->
            <div v-if="editingPins.length === 0" class="text-center py-6 text-xs text-slate-500 italic bg-slate-900/10 border border-dashed border-slate-800 rounded-xl">
              Yapılandırılmış pin bulunmuyor. "Pin Ekle" butonuna basarak donanım ataması yapın.
            </div>

            <div v-else class="space-y-2.5">
              <div 
                v-for="(pin, index) in editingPins" 
                :key="index"
                class="flex items-center gap-3 bg-slate-950/40 p-3 rounded-xl border border-slate-900/60"
              >
                <!-- Pin Number selection -->
                <div class="flex-grow grid grid-cols-2 gap-3">
                  <div>
                    <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Pin No</label>
                    <select v-model="pin.p" @change="runSafetyChecks" class="w-full bg-slate-800 text-xs text-white border border-slate-700 rounded px-2.5 py-1.5 focus:outline-none">
                      <option v-for="pNo in availablePinsList" :key="pNo" :value="pNo">GPIO_{{ pNo }}</option>
                    </select>
                  </div>
                  <div>
                    <label class="block text-[8px] font-bold text-slate-500 uppercase mb-1">Pin Modu</label>
                    <select v-model="pin.m" @change="runSafetyChecks" class="w-full bg-slate-800 text-xs text-white border border-slate-700 rounded px-2.5 py-1.5 focus:outline-none">
                      <option value="DO">DO (Dijital Çıkış / Röle)</option>
                      <option value="DI">DI (Dijital Giriş / Buton)</option>
                      <option value="AI">AI (Analog Giriş / Sensör)</option>
                      <option value="PWM">PWM (Darbelik Güç Sürücü)</option>
                      <option value="SRV">SRV (Servo Açı Kontrolü)</option>
                      <option value="STM">STM (Step Motor Hız/Adım)</option>
                      <option value="DHT">DHT22 (Isı & Nem Sensörü)</option>
                      <option value="DS">DS18B20 (Tek Kablo Sıcaklık)</option>
                    </select>
                  </div>
                </div>

                <!-- Delete pin -->
                <button 
                  @click="removePin(index)" 
                  class="mt-4 p-1.5 hover:bg-red-500/10 hover:text-red-400 text-slate-500 transition-all rounded-lg"
                >
                  <Trash2 class="w-4 h-4" />
                </button>
              </div>
            </div>
          </div>

          <!-- Action controls -->
          <div class="flex items-center justify-end space-x-3 border-t border-slate-800 pt-4 mt-6">
            <button 
              @click="cancelEdit" 
              class="px-4 py-2 bg-slate-800 hover:bg-slate-700 text-xs font-semibold rounded-lg transition-all text-slate-300"
            >
              İptal
            </button>
            <button 
              @click="saveConfig" 
              :disabled="hasValidationError"
              class="px-4 py-2 bg-blue-600 hover:bg-blue-500 active:scale-95 disabled:opacity-40 disabled:scale-100 transition-all text-xs font-semibold rounded-lg text-white shadow-lg shadow-blue-500/20"
            >
              Ayarla & Cihazı Yenile
            </button>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { store } from '../store'
import { 
  Sliders, 
  Cpu, 
  Plus, 
  Trash2, 
  AlertTriangle 
} from 'lucide-vue-next'

const selectedDevice = ref(null)
const boardProfile = ref('esp32')
const editingPins = ref([])
const safetyWarnings = ref([])

// Detect likely board profiles based on MAC prefix or length
const detectedProfile = computed(() => {
  if (!selectedDevice.value) return 'Unknown'
  if (boardProfile.value === 'esp01') return 'ESP-01/S Module (Safety Locks Enabled)'
  if (boardProfile.value === 'esp8266') return 'ESP8266 NodeMCU/Wemos'
  return 'ESP32 Dual-Core (Watchdog Active)'
})

// Max allowed pins based on profile
const pinLimits = computed(() => {
  if (boardProfile.value === 'esp01') return 4;
  if (boardProfile.value === 'esp8266') return 17;
  return 40;
})

const availablePinsList = computed(() => {
  if (boardProfile.value === 'esp01') {
    return [0, 1, 2, 3]; // TX:1, RX:3, GPIO0:0, GPIO2:2
  }
  if (boardProfile.value === 'esp8266') {
    return [0, 1, 2, 3, 4, 5, 9, 10, 12, 13, 14, 15, 16]; // ESP8266 GPIOs
  }
  // ESP32 supports up to 39 GPIOs
  return Array.from({ length: 40 }, (_, i) => i);
})

const hasValidationError = computed(() => {
  // ESP-01/S cannot exceed 4 active pins
  if (boardProfile.value === 'esp01' && editingPins.value.length > 4) {
    return true;
  }
  // Check duplicates
  const pinsMapped = editingPins.value.map(p => p.p);
  const duplicates = pinsMapped.filter((item, index) => pinsMapped.indexOf(item) !== index);
  return duplicates.length > 0;
})

function selectDevice(device) {
  selectedDevice.value = device
  // Default guess profile based on config size
  if (device.config && device.config.length > 0) {
    // If it maps to RX (3) or TX (1) and size <= 4, guess ESP-01
    const rxTxUsed = device.config.some(p => p.p === 1 || p.p === 3);
    if (device.config.length <= 4 && rxTxUsed) {
      boardProfile.value = 'esp01'
    } else if (device.config.some(p => p.p > 16)) {
      boardProfile.value = 'esp32'
    } else {
      boardProfile.value = 'esp8266'
    }
    // Deep copy current config to editing list
    editingPins.value = JSON.parse(JSON.stringify(device.config))
  } else {
    boardProfile.value = 'esp32'
    editingPins.value = []
  }
  runSafetyChecks()
}

function detectBoard(mac) {
  // Simple layout identifier helper
  if (!mac) return 'ESP32'
  if (mac.startsWith('EC') || mac.startsWith('48')) return 'ESP32'
  return 'ESP8266'
}

function formatMac(mac) {
  if (!mac) return ''
  return mac.replace(/(.{2})/g, '$1:').slice(0, -1)
}

function addNewPin() {
  if (editingPins.value.length >= pinLimits.value) {
    alert(`Bu kart profili için limit olan ${pinLimits.value} pini aştınız!`);
    return;
  }
  // Default to a free GPIO
  const currentMapped = editingPins.value.map(p => p.p);
  const nextFree = availablePinsList.value.find(p => !currentMapped.includes(p)) || 0;
  
  editingPins.value.push({ p: nextFree, m: 'DO' });
  runSafetyChecks();
}

function removePin(index) {
  editingPins.value.splice(index, 1);
  runSafetyChecks();
}

function cancelEdit() {
  if (selectedDevice.value) {
    selectDevice(selectedDevice.value);
  }
}

async function saveConfig() {
  if (hasValidationError.value) return;
  const success = await store.saveDeviceConfig(selectedDevice.value.mac, editingPins.value);
  if (success) {
    alert('Pin konfigürasyonu cihaza gönderildi ve LittleFS kaydı yenilendi.');
  }
}

// Strictly evaluates microcontroller strapping pins and RX/TX safety rules
function runSafetyChecks() {
  safetyWarnings.value = [];
  const pins = editingPins.value;
  
  // Rule 1: Check pin duplicates
  const pinsMapped = pins.map(p => p.p);
  const duplicates = pinsMapped.filter((item, index) => pinsMapped.indexOf(item) !== index);
  if (duplicates.length > 0) {
    safetyWarnings.value.push(`UYARI: ${duplicates.map(p => 'GPIO_' + p).join(', ')} pinleri birden fazla defa atanmış! Çakışmaları düzeltin.`);
  }

  // Rule 2: ESP-01/S Pin count check
  if (boardProfile.value === 'esp01' && pins.length > 4) {
    safetyWarnings.value.push(`HATA: ESP-01/S modülü en fazla 4 GPIO (0, 2, RX/3, TX/1) kontrol edebilir.`);
  }

  // Rule 3: Strapping pins safety check
  // ESP32 strapping pins: GPIO 0, 2, 12, 15
  // ESP8266 strapping pins: GPIO 0, 2, 15
  const esp32Strappings = [0, 2, 12, 15];
  const esp8266Strappings = [0, 2, 15];
  
  const targetStrappings = boardProfile.value === 'esp32' ? esp32Strappings : esp8266Strappings;
  
  for (const pin of pins) {
    if (targetStrappings.includes(pin.p)) {
      // If strapping pin is used as Output
      if (['DO', 'PWM', 'SRV', 'STM'].includes(pin.m)) {
        safetyWarnings.value.push(
          `KRİTİK UYARI: GPIO_${pin.p} başlangıç strapping pinidir. Bu pini Çıkış (${pin.m}) olarak atamak, cihaz boot olurken bootloader kilitlenmesine veya beklenmedik röle tetiklenmelerine sebep olabilir!`
        );
      }
    }

    // Rule 4: RX/TX usage logs shutdown check
    if (boardProfile.value === 'esp01') {
      if (pin.p === 1 || pin.p === 3) {
        safetyWarnings.value.push(
          `NOT: ESP-01 üzerinde RX (GPIO_3) veya TX (GPIO_1) pinleri I/O olarak atandığında, donanımsal Serial port kilitlenmesini engellemek için cihaz debug loglarını otomatik olarak kapatacaktır (Serial.end() tetiklenecek).`
        );
      }
    }
  }
}
</script>
