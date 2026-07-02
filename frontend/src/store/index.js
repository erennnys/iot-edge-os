import { reactive } from 'vue'

export const store = reactive({
  devices: [],
  telemetry: [],
  flow: { nodes: [], edges: [] },
  isLoading: false,
  error: null,

  async fetchDevices() {
    this.isLoading = true;
    try {
      const res = await fetch('/api/devices');
      if (!res.ok) throw new Error('Failed to fetch devices');
      this.devices = await res.json();
    } catch (err) {
      this.error = err.message;
      console.error(err);
    } finally {
      this.isLoading = false;
    }
  },

  async fetchTelemetry() {
    try {
      const res = await fetch('/api/telemetry?limit=50');
      if (!res.ok) throw new Error('Failed to fetch telemetry');
      this.telemetry = await res.json();
    } catch (err) {
      console.error(err);
    }
  },

  async saveDeviceConfig(mac, pinConfig) {
    this.isLoading = true;
    try {
      const res = await fetch(`/api/config/${mac}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(pinConfig)
      });
      if (!res.ok) throw new Error('Failed to save device pin configuration');
      await this.fetchDevices();
      return true;
    } catch (err) {
      this.error = err.message;
      console.error(err);
      return false;
    } finally {
      this.isLoading = false;
    }
  },

  async controlPin(mac, pin, value) {
    try {
      const res = await fetch(`/api/control/${mac}/${pin}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ value })
      });
      if (!res.ok) throw new Error('Failed to send control command');
      return true;
    } catch (err) {
      console.error(err);
      return false;
    }
  },

  async saveFlow(flowData) {
    this.isLoading = true;
    try {
      const res = await fetch('/api/flow', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(flowData)
      });
      if (!res.ok) throw new Error('Failed to compile rules flow');
      const data = await res.json();
      return data;
    } catch (err) {
      this.error = err.message;
      console.error(err);
      return { success: false, error: err.message };
    } finally {
      this.isLoading = false;
    }
  },

  async fetchFlow() {
    try {
      const res = await fetch('/api/flow');
      if (!res.ok) throw new Error('Failed to fetch flow configuration');
      this.flow = await res.json();
    } catch (err) {
      console.error(err);
    }
  }
});
