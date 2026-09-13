import { Haptics, ImpactStyle } from '@capacitor/haptics';
import { getRemoteLayout, saveRemoteLayout, resetRemoteLayout } from './storage.js';

export class RemoteEngine {
  constructor(client) {
    this.client = client;
    this.editMode = false;
    this.currentSubTab = 'remote'; // 'remote' | 'commands' | 'keypad'
    this.currentScope = null; // 'activity' | 'device'
    this.currentTarget = null;
    this.availableDevices = [];
    this.deviceCommandsMap = {};
    this.activeLayout = null;
    this.onSendCallback = null;
    this.container = null;
    this.cmdSearchFilter = '';
    this.selectedKeypadDeviceId = null;
  }

  async triggerHaptic() {
    try {
      await Haptics.impact({ style: ImpactStyle.Light });
    } catch {
      // Browser fallback - ignore
    }
  }

  getNumberPatterns() {
    return [
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
      'number0', 'number1', 'number2', 'number3', 'number4', 'number5', 'number6', 'number7', 'number8', 'number9'
    ];
  }

  getDevicesWithNumbers() {
    const patterns = this.getNumberPatterns();
    return this.availableDevices.filter(dev => {
      const cmds = this.deviceCommandsMap[dev.id] || [];
      return cmds.some(c => {
        const name = String(c.name || c).toLowerCase().replace(/[\s_\-]/g, '');
        return patterns.includes(name) || /^(num|number|digit)?[0-9]$/i.test(name);
      });
    });
  }

  hasNumberCommands() {
    return this.getDevicesWithNumbers().length > 0;
  }

  hasBluetoothConnection() {
    for (const dev of this.availableDevices) {
      if (Number(dev.transport) === 32 || dev.isBt) return true;
      const cmds = this.deviceCommandsMap[dev.id] || [];
      for (const c of cmds) {
        if (Number(c.protocolId) === 999997 || (c.keycode && String(c.keycode).startsWith('G:HID'))) {
          return true;
        }
      }
    }
    return false;
  }

  getBluetoothDevice() {
    return this.availableDevices.find(dev => {
      if (Number(dev.transport) === 32 || dev.isBt) return true;
      const cmds = this.deviceCommandsMap[dev.id] || [];
      return cmds.some(c => Number(c.protocolId) === 999997 || (c.keycode && String(c.keycode).startsWith('G:HID')));
    }) || this.availableDevices[0] || this.currentTarget;
  }

  normalizeCmdName(name) {
    return String(name || '')
      .toLowerCase()
      .replace(/\+/g, 'up')
      .replace(/-/g, 'down')
      .replace(/[^a-z0-9]/g, '');
  }

  findCmdInDevice(devId, patterns) {
    if (!devId) return null;
    const cmds = this.deviceCommandsMap[devId] || [];
    if (!Array.isArray(cmds) || cmds.length === 0) return null;

    for (const pat of patterns) {
      const normPat = this.normalizeCmdName(pat);
      for (const c of cmds) {
        const rawName = String(c.name || c);
        const normName = this.normalizeCmdName(rawName);
        if (normName === normPat) {
          return rawName;
        }
      }
    }
    return null;
  }

  findCmd(devIdOrList, patterns) {
    if (!devIdOrList) return null;
    if (Array.isArray(devIdOrList)) {
      for (const dev of devIdOrList) {
        const id = typeof dev === 'object' ? dev.id : dev;
        const found = this.findCmdInDevice(id, patterns);
        if (found) return found;
      }
      return null;
    }
    const id = typeof devIdOrList === 'object' ? devIdOrList.id : devIdOrList;
    return this.findCmdInDevice(id, patterns);
  }

  findBestCmd(candidateDevices, patterns) {
    if (!candidateDevices || !Array.isArray(candidateDevices)) return null;
    for (const dev of candidateDevices) {
      if (!dev) continue;
      const devId = typeof dev === 'object' ? dev.id : dev;
      const cmd = this.findCmdInDevice(devId, patterns);
      if (cmd) {
        return { deviceId: devId, command: cmd };
      }
    }
    return null;
  }

  classifyDevices(devices) {
    const isDisplay = (d) => {
      const t = String(d.type || '').toLowerCase();
      const n = String(d.name || '').toLowerCase();
      const m = String(d.model || '').toLowerCase();
      return t.includes('tv') || t.includes('television') || t.includes('projector') || t.includes('display') ||
             n.includes('tv') || n.includes('television') || n.includes('projector') || n.includes('samsung') || n.includes('lg') || n.includes('sony') ||
             m.includes('tv') || m.includes('oled');
    };

    const isAudio = (d) => {
      const t = String(d.type || '').toLowerCase();
      const n = String(d.name || '').toLowerCase();
      const m = String(d.model || '').toLowerCase();
      return t.includes('receiver') || t.includes('stereo') || t.includes('soundbar') || t.includes('audio') || t.includes('amplifier') || t.includes('avr') ||
             n.includes('denon') || n.includes('receiver') || n.includes('soundbar') || n.includes('yamaha') || n.includes('sonos') || n.includes('marantz') || n.includes('onkyo') || n.includes('bose') || n.includes('avr') ||
             m.includes('avr');
    };

    const isMediaSource = (d) => {
      const t = String(d.type || '').toLowerCase();
      const n = String(d.name || '').toLowerCase();
      const m = String(d.model || '').toLowerCase();
      return t.includes('streaming') || t.includes('game') || t.includes('cable') || t.includes('satellite') || t.includes('settop') || t.includes('media') || t.includes('dvd') || t.includes('bluray') || t.includes('cd') || t.includes('tuner') ||
             n.includes('apple') || n.includes('roku') || n.includes('shield') || n.includes('fire') || n.includes('chromecast') || n.includes('kodi') || n.includes('playstation') || n.includes('xbox') || n.includes('nintendo') || n.includes('tivo') || n.includes('bluray') ||
             m.includes('apple') || m.includes('roku');
    };

    const displayDevs = devices.filter(isDisplay);
    const audioDevs = devices.filter(isAudio);
    const mediaDevs = devices.filter(isMediaSource);

    return { displayDevs, audioDevs, mediaDevs };
  }

