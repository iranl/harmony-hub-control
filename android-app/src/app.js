import { HarmonyClient } from './api.js';
import { getSavedHubs, saveHub, deleteHub, getActiveHub, setActiveHubId } from './storage.js';
import { RemoteEngine } from './remote-engine.js';

class HarmonyApp {
  constructor() {
    this.client = new HarmonyClient(null);
    this.remoteEngine = new RemoteEngine(this.client);
    this.activeHub = null;
    this.inventory = { devices: [] };
    this.activitiesData = { activities: [], currentId: '-1', currentName: 'PowerOff' };
    this.deviceCommandsMap = {};
    this.currentTab = 'activity-remote';
    this.selectedDeviceId = null;
    this.pollTimer = null;
    this.transPollTimer = null;
    this.wsProgress = null;
    this.isHubOnline = false;
    this.setupWebSocket();
  }

  setHubStatus(online) {
    this.isHubOnline = Boolean(online);
    const dot = document.getElementById('activeHubStatusDot');
    const hubBtn = document.getElementById('hubSelectorBtn');
    if (dot) {
      dot.className = online ? 'hub-status-dot online' : 'hub-status-dot offline';
      dot.title = online ? 'Hub is online' : 'Hub is offline';
    }
    if (hubBtn) {
      const name = this.activeHub ? this.activeHub.name : 'Hub';
      hubBtn.title = `${name}: ${online ? 'Online' : 'Offline'} (Tap to manage hubs)`;
    }
  }

  setupWebSocket() {
    this.client.onWsMessage = (data) => {
      if (!data) return;
      if (data.type === 'activity_progress') {
        this.wsProgress = data;
        const isRunningOrIdle = (data.state === 2 || data.state === 0);
        if (isRunningOrIdle) {
          setTimeout(() => {
            this.wsProgress = null;
            this.refreshActivityState();
          }, 500);
        } else {
          this.updateActivityBanner();
        }
      } else if (data.type === 'activity_state') {
        const changed = this.activitiesData.currentId !== data.activity;
        this.activitiesData.currentId = data.activity;
        this.activitiesData.isTransitioning = (data.state === 1 || data.state === 3);
        const curAct = (this.activitiesData.activities || []).find(a => String(a.id) === String(data.activity));
        this.activitiesData.currentName = curAct ? (curAct.name || curAct.label) : (data.activity === '-1' ? 'PowerOff' : data.activity);
        this.updateActivityBanner();
        if (changed && this.currentTab === 'activity-remote') {
          this.renderActivityRemote();
        }
      }
    };
    this.client.onWsStatus = (online) => {
      if (online) {
        this.setHubStatus(true);
      } else {
        // WS closed; immediately verify reachability via HTTP polling
        this.refreshActivityState();
      }
    };
  }

  async init() {
    this.bindEvents();
    await this.loadActiveHub();
  }

  bindEvents() {
    // Nav Tabs
    document.querySelectorAll('.nav-item').forEach(btn => {
      btn.addEventListener('click', () => {
        const tab = btn.getAttribute('data-tab');
        this.switchTab(tab);
      });
    });

    // Hub Switcher Button
    const hubBtn = document.getElementById('hubSelectorBtn');
    if (hubBtn) {
      hubBtn.addEventListener('click', () => this.openHubModal());
    }

    // Power Off in Banner
    const pwrBtn = document.getElementById('btnBannerPowerOff');
    if (pwrBtn) {
      pwrBtn.addEventListener('click', async () => {
        await this.remoteEngine.triggerHaptic();
        if (confirm('Power off all devices in current activity?')) {
          this.activitiesData.isTransitioning = true;
          this.activitiesData.transitionTarget = '-1';
          this.activitiesData.transitionStep = 1;
          this.activitiesData.transitionTotal = 3;
          this.activitiesData.transitionDesc = 'Powering off...';
          this.updateActivityBanner();

          try {
            await this.client.stopActivity();
          } catch (err) {
            console.warn('Power off error:', err);
          }
          await this.refreshActivityState();
        }
      });
    }

    // Add Hub Button in Hubs View
    const addHubBtn = document.getElementById('btnAddHub');
    if (addHubBtn) {
      addHubBtn.addEventListener('click', () => this.openAddHubModal());
    }

    // Device Remote Selector Dropdown
    const devSelect = document.getElementById('deviceRemoteSelect');
    if (devSelect) {
      devSelect.addEventListener('change', () => {
        this.selectedDeviceId = devSelect.value;
        this.renderDeviceRemote();
      });
    }
  }

