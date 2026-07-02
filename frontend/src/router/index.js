import { createRouter, createWebHistory } from 'vue-router'
import DashboardView from '../views/DashboardView.vue'
import ConfigView from '../views/ConfigView.vue'
import RulesView from '../views/RulesView.vue'
import TelemetryView from '../views/TelemetryView.vue'

const routes = [
  {
    path: '/',
    name: 'Dashboard',
    component: DashboardView
  },
  {
    path: '/config',
    name: 'PinConfig',
    component: ConfigView
  },
  {
    path: '/rules',
    name: 'RulesDesigner',
    component: RulesView
  },
  {
    path: '/telemetry',
    name: 'TelemetryLogs',
    component: TelemetryView
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router