  isButtonDeviceValid(btn) {
    if (!btn || !btn.deviceId) return false;
    if (this.currentScope === 'device') {
      return String(btn.deviceId) === String(this.currentTarget?.id);
    }
    if (this.currentScope === 'activity') {
      return this.availableDevices.some(d => String(d.id) === String(btn.deviceId));
    }
    return true;
  }

  isButtonCommandValid(btn) {
    if (!btn || !btn.deviceId || !btn.command) return false;
    const cmds = this.deviceCommandsMap[btn.deviceId];
    if (Array.isArray(cmds) && cmds.length > 0) {
      const targetNorm = this.normalizeCmdName(btn.command);
      return cmds.some(c => this.normalizeCmdName(c.name || c) === targetNorm);
    }
    return false;
  }

  isButtonValid(btn) {
    return this.isButtonDeviceValid(btn) && this.isButtonCommandValid(btn);
  }

  sanitizeGrid(grid) {
    if (!Array.isArray(grid)) return { sanitized: new Array(40).fill(null), changed: true };
    let changed = false;
    const sanitized = grid.map(btn => {
      if (!btn) return null;
      if (!this.isButtonValid(btn)) {
        changed = true;
        return null;
      }
      return btn;
    });
    return { sanitized, changed };
  }

  generateDefaultLayout(scope, target, availableDevices, commandsMap) {
    // 5 columns x 8 rows = 40 slots (indices 0..39)
    const grid = new Array(40).fill(null);

    if (!availableDevices || availableDevices.length === 0) {
      return {
        scope,
        targetId: target?.id,
        targetName: target?.name || target?.label,
        grid
      };
    }

    const assignedKeys = new Set();

    const setSlot = (row, col, devId, command, label, variant = '') => {
      if (!devId || !command) return false;
      const idx = row * 5 + col;
      if (idx < 0 || idx >= 40) return false;
      grid[idx] = {
        id: `btn_${row}_${col}`,
        deviceId: devId,
        command,
        label: label || command,
        variant
      };
      assignedKeys.add(`${devId}:${command}`);
      return true;
    };

    const placeCmd = (row, col, candidates, patterns, label, variant = '', allowDuplicate = false) => {
      const match = this.findBestCmd(candidates, patterns);
      if (!match) return false;
      if (!allowDuplicate && assignedKeys.has(`${match.deviceId}:${match.command}`)) {
        return false;
      }
      return setSlot(row, col, match.deviceId, match.command, label, variant);
    };

    // Candidate devices resolution
    let uniqueNav = [];
    let uniqueAudio = [];
    let uniqueDisplay = [];

    if (scope === 'device') {
      uniqueNav = [target];
      uniqueAudio = [target];
      uniqueDisplay = [target];
    } else {
      const { displayDevs, audioDevs, mediaDevs } = this.classifyDevices(availableDevices);
      const navOrder = [...mediaDevs, ...displayDevs, ...audioDevs, ...availableDevices];
      uniqueNav = navOrder.filter((d, i, arr) => arr.findIndex(x => String(x.id) === String(d.id)) === i);

      const audioOrder = [...audioDevs, ...displayDevs, ...availableDevices];
      uniqueAudio = audioOrder.filter((d, i, arr) => arr.findIndex(x => String(x.id) === String(d.id)) === i);

      const displayOrder = [...displayDevs, ...audioDevs, ...mediaDevs, ...availableDevices];
      uniqueDisplay = displayOrder.filter((d, i, arr) => arr.findIndex(x => String(x.id) === String(d.id)) === i);
    }

    const PATTERNS = {
      power: ['powertoggle', 'power', 'poweron', 'pwr', 'standby', 'standbytoggle', 'systempower'],
      powerOff: ['poweroff', 'systempoweroff', 'off'],
      input: ['inputnext', 'input', 'source', 'inputsource', 'inputhdmi1', 'hdmi'],
      home: ['home', 'smart', 'hub', 'mainmenu', 'topmenu', 'rootmenu'],
      menu: ['menu', 'settings', 'setup', 'options', 'tools', 'actionmenu', 'preferences'],
      exit: ['exit', 'clear', 'cancel', 'close'],
      back: ['back', 'return', 'exit', 'escape', 'esc', 'goback', 'backspace'],
      info: ['info', 'display', 'status', 'osd', 'displayinfo', 'information'],
      guide: ['guide', 'epg', 'programguide', 'livetv', 'tvguide'],

      up: ['directionup', 'cursorup', 'arrowup', 'navup', 'up', 'uparrow', 'cursor_up'],
      down: ['directiondown', 'cursordown', 'arrowdown', 'navdown', 'down', 'downarrow', 'cursor_down'],
      left: ['directionleft', 'cursorleft', 'arrowleft', 'navleft', 'left', 'leftarrow', 'cursor_left'],
      right: ['directionright', 'cursorright', 'arrowright', 'navright', 'right', 'rightarrow', 'cursor_right'],
      select: ['select', 'ok', 'enter', 'confirm', 'center', 'selectenter', 'cursorenter'],

      volUp: ['volumeup', 'volup', 'volumeplus', 'volplus', 'volume+'],
      volDown: ['volumedown', 'voldown', 'volumeminus', 'volminus', 'volume-'],
      mute: ['mute', 'volumemute', 'mutetoggle', 'soundmute'],

      chUp: ['channelup', 'chup', 'channelplus', 'chplus', 'pageup', 'pgup', 'nextchannel'],
      chDown: ['channeldown', 'chdown', 'channelminus', 'chminus', 'pagedown', 'pgdown', 'previouschannel', 'prevchannel'],
      prevCh: ['prevchannel', 'previouschannel', 'lastchannel', 'last', 'prev', 'recall', 'jump'],

      rewind: ['rewind', 'rev', 'scanrev', 'skipback', 'previoustrack', 'skipbackward'],
      play: ['play', 'playpause'],
      pause: ['pause', 'playpause'],
      stop: ['stop'],
      fastForward: ['fastforward', 'ff', 'forward', 'ffwd', 'scanfwd', 'skipforward', 'nexttrack'],
      replay: ['replay', 'jumpback', 'instantreplay', 'quickskip', 'skipback'],
      skipFwd: ['skipforward', 'nexttrack', 'jumpforward', 'advance', 'skip'],
      record: ['record', 'rec'],

      sub: ['subtitle', 'subtitles', 'cc', 'closedcaption'],
      audio: ['audio', 'soundtrack', 'audiotrack', 'sound', 'surround', 'soundmode'],
      popup: ['popupmenu', 'popup', 'options', 'tools', 'quickmenu'],
      dvr: ['dvr', 'list', 'recordedtv', 'recordings'],

      red: ['red', 'colorred', 'buttonred'],
      green: ['green', 'colorgreen', 'buttongreen'],
      yellow: ['yellow', 'coloryellow', 'buttonyellow'],
      blue: ['blue', 'colorblue', 'buttonblue']
    };

    // Row 0: Top Controls - Power in corner, Input, Home, Guide, Info/PowerOff
    placeCmd(0, 0, uniqueDisplay, PATTERNS.power, 'PWR', 'danger');
    placeCmd(0, 1, uniqueDisplay, PATTERNS.input, 'INPUT');
    placeCmd(0, 2, uniqueNav, PATTERNS.home, 'HOME');
    placeCmd(0, 3, uniqueDisplay, PATTERNS.guide, 'GUIDE');
    if (!placeCmd(0, 4, uniqueDisplay, PATTERNS.powerOff, 'OFF', 'danger')) {
      placeCmd(0, 4, uniqueDisplay, PATTERNS.info, 'INFO');
    }

    // Row 1: Menus, Back, D-Pad Up (▲), Exit, Info/Guide
    placeCmd(1, 0, uniqueNav, PATTERNS.menu, 'MENU');
    placeCmd(1, 1, uniqueNav, PATTERNS.back, 'BACK');
    placeCmd(1, 2, uniqueNav, PATTERNS.up, '▲');
    placeCmd(1, 3, uniqueNav, PATTERNS.exit, 'EXIT');
    if (!grid[4]) {
      placeCmd(1, 4, uniqueDisplay, PATTERNS.info, 'INFO');
    }

    // Row 2: Vol+, D-Pad Left (◀), D-Pad OK (OK), D-Pad Right (▶), Ch+
    placeCmd(2, 0, uniqueAudio, PATTERNS.volUp, 'VOL +');
    placeCmd(2, 1, uniqueNav, PATTERNS.left, '◀');
    placeCmd(2, 2, uniqueNav, PATTERNS.select, 'OK', 'primary');
    placeCmd(2, 3, uniqueNav, PATTERNS.right, '▶');
    placeCmd(2, 4, uniqueDisplay, PATTERNS.chUp, 'CH +');

    // Row 3: Vol-, Replay, D-Pad Down (▼), Skip Fwd, Ch-
    placeCmd(3, 0, uniqueAudio, PATTERNS.volDown, 'VOL -');
    placeCmd(3, 1, uniqueNav, PATTERNS.replay, 'REPLAY');
    placeCmd(3, 2, uniqueNav, PATTERNS.down, '▼');
    placeCmd(3, 3, uniqueNav, PATTERNS.skipFwd, 'SKIP');
    placeCmd(3, 4, uniqueDisplay, PATTERNS.chDown, 'CH -');

    // Row 4: Mute, Audio, PopUp/Options, Subtitle, Prev Channel
    placeCmd(4, 0, uniqueAudio, PATTERNS.mute, 'MUTE', 'warning');
    placeCmd(4, 1, uniqueAudio, PATTERNS.audio, 'AUDIO');
    placeCmd(4, 2, uniqueNav, PATTERNS.popup, 'OPT');
    placeCmd(4, 3, uniqueNav, PATTERNS.sub, 'SUB');
    placeCmd(4, 4, uniqueDisplay, PATTERNS.prevCh, 'PREV');

    // Row 5: Media Controls: Rewind, Play, Pause, Stop, Fast Forward
    placeCmd(5, 0, uniqueNav, PATTERNS.rewind, 'REW');
    placeCmd(5, 1, uniqueNav, PATTERNS.play, 'PLAY', 'accent');
    placeCmd(5, 2, uniqueNav, PATTERNS.pause, 'PAUSE');
    placeCmd(5, 3, uniqueNav, PATTERNS.stop, 'STOP');
    placeCmd(5, 4, uniqueNav, PATTERNS.fastForward, 'FF');

    // Row 6: Record, Secondary Media / DVR
    placeCmd(6, 0, uniqueNav, PATTERNS.record, 'REC', 'danger');
    placeCmd(6, 4, uniqueNav, PATTERNS.dvr, 'DVR');

    // Row 7: Color buttons
    placeCmd(7, 0, uniqueDisplay, PATTERNS.red, 'RED', 'danger');
    placeCmd(7, 1, uniqueDisplay, PATTERNS.green, 'GREEN', 'accent');
    placeCmd(7, 2, uniqueDisplay, PATTERNS.yellow, 'YELLOW', 'warning');
    placeCmd(7, 3, uniqueDisplay, PATTERNS.blue, 'BLUE', 'primary');

    // Fallback pass: For devices with remaining unassigned commands, fill empty slots
    if (scope === 'device' && target) {
      const devCmds = this.deviceCommandsMap[target.id] || [];
      const unassigned = devCmds.filter(c => {
        const rawName = String(c.name || c);
        return !assignedKeys.has(`${target.id}:${rawName}`) &&
               !/^(number|digit)?[0-9]$/i.test(rawName);
      });

      const fillSlots = [31, 32, 33, 39, 21, 22, 23, 16, 18];
      let uIdx = 0;
      for (const slotIdx of fillSlots) {
        if (!grid[slotIdx] && uIdx < unassigned.length) {
          const c = unassigned[uIdx++];
          const rawName = String(c.name || c);
          const r = Math.floor(slotIdx / 5);
          const col = slotIdx % 5;
          setSlot(r, col, target.id, rawName, rawName);
        }
      }
    }

    return {
      scope,
      targetId: target.id,
      targetName: target.name || target.label,
      grid
    };
  }

