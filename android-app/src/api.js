/**
 * Harmony Hub Client API
 * Direct network communication with codex_webui HTTP API.
 * Supports HTTP Basic Authentication and CORS.
 */

export class HarmonyClient {
  constructor(hub) {
    this.updateHub(hub);
  }

  updateHub(hub) {
    this.hub = hub || {};
    const host = this.hub.host || '127.0.0.1';
    const port = this.hub.port || 8080;
    this.wsPort = this.hub.wsPort || 8089;
    this.baseUrl = `http://${host}:${port}`;
    this.authHeader = (this.hub.username && this.hub.password)
      ? `Basic ${btoa(`${this.hub.username}:${this.hub.password}`)}`
      : null;
    this.onWsMessage = null;
    this.onWsStatus = null;
    this.connectWebSocket();
  }

  connectWebSocket() {
    this.disconnectWebSocket();
    const host = this.hub.host;
    if (!host) return;

    const wsUrl = `ws://${host}:${this.wsPort}/`;
    try {
      this.ws = new WebSocket(wsUrl);
      this.ws.onopen = () => {
        if (this.onWsStatus) this.onWsStatus(true);
      };
      this.ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data && this.onWsMessage) {
            this.onWsMessage(data);
          }
        } catch {}
      };
      this.ws.onerror = () => {
        // Triggers onclose
      };
      this.ws.onclose = () => {
        if (this.onWsStatus) this.onWsStatus(false);
        this.scheduleWsReconnect();
      };
    } catch {
      this.scheduleWsReconnect();
    }
  }

  scheduleWsReconnect() {
    if (this.wsReconnectTimer) clearTimeout(this.wsReconnectTimer);
    this.wsReconnectTimer = setTimeout(() => {
      if (this.hub && this.hub.host) {
        this.connectWebSocket();
      }
    }, 3000);
  }

  disconnectWebSocket() {
    if (this.wsReconnectTimer) {
      clearTimeout(this.wsReconnectTimer);
      this.wsReconnectTimer = null;
    }
    if (this.ws) {
      try {
        this.ws.onclose = null;
        this.ws.onerror = null;
        this.ws.close();
      } catch {}
      this.ws = null;
    }
  }

  getHeaders(extra = {}) {
    const headers = { ...extra };
    if (this.authHeader) {
      headers['Authorization'] = this.authHeader;
    }
    return headers;
  }

  async request(path, options = {}) {
    const url = `${this.baseUrl}${path}`;
    const opts = {
      ...options,
      headers: this.getHeaders(options.headers || {})
    };

    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), options.timeout || 8000);
    opts.signal = controller.signal;

    try {
      const res = await fetch(url, opts);
      clearTimeout(timeout);
      if (res.status === 401) {
        throw new Error('Authentication failed. Check hub credentials.');
      }
      if (!res.ok) {
        let errMsg = `Hub returned HTTP ${res.status}`;
        try {
          const text = await res.text();
          const j = JSON.parse(text);
          if (j.error) errMsg = j.error;
          else if (j.reply) errMsg = j.reply;
        } catch {}
        if (res.status === 409 && errMsg.startsWith('Hub returned')) {
          errMsg = 'Activity switch already in progress';
        }
        throw new Error(errMsg);
      }
      const text = await res.text();
      try {
        return JSON.parse(text);
      } catch {
        return { ok: true, text };
      }
    } catch (err) {
      clearTimeout(timeout);
      if (err.name === 'AbortError') {
        throw new Error('Connection timed out. Check IP and network.');
      }
      throw err;
    }
  }

  async testConnection() {
    try {
      const data = await this.request('/api/inventory', { timeout: 4000 });
      return { ok: true, data };
    } catch (err) {
      return { ok: false, error: err.message };
    }
  }

  async getInventory() {
    return this.request('/api/inventory');
  }

  async getActivities(options = {}) {
    return this.request('/api/activities', options);
  }

  isWsConnected() {
    return Boolean(this.ws && this.ws.readyState === WebSocket.OPEN);
  }

  sendWs(data) {
    if (!this.isWsConnected()) return false;
    try {
      this.ws.send(JSON.stringify(data));
      return true;
    } catch {
      return false;
    }
  }

  async getDeviceCommands(deviceId) {
    if (!deviceId) return [];
    try {
      const res = await this.request(`/api/device-commands?deviceId=${encodeURIComponent(deviceId)}`);
      return Array.isArray(res) ? res : (res.commands || []);
    } catch (err) {
      console.warn(`Failed to fetch commands for device ${deviceId}:`, err);
      return [];
    }
  }

  async startActivity(activityId) {
    if (this.sendWs({ action: 'start_activity', id: String(activityId) })) {
      return { ok: true, ws: true };
    }
    return this.request('/api/activity-start', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: String(activityId) })
    });
  }

  async stopActivity() {
    if (this.sendWs({ action: 'stop_activity' })) {
      return { ok: true, ws: true };
    }
    return this.request('/api/activity-stop', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({})
    });
  }

  async sendCommand(deviceId, command) {
    if (this.sendWs({ action: 'send_command', deviceId: String(deviceId), command: String(command) })) {
      return { ok: true, ws: true };
    }
    return this.request('/api/ir-send', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        deviceId: String(deviceId),
        command: String(command)
      })
    });
  }

  async sendBtCommand(deviceId, command) {
    return this.request('/api/bt-saved-command', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        deviceId: String(deviceId),
        command: String(command)
      })
    });
  }

  async sendBtText(text) {
    if (this.sendWs({ action: 'bt_text', text: String(text) })) {
      return { ok: true, ws: true };
    }
    return this.request('/api/bt-text', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ text: String(text) })
    });
  }

  /* Advanced / Hub Management APIs */
  async getNetworkStatus() {
    return this.request('/api/network-status');
  }

  async getWifiScan() {
    return this.request('/api/wifi-scan');
  }

  async saveWifi(ssid, psk) {
    return this.request('/wifi', {
      method: 'POST',
      body: new URLSearchParams({ ssid: ssid || '', psk: psk || '' })
    });
  }

  async getBtStatus() {
    return this.request('/api/bt-status');
  }

  async startBtScan() {
    return this.request('/api/remote-scan', { method: 'POST' });
  }

  async pairBt(bdaddr, transport = 32) {
    return this.request('/api/remote-pair', {
      method: 'POST',
      body: new URLSearchParams({ bdaddr: bdaddr || '', transport: String(transport) })
    });
  }

  async getSystemDisk() {
    return this.request('/api/disk-space');
  }

  async getUpdateStatus() {
    return this.request('/api/update-status');
  }

  async checkUpdate(force = false) {
    return this.request('/api/update-check-state', {
      method: 'POST',
      body: new URLSearchParams({ check: force ? '1' : '0' })
    });
  }

  async rebootHub() {
    return this.request('/system/reboot', {
      method: 'POST',
      body: new URLSearchParams({ action: 'reboot' })
    });
  }

  async captureIr() {
    return this.request('/api/capture', { method: 'POST' });
  }

  async testLearnedIr(data) {
    return this.request('/api/ir-test-learned', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data || {})
    });
  }

  async saveLearnedCommand(data) {
    return this.request('/ir/command', {
      method: 'POST',
      body: new URLSearchParams(data || {})
    });
  }
}

