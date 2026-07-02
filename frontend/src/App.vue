<template>
  <div class="min-h-screen bg-[#0b0f19] text-[#e2e8f0] flex flex-col">
    <!-- Header -->
    <header class="glass sticky top-0 z-50 px-4 py-3 flex items-center justify-between shadow-lg">
      <div class="flex items-center space-x-3">
        <div class="w-8 h-8 rounded-lg bg-blue-600 flex items-center justify-center font-bold text-white shadow-md shadow-blue-500/20">
          Ω
        </div>
        <div>
          <h1 class="text-md font-bold tracking-tight text-white">IoT Edge OS</h1>
          <p class="text-[10px] text-slate-400">Kurşun Geçirmez Yerel Ağ Sistem Mimarı</p>
        </div>
      </div>
      
      <!-- Nav Links -->
      <nav class="hidden md:flex space-x-1">
        <router-link to="/" class="nav-btn" active-class="nav-btn-active">
          <LayoutDashboard class="w-4 h-4 mr-1.5" /> Dashboard
        </router-link>
        <router-link to="/config" class="nav-btn" active-class="nav-btn-active">
          <Settings class="w-4 h-4 mr-1.5" /> Cihaz Ayarları
        </router-link>
        <router-link to="/rules" class="nav-btn" active-class="nav-btn-active">
          <GitMerge class="w-4 h-4 mr-1.5" /> Otomasyon Kuralları
        </router-link>
        <router-link to="/telemetry" class="nav-btn" active-class="nav-btn-active">
          <Database class="w-4 h-4 mr-1.5" /> Telemetri Günlükleri
        </router-link>
      </nav>

      <!-- App Status Indicator -->
      <div class="flex items-center space-x-2 bg-slate-900/60 px-3 py-1.5 rounded-full border border-slate-800">
        <div class="w-2.5 h-2.5 rounded-full bg-emerald-500 animate-pulse"></div>
        <span class="text-[11px] font-medium text-slate-300">Edge Local</span>
      </div>
    </header>

    <!-- Main Content -->
    <main class="flex-grow max-w-7xl w-full mx-auto p-4">
      <router-view v-slot="{ Component }">
        <transition name="fade" mode="out-in">
          <component :is="Component" />
        </transition>
      </router-view>
    </main>

    <!-- Bottom Navigation for Mobile -->
    <nav class="md:hidden glass border-t border-slate-800 fixed bottom-0 left-0 right-0 py-2.5 px-4 flex justify-around shadow-2xl z-50">
      <router-link to="/" class="mobile-nav-btn" active-class="mobile-nav-btn-active">
        <LayoutDashboard class="w-5 h-5" />
        <span>Dash</span>
      </router-link>
      <router-link to="/config" class="mobile-nav-btn" active-class="mobile-nav-btn-active">
        <Settings class="w-5 h-5" />
        <span>Ayarlar</span>
      </router-link>
      <router-link to="/rules" class="mobile-nav-btn" active-class="mobile-nav-btn-active">
        <GitMerge class="w-5 h-5" />
        <span>Kurallar</span>
      </router-link>
      <router-link to="/telemetry" class="mobile-nav-btn" active-class="mobile-nav-btn-active">
        <Database class="w-5 h-5" />
        <span>Logs</span>
      </router-link>
    </nav>
    <div class="h-16 md:hidden"></div> <!-- Spacer for mobile bottom nav -->
  </div>
</template>

<script setup>
import { 
  LayoutDashboard, 
  Settings, 
  GitMerge, 
  Database 
} from 'lucide-vue-next'
</script>

<style>
@reference "tailwindcss";

.nav-btn {
  @apply px-3 py-2 rounded-lg text-slate-300 hover:text-white hover:bg-slate-800/50 transition-all text-xs font-medium flex items-center;
}
.nav-btn-active {
  @apply bg-blue-600/10 border border-blue-500/20 text-blue-400 font-semibold;
}
.mobile-nav-btn {
  @apply flex flex-col items-center justify-center text-slate-400 hover:text-white transition-all text-[10px] space-y-1;
}
.mobile-nav-btn-active {
  @apply text-blue-400 font-bold;
}

.fade-enter-active,
.fade-leave-active {
  transition: opacity 0.15s ease;
}
.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}
</style>