  async loadActiveHub() {
    const hub = await getActiveHub();
    if (!hub) {
      // Create default localhost hub if none exists
      const defaultHub = {
        id: 'hub_' + Date.now(),
        name: 'My Harmony Hub',
        host: window.location.hostname || '192.168.1.100',
        port: 8080,
        username: '',
        password: '',
        isDefault: true
      };
      await saveHub(defaultHub);
      await setActiveHubId(defaultHub.id);
      this.activeHub = defaultHub;
    } else {
      this.activeHub = hub;
    }

    this.client.updateHub(this.activeHub);
    this.updateHubUI();
    await this.refreshAllData();
    this.startPolling();
  }

  updateHubUI() {
    const label = document.getElementById('activeHubLabel');
    if (label) label.textContent = this.activeHub ? this.activeHub.name : 'Select Hub';
    this.setHubStatus(false);
  }

  async refreshAllData() {
    if (!this.activeHub) return;
    try {
      const [inv, act] = await Promise.all([
        this.client.getInventory(),
        this.client.getActivities({ timeout: 4000 })
      ]);

      this.inventory = inv || { devices: [] };
      this.activitiesData = act || { activities: [], currentId: '-1', currentName: 'PowerOff' };

      // Pre-fetch commands for all devices
      const devices = this.inventory.devices || [];
      await Promise.all(devices.map(async d => {
        if (!this.deviceCommandsMap[d.id]) {
          const cmds = await this.client.getDeviceCommands(d.id);
          this.deviceCommandsMap[d.id] = cmds;
        }
      }));

      this.setHubStatus(true);
      this.updateActivityBanner();
      this.renderCurrentView();
    } catch (err) {
      console.warn('Failed to load hub data:', err);
      this.setHubStatus(false);
    }
  }

  startPolling() {
    if (this.pollTimer) clearInterval(this.pollTimer);
    this.pollTimer = setInterval(() => this.refreshActivityState(), 5000);
  }

  async refreshActivityState() {
    if (!this.activeHub) return;
    try {
      const act = await this.client.getActivities({ timeout: 4000 });
      if (act && act.currentId !== undefined) {
        const wasOffline = !this.isHubOnline;
        this.setHubStatus(true);

        // If hub just recovered connectivity or inventory was empty, recover full data
        if (wasOffline || !this.inventory.devices || this.inventory.devices.length === 0) {
          await this.recoverHubInventory();
        }

        const wasTransitioning = Boolean(this.activitiesData.isTransitioning);
        const changed = this.activitiesData.currentId !== act.currentId;
        this.activitiesData = act;
        this.updateActivityBanner();
        if ((changed || (wasTransitioning && !act.isTransitioning)) && this.currentTab === 'activity-remote') {
          this.renderActivityRemote();
        }
        if (act.isTransitioning) {
          if (this.transPollTimer) clearTimeout(this.transPollTimer);
          this.transPollTimer = setTimeout(() => this.refreshActivityState(), 800);
        }
      } else {
        this.setHubStatus(false);
      }
    } catch {
      this.setHubStatus(false);
    }
  }

  async recoverHubInventory() {
    try {
      const inv = await this.client.getInventory();
      if (inv && Array.isArray(inv.devices) && inv.devices.length > 0) {
        this.inventory = inv;
        await Promise.all(inv.devices.map(async d => {
          if (!this.deviceCommandsMap[d.id] || this.deviceCommandsMap[d.id].length === 0) {
            const cmds = await this.client.getDeviceCommands(d.id);
            this.deviceCommandsMap[d.id] = cmds;
          }
        }));
        this.updateActivityBanner();
        this.renderCurrentView();
      }
    } catch (err) {
      console.warn('Failed to recover inventory:', err);
    }
  }

