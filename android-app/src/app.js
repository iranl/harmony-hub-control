import { HarmonyClient } from './api.js';
import { getSavedHubs, saveHub, deleteHub, getActiveHub, setActiveHubId } from './storage.js';
import { RemoteEngine } from './remote-engine.js';
import {
  parseIrText,
  loadIrdSource,
  rankIrdEntries,
  fetchIrdFile,
  sourceLabel,
  safeImportName
} from './irdb.js';

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
    this.wsProgress = null;
    this.isHubOnline = false;

    // Sub-states
    this.b25MapData = { remote: { name: 'Homatics B25' }, activities: {} };
    this.b25SelectedBtn = 'power';
    this.kbMods = { ctrl: false, shift: false, alt: false, win: false };
    this.labQueue = [];
    this.labRunning = false;
    this.labRunId = null;

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
        this.refreshActivityState();
      }
    };
  }

  async init() {
    this.bindEvents();
    await this.loadActiveHub();
  }

  bindEvents() {
    // Drawer Navigation
    const btnOpenDrawer = document.getElementById('btnOpenDrawer');
    const btnCloseDrawer = document.getElementById('btnCloseDrawer');
    const drawerBackdrop = document.getElementById('drawerBackdrop');
    const appDrawer = document.getElementById('appDrawer');

    const toggleDrawer = (open) => {
      if (appDrawer) appDrawer.classList.toggle('active', open);
      if (drawerBackdrop) drawerBackdrop.classList.toggle('active', open);
    };

    if (btnOpenDrawer) btnOpenDrawer.addEventListener('click', () => toggleDrawer(true));
    if (btnCloseDrawer) btnCloseDrawer.addEventListener('click', () => toggleDrawer(false));
    if (drawerBackdrop) drawerBackdrop.addEventListener('click', () => toggleDrawer(false));

    // Bottom Navigation Items
    document.querySelectorAll('.app-nav .nav-item').forEach(btn => {
      btn.addEventListener('click', () => {
        if (btn.id === 'navItemMore') {
          toggleDrawer(true);
          return;
        }
        const tab = btn.getAttribute('data-tab');
        if (tab) this.switchTab(tab);
      });
    });

    // Drawer Menu Items
    document.querySelectorAll('.drawer-item').forEach(btn => {
      btn.addEventListener('click', () => {
        const target = btn.getAttribute('data-view-target');
        if (target) {
          this.switchTab(target);
          toggleDrawer(false);
        }
      });
    });

    // Quick Action Buttons (Dashboard shortcuts)
    document.querySelectorAll('.quick-action-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const target = btn.getAttribute('data-view-target');
        if (target) this.switchTab(target);
      });
    });

    // Hub Switcher Button in Header
    const hubBtn = document.getElementById('hubSelectorBtn');
    if (hubBtn) {
      hubBtn.addEventListener('click', () => this.switchTab('hubs'));
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

    // Stepper navigation in IR Setup
    document.querySelectorAll('.step-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const target = btn.getAttribute('data-step-target');
        document.querySelectorAll('.step-btn').forEach(b => {
          b.classList.toggle('btn-primary', b === btn);
          b.classList.toggle('btn-ghost', b !== btn);
        });
        document.querySelectorAll('.wizard-panel').forEach(p => {
          p.style.display = p.getAttribute('data-panel') === target ? 'block' : 'none';
        });
        if (target === 'manage') this.populateManageDevices();
        if (target === 'verify') this.populateVerifyDropdowns();
      });
    });

    // Activities Actions
    document.getElementById('actRefreshBtn')?.addEventListener('click', () => this.loadActivitiesView());
    document.getElementById('actPowerOffBtn')?.addEventListener('click', () => this.client.stopActivity());
    document.getElementById('actNewBtn')?.addEventListener('click', () => this.openActivityEditor(null));
    document.getElementById('actEditorCloseBtn')?.addEventListener('click', () => {
      const box = document.getElementById('actEditorBox');
      if (box) box.style.display = 'none';
    });
    document.getElementById('actCancelBtn')?.addEventListener('click', () => {
      const box = document.getElementById('actEditorBox');
      if (box) box.style.display = 'none';
    });
    document.getElementById('actSaveBtn')?.addEventListener('click', () => this.saveCurrentActivity());
    document.getElementById('actDeleteBtn')?.addEventListener('click', () => this.deleteCurrentActivity());

    document.getElementById('actAddStartCmdBtn')?.addEventListener('click', () => this.addSequenceStep('actStartStepsList', 'IRCommand'));
    document.getElementById('actAddStartDelayBtn')?.addEventListener('click', () => this.addSequenceStep('actStartStepsList', 'Delay'));
    document.getElementById('actAddStopCmdBtn')?.addEventListener('click', () => this.addSequenceStep('actStopStepsList', 'IRCommand'));
    document.getElementById('actAddStopDelayBtn')?.addEventListener('click', () => this.addSequenceStep('actStopStepsList', 'Delay'));

    document.getElementById('actTestStartSeqBtn')?.addEventListener('click', () => this.testLiveSequence('actStartStepsList'));
    document.getElementById('actTestStopSeqBtn')?.addEventListener('click', () => this.testLiveSequence('actStopStepsList'));
    document.getElementById('previewSimulateBtn')?.addEventListener('click', () => this.simulateActivityTransition());

    // IR Wizard Actions
    document.getElementById('btnCreateNewDevice')?.addEventListener('click', () => this.submitNewDeviceProfile());
    document.getElementById('irdbLoadIndex')?.addEventListener('click', () => this.searchIrdb());
    document.getElementById('irdbFetch')?.addEventListener('click', () => this.fetchSelectedIrdbFile());
    document.getElementById('irdbParsePaste')?.addEventListener('click', () => this.parsePastIrdb());
    document.getElementById('irdbSaveBtn')?.addEventListener('click', () => this.saveSelectedIrdbCommands());
    document.getElementById('wizardCapture')?.addEventListener('click', () => this.captureIrSignal());
    document.getElementById('wizardLearnTest')?.addEventListener('click', () => this.testLearnedIrSignal());
    document.getElementById('wizardSaveBtn')?.addEventListener('click', () => this.saveLearnedIrCommand());
    document.getElementById('wizardTest')?.addEventListener('click', () => this.sendVerifyTestCommand());

    // Bulk IR Lab Actions
    document.getElementById('labCandidatesBtn')?.addEventListener('click', () => this.findLabCandidates());
    document.getElementById('labScan')?.addEventListener('click', () => this.loadLabCommands());
    document.getElementById('labStored')?.addEventListener('click', () => this.useStoredLabCommands());
    document.getElementById('labClear')?.addEventListener('click', () => this.clearLabQueue());
    document.getElementById('labSelectAll')?.addEventListener('click', () => this.setLabSelections(true));
    document.getElementById('labSelectNone')?.addEventListener('click', () => this.setLabSelections(false));
    document.getElementById('labDropUnchecked')?.addEventListener('click', () => this.dropUncheckedLab());
    document.getElementById('labImport')?.addEventListener('click', () => this.importLabSelected());
    document.getElementById('labRun')?.addEventListener('click', () => this.runLabQueue());
    document.getElementById('labStop')?.addEventListener('click', () => this.stopLabQueue());

    // Bluetooth View Actions
    document.getElementById('btPairToggleBtn')?.addEventListener('click', () => this.toggleBtPairing());
    document.getElementById('btDisconnBtn')?.addEventListener('click', () => this.client.disconnectBt());
    document.getElementById('btRefreshBtn')?.addEventListener('click', () => this.loadBtView());
    document.getElementById('btReleaseAll')?.addEventListener('click', () => this.client.sendBtKey('release_all'));
    document.getElementById('btSendTextBtn')?.addEventListener('click', () => this.sendBtDirectText());
    document.getElementById('btClearTextBtn')?.addEventListener('click', () => {
      const el = document.getElementById('btDirectText');
      if (el) el.value = '';
    });
    document.getElementById('btRunScriptBtn')?.addEventListener('click', () => this.runBtMacroScript());
    document.getElementById('btSaveCommandBtn')?.addEventListener('click', () => this.saveBtMacroCommand());
    document.getElementById('btClearLogBtn')?.addEventListener('click', () => {
      const el = document.getElementById('btLog');
      if (el) el.textContent = 'Log cleared.';
    });

    // Virtual Keyboard Buttons
    document.querySelectorAll('.kb-key').forEach(btn => {
      btn.addEventListener('click', async () => {
        await this.remoteEngine.triggerHaptic();
        const mod = btn.getAttribute('data-mod');
        if (mod) {
          this.kbMods[mod] = !this.kbMods[mod];
          btn.classList.toggle('kb-on', this.kbMods[mod]);
          return;
        }
        const key = btn.getAttribute('data-key');
        if (key) {
          let code = key;
          const mods = [];
          if (this.kbMods.ctrl) mods.push('ctrl');
          if (this.kbMods.shift) mods.push('shift');
          if (this.kbMods.alt) mods.push('alt');
          if (this.kbMods.win) mods.push('win');
          if (mods.length) code = `${mods.join('+')}+${key}`;
          try {
            await this.client.sendBtKey(code, 'tap');
          } catch (err) {
            console.warn('BT Key send error:', err);
          }
        }
      });
    });

    // Homatics B25 Buttons
    document.querySelectorAll('.b25-hotspot').forEach(btn => {
      btn.addEventListener('click', () => {
        const btnId = btn.getAttribute('data-btn');
        if (btnId) this.selectB25Button(btnId);
      });
    });
    document.getElementById('b25ActivitySelect')?.addEventListener('change', () => this.updateB25ButtonStates());
    document.getElementById('b25ActionType')?.addEventListener('change', () => this.toggleB25ActionFields());
    document.getElementById('b25ApplyBtn')?.addEventListener('click', () => this.applyB25Mapping());
    document.getElementById('b25ClearBtn')?.addEventListener('click', () => this.clearB25Mapping());
    document.getElementById('b25SaveAllBtn')?.addEventListener('click', () => this.saveAllB25Mappings());
    document.getElementById('b25RunPresetBtn')?.addEventListener('click', () => this.autoMapB25Preset());
    document.getElementById('b25ScanBtn')?.addEventListener('click', () => this.scanB25Remote());
    document.getElementById('b25PairBtn')?.addEventListener('click', () => this.pairB25Remote());

    // MQTT Actions
    document.getElementById('mqttSaveBtn')?.addEventListener('click', () => this.saveMqttSettings());

    // Network Actions
    document.getElementById('ethModeSelect')?.addEventListener('change', (e) => {
      const f = document.getElementById('ethStaticFields');
      if (f) f.style.display = e.target.value === 'static' ? 'block' : 'none';
    });
    document.querySelectorAll('.btn-save-eth').forEach(b => {
      b.addEventListener('click', () => this.saveEthernetConfig(b.getAttribute('data-apply')));
    });
    document.querySelectorAll('.btn-save-wifi').forEach(b => {
      b.addEventListener('click', () => this.saveWifiConfig(b.getAttribute('data-apply')));
    });

    // Backup & Restore Actions
    document.querySelectorAll('.btn-backup-export').forEach(b => {
      b.addEventListener('click', () => this.exportBackupItem(b.getAttribute('data-target')));
    });
    document.getElementById('importFile')?.addEventListener('change', (e) => {
      const file = e.target.files && e.target.files[0];
      if (!file) return;
      const reader = new FileReader();
      reader.onload = () => {
        const box = document.getElementById('backupPayload');
        if (box) box.value = reader.result || '';
      };
      reader.readAsText(file);
    });
    document.getElementById('btnRestoreBackup')?.addEventListener('click', () => this.restoreBackupPayload());

    // System Actions
    document.getElementById('btnSaveDebug')?.addEventListener('click', () => this.saveDebugLogging());
    document.getElementById('btnSaveAuth')?.addEventListener('click', () => this.saveWebUiAuth());
    document.getElementById('updateCheck')?.addEventListener('click', () => this.checkSoftwareUpdate());
    document.getElementById('updateInstall')?.addEventListener('click', () => this.installSoftwareUpdate());
    document.getElementById('btnRediscover')?.addEventListener('click', () => this.rediscoverMqtt());
    document.getElementById('btnRebootHub')?.addEventListener('click', () => this.rebootHub());
  }

  async loadActiveHub() {
    this.activeHub = await getActiveHub();
    if (this.activeHub) {
      document.getElementById('activeHubLabel').textContent = this.activeHub.name || 'Harmony Hub';
      this.client.updateHub(this.activeHub);
      await this.refreshData();
    } else {
      document.getElementById('activeHubLabel').textContent = 'No Hub Selected';
      this.setHubStatus(false);
      this.openHubModal();
    }
  }

  async refreshData() {
    try {
      const [inv, acts] = await Promise.all([
        this.client.getInventory(),
        this.client.getActivities()
      ]);
      this.inventory = inv || { devices: [] };
      this.activitiesData = acts || { activities: [], currentId: '-1', currentName: 'PowerOff' };
      this.setHubStatus(true);
    } catch (err) {
      console.warn('Failed to load initial hub data:', err);
      this.setHubStatus(false);
    }
    this.updateActivityBanner();
    this.switchTab(this.currentTab);
  }

  async refreshActivityState() {
    try {
      const acts = await this.client.getActivities();
      if (acts) {
        this.activitiesData = acts;
        this.setHubStatus(true);
        this.updateActivityBanner();
      }
    } catch {
      this.setHubStatus(false);
    }
  }

  switchTab(tabId) {
    if (!tabId) tabId = 'activity-remote';
    this.currentTab = tabId;

    // Toggle active view panel
    document.querySelectorAll('.view-panel').forEach(panel => {
      panel.classList.toggle('active', panel.id === `view-${tabId}`);
    });

    // Update bottom nav active state
    document.querySelectorAll('.app-nav .nav-item').forEach(btn => {
      const t = btn.getAttribute('data-tab');
      btn.classList.toggle('active', t === tabId);
    });

    // Update drawer item active state
    document.querySelectorAll('.drawer-item').forEach(btn => {
      const t = btn.getAttribute('data-view-target');
      btn.classList.toggle('active', t === tabId);
    });

    // Load content for active tab
    if (tabId === 'overview') this.loadDashboard();
    else if (tabId === 'activity-remote') this.renderActivityRemote();
    else if (tabId === 'activities') this.loadActivitiesView();
    else if (tabId === 'control') this.renderControlView();
    else if (tabId === 'ir') this.renderIrView();
    else if (tabId === 'lab') this.initLabView();
    else if (tabId === 'bluetooth') this.loadBtView();
    else if (tabId === 'remotes') this.loadRemotesView();
    else if (tabId === 'mqtt') this.loadMqttView();
    else if (tabId === 'network') this.loadNetworkView();
    else if (tabId === 'backup') this.loadBackupView();
    else if (tabId === 'system') this.loadSystemView();
    else if (tabId === 'hubs') this.renderHubsList();
  }

  updateActivityBanner() {
    const listEl = document.getElementById('bannerActivitiesList');
    if (!listEl) return;

    const acts = this.activitiesData.activities || [];
    const curId = String(this.activitiesData.currentId || '-1');

    listEl.innerHTML = acts.map(a => {
      const isActive = String(a.id) === curId;
      return `<button type="button" class="btn btn-xs ${isActive ? 'btn-primary' : 'btn-secondary'} banner-activity-btn ${isActive ? 'active' : ''}" data-act-id="${a.id}">${a.name || a.label}</button>`;
    }).join('');

    listEl.querySelectorAll('.banner-activity-btn').forEach(btn => {
      btn.addEventListener('click', async () => {
        const id = btn.getAttribute('data-act-id');
        if (id === curId) return;
        await this.remoteEngine.triggerHaptic();
        btn.classList.add('btn-switching');
        try {
          await this.client.startActivity(id);
        } catch (err) {
          alert('Failed to start activity: ' + err.message);
        } finally {
          btn.classList.remove('btn-switching');
        }
      });
    });
  }

  /* -------------------------------------------------------------
   * 1. Activity Remote View
   * ------------------------------------------------------------- */
  async renderActivityRemote() {
    const cont = document.getElementById('activityRemoteContainer');
    if (!cont) return;

    const curId = String(this.activitiesData.currentId || '-1');
    const curAct = (this.activitiesData.activities || []).find(a => String(a.id) === curId);

    if (!curAct || curId === '-1') {
      cont.innerHTML = `
        <div class="card" style="text-align: center; padding: 32px 16px;">
          <h3 style="font-size: 16px; margin-bottom: 8px;">Hub is in PowerOff State</h3>
          <p class="muted" style="font-size: 13px; margin-bottom: 16px;">Select an activity from the top banner to turn on devices and use the remote.</p>
          <div style="display: flex; gap: 8px; justify-content: center; flex-wrap: wrap;">
            ${(this.activitiesData.activities || []).map(a => `
              <button type="button" class="btn btn-sm btn-primary btn-start-act-idle" data-act-id="${a.id}">Start ${a.name || a.label}</button>
            `).join('')}
          </div>
        </div>
      `;
      cont.querySelectorAll('.btn-start-act-idle').forEach(b => {
        b.addEventListener('click', async () => {
          await this.client.startActivity(b.getAttribute('data-act-id'));
          await this.refreshActivityState();
        });
      });
      return;
    }

    await this.remoteEngine.render(cont, {
      scope: 'activity',
      target: curAct,
      availableDevices: this.inventory.devices || [],
      commandsMap: this.deviceCommandsMap,
      onSendCommand: async (devId, cmd) => {
        await this.client.sendCommand(devId, cmd);
      }
    });
  }

  /* -------------------------------------------------------------
   * 2. Dashboard View
   * ------------------------------------------------------------- */
  async loadDashboard() {
    const fwEl = document.getElementById('dashFirmwareVal');
    const utEl = document.getElementById('dashUptimeVal');
    const mqBadge = document.getElementById('dashMqttBadge');
    const mqDet = document.getElementById('dashMqttDetail');
    const invVal = document.getElementById('dashInventoryVal');
    const cmdVal = document.getElementById('dashCommandsVal');
    const actName = document.getElementById('dashActiveActName');
    const actDet = document.getElementById('dashActiveActDetail');

    if (actName) actName.textContent = this.activitiesData.currentName || 'PowerOff';
    if (actDet) actDet.textContent = `ID: ${this.activitiesData.currentId || '-1'}`;

    const devs = this.inventory.devices || [];
    let totalCmds = 0;
    devs.forEach(d => { totalCmds += (d.commands || []).length; });
    if (invVal) invVal.textContent = `${devs.length} devices`;
    if (cmdVal) cmdVal.textContent = `${totalCmds} commands`;

    try {
      const mq = await this.client.getMqttStatus();
      if (mqBadge) {
        mqBadge.className = `badge ${mq.connected ? 'ok' : 'bad'}`;
        mqBadge.textContent = mq.connected ? 'Connected' : 'Disconnected';
      }
      if (mqDet) mqDet.textContent = mq.enabled ? 'Bridge enabled' : 'Bridge disabled';
    } catch {}

    try {
      const disk = await this.client.getSystemDisk();
      if (fwEl) fwEl.textContent = 'Linux 2.6.31 (Codex)';
      if (utEl && disk.output) {
        const m = disk.output.match(/up\s+([^,]+)/i);
        utEl.textContent = m ? m[1] : 'Online';
      }
    } catch {}
  }

  /* -------------------------------------------------------------
   * 3. Activities Manager View
   * ------------------------------------------------------------- */
  async loadActivitiesView() {
    await this.refreshActivityState();
    const curNameEl = document.getElementById('actCurrentName');
    const curMetaEl = document.getElementById('actCurrentMeta');
    const badgeEl = document.getElementById('actCurrentStatusBadge');

    if (curNameEl) curNameEl.textContent = this.activitiesData.currentName || 'PowerOff';
    if (curMetaEl) curMetaEl.textContent = `Activity ID: ${this.activitiesData.currentId || '-1'}`;
    if (badgeEl) {
      badgeEl.className = `badge ${this.activitiesData.isTransitioning ? 'warn' : 'ok'}`;
      badgeEl.textContent = this.activitiesData.isTransitioning ? 'Switching...' : 'Idle';
    }

    const grid = document.getElementById('actGrid');
    if (!grid) return;

    const acts = this.activitiesData.activities || [];
    if (!acts.length) {
      grid.innerHTML = '<div class="muted">No configured activities found.</div>';
      return;
    }

    grid.innerHTML = acts.map(a => `
      <div class="card" style="display: flex; flex-direction: column; justify-content: space-between; gap: 8px;">
        <div>
          <div style="display: flex; justify-content: space-between; align-items: flex-start;">
            <strong style="font-size: 14px;">${a.name || a.label}</strong>
            <span class="badge">${a.type || 'Activity'}</span>
          </div>
          <div class="muted mini" style="margin-top: 4px;">Order: ${a.order || 1} • ${(a.devices || []).length} devices</div>
        </div>
        <div style="display: flex; gap: 6px; margin-top: 6px;">
          <button type="button" class="btn btn-xs btn-primary btn-act-run" data-id="${a.id}" style="flex: 1;">Run</button>
          <button type="button" class="btn btn-xs btn-secondary btn-act-edit" data-id="${a.id}">Edit</button>
        </div>
      </div>
    `).join('');

    grid.querySelectorAll('.btn-act-run').forEach(b => {
      b.addEventListener('click', async () => {
        await this.client.startActivity(b.getAttribute('data-id'));
        await this.refreshActivityState();
      });
    });

    grid.querySelectorAll('.btn-act-edit').forEach(b => {
      b.addEventListener('click', () => {
        const id = b.getAttribute('data-id');
        const act = (this.activitiesData.activities || []).find(x => String(x.id) === String(id));
        this.openActivityEditor(act);
      });
    });

    // Populate transition simulator dropdowns
    const fromSel = document.getElementById('previewFromAct');
    const toSel = document.getElementById('previewToAct');
    if (fromSel && toSel) {
      const opts = acts.map(a => `<option value="${a.id}">${a.name || a.label}</option>`).join('');
      fromSel.innerHTML = `<option value="-1">PowerOff</option>${opts}`;
      toSel.innerHTML = `${opts}<option value="-1">PowerOff</option>`;
    }
  }

  openActivityEditor(act) {
    const box = document.getElementById('actEditorBox');
    if (!box) return;
    box.style.display = 'block';

    const titleEl = document.getElementById('actEditorTitle');
    const idInput = document.getElementById('actEditId');
    const nameInput = document.getElementById('actEditName');
    const typeSelect = document.getElementById('actEditType');
    const orderInput = document.getElementById('actEditOrder');
    const delBtn = document.getElementById('actDeleteBtn');

    if (act) {
      if (titleEl) titleEl.textContent = `Edit Activity: ${act.name}`;
      if (idInput) idInput.value = act.id;
      if (nameInput) nameInput.value = act.name || '';
      if (typeSelect) typeSelect.value = act.type || 'Custom';
      if (orderInput) orderInput.value = act.order || 1;
      if (delBtn) delBtn.style.display = (String(act.id) === '-1') ? 'none' : 'block';
    } else {
      if (titleEl) titleEl.textContent = 'Create New Activity';
      if (idInput) idInput.value = '';
      if (nameInput) nameInput.value = '';
      if (typeSelect) typeSelect.value = 'VirtualTelevision';
      if (orderInput) orderInput.value = 1;
      if (delBtn) delBtn.style.display = 'none';
    }

    // Participating devices checkboxes
    const chkCont = document.getElementById('actDevicesCheckboxes');
    if (chkCont) {
      const partDevs = new Set((act?.devices || []).map(String));
      chkCont.innerHTML = (this.inventory.devices || []).map(d => `
        <label style="display: inline-flex; align-items: center; gap: 6px; font-size: 12px; cursor: pointer;">
          <input type="checkbox" class="act-part-dev" value="${d.id}" ${partDevs.has(String(d.id)) ? 'checked' : ''}>
          <span>${d.name || d.model || d.id}</span>
        </label>
      `).join('');
    }

    // Render Sequences
    this.renderSequenceList('actStartStepsList', act?.startSequence || []);
    this.renderSequenceList('actStopStepsList', act?.stopSequence || []);
  }

  renderSequenceList(containerId, steps = []) {
    const cont = document.getElementById(containerId);
    if (!cont) return;
    cont.innerHTML = '';
    if (!steps.length) {
      cont.innerHTML = '<div class="muted mini" style="padding: 8px;">No steps configured.</div>';
      return;
    }
    steps.forEach((st, idx) => {
      this.addSequenceStep(containerId, st.type || 'IRCommand', st, idx);
    });
  }

  addSequenceStep(containerId, type = 'IRCommand', initialData = null, index = -1) {
    const cont = document.getElementById(containerId);
    if (!cont) return;
    const placeholder = cont.querySelector('.muted');
    if (placeholder) placeholder.remove();

    const row = document.createElement('div');
    row.className = 'queue-row';
    const isDelay = (type === 'Delay');

    row.innerHTML = `
      <div style="display: flex; align-items: center; gap: 6px; flex: 1;">
        <span class="badge ${isDelay ? 'warn' : 'ok'}" style="min-width: 60px; justify-content: center;">${isDelay ? 'Delay' : 'Command'}</span>
        ${isDelay ? `
          <input type="number" class="form-control step-delay-ms" min="50" max="60000" step="50" value="${initialData?.delayMs || 1000}" style="width: 110px;"> ms
        ` : `
          <select class="form-control step-dev-select" style="max-width: 140px;">
            ${(this.inventory.devices || []).map(d => `<option value="${d.id}" ${String(d.id) === String(initialData?.deviceId) ? 'selected' : ''}>${d.name || d.id}</option>`).join('')}
          </select>
          <input type="text" class="form-control step-cmd-input" value="${initialData?.command || ''}" placeholder="Command (e.g. PowerOn)" style="flex: 1;">
        `}
      </div>
      <div style="display: flex; gap: 4px;">
        <button type="button" class="btn btn-xs btn-ghost btn-step-up">↑</button>
        <button type="button" class="btn btn-xs btn-ghost btn-step-down">↓</button>
        <button type="button" class="btn btn-xs btn-danger btn-step-del">✕</button>
      </div>
    `;

    row.querySelector('.btn-step-up').addEventListener('click', () => {
      if (row.previousElementSibling) cont.insertBefore(row, row.previousElementSibling);
    });
    row.querySelector('.btn-step-down').addEventListener('click', () => {
      if (row.nextElementSibling) cont.insertBefore(row.nextElementSibling, row);
    });
    row.querySelector('.btn-step-del').addEventListener('click', () => {
      row.remove();
      if (!cont.children.length) cont.innerHTML = '<div class="muted mini" style="padding: 8px;">No steps configured.</div>';
    });

    cont.appendChild(row);
  }

  serializeSequence(containerId) {
    const cont = document.getElementById(containerId);
    if (!cont) return [];
    const steps = [];
    cont.querySelectorAll('.queue-row').forEach(row => {
      const delayInp = row.querySelector('.step-delay-ms');
      if (delayInp) {
        steps.push({ type: 'Delay', delayMs: Number(delayInp.value) || 1000 });
      } else {
        const devSel = row.querySelector('.step-dev-select');
        const cmdInp = row.querySelector('.step-cmd-input');
        if (devSel && cmdInp && cmdInp.value.trim()) {
          steps.push({
            type: 'IRCommand',
            deviceId: devSel.value,
            command: cmdInp.value.trim()
          });
        }
      }
    });
    return steps;
  }

  async testLiveSequence(containerId) {
    const steps = this.serializeSequence(containerId);
    if (!steps.length) return alert('No steps to test.');
    try {
      await this.client.testActivitySequence(JSON.stringify(steps));
      alert('Sequence test sent to hub.');
    } catch (err) {
      alert('Test failed: ' + err.message);
    }
  }

  async saveCurrentActivity() {
    const id = document.getElementById('actEditId')?.value;
    const name = document.getElementById('actEditName')?.value.trim();
    const type = document.getElementById('actEditType')?.value;
    const order = Number(document.getElementById('actEditOrder')?.value) || 1;
    const statusEl = document.getElementById('actEditorStatus');

    if (!name) return alert('Activity name is required.');

    const devices = [];
    document.querySelectorAll('.act-part-dev:checked').forEach(cb => devices.push(cb.value));

    const startSequence = this.serializeSequence('actStartStepsList');
    const stopSequence = this.serializeSequence('actStopStepsList');

    const actObj = { id: id || undefined, name, type, order, devices, startSequence, stopSequence };

    try {
      if (statusEl) statusEl.textContent = 'Saving activity...';
      await this.client.saveActivity(actObj);
      if (statusEl) statusEl.textContent = 'Saved successfully!';
      document.getElementById('actEditorBox').style.display = 'none';
      await this.refreshData();
      await this.loadActivitiesView();
    } catch (err) {
      if (statusEl) statusEl.textContent = 'Error: ' + err.message;
    }
  }

  async deleteCurrentActivity() {
    const id = document.getElementById('actEditId')?.value;
    if (!id || String(id) === '-1') return alert('Cannot delete this activity.');
    if (!confirm('Are you sure you want to delete this activity?')) return;
    try {
      await this.client.deleteActivity(id);
      document.getElementById('actEditorBox').style.display = 'none';
      await this.refreshData();
      await this.loadActivitiesView();
    } catch (err) {
      alert('Delete failed: ' + err.message);
    }
  }

  simulateActivityTransition() {
    const fromId = document.getElementById('previewFromAct')?.value;
    const toId = document.getElementById('previewToAct')?.value;
    const resultsBox = document.getElementById('previewResultsBox');
    const summaryEl = document.getElementById('previewSummary');
    const timelineEl = document.getElementById('previewTimeline');

    if (!resultsBox || !summaryEl || !timelineEl) return;

    const fromAct = (this.activitiesData.activities || []).find(a => String(a.id) === String(fromId));
    const toAct = (this.activitiesData.activities || []).find(a => String(a.id) === String(toId));

    const steps = [];
    if (fromAct && fromAct.stopSequence) {
      fromAct.stopSequence.forEach(s => steps.push({ ...s, phase: 'Exit ' + fromAct.name }));
    }
    if (toAct && toAct.startSequence) {
      toAct.startSequence.forEach(s => steps.push({ ...s, phase: 'Enter ' + toAct.name }));
    }

    resultsBox.style.display = 'block';
    summaryEl.textContent = `Transition from ${fromAct ? fromAct.name : 'PowerOff'} to ${toAct ? toAct.name : 'PowerOff'} (${steps.length} total operations)`;

    timelineEl.innerHTML = steps.map((st, i) => `
      <div class="queue-row">
        <span>${i + 1}. [${st.phase}] ${st.type === 'Delay' ? `Wait ${st.delayMs} ms` : `Send ${st.command} to ${st.deviceId}`}</span>
        <span class="badge ${st.type === 'Delay' ? 'warn' : 'ok'}">${st.type}</span>
      </div>
    `).join('') || '<div class="muted mini">No operations required.</div>';
  }

  /* -------------------------------------------------------------
   * 4. Control View (Device Remotes)
   * ------------------------------------------------------------- */
  renderControlView() {
    const sel = document.getElementById('controlDeviceSelect');
    const cont = document.getElementById('controlDeviceWorkspace');
    if (!sel || !cont) return;

    const devs = this.inventory.devices || [];
    if (!devs.length) {
      cont.innerHTML = '<div class="card"><div class="muted">No devices stored. Go to IR Setup to add one.</div></div>';
      return;
    }

    if (!this.selectedDeviceId || !devs.some(d => String(d.id) === String(this.selectedDeviceId))) {
      this.selectedDeviceId = devs[0].id;
    }

    sel.innerHTML = devs.map(d => `
      <option value="${d.id}" ${String(d.id) === String(this.selectedDeviceId) ? 'selected' : ''}>${d.name || d.model || d.id}</option>
    `).join('');

    sel.onchange = () => {
      this.selectedDeviceId = sel.value;
      this.renderControlView();
    };

    const curDev = devs.find(d => String(d.id) === String(this.selectedDeviceId));
    this.remoteEngine.render(cont, {
      scope: 'device',
      target: curDev,
      availableDevices: devs,
      commandsMap: this.deviceCommandsMap,
      onSendCommand: async (devId, cmd) => {
        await this.client.sendCommand(devId, cmd);
      }
    });
  }

  /* -------------------------------------------------------------
   * 5. IR Setup View
   * ------------------------------------------------------------- */
  renderIrView() {
    const devs = this.inventory.devices || [];
    ['wizardDevice', 'verifyDevice', 'irdbDevice'].forEach(id => {
      const el = document.getElementById(id);
      if (el) el.innerHTML = devs.map(d => `<option value="${d.id}">${d.name || d.id}</option>`).join('');
    });
  }

  async submitNewDeviceProfile() {
    const name = document.getElementById('newDeviceName')?.value.trim();
    const type = document.getElementById('newDeviceType')?.value;
    const manufacturer = document.getElementById('newDeviceManufacturer')?.value.trim();
    const model = document.getElementById('newDeviceModel')?.value.trim();
    const status = document.getElementById('profileStatus');

    if (!name || !manufacturer || !model) return alert('Name, manufacturer, and model are required.');
    try {
      if (status) status.textContent = 'Saving new device...';
      await this.client.saveNewDevice({ name, type, manufacturer, model });
      if (status) status.innerHTML = '<span style="color:var(--success)">✓ Device created successfully!</span>';
      await this.refreshData();
    } catch (err) {
      if (status) status.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
  }

  async searchIrdb() {
    const q = document.getElementById('irdbSearch')?.value.trim();
    const src = document.getElementById('irdbSource')?.value || 'all';
    const status = document.getElementById('irdbStatus');
    const log = document.getElementById('irdbLog');
    const resBox = document.getElementById('irdbResults');

    if (status) status.textContent = 'Searching index...';
    if (log) log.textContent = `Searching for "${q}" in ${src}...`;

    try {
      let entries = [];
      if (src === 'all') {
        const [a, b] = await Promise.all([loadIrdSource('irdb'), loadIrdSource('flipper')]);
        entries = a.concat(b);
      } else {
        entries = await loadIrdSource(src);
      }
      const matches = rankIrdEntries(entries, q);
      if (status) status.textContent = `Found ${matches.length} files.`;

      if (resBox) {
        resBox.style.display = matches.length ? 'block' : 'none';
        resBox.innerHTML = matches.map(m => `
          <button type="button" class="btn btn-xs btn-ghost irdb-pick" data-path="${m.path}" data-source="${m.source}" style="width: 100%; text-align: left; margin-bottom: 4px;">
            <strong>[${sourceLabel(m.source)}]</strong> ${m.path}
          </button>
        `).join('');

        resBox.querySelectorAll('.irdb-pick').forEach(b => {
          b.addEventListener('click', () => {
            const p = b.getAttribute('data-path');
            const s = b.getAttribute('data-source');
            const pathInp = document.getElementById('irdbPath');
            if (pathInp) {
              pathInp.value = p;
              pathInp.dataset.source = s;
            }
            this.fetchSelectedIrdbFile();
          });
        });
      }
    } catch (err) {
      if (status) status.textContent = 'Search failed: ' + err.message;
    }
  }

  async fetchSelectedIrdbFile() {
    const pathInp = document.getElementById('irdbPath');
    const p = pathInp?.value.trim();
    const s = pathInp?.dataset.source || 'flipper';
    const status = document.getElementById('irdbStatus');

    if (!p) return alert('Select or type a file path first.');
    try {
      if (status) status.textContent = 'Loading file...';
      const text = await fetchIrdFile(p, s);
      const rows = parseIrText(text, s, p);
      this.renderIrdbPreview(rows);
    } catch (err) {
      if (status) status.textContent = 'Failed to load file: ' + err.message;
    }
  }

  parsePastIrdb() {
    const paste = document.getElementById('irdbPaste')?.value.trim();
    if (!paste) return alert('Paste code contents first.');
    const rows = parseIrText(paste, 'custom', '');
    this.renderIrdbPreview(rows);
  }

  renderIrdbPreview(rows = []) {
    const previewBox = document.getElementById('irdbPreview');
    const status = document.getElementById('irdbStatus');
    if (!previewBox) return;

    previewBox.style.display = 'block';
    if (!rows.length) {
      previewBox.innerHTML = '<div class="muted mini">No valid commands parsed.</div>';
      return;
    }

    if (status) status.textContent = `Parsed ${rows.length} commands.`;
    previewBox.innerHTML = rows.map((r, i) => `
      <label style="display: flex; align-items: center; gap: 8px; font-size: 11px; margin-bottom: 4px;">
        <input type="checkbox" class="irdb-cmd-cb" data-name="${r.name}" data-keycode="${r.keycode || ''}" data-raw="${r.raw || ''}" checked>
        <span><strong>${r.name}</strong> (${r.meta})</span>
      </label>
    `).join('');
  }

  async saveSelectedIrdbCommands() {
    const devId = document.getElementById('irdbDevice')?.value;
    const prefix = document.getElementById('irdbPrefix')?.value || '';
    if (!devId) return alert('Select a device first.');

    const lines = [];
    document.querySelectorAll('.irdb-cmd-cb:checked').forEach(cb => {
      const name = prefix + cb.getAttribute('data-name');
      const raw = cb.getAttribute('data-raw');
      const key = cb.getAttribute('data-keycode');
      if (raw) lines.push(`${name}|raw|${raw}`);
      else if (key) lines.push(`${name}|${key}`);
    });

    if (!lines.length) return alert('No commands selected.');
    try {
      await this.client.importIrdb({ deviceId: devId, payload: lines.join('\n') });
      alert(`Imported ${lines.length} commands to device.`);
      await this.refreshData();
    } catch (err) {
      alert('Import failed: ' + err.message);
    }
  }

  async captureIrSignal() {
    const status = document.getElementById('wizardLearnStatus');
    const rawBox = document.getElementById('wizardRaw');
    const necBox = document.getElementById('wizardNec');
    const keyBox = document.getElementById('wizardKeycode');

    try {
      if (status) status.textContent = 'Listening for IR signal (press remote button)...';
      const res = await this.client.captureIr();
      if (res && res.raw) {
        if (rawBox) rawBox.value = res.raw;
        if (necBox && res.nec) necBox.value = res.nec;
        if (keyBox && res.keycode) keyBox.value = res.keycode;
        if (status) status.textContent = `Captured signal! Mode: ${res.mode || 'raw'}`;
      } else {
        if (status) status.textContent = 'No signal received or timed out.';
      }
    } catch (err) {
      if (status) status.textContent = 'Capture failed: ' + err.message;
    }
  }

  async testLearnedIrSignal() {
    const raw = document.getElementById('wizardRaw')?.value.trim();
    const nec = document.getElementById('wizardNec')?.value.trim();
    const keycode = document.getElementById('wizardKeycode')?.value.trim();
    const mode = document.getElementById('wizardMode')?.value || 'auto';
    const proto = document.getElementById('wizardProtocol')?.value || '2';
    const devId = document.getElementById('wizardDevice')?.value;
    const status = document.getElementById('wizardLearnStatus');

    try {
      if (status) status.textContent = 'Sending test signal...';
      await this.client.testLearnedIr({ deviceId: devId, raw, nec, keycode, mode, protocolId: proto });
      if (status) status.textContent = 'Test signal transmitted!';
    } catch (err) {
      if (status) status.textContent = 'Test failed: ' + err.message;
    }
  }

  async saveLearnedIrCommand() {
    const devId = document.getElementById('wizardDevice')?.value;
    const name = document.getElementById('wizardCommandName')?.value.trim();
    const raw = document.getElementById('wizardRaw')?.value.trim();
    const nec = document.getElementById('wizardNec')?.value.trim();
    const keycode = document.getElementById('wizardKeycode')?.value.trim();
    const mode = document.getElementById('wizardMode')?.value || 'auto';
    const protocol = document.getElementById('wizardProtocol')?.value || '2';
    const status = document.getElementById('wizardLearnStatus');

    if (!devId || !name) return alert('Device and command name are required.');

    try {
      if (status) status.textContent = 'Saving command...';
      await this.client.saveLearnedCommand({ deviceId: devId, name, raw, nec, keycode, mode, protocol });
      if (status) status.innerHTML = '<span style="color:var(--success)">✓ Command saved!</span>';
      await this.refreshData();
    } catch (err) {
      if (status) status.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
  }

  populateVerifyDropdowns() {
    const devSel = document.getElementById('verifyDevice');
    const cmdSel = document.getElementById('verifyCommand');
    if (!devSel || !cmdSel) return;
    const devs = this.inventory.devices || [];
    devSel.innerHTML = devs.map(d => `<option value="${d.id}">${d.name || d.id}</option>`).join('');

    const updateCmds = () => {
      const d = devs.find(x => String(x.id) === String(devSel.value));
      cmdSel.innerHTML = (d?.commands || []).map(c => `<option value="${c.name}">${c.name}</option>`).join('');
    };
    devSel.onchange = updateCmds;
    updateCmds();
  }

  async sendVerifyTestCommand() {
    const devId = document.getElementById('verifyDevice')?.value;
    const cmd = document.getElementById('verifyCommand')?.value;
    const status = document.getElementById('wizardVerifyStatus');
    if (!devId || !cmd) return alert('Select device and command first.');
    try {
      if (status) status.textContent = `Sending ${cmd}...`;
      await this.client.sendCommand(devId, cmd);
      if (status) status.textContent = `Sent ${cmd} successfully!`;
    } catch (err) {
      if (status) status.textContent = `Send failed: ${err.message}`;
    }
  }

  populateManageDevices() {
    const sel = document.getElementById('manageDeviceSelect');
    const editor = document.getElementById('storedDeviceEditor');
    if (!sel || !editor) return;

    const devs = this.inventory.devices || [];
    sel.innerHTML = devs.map(d => `<option value="${d.id}">${d.name || d.id}</option>`).join('');

    const render = () => {
      const dev = devs.find(d => String(d.id) === String(sel.value));
      if (!dev) { editor.innerHTML = ''; return; }
      const cp = dev.control_port > 0 ? dev.control_port : 7;

      editor.innerHTML = `
        <div class="card" style="margin-bottom: 12px;">
          <h3 style="font-size: 15px; margin-bottom: 8px;">Device Details</h3>
          <div class="row">
            <div><label>Name</label><input id="mgrDevName" class="form-control" value="${dev.name || ''}"></div>
            <div><label>Type</label><input id="mgrDevType" class="form-control" value="${dev.type || ''}"></div>
          </div>
          <div class="row" style="margin-top: 8px;">
            <div><label>Manufacturer</label><input id="mgrDevMan" class="form-control" value="${dev.manufacturer || ''}"></div>
            <div><label>Model</label><input id="mgrDevMod" class="form-control" value="${dev.model || ''}"></div>
          </div>
          <div style="margin-top: 10px;">
            <label>IR Blaster Assignment</label>
            <div style="display: flex; gap: 12px; flex-wrap: wrap;">
              <label style="display: flex; align-items: center; gap: 6px;"><input type="checkbox" id="mgrBlasterInternal" value="4" ${(cp & 4) ? 'checked' : ''}> Hub Internal</label>
              <label style="display: flex; align-items: center; gap: 6px;"><input type="checkbox" id="mgrBlasterP1" value="1" ${(cp & 1) ? 'checked' : ''}> Blaster Port 1</label>
              <label style="display: flex; align-items: center; gap: 6px;"><input type="checkbox" id="mgrBlasterP2" value="2" ${(cp & 2) ? 'checked' : ''}> Blaster Port 2</label>
            </div>
          </div>
          <div class="actions" style="margin-top: 12px; display: flex; justify-content: space-between;">
            <button type="button" id="btnMgrSaveDev" class="btn btn-sm btn-primary">Save Device Details</button>
            <button type="button" id="btnMgrDelDev" class="btn btn-sm btn-danger">Delete Device</button>
          </div>
        </div>

        <div class="card" style="margin-bottom: 12px;">
          <h3 style="font-size: 15px; margin-bottom: 8px;">Power Management</h3>
          <label style="display: flex; align-items: center; gap: 6px;">
            <input type="checkbox" id="mgrDevAlwaysOn" ${dev.is_power_always_on ? 'checked' : ''}>
            <span>Always On (leave powered between activities)</span>
          </label>
          <div style="margin-top: 8px;">
            <label>Warmup Delay (ms)</label>
            <input id="mgrDevPowerDelay" type="number" class="form-control" value="${dev.power_on_delay || 1500}">
          </div>
          <div class="actions" style="margin-top: 12px;">
            <button type="button" id="btnMgrSavePower" class="btn btn-sm btn-secondary">Save Power Settings</button>
          </div>
        </div>

        <div class="card" style="margin-bottom: 12px;">
          <h3 style="font-size: 15px; margin-bottom: 8px;">MQTT Connection</h3>
          <label style="display: flex; align-items: center; gap: 6px;">
            <input type="checkbox" id="mgrDevMqttEnabled" ${dev.mqtt_enabled ? 'checked' : ''}>
            <span>Publish button presses to MQTT</span>
          </label>
          <div class="row" style="margin-top: 8px;">
            <div><label>Topic Template</label><input id="mgrDevMqttTopic" class="form-control" value="${dev.mqtt_topic || '{root}/button/{device}/{command}'}"></div>
            <div><label>Pulse (ms)</label><input id="mgrDevMqttPulse" type="number" class="form-control" value="${dev.mqtt_pulse_ms || 1000}"></div>
          </div>
          <div class="actions" style="margin-top: 12px; display: flex; gap: 8px;">
            <button type="button" id="btnMgrSaveMqtt" class="btn btn-sm btn-secondary">Save MQTT</button>
            <button type="button" id="btnMgrTestMqtt" class="btn btn-sm btn-ghost">Test Pulse</button>
          </div>
        </div>

        <div class="card">
          <h3 style="font-size: 15px; margin-bottom: 8px;">Saved Commands (${(dev.commands || []).length})</h3>
          <div style="display: flex; flex-direction: column; gap: 6px; max-height: 240px; overflow-y: auto;">
            ${(dev.commands || []).map(c => `
              <div class="queue-row">
                <div>
                  <strong>${c.name}</strong>
                  <div class="muted mini">Proto ${c.protocol_id} ${c.learned ? '• Learned' : ''}</div>
                </div>
                <div style="display: flex; gap: 6px;">
                  <button type="button" class="btn btn-xs btn-primary btn-mgr-test-cmd" data-cmd="${c.name}">Send</button>
                  <button type="button" class="btn btn-xs btn-danger btn-mgr-del-cmd" data-cmd="${c.name}">Delete</button>
                </div>
              </div>
            `).join('')}
          </div>
        </div>
      `;

      editor.querySelector('#btnMgrSaveDev')?.addEventListener('click', async () => {
        let cpVal = 0;
        if (editor.querySelector('#mgrBlasterInternal')?.checked) cpVal |= 4;
        if (editor.querySelector('#mgrBlasterP1')?.checked) cpVal |= 1;
        if (editor.querySelector('#mgrBlasterP2')?.checked) cpVal |= 2;
        await this.client.saveDeviceDetails({
          deviceId: dev.id,
          name: editor.querySelector('#mgrDevName').value,
          type: editor.querySelector('#mgrDevType').value,
          manufacturer: editor.querySelector('#mgrDevMan').value,
          model: editor.querySelector('#mgrDevMod').value,
          controlPort: cpVal
        });
        alert('Device details saved.');
        await this.refreshData();
      });

      editor.querySelector('#btnMgrDelDev')?.addEventListener('click', async () => {
        if (!confirm(`Delete device ${dev.name}?`)) return;
        await this.client.deleteDevice(dev.id);
        alert('Device deleted.');
        await this.refreshData();
        this.populateManageDevices();
      });

      editor.querySelector('#btnMgrSavePower')?.addEventListener('click', async () => {
        await this.client.saveDevicePower({
          deviceId: dev.id,
          isPowerAlwaysOn: editor.querySelector('#mgrDevAlwaysOn').checked ? 1 : 0,
          powerOnDelay: Number(editor.querySelector('#mgrDevPowerDelay').value) || 1500
        });
        alert('Power settings saved.');
      });

      editor.querySelector('#btnMgrSaveMqtt')?.addEventListener('click', async () => {
        await this.client.saveDeviceMqtt({
          deviceId: dev.id,
          mqttEnabled: editor.querySelector('#mgrDevMqttEnabled').checked ? 1 : 0,
          mqttTopic: editor.querySelector('#mgrDevMqttTopic').value,
          mqttPulseMs: Number(editor.querySelector('#mgrDevMqttPulse').value) || 1000
        });
        alert('Device MQTT settings saved.');
      });

      editor.querySelector('#btnMgrTestMqtt')?.addEventListener('click', async () => {
        await this.client.testDeviceMqtt(dev.id);
        alert('Test MQTT pulse sent.');
      });

      editor.querySelectorAll('.btn-mgr-test-cmd').forEach(b => {
        b.addEventListener('click', () => this.client.sendCommand(dev.id, b.getAttribute('data-cmd')));
      });

      editor.querySelectorAll('.btn-mgr-del-cmd').forEach(b => {
        b.addEventListener('click', async () => {
          const cmd = b.getAttribute('data-cmd');
          if (!confirm(`Delete command ${cmd}?`)) return;
          await this.client.deleteCommand(dev.id, cmd);
          await this.refreshData();
          render();
        });
      });
    };

    sel.onchange = render;
    render();
  }

  /* -------------------------------------------------------------
   * 6. Bulk IR Test View (Lab)
   * ------------------------------------------------------------- */
  initLabView() {
    const sel = document.getElementById('labDevice');
    if (sel) {
      sel.innerHTML = '<option value="__auto_lab__">Temporary test device</option>' +
        (this.inventory.devices || []).map(d => `<option value="${d.id}">${d.name || d.id}</option>`).join('');
    }
  }

  async findLabCandidates() {
    const q = document.getElementById('labPathFilter')?.value.trim();
    const src = document.getElementById('labSource')?.value || 'all';
    const status = document.getElementById('labStatus');
    const box = document.getElementById('labCandidates');

    if (status) status.textContent = 'Searching candidate files...';
    try {
      const entries = await loadIrdSource(src === 'all' ? 'irdb' : src);
      const matches = rankIrdEntries(entries, q);
      if (status) status.textContent = `Found ${matches.length} matching files.`;
      if (box) {
        box.style.display = matches.length ? 'block' : 'none';
        box.innerHTML = matches.slice(0, 30).map(m => `
          <div class="queue-row" style="padding: 4px 8px;">
            <span>${m.path}</span>
            <span class="badge">${m.source}</span>
          </div>
        `).join('');
      }
    } catch (err) {
      if (status) status.textContent = 'Search failed: ' + err.message;
    }
  }

  async loadLabCommands() {
    const q = document.getElementById('labPathFilter')?.value.trim();
    const src = document.getElementById('labSource')?.value || 'all';
    const cmdFilter = (document.getElementById('labCommandFilter')?.value || '').toLowerCase();
    const status = document.getElementById('labStatus');

    if (status) status.textContent = 'Loading commands from files...';
    try {
      const entries = await loadIrdSource(src === 'all' ? 'irdb' : src);
      const matches = rankIrdEntries(entries, q).slice(0, 5);
      let allRows = [];
      for (const m of matches) {
        const text = await fetchIrdFile(m.path, m.source);
        const rows = parseIrText(text, m.source, m.path);
        allRows = allRows.concat(rows);
      }
      if (cmdFilter) {
        allRows = allRows.filter(r => r.name.toLowerCase().includes(cmdFilter));
      }
      this.labQueue = allRows.slice(0, 300);
      this.renderLabQueue();
      if (status) status.textContent = `Loaded ${this.labQueue.length} commands into queue.`;
    } catch (err) {
      if (status) status.textContent = 'Failed to load: ' + err.message;
    }
  }

  useStoredLabCommands() {
    const devId = document.getElementById('labDevice')?.value;
    const dev = (this.inventory.devices || []).find(d => String(d.id) === String(devId));
    if (!dev) return alert('Select a stored device.');
    this.labQueue = (dev.commands || []).map(c => ({
      name: c.name,
      meta: 'Stored command',
      keycode: c.keycode,
      raw: c.raw,
      stored: true
    }));
    this.renderLabQueue();
  }

  renderLabQueue() {
    const box = document.getElementById('labQueue');
    const sum = document.getElementById('labSummary');
    if (sum) sum.textContent = `${this.labQueue.length} queued`;
    if (!box) return;
    if (!this.labQueue.length) {
      box.innerHTML = '<div class="muted mini" style="padding: 8px;">No commands queued.</div>';
      return;
    }
    box.innerHTML = this.labQueue.map((c, i) => `
      <div class="queue-row" style="padding: 4px 8px;">
        <label style="display: flex; align-items: center; gap: 8px; flex: 1; cursor: pointer; margin: 0;">
          <input type="checkbox" class="lab-pick" data-index="${i}" checked>
          <span><strong>${c.name}</strong> <span class="muted mini">(${c.meta})</span></span>
        </label>
        <span class="badge ${c.raw ? 'ok' : 'primary'}">${c.raw ? 'Raw' : 'Code'}</span>
      </div>
    `).join('');
  }

  setLabSelections(check) {
    document.querySelectorAll('.lab-pick').forEach(cb => { cb.checked = check; });
  }

  dropUncheckedLab() {
    const picks = new Set();
    document.querySelectorAll('.lab-pick:checked').forEach(cb => {
      picks.add(Number(cb.getAttribute('data-index')));
    });
    this.labQueue = this.labQueue.filter((_, i) => picks.has(i));
    this.renderLabQueue();
  }

  clearLabQueue() {
    this.labQueue = [];
    this.renderLabQueue();
  }

  async importLabSelected() {
    const devId = document.getElementById('labDevice')?.value;
    if (!devId || devId === '__auto_lab__') return alert('Please select a real stored device to save into.');
    const lines = [];
    document.querySelectorAll('.lab-pick:checked').forEach(cb => {
      const idx = Number(cb.getAttribute('data-index'));
      const c = this.labQueue[idx];
      if (c.raw) lines.push(`${c.name}|raw|${c.raw}`);
      else if (c.keycode) lines.push(`${c.name}|${c.keycode}`);
    });
    if (!lines.length) return alert('No commands selected.');
    try {
      await this.client.importIrdb({ deviceId: devId, payload: lines.join('\n') });
      alert(`Imported ${lines.length} commands.`);
      await this.refreshData();
    } catch (err) {
      alert('Import failed: ' + err.message);
    }
  }

  async runLabQueue() {
    if (this.labRunning) return;
    const devId = document.getElementById('labDevice')?.value;
    const delay = Number(document.getElementById('labSendDelay')?.value) || 80;
    const dry = document.getElementById('labDryRun')?.checked;
    const log = document.getElementById('labLog');
    const meter = document.getElementById('labMeter');

    const selected = [];
    document.querySelectorAll('.lab-pick:checked').forEach(cb => {
      const idx = Number(cb.getAttribute('data-index'));
      selected.push(this.labQueue[idx]);
    });

    if (!selected.length) return alert('No commands selected to run.');
    this.labRunning = true;
    this.labRunId = Date.now().toString();

    let done = 0;
    for (const cmd of selected) {
      if (!this.labRunning) break;
      done++;
      if (meter) meter.style.width = `${Math.round((done / selected.length) * 100)}%`;
      if (log) log.textContent = `[${done}/${selected.length}] Sending ${cmd.name}...`;
      if (!dry) {
        try {
          await this.client.sendCommand(devId, cmd.name);
        } catch (e) {
          if (log) log.textContent += ` (error: ${e.message})`;
        }
      }
      await new Promise(r => setTimeout(r, delay));
    }

    this.labRunning = false;
    if (log) log.textContent = 'Bulk test completed.';
  }

  stopLabQueue() {
    this.labRunning = false;
    if (this.labRunId) this.client.cancelBatchIr(this.labRunId);
  }

  /* -------------------------------------------------------------
   * 7. Bluetooth View
   * ------------------------------------------------------------- */
  async loadBtView() {
    const badge = document.getElementById('btStatusBadge');
    const desc = document.getElementById('btHostDesc');
    const list = document.getElementById('btDevicesList');
    const hostSel = document.getElementById('btTargetHostSelect');
    const saveSel = document.getElementById('btSaveDeviceSelect');
    const pairTarget = document.getElementById('btPairTargetDev');

    if (pairTarget) {
      pairTarget.innerHTML = '<option value="">-- Generic Bluetooth Host --</option>' +
        (this.inventory.devices || []).map(d => `<option value="${d.id}">${d.name || d.id}</option>`).join('');
    }
    if (saveSel) {
      saveSel.innerHTML = (this.inventory.devices || []).map(d => `<option value="${d.id}">${d.name || d.id}</option>`).join('');
    }

    try {
      const st = await this.client.getBtStatus();
      if (badge) {
        badge.className = `badge ${st.running ? 'ok' : 'bad'}`;
        badge.textContent = st.running ? 'BTstack Running' : 'Stopped';
      }
      if (desc) desc.textContent = st.running ? 'Daemon active • ready for pairing' : 'Daemon stopped';

      const devs = st.devices || [];
      if (list) {
        list.innerHTML = devs.length ? devs.map(d => `
          <div class="queue-row">
            <div>
              <strong>${d.name || 'Unknown Device'}</strong>
              <div class="muted mini">${d.bdaddr || d.address} • Connected</div>
            </div>
            <button type="button" class="btn btn-xs btn-danger btn-bt-disconn" data-addr="${d.bdaddr}">Disconnect</button>
          </div>
        `).join('') : '<div class="muted mini">No active connections.</div>';

        list.querySelectorAll('.btn-bt-disconn').forEach(b => {
          b.addEventListener('click', async () => {
            await this.client.disconnectBt(b.getAttribute('data-addr'));
            this.loadBtView();
          });
        });
      }

      if (hostSel) {
        hostSel.innerHTML = '<option value="">Auto (Active Host)</option>' +
          devs.map(d => `<option value="${d.bdaddr}">${d.name || d.bdaddr}</option>`).join('');
      }
    } catch (err) {
      if (desc) desc.textContent = 'Error: ' + err.message;
    }
  }

  async toggleBtPairing() {
    const btn = document.getElementById('btPairToggleBtn');
    const name = document.getElementById('btPairName')?.value || 'Harmony Keyboard';
    const target = document.getElementById('btPairTargetDev')?.value || '';
    try {
      if (btn) btn.textContent = 'Entering pairing mode...';
      await this.client.setBtPairing(true, name, target);
      alert('Hub is now discoverable as "' + name + '". Pair from your host device.');
      this.loadBtView();
    } catch (err) {
      alert('Pairing mode failed: ' + err.message);
    } finally {
      if (btn) btn.textContent = 'Start Pairing Mode';
    }
  }

  async sendBtDirectText() {
    const txt = document.getElementById('btDirectText')?.value;
    const stat = document.getElementById('btTextStatus');
    if (!txt) return;
    try {
      if (stat) stat.textContent = 'Sending text...';
      await this.client.sendBtText(txt);
      if (stat) stat.innerHTML = '<span style="color:var(--success)">Sent!</span>';
    } catch (err) {
      if (stat) stat.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
  }

  async runBtMacroScript() {
    const script = document.getElementById('btMacroScript')?.value;
    const delay = Number(document.getElementById('btScriptDelay')?.value) || 35;
    const stat = document.getElementById('btScriptStatus');
    if (!script) return;
    try {
      if (stat) stat.textContent = 'Running macro script...';
      await this.client.runBtScript(script, delay);
      if (stat) stat.innerHTML = '<span style="color:var(--success)">Script finished!</span>';
    } catch (err) {
      if (stat) stat.innerHTML = `<span style="color:var(--danger)">Failed: ${err.message}</span>`;
    }
  }

  async saveBtMacroCommand() {
    const devId = document.getElementById('btSaveDeviceSelect')?.value;
    const name = document.getElementById('btSaveCommandName')?.value.trim();
    const script = document.getElementById('btMacroScript')?.value.trim();
    const stat = document.getElementById('btSaveCommandStatus');
    if (!devId || !name || !script) return alert('Device, command name, and script content are required.');
    try {
      await this.client.saveBtCommand(devId, name, script);
      if (stat) stat.innerHTML = '<span style="color:var(--success)">Saved!</span>';
      await this.refreshData();
    } catch (err) {
      if (stat) stat.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
  }

  /* -------------------------------------------------------------
   * 8. BT Remote View (Homatics B25)
   * ------------------------------------------------------------- */
  async loadRemotesView() {
    const sel = document.getElementById('b25ActivitySelect');
    if (sel) {
      sel.innerHTML = '<option value="-1">Off / Idle (Default)</option>' +
        (this.activitiesData.activities || []).map(a => `<option value="${a.id}">${a.name || a.label}</option>`).join('');
    }
    const tgtDev = document.getElementById('b25TargetDevice');
    const presetDev = document.getElementById('b25PresetBtDevice');
    const devs = this.inventory.devices || [];
    if (tgtDev) tgtDev.innerHTML = devs.map(d => `<option value="${d.id}">${d.name || d.id}</option>`).join('');
    if (presetDev) presetDev.innerHTML = devs.map(d => `<option value="${d.id}">${d.name || d.id}</option>`).join('');

    const actSel = document.getElementById('b25TargetActivity');
    if (actSel) {
      actSel.innerHTML = (this.activitiesData.activities || []).map(a => `<option value="${a.id}">${a.name || a.label}</option>`).join('');
    }

    try {
      const m = await this.client.getRemoteMappings();
      if (m && m.activities) this.b25MapData = m;
    } catch {}

    this.selectB25Button('power');
    this.updateB25ButtonStates();
  }

  selectB25Button(btnId) {
    this.b25SelectedBtn = btnId;
    document.querySelectorAll('.b25-hotspot').forEach(b => {
      b.classList.toggle('selected', b.getAttribute('data-btn') === btnId);
    });
    const lbl = document.getElementById('b25CurrentBtnName');
    if (lbl) lbl.textContent = btnId;

    const actId = document.getElementById('b25ActivitySelect')?.value || '-1';
    const mapping = this.b25MapData?.activities?.[actId]?.[btnId];

    const typeSel = document.getElementById('b25ActionType');
    if (typeSel) {
      if (!mapping) typeSel.value = 'unmapped';
      else if (mapping.type) typeSel.value = mapping.type;
      else if (mapping.command) typeSel.value = 'device_cmd';
      this.toggleB25ActionFields();
    }

    if (mapping && mapping.deviceId) {
      const devSel = document.getElementById('b25TargetDevice');
      if (devSel) devSel.value = mapping.deviceId;
    }
    this.updateB25CommandDropdown(mapping?.command);
  }

  updateB25CommandDropdown(curVal) {
    const devId = document.getElementById('b25TargetDevice')?.value;
    const cmdSel = document.getElementById('b25TargetCommand');
    if (!cmdSel) return;
    const dev = (this.inventory.devices || []).find(d => String(d.id) === String(devId));
    cmdSel.innerHTML = (dev?.commands || []).map(c => `<option value="${c.name}" ${c.name === curVal ? 'selected' : ''}>${c.name}</option>`).join('');
  }

  toggleB25ActionFields() {
    const t = document.getElementById('b25ActionType')?.value;
    const wDev = document.getElementById('b25WrapTargetDevice');
    const wCmd = document.getElementById('b25WrapTargetCommand');
    const wAct = document.getElementById('b25WrapTargetActivity');

    if (wDev) wDev.style.display = t === 'device_cmd' ? 'block' : 'none';
    if (wCmd) wCmd.style.display = t === 'device_cmd' ? 'block' : 'none';
    if (wAct) wAct.style.display = t === 'activity_start' ? 'block' : 'none';
  }

  applyB25Mapping() {
    const actId = document.getElementById('b25ActivitySelect')?.value || '-1';
    const type = document.getElementById('b25ActionType')?.value;
    if (!this.b25MapData.activities) this.b25MapData.activities = {};
    if (!this.b25MapData.activities[actId]) this.b25MapData.activities[actId] = {};

    if (type === 'unmapped') {
      delete this.b25MapData.activities[actId][this.b25SelectedBtn];
    } else if (type === 'device_cmd') {
      this.b25MapData.activities[actId][this.b25SelectedBtn] = {
        type: 'device_cmd',
        deviceId: document.getElementById('b25TargetDevice')?.value,
        command: document.getElementById('b25TargetCommand')?.value
      };
    } else if (type === 'activity_start') {
      this.b25MapData.activities[actId][this.b25SelectedBtn] = {
        type: 'activity_start',
        activityId: document.getElementById('b25TargetActivity')?.value
      };
    } else if (type === 'activity_stop') {
      this.b25MapData.activities[actId][this.b25SelectedBtn] = {
        type: 'activity_stop'
      };
    }
    this.updateB25ButtonStates();
  }

  clearB25Mapping() {
    const actId = document.getElementById('b25ActivitySelect')?.value || '-1';
    if (this.b25MapData.activities?.[actId]) {
      delete this.b25MapData.activities[actId][this.b25SelectedBtn];
    }
    this.selectB25Button(this.b25SelectedBtn);
    this.updateB25ButtonStates();
  }

  updateB25ButtonStates() {
    const actId = document.getElementById('b25ActivitySelect')?.value || '-1';
    const actMap = this.b25MapData?.activities?.[actId] || {};
    document.querySelectorAll('.b25-hotspot').forEach(b => {
      const id = b.getAttribute('data-btn');
      b.classList.toggle('mapped', Boolean(actMap[id]));
    });
  }

  async saveAllB25Mappings() {
    const btn = document.getElementById('b25SaveAllBtn');
    try {
      if (btn) btn.textContent = 'Saving...';
      await this.client.saveRemoteMappings(this.b25MapData);
      alert('All Homatics B25 button mappings saved to hub!');
    } catch (err) {
      alert('Save failed: ' + err.message);
    } finally {
      if (btn) btn.textContent = 'Save All Mappings';
    }
  }

  autoMapB25Preset() {
    const devId = document.getElementById('b25PresetBtDevice')?.value;
    const actId = document.getElementById('b25ActivitySelect')?.value || '-1';
    if (!devId) return alert('Select target device first.');
    const dev = (this.inventory.devices || []).find(d => String(d.id) === String(devId));
    if (!dev || !dev.commands) return alert('Device has no commands.');

    const norm = s => String(s || '').toLowerCase().replace(/[^a-z0-9]/g, '');
    const aliases = {
      power: ['power', 'poweroff', 'standby', 'powertoggle'],
      up: ['up', 'directionup'],
      down: ['down', 'directiondown'],
      left: ['left', 'directionleft'],
      right: ['right', 'directionright'],
      select: ['ok', 'select', 'enter'],
      back: ['back', 'return', 'exit'],
      home: ['home'],
      vol_up: ['volumeup', 'volup'],
      vol_down: ['volumedown', 'voldown'],
      mute: ['mute']
    };

    if (!this.b25MapData.activities) this.b25MapData.activities = {};
    if (!this.b25MapData.activities[actId]) this.b25MapData.activities[actId] = {};

    let mapped = 0;
    Object.entries(aliases).forEach(([btn, list]) => {
      for (const a of list) {
        const cmd = dev.commands.find(c => norm(c.name) === norm(a));
        if (cmd) {
          this.b25MapData.activities[actId][btn] = { type: 'device_cmd', deviceId: devId, command: cmd.name };
          mapped++;
          break;
        }
      }
    });

    this.updateB25ButtonStates();
    alert(`Auto-mapped ${mapped} buttons for device! Click "Save All Mappings" to persist.`);
  }

  async scanB25Remote() {
    const btn = document.getElementById('b25ScanBtn');
    const log = document.getElementById('b25PairLog');
    try {
      if (btn) btn.textContent = 'Scanning...';
      if (log) log.textContent = 'Scanning for BLE remotes (10s)...';
      await this.client.scanBtRemote();
      setTimeout(async () => {
        const st = await this.client.getBtRemotePairStatus();
        if (log) log.textContent = st.message || 'Scan completed.';
        const list = document.getElementById('b25DiscoveredList');
        if (list && st.devices) {
          list.innerHTML = st.devices.map(d => `<option value="${d.bdaddr}">${d.name || d.bdaddr}</option>`).join('');
        }
      }, 4000);
    } catch (err) {
      if (log) log.textContent = 'Scan error: ' + err.message;
    } finally {
      if (btn) btn.textContent = 'Scan for Remote';
    }
  }

  async pairB25Remote() {
    const addr = document.getElementById('b25DiscoveredList')?.value;
    const log = document.getElementById('b25PairLog');
    if (!addr) return alert('Select discovered remote first.');
    try {
      if (log) log.textContent = `Pairing with ${addr}...`;
      await this.client.pairBtRemote(addr);
      if (log) log.textContent = `Paired with ${addr}!`;
    } catch (err) {
      if (log) log.textContent = 'Pairing failed: ' + err.message;
    }
  }

  /* -------------------------------------------------------------
   * 9. MQTT View
   * ------------------------------------------------------------- */
  async loadMqttView() {
    const badge = document.getElementById('mqttConnBadge');
    try {
      const st = await this.client.getMqttStatus();
      if (badge) {
        badge.className = `badge ${st.connected ? 'ok' : 'bad'}`;
        badge.textContent = st.connected ? 'Connected' : 'Disconnected';
      }
      if (st.config) {
        document.getElementById('mqttEnabled').checked = Boolean(st.config.enabled);
        document.getElementById('mqttHost').value = st.config.host || '';
        document.getElementById('mqttPort').value = st.config.port || 1883;
        document.getElementById('mqttUsername').value = st.config.username || '';
        document.getElementById('mqttBaseTopic').value = st.config.baseTopic || 'harmony/hub';
        document.getElementById('mqttDiscoveryPrefix').value = st.config.discoveryPrefix || 'homeassistant';
        document.getElementById('mqttClientId').value = st.config.clientId || 'harmony_hub';
        document.getElementById('mqttName').value = st.config.name || 'Harmony Hub';
        document.getElementById('mqttPollSeconds').value = st.config.pollSeconds || 10;
        document.getElementById('mqttKeepAlive').value = st.config.keepAlive || 60;
        document.getElementById('mqttHaDiscovery').checked = Boolean(st.config.haDiscovery);
        document.getElementById('mqttBtEvents').checked = Boolean(st.config.btRemoteEvents);
      }
    } catch (err) {
      if (badge) badge.textContent = 'Offline';
    }
  }

  async saveMqttSettings() {
    const status = document.getElementById('mqttSaveStatus');
    const data = {
      enabled: document.getElementById('mqttEnabled')?.checked ? 1 : 0,
      host: document.getElementById('mqttHost')?.value.trim(),
      port: Number(document.getElementById('mqttPort')?.value) || 1883,
      username: document.getElementById('mqttUsername')?.value.trim(),
      password: document.getElementById('mqttPassword')?.value,
      keep_password: document.getElementById('mqttKeepPass')?.checked ? 1 : 0,
      baseTopic: document.getElementById('mqttBaseTopic')?.value.trim(),
      discoveryPrefix: document.getElementById('mqttDiscoveryPrefix')?.value.trim(),
      clientId: document.getElementById('mqttClientId')?.value.trim(),
      name: document.getElementById('mqttName')?.value.trim(),
      pollSeconds: Number(document.getElementById('mqttPollSeconds')?.value) || 10,
      keepAlive: Number(document.getElementById('mqttKeepAlive')?.value) || 60,
      haDiscovery: document.getElementById('mqttHaDiscovery')?.checked ? 1 : 0,
      btRemoteEvents: document.getElementById('mqttBtEvents')?.checked ? 1 : 0
    };

    try {
      if (status) status.textContent = 'Saving MQTT settings...';
      await this.client.saveMqtt(data);
      if (status) status.innerHTML = '<span style="color:var(--success)">Saved!</span>';
    } catch (err) {
      if (status) status.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
  }

  /* -------------------------------------------------------------
   * 10. Network View (Wi-Fi & Ethernet)
   * ------------------------------------------------------------- */
  async loadNetworkView() {
    try {
      const st = await this.client.getNetworkStatus();
      document.getElementById('netConnType').textContent = st.connection_type || 'Unknown';
      document.getElementById('netActiveIf').textContent = st.active_interface || 'none';
      document.getElementById('netIp').textContent = st.ip || 'None';
      document.getElementById('netMask').textContent = st.netmask || '-';
      document.getElementById('netGw').textContent = st.gateway || '-';
      document.getElementById('netMac').textContent = st.mac || '-';
      document.getElementById('netUsbMode').textContent = st.usb_host_mode ? 'Host Mode (Ethernet)' : 'Gadget Mode (USB Console/PC)';
      document.getElementById('netEthStatus').textContent = st.eth_present ? (st.eth_carrier ? 'Connected (Link Up)' : 'Cable Unplugged') : 'Not Detected';
      document.getElementById('netWifiStatus').textContent = st.wifi_connected ? 'Connected' : 'Disconnected';
    } catch (err) {
      console.warn('Network status error:', err);
    }
  }

  async saveEthernetConfig(applyMode = 'save') {
    const status = document.getElementById('ethSaveStatus');
    const data = {
      apply: applyMode,
      eth_enabled: document.getElementById('ethEnabledCheck')?.checked ? 1 : 0,
      eth_fallback_wifi: document.getElementById('ethFallbackCheck')?.checked ? 1 : 0,
      eth_usb_serial: 1,
      eth_mode: document.getElementById('ethModeSelect')?.value || 'dhcp',
      eth_ip: document.getElementById('ethIp')?.value.trim(),
      eth_netmask: document.getElementById('ethNetmask')?.value.trim(),
      eth_gateway: document.getElementById('ethGateway')?.value.trim(),
      eth_dns: document.getElementById('ethDns')?.value.trim()
    };
    try {
      if (status) status.textContent = 'Saving Ethernet...';
      await this.client.saveEthernet(data);
      if (status) status.innerHTML = '<span style="color:var(--success)">Saved!</span>';
    } catch (err) {
      if (status) status.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
  }

  async saveWifiConfig(applyMode = 'save') {
    const status = document.getElementById('wifiSaveStatus');
    const data = {
      apply: applyMode,
      ssid: document.getElementById('wifiSsidInput')?.value.trim(),
      password: document.getElementById('wifiPskInput')?.value,
      keep_password: document.getElementById('wifiKeepPass')?.checked ? 1 : 0,
      hidden: document.getElementById('wifiHidden')?.checked ? 1 : 0,
      open: document.getElementById('wifiOpen')?.checked ? 1 : 0
    };
    try {
      if (status) status.textContent = 'Saving Wi-Fi...';
      await this.client.saveWifi(data);
      if (status) status.innerHTML = '<span style="color:var(--success)">Saved!</span>';
    } catch (err) {
      if (status) status.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
  }

  /* -------------------------------------------------------------
   * 11. Backup & Restore View
   * ------------------------------------------------------------- */
  loadBackupView() {}

  async exportBackupItem(target) {
    try {
      const text = await this.client.fetchExportText(target);
      const box = document.getElementById('backupPayload');
      if (box) box.value = typeof text === 'string' ? text : JSON.stringify(text, null, 2);
      const tSel = document.getElementById('backupTarget');
      if (tSel) tSel.value = target;
      alert(`Exported "${target}" to the Backup Contents box below.`);
    } catch (err) {
      alert(`Export failed: ${err.message}`);
    }
  }

  async restoreBackupPayload() {
    const target = document.getElementById('backupTarget')?.value;
    const payload = document.getElementById('backupPayload')?.value.trim();
    const status = document.getElementById('backupImportStatus');
    if (!payload) return alert('Enter backup payload to restore.');
    if (!confirm(`Restore ${target} from payload?`)) return;
    try {
      if (status) status.textContent = 'Restoring...';
      await this.client.restoreBackup(target, payload);
      if (status) status.innerHTML = '<span style="color:var(--success)">Restored!</span>';
      alert('Restore complete. Reboot hub if necessary.');
      await this.refreshData();
    } catch (err) {
      if (status) status.innerHTML = `<span style="color:var(--danger)">Restore failed: ${err.message}</span>`;
    }
  }

  /* -------------------------------------------------------------
   * 12. System Management View
   * ------------------------------------------------------------- */
  async loadSystemView() {
    const infoEl = document.getElementById('sysInfoPre');
    const logsEl = document.getElementById('sysLogsPre');
    const netEl = document.getElementById('sysNetInfoPre');

    try {
      const html = await this.client.getWebUiHtml();
      if (typeof html === 'string') {
        const mInfo = html.match(/<summary>System information<\/summary><pre>([\s\S]*?)<\/pre>/i);
        const mLogs = html.match(/<summary>Logs<\/summary><pre>([\s\S]*?)<\/pre>/i);
        const mNet = html.match(/<summary>Network details<\/summary><pre>([\s\S]*?)<\/pre>/i);
        if (infoEl) infoEl.textContent = mInfo ? mInfo[1] : 'Information retrieved.';
        if (logsEl) logsEl.textContent = mLogs ? mLogs[1] : 'Logs retrieved.';
        if (netEl) netEl.textContent = mNet ? mNet[1] : 'Network details retrieved.';
      }
    } catch (err) {
      if (infoEl) infoEl.textContent = 'Failed to load system details: ' + err.message;
    }
  }

  async saveDebugLogging() {
    const status = document.getElementById('sysDebugStatus');
    const on = document.getElementById('sysDebugCheck')?.checked ? 1 : 0;
    try {
      await this.client.saveSystemAction('debug_logging', { debugLogging: on });
      if (status) status.innerHTML = '<span style="color:var(--success)">Saved!</span>';
    } catch (err) {
      if (status) status.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
  }

  async saveWebUiAuth() {
    const status = document.getElementById('authSaveStatus');
    const on = document.getElementById('authEnabledCheck')?.checked ? 1 : 0;
    const u = document.getElementById('authUsername')?.value.trim();
    const p = document.getElementById('authPassword')?.value;
    try {
      await this.client.saveSystemAction('auth', { authEnabled: on, authUsername: u, authPassword: p });
      if (status) status.innerHTML = '<span style="color:var(--success)">Sign-in setting saved!</span>';
    } catch (err) {
      if (status) status.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
  }

  async checkSoftwareUpdate() {
    const log = document.getElementById('updateLog');
    try {
      if (log) log.textContent = 'Checking GitHub releases for updates...';
      const res = await this.client.checkUpdate(true);
      if (log) log.textContent = res.message || 'Update check completed.';
    } catch (err) {
      if (log) log.textContent = 'Update check failed: ' + err.message;
    }
  }

  async installSoftwareUpdate() {
    const repo = document.getElementById('updateRepo')?.value.trim();
    const token = document.getElementById('updateToken')?.value.trim();
    const log = document.getElementById('updateLog');
    if (!confirm('Download and apply update? Hub services will restart.')) return;
    try {
      if (log) log.textContent = 'Applying software update...';
      const res = await this.client.applyUpdate(token, repo);
      if (log) log.textContent = res.message || 'Update applied successfully!';
    } catch (err) {
      if (log) log.textContent = 'Install failed: ' + err.message;
    }
  }

  async rediscoverMqtt() {
    const status = document.getElementById('sysActionStatus');
    try {
      await this.client.saveSystemAction('rediscover');
      if (status) status.innerHTML = '<span style="color:var(--success)">Discovery triggered!</span>';
    } catch (err) {
      if (status) status.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
  }

  async rebootHub() {
    if (!confirm('Are you sure you want to reboot the Harmony Hub?')) return;
    try {
      await this.client.rebootHub();
      alert('Reboot command sent to hub. Reconnecting in 30 seconds...');
    } catch (err) {
      alert('Reboot failed: ' + err.message);
    }
  }

  /* -------------------------------------------------------------
   * 13. Saved Hubs View (Multi-Hub Manager)
   * ------------------------------------------------------------- */
  async renderHubsList() {
    const list = document.getElementById('hubsList');
    if (!list) return;

    const hubs = await getSavedHubs();
    if (!hubs.length) {
      list.innerHTML = '<div class="muted" style="padding: 16px;">No saved hubs yet. Click "+ Add Hub".</div>';
      return;
    }

    list.innerHTML = hubs.map(h => {
      const isCur = this.activeHub && this.activeHub.id === h.id;
      return `
        <div class="card" style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
          <div>
            <div style="display: flex; align-items: center; gap: 8px;">
              <strong>${h.name}</strong>
              ${isCur ? '<span class="badge ok">Active</span>' : ''}
            </div>
            <div class="muted mini">${h.host}:${h.port || 8080}</div>
          </div>
          <div style="display: flex; gap: 6px;">
            ${!isCur ? `<button type="button" class="btn btn-xs btn-primary btn-select-hub" data-id="${h.id}">Connect</button>` : ''}
            <button type="button" class="btn btn-xs btn-secondary btn-edit-hub" data-id="${h.id}">Edit</button>
            <button type="button" class="btn btn-xs btn-danger btn-delete-hub" data-id="${h.id}">✕</button>
          </div>
        </div>
      `;
    }).join('');

    list.querySelectorAll('.btn-select-hub').forEach(b => {
      b.addEventListener('click', async () => {
        await setActiveHubId(b.getAttribute('data-id'));
        await this.loadActiveHub();
        this.renderHubsList();
      });
    });

    list.querySelectorAll('.btn-edit-hub').forEach(b => {
      b.addEventListener('click', () => {
        const h = hubs.find(x => x.id === b.getAttribute('data-id'));
        if (h) this.openEditHubModal(h);
      });
    });

    list.querySelectorAll('.btn-delete-hub').forEach(b => {
      b.addEventListener('click', async () => {
        const id = b.getAttribute('data-id');
        if (confirm('Delete this hub profile?')) {
          await deleteHub(id);
          await this.loadActiveHub();
          this.renderHubsList();
        }
      });
    });
  }

  openAddHubModal() {
    this.openEditHubModal({ name: 'Harmony Hub', host: '192.168.1.50', port: 8080, wsPort: 8089 }, false);
  }

  openEditHubModal(h, isEdit = true) {
    let modal = document.getElementById('hubModal');
    if (!modal) {
      modal = document.createElement('div');
      modal.id = 'hubModal';
      modal.style.cssText = 'position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.7);z-index:2000;display:flex;align-items:center;justify-content:center;padding:16px;';
      document.body.appendChild(modal);
    }

    modal.innerHTML = `
      <div class="card" style="width:100%;max-width:380px;">
        <h3 style="font-size:16px;margin-bottom:12px;">${isEdit ? 'Edit Hub Profile' : 'Add New Harmony Hub'}</h3>
        <label>Hub Name</label><input id="hubFormName" class="form-control" value="${h.name || ''}">
        <label>IP Address / Host</label><input id="hubFormHost" class="form-control" value="${h.host || ''}">
        <label>HTTP Port</label><input id="hubFormPort" type="number" class="form-control" value="${h.port || 8080}">
        <label>Username (Optional)</label><input id="hubFormUser" class="form-control" value="${h.username || ''}">
        <label>Password (Optional)</label><input id="hubFormPass" type="password" class="form-control" value="${h.password || ''}">
        <div id="hubTestStatus" class="subtle" style="margin-top:8px;"></div>
        <div class="actions" style="margin-top:14px;display:flex;justify-content:space-between;">
          <div style="display:flex;gap:6px;">
            <button type="button" id="btnHubSave" class="btn btn-sm btn-primary">Save Hub</button>
            <button type="button" id="btnHubTest" class="btn btn-sm btn-secondary">Test</button>
          </div>
          <button type="button" id="btnHubCancel" class="btn btn-sm btn-ghost">Cancel</button>
        </div>
      </div>
    `;

    modal.style.display = 'flex';
    const close = () => { modal.style.display = 'none'; };
    modal.querySelector('#btnHubCancel').onclick = close;

    modal.querySelector('#btnHubTest').onclick = async () => {
      const stat = modal.querySelector('#hubTestStatus');
      stat.textContent = 'Testing connection...';
      const c = new HarmonyClient({
        host: modal.querySelector('#hubFormHost').value.trim(),
        port: Number(modal.querySelector('#hubFormPort').value) || 8080,
        username: modal.querySelector('#hubFormUser').value.trim(),
        password: modal.querySelector('#hubFormPass').value.trim()
      });
      const res = await c.testConnection();
      if (res.ok) stat.innerHTML = '<span style="color:var(--success)">✓ Connected!</span>';
      else stat.innerHTML = `<span style="color:var(--danger)">✗ ${res.error}</span>`;
    };

    modal.querySelector('#btnHubSave').onclick = async () => {
      h.name = modal.querySelector('#hubFormName').value.trim() || 'Harmony Hub';
      h.host = modal.querySelector('#hubFormHost').value.trim() || '127.0.0.1';
      h.port = Number(modal.querySelector('#hubFormPort').value) || 8080;
      h.username = modal.querySelector('#hubFormUser').value.trim();
      h.password = modal.querySelector('#hubFormPass').value.trim();
      await saveHub(h);
      await setActiveHubId(h.id);
      await this.loadActiveHub();
      close();
      this.renderHubsList();
    };
  }
}

document.addEventListener('DOMContentLoaded', () => {
  const app = new HarmonyApp();
  app.init();
});