  async loadLayout(scope, target, availableDevices, commandsMap) {
    let saved = await getRemoteLayout(scope, target.id);
    if (!saved || !Array.isArray(saved.grid) || saved.grid.length !== 40) {
      saved = this.generateDefaultLayout(scope, target, availableDevices, commandsMap);
      await saveRemoteLayout(scope, target.id, saved);
    } else {
      const { sanitized, changed } = this.sanitizeGrid(saved.grid);
      if (changed) {
        saved.grid = sanitized;
        await saveRemoteLayout(scope, target.id, saved);
      }
    }
    return saved;
  }

  async render(container, { scope, target, availableDevices, commandsMap, onSendCommand }) {
    if (this.currentTarget?.id !== target?.id || this.currentScope !== scope) {
      this.selectedKeypadDeviceId = null;
    }

    this.currentScope = scope;
    this.currentTarget = target;
    this.availableDevices = availableDevices || [];
    this.deviceCommandsMap = commandsMap || {};
    this.onSendCallback = onSendCommand;
    this.container = container;

    // Reset keypad tab if numbers not available
    if (this.currentSubTab === 'keypad' && !this.hasNumberCommands()) {
      this.currentSubTab = 'remote';
    }

    // Reset keyboard tab if bluetooth not available
    if (this.currentSubTab === 'keyboard' && !this.hasBluetoothConnection()) {
      this.currentSubTab = 'remote';
    }

    this.activeLayout = await this.loadLayout(scope, target, this.availableDevices, this.deviceCommandsMap);
    this.updateDOM();
  }