  updateActivityBanner() {
    const listEl = document.getElementById('bannerActivitiesList');
    const pwrBtn = document.getElementById('btnBannerPowerOff');
    const transCard = document.getElementById('bannerTransitionCard');
    const transTitle = document.getElementById('bannerTransitionName');
    const transDesc = document.getElementById('bannerTransitionDesc');
    const transProg = document.getElementById('bannerTransitionProgress');

    const acts = (this.activitiesData.activities || []).filter(a => String(a.id) !== '-1' && (a.name || '').toLowerCase() !== 'poweroff');
    const currentActId = String(this.activitiesData.currentId);
    const isTransHttp = Boolean(this.activitiesData.isTransitioning);
    const isTransWs = Boolean(this.wsProgress && (this.wsProgress.state === 1 || this.wsProgress.state === 3));
    const isTransitioning = isTransHttp || isTransWs;
    const transTarget = String(this.wsProgress?.activity || this.activitiesData.transitionTarget || '');
    const isOff = currentActId === '-1' || (this.activitiesData.currentName || '').toLowerCase() === 'poweroff';

    if (isTransitioning) {
      // 1. Hide activity buttons and poweroff during transition
      if (listEl) listEl.style.display = 'none';
      if (pwrBtn) pwrBtn.style.display = 'none';

      // 2. Show sleek transition card
      if (transCard) {
        transCard.style.display = 'flex';

        // Target Name
        let targetName = 'Activity';
        if (transTarget === '-1' || (!transTarget && this.wsProgress?.state === 3)) {
          targetName = 'Powering Off...';
        } else {
          const match = (this.activitiesData.activities || []).find(a => String(a.id) === transTarget);
          targetName = match ? `Switching to ${match.name}...` : (transTarget ? `Switching to ${transTarget}...` : 'Switching Activity...');
        }
        if (transTitle) transTitle.textContent = targetName;

        // Progress & Step Description
        let step = this.wsProgress?.step ?? this.activitiesData.transitionStep ?? 0;
        let total = this.wsProgress?.total ?? this.activitiesData.transitionTotal ?? 0;
        let desc = this.wsProgress?.desc || this.activitiesData.transitionDesc || 'Switching devices...';

        let percent = 20;
        if (total > 0) {
          percent = Math.min(95, Math.max(15, Math.round((step / total) * 100)));
          desc = `${desc} (${step}/${total})`;
        }
        if (transDesc) transDesc.textContent = desc;
        if (transProg) transProg.style.width = `${percent}%`;
      }
      return;
    }

    // Not transitioning: hide transition card, restore activity buttons & poweroff
    if (transCard) transCard.style.display = 'none';
    if (pwrBtn) {
      pwrBtn.style.display = 'inline-flex';
      pwrBtn.disabled = false;
      pwrBtn.style.cursor = 'pointer';
      pwrBtn.style.opacity = isOff ? '0.6' : '1';
      pwrBtn.title = isOff ? 'System is currently Powered Off' : 'Power Off All Devices';
    }

    if (listEl) {
      listEl.style.display = 'flex';
      if (acts.length === 0) {
        listEl.innerHTML = '<span class="activity-empty-msg">No activities</span>';
      } else {
        listEl.innerHTML = acts.map(a => {
          const actId = String(a.id);
          const isRunning = actId === currentActId;
          const btnClass = isRunning ? 'btn-accent active' : 'btn-secondary';
          const prefix = isRunning ? '● ' : '';
          return `
            <button type="button" class="btn btn-xs ${btnClass} banner-activity-btn" data-act-id="${a.id}">
              ${prefix}${a.name || a.label}
            </button>
          `;
        }).join('');

        listEl.querySelectorAll('.banner-activity-btn').forEach(btn => {
          btn.addEventListener('click', async () => {
            const id = btn.getAttribute('data-act-id');
            await this.remoteEngine.triggerHaptic();

            // Do not rerun starting sequence if already running
            if (String(id) === String(this.activitiesData.currentId)) {
              if (this.currentTab !== 'activity-remote') {
                this.switchTab('activity-remote');
              }
              return;
            }

            // Guard activity switch behind confirmation
            const targetAct = (this.activitiesData.activities || []).find(a => String(a.id) === String(id));
            const targetName = targetAct ? (targetAct.name || targetAct.label) : 'selected activity';
            if (!confirm(`Switch activity to "${targetName}"?`)) {
              return;
            }

            try {
              // Immediately set local optimistic state to show transition bar immediately
              this.activitiesData.isTransitioning = true;
              this.activitiesData.transitionTarget = id;
              this.activitiesData.transitionStep = 1;
              this.activitiesData.transitionTotal = 4;
              this.activitiesData.transitionDesc = 'Requesting switch...';
              this.updateActivityBanner();

              await this.client.startActivity(id);
            } catch (err) {
              console.warn('Activity switch error:', err);
            }
            await this.refreshActivityState();
            if (this.currentTab !== 'activity-remote') {
              this.switchTab('activity-remote');
            }
          });
        });
      }
    }
  }

  switchTab(tabId) {
    this.currentTab = tabId;
    document.querySelectorAll('.nav-item').forEach(b => {
      b.classList.toggle('active', b.getAttribute('data-tab') === tabId);
    });
    document.querySelectorAll('.view-panel').forEach(p => {
      p.classList.toggle('active', p.id === `view-${tabId}`);
    });
    this.renderCurrentView();
  }

