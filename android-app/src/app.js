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

    // Settings Sub-tab Switching
    document.querySelectorAll('.set-tab-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const sub = btn.getAttribute('data-set-tab');
        document.querySelectorAll('.set-tab-btn').forEach(b => {
          b.classList.toggle('btn-primary', b === btn);
          b.classList.toggle('btn-secondary', b !== btn);
        });
        document.querySelectorAll('.set-tab-content').forEach(p => {
          p.style.display = p.id === `set-tab-${sub}` ? 'block' : 'none';
        });
        if (sub === 'sys') this.loadSystemInfo();
        if (sub === 'net') this.loadNetworkInfo();
        if (sub === 'bt') this.loadBtInfo();
        if (sub === 'irlab') this.loadIrLabInfo();
      });
    });

    // Reboot Hub
    const btnReboot = document.getElementById('btnRebootHub');
    if (btnReboot) {
      btnReboot.addEventListener('click', async () => {
        if (!confirm('Are you sure you want to reboot the Harmony Hub?')) return;
        try {
          btnReboot.disabled = true;
          btnReboot.textContent = 'Rebooting...';
          await this.client.rebootHub();
          alert('Reboot command sent to hub.');
        } catch (err) {
          alert('Reboot failed: ' + err.message);
        } finally {
          btnReboot.disabled = false;
          btnReboot.textContent = 'Reboot Hub';
        }
      });
    }

    // Check Update
    const btnCheck = document.getElementById('btnCheckUpdate');
    if (btnCheck) {
      btnCheck.addEventListener('click', async () => {
        const msg = document.getElementById('sysUpdateMsg');
        try {
          btnCheck.disabled = true;
          if (msg) msg.textContent = 'Checking GitHub for updates...';
          const res = await this.client.checkUpdate(true);
          if (msg) msg.textContent = res.message || 'Check complete.';
        } catch (err) {
          if (msg) msg.textContent = 'Update check failed: ' + err.message;
        } finally {
          btnCheck.disabled = false;
        }
      });
    }

    // Scan Wi-Fi & Join
    const btnScanWifi = document.getElementById('btnScanWifi');
    if (btnScanWifi) {
      btnScanWifi.addEventListener('click', async () => {
        const list = document.getElementById('wifiScanResults');
        if (list) list.innerHTML = '<span style="font-size:12px;color:var(--text-dim)">Scanning Wi-Fi...</span>';
        try {
          const res = await this.client.getWifiScan();
          const ssids = res.ssids || res.networks || [];
          if (!ssids.length) {
            if (list) list.innerHTML = '<span style="font-size:12px;color:var(--text-dim)">No networks found.</span>';
            return;
          }
          if (list) {
            list.innerHTML = ssids.map(s => {
              const name = typeof s === 'string' ? s : (s.ssid || 'Unknown');
              return `<button type="button" class="btn btn-xs btn-secondary wifi-ssid-pick" data-ssid="${name}" style="margin:2px;">${name}</button>`;
            }).join(' ');
            list.querySelectorAll('.wifi-ssid-pick').forEach(b => {
              b.addEventListener('click', () => {
                const input = document.getElementById('wifiSsidInput');
                if (input) input.value = b.getAttribute('data-ssid');
              });
            });
          }
        } catch (err) {
          if (list) list.innerHTML = `<span style="font-size:12px;color:var(--danger)">Scan failed: ${err.message}</span>`;
        }
      });
    }

    const formJoinWifi = document.getElementById('formJoinWifi');
    if (formJoinWifi) {
      formJoinWifi.addEventListener('submit', async (e) => {
        e.preventDefault();
        const ssid = document.getElementById('wifiSsidInput')?.value.trim();
        const psk = document.getElementById('wifiPskInput')?.value;
        const msg = document.getElementById('wifiStatusMsg');
        if (!ssid) return;
        try {
          if (msg) msg.textContent = 'Saving Wi-Fi settings...';
          await this.client.saveWifi(ssid, psk);
          if (msg) msg.innerHTML = '<span style="color:var(--success)">Saved! Reboot hub to apply.</span>';
        } catch (err) {
          if (msg) msg.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
        }
      });
    }

    // Bluetooth Scan
    const btnScanBt = document.getElementById('btnScanBt');
    if (btnScanBt) {
      btnScanBt.addEventListener('click', async () => {
        const list = document.getElementById('btDeviceList');
        if (list) list.innerHTML = '<span style="font-size:12px;color:var(--text-dim)">Scanning Bluetooth...</span>';
        try {
          await this.client.startBtScan();
          setTimeout(async () => {
            const res = await this.client.getBtStatus();
            const devs = res.devices || [];
            if (!devs.length) {
              if (list) list.innerHTML = '<span style="font-size:12px;color:var(--text-dim)">No Bluetooth devices found.</span>';
              return;
            }
            if (list) {
              list.innerHTML = devs.map(d => `
                <div class="card" style="display:flex;justify-content:space-between;align-items:center;padding:8px 12px;margin-bottom:6px;">
                  <div>
                    <strong>${d.name || 'Unknown'}</strong>
                    <div style="font-size:11px;color:var(--text-dim)">${d.bdaddr || d.address}</div>
                  </div>
                  <button type="button" class="btn btn-xs btn-primary btn-pair-bt" data-addr="${d.bdaddr || d.address}">Pair</button>
                </div>
              `).join('');
              list.querySelectorAll('.btn-pair-bt').forEach(b => {
                b.addEventListener('click', async () => {
                  const addr = b.getAttribute('data-addr');
                  b.textContent = 'Pairing...';
                  try {
                    await this.client.pairBt(addr);
                    b.textContent = 'Paired!';
                  } catch (err) {
                    alert('Pair failed: ' + err.message);
                    b.textContent = 'Pair';
                  }
                });
              });
            }
          }, 3000);
        } catch (err) {
          if (list) list.innerHTML = `<span style="font-size:12px;color:var(--danger)">BT Scan failed: ${err.message}</span>`;
        }
      });
    }

    // IR Learning
    const btnCapture = document.getElementById('btnCaptureIr');
    if (btnCapture) {
      btnCapture.addEventListener('click', async () => {
        const statusEl = document.getElementById('irLabCaptureStatus');
        const rawEl = document.getElementById('irLabRawText');
        if (statusEl) statusEl.textContent = 'Listening for IR signal (press remote button)...';
        try {
          const res = await this.client.captureIr();
          if (res.raw) {
            if (rawEl) rawEl.value = res.raw;
            if (statusEl) statusEl.textContent = `Captured signal! Mode: ${res.mode || 'raw'}`;
          } else {
            if (statusEl) statusEl.textContent = 'No signal received or timed out.';
          }
        } catch (err) {
          if (statusEl) statusEl.textContent = 'Capture failed: ' + err.message;
        }
      });
    }

    const btnTestIr = document.getElementById('btnTestLearnedIr');
    if (btnTestIr) {
      btnTestIr.addEventListener('click', async () => {
        const devSelect = document.getElementById('irLabDeviceSelect');
        const rawEl = document.getElementById('irLabRawText');
        const devId = devSelect?.value;
        const raw = rawEl?.value?.trim();
        const statusEl = document.getElementById('irLabCaptureStatus');
        if (!raw) {
          alert('Please capture a signal first.');
          return;
        }
        try {
          if (statusEl) statusEl.textContent = 'Testing signal...';
          const res = await this.client.testLearnedIr({ deviceId: devId, raw: raw });
          if (statusEl) statusEl.textContent = res.message || 'Test signal transmitted!';
        } catch (err) {
          if (statusEl) statusEl.textContent = 'Test failed: ' + err.message;
        }
      });
    }

    const btnSaveIr = document.getElementById('btnSaveLearnedIr');
    if (btnSaveIr) {
      btnSaveIr.addEventListener('click', async () => {
        const devSelect = document.getElementById('irLabDeviceSelect');
        const nameInput = document.getElementById('irLabCmdName');
        const rawEl = document.getElementById('irLabRawText');
        const msg = document.getElementById('irLabSaveMsg');
        const devId = devSelect?.value;
        const name = nameInput?.value?.trim();
        const raw = rawEl?.value?.trim();
        if (!devId || !name || !raw) {
          alert('Please select device, enter command name, and capture signal.');
          return;
        }
        try {
          if (msg) msg.textContent = 'Saving command...';
          await this.client.saveLearnedCommand({ deviceId: devId, name: name, raw: raw });
          if (msg) msg.innerHTML = '<span style="color:var(--success)">Command saved to device!</span>';
          await this.recoverHubInventory();
        } catch (err) {
          if (msg) msg.innerHTML = `<span style="color:var(--danger)">Save failed: ${err.message}</span>`;
        }
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
      case 'settings':
        this.renderSettings();
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

  renderSettings() {
    this.loadSystemInfo();
    this.loadIrLabInfo();
  }

  async loadSystemInfo() {
    const el = document.getElementById('sysDiskInfo');
    if (!el) return;
    try {
      const res = await this.client.getSystemDisk();
      el.textContent = res.output || 'System info retrieved.';
    } catch (err) {
      el.textContent = `Could not fetch system info: ${err.message}`;
    }
  }

  async loadNetworkInfo() {
    const el = document.getElementById('netStatusInfo');
    if (!el) return;
    try {
      const res = await this.client.getNetworkStatus();
      el.textContent = JSON.stringify(res, null, 2);
    } catch (err) {
      el.textContent = `Could not fetch network status: ${err.message}`;
    }
  }

  async loadBtInfo() {
    const el = document.getElementById('btStatusInfo');
    if (!el) return;
    try {
      const res = await this.client.getBtStatus();
      el.textContent = res.state ? `BT State: ${res.state}` : 'Bluetooth ready.';
    } catch (err) {
      el.textContent = `Could not fetch BT info: ${err.message}`;
    }
  }

  loadIrLabInfo() {
    const select = document.getElementById('irLabDeviceSelect');
    if (!select) return;
    const devs = this.inventory.devices || [];
    select.innerHTML = devs.map(d => `<option value="${d.id}">${d.name || d.model || d.id}</option>`).join('');
  }
}

document.addEventListener('DOMContentLoaded', () => {
  const app = new HarmonyApp();
  app.init();
});