  toggleEditMode() {
    this.editMode = !this.editMode;
    this.updateDOM();
  }

  async resetCurrentLayout() {
    const targetName = this.currentTarget?.name || this.currentTarget?.label || (this.currentScope === 'activity' ? 'Activity' : 'Device');
    const msg = `Reset remote for "${targetName}" to default layout based on available buttons?\n\nThis will organize available buttons into D-Pad, Volume, Power in the corner, Back/Home, and Media controls.`;
    if (!confirm(msg)) return;
    await resetRemoteLayout(this.currentScope, this.currentTarget.id);
    this.activeLayout = this.generateDefaultLayout(
      this.currentScope,
      this.currentTarget,
      this.availableDevices,
      this.deviceCommandsMap
    );
    await saveRemoteLayout(this.currentScope, this.currentTarget.id, this.activeLayout);
    await this.triggerHaptic();
    this.updateDOM();
  }

  updateDOM() {
    if (!this.container || !this.activeLayout) return;
    this.container.innerHTML = '';

    const root = document.createElement('div');
    root.className = 'remote-canvas';

    const hasNumbers = this.hasNumberCommands();
    const hasBt = this.hasBluetoothConnection();

    // Sub-Tabs Header Bar
    const subtabsBar = document.createElement('div');
    subtabsBar.className = 'remote-subtabs-bar';
    subtabsBar.innerHTML = `
      <div class="remote-subtabs-group">
        <button type="button" class="subtab-btn ${this.currentSubTab === 'remote' ? 'active' : ''}" data-subtab="remote">
          Remote
        </button>
        <button type="button" class="subtab-btn ${this.currentSubTab === 'commands' ? 'active' : ''}" data-subtab="commands" title="All Commands">
          Commands
        </button>
        ${hasNumbers ? `
        <button type="button" class="subtab-btn ${this.currentSubTab === 'keypad' ? 'active' : ''}" data-subtab="keypad">
          Keypad
        </button>
        ` : ''}
        ${hasBt ? `
        <button type="button" class="subtab-btn ${this.currentSubTab === 'keyboard' ? 'active' : ''}" data-subtab="keyboard">
          Keyboard
        </button>
        ` : ''}
      </div>
      <div class="remote-subtabs-actions">
        ${this.currentSubTab === 'remote' ? `
          ${this.editMode ? `
          <button type="button" class="subtab-reset-btn" id="btnResetLayout" title="Reset remote to default state based on available buttons">
            <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8"></path>
              <path d="M3 3v5h5"></path>
            </svg>
            <span>Reset</span>
          </button>
          ` : ''}
          <button type="button" class="subtab-pencil-btn ${this.editMode ? 'active' : ''}" id="btnToggleEdit" title="${this.editMode ? 'Finish Editing' : 'Customize Remote Buttons'}">
            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M12 20h9"></path>
              <path d="M16.5 3.5a2.121 2.121 0 0 1 3 3L7 19l-4 1 1-4L16.5 3.5z"></path>
            </svg>
          </button>
        ` : ''}
      </div>
    `;
    root.appendChild(subtabsBar);

    // Bind sub-tabs events
    subtabsBar.querySelectorAll('.subtab-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        this.currentSubTab = btn.getAttribute('data-subtab');
        this.updateDOM();
      });
    });

    const activeTabEl = subtabsBar.querySelector('.subtab-btn.active');
    if (activeTabEl && typeof activeTabEl.scrollIntoView === 'function') {
      activeTabEl.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'nearest' });
    }

    const editBtn = subtabsBar.querySelector('#btnToggleEdit');
    if (editBtn) {
      editBtn.addEventListener('click', () => this.toggleEditMode());
    }

    const resetBtn = subtabsBar.querySelector('#btnResetLayout');
    if (resetBtn) {
      resetBtn.addEventListener('click', () => this.resetCurrentLayout());
    }

    // View Content based on active subtab
    if (this.currentSubTab === 'remote') {
      root.appendChild(this.renderRemoteGrid());
    } else if (this.currentSubTab === 'commands') {
      root.appendChild(this.renderAllCommandsView());
    } else if (this.currentSubTab === 'keypad') {
      root.appendChild(this.renderKeypadView());
    } else if (this.currentSubTab === 'keyboard') {
      root.appendChild(this.renderKeyboardView());
    }

    this.container.appendChild(root);
  }

  renderRemoteGrid() {
    const gridContainer = document.createElement('div');
    gridContainer.className = `remote-grid-5x8 ${this.editMode ? 'edit-mode' : ''}`;

    const grid = this.activeLayout.grid || [];

    for (let i = 0; i < 40; i++) {
      const btn = grid[i];
      if (btn && this.isButtonValid(btn)) {
        gridContainer.appendChild(this.createGridButtonEl(btn, i));
      } else {
        const slotEl = document.createElement('div');
        slotEl.className = `grid-slot-empty ${this.editMode ? 'clickable' : ''}`;
        if (this.editMode) {
          slotEl.innerHTML = `<span class="slot-add-icon">+</span>`;
          slotEl.title = `Slot ${i + 1}: Tap to add button`;
          slotEl.addEventListener('click', () => {
            this.openSlotEditorModal(null, i);
          });
        }
        gridContainer.appendChild(slotEl);
      }
    }

    return gridContainer;
  }

  createGridButtonEl(btn, slotIndex) {
    const el = document.createElement('button');
    el.type = 'button';
    el.className = `remote-btn ${btn.variant ? 'btn-' + btn.variant : ''} ${this.editMode ? 'in-edit' : ''}`;
    el.setAttribute('data-slot', slotIndex);

    el.innerHTML = `
      <span class="btn-label">${btn.label || btn.command || 'Btn'}</span>
      ${this.editMode ? `<span class="btn-edit-badge">✎</span>` : ''}
    `;

    el.addEventListener('click', (e) => {
      e.stopPropagation();
      this.triggerHaptic();
      if (this.editMode) {
        this.openSlotEditorModal(btn, slotIndex);
      } else {
        if (btn.deviceId && btn.command && this.onSendCallback) {
          el.classList.add('active-press');
          setTimeout(() => el.classList.remove('active-press'), 150);
          this.onSendCallback(btn.deviceId, btn.command);
        }
      }
    });

    return el;
  }

  renderAllCommandsView() {
    const view = document.createElement('div');
    view.className = 'all-commands-panel';

    const searchInput = document.createElement('input');
    searchInput.type = 'text';
    searchInput.className = 'form-control cmd-search-input';
    searchInput.placeholder = 'Search commands...';
    searchInput.value = this.cmdSearchFilter;
    view.appendChild(searchInput);

    const listContainer = document.createElement('div');
    listContainer.className = 'commands-category-list';
    view.appendChild(listContainer);

    const targetDevices = this.currentScope === 'device'
      ? (this.currentTarget ? [this.currentTarget] : [])
      : this.availableDevices;

    const renderList = (filterText) => {
      listContainer.innerHTML = '';
      const q = filterText.trim().toLowerCase();

      targetDevices.forEach(dev => {
        const rawCmds = this.deviceCommandsMap[dev.id] || [];
        const filteredCmds = rawCmds.filter(c => {
          const name = String(c.name || c).toLowerCase();
          return !q || name.includes(q);
        });

        if (filteredCmds.length === 0 && q) return;

        const devSec = document.createElement('div');
        devSec.className = 'cmd-dev-section';

        if (targetDevices.length > 1) {
          const title = document.createElement('div');
          title.className = 'cmd-dev-header';
          title.textContent = `${dev.name || dev.label} (${filteredCmds.length})`;
          devSec.appendChild(title);
        }

        const chipsGrid = document.createElement('div');
        chipsGrid.className = 'cmds-grid';

        filteredCmds.forEach(c => {
          const cmdName = c.name || c;
          const chip = document.createElement('button');
          chip.type = 'button';
          chip.className = 'btn btn-xs btn-ghost cmd-chip';
          chip.textContent = cmdName;
          chip.addEventListener('click', async () => {
            await this.triggerHaptic();
            chip.classList.add('active-press');
            setTimeout(() => chip.classList.remove('active-press'), 150);
            if (this.onSendCallback) {
              this.onSendCallback(dev.id, cmdName);
            }
          });
          chipsGrid.appendChild(chip);
        });

        devSec.appendChild(chipsGrid);
        listContainer.appendChild(devSec);
      });

      if (listContainer.children.length === 0) {
        listContainer.innerHTML = '<div class="muted text-center p-2 mini">No matching commands found.</div>';
      }
    };

    renderList(this.cmdSearchFilter);

    searchInput.addEventListener('input', (e) => {
      this.cmdSearchFilter = e.target.value;
      renderList(this.cmdSearchFilter);
    });

    return view;
  }

  renderKeypadView() {
    const view = document.createElement('div');
    view.className = 'keypad-panel';

    const numDevs = this.getDevicesWithNumbers();
    if (numDevs.length === 0) {
      view.innerHTML = '<div class="muted text-center p-2 mini">No number keypad commands found.</div>';
      return view;
    }

    // Default to first device in activity based on order
    if (!this.selectedKeypadDeviceId || !numDevs.some(d => String(d.id) === String(this.selectedKeypadDeviceId))) {
      this.selectedKeypadDeviceId = numDevs[0].id;
    }

    const currentDev = numDevs.find(d => String(d.id) === String(this.selectedKeypadDeviceId)) || numDevs[0];
    const cmds = this.deviceCommandsMap[currentDev.id] || [];

    const findNumCmd = (num) => {
      const match = cmds.find(c => {
        const n = String(c.name || c).toLowerCase().replace(/[\s_\-]/g, '');
        return n === String(num) || n === `number${num}` || n === `num${num}` || n === `digit${num}`;
      });
      return match ? (match.name || match) : String(num);
    };

    if (numDevs.length > 1) {
      const selectBar = document.createElement('div');
      selectBar.className = 'keypad-select-bar';
      selectBar.innerHTML = `
        <label class="keypad-select-label">Target Device</label>
        <select class="form-control keypad-dev-select">
          ${numDevs.map(d => `<option value="${d.id}" ${String(d.id) === String(currentDev.id) ? 'selected' : ''}>${d.name || d.label} (${d.model || d.id})</option>`).join('')}
        </select>
      `;

      const selectEl = selectBar.querySelector('select');
      selectEl.addEventListener('change', () => {
        this.selectedKeypadDeviceId = selectEl.value;
        this.updateDOM();
      });

      view.appendChild(selectBar);
    }

    const keys = [
      '1', '2', '3',
      '4', '5', '6',
      '7', '8', '9',
      '', '0', ''
    ];

    const grid = document.createElement('div');
    grid.className = 'keypad-grid';

    keys.forEach(k => {
      if (k === '') {
        const spacer = document.createElement('div');
        spacer.className = 'btn-spacer';
        grid.appendChild(spacer);
      } else {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = 'remote-btn num-key-btn';
        btn.textContent = k;
        btn.addEventListener('click', async () => {
          await this.triggerHaptic();
          btn.classList.add('active-press');
          setTimeout(() => btn.classList.remove('active-press'), 150);
          if (currentDev && this.onSendCallback) {
            const actualCmd = findNumCmd(k);
            this.onSendCallback(currentDev.id, actualCmd);
          }
        });
        grid.appendChild(btn);
      }
    });

    view.appendChild(grid);
    return view;
  }

  renderKeyboardView() {
    const view = document.createElement('div');
    view.className = 'keyboard-panel';

    const btDev = this.getBluetoothDevice();
    const devName = btDev ? (btDev.name || btDev.label || 'Bluetooth Device') : 'Bluetooth';

    view.innerHTML = `
      <div class="keyboard-header">
        <div class="keyboard-device-badge">
          <span class="bt-dot"></span>
          <span>Target: <strong>${devName}</strong></span>
        </div>
      </div>

      <div class="keyboard-card">
        <form class="keyboard-form" id="kbForm">
          <div class="keyboard-input-wrap">
            <input type="text" id="kbTextInput" class="form-control keyboard-input" placeholder="Type text to send to TV..." autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false">
            <button type="submit" class="btn btn-primary keyboard-send-btn" id="kbSendBtn">Send</button>
          </div>
          <div class="keyboard-hint mini muted">Press Send or Enter on mobile keyboard to send text block.</div>
          <div class="keyboard-feedback" id="kbFeedback" style="display: none;"></div>
        </form>
      </div>

      <div class="keyboard-quick-section">
        <div class="keyboard-section-title">Quick Keys</div>
        <div class="keyboard-quick-grid">
          <button type="button" class="remote-btn kb-action-btn" data-key="enter">
            <span>⏎</span>
            <span class="mini">Enter</span>
          </button>
          <button type="button" class="remote-btn kb-action-btn" data-key="backspace">
            <span>⌫</span>
            <span class="mini">Backspace</span>
          </button>
          <button type="button" class="remote-btn kb-action-btn" data-key="space">
            <span>␣</span>
            <span class="mini">Space</span>
          </button>
          <button type="button" class="remote-btn kb-action-btn" data-key="back">
            <span>✕</span>
            <span class="mini">Back</span>
          </button>
        </div>
      </div>

      <div class="keyboard-nav-section">
        <div class="keyboard-section-title">Navigation</div>
        <div class="keyboard-dpad-grid">
          <div class="btn-spacer"></div>
          <button type="button" class="remote-btn kb-dpad-btn" data-nav="up">▲</button>
          <div class="btn-spacer"></div>
          <button type="button" class="remote-btn kb-dpad-btn" data-nav="left">◀</button>
          <button type="button" class="remote-btn btn-primary kb-dpad-btn" data-nav="ok">OK</button>
          <button type="button" class="remote-btn kb-dpad-btn" data-nav="right">▶</button>
          <div class="btn-spacer"></div>
          <button type="button" class="remote-btn kb-dpad-btn" data-nav="down">▼</button>
          <div class="btn-spacer"></div>
        </div>
      </div>
    `;

    const form = view.querySelector('#kbForm');
    const input = view.querySelector('#kbTextInput');
    const sendBtn = view.querySelector('#kbSendBtn');
    const feedback = view.querySelector('#kbFeedback');

    const showFeedback = (msg, isErr = false) => {
      feedback.style.display = 'block';
      feedback.className = `keyboard-feedback ${isErr ? 'text-danger' : 'text-success'}`;
      feedback.textContent = msg;
      setTimeout(() => {
        if (feedback) feedback.style.display = 'none';
      }, 3000);
    };

    const sendText = async (text) => {
      if (!text) return;
      await this.triggerHaptic();
      sendBtn.disabled = true;
      try {
        if (this.client && typeof this.client.sendBtText === 'function') {
          const res = await this.client.sendBtText(text);
          if (res && res.error) {
            showFeedback(`Error: ${res.error}`, true);
          } else {
            showFeedback(`✓ Sent "${text}"`);
            input.value = '';
          }
        }
      } catch (err) {
        showFeedback(`Failed: ${err.message || err}`, true);
      } finally {
        sendBtn.disabled = false;
        input.focus();
      }
    };

    form.addEventListener('submit', (e) => {
      e.preventDefault();
      sendText(input.value);
    });

    view.querySelectorAll('.kb-action-btn').forEach(btn => {
      btn.addEventListener('click', async () => {
        const key = btn.getAttribute('data-key');
        await this.triggerHaptic();
        btn.classList.add('active-press');
        setTimeout(() => btn.classList.remove('active-press'), 150);

        if (key === 'enter') {
          try {
            if (this.client) await this.client.sendBtText('\n');
          } catch {
            const devId = btDev ? btDev.id : null;
            const cmd = this.findCmd(devId, ['Select', 'OK', 'Enter']);
            if (cmd && this.onSendCallback) this.onSendCallback(devId, cmd);
          }
        } else if (key === 'space') {
          try {
            if (this.client) await this.client.sendBtText(' ');
          } catch {
            const devId = btDev ? btDev.id : null;
            const cmd = this.findCmd(devId, ['Space']);
            if (cmd && this.onSendCallback) this.onSendCallback(devId, cmd);
          }
        } else if (key === 'backspace') {
          try {
            if (this.client) await this.client.sendBtText('\b');
          } catch {
            const devId = btDev ? btDev.id : null;
            const cmd = this.findCmd(devId, ['Back', 'Return', 'Clear']);
            if (cmd && this.onSendCallback) this.onSendCallback(devId, cmd);
          }
        } else if (key === 'back') {
          const devId = btDev ? btDev.id : null;
          const cmd = this.findCmd(devId, ['Back', 'Return', 'Exit']);
          if (cmd && this.onSendCallback) this.onSendCallback(devId, cmd);
        }
      });
    });

    view.querySelectorAll('.kb-dpad-btn').forEach(btn => {
      btn.addEventListener('click', async () => {
        const nav = btn.getAttribute('data-nav');
        await this.triggerHaptic();
        btn.classList.add('active-press');
        setTimeout(() => btn.classList.remove('active-press'), 150);

        const devId = btDev ? btDev.id : null;
        let cmd = null;
        if (nav === 'up') cmd = this.findCmd(devId, ['DirectionUp', 'Up']);
        else if (nav === 'down') cmd = this.findCmd(devId, ['DirectionDown', 'Down']);
        else if (nav === 'left') cmd = this.findCmd(devId, ['DirectionLeft', 'Left']);
        else if (nav === 'right') cmd = this.findCmd(devId, ['DirectionRight', 'Right']);
        else if (nav === 'ok') cmd = this.findCmd(devId, ['Select', 'OK', 'Enter']);

        if (cmd && this.onSendCallback) {
          this.onSendCallback(devId, cmd);
        }
      });
    });

    return view;
  }

  openSlotEditorModal(btn, slotIndex) {
    let modal = document.getElementById('buttonEditorModal');
    if (!modal) {
      modal = document.createElement('div');
      modal.id = 'buttonEditorModal';
      modal.className = 'modal-backdrop';
      document.body.appendChild(modal);
    }

    const isEdit = !!btn && this.isButtonValid(btn);
    const allowedDevices = this.currentScope === 'device'
      ? (this.currentTarget ? [this.currentTarget] : [])
      : this.availableDevices;

    let initialDevId = isEdit ? btn.deviceId : (allowedDevices[0] ? allowedDevices[0].id : '');
    if (!allowedDevices.some(d => String(d.id) === String(initialDevId))) {
      initialDevId = allowedDevices[0] ? allowedDevices[0].id : '';
    }

    const devOptions = allowedDevices.map(d =>
      `<option value="${d.id}" ${String(d.id) === String(initialDevId) ? 'selected' : ''}>${d.name || d.label} (${d.model || d.id})</option>`
    ).join('');

    const cmds = this.deviceCommandsMap[initialDevId] || [];
    const currentCmd = isEdit ? btn.command : '';
    const cmdOptions = cmds.map(c => {
      const name = c.name || c;
      return `<option value="${name}" ${name === currentCmd ? 'selected' : ''}>${name}</option>`;
    }).join('');

    const row = Math.floor(slotIndex / 5) + 1;
    const col = (slotIndex % 5) + 1;

    modal.innerHTML = `
      <div class="modal-card">
        <div class="modal-header">
          <h3>${isEdit ? 'Customize Button' : 'Add Button'} (Row ${row}, Col ${col})</h3>
          <button type="button" class="btn btn-ghost btn-sm" id="btnModalClose">✕</button>
        </div>
        <div class="modal-body">
          <div class="form-group">
            <label>Target Device ${this.currentScope === 'device' ? '<span class="mini muted">(Current Device)</span>' : ''}</label>
            <select id="editBtnDevice" class="form-control" ${this.currentScope === 'device' ? 'disabled' : ''}>
              ${devOptions || '<option value="">(No devices available)</option>'}
            </select>
          </div>
          <div class="form-group">
            <label>Command / Function</label>
            <select id="editBtnCommand" class="form-control">
              ${cmdOptions || '<option value="">(No commands available)</option>'}
            </select>
          </div>
          <div class="form-group">
            <label>Button Label</label>
            <input type="text" id="editBtnLabel" class="form-control" value="${isEdit ? (btn.label || '') : ''}" placeholder="e.g. PLAY, HDMI 2, ▲">
          </div>
          <div class="form-group">
            <label>Color Accent</label>
            <select id="editBtnVariant" class="form-control">
              <option value="" ${!isEdit || !btn.variant ? 'selected' : ''}>Standard Dark</option>
              <option value="primary" ${isEdit && btn.variant === 'primary' ? 'selected' : ''}>Cyan Accent</option>
              <option value="accent" ${isEdit && btn.variant === 'accent' ? 'selected' : ''}>Green Play</option>
              <option value="warning" ${isEdit && btn.variant === 'warning' ? 'selected' : ''}>Orange Warning</option>
              <option value="danger" ${isEdit && btn.variant === 'danger' ? 'selected' : ''}>Red Power</option>
            </select>
          </div>
        </div>
        <div class="modal-footer">
          ${btn ? '<button type="button" class="btn btn-danger btn-sm" id="btnModalDelete">Clear Slot</button>' : ''}
          <div style="flex:1;"></div>
          <button type="button" class="btn btn-secondary btn-sm" id="btnModalCancel">Cancel</button>
          <button type="button" class="btn btn-primary btn-sm" id="btnModalSave">Save</button>
        </div>
      </div>
    `;
    modal.style.display = 'flex';

    const devSelect = modal.querySelector('#editBtnDevice');
    const cmdSelect = modal.querySelector('#editBtnCommand');
    const labelInput = modal.querySelector('#editBtnLabel');
    const saveBtn = modal.querySelector('#btnModalSave');

    const updateSaveState = () => {
      saveBtn.disabled = !cmdSelect.value;
    };
    updateSaveState();

    devSelect.addEventListener('change', () => {
      const newDevId = devSelect.value;
      const newCmds = this.deviceCommandsMap[newDevId] || [];
      cmdSelect.innerHTML = newCmds.map(c => {
        const name = c.name || c;
        return `<option value="${name}">${name}</option>`;
      }).join('') || '<option value="">(No commands available)</option>';
      if (cmdSelect.value) {
        labelInput.value = cmdSelect.value;
      } else {
        labelInput.value = '';
      }
      updateSaveState();
    });

    cmdSelect.addEventListener('change', () => {
      if (!labelInput.value || labelInput.value === currentCmd) {
        labelInput.value = cmdSelect.value;
      }
      updateSaveState();
    });

    const closeModal = () => { modal.style.display = 'none'; };
    modal.querySelector('#btnModalClose').addEventListener('click', closeModal);
    modal.querySelector('#btnModalCancel').addEventListener('click', closeModal);

    const deleteBtn = modal.querySelector('#btnModalDelete');
    if (deleteBtn) {
      deleteBtn.addEventListener('click', async () => {
        this.activeLayout.grid[slotIndex] = null;
        await saveRemoteLayout(this.currentScope, this.currentTarget.id, this.activeLayout);
        closeModal();
        this.updateDOM();
      });
    }

    saveBtn.addEventListener('click', async () => {
      const chosenDevId = this.currentScope === 'device' ? this.currentTarget.id : devSelect.value;
      const chosenCmd = cmdSelect.value;
      if (!chosenDevId || !chosenCmd) {
        alert('Please select a valid device and command.');
        return;
      }

      this.activeLayout.grid[slotIndex] = {
        id: `btn_${slotIndex}`,
        deviceId: chosenDevId,
        command: chosenCmd,
        label: labelInput.value.trim() || chosenCmd,
        variant: modal.querySelector('#editBtnVariant').value
      };

      await saveRemoteLayout(this.currentScope, this.currentTarget.id, this.activeLayout);
      closeModal();
      this.updateDOM();
    });
  }
}