  renderCurrentView() {
    switch (this.currentTab) {
      case 'activity-remote':
        this.renderActivityRemote();
        break;
      case 'device-remote':
        this.renderDeviceRemote();
        break;
      case 'hubs':
        this.renderHubsList();
        break;
      default:
        this.switchTab('activity-remote');
        break;
    }
  }

  renderActivityRemote() {
    const container = document.getElementById('activityRemoteContainer');
    if (!container) return;

    let currentActId = this.activitiesData.currentId;
    let act = (this.activitiesData.activities || []).find(a => String(a.id) === String(currentActId));
    if (!act) {
      act = (this.activitiesData.activities || [])[0] || { id: '-1', name: 'PowerOff', deviceIds: [] };
    }

    const assignedDevices = [];
    if (act.deviceIds && Array.isArray(act.deviceIds)) {
      for (const id of act.deviceIds) {
        const found = (this.inventory.devices || []).find(d => String(d.id) === String(id));
        if (found && !assignedDevices.some(d => String(d.id) === String(found.id))) {
          assignedDevices.push(found);
        }
      }
    }
    const availableDevices = assignedDevices;

    this.remoteEngine.render(container, {
      scope: 'activity',
      target: act,
      availableDevices,
      commandsMap: this.deviceCommandsMap,
      onSendCommand: (devId, cmd) => this.client.sendCommand(devId, cmd)
    });
  }

  renderDeviceRemote() {
    const container = document.getElementById('deviceRemoteContainer');
    const select = document.getElementById('deviceRemoteSelect');
    if (!container || !select) return;

    const devices = this.inventory.devices || [];
    if (devices.length === 0) {
      container.innerHTML = '<div class="muted text-center p-2">No devices found on this hub.</div>';
      return;
    }

    select.innerHTML = devices.map(d =>
      `<option value="${d.id}" ${String(d.id) === String(this.selectedDeviceId) ? 'selected' : ''}>${d.name || d.label} (${d.model || d.id})</option>`
    ).join('');

    if (!this.selectedDeviceId || !devices.some(d => String(d.id) === String(this.selectedDeviceId))) {
      this.selectedDeviceId = devices[0].id;
      select.value = this.selectedDeviceId;
    }

    const targetDevice = devices.find(d => String(d.id) === String(this.selectedDeviceId));
    this.remoteEngine.render(container, {
      scope: 'device',
      target: targetDevice,
      availableDevices: targetDevice ? [targetDevice] : [],
      commandsMap: this.deviceCommandsMap,
      onSendCommand: (devId, cmd) => this.client.sendCommand(devId, cmd)
    });
  }

  async renderHubsList() {
    const list = document.getElementById('hubsList');
    if (!list) return;
    const hubs = await getSavedHubs();
    list.innerHTML = hubs.map(h => {
      const isActive = this.activeHub && this.activeHub.id === h.id;
      return `
        <div class="card-item ${isActive ? 'active' : ''}" data-hub-id="${h.id}">
          <div>
            <div class="card-item-title">${h.name} ${isActive ? '★ (Active)' : ''}</div>
            <div class="card-item-sub">${h.host}:${h.port || 8080} ${h.username ? '• Auth: ' + h.username : '• No Auth'}</div>
          </div>
          <div style="display:flex;gap:6px;">
            ${!isActive ? `<button type="button" class="btn btn-xs btn-primary btn-switch-hub">Select</button>` : ''}
            <button type="button" class="btn btn-xs btn-secondary btn-edit-hub">Edit</button>
            <button type="button" class="btn btn-xs btn-danger btn-delete-hub">Delete</button>
          </div>
        </div>
      `;
    }).join('');

    list.querySelectorAll('.btn-switch-hub').forEach(b => {
      b.addEventListener('click', async (e) => {
        e.stopPropagation();
        const card = b.closest('.card-item');
        const id = card.getAttribute('data-hub-id');
        await setActiveHubId(id);
        await this.loadActiveHub();
        this.renderHubsList();
      });
    });

    list.querySelectorAll('.btn-edit-hub').forEach(b => {
      b.addEventListener('click', async (e) => {
        e.stopPropagation();
        const card = b.closest('.card-item');
        const id = card.getAttribute('data-hub-id');
        const hub = hubs.find(h => h.id === id);
        this.openEditHubModal(hub);
      });
    });

    list.querySelectorAll('.btn-delete-hub').forEach(b => {
      b.addEventListener('click', async (e) => {
        e.stopPropagation();
        const card = b.closest('.card-item');
        const id = card.getAttribute('data-hub-id');
        if (confirm('Delete this saved hub?')) {
          await deleteHub(id);
          await this.loadActiveHub();
          this.renderHubsList();
        }
      });
    });
  }

