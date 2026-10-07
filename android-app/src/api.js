/**
 * Harmony Hub Client API
 * Direct network communication with codex_daemon HTTP & WebSocket API.
 * Supports HTTP Basic Authentication, CORS, and AJAX JSON messaging.
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
      this.ws.onerror = () => {};
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
    const timeout = setTimeout(() => controller.abort(), options.timeout || 12000);
    opts.signal = controller.signal;

    try {
      const res = await fetch(url, opts);
      clearTimeout(timeout);
      if (res.status === 401) {
        throw new Error('Authentication failed. Check hub credentials.');
      }
      const text = await res.text();
      let j;
      try {
        j = JSON.parse(text);
      } catch {
        j = null;
      }

      if (!res.ok) {
        let errMsg = `Hub returned HTTP ${res.status}`;
        if (j && (j.error || j.message || j.reply)) {
          errMsg = j.error || j.message || j.reply;
        } else if (text && text.length < 200 && !text.includes('<!DOCTYPE')) {
          errMsg = text;
        }
        if (res.status === 409 && errMsg.startsWith('Hub returned')) {
          errMsg = 'Activity switch already in progress';
        }
        throw new Error(errMsg);
      }

      return j !== null ? j : text;
    } catch (err) {
      clearTimeout(timeout);
      if (err.name === 'AbortError') {
        throw new Error('Connection timed out. Check IP and network.');
      }
      throw err;
    }
  }

  async postForm(path, data = {}) {
    const params = new URLSearchParams();
    for (const [k, v] of Object.entries(data)) {
      if (v !== undefined && v !== null) {
        params.append(k, String(v));
      }
    }
    return this.request(path, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
        'Accept': 'application/json, text/plain, */*',
        'X-Requested-With': 'XMLHttpRequest'
      },
      body: params.toString()
    });
  }

  async postJson(path, data = {}) {
    return this.request(path, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json, text/plain, */*',
        'X-Requested-With': 'XMLHttpRequest'
      },
      body: JSON.stringify(data)
    });
  }

  async testConnection() {
    try {
      const data = await this.request('/api/inventory', { timeout: 4000 });
      return { ok: true, data };
    } catch (err) {
      return { ok: false, error: err.message };
    }
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

  /* Core Inventory & Activities */
  async getInventory() {
    return this.request('/api/inventory');
  }

  async getActivities(options = {}) {
    return this.request('/api/activities', options);
  }

  async startActivity(activityId) {
    if (this.sendWs({ action: 'start_activity', id: String(activityId) })) {
      return { ok: true, ws: true };
    }
    return this.postJson('/api/activity-start', { id: String(activityId) });
  }

  async stopActivity() {
    if (this.sendWs({ action: 'stop_activity' })) {
      return { ok: true, ws: true };
    }
    return this.postJson('/api/activity-stop', {});
  }

  async saveActivity(data) {
    return this.postJson('/api/activity-save', data);
  }

  async deleteActivity(id) {
    return this.postJson('/api/activity-delete', { id: String(id) });
  }

  async testActivitySequence(steps) {
    return this.postJson('/api/activity-test-sequence', { steps: String(steps) });
  }

  /* Device Control & Commands */
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

  async sendCommand(deviceId, command) {
    if (this.sendWs({ action: 'send_command', deviceId: String(deviceId), command: String(command) })) {
      return { ok: true, ws: true };
    }
    return this.postJson('/api/ir-send', {
      deviceId: String(deviceId),
      command: String(command)
    });
  }

  async sendBtCommand(deviceId, command) {
    return this.postJson('/api/bt-saved-command', {
      deviceId: String(deviceId),
      command: String(command)
    });
  }

  async sendBtKey(code, action = 'tap', target = '') {
    if (!code) return { ok: false };
    if (this.sendWs({ action: 'bt_key', key: String(code), keyAction: String(action), target: String(target) })) {
      return { ok: true, ws: true };
    }
    return this.postJson('/api/bt-key', {
      key: String(code),
      code: String(code),
      action: String(action),
      target: String(target)
    });
  }

  async sendBtText(text, target = '') {
    if (this.sendWs({ action: 'bt_text', text: String(text), target: String(target) })) {
      return { ok: true, ws: true };
    }
    return this.postJson('/api/bt-text', { text: String(text), target: String(target) });
  }

  async runBtScript(script, delay = 35) {
    return this.postJson('/api/bt-script', { script: String(script), delay: Number(delay) || 35 });
  }

  async saveBtCommand(deviceId, name, script) {
    return this.postForm('/bt/command', { deviceId, name, script });
  }

  async deleteBtCommand(deviceId, command) {
    return this.postForm('/bt/delete-command', { deviceId, command });
  }

  async getBtStatus() {
    return this.request('/api/bt-status');
  }

  async setBtPairing(enable, name = '', targetDev = '') {
    return this.postJson('/api/bt-pairing', {
      enable: enable ? 1 : 0,
      name: name || 'Harmony Keyboard',
      targetDev: targetDev || ''
    });
  }

  async connectBt(bdaddr) {
    return this.postJson('/api/bt-connect', { bdaddr });
  }

  async disconnectBt(bdaddr = '') {
    return this.postJson('/api/bt-disconnect', { bdaddr: bdaddr || '' });
  }

  async linkBtDevice(bdaddr, deviceId) {
    return this.postJson('/api/bt-link-device', { bdaddr, deviceId });
  }

  /* Homatics B25 / Physical BT Remote Mapping */
  async getRemoteMappings() {
    return this.request('/api/remote-mapping');
  }

  async saveRemoteMappings(data) {
    return this.postJson('/api/remote-mapping-save', data);
  }

  async getUiRemoteLayout() {
    return this.request('/api/ui-remote-layout');
  }

  async saveUiRemoteLayout(layouts) {
    return this.postJson('/api/ui-remote-layout', layouts);
  }

  /* Harmony Elite RF Remote & Mapping */
  async getEliteMapping() {
    return this.request('/api/elite-mapping');
  }

  async saveEliteMapping(data) {
    return this.postJson('/api/elite-mapping-save', data);
  }

  async getRfStatus() {
    return this.request('/api/rf/status');
  }

  async pairRfRemote() {
    return this.request('/api/rf/pair', { method: 'POST' });
  }

  async stopPairRfRemote() {
    return this.request('/api/rf/stop-pair', { method: 'POST' });
  }

  async unpairRfRemote() {
    return this.request('/api/rf/unpair', { method: 'POST' });
  }

  async scanBtRemote() {
    return this.request('/api/remote-scan', { method: 'POST' });
  }

  async pairBtRemote(bdaddr, transport = 32) {
    return this.postForm('/api/remote-pair', { bdaddr: bdaddr || '', transport: String(transport) });
  }

  async getBtRemotePairStatus() {
    return this.request('/api/remote-pair-status');
  }

  /* IR Setup & Learning */
  async captureIr() {
    return this.request('/api/capture', { method: 'POST' });
  }

  async testLearnedIr(data) {
    return this.postJson('/api/ir-test-learned', data);
  }

  async saveLearnedCommand(data) {
    return this.postForm('/ir/command', data);
  }

  async saveNewDevice(data) {
    return this.postForm('/ir/new-device', data);
  }

  async saveDeviceDetails(data) {
    return this.postForm('/ir/device', data);
  }

  async deleteDevice(deviceId) {
    return this.postForm('/ir/delete-device', { deviceId });
  }

  async saveDevicePower(data) {
    return this.postForm('/ir/device-power', data);
  }

  async saveDeviceMqtt(data) {
    return this.postForm('/ir/device-mqtt', data);
  }

  async testDeviceMqtt(deviceId) {
    return this.postJson('/api/device-mqtt-test', { deviceId });
  }

  async deleteCommand(deviceId, command) {
    return this.postForm('/ir/delete-command', { deviceId, command });
  }

  async importIrdb(data) {
    return this.postForm('/ir/irdb-import', data);
  }

  /* Bulk IR Lab */
  async sendBatchIr(data) {
    return this.postJson('/api/ir-batch-send', data);
  }

  async cancelBatchIr(runId) {
    return this.postJson('/api/ir-cancel', { runId });
  }

  async clearLabTarget(deviceId) {
    return this.postJson('/api/ir-lab-clear', { deviceId });
  }

  /* MQTT */
  async getMqttStatus() {
    return this.request('/api/mqtt-status');
  }

  async saveMqtt(data) {
    return this.postForm('/mqtt', data);
  }

  /* Network (Wi-Fi & Ethernet) */
  async getNetworkStatus() {
    return this.request('/api/network-status');
  }

  async saveEthernet(data) {
    return this.postForm('/ethernet', data);
  }

  async saveWifi(data) {
    return this.postForm('/wifi', data);
  }

  /* Backup & Restore */
  getExportUrl(target) {
    const host = this.hub.host || '127.0.0.1';
    const port = this.hub.port || 8080;
    return `http://${host}:${port}/export/${target}`;
  }

  async fetchExportText(target) {
    return this.request(`/export/${target}`);
  }

  async restoreBackup(target, payload) {
    return this.postForm('/import', { target, payload });
  }

  /* System & Maintenance */
  async getSystemDisk() {
    return this.request('/api/disk-space');
  }

  async getWebUiHtml() {
    return this.request('/');
  }

  async saveSystemAction(action, extra = {}) {
    return this.postForm('/system', { action, ...extra });
  }

  async rebootHub() {
    return this.saveSystemAction('reboot');
  }

  async getUpdateStatus() {
    return this.request('/api/update-status');
  }

  async checkUpdate(force = false) {
    return this.postForm('/api/update-check-state', { check: force ? '1' : '0' });
  }

  async applyUpdate(token = '', repo = '') {
    return this.postForm('/api/update-apply', { token: token || '', repo: repo || '' });
  }
}