  openHubModal() {
    this.switchTab('hubs');
  }

  openAddHubModal() {
    this.openEditHubModal(null);
  }

  openEditHubModal(hub) {
    let modal = document.getElementById('hubEditorModal');
    if (!modal) {
      modal = document.createElement('div');
      modal.id = 'hubEditorModal';
      modal.className = 'modal-backdrop';
      document.body.appendChild(modal);
    }

    const isEdit = !!hub;
    const h = hub || {
      id: 'hub_' + Date.now(),
      name: 'Living Room Hub',
      host: '192.168.1.100',
      port: 8080,
      username: '',
      password: ''
    };

    modal.innerHTML = `
      <div class="modal-card">
        <div class="modal-header">
          <h3>${isEdit ? 'Edit Hub Connection' : 'Add Harmony Hub'}</h3>
          <button type="button" class="btn btn-ghost btn-sm" id="btnHubClose">✕</button>
        </div>
        <div class="modal-body">
          <div class="form-group">
            <label>Hub Friendly Name</label>
            <input type="text" id="hubFormName" class="form-control" value="${h.name}">
          </div>
          <div class="form-group">
            <label>IP Address / Hostname</label>
            <input type="text" id="hubFormHost" class="form-control" value="${h.host}" placeholder="e.g. 192.168.1.150">
          </div>
          <div class="form-group">
            <label>Port</label>
            <input type="number" id="hubFormPort" class="form-control" value="${h.port || 8080}">
          </div>
          <div class="form-group">
            <label>Username (Optional Basic Auth)</label>
            <input type="text" id="hubFormUser" class="form-control" value="${h.username || ''}" placeholder="Leave blank if disabled">
          </div>
          <div class="form-group">
            <label>Password (Optional)</label>
            <input type="password" id="hubFormPass" class="form-control" value="${h.password || ''}" placeholder="Leave blank if disabled">
          </div>
          <div id="hubTestStatus" class="mini muted text-center pt-2"></div>
        </div>
        <div class="modal-footer">
          <button type="button" class="btn btn-ghost btn-sm" id="btnHubTest">Test Connection</button>
          <div style="flex:1;"></div>
          <button type="button" class="btn btn-secondary btn-sm" id="btnHubCancel">Cancel</button>
          <button type="button" class="btn btn-primary btn-sm" id="btnHubSave">Save Hub</button>
        </div>
      </div>
    `;
    modal.style.display = 'flex';

    const closeModal = () => { modal.style.display = 'none'; };
    modal.querySelector('#btnHubClose').addEventListener('click', closeModal);
    modal.querySelector('#btnHubCancel').addEventListener('click', closeModal);

    modal.querySelector('#btnHubTest').addEventListener('click', async () => {
      const statusEl = modal.querySelector('#hubTestStatus');
      statusEl.textContent = 'Testing connection...';
      const tempClient = new HarmonyClient({
        host: modal.querySelector('#hubFormHost').value.trim(),
        port: Number(modal.querySelector('#hubFormPort').value) || 8080,
        username: modal.querySelector('#hubFormUser').value.trim(),
        password: modal.querySelector('#hubFormPass').value.trim()
      });
      const res = await tempClient.testConnection();
      if (res.ok) {
        statusEl.innerHTML = '<span style="color:var(--success);">✓ Connected successfully!</span>';
      } else {
        statusEl.innerHTML = `<span style="color:var(--danger);">✗ ${res.error}</span>`;
      }
    });

    modal.querySelector('#btnHubSave').addEventListener('click', async () => {
      h.name = modal.querySelector('#hubFormName').value.trim() || 'Harmony Hub';
      h.host = modal.querySelector('#hubFormHost').value.trim() || '127.0.0.1';
      h.port = Number(modal.querySelector('#hubFormPort').value) || 8080;
      h.username = modal.querySelector('#hubFormUser').value.trim();
      h.password = modal.querySelector('#hubFormPass').value.trim();

      await saveHub(h);
      if (!isEdit || (this.activeHub && this.activeHub.id === h.id)) {
        await setActiveHubId(h.id);
        await this.loadActiveHub();
      }
      closeModal();
      this.renderHubsList();
    });
  }
}

document.addEventListener('DOMContentLoaded', () => {
  const app = new HarmonyApp();
  app.init();
});
