/* codex_webui_html.c - HTML page rendering for the web UI.
 * Included via #include from webui_server.c; do not compile separately. */

static void page_head(FILE *f, const char *title) {
    fprintf(f,
        "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>");
    html(f, title);
    fputs(
        "</title><style>"
        ":root{color-scheme:light;--bg:#f5f7f9;--fg:#182321;--muted:#687672;--line:#dce3e1;--panel:#fff;--soft:#f2f6f5;--soft2:#eaf6f4;--accent:#0f766e;--accent2:#32649d;--bad:#b42318;--ok:#087443;--warn:#a05a00;--wash:#fafcfb;--shadow:0 10px 28px rgba(25,41,37,.07)}"
        "*{box-sizing:border-box}html,body{width:100%;max-width:100%;overflow-x:hidden}body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.48 system-ui,-apple-system,Segoe UI,sans-serif}"
        "header{position:sticky;top:0;background:rgba(255,255,255,.98);backdrop-filter:saturate(1.15) blur(12px);border-bottom:1px solid var(--line);padding:12px 20px;z-index:3}"
        ".topbar{max-width:1280px;margin:0 auto;display:flex;align-items:center;justify-content:space-between;gap:16px}.brand{display:flex;align-items:center;gap:11px}.brand-mark{width:34px;height:34px;border-radius:8px;background:var(--accent);display:grid;place-items:center;color:#fff;font-weight:750}.brand h1{letter-spacing:0}.brand small{display:block;color:var(--muted);font-size:12px}.top-status{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}"
        ".app-shell{max-width:1280px;margin:0 auto;padding:22px 20px;display:grid;grid-template-columns:236px minmax(0,1fr);gap:22px}.side-menu{position:sticky;top:78px;align-self:start;display:grid;gap:5px;background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:9px;box-shadow:var(--shadow);min-width:0;max-width:100%}.menu-item{display:grid;grid-template-columns:34px 1fr;gap:10px;align-items:center;text-align:left;border:0;background:transparent;color:var(--fg);border-radius:8px;padding:10px 11px;box-shadow:none;min-height:54px}.menu-item>*{min-width:0}.menu-item:hover{background:var(--soft)}.menu-item span:first-child{width:28px;height:28px;border-radius:8px;display:grid;place-items:center;background:#edf3f2;color:var(--accent);font-size:0;position:relative}.menu-item span:first-child:before{content:\"\";display:block;width:13px;height:13px;border:2px solid currentColor;border-radius:4px}.menu-item[data-view-target=control] span:first-child:before{width:16px;height:16px;border-radius:50%;box-shadow:inset 0 0 0 4px #fff}.menu-item[data-view-target=activities] span:first-child:before{width:16px;height:12px;border:2px solid currentColor;border-radius:3px}.menu-item[data-view-target=ir] span:first-child:before{width:15px;height:9px;border-radius:9px}.menu-item[data-view-target=lab] span:first-child:before{width:15px;height:15px;border-radius:50%;box-shadow:inset 0 0 0 3px #fff}.menu-item[data-view-target=bluetooth] span:first-child:before{width:15px;height:15px;border-radius:50%;border-width:2px;box-shadow:0 -5px 0 -3px currentColor,0 5px 0 -3px currentColor}.menu-item[data-view-target=rf-remote] span:first-child:before{width:16px;height:14px;border:2px solid currentColor;border-radius:4px}.menu-item[data-view-target=mqtt] span:first-child:before{width:14px;height:14px;border-radius:50%;border-width:2px}.menu-item[data-view-target=wifi] span:first-child:before,.menu-item[data-view-target=network] span:first-child:before{width:15px;height:10px;border:0;border-top:2px solid currentColor;border-radius:50%}.menu-item[data-view-target=backup] span:first-child:before{width:15px;height:12px;border-radius:3px}.menu-item[data-view-target=system] span:first-child:before{width:13px;height:13px;border-radius:50%}.menu-item strong{display:block;font-size:13px;line-height:1.15;overflow-wrap:anywhere}.menu-item small{display:block;color:var(--muted);font-size:11px;margin-top:2px;line-height:1.15;overflow-wrap:anywhere}.menu-item.active{background:var(--soft2);color:#0c514d}.menu-item.active span:first-child{background:#fff;box-shadow:inset 0 0 0 1px rgba(15,118,110,.14)}.content{min-width:0;max-width:100%;display:grid;gap:18px}.section{display:none;scroll-margin-top:94px;min-width:0;max-width:100%}.section.active{display:grid;gap:15px}.section-head{display:flex;align-items:end;justify-content:space-between;gap:12px;padding:2px 0 4px}.section-lead{max-width:100%;color:var(--muted);font-size:13px;margin-top:5px;overflow-wrap:break-word}"
        "h1{font-size:20px;margin:0}h2{font-size:24px;line-height:1.12;margin:0;font-weight:750}h3{font-size:15px;margin:0 0 10px}.panel h2{font-size:18px;line-height:1.2;margin:0 0 6px}.muted{color:var(--muted)}.mini{font-size:12px}.nowrap{white-space:nowrap}.subtle{font-size:12px;color:var(--muted);margin-top:2px}.help{font-size:12px;color:var(--muted);margin-top:5px}.help,.muted,.subtle,.callout{overflow-wrap:anywhere}.callout{border:1px solid #dbe7ef;border-left:4px solid var(--accent2);background:#fbfdff;border-radius:8px;padding:12px 14px;margin:0 0 12px;color:#263a52}.callout strong{display:block;margin-bottom:2px}.quick-actions{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:12px;margin-top:14px}.quick-actions button{min-height:72px;text-align:left;background:#fff;color:var(--fg);border-color:var(--line);box-shadow:0 1px 2px rgba(20,40,32,.04);padding:14px}.quick-actions button strong{display:block;margin-bottom:4px}.quick-actions button:hover{border-color:#b9c8c4;background:#fbfefd}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px}.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:12px}.dashboard-cards{grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:12px}.panel,.stat,.setup-shell{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:16px;box-shadow:0 1px 2px rgba(20,40,32,.04);min-width:0;max-width:100%}.panel>*,.stat>*,.setup-shell>*{min-width:0;max-width:100%}.stat{min-height:92px;padding:17px}.stat .value{font-size:20px;font-weight:700;margin-top:5px}.stat .label{text-transform:uppercase;letter-spacing:0;color:var(--muted);font-size:11px}"
        ".kv{display:grid;grid-template-columns:132px 1fr;gap:6px 10px}.badge,.pill{display:inline-flex;align-items:center;border:1px solid var(--line);border-radius:999px;padding:3px 9px;background:var(--wash);font-size:12px}.ok{color:var(--ok)}.bad{color:var(--bad)}.warn{color:var(--warn)}"
        "label{display:block;margin:10px 0 4px;color:var(--muted);font-size:12px}input,select,textarea{width:100%;max-width:100%;min-width:0;padding:10px 11px;border:1px solid var(--line);border-radius:6px;background:#fff;color:var(--fg);min-height:38px}input:focus,select:focus,textarea:focus{outline:2px solid rgba(15,118,110,.16);border-color:var(--accent)}textarea{min-height:88px;resize:vertical}.textarea-tall{min-height:190px;font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:12px}"
        "input[type=checkbox]{width:auto;min-height:0}.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;align-items:center}.save-status{font-size:12px;color:var(--muted);align-self:center}"
        "button,a.button{border:1px solid var(--accent);background:var(--accent);color:#fff;border-radius:6px;padding:9px 12px;min-height:38px;cursor:pointer;font-weight:650;transition:border-color .12s,background .12s,box-shadow .12s,transform .08s;text-decoration:none;line-height:1.15}button:hover,a.button:hover{box-shadow:0 2px 8px rgba(15,118,110,.12)}button:active,a.button:active{transform:translateY(1px)}button:focus-visible,a.button:focus-visible,input:focus-visible,select:focus-visible,textarea:focus-visible{outline:2px solid rgba(15,118,110,.28);outline-offset:2px}a.button{display:inline-flex;align-items:center;justify-content:center}.secondary{background:#fff;color:var(--accent)}.danger{border-color:var(--bad);color:var(--bad);background:#fff}.ghost{background:var(--wash);border-color:var(--line);color:var(--fg)}"
        "pre{white-space:pre-wrap;word-break:break-word;margin:0;background:var(--soft);border-radius:6px;padding:10px;max-height:340px;overflow:auto}details{border:1px solid var(--line);border-radius:8px;background:var(--panel);padding:11px 13px}summary{cursor:pointer;font-weight:650}"
        ".msg{border-left:4px solid var(--accent2);padding:10px 12px;background:#f8fbff;border-radius:8px}.table{width:100%;border-collapse:collapse}.table th,.table td{border-top:1px solid var(--line);padding:7px;text-align:left;vertical-align:top}.device-list{display:grid;gap:14px}.command-list{display:grid;gap:8px}.command{display:grid;grid-template-columns:1fr auto;gap:10px;align-items:center;border-top:1px solid var(--line);padding-top:10px}.command .command-editor{grid-column:1/-1;background:var(--wash)}.command-editor{margin-top:10px}.command-preview{max-height:110px;background:#fff;border:1px solid var(--line);margin-top:6px}.bt-saved{grid-column:1/-1}.bt-device-card{border-top:1px solid var(--line);padding-top:14px;margin-top:14px}.bt-device-card h4{margin:0 0 4px;font-size:15px}.save-script-box{border-top:1px solid var(--line);padding-top:10px;margin-top:4px}.export-list{display:flex;gap:8px;flex-wrap:wrap}.hidden{display:none!important}.preview{display:grid;gap:6px;max-height:260px;overflow:auto;border:1px solid var(--line);border-radius:6px;padding:8px;background:var(--soft)}.preview label{display:grid;grid-template-columns:auto 1fr;gap:8px;align-items:start;margin:0;color:var(--fg)}.match{width:100%;text-align:left;background:#fff;color:var(--fg);border-color:var(--line);padding:8px;white-space:normal;overflow-wrap:anywhere;word-break:break-word}.match strong{color:var(--accent2)}"
        ".ir-stored-layout{display:grid;grid-template-columns:minmax(230px,.34fr) minmax(0,1fr);gap:14px;align-items:start;margin-bottom:15px}.ir-stored-layout>*,.ir-control-layout>*,.ir-device-workspace>*{min-width:0;max-width:100%}.ir-device-picks{display:grid;gap:8px}.ir-device-pick{display:grid;grid-template-columns:1fr auto;gap:8px;align-items:center;text-align:left;background:#fff;color:var(--fg);border-color:var(--line);padding:11px}.ir-device-pick:hover,.ir-device-pick.active{background:var(--soft2);border-color:#b9d8d3}.ir-device-pick strong{display:block}.ir-device-workspace{display:none;gap:14px;min-width:0;max-width:100%}.ir-device-workspace.active{display:grid}.ir-work-head{display:flex;align-items:start;justify-content:space-between;gap:12px}.ir-quick-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(128px,1fr));gap:8px}.ir-quick-grid .ir-send-form button{width:100%;background:#fff;color:var(--accent);border-color:var(--line);text-align:left;min-height:42px}.ir-quick-grid .ir-send-form button:hover,.ir-quick-grid .ir-send-form button.sending{background:var(--soft2);border-color:var(--accent)}.ir-command-tools{display:grid;gap:10px}.ir-command-row{display:grid;grid-template-columns:minmax(0,1fr) auto;gap:10px;align-items:center;border-top:1px solid var(--line);padding-top:10px}.ir-command-row form{margin:0}.ir-status{min-height:20px}.sr-only{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}"
        ".vremote-tabs{display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid var(--line);padding-bottom:10px;margin-bottom:14px;gap:8px;flex-wrap:wrap}.vremote-tab-group{display:flex;gap:6px}.vtab-btn{background:var(--wash);border:1px solid var(--line);color:var(--fg);border-radius:6px;padding:6px 12px;font-size:12px;font-weight:600;cursor:pointer}.vtab-btn:hover{background:var(--soft2);border-color:#b9d8d3}.vtab-btn.active{background:var(--accent);color:#fff;border-color:var(--accent)}.vtab-edit-toggle.active{background:var(--warn);color:#fff;border-color:var(--warn)}.vtab-panel{min-width:0;max-width:100%}.vtab-panel.hidden{display:none!important}"
        ".vremote-stage{display:grid;grid-template-columns:minmax(0,1fr);gap:18px;align-items:start;min-width:0;width:100%}.vremote-stage.editing{grid-template-columns:minmax(280px,330px) minmax(0,1fr)}.vslot-inspector{min-width:0;max-width:100%}.vslot-inspector .panel{border-color:#b9d8d3;background:var(--wash)}.vrem-btn.selected{border-color:#ffb703!important;box-shadow:0 0 12px rgba(255,183,3,.75)!important;background:#353324!important}.vslot-chip{padding:4px 8px;border-radius:4px;font-size:11px;background:#fff;border:1px solid var(--line);cursor:pointer;color:var(--fg);font-weight:550;transition:all .1s ease}.vslot-chip:hover{background:var(--soft2);border-color:var(--accent)}.vslot-chip.active{background:var(--accent);color:#fff;border-color:var(--accent)}"
        ".vremote-body{width:min(100%,320px);margin:0 auto;background:linear-gradient(180deg,#242b29,#161d1b);border:1px solid rgba(255,255,255,.08);border-radius:24px;padding:18px 14px;box-shadow:0 18px 40px rgba(0,0,0,.45);display:grid;gap:14px}.vrem-row{display:flex;justify-content:space-between;align-items:center;gap:8px}.vrem-btn{flex:1;min-height:40px;background:#2d3734;border:1px solid rgba(255,255,255,.08);color:#edf3f1;border-radius:8px;font-size:12px;font-weight:600;cursor:pointer;display:inline-flex;align-items:center;justify-content:center;padding:4px 8px;transition:all .1s ease}.vrem-btn:hover{background:#3b4744;border-color:#00ffb4;color:#fff}.vrem-btn.sending{background:var(--accent);color:#fff;border-color:#00ffb4;box-shadow:0 0 10px rgba(0,255,180,.6)}.vrem-btn.unmapped{opacity:.4;border-style:dashed}.vrem-edit-mode .vrem-btn{border-color:#ffb703;background:#353324}.vrem-btn-power{background:#4a151b;color:#ff9fa9;border-color:#782029}.vrem-btn-power:hover{background:#631b23;color:#fff}"
        ".vrem-dpad{display:grid;grid-template-columns:repeat(3,1fr);grid-template-rows:repeat(3,44px);gap:6px;width:190px;height:190px;margin:4px auto}.vrem-dpad-up{grid-column:2;grid-row:1;border-radius:12px 12px 4px 4px}.vrem-dpad-left{grid-column:1;grid-row:2;border-radius:12px 4px 4px 12px}.vrem-dpad-ok{grid-column:2;grid-row:2;border-radius:50%;background:#3b4744;font-weight:700}.vrem-dpad-right{grid-column:3;grid-row:2;border-radius:4px 12px 12px 4px}.vrem-dpad-down{grid-column:2;grid-row:3;border-radius:4px 4px 12px 12px}"
        ".vrem-rocker{display:flex;flex-direction:column;gap:3px;flex:1}.vrem-rocker button{min-height:36px}.vrem-rocker-top{border-radius:8px 8px 3px 3px}.vrem-rocker-lbl{font-size:10px;text-transform:uppercase;color:var(--muted);text-align:center;padding:2px 0}.vrem-rocker-bot{border-radius:3px 3px 8px 8px}.vrem-colors{display:flex;gap:8px;justify-content:center}.vrem-color-btn{width:36px;height:24px;border-radius:6px;border:0;cursor:pointer}.vrem-color-red{background:#e53935}.vrem-color-green{background:#43a047}.vrem-color-yellow{background:#fdd835}.vrem-color-blue{background:#1e88e5}.vrem-numpad{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;max-width:240px;margin:0 auto}"
        ".setup-shell{padding:0;overflow:hidden}.wizard-top{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:16px 18px;border-bottom:1px solid var(--line);background:#fff}.wizard-top h3{margin:0}.wizard-grid{display:grid;grid-template-columns:210px 1fr;min-height:420px}.stepper{border-right:1px solid var(--line);background:#f8fbfa;padding:12px;display:grid;align-content:start;gap:6px}.step{display:grid;grid-template-columns:28px 1fr;gap:9px;align-items:center;width:100%;text-align:left;background:transparent;color:var(--fg);border-color:transparent;padding:10px}.step span{width:26px;height:26px;border-radius:999px;display:grid;place-items:center;background:#fff;border:1px solid var(--line);color:var(--accent);font-weight:750}.step.active{background:#fff;border-color:var(--line);box-shadow:0 1px 2px rgba(20,40,32,.04)}.wizard-body{padding:18px;min-width:0;max-width:100%}.wizard-panel{display:none;min-width:0;max-width:100%}.wizard-panel.active{display:block}.wizard-status{min-height:20px;margin-top:10px;color:var(--muted)}.device-sync{display:grid;grid-template-columns:1fr auto;gap:10px;align-items:end}.guide-steps{display:grid;gap:8px;margin:10px 0 12px}.guide-step{display:grid;grid-template-columns:28px 1fr;gap:10px;align-items:start;border:1px solid var(--line);background:var(--wash);border-radius:8px;padding:9px 10px}.guide-step>*{min-width:0}.guide-step b{width:22px;height:22px;border-radius:999px;background:var(--soft2);color:var(--accent);display:grid;place-items:center;font-size:12px}"
        ".lab-layout{display:grid;grid-template-columns:minmax(0,1.05fr) minmax(320px,.95fr);gap:14px;min-width:0}.lab-layout>*{min-width:0}.bt-layout{display:grid;grid-template-columns:1fr;gap:16px;min-width:0}.bt-left-col,.bt-right-col{display:grid;gap:14px;min-width:0}.bt-script-layout{display:grid;grid-template-columns:1fr;gap:12px;min-width:0}.bt-script-tools{display:grid;gap:8px;align-content:start;min-width:0}.lab-toolbar{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px}.lab-quick{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:8px;margin:10px 0 12px}.lab-quick button{text-align:left;min-height:50px;background:#fff;color:var(--accent);border-color:var(--accent)}.lab-quick button:hover{background:#f8fbfa;border-color:#0b625c}.lab-quick button .queue-meta{color:var(--muted);font-weight:600}.lab-presets,.queue-tools{display:flex;gap:7px;flex-wrap:wrap;margin-top:10px}.lab-presets button,.queue-tools button{padding:6px 9px;font-size:12px}.lab-advanced{margin-top:12px}.inline-check{display:inline-flex;align-items:center;gap:8px;margin-top:10px}.lab-summary{display:flex;justify-content:space-between;gap:10px;align-items:center;border:1px solid var(--line);border-radius:8px;background:var(--wash);padding:9px 10px;margin:8px 0;color:var(--muted);font-size:12px}.queue-list{display:grid;gap:7px;max-height:390px;overflow:auto;border:1px solid var(--line);border-radius:8px;background:var(--soft);padding:8px}.queue-row{display:grid;grid-template-columns:auto 1fr auto;gap:9px;align-items:start;background:#fff;border:1px solid var(--line);border-radius:7px;padding:8px}.queue-row strong{display:block}.queue-meta{color:var(--muted);font-size:11px;overflow-wrap:anywhere}.meter{height:8px;border-radius:999px;background:#e7eeec;overflow:hidden}.meter span{display:block;height:100%;width:0;background:var(--accent)}button:disabled{opacity:.55;cursor:not-allowed;transform:none}.kb-panel{width:100%;max-width:100%;min-width:0;margin-top:0;overflow-x:auto;overflow-y:hidden;padding-bottom:4px;-webkit-overflow-scrolling:touch}.kb-row{display:flex;gap:3px;margin-bottom:3px;justify-content:flex-start;min-width:max-content}.kb-key{min-width:36px;height:38px;padding:0 6px;border:1px solid var(--line);border-radius:5px;background:#fff;cursor:pointer;font-size:12px;font-family:inherit;display:inline-flex;align-items:center;justify-content:center;flex-shrink:0;transition:background .06s,color .06s;color:var(--fg)}.kb-key:hover{background:var(--soft);border-color:var(--accent)}.kb-key.kb-on{background:var(--accent);color:#fff;border-color:var(--accent)}.kb-125{min-width:48px}.kb-15{min-width:54px}.kb-2{min-width:72px}.kb-225{min-width:82px}.kb-25{min-width:90px}.kb-275{min-width:100px}.kb-sp{min-width:200px;max-width:200px;width:200px}.kb-fwd{margin-bottom:8px}"
        ".b25-layout{display:grid;grid-template-columns:minmax(260px,320px) minmax(0,1fr);gap:18px;align-items:start;min-width:0}.b25-layout>*{min-width:0}.b25-remote-shell{width:100%;display:grid;place-items:center;background:linear-gradient(180deg,#1c2321,#121816);border:1px solid rgba(255,255,255,.08);border-radius:12px;padding:14px}.b25-remote-skin{position:relative;width:min(100%,230px);aspect-ratio:191/898}.b25-remote-skin img{display:block;width:100%;height:100%;object-fit:contain;border-radius:24px;box-shadow:0 18px 40px rgba(0,0,0,.5)}.b25-hotspot{position:absolute;padding:0;margin:0;border:1.5px solid rgba(0,255,180,.35);background:rgba(0,255,180,.08);border-radius:999px;cursor:pointer;transition:all .1s ease;box-sizing:border-box}.b25-hotspot:hover{border-color:#00ffb4;background:rgba(0,255,180,.35);box-shadow:0 0 10px rgba(0,255,180,.5)}.b25-hotspot.selected{border-color:#ffb703;background:rgba(255,183,3,.35);box-shadow:0 0 12px rgba(255,183,3,.7)}.b25-hotspot.mapped{border-color:#06d6a0;background:rgba(6,214,160,.24)}.b25-hotspot.rect{border-radius:6px}"
        ".elite-layout{display:grid;grid-template-columns:minmax(280px,360px) minmax(0,1fr);gap:18px;align-items:start;min-width:0}.elite-layout>*{min-width:0}.elite-remote-shell{width:100%;display:grid;place-items:center;background:linear-gradient(180deg,#1c2321,#121816);border:1px solid rgba(255,255,255,.08);border-radius:12px;padding:14px}.elite-remote-skin{position:relative;width:min(100%,300px);aspect-ratio:9/16}.elite-remote-skin img{display:block;width:100%;height:100%;object-fit:contain;border-radius:28px;box-shadow:0 18px 40px rgba(0,0,0,.5)}.elite-hotspot{position:absolute;padding:0;margin:0;border:1.5px solid rgba(0,255,180,.35);background:rgba(0,255,180,.08);border-radius:999px;cursor:pointer;transition:all .1s ease;box-sizing:border-box}.elite-hotspot:hover{border-color:#00ffb4;background:rgba(0,255,180,.35);box-shadow:0 0 10px rgba(0,255,180,.5)}.elite-hotspot.selected{border-color:#ffb703;background:rgba(255,183,3,.35);box-shadow:0 0 12px rgba(255,183,3,.7)}.elite-hotspot.mapped{border-color:#06d6a0;background:rgba(6,214,160,.24)}.elite-hotspot.rect{border-radius:6px}"
        "@media(max-width:980px){.ir-stored-layout,.ir-control-layout,.b25-layout,.elite-layout,.vremote-stage.editing{grid-template-columns:minmax(0,1fr)}.ir-remote-skin{width:min(100%,250px)}}"
        "@media(max-width:860px){header{padding:12px 14px}.topbar{max-width:none;width:100%}.app-shell{width:100%;max-width:100%;grid-template-columns:minmax(0,1fr);padding:14px;gap:16px}.side-menu{position:sticky;top:62px;z-index:2;display:flex;max-width:100%;overflow-x:auto;gap:6px;border-radius:10px;box-shadow:0 4px 16px rgba(25,41,37,.06);scrollbar-width:thin}.menu-item{min-width:168px}.row,.wizard-grid,.device-sync,.lab-layout,.bt-layout,.bt-script-layout{grid-template-columns:minmax(0,1fr)}.kv{grid-template-columns:1fr}.command,.ir-command-row{grid-template-columns:1fr}.stepper{border-right:0;border-bottom:1px solid var(--line);grid-template-columns:repeat(2,1fr)}}"
        "@media(max-width:520px){body{font-size:13px}header{position:static;padding:10px}.topbar{align-items:flex-start;flex-direction:column;gap:8px}.brand-mark{width:30px;height:30px}.brand h1{font-size:16px}.top-status{justify-content:flex-start}.app-shell{padding:10px;gap:14px}.side-menu{position:static;display:grid;grid-template-columns:minmax(0,1fr);gap:7px;padding:7px}.menu-item{min-width:0;min-height:46px;padding:8px;grid-template-columns:28px 1fr}.menu-item span:first-child{width:24px;height:24px}.menu-item strong{font-size:12px}.menu-item small{font-size:10px}.section-head,.ir-work-head{align-items:flex-start;flex-direction:column}.panel,.stat,.wizard-body{padding:14px}.grid,.cards,.quick-actions,.lab-toolbar,.lab-quick{grid-template-columns:minmax(0,1fr)}.guide-step{grid-template-columns:24px 1fr;padding:8px}.actions button,.actions a.button{width:100%}.ir-quick-grid{grid-template-columns:minmax(0,1fr) minmax(0,1fr)}.ir-remote-shell{padding:8px}.ir-remote-skin{width:min(100%,230px)}.kb-panel{margin-left:-2px;margin-right:-2px}.kb-key{min-width:32px;height:36px;font-size:11px}.kb-15{min-width:48px}.kb-2{min-width:64px}.kb-225{min-width:74px}.kb-sp{min-width:150px}}"
        "@media(max-width:420px){.side-menu{grid-template-columns:minmax(0,1fr)}.menu-item{min-height:46px}}"
        "</style></head><body>",
        f);
    fprintf(f,
        "<header><div class='topbar'><div class='brand'><div class='brand-mark'>H</div><div><h1>Harmony Hub Control</h1><small>Local smart home console</small></div></div><div class='top-status'><span class='pill'>Local control</span></div></div></header><main class='app-shell'><aside class='side-menu' aria-label='Main menu'><button type='button' class='menu-item active' data-view-target='overview'><span>D</span><div><strong>Dashboard</strong><small>Status</small></div></button><button type='button' class='menu-item' data-view-target='activities'><span>A</span><div><strong>Activities</strong><small>Run & setup</small></div></button><button type='button' class='menu-item' data-view-target='control'><span>R</span><div><strong>Control</strong><small>Send buttons</small></div></button><button type='button' class='menu-item' data-view-target='ir'><span>IR</span><div><strong>IR Setup</strong><small>Add remotes</small></div></button><button type='button' class='menu-item' data-view-target='lab'><span>L</span><div><strong>Bulk IR Test</strong><small>Queue IR codes</small></div></button><button type='button' class='menu-item' data-view-target='bluetooth'><span>BT</span><div><strong>Bluetooth</strong><small>Keyboard</small></div></button><button type='button' class='menu-item' data-view-target='rf-remote'><span>EL</span><div><strong>Harmony Elite</strong><small>RF Remote & Screen</small></div></button><button type='button' class='menu-item' data-view-target='remotes'><span>BT</span><div><strong>Bluetooth Remote</strong><small>Homatics BLE</small></div></button><button type='button' class='menu-item' data-view-target='mqtt'><span>M</span><div><strong>MQTT</strong><small>Home Assistant</small></div></button><button type='button' class='menu-item' data-view-target='network'><span>N</span><div><strong>Network</strong><small>Wi-Fi & Ethernet</small></div></button><button type='button' class='menu-item' data-view-target='backup'><span>B</span><div><strong>Backup</strong><small>Import/export</small></div></button><button type='button' class='menu-item' data-view-target='system'><span>S</span><div><strong>System</strong><small>Logs/update</small></div></button></aside><div class='content'>");
}

static void page_end(FILE *f) {
    fputs("<script>const REMOTE_B25_SKIN_SRC='https://cdn.jsdelivr.net/gh/iranl/harmony-hub-control@main/docs/assets/remote_b25_skin.jpg';\nconst REMOTE_ELITE_SKIN_SRC='/assets/remote_elite_skin.jpg';\n", f);
    fputs(
        "const $=id=>document.getElementById(id);"
        "if('scrollRestoration' in history)history.scrollRestoration='manual';"
        "var wizardInventory=null,wizardInventoryPromise=null,activityInventory=null,btPollTimer=null,actPollTimer=null;"
        "var b25MapData={remote:{name:'Homatics B25'},activities:{}},b25SelectedBtn='power';"
        "var eliteMapData={remote:{name:'Harmony Elite'},activities:{}},eliteSelectedBtn='Select';"
        "var wsClient=null,wsConnected=false,wsPendingReqs=new Map(),wsMsgSeq=1;"
        "function initWebUiWs(){if(wsClient){try{wsClient.close();}catch(e){}}try{const wsHost=location.hostname||'127.0.0.1';wsClient=new WebSocket('ws://'+wsHost+':8089/');wsClient.onopen=()=>{wsConnected=true;};wsClient.onmessage=e=>{try{const d=JSON.parse(e.data);if(d.msgId&&wsPendingReqs.has(d.msgId)){const cb=wsPendingReqs.get(d.msgId);wsPendingReqs.delete(d.msgId);cb(d);}if(d.type==='activity_progress'||d.type==='activity_state'){loadActivities();}}catch(err){}};wsClient.onerror=()=>{wsConnected=false;};wsClient.onclose=()=>{wsConnected=false;setTimeout(initWebUiWs,3000);};}catch(e){setTimeout(initWebUiWs,3000);}}"
        "function sendWsCommand(action,payload){return new Promise((resolve,reject)=>{if(!wsConnected||!wsClient||wsClient.readyState!==WebSocket.OPEN){return reject(new Error('WS offline'));}const msgId=wsMsgSeq++;const timer=setTimeout(()=>{wsPendingReqs.delete(msgId);reject(new Error('WS timeout'));},3000);wsPendingReqs.set(msgId,res=>{clearTimeout(timer);resolve(res);});try{wsClient.send(JSON.stringify(Object.assign({action:action,msgId:msgId},payload||{})));}catch(err){clearTimeout(timer);wsPendingReqs.delete(msgId);reject(err);}});}"
        "function loadWizardInventory(){if(wizardInventory)return Promise.resolve(wizardInventory);if(wizardInventoryPromise)return wizardInventoryPromise;wizardInventoryPromise=fetch('/api/inventory').then(r=>r.json()).then(j=>{wizardInventory=j;populateVerifyCommands();return wizardInventory;}).catch(e=>{wizardInventory=null;return null;}).finally(()=>{wizardInventoryPromise=null;});return wizardInventoryPromise;}"
        "var webRemoteLayouts={},webRemoteEditMode={},webRemoteSelectedSlot={};"
        "async function initWebRemoteLayouts(){try{const s=localStorage.getItem('webui_remote_layout');if(s){webRemoteLayouts=JSON.parse(s);}else{const r=await fetch('/api/ui-remote-layout');const j=await r.json();if(j&&j.ok&&j.layouts){webRemoteLayouts=j.layouts;localStorage.setItem('webui_remote_layout',JSON.stringify(j.layouts));}}}catch(e){}applyWebRemoteLayouts();}"
        "function applyWebRemoteLayouts(){document.querySelectorAll('.vremote-body').forEach(rem=>{const devId=rem.dataset.vremoteShell;const dl=(webRemoteLayouts&&webRemoteLayouts[devId])||{};rem.querySelectorAll('[data-vslot]').forEach(btn=>{const slot=btn.dataset.vslot;if(dl[slot]!==undefined){const c=dl[slot];btn.dataset.cmdName=c||'';btn.classList.toggle('unmapped',!c);btn.classList.toggle('mapped',!!c);btn.title=(btn.dataset.slotLabel||slot)+(c?': '+c:' (Unmapped)');}else{btn.classList.toggle('unmapped',!btn.dataset.cmdName);btn.classList.toggle('mapped',!!btn.dataset.cmdName);}});});}"
        "const STANDARD_REMOTE_ALIASES={power:['poweroff','power off','standby','shutdown','off','powertoggle','power toggle','power'],input:['input','source','inputnext','inputsource'],back:['back','return','exit','escape','previous'],home:['home','menu','topmenu'],menu:['menu','settings','setup','options','option'],info:['info','information','display','guide','epg'],up:['up','directionup','arrowup','cursorup'],down:['down','directiondown','arrowdown','cursordown'],left:['left','directionleft','arrowleft','cursorleft'],right:['right','directionright','arrowright','cursorright'],select:['ok','select','enter'],vol_up:['volumeup','volup','vol up','vol_up','volume up'],vol_down:['volumedown','voldown','voldn','vol down','vol_down','volume down'],mute:['mute','volumemute'],ch_up:['channelup','chup','chnext','ch_next','ch up','pageup','pgup'],ch_down:['channeldown','chdown','chdn','chprev','ch_prev','ch down','pagedown','pgdown'],rewind:['rewind','rev','skipback'],play:['play'],pause:['pause'],stop:['stop'],forward:['fastforward','forward','ffwd','skipforward'],red:['red','colorred'],green:['green','colorgreen'],yellow:['yellow','coloryellow'],blue:['blue','colorblue'],'1':['1','digit1','number1','num1'],'2':['2','digit2','number2','num2'],'3':['3','digit3','number3','num3'],'4':['4','digit4','number4','num4'],'5':['5','digit5','number5','num5'],'6':['6','digit6','number6','num6'],'7':['7','digit7','number7','num7'],'8':['8','digit8','number8','num8'],'9':['9','digit9','number9','num9'],'0':['0','digit0','number0','num0'],dash:['dash','hyphen','dot','period','minus'],enter:['enter','e','select','ok']};"
        "function assignSlotCommand(devId,slot,cmdName){if(!webRemoteLayouts[devId])webRemoteLayouts[devId]={};if(cmdName){webRemoteLayouts[devId][slot]=cmdName;}else{delete webRemoteLayouts[devId][slot];}localStorage.setItem('webui_remote_layout',JSON.stringify(webRemoteLayouts));const rem=document.querySelector('.vremote-body[data-vremote-shell=\"'+devId+'\"]');if(rem){const btn=rem.querySelector('[data-vslot=\"'+slot+'\"]');if(btn){btn.dataset.cmdName=cmdName||'';btn.classList.toggle('unmapped',!cmdName);btn.classList.toggle('mapped',!!cmdName);btn.title=(btn.dataset.slotLabel||slot)+(cmdName?': '+cmdName:' (Unmapped)');populateInspector(devId,btn);}}irStatus(devId,cmdName?('Assigned \"'+cmdName+'\" to '+slot):('Cleared '+slot));}"
        "function populateInspector(devId,slotBtn){if(!slotBtn)return;const insp=document.querySelector('.vslot-inspector[data-inspector-device=\"'+devId+'\"]');if(!insp)return;webRemoteSelectedSlot[devId]=slotBtn.dataset.vslot;const rem=slotBtn.closest('.vremote-body');if(rem)rem.querySelectorAll('[data-vslot]').forEach(b=>b.classList.toggle('selected',b===slotBtn));const slot=slotBtn.dataset.vslot,lbl=slotBtn.dataset.slotLabel||slot,cmd=slotBtn.dataset.cmdName||'';const nameEl=insp.querySelector('.vslot-target-name'),statEl=insp.querySelector('.vslot-target-status');if(nameEl)nameEl.textContent=lbl;if(statEl){statEl.className='vslot-target-status badge '+(cmd?'ok':'warn');statEl.textContent=cmd?('mapped: '+cmd):'unmapped';}const cmds=currentCommandsFor(devId);const sel=insp.querySelector('.vslot-cmd-select'),chipsBox=insp.querySelector('.vslot-chips-grid');if(sel){const curVal=cmd;sel.replaceChildren();const optNone=document.createElement('option');optNone.value='';optNone.textContent='-- None (Unmapped) --';sel.append(optNone);cmds.forEach(c=>{const o=document.createElement('option');o.value=c.name;o.textContent=c.name;sel.append(o);});sel.value=curVal;}if(chipsBox){chipsBox.replaceChildren();cmds.forEach(c=>{const chip=document.createElement('button');chip.type='button';chip.className='vslot-chip'+(c.name===cmd?' active':'');chip.textContent=c.name;chip.dataset.cmd=c.name;chip.addEventListener('click',()=>assignSlotCommand(devId,slot,c.name));chipsBox.append(chip);});}}"
        "function setupWebRemoteEvents(){document.querySelectorAll('.vtab-btn').forEach(btn=>{btn.addEventListener('click',()=>{const t=btn.dataset.vtabTarget,c=btn.closest('.ir-device-workspace');if(!c)return;c.querySelectorAll('.vtab-btn').forEach(b=>b.classList.toggle('active',b===btn));c.querySelectorAll('.vtab-panel').forEach(p=>{p.classList.toggle('hidden',!p.classList.contains('vtab-panel-'+t));p.classList.toggle('active',p.classList.contains('vtab-panel-'+t));});});});document.querySelectorAll('.vtab-edit-toggle').forEach(btn=>{btn.addEventListener('click',()=>{const devId=btn.dataset.deviceEdit;webRemoteEditMode[devId]=!webRemoteEditMode[devId];const isEdit=!!webRemoteEditMode[devId];btn.classList.toggle('active',isEdit);btn.textContent=isEdit?'Done Editing':'Edit Layout';const stage=document.querySelector('.vremote-stage[data-remote-stage=\"'+devId+'\"]');const insp=document.querySelector('.vslot-inspector[data-inspector-device=\"'+devId+'\"]');const rem=document.querySelector('.vremote-body[data-vremote-shell=\"'+devId+'\"]');if(stage)stage.classList.toggle('editing',isEdit);if(insp)insp.classList.toggle('hidden',!isEdit);if(rem)rem.classList.toggle('vrem-edit-mode',isEdit);if(isEdit){let selBtn=rem?rem.querySelector('[data-vslot].selected'):null;if(!selBtn&&rem)selBtn=rem.querySelector('[data-vslot]');if(selBtn)populateInspector(devId,selBtn);}else if(rem){rem.querySelectorAll('[data-vslot]').forEach(b=>b.classList.remove('selected'));}});});document.querySelectorAll('.vslot-inspector').forEach(insp=>{const devId=insp.dataset.inspectorDevice;const searchInp=insp.querySelector('.vslot-search');if(searchInp){searchInp.addEventListener('input',()=>{const q=searchInp.value.trim().toLowerCase();insp.querySelectorAll('.vslot-chip').forEach(ch=>{const txt=ch.dataset.cmd.toLowerCase();ch.classList.toggle('hidden',!!q&&!txt.includes(q));});const sel=insp.querySelector('.vslot-cmd-select');if(sel){Array.from(sel.options).forEach(opt=>{if(!opt.value)return;opt.hidden=!!q&&!opt.value.toLowerCase().includes(q);});}});};const assignBtn=insp.querySelector('.vslot-assign-btn');if(assignBtn){assignBtn.addEventListener('click',()=>{const slot=webRemoteSelectedSlot[devId];const sel=insp.querySelector('.vslot-cmd-select');if(!slot)return alert('Select a button first.');assignSlotCommand(devId,slot,sel?sel.value:'');});}const clearBtn=insp.querySelector('.vslot-clear-btn');if(clearBtn){clearBtn.addEventListener('click',()=>{const slot=webRemoteSelectedSlot[devId];if(!slot)return;assignSlotCommand(devId,slot,'');});}const testBtn=insp.querySelector('.vslot-test-btn');if(testBtn){testBtn.addEventListener('click',async()=>{const sel=insp.querySelector('.vslot-cmd-select');const cmd=sel?sel.value:'';if(!cmd)return alert('No command selected to test.');try{testBtn.disabled=true;irStatus(devId,'Testing '+cmd+'...');const j=await postJson('/api/ir-send',{deviceId:devId,command:cmd});irStatus(devId,'Sent '+cmd+(j.reply?': '+String(j.reply).slice(0,100):''));}catch(e){irStatus(devId,'Test failed: '+(e.message||e));}finally{testBtn.disabled=false;}});};const autoBtn=insp.querySelector('.vslot-automap-btn');if(autoBtn){autoBtn.addEventListener('click',()=>{const cmds=currentCommandsFor(devId);if(!cmds||!cmds.length)return alert('No saved commands for this device.');const norm=s=>String(s||'').toLowerCase().replace(/[^a-z0-9]/g,'');if(!webRemoteLayouts[devId])webRemoteLayouts[devId]={};let mapped=0;Object.entries(STANDARD_REMOTE_ALIASES).forEach(([slot,aliases])=>{for(const a of aliases){const ta=norm(a);const m=cmds.find(c=>norm(c.name)===ta);if(m){webRemoteLayouts[devId][slot]=m.name;mapped++;break;}}});localStorage.setItem('webui_remote_layout',JSON.stringify(webRemoteLayouts));applyWebRemoteLayouts();const rem=document.querySelector('.vremote-body[data-vremote-shell=\"'+devId+'\"]');const selBtn=rem?rem.querySelector('[data-vslot].selected')||rem.querySelector('[data-vslot]'):null;if(selBtn)populateInspector(devId,selBtn);alert('Auto-mapped '+mapped+' standard button(s)!');});};const resetBtn=insp.querySelector('.vslot-reset-btn');if(resetBtn){resetBtn.addEventListener('click',()=>{if(!confirm('Reset remote layout to defaults for this device?'))return;delete webRemoteLayouts[devId];localStorage.setItem('webui_remote_layout',JSON.stringify(webRemoteLayouts));location.reload();});};});document.querySelectorAll('.vtab-save-hub').forEach(btn=>{btn.addEventListener('click',async()=>{try{btn.disabled=true;btn.textContent='Saving...';const r=await fetch('/api/ui-remote-layout',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(webRemoteLayouts)});const j=await r.json();if(j&&j.ok)alert('Remote layout saved to Hub!');else throw new Error(j.error||j.message||'Failed');}catch(e){alert('Save to hub failed: '+(e.message||e));}finally{btn.disabled=false;btn.textContent='Save to Hub';}});});document.querySelectorAll('.vremote-body [data-vslot]').forEach(btn=>{btn.addEventListener('click',async()=>{const devId=btn.dataset.deviceId,slot=btn.dataset.vslot,isEdit=!!webRemoteEditMode[devId];if(isEdit){populateInspector(devId,btn);return;}const cmd=btn.dataset.cmdName;if(!cmd){irStatus(devId,'Button \"'+(btn.dataset.slotLabel||slot)+'\" is unmapped. Click Edit Layout.');return;}try{btn.classList.add('sending');irStatus(devId,'Sending '+cmd+'...');if(wsConnected){try{await sendWsCommand('send_command',{deviceId:devId,command:cmd});irStatus(devId,'Sent '+cmd);return;}catch(_e){}}const j=await postJson('/api/ir-send',{deviceId:devId,command:cmd});irStatus(devId,'Sent '+cmd+(j.reply?': '+String(j.reply).slice(0,140):''));}catch(err){irStatus(devId,'Send failed: '+(err.message||err));}finally{setTimeout(()=>btn.classList.remove('sending'),200);}});});}"
        "document.querySelectorAll('[data-remote-b25-skin]').forEach(img=>{img.onerror=()=>{if(!img.dataset.fallback){img.dataset.fallback='1';img.src='https://raw.githubusercontent.com/iranl/harmony-hub-control/main/docs/assets/remote_b25_skin.jpg';}};img.src=REMOTE_B25_SKIN_SRC;});"
        "document.querySelectorAll('[data-remote-elite-skin]').forEach(img=>{img.onerror=()=>{if(!img.dataset.fallback){img.dataset.fallback='1';img.src='https://raw.githubusercontent.com/iranl/harmony-hub-control/main/docs/assets/remote_elite_skin.jpg';}};img.src=REMOTE_ELITE_SKIN_SRC;});"
        "function showView(name){let panel='';if(name&&name.startsWith('ir-')){panel=name.slice(3);name='ir';}if(!name)name='overview';if(name==='wifi')name='network';let found=false;document.querySelectorAll('[data-view]').forEach(s=>{const on=s.dataset.view===name;s.classList.toggle('active',on);if(on)found=true;});if(!found&&name!=='overview'){showView('overview');return;}document.querySelectorAll('[data-view-target]').forEach(b=>b.classList.toggle('active',b.dataset.viewTarget===name));if(location.hash!=='#'+name)history.replaceState(null,'','#'+name);if(name==='overview'||name==='mqtt')refreshMqttStatus();if(name==='ir'&&panel)setTimeout(()=>showWizardPanel(panel),0);if(name==='activities')loadActivities();if(name==='remotes'){loadRemoteMappings();refreshBtBackend();}if(name==='rf-remote'){loadEliteMappings();refreshRfStatus();}if(name==='bluetooth')startBtPolling();else stopBtPolling();window.scrollTo(0,0);setTimeout(()=>window.scrollTo(0,0),0);}"
        "document.querySelectorAll('[data-view-target]').forEach(b=>b.addEventListener('click',()=>showView(b.dataset.viewTarget)));"
        "showView((location.hash||'#overview').slice(1));setTimeout(loadActivities,60);setTimeout(refreshMqttStatus,100);initWebUiWs();initWebRemoteLayouts();setupWebRemoteEvents();"
        "const importFile=$('importFile');if(importFile){importFile.addEventListener('change',()=>{const file=importFile.files&&importFile.files[0];if(!file)return;const reader=new FileReader();reader.onload=()=>{const box=document.querySelector('textarea[name=payload]');if(box)box.value=reader.result||''};reader.readAsText(file);});}"
        "const cap=$('captureNow');if(cap){cap.addEventListener('click',async()=>{const s=$('captureStatus'),raw=$('captureRaw');try{s.textContent='capturing...';const r=await fetch('/api/capture',{method:'POST'});const j=await r.json();raw.value=j.raw||'';s.textContent=j.raw?'capture received':'no capture payload';}catch(e){s.textContent='capture failed';}});}"
        "function plainText(html){return String(html||'').replace(/<script[\\s\\S]*?<\\/script>/gi,'').replace(/<style[\\s\\S]*?<\\/style>/gi,'').replace(/<[^>]+>/g,' ').replace(/\\s+/g,' ').trim();}"
        "function escHtml(s){return String(s||'').replace(/[&<>\"']/g,c=>c==='&'?'&amp;':c==='<'?'&lt;':c==='>'?'&gt;':c==='\"'?'&quot;':'&#39;');}"
        "function wizStatus(id,t){const el=$(id);if(el)el.textContent=t||'';}"
        "function showWizardPanel(name){document.querySelectorAll('.wizard-panel').forEach(p=>p.classList.toggle('active',p.dataset.panel===name));document.querySelectorAll('.step').forEach(b=>b.classList.toggle('active',b.dataset.stepTarget===name));if(name==='library')syncProfileToSearch(false);}"
        "document.querySelectorAll('[data-step-target]').forEach(b=>b.addEventListener('click',()=>showWizardPanel(b.dataset.stepTarget)));"
        "async function postJson(path,data){const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','Accept':'application/json','X-Requested-With':'XMLHttpRequest'},body:new URLSearchParams(data||{})});const t=await r.text();let j;try{j=JSON.parse(t);}catch(e){throw new Error(t||('http '+r.status));}if(!r.ok||j.ok===false)throw new Error(j.message||j.error||('http '+r.status));return j;}"
        "async function postForm(target,extra){if(typeof target==='string')return await postJson(target,extra);const form=target,fd=new FormData(form);if(extra)Object.entries(extra).forEach(([k,v])=>fd.set(k,v));const url=form.getAttribute('action')||window.location.pathname;const r=await fetch(url,{method:form.method||'POST',headers:{'Accept':'application/json','X-Requested-With':'XMLHttpRequest'},body:new URLSearchParams(fd)});const t=await r.text();let j;try{j=JSON.parse(t);}catch(e){throw new Error(t||('http '+r.status));}if(!r.ok||j.ok===false)throw new Error(j.message||j.error||('http '+r.status));return j;}"
        "function keyProtocol(k){return /MemorexO1/i.test(k||'')?'679':'2';}"
        "function extractKeycode(t){const m=String(t||'').match(/G:[^\\r\\n\"'<>]+?:\\d+/);return m?m[0].replace(/,$/,'').trim():'';}"
        "function extractNecHex(t){t=String(t||'').trim();let m=t.match(/^0x([0-9a-f]{1,8})$/i)||t.match(/^([0-9a-f]{1,8})$/i);if(m)return m[1].toUpperCase();m=t.match(/0x([0-9a-f]{8})(?![0-9a-f])/i);if(m)return m[1].toUpperCase();if(/nec|samsung|toshiba|memorex|protocol|keycode/i.test(t)){m=t.match(/(^|[^0-9a-f])([0-9a-f]{8})(?![0-9a-f])/i);if(m)return m[2].toUpperCase();}return '';}"
        "function captureLooksEmpty(t){return !String(t||'').trim()||/no payload|returned no|timeout/i.test(t);}"
        "function applyCaptureAnalysis(j,text){text=String(text||j?.raw||'').trim();if(captureLooksEmpty(text))return false;const mode=$('wizardMode'),proto=$('wizardProtocol'),raw=$('wizardRaw'),key=$('wizardKeycode'),nec=$('wizardNec');if(raw)raw.value=text;let bestMode=j?.mode||'',bestKey=j?.keycode||'',bestNec=j?.nec||'',note=j?.analysis||'';if(!bestKey)bestKey=extractKeycode(text);if(!bestNec)bestNec=extractNecHex(text);if(bestKey){if(key)key.value=bestKey;if(proto)proto.value=String(j?.protocolId||keyProtocol(bestKey));if(mode)mode.value='keycode';wizStatus('wizardLearnStatus',note||'decoded a compact Harmony code');return true;}if(bestNec){if(nec)nec.value=bestNec;if(key)key.value='';if(proto)proto.value=String(j?.protocolId||keyProtocol(text));if(mode)mode.value='nec';wizStatus('wizardLearnStatus',note||'decoded an NEC-style code');return true;}if(mode)mode.value='raw';wizStatus('wizardLearnStatus',note||'captured timing data; storing as raw replay');return true;}"
        "function selectedDeviceId(id){const el=$(id);return el?el.value:'';}"
        "function currentCommandsFor(id){const dev=(wizardInventory&&wizardInventory.devices||[]).find(d=>d.id===id);return dev&&dev.commands?dev.commands:[];}"
        "async function loadDeviceCommands(id){if(!id)return[];try{const j=await(await fetch('/api/device-commands?deviceId='+encodeURIComponent(id))).json();if(j.ok&&Array.isArray(j.commands))return j.commands;}catch(e){}return currentCommandsFor(id);}"
        "function populateVerifyCommands(){const dev=selectedDeviceId('verifyDevice')||selectedDeviceId('wizardDevice'),sel=$('verifyCommand');if(!sel)return;sel.replaceChildren();currentCommandsFor(dev).forEach(c=>{const o=document.createElement('option');o.value=c.name;o.textContent=c.name;sel.append(o);});const typed=($('wizardCommandName')?.value||'').trim();if(typed&&!Array.from(sel.options).some(o=>o.value===typed)){const o=document.createElement('option');o.value=typed;o.textContent=typed;sel.prepend(o);sel.value=typed;}}"
        "function syncWizardDevice(from,to){const a=$(from),b=$(to);if(a&&b)b.value=a.value;populateVerifyCommands();}"
        "function profileData(){return{name:($('newDeviceName')?.value||'').trim(),type:($('newDeviceType')?.value||'').trim(),manufacturer:($('newDeviceManufacturer')?.value||'').trim(),model:($('newDeviceModel')?.value||'').trim()};}"
        "function profileQuery(){const d=profileData();return(d.manufacturer&&d.model)?d.manufacturer+' '+d.model:(d.name||d.model||d.manufacturer);}"
        "function syncProfileToSearch(force){const q=profileQuery(),s=$('irdbSearch'),pre=$('irdbPrefix');if(s&&q&&(force||!s.value.trim()))s.value=q;if(pre&&!pre.value.trim())pre.value='';}"
        "function findProfileDeviceId(d){const list=(wizardInventory&&wizardInventory.devices)||[],same=x=>String(x||'').trim().toLowerCase();let dev=list.find(x=>same(x.name)===same(d.name)&&same(x.manufacturer)===same(d.manufacturer)&&same(x.model)===same(d.model));if(!dev)dev=list.find(x=>same(x.manufacturer)===same(d.manufacturer)&&same(x.model)===same(d.model));return dev&&dev.id?dev.id:'';}"
        "function selectDeviceEverywhere(id,name){if(!id)return;['wizardDevice','verifyDevice','irdbDevice','labDevice'].forEach(selId=>{const s=$(selId);if(!s)return;if(!Array.from(s.options).some(o=>o.value===id)){const o=document.createElement('option');o.value=id;o.textContent=(name||'Device')+' ('+id+')';s.append(o);}s.value=id;});populateVerifyCommands();}"
        "function openIrSetup(panel,id){if(id)selectDeviceEverywhere(id);showView('ir-'+(panel||'device'));setTimeout(()=>{if(id)selectDeviceEverywhere(id);document.querySelector('.setup-shell')?.scrollIntoView({behavior:'smooth',block:'start'});},0);}"
        "document.querySelectorAll('[data-next-step]').forEach(b=>b.addEventListener('click',()=>{const p=b.closest('[data-ir-device]');openIrSetup(b.dataset.nextStep,p&&p.dataset.irDevice);}));"
        "const wizDevice=$('wizardDevice');if(wizDevice)wizDevice.addEventListener('change',()=>{syncWizardDevice('wizardDevice','verifyDevice');const imp=$('irdbDevice');if(imp)imp.value=wizDevice.value;});"
        "const verDevice=$('verifyDevice');if(verDevice)verDevice.addEventListener('change',populateVerifyCommands);"
        "const wizName=$('wizardCommandName');if(wizName)wizName.addEventListener('input',populateVerifyCommands);"
        "const wizNec=$('wizardNec');if(wizNec)wizNec.addEventListener('input',()=>{if(wizNec.value.trim()&&$('wizardMode'))$('wizardMode').value='nec';});"
        "const wizKey=$('wizardKeycode');if(wizKey)wizKey.addEventListener('input',()=>{if(wizKey.value.trim()&&$('wizardMode'))$('wizardMode').value='keycode';});"
        "const wizRaw=$('wizardRaw');if(wizRaw)wizRaw.addEventListener('input',()=>{const m=$('wizardMode');if(wizRaw.value.trim()&&m&&m.value!=='auto')m.value='raw';});"
        "const wizCap=$('wizardCapture');if(wizCap){wizCap.addEventListener('click',async()=>{try{wizStatus('wizardLearnStatus','listening...');const r=await postJson('/api/capture',{});let txt=(r.raw||r.message||'').trim();if(!applyCaptureAnalysis(r,txt))wizStatus('wizardLearnStatus','no signal received');}catch(e){wizStatus('wizardLearnStatus','capture failed: '+(e.message||e));}});}"
        "function learnFormData(){return{deviceId:selectedDeviceId('wizardDevice'),name:($('wizardCommandName')?.value||'').trim(),mode:$('wizardMode')?.value||'raw',protocol:$('wizardProtocol')?.value||'2',nec:$('wizardNec')?.value||'',keycode:$('wizardKeycode')?.value||'',raw:$('wizardRaw')?.value||''};}"
        "function learnHasSignal(d){return!!((d.keycode||'').trim()||(d.nec||'').trim()||(d.raw||'').trim());}"
        "const wizLearnTest=$('wizardLearnTest');if(wizLearnTest){wizLearnTest.addEventListener('click',async()=>{const data=learnFormData();if(!data.deviceId){wizStatus('wizardLearnStatus','choose a device before testing');return;}if(!learnHasSignal(data)){wizStatus('wizardLearnStatus','learn or enter a signal before testing');return;}try{wizStatus('wizardLearnStatus','testing learned signal...');const j=await postJson('/api/ir-test-learned',data);const tail=String(j.reply||'').trim();wizStatus('wizardLearnStatus','test sent'+(tail?': '+tail.slice(0,180):''));await loadWizardInventory();}catch(e){wizStatus('wizardLearnStatus','test failed: '+(e.message||e));}});}"
        "const wizForm=$('wizardLearnForm');if(wizForm){wizForm.addEventListener('submit',async e=>{e.preventDefault();const data=learnFormData();try{wizStatus('wizardLearnStatus','saving...');const res=await postJson('/ir/command',data);wizStatus('wizardLearnStatus',res.message||'command saved');wizardInventory=null;await loadWizardInventory();const vd=$('verifyDevice');if(vd)vd.value=data.deviceId;populateVerifyCommands();}catch(err){wizStatus('wizardLearnStatus','save failed: '+(err.message||err));}});}"
        "const wizTest=$('wizardTest');if(wizTest){wizTest.addEventListener('click',async()=>{const data={deviceId:selectedDeviceId('verifyDevice')||selectedDeviceId('wizardDevice'),command:$('verifyCommand')?.value||($('wizardCommandName')?.value||'')};if(!data.deviceId||!data.command){wizStatus('wizardVerifyStatus','choose a command');return;}try{wizStatus('wizardVerifyStatus','sending...');const res=await postJson('/api/ir-send',data);wizStatus('wizardVerifyStatus',res.message||res.reply||'test sent');}catch(e){wizStatus('wizardVerifyStatus','test failed: '+(e.message||e));}});}"
        "loadWizardInventory();"
        "function irStatus(deviceId,msg){document.querySelectorAll('[data-ir-status]').forEach(el=>{if(el.dataset.irStatus===deviceId)el.textContent=msg;});}"
        "function showIrStoredDevice(id){if(!id)return;document.querySelectorAll('[data-ir-device-pick]').forEach(b=>b.classList.toggle('active',b.dataset.irDevicePick===id));document.querySelectorAll('[data-ir-device]').forEach(p=>p.classList.toggle('active',p.dataset.irDevice===id));selectDeviceEverywhere(id);irStatus(id,'Ready.');}"
        "document.querySelectorAll('[data-ir-device-pick]').forEach(b=>b.addEventListener('click',()=>showIrStoredDevice(b.dataset.irDevicePick)));"
        "function irFormData(form){const data={};new FormData(form).forEach((v,k)=>data[k]=v);return data;}"
        "document.querySelectorAll('.ir-send-form').forEach(form=>form.addEventListener('submit',async e=>{e.preventDefault();const data=irFormData(form),btn=form.querySelector('button')||form;if(!data.deviceId||!data.command)return;try{btn.classList.add('sending');form.classList.add('sending');irStatus(data.deviceId,'Sending '+data.command+'...');if(wsConnected){try{await sendWsCommand('send_command',{deviceId:data.deviceId,command:data.command});irStatus(data.deviceId,'Sent '+data.command);return;}catch(_e){}}const j=await postJson('/api/ir-send',data);irStatus(data.deviceId,'Sent '+data.command+(j.reply?': '+String(j.reply).slice(0,140):''));}catch(err){irStatus(data.deviceId,'Send failed: '+(err.message||err));}finally{setTimeout(()=>{btn.classList.remove('sending');form.classList.remove('sending');},220);}}));"
        "document.querySelectorAll('.ir-command-filter').forEach(input=>input.addEventListener('input',()=>{const id=input.dataset.commandFilter,q=normText(input.value||'');document.querySelectorAll('[data-command-list=\"'+id+'\"]').forEach(list=>Array.from(list.children).forEach(row=>{const t=normText(row.textContent||row.dataset.commandName||'');row.classList.toggle('hidden',!!q&&!t.includes(q));}));}));"
        "const IRDB_BASE='https://cdn.jsdelivr.net/gh/probonopd/irdb@master/codes/';"
        "const FLIPPER_BASE='https://cdn.jsdelivr.net/gh/Lucaslhm/Flipper-IRDB@main/';"
        "const FLIPPER_INDEX='https://api.github.com/repos/Lucaslhm/Flipper-IRDB/git/trees/main?recursive=1';"
        "const LIRC_BASE='https://raw.githubusercontent.com/probonopd/lirc-remotes/master/';const LIRC_INDEX='https://api.github.com/repos/probonopd/lirc-remotes/git/trees/master?recursive=1';"
        "const SMARTIR_BASE='https://raw.githubusercontent.com/smartHomeHub/SmartIR/master/';const SMARTIR_INDEX='https://api.github.com/repos/smartHomeHub/SmartIR/git/trees/master?recursive=1';let irdbIndex=[],irdbCache={irdb:null,flipper:null,lirc:null,smartir:null};"
        "function rev8(v){v=((v&240)>>4)|((v&15)<<4);v=((v&204)>>2)|((v&51)<<2);v=((v&170)>>1)|((v&85)<<1);return v&255;}"
        "function keyFromParts(proto,d,s,f){if(!/^(NEC|Samsung32|Pioneer)/i.test(proto))return '';d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?(d^255):Number(s);f=Number(f);if([d,s,f].some(x=>!Number.isFinite(x)||x<0||x>255))return '';if(/^Samsung32/i.test(proto))s=d;const val=((rev8(d)<<24)|(rev8(s)<<16)|(rev8(f)<<8)|rev8((~f)&255))>>>0;return 'G:Toshiba 32 Bit:(0x'+val.toString(16).toUpperCase().padStart(8,'0')+')(Repeat)():3';}"
        "function csvCells(line){const out=[];let cur='',q=false;for(let i=0;i<line.length;i++){const ch=line[i];if(ch==='\"'){if(q&&line[i+1]==='\"'){cur+='\"';i++;}else q=!q;}else if(ch===','&&!q){out.push(cur);cur='';}else cur+=ch;}out.push(cur);return out.map(x=>x.trim());}"
        "function csvEntries(t){return t.replace(/\\r/g,'').split('\\n').map(x=>x.trim()).filter(Boolean).map(line=>{const p=csvCells(line),proto=p[1]||'',key=p[0]==='functionname'?'':keyFromParts(proto,p[2]||'',p[3]||'',p[4]||''),raw=p[0]==='functionname'?'':csvProtocolRaw(proto,p[2]||'',p[3]||'',p[4]||'',p[0]||'');return{name:p[0]||'',meta:proto+' '+(p[2]||'')+','+(p[3]||'')+','+(p[4]||''),protocol:proto,keycode:key,raw:raw};}).filter(r=>r.name&&r.name!=='functionname');}"
        "function hexBytes(v){return String(v||'').trim().split(/\\s+/).filter(Boolean).map(x=>parseInt(x,16)||0);}"
        "function hexValue(v){return hexBytes(v).slice(0,4).reduce((a,b,i)=>a|((b&255)<<(8*i)),0)>>>0;}"
        "function harmonyRawFromTimings(freq,vals){freq=Math.max(10000,Math.min(60000,Math.round(Number(freq)||38000)));vals=(vals||[]).map(v=>Math.max(1,Math.min(0xfffff,Math.round(Math.abs(Number(v)||0))))).filter(Boolean);if(vals.length===3)vals.push(100000);if(vals.length<4)return'';let raw='F'+freq.toString(16).toUpperCase();vals.forEach((v,i)=>{raw+=(i%2?'S':'P')+v.toString(16).toUpperCase();});return raw.length<=4096?raw:'';}"
        "function pulse(seq,level,dur){dur=Math.round(dur);if(dur<=0)return;const last=seq[seq.length-1];if(last&&last.level===level)last.dur+=dur;else seq.push({level:level,dur:dur});}"
        "function seqRaw(freq,seq){if(!seq.length)return'';if(seq[0].level===0)seq.unshift({level:1,dur:1});return harmonyRawFromTimings(freq,seq.map(x=>x.dur));}"
        "function manchester(seq,bits,half,doubleIndex){bits.forEach((bit,i)=>{const h=i===doubleIndex?half*2:half;if(bit){pulse(seq,1,h);pulse(seq,0,h);}else{pulse(seq,0,h);pulse(seq,1,h);}});}"
        "function msbBits(v,n){const a=[];for(let i=n-1;i>=0;i--)a.push((v>>i)&1);return a;}"
        "function lsbBits(v,n){const a=[];for(let i=0;i<n;i++)a.push((v>>i)&1);return a;}"
        "function pop8(v){v&=255;let n=0;while(v){n+=v&1;v>>=1;}return n;}"
        "function rc5Raw(cur){const addr=hexValue(cur.address)&31,cmd=hexValue(cur.command)&127,tog=hexValue(cur.toggle)&1,seq=[];const bits=[1,cmd<64?1:0,tog].concat(msbBits(addr,5),msbBits(cmd&63,6));manchester(seq,bits,889,-1);return seqRaw(36000,seq);}"
        "function rc6Raw(cur){const addr=hexValue(cur.address)&255,cmd=hexValue(cur.command)&255,tog=hexValue(cur.toggle)&1,seq=[];pulse(seq,1,2666);pulse(seq,0,889);const bits=[1,0,0,0,tog].concat(msbBits(addr,8),msbBits(cmd,8));manchester(seq,bits,444,4);return seqRaw(36000,seq);}"
        "function mceRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?15:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<0||d>127||s<0||s>255||f<0||f>255)return'';const seq=[],bits=[1,1,1,0,0].concat(msbBits(128,8),msbBits(s,8),[0],msbBits(d,7),msbBits(f,8));pulse(seq,1,2664);pulse(seq,0,888);manchester(seq,bits,444,4);pulse(seq,0,100000);return seqRaw(36000,seq);}"
        "function recs80Raw(d,s,f,name=''){d=Number(d);f=Number(f);const t=/\\bT1\\b/i.test(String(name||''))?1:0;if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>7||f<0||f>63)return'';const seq=[];pulse(seq,1,158);pulse(seq,0,7432);[t].concat(msbBits(d,3),msbBits(f,6)).forEach(b=>{pulse(seq,1,158);pulse(seq,0,b?7432:4902);});pulse(seq,1,158);pulse(seq,0,45000);return seqRaw(38000,seq);}"
        "function akaiRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>7||f<0||f>127)return'';const seq=[];lsbBits(d,3).concat(lsbBits(f,7),[1]).forEach(b=>{pulse(seq,1,289);pulse(seq,0,Math.round((b?6.3:2.6)*289));});pulse(seq,0,25300);return seqRaw(38000,seq);}"
        "function denonRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>31||f<0||f>255)return'';const seq=[],u=264;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*7:u*3);}function half(func,suffix){lsbBits(d,5).concat(lsbBits(func,8),lsbBits(suffix,2)).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*165);}half(f,0);half((~f)&255,3);return seqRaw(38000,seq);}"
        "function denonKRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<0||d>15||s<0||s>15||f<0||f>4095)return'';const seq=[],u=432,c=((d<<4)^s^((f<<4)&255)^((f>>4)&255))&255;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*3:u);}pulse(seq,1,u*8);pulse(seq,0,u*4);lsbBits(84,8).concat(lsbBits(50,8),lsbBits(0,4),lsbBits(d,4),lsbBits(s,4),lsbBits(f,12),lsbBits(c,8)).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*173);return seqRaw(37000,seq);}"
        "function mitsubishiRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>255||f<0||f>255)return'';const seq=[],u=300;lsbBits(d,8).concat(lsbBits(f,8)).forEach(b=>{pulse(seq,1,u);pulse(seq,0,b?u*7:u*3);});pulse(seq,1,u);pulse(seq,0,u*80);return seqRaw(32600,seq);}"
        "function vellemanRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>7||f<0||f>63)return'';const seq=[];[1,0].concat(msbBits(d,3),msbBits(f,6),[1]).forEach(b=>{pulse(seq,1,700);pulse(seq,0,b?7590:5060);});pulse(seq,0,55000);return seqRaw(38000,seq);}"
        "function fujitsuRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<0||d>255||s<0||s>255||f<0||f>255)return'';const seq=[],u=432;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*3:u);}pulse(seq,1,u*8);pulse(seq,0,u*4);lsbBits(20,8).concat(lsbBits(99,8),lsbBits(0,4),lsbBits(0,4),lsbBits(d,8),lsbBits(s,8),lsbBits(f,8)).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*110);return seqRaw(37000,seq);}"
        "function sharpRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>31||f<0||f>255)return'';const seq=[],u=264;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*7:u*3);}function half(func,suffix){lsbBits(d,5).concat(lsbBits(func,8),lsbBits(suffix,2)).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*165);}half(f,1);half((~f)&255,2);return seqRaw(38000,seq);}"
        "function directvRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>15||f<0||f>255)return'';const c=(7*((f>>6)&3)+5*((f>>4)&3)+3*((f>>2)&3)+(f&3))&15,seq=[],u=600,bits=msbBits(d,4).concat(msbBits(f,8),msbBits(c,4));pulse(seq,1,u*10);pulse(seq,0,u*2);for(let i=0;i<bits.length;i+=2){const v=(bits[i]<<1)|bits[i+1];pulse(seq,1,(v&2)?u*2:u);pulse(seq,0,(v&1)?u*2:u);}pulse(seq,1,u);pulse(seq,0,u*50);return seqRaw(38000,seq);}"
        "function gxbRaw(d,s,f){d=Number(d);f=Number(f);const ss=String(s===undefined?'':s).trim();const p=(ss&&ss!=='-1')?(Number(ss)&1):0;if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>15||f<0||f>255)return'';const seq=[],u=520;pulse(seq,1,u);pulse(seq,0,u);msbBits(d,4).concat(msbBits(f,8),[p]).forEach(b=>{pulse(seq,1,b?u*3:u);pulse(seq,0,b?u:u*3);});pulse(seq,1,u);pulse(seq,0,60000);return seqRaw(38300,seq);}"
        "function giCableRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>15||f<0||f>255)return'';const seq=[],u=490,c=(-(d+(f&15)+((f>>4)&15)))&15;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*9:u*4.5);}pulse(seq,1,u*18);pulse(seq,0,u*9);lsbBits(f,8).concat(lsbBits(d,4),lsbBits(c,4)).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*84);pulse(seq,1,u*18);pulse(seq,0,u*4.5);pulse(seq,1,u);pulse(seq,0,u*178);return seqRaw(38700,seq);}"
        "function protonRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>255||f<0||f>255)return'';const seq=[],u=500;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*3:u);}pulse(seq,1,u*16);pulse(seq,0,u*8);lsbBits(d,8).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*8);lsbBits(f,8).forEach(bit);pulse(seq,1,u);pulse(seq,0,63000);return seqRaw(38000,seq);}"
        "function f12Raw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<0||d>7||s<0||s>1||f<0||f>255)return'';const seq=[],u=422,bits=lsbBits(d,3).concat([s&1],lsbBits(f,8));function bit(b){pulse(seq,1,b?u*3:u);pulse(seq,0,b?u:u*3);}function part(){bits.forEach(bit);pulse(seq,0,u*34);bits.forEach(bit);}part();if(s&1){pulse(seq,0,u*88);part();}return seqRaw(37900,seq);}"
        "function nokiaRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(![d,s,f].every(x=>Number.isFinite(x)&&x>=0&&x<=255))return'';const seq=[],bits=msbBits(d,8).concat(msbBits(s,8),msbBits(f,8));pulse(seq,1,412);pulse(seq,0,276);for(let i=0;i<bits.length;i+=2){const v=(bits[i]<<1)|bits[i+1];pulse(seq,1,164);pulse(seq,0,[276,445,614,783][v]);}pulse(seq,1,164);pulse(seq,0,100000);return seqRaw(36000,seq);}"
        "function nokia12Raw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>15||f<0||f>255)return'';const seq=[],bits=msbBits(d,4).concat(msbBits(f,8));pulse(seq,1,412);pulse(seq,0,276);for(let i=0;i<bits.length;i+=2){const v=(bits[i]<<1)|bits[i+1];pulse(seq,1,164);pulse(seq,0,[276,445,614,783][v]);}pulse(seq,1,164);pulse(seq,0,60000);return seqRaw(36000,seq);}"
        "function nrc17Raw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>255||f<0||f>255)return'';const seq=[],u=500;function frame(cmd,addr,gap){pulse(seq,1,u);pulse(seq,0,u*5);manchester(seq,[1].concat(lsbBits(cmd,8),lsbBits(addr,8)),u,-1);pulse(seq,0,gap);}frame(254,255,u*28);frame(f,d,u*220);frame(254,255,u*200);return seqRaw(38000,seq);}"
        "function streamZapRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>63||f<0||f>127)return'';const seq=[],bits=[1,((~f)>>6)&1,0].concat(msbBits(d,6),msbBits(f&63,6));manchester(seq,bits,889,-1);pulse(seq,0,114000);return seqRaw(36000,seq);}"
        "function logitechRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>15||f<0||f>255)return'';const seq=[],u=127;function bit(b){pulse(seq,1,u*3);pulse(seq,0,b?u*8:u*4);}pulse(seq,1,u*31);pulse(seq,0,u*36);lsbBits(d,4).concat(lsbBits((~d)&15,4),lsbBits(f,8),lsbBits((~f)&255,8)).forEach(bit);pulse(seq,1,u*3);pulse(seq,0,50000);return seqRaw(38000,seq);}"
        "function sircRaw(cur,proto){const cmd=hexValue(cur.command)&127,addr=hexValue(cur.address),bits=/20/.test(proto)?20:(/15/.test(proto)?15:12),addrBits=bits-7,seq=[];pulse(seq,1,2400);pulse(seq,0,600);lsbBits(cmd,7).concat(lsbBits(addr,addrBits)).forEach(b=>{pulse(seq,1,b?1200:600);pulse(seq,0,600);});return seqRaw(40000,seq);}"
        "function jvcRaw(d,f){d=Number(d);f=Number(f);if(![d,f].every(x=>Number.isFinite(x)&&x>=0&&x<=255))return'';const seq=[];pulse(seq,1,8400);pulse(seq,0,4200);lsbBits(d,8).concat(lsbBits(f,8)).forEach(b=>{pulse(seq,1,525);pulse(seq,0,b?1575:525);});pulse(seq,1,525);pulse(seq,0,23625);return seqRaw(38000,seq);}"
        "function jvc48Raw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(![d,s,f].every(x=>Number.isFinite(x)&&x>=0&&x<=255))return'';const seq=[],u=432;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*3:u);}pulse(seq,1,u*8);pulse(seq,0,u*4);lsbBits(3,8).concat(lsbBits(1,8),lsbBits(d,8),lsbBits(s,8),lsbBits(f,8),lsbBits((d^s^f)&255,8)).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*173);return seqRaw(37000,seq);}"
        "function konkaRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>255||f<0||f>255)return'';const seq=[],u=500;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*5:u*3);}pulse(seq,1,u*6);pulse(seq,0,u*6);msbBits(d,8).concat(msbBits(f,8)).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*8);pulse(seq,1,u);pulse(seq,0,u*46);return seqRaw(38000,seq);}"
        "function tivoRaw(proto,d,s,f){f=Number(f);const m=String(proto||'').match(/unit\\s*=\\s*(\\d+)/i),u=m?Number(m[1]):0;if(!Number.isFinite(f)||!Number.isFinite(u)||f<0||f>255||u<0||u>15)return'';const seq=[],unit=564;function bit(b){pulse(seq,1,unit);pulse(seq,0,b?unit*3:unit);}pulse(seq,1,unit*16);pulse(seq,0,unit*8);lsbBits(133,8).concat(lsbBits(48,8),lsbBits(f,8),lsbBits(u,4),lsbBits(((~f)>>4)&15,4)).forEach(bit);pulse(seq,1,unit);pulse(seq,0,unit*78);pulse(seq,1,unit*16);pulse(seq,0,unit*4);pulse(seq,1,unit);pulse(seq,0,unit*173);return seqRaw(38400,seq);}"
        "function xmpRaw(proto,d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(![d,s,f].every(x=>Number.isFinite(x))||d<0||d>255||s<0||s>255||f<0||f>65535)return'';const p=String(proto||''),oem=68,full=/XMP-1/i.test(p)?((f&255)<<8):(/XMP-2/i.test(p)?(f&255):(f&65535)),seq=[],u=136,sh=(s>>4)&15,sl=s&15,oh=(oem>>4)&15,ol=oem&15,dh=(d>>4)&15,dl=d&15,fn=[(full>>12)&15,(full>>8)&15,(full>>4)&15,full&15],c1=(-(sh+sl+15+oh+ol+dh+dl))&15;function nib(n){pulse(seq,1,210);pulse(seq,0,760+(n&15)*u);}function frame(t){const c2=(-(sh+t+sl+fn[0]+fn[1]+fn[2]+fn[3]))&15;[sh,c1,sl,15,oh,ol,dh,dl].forEach(nib);pulse(seq,1,210);pulse(seq,0,13800);[sh,c2,t,sl].concat(fn).forEach(nib);pulse(seq,1,210);pulse(seq,0,80400);}frame(0);frame(8);return seqRaw(38000,seq);}"
        "function sharpDvdRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<0||d>15||s<0||s>255||f<0||f>255)return'';const e=1,c=(d^(s&15)^((s>>4)&15)^(f&15)^((f>>4)&15)^e)&15,seq=[],u=400;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*3:u);}pulse(seq,1,u*8);pulse(seq,0,u*4);lsbBits(170,8).concat(lsbBits(90,8),lsbBits(15,4),lsbBits(d,4),lsbBits(s,8),lsbBits(f,8),lsbBits(e,4),lsbBits(c,4)).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*48);return seqRaw(38000,seq);}"
        "function rcaRaw(proto,d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>15||f<0||f>255)return'';const old=/old/i.test(proto||''),freq=/38/.test(proto||'')?38700:58000,seq=[],u=460;if(old)pulse(seq,1,u*32);pulse(seq,1,u*8);pulse(seq,0,u*8);msbBits(d,4).concat(msbBits(f,8),msbBits((~d)&15,4),msbBits((~f)&255,8)).forEach(b=>{pulse(seq,1,u);pulse(seq,0,b?u*4:u*2);});pulse(seq,1,u*(old?2:1));pulse(seq,0,u*16);return seqRaw(freq,seq);}"
        "function panasonicRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(![d,s,f].every(x=>Number.isFinite(x)&&x>=0&&x<=255))return'';const seq=[],bytes=[2,32,d&255,s&255,f&255,(d^s^f)&255];pulse(seq,1,3456);pulse(seq,0,1728);bytes.flatMap(b=>lsbBits(b,8)).forEach(b=>{pulse(seq,1,432);pulse(seq,0,b?1296:432);});pulse(seq,1,432);pulse(seq,0,74400);return seqRaw(37000,seq);}"
        "function panasonic2Raw(d,s,f,x=0){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);x=Number(x);if(![d,s,f,x].every(v=>Number.isFinite(v)&&v>=0&&v<=255))return'';const seq=[],bytes=[2,32,d&255,s&255,x&255,f&255,(d^s^x^f)&255];pulse(seq,1,3456);pulse(seq,0,1728);bytes.flatMap(b=>lsbBits(b,8)).forEach(b=>{pulse(seq,1,432);pulse(seq,0,b?1296:432);});pulse(seq,1,432);pulse(seq,0,74400);return seqRaw(37000,seq);}"
        "function aiwaRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<0||d>255||s<0||s>31||f<0||f>255)return'';const seq=[];pulse(seq,1,8800);pulse(seq,0,4400);lsbBits(d,8).concat(lsbBits(s,5),lsbBits((~d)&255,8),lsbBits((~s)&31,5),lsbBits(f,8),lsbBits((~f)&255,8)).forEach(b=>{pulse(seq,1,550);pulse(seq,0,b?1650:550);});pulse(seq,1,550);pulse(seq,0,23100);pulse(seq,1,8800);pulse(seq,0,4400);pulse(seq,1,550);pulse(seq,0,90750);return seqRaw(38000,seq);}"
        "function panasonicOldRaw(d,s,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>31||f<0||f>63)return'';const seq=[];pulse(seq,1,3332);pulse(seq,0,3332);lsbBits(d,5).concat(lsbBits(f,6),lsbBits((~d)&31,5),lsbBits((~f)&63,6)).forEach(b=>{pulse(seq,1,833);pulse(seq,0,b?2499:833);});pulse(seq,1,833);pulse(seq,0,100000);return seqRaw(57600,seq);}"
        "function nec48Raw(d,s,f,e=0){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?(d^255):Number(s);f=Number(f);e=Number(e);if(![d,s,f,e].every(x=>Number.isFinite(x)&&x>=0&&x<=255))return'';const seq=[];pulse(seq,1,9024);pulse(seq,0,4512);lsbBits(d,8).concat(lsbBits(s,8),lsbBits(f,8),lsbBits((~f)&255,8),lsbBits(e,8),lsbBits((~e)&255,8)).forEach(b=>{pulse(seq,1,564);pulse(seq,0,b?1692:564);});pulse(seq,1,564);pulse(seq,0,108000);pulse(seq,1,9024);pulse(seq,0,2256);pulse(seq,1,564);pulse(seq,0,108000);return seqRaw(38000,seq);}"
        "function blaupunktRaw(d,s,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>7||f<0||f>63)return'';const seq=[];pulse(seq,1,528);pulse(seq,0,2640);manchester(seq,Array(10).fill(1),528,-1);pulse(seq,0,20592);pulse(seq,1,528);pulse(seq,0,2640);manchester(seq,[1].concat(lsbBits(f,6),lsbBits(d,3)),528,-1);pulse(seq,0,121440);return seqRaw(30300,seq);}"
        "function dishRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<0||d>31||s<0||s>31||f<0||f>63)return'';const bits=msbBits(f,6).concat(msbBits(s,5),msbBits(d,5)),seq=[];pulse(seq,1,400);pulse(seq,0,6100);for(let r=0;r<4;r++){bits.forEach(b=>{pulse(seq,1,400);pulse(seq,0,b?1700:2800);});pulse(seq,1,400);pulse(seq,0,6100);}return seqRaw(57600,seq);}"
        "function barcoRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<0||d>127||s<0||s>63||f<0||f>127)return'';const seq=[],u=250;function bit(b){if(b){pulse(seq,1,u);pulse(seq,0,u);}else{pulse(seq,0,u);pulse(seq,1,u);}}pulse(seq,1,u);pulse(seq,0,u);msbBits(d,7).concat(msbBits(s,6),[0,0],msbBits(f,7)).forEach(bit);pulse(seq,0,89000);return seqRaw(55500,seq);}"
        "function thomsonRaw(proto,d,f){d=Number(d);f=Number(f);const seven=/7$/i.test(proto);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>(seven?15:31)||f<0||f>(seven?127:63))return'';const seq=[],u=500;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*9:u*4);}const bits=seven?lsbBits(d,4).concat([0],lsbBits(f,7)):lsbBits(d,4).concat([0],lsbBits(d>>4,1),lsbBits(f,6));bits.concat([1]).forEach(bit);pulse(seq,0,80000);return seqRaw(33000,seq);}"
        "function emersonLikeRaw(proto,d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>63||f<0||f>63)return'';const sc=/^ScAtl-6$/i.test(proto),u=sc?846:872,seq=[];function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*3:u);}pulse(seq,1,u*4);pulse(seq,0,u*4);lsbBits(d,6).concat(lsbBits(f,6),lsbBits((~d)&63,6),lsbBits((~f)&63,6)).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*(sc?40:39));return seqRaw(sc?57600:36700,seq);}"
        "function appleRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?135:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<0||d>255||s<0||s>255||f<0||f>127)return'';const i=0,c=((pop8(f&127)+pop8(i))%2)===0?1:0,seq=[],u=564;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*3:u);}pulse(seq,1,u*16);pulse(seq,0,u*8);lsbBits(d,8).concat(lsbBits(s,8),[c],lsbBits(f,7),lsbBits(i,8)).forEach(bit);pulse(seq,1,u);pulse(seq,0,u*78);pulse(seq,1,u*16);pulse(seq,0,u*4);pulse(seq,1,u);pulse(seq,0,u*173);return seqRaw(38400,seq);}"
        "function nokia32Raw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(![d,s,f].every(x=>Number.isFinite(x)&&x>=0&&x<=255))return'';const seq=[],bits=msbBits(d,8).concat(msbBits(s,8),msbBits(0,8),msbBits(f,8));pulse(seq,1,412);pulse(seq,0,276);for(let i=0;i<bits.length;i+=2){const v=(bits[i]<<1)|bits[i+1];pulse(seq,1,164);pulse(seq,0,[276,445,614,783][v]);}pulse(seq,1,164);pulse(seq,0,100000);return seqRaw(36000,seq);}"
        "function paceMssRaw(d,f){d=Number(d);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(f)||d<0||d>1||f<0||f>255)return'';const seq=[],u=630;function bit(b){pulse(seq,1,u);pulse(seq,0,b?u*11:u*7);}pulse(seq,1,u);pulse(seq,0,u*5);pulse(seq,1,u);pulse(seq,0,u*5);[0,d&1].concat(msbBits(f,8)).forEach(bit);pulse(seq,1,u);pulse(seq,0,120000);return seqRaw(38000,seq);}"
        "function sejinRaw(proto,d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<0||d>255||s<0||s>255||f<0||f>255)return'';const freq=/-38$/i.test(proto)?38800:56300,u=310,dx=d||1,fx=s,fy=f,e=0,c=((dx&15)+((dx>>4)&15)+(fx&15)+((fx>>4)&15)+(fy&15)+((fy>>4)&15)+e)&15,seq=[],bits=msbBits(3,2).concat(msbBits(dx,8),msbBits(fx,8),msbBits(fy,8),msbBits(e,4),msbBits(c,4));function slot(b){pulse(seq,b?1:0,u);}function pair(v){msbBits([8,4,2,1][v&3],4).forEach(slot);}pulse(seq,1,u*3);for(let i=0;i<bits.length;i+=2)pair((bits[i]<<1)|bits[i+1]);pulse(seq,0,76000);return seqRaw(freq,seq);}"
        "function zenithRaw(d,s,f){d=Number(d);const ss=String(s===undefined?'':s).trim();s=(ss===''||ss==='-1')?0:Number(s);f=Number(f);if(!Number.isFinite(d)||!Number.isFinite(s)||!Number.isFinite(f)||d<1||d>12||s<0||s>1||f<0||f>=Math.pow(2,d))return'';const seq=[],u=520;function zbit(b){if(b){pulse(seq,1,u);pulse(seq,0,u);pulse(seq,1,u);pulse(seq,0,u*8);}else{pulse(seq,1,u);pulse(seq,0,u*10);}}zbit(s&1);for(let i=d-1;i>=0;i--){const b=(Math.floor(f/Math.pow(2,i))&1);if(b){zbit(1);zbit(0);}else{zbit(0);zbit(1);}}pulse(seq,0,90000);return seqRaw(40000,seq);}"
        "function csvProtocolRaw(proto,d,s,f,name=''){proto=String(proto||'');const D=Number(d),F=Number(f),S=String(s===undefined?'':s).trim();if(!Number.isFinite(D)||!Number.isFinite(F)||D<0||F<0)return'';if(/^RC5X?/i.test(proto))return rc5Raw({address:D.toString(16),command:F.toString(16),toggle:'0'});if(/^RC6/i.test(proto))return rc6Raw({address:D.toString(16),command:F.toString(16),toggle:'0'});if(/^MCE$/i.test(proto))return mceRaw(D,S,F);if(/^RECS80$/i.test(proto))return recs80Raw(D,S,F,name);if(/^Akai$/i.test(proto))return akaiRaw(D,F);if(/^Denon-K$/i.test(proto))return denonKRaw(D,S,F);if(/^Denon(?:\\{[12]\\})?$/i.test(proto))return denonRaw(D,F);if(/^Mitsubishi$/i.test(proto))return mitsubishiRaw(D,F);if(/^Velleman$/i.test(proto))return vellemanRaw(D,F);if(/^Fujitsu$/i.test(proto))return fujitsuRaw(D,S,F);if(/^SharpDVD$/i.test(proto))return sharpDvdRaw(D,S,F);if(/^Sharp(?:\\{[12]\\})?$/i.test(proto))return sharpRaw(D,F);if(/^DirecTV$/i.test(proto))return directvRaw(D,F);if(/^GXB$/i.test(proto))return gxbRaw(D,S,F);if(/^G\\.I\\.Cable(?:\\{[12]\\})?$/i.test(proto))return giCableRaw(D,F);if(/^Proton$/i.test(proto))return protonRaw(D,F);if(/^F12$/i.test(proto))return f12Raw(D,S,F);if(/^NRC17$/i.test(proto))return nrc17Raw(D,F);if(/^Nokia32$/i.test(proto))return nokia32Raw(D,S,F);if(/^Nokia$/i.test(proto))return nokiaRaw(D,S,F);if(/^Nokia12$/i.test(proto))return nokia12Raw(D,F);if(/^StreamZap$/i.test(proto))return streamZapRaw(D,F);if(/^Logitech$/i.test(proto))return logitechRaw(D,F);if(/^JVC-48$/i.test(proto))return jvc48Raw(D,S,F);if(/^JVC(?:\\{[12]\\})?$/i.test(proto))return jvcRaw(D,F);if(/^Konka$/i.test(proto))return konkaRaw(D,F);if(/^Tivo\\s+unit\\s*=\\s*\\d+$/i.test(proto))return tivoRaw(proto,D,S,F);if(/^XMP(?:-[12])?$/i.test(proto))return xmpRaw(proto,D,S,F);if(/^Apple$/i.test(proto))return appleRaw(D,S,F);if(/^(?:Emerson|ScAtl-6)$/i.test(proto))return emersonLikeRaw(proto,D,F);if(/^PaceMSS$/i.test(proto))return paceMssRaw(D,F);if(/^Sejin-1-(?:38|56)$/i.test(proto))return sejinRaw(proto,D,S,F);if(/^Zenith$/i.test(proto))return zenithRaw(D,S,F);if(/^Barco$/i.test(proto))return barcoRaw(D,S,F);if(/^Thomson7?$/i.test(proto))return thomsonRaw(proto,D,F);if(/^Panasonic2$/i.test(proto))return panasonic2Raw(D,S,F);if(/^Panasonic$/i.test(proto))return panasonicRaw(D,S,F);if(/^Aiwa$/i.test(proto))return aiwaRaw(D,S,F);if(/^Panasonic_Old$/i.test(proto))return panasonicOldRaw(D,S,F);if(/^Dish_Network$/i.test(proto))return dishRaw(D,S,F);if(/^48-NEC1$/i.test(proto))return nec48Raw(D,S,F,0);if(/^Blaupunkt$/i.test(proto))return blaupunktRaw(D,S,F);if(/^RCA(?:-38)?(?:\\(Old\\))?$/i.test(proto))return rcaRaw(proto,D,F);if(/^Sony(12|15|20)?/i.test(proto)){const bits=(proto.match(/Sony(\\d+)/i)||[])[1]||'12';let addr=D;if(bits==='20'&&S&&S!=='-1'){const sub=Number(S);if(Number.isFinite(sub)&&sub>=0)addr=(D&31)|((sub&255)<<5);}return sircRaw({address:addr.toString(16),command:F.toString(16)},'SIRC'+bits);}return'';}"
        "function prontoToHarmonyRaw(hex){const words=String(hex||'').trim().split(/\\s+/).filter(Boolean).map(x=>parseInt(x,16));if(words.length<8||words.some(x=>!Number.isFinite(x)))return'';if(words[0]!==0)return'';const unit=(words[1]||1)*0.241246,freq=Math.round(1000000/unit),pairs=(words[2]||0)+(words[3]||0),vals=words.slice(4,4+pairs*2).map(w=>Math.round(w*unit));return harmonyRawFromTimings(freq,vals);}"
        "function kaseikyoRaw(cur){const a=hexBytes(cur.address),c=hexBytes(cur.command);if(a.length<4||c.length<1)return'';const bytes=[a[1],a[2],a[0],a[3],c[0],(a[0]^a[3]^c[0])&255],seq=[];pulse(seq,1,3456);pulse(seq,0,1728);bytes.flatMap(b=>lsbBits(b,8)).forEach(b=>{pulse(seq,1,432);pulse(seq,0,b?1296:432);});pulse(seq,1,432);pulse(seq,0,74736);return seqRaw(38000,seq);}"
        "function flipperEntries(t,path=''){const out=[],src=String(path||'');let cur={};function push(){if(!cur.name){cur={};return;}let key='',raw='',meta=(cur.protocol||cur.type||'raw');if(String(cur.type||'').toLowerCase()==='parsed'){const a=hexBytes(cur.address),c=hexBytes(cur.command),p=cur.protocol||'';if(/^Samsung32/i.test(p))key=keyFromParts(p,a[0],a[0],c[0]);else if(/^NECext/i.test(p)){key=keyFromParts(p,a[0],a[1],c[0]);if(!key&&/_Converted_\\/CSV\\/D\\/Denon\\//i.test(src)){raw=denonKRaw(a[0],a[1],c[0]);meta=raw?'Denon-K converted timing':'NECext unsupported';}}else if(/^NEC/i.test(p))key=keyFromParts(p,a[0],a[0]^255,c[0]);else if(/^Pioneer/i.test(p))key=keyFromParts(p,a[0],a[0]^255,c[0]);else if(/^RCA/i.test(p)){raw=rcaRaw(p,a[0],c[0]);meta=raw?p+' converted timing':p+' unsupported';}else if(/^RC5/i.test(p)){raw=rc5Raw(cur);meta=raw?'RC5 converted timing':'RC5 unsupported';}else if(/^RC6/i.test(p)){raw=rc6Raw(cur);meta=raw?'RC6 converted timing':'RC6 unsupported';}else if(/^SIRC/i.test(p)){raw=sircRaw(cur,p);meta=raw?p+' converted timing':p+' unsupported';}else if(/^Kaseikyo/i.test(p)){raw=kaseikyoRaw(cur);meta=raw?'Kaseikyo converted timing':'Kaseikyo unsupported';}}else if(String(cur.type||'').toLowerCase()==='raw'){const vals=String(cur.data||'').trim().split(/\\s+/).filter(Boolean).map(Number);raw=harmonyRawFromTimings(cur.frequency||38000,vals);meta='raw timings '+(cur.frequency||38000)+' Hz ('+vals.length+' durations)';}out.push({name:cur.name,meta:meta,keycode:key,raw:raw});cur={};}t.replace(/\\r/g,'').split('\\n').forEach(line=>{line=line.trim();if(!line)return;if(line[0]==='#'){push();return;}const i=line.indexOf(':');if(i<0)return;const k=line.slice(0,i).trim().toLowerCase(),v=line.slice(i+1).trim();if(k==='name'&&cur.name)push();cur[k]=cur[k]&&k==='data'?cur[k]+' '+v:v;});push();return out;}"
        "function base64Bytes(s){try{const bin=atob(String(s||'').replace(/\\s+/g,''));return Array.from(bin,c=>c.charCodeAt(0));}catch(e){return[];}}"
        "function broadlinkKind(s){const b=base64Bytes(s);if(b.length<8)return'';if(b[0]===0x26)return'ir';if(b[0]===0xb2||b[0]===0xd7)return'rf';return'';}"
        "function broadlinkRaw(s){const b=base64Bytes(s);if(b.length<8||b[0]!==0x26)return'';let len=b[2]|(b[3]<<8),end=Math.min(b.length,4+len),vals=[];for(let i=4;i<end;){let v=b[i++];if(v===0){if(i+1>=end)break;v=(b[i]<<8)|b[i+1];i+=2;}vals.push(Math.round(v*269000/8192));}return harmonyRawFromTimings(38000,vals);}"
        "function miioRaw(s){const b=base64Bytes(s);if(b.length<8)return'';let vals=[];if(b[0]===0x67&&b[1]===0xa5&&b.length>=72){const edge=b[2]|(b[3]<<8),pairs=Math.floor((edge+1)/2),times=[];for(let i=0;i<16;i++){const o=4+i*4;times.push((b[o]|(b[o+1]<<8)|(b[o+2]<<16)|(b[o+3]<<24))>>>0);}for(let i=68;i<Math.min(b.length,68+pairs);i++){const p=times[b[i]&15]||0,g=times[(b[i]>>4)&15]||0;vals.push(p,g);}}else if(b[0]===0xa5&&b[1]===0x67){const pairs=Math.floor(((b[2]<<8)+b[3]+1)/2),dataStart=Math.max(4,b.length-pairs),times=[];for(let i=4;i+1<dataStart;i+=2)times.push((b[i]<<8)+b[i+1]);for(let i=dataStart;i<b.length;i++)vals.push(times[b[i]&15]||0,times[(b[i]>>4)&15]||0);}vals=vals.filter(Boolean);return harmonyRawFromTimings(38000,vals);}"
        "function sendirRaw(s){const m=String(s||'').match(/sendir\\s*,\\s*[^,]+\\s*,\\s*\\d+\\s*,\\s*(\\d+)\\s*,\\s*\\d+\\s*,\\s*\\d+\\s*,\\s*([0-9,\\s]+)/i);if(!m)return'';const freq=Math.max(10000,Math.min(60000,parseInt(m[1],10)||38000)),vals=m[2].split(',').map(x=>Math.round((parseInt(x,10)||0)*1000000/freq)).filter(Boolean);return harmonyRawFromTimings(freq,vals);}"
        "function rawTimingRows(t,label){const rows=[];String(t||'').replace(/\\r/g,'').split('\\n').forEach((line,i)=>{let m=line.match(/^\\s*([^:=]+?)\\s*[:=]\\s*(\\[?\\s*(?:[-+]?\\d+[\\s,]+){3,}[-+]?\\d+\\s*\\]?)\\s*$/),body=m?m[2]:String(line||'').trim();if(!m){const bm=body.match(/^\\[?\\s*((?:[-+]?\\d+[\\s,]+){3,}[-+]?\\d+)\\s*\\]?\\s*$/);if(!bm)return;body=bm[1];}body=String(body).replace(/^\\s*\\[/,'').replace(/\\]\\s*$/,'');const vals=body.split(/[\\s,]+/).map(Number).filter(Number.isFinite),raw=harmonyRawFromTimings(38000,vals);if(raw)rows.push({name:m?safeImportName(m[1]):(label||'Raw command')+' '+(i+1),meta:'raw timings 38000 Hz ('+vals.length+' durations)',raw:raw});});return rows;}"
        "function prontoEntries(t){const rows=[],lines=String(t||'').replace(/\\r/g,'').split('\\n'),re=/((?:0000|0100|5000|6000|7000)(?:\\s+[0-9a-fA-F]{4}){6,})/g;lines.forEach((line,i)=>{let m;while((m=re.exec(line))){const hex=m[1].replace(/\\s+/g,' ').trim(),raw=prontoToHarmonyRaw(hex),name=safeImportName(line.slice(0,m.index).trim().replace(/[:=,]+$/,'').trim()||'Pronto '+(i+1));rows.push({name:name,meta:raw?'Pronto Hex converted timing':'Pronto Hex unsupported',raw:raw});}});return rows;}"
        "function codeStringRow(name,s,meta){s=String(s||'').trim();if(!s)return null;if(/^G:[^\\r\\n]+?:\\d+$/i.test(s))return{name:name,meta:meta||'Harmony compact code',keycode:s};if(/^F[0-9A-F]+(?:[PS][0-9A-F]+)+$/i.test(s))return{name:name,meta:meta||'Harmony raw timing',raw:s};let raw=sendirRaw(s);if(raw)return{name:name,meta:'Global Cache sendir converted timing',raw:raw};raw=prontoToHarmonyRaw((s.match(/(?:0000|0100|5000|6000|7000)(?:\\s+[0-9a-fA-F]{4}){6,}/)||[])[0]||'');if(raw)return{name:name,meta:'Pronto Hex converted timing',raw:raw};const bk=broadlinkKind(s);raw=broadlinkRaw(s);if(raw)return{name:name,meta:'BroadLink IR base64 converted timing',raw:raw};if(bk==='rf')return{name:name,meta:'BroadLink RF packet - not an IR command',raw:''};raw=miioRaw(s);if(raw)return{name:name,meta:'Xiaomi Miio raw converted timing',raw:raw};const rr=rawTimingRows(s,name);if(rr[0])rr[0].name=name;return rr[0]||null;}"
        "function lircProp(block,key){const m=String(block||'').match(new RegExp('^\\\\s*'+key+'\\\\s+([^\\\\n]+)','im'));return m?m[1].trim():'';}function lircNums(block,key){return lircProp(block,key).split(/\\s+/).map(Number).filter(Number.isFinite);}"
        "function lircCodeBits(hex,n,rev){hex=String(hex||'').replace(/^0x/i,'')||'0';if(typeof BigInt==='function'){try{const v=BigInt('0x'+hex),one=BigInt(1),a=[];if(rev){for(let i=0;i<n;i++)a.push(Number((v>>BigInt(i))&one));}else{for(let i=n-1;i>=0;i--)a.push(Number((v>>BigInt(i))&one));}return a;}catch(e){}}const v=parseInt(hex,16)||0;return rev?lsbBits(v,n):msbBits(v,n);}"
        "function lircCodeRaw(block,hex){const bits=parseInt(lircProp(block,'bits'),10)||32,preBits=parseInt(lircProp(block,'pre_data_bits'),10)||0,one=lircNums(block,'one'),zero=lircNums(block,'zero'),head=lircNums(block,'header'),ptrail=parseInt(lircProp(block,'ptrail'),10)||0,gap=parseInt(lircProp(block,'gap'),10)||45000,freq=parseInt(lircProp(block,'frequency'),10)||38000,flags=lircProp(block,'flags'),rev=/REVERSE/i.test(flags);if(one.length<2||zero.length<2)return'';let arr=[];if(preBits)arr=arr.concat(lircCodeBits(lircProp(block,'pre_data'),preBits,rev));arr=arr.concat(lircCodeBits(hex,bits,rev));const seq=[];if(head.length>=2){pulse(seq,1,head[0]);pulse(seq,0,head[1]);}if(/SHIFT_ENC/i.test(flags)){const half=Math.max(250,Math.round(((one[0]||889)+(one[1]||889))/2));manchester(seq,arr,half,-1);}else arr.forEach(b=>{pulse(seq,1,b?one[0]:zero[0]);pulse(seq,0,b?one[1]:zero[1]);});if(ptrail)pulse(seq,1,ptrail);pulse(seq,0,gap);return seqRaw(freq,seq);}"
        "function lircUnsupportedReason(block){const driver=lircProp(block,'driver'),one=lircNums(block,'one'),zero=lircNums(block,'zero');if(/irman/i.test(driver)||(one[0]===0&&one[1]===0&&zero[0]===0&&zero[1]===0))return'LIRC IRMan decoded code - no replayable timing data';if(driver&&one.length<2&&zero.length<2)return'LIRC driver decoded code - no replayable timing data';return'LIRC code unsupported';}"
        "function lircEntries(t){const out=[];String(t||'').replace(/\\r/g,'').split(/begin\\s+remote/i).slice(1).forEach((part,bi)=>{const block=(part.split(/end\\s+remote/i)[0]||''),remote=safeImportName(lircProp(block,'name')||'LIRC remote '+(bi+1)),freq=parseInt(lircProp(block,'frequency'),10)||38000,rawSec=(block.match(/begin\\s+raw_codes([\\s\\S]*?)end\\s+raw_codes/i)||[])[1];if(rawSec){let name='',vals=[];function push(){const raw=harmonyRawFromTimings(freq,vals);if(name&&raw)out.push({name:name,meta:'LIRC raw timings '+freq+' Hz ('+vals.length+' durations)',raw:raw});vals=[];}rawSec.split('\\n').forEach(line=>{line=line.trim();if(!line)return;const m=line.match(/^name\\s+(.+)/i);if(m){push();name=safeImportName(m[1]);}else vals=vals.concat(line.split(/\\s+/).map(Number).filter(Number.isFinite));});push();}const codes=(block.match(/begin\\s+codes([\\s\\S]*?)end\\s+codes/i)||[])[1];if(codes){codes.split('\\n').forEach(line=>{line=line.trim();const m=line.match(/^(.+?)\\s+(0x[0-9a-fA-F]+|[0-9a-fA-F]+)\\b/);if(!m)return;const raw=lircCodeRaw(block,m[2]);out.push({name:safeImportName(m[1]),meta:raw?'LIRC '+remote+' rendered timing':lircUnsupportedReason(block),raw:raw});});}});return out;}"
        "function girrRegexEntries(t){const out=[],cmdRe=/<(?:[^:>\\s]+:)?command\\b([^>]*)>([\\s\\S]*?)<\\/(?:[^:>\\s]+:)?command>/gi;let m,i=0;while((m=cmdRe.exec(String(t||'')))){const attrs=m[1]||'',body=m[2]||'',am=attrs.match(/\\b(?:name|id)=[\"']([^\"']+)[\"']/i),name=safeImportName((am&&am[1])||'GIRR '+(++i));['raw','pronto','ccf','sendir'].forEach(tag=>{const re=new RegExp('<(?:[^:>\\\\s]+:)?'+tag+'\\\\b[^>]*>([\\\\s\\\\S]*?)<\\\\/(?:[^:>\\\\s]+:)?'+tag+'>','gi');let tm;while((tm=re.exec(body))){const txt=decodeHtml((tm[1]||'').replace(/<[^>]+>/g,' ')).trim();let row=null;if(tag==='raw'){row=rawTimingRows(txt,name)[0];if(row)row.name=name;}else row=codeStringRow(name,txt,tag==='sendir'?'Global Cache sendir converted timing':'GIRR '+tag+' converted timing');if(row)out.push(row);}});}return out;}"
        "function girrEntries(t){const out=[];try{const doc=new DOMParser().parseFromString(String(t||''),'application/xml');if(doc.querySelector('parsererror'))return girrRegexEntries(t);doc.querySelectorAll('command').forEach((cmd,i)=>{const name=safeImportName(cmd.getAttribute('name')||cmd.getAttribute('id')||'GIRR '+(i+1));['raw','pronto','ccf','sendir'].forEach(tag=>{cmd.querySelectorAll(tag).forEach(el=>{const txt=(el.textContent||'').trim();let row=null;if(tag==='raw'){row=rawTimingRows(txt,name)[0];if(row)row.name=name;}else row=codeStringRow(name,txt,tag==='sendir'?'Global Cache sendir converted timing':'GIRR '+tag+' converted timing');if(row)out.push(row);});});});}catch(e){}return out.length?out:girrRegexEntries(t);}"
        "function jsonEntries(t){let obj;try{obj=JSON.parse(String(t||''));}catch(e){return[];}const out=[];function walk(v,path){const name=safeImportName(path.filter(Boolean).slice(-4).join(' ')||'JSON command');if(typeof v==='string'){const row=codeStringRow(name,v,'JSON code');if(row)out.push(row);return;}if(Array.isArray(v)){if(v.length>=4&&v.every(x=>Number.isFinite(Number(x)))){const raw=harmonyRawFromTimings(38000,v.map(Number));if(raw)out.push({name:name,meta:'JSON raw timing array',raw:raw});}else v.forEach((x,i)=>walk(x,path.concat(String(i+1))));return;}if(v&&typeof v==='object'){const label=v.name||v.command||v.button||v.key||v.label;['code','data','raw','pronto','prontoHex','sendir','broadlink','base64','command'].forEach(k=>{if(typeof v[k]==='string'){const row=codeStringRow(safeImportName(label||name),v[k],'JSON '+k);if(row)out.push(row);}});Object.keys(v).forEach(k=>walk(v[k],path.concat(k)));}}walk(obj,[]);return out;}"
        "function genericCsvEntries(t){const lines=String(t||'').replace(/\\r/g,'').split('\\n').filter(x=>x.trim());if(!lines.length)return[];const head=csvCells(lines[0]).map(x=>x.toLowerCase());if(head[0]==='functionname')return csvEntries(t);const nameIdx=head.findIndex(x=>/^(name|button|command|function|key)$/.test(x)),codeIdx=head.findIndex(x=>/(code|raw|pronto|sendir|broadlink|base64|data)/.test(x));if(nameIdx<0||codeIdx<0)return csvEntries(t);return lines.slice(1).map((line,i)=>{const c=csvCells(line);return codeStringRow(safeImportName(c[nameIdx]||'CSV '+(i+1)),c[codeIdx],'CSV code');}).filter(Boolean);}"
        "function ircEntries(t){const rows=[];String(t||'').replace(/\\r/g,'').split('\\n').forEach((line,i)=>{const m=line.match(/^\\s*([^:=,]+?)\\s*[:=,]\\s*(.+)$/);const row=codeStringRow(safeImportName(m?m[1]:'IRC '+(i+1)),m?m[2]:line,'IRC code');if(row)rows.push(row);});return rows;}"
        "function dedupeCommandRows(rows){const seen=new Set(),names=new Set(),out=[];(rows||[]).forEach(r=>{if(!(r&&r.name))return;const supported=!!(r.raw||r.keycode),fp=(supported?(r.raw?'raw:'+r.raw:'key:'+r.keycode):'unsupported:'+(r.meta||'')+':'+r.name).replace(/\\s+/g,'').toLowerCase();if(seen.has(fp))return;seen.add(fp);let base=safeImportName(r.name),name=base,n=2;while(names.has(name.toLowerCase()))name=(base+' '+(n++)).slice(0,96);names.add(name.toLowerCase());out.push({...r,name:name});});return out;}"
        "function parseIrText(t,source,path){t=String(t||'');let rows=[];const low=String(path||'').toLowerCase(),hint=String(source||'').toLowerCase();if(hint==='flipper'||/\\.ir$/.test(low)||/^Filetype:\\s*IR/i.test(t))rows=rows.concat(flipperEntries(t,path));if(hint==='irdb'||/\\.csv$/.test(low))rows=rows.concat(genericCsvEntries(t));if(hint==='lirc'||/begin\\s+remote/i.test(t)||/\\.lirc|\\.lircd|\\.conf/.test(low))rows=rows.concat(lircEntries(t));if(hint==='smartir'||/\\.json$/.test(low)||/^\\s*[\\[{]/.test(t))rows=rows.concat(jsonEntries(t));if(hint==='remotecentral'||/<html|Copy to Clipboard|Infrared Hex/i.test(t))rows=rows.concat(remoteCentralCommandEntries(t));if(/<\\?xml|<girr|<command/i.test(t))rows=rows.concat(girrEntries(t));const structured=/^(flipper|irdb|lirc|smartir|remotecentral)$/.test(hint)||/^Filetype:\\s*IR/i.test(t)||/begin\\s+remote/i.test(t)||/^\\s*[\\[{]/.test(t);if(hint==='custom'||!structured||!rows.length)rows=rows.concat(prontoEntries(t),ircEntries(t),rawTimingRows(t,'Raw command'));return dedupeCommandRows(rows);}"
        "function irdbStatus(t){const s=$('irdbStatus');if(s)s.textContent=t;}"
        "function irdbLog(t){const l=$('irdbLog');if(!l)return;let p=l.textContent||'';if(/^Ready\\./.test(p))p='';l.textContent=(t?new Date().toLocaleTimeString()+'  '+t+'\\n':'')+p.slice(0,6000);}"
        "function clearIrdPreview(){const p=$('irdbPayload'),box=$('irdbPreview');if(p)p.value='';if(box){box.replaceChildren();box.classList.add('hidden');}}"
        "function safeImportName(s){s=String(s||'Command').replace(/[|\"\\\\\\r\\n]/g,' ').replace(/\\s+/g,' ').trim().slice(0,96);return s||'Command';}"
        "function sourceLabel(s){return({irdb:'probonopd/irdb',flipper:'Flipper-IRDB',lirc:'LIRC remotes',smartir:'SmartIR JSON',remotecentral:'RemoteCentral',custom:'Pasted file or URL',stored:'Saved device'}[s]||s||'Unknown source');}"
        "function harmonyQueryParts(q){q=String(q||'').trim();let m=q.match(/^([^,]+),\\s*(.+)$/);if(m)return{man:m[1].trim(),model:m[2].trim()};const p=q.split(/\\s+/);return{man:p.shift()||'',model:p.join(' ')||''};}"
        "function decodeHtml(s){const e=document.createElement('textarea');e.innerHTML=String(s||'');return e.value;}"
        "function rcSlug(s){return normText(String(s||'').replace(/&/g,' ')).replace(/\\s+/g,'_').replace(/^_+|_+$/g,'');}"
        "function rcNormalizePath(href,base){href=String(href||'').trim();try{if(/^https?:/i.test(href)){const u=new URL(href);href=u.pathname;}}catch(e){}if(!href||href[0]==='#'||href[0]==='?')return'';if(href[0]==='/')return href;return String(base||'/cgi-bin/codes/').replace(/\\/?$/,'/')+href.replace(/^\\.\\//,'');}"
        "let rcProxyBlockedUntil=0;async function rcFetch(path){path=rcNormalizePath(path,'/cgi-bin/codes/');try{const r=await fetch('/api/remotecentral-fetch?path='+encodeURIComponent(path));const j=await r.json();if(j.ok)return j.html||'';if(!/http 30[1278]/i.test(j.error||''))throw new Error(j.error||'RemoteCentral fetch failed');}catch(e){if(/invalid RemoteCentral/i.test(e.message||''))throw e;}if(Date.now()<rcProxyBlockedUntil)throw new Error('RemoteCentral reader temporarily unavailable; try again shortly');const proxy='https://r.jina.ai/http://www.remotecentral.com'+path,ctl=new AbortController(),timer=setTimeout(()=>ctl.abort(),9000);let pr;try{pr=await fetch(proxy,{headers:{Accept:'text/plain'},signal:ctl.signal});}catch(e){throw new Error(e&&e.name==='AbortError'?'RemoteCentral reader timed out':'RemoteCentral reader request failed');}finally{clearTimeout(timer);}if(!pr.ok){if(pr.status===429)rcProxyBlockedUntil=Date.now()+120000;throw new Error('RemoteCentral reader http '+pr.status);}return await pr.text();}"
        "function rcPageLinks(html,brand){const base='/cgi-bin/codes/'+brand+'/',seen=new Set(),out=[],re=/<a\\s+[^>]*href=[\"']([^\"']+)[\"'][^>]*>([\\s\\S]*?)<\\/a>/gi;let m;while((m=re.exec(html))){const path=rcNormalizePath(m[1],base);if(path.startsWith(base)&&/\\/page-\\d+\\/?$/i.test(path)&&!seen.has(path)){seen.add(path);out.push(path);}}return out.slice(0,5);}"
        "function scoreRcMatch(label,q){const toks=queryTokens(q),n=normText(label);if(!toks.length)return 0;let score=0;for(const t of toks){if(!n.includes(t))return -1;score+=n.split(/\\s+/).includes(t)?45:18;}if(n.includes(normText(q)))score+=80;return score-Math.min(n.length/24,20);}"
        "function rcPushModelLink(out,seen,path,title,base,q,weak){path=rcNormalizePath(path,base);title=plainText(title);if(!path.startsWith(base)||path===base||/\\/page-\\d+\\/?$/i.test(path)||!title||seen.has(path))return;let score=scoreRcMatch(title+' '+path,q),browse=false;if(score<0){if(!weak)return;score=8;browse=true;}seen.add(path);out.push({source:'remotecentral',path:path,title:title,meta:browse?'RemoteCentral category page':'RemoteCentral model page',score:score,browseFallback:browse});}"
        "function rcModelLinks(html,brand,q,weak){const base='/cgi-bin/codes/'+brand+'/',seen=new Set(),out=[];let m,re=/<a\\s+[^>]*href=[\"']([^\"']+)[\"'][^>]*>([\\s\\S]*?)<\\/a>/gi;while((m=re.exec(html)))rcPushModelLink(out,seen,m[1],m[2],base,q,weak);re=/\\[([^\\]\\n]{1,180})\\]\\(((?:https?:\\/\\/(?:www\\.)?remotecentral\\.com)?[^)\\s]+)\\)/gi;while((m=re.exec(html)))rcPushModelLink(out,seen,m[2],m[1],base,q,weak);return out;}"
        "async function remoteCentralEntries(q){const parts=harmonyQueryParts(q),brand=rcSlug(parts.man),model=(parts.model||q).trim();if(!brand||!model||normText(model).length<3)return[];let first;try{first=await rcFetch('/cgi-bin/codes/'+brand+'/');}catch(e){return[];}const pages=[first],links=rcPageLinks(first,brand);for(const p of links){try{pages.push(await rcFetch(p));}catch(e){}}let rows=pages.flatMap(h=>rcModelLinks(h,brand,model,false));if(!rows.length)rows=pages.flatMap(h=>rcModelLinks(h,brand,model,true)).slice(0,24);return rows.sort((a,b)=>b.score-a.score||a.title.length-b.title.length).slice(0,80);}"
        "function rcCleanCommandName(s){s=decodeHtml(String(s||'')).replace(/\\(\\s*Copy\\s+to\\s+Clipboard\\s*\\)/ig,'').replace(/[|\"\\\\]/g,'').replace(/\\s+/g,' ').trim();if(!s||/^(Image|Return|Remote Model|Infrared Hex|This model|Features|Hex Codes|Page:|Copyright|Home|News|Reviews|Files|Forums)$/i.test(s))return'';return s.slice(0,96);}"
        "function rcCommandName(lines,i,prefix){let n=rcCleanCommandName(prefix);if(n)return n;for(let j=i-1;j>=0&&j>=i-8;j--){n=rcCleanCommandName(lines[j]);if(n&&!/^[0-9a-f]{4}\\s/i.test(n))return n;}return 'Command '+(i+1);}"
        "function remoteCentralCommandEntries(html){let text=String(html||'').replace(/<script[\\s\\S]*?<\\/script>/gi,'').replace(/<style[\\s\\S]*?<\\/style>/gi,'').replace(/<br\\s*\\/?\\s*>/gi,'\\n').replace(/<\\/(td|tr|p|div|li)>/gi,'\\n').replace(/<[^>]+>/g,' ');text=decodeHtml(text).replace(/\\(Copy to Clipboard\\)/ig,'\\n(Copy to Clipboard)\\n');const lines=text.replace(/\\r/g,'').split('\\n').map(x=>x.replace(/\\s+/g,' ').trim()).filter(Boolean),rows=[];const pronto=/((?:0000|0100|5000|6000|7000)(?:\\s+[0-9a-fA-F]{4}){10,})/g;lines.forEach((line,i)=>{let m;while((m=pronto.exec(line))){const hex=m[1].replace(/\\s+/g,' ').trim(),raw=prontoToHarmonyRaw(hex);rows.push({name:rcCommandName(lines,i,line.slice(0,m.index)),meta:raw?'Pronto converted raw ('+hex.split(' ').length+' words)':'Pronto unsupported format',raw:raw});}});return rows;}"
        "function normText(s){return String(s||'').toLowerCase().replace(/[^a-z0-9]+/g,' ').trim();}"
        "function queryTokens(s){return normText(s).split(/\\s+/).filter(Boolean);}"
        "function scoreIrdPath(entry,q){if(entry.source==='remotecentral'&&entry.browseFallback)return entry.score||1;const label=(entry.title||'')+' '+entry.path+' '+(entry.meta||''),toks=queryTokens(q),n=normText(label),parts=n.split(/\\s+/).filter(Boolean);if(!toks.length)return 0;let score=0;for(const t of toks){if(!n.includes(t))return -1;if(parts.includes(t))score+=40;else if(parts.some(p=>p.startsWith(t)))score+=22;else score+=10;}const nq=normText(q);if(n.includes(nq))score+=80;if(label.toLowerCase().includes(q.toLowerCase()))score+=30;if(entry.source==='flipper')score+=4;if(entry.source==='irdb')score+=2;score-=Math.min(label.length/16,25);return score;}"
        "function categoryHints(q){const n=normText(q),p=harmonyQueryParts(q),man=normText(p.man),model=normText(p.model),h=[];function add(){[...arguments].forEach(x=>{if(!h.includes(x))h.push(x);});}if(/\\b(tv|television|hdtv|plasma|oled|qled|qned|webos|roku|bravia|viera|4k|8k)\\b/.test(n)||(man==='lg'&&/^(?:[a-z]?\\d{1,2}|[cbgmz]\\d)\\b/.test(model)))add('tv','tvs','television','plasma','oled');if(/\\b(ac|air|conditioner|climate|heat|cool)\\b/.test(n))add('ac','acs','air','conditioner');if(/\\b(sound ?bar|speaker|receiver|avr|amplifier|audio)\\b/.test(n))add('sound','soundbar','soundbars','receiver','audio','speaker');if(/\\b(blu ?ray|dvd|player|disc)\\b/.test(n))add('blu','ray','dvd','player');if(/\\b(projector|beamer)\\b/.test(n))add('projector','projectors');return h;}"
        "function scoreIrdFallback(entry,q){const label=(entry.title||'')+' '+entry.path+' '+(entry.meta||''),n=normText(label),parts=n.split(/\\s+/).filter(Boolean),qp=harmonyQueryParts(q),man=normText(qp.man);if(!man||!n.includes(man))return-1;let score=18;if(parts.includes(man))score+=38;else if(parts.some(p=>p.startsWith(man)))score+=20;const hints=categoryHints(q);let hintHits=0;hints.forEach(h=>{if(n.includes(h)){score+=24;hintHits++;}});queryTokens(qp.model).filter(t=>t.length>=3).forEach(t=>{if(n.includes(t))score+=45;});if(hints.length&&!hintHits)score-=12;if(entry.source==='flipper')score+=4;if(entry.source==='irdb')score+=2;score-=Math.min(label.length/22,18);return score;}"
        "function rankIrdEntries(entries,q){let rows=dedupeIrdEntries((entries||[]).map(r=>({...r,score:scoreIrdPath(r,q)})).filter(r=>r.score>=0)).sort((a,b)=>b.score-a.score||String(a.path).length-String(b.path).length);rows.fallback=false;if(q&&rows.length===0){rows=dedupeIrdEntries((entries||[]).map(r=>({...r,score:scoreIrdFallback(r,q)})).filter(r=>r.score>=0)).sort((a,b)=>b.score-a.score||String(a.path).length-String(b.path).length);rows.fallback=rows.length>0;}return rows;}"
        "function dedupeIrdEntries(rows){const seen=new Set(),out=[],dupes=[];(rows||[]).forEach(r=>{const key=(r.source||'')+'|'+String(r.path||'').toLowerCase();if(!r.path)return;if(seen.has(key)){dupes.push(r);return;}seen.add(key);out.push(r);});out.dupes=dupes.length;return out;}"
        "function sourceBreakdown(rows){const c={};(rows||[]).forEach(r=>{const k=sourceLabel(r.source||'irdb');c[k]=(c[k]||0)+1;});return Object.entries(c).sort((a,b)=>b[1]-a[1]||a[0].localeCompare(b[0])).map(x=>x[0]+' '+x[1]).join(', ');}"
        "async function jsonOk(url){const r=await fetch(url);if(!r.ok)throw new Error('http '+r.status);return r.json();}"
        "async function loadIrdSource(src){if(irdbCache[src])return irdbCache[src];if(src==='flipper'){const j=await jsonOk(FLIPPER_INDEX),files=j.tree||j.files||[];irdbCache[src]=files.filter(x=>!x.type||x.type==='blob').map(x=>(x.path||x.name||x).replace(/^\\//,'')).filter(x=>x.endsWith('.ir')).map(path=>({source:src,path:path}));}else if(src==='lirc'){let paths;try{const j=await jsonOk(LIRC_INDEX);paths=(j.tree||[]).filter(x=>x.type==='blob').map(x=>(x.path||'').replace(/^\\//,''));}catch(e){const j=await jsonOk('https://data.jsdelivr.com/v1/package/gh/probonopd/lirc-remotes@master/flat');paths=(j.files||[]).map(x=>(x.name||'').replace(/^\\//,''));}irdbCache[src]=paths.filter(x=>x&&x!=='README.md'&&!/\\.(png|jpg|jpeg|gif|md|html)$/i.test(x)).map(path=>({source:src,path:path}));}else if(src==='smartir'){let paths;try{const j=await jsonOk(SMARTIR_INDEX);paths=(j.tree||[]).filter(x=>x.type==='blob').map(x=>(x.path||'').replace(/^\\//,''));}catch(e){const j=await jsonOk('https://data.jsdelivr.com/v1/package/gh/smartHomeHub/SmartIR@master/flat');paths=(j.files||[]).map(x=>(x.name||'').replace(/^\\//,''));}irdbCache[src]=paths.filter(x=>/^codes\\/.+\\.json$/i.test(x)).map(path=>({source:src,path:path}));}else{const r=await fetch(IRDB_BASE+'index');if(!r.ok)throw new Error('http '+r.status);const t=await r.text();irdbCache[src]=t.replace(/\\r/g,'').split('\\n').map(x=>x.trim()).filter(x=>x.endsWith('.csv')).map(path=>({source:src,path:path}));}return irdbCache[src];}"
        "async function loadIrdSources(names,logFn){const settled=await Promise.all(names.map(s=>loadIrdSource(s).then(rows=>({s:s,rows:rows})).catch(e=>({s:s,error:e}))));const missed=settled.filter(x=>x.error).map(x=>x.s+' '+(x.error.message||x.error));if(missed.length&&logFn)logFn('source index skipped: '+missed.join('; '));return settled.filter(x=>!x.error).flatMap(x=>x.rows);}"
        "async function loadIrdIndexes(){const src=$('irdbSource')?.value||'all',want=src==='all'?['irdb','flipper','lirc','smartir']:(src==='remotecentral'||src==='custom'?[]:[src]);irdbIndex=await loadIrdSources(want,irdbLog);if(want.length&&!irdbIndex.length)throw new Error('no database indexes loaded');return irdbIndex;}"
        "function updateIrdPayload(){const p=$('irdbPayload'),pre=$('irdbPrefix');if(!p)return;const prefix=pre?pre.value:'';p.value=Array.from(document.querySelectorAll('#irdbPreview input[type=checkbox]:checked')).map(cb=>{const name=prefix+cb.dataset.name;if(cb.dataset.mode==='raw')return name+'|raw|'+(cb.dataset.raw||'');return name+'|'+cb.dataset.keycode;}).join('\\n');}"
        "function renderIrdRows(rows){const box=$('irdbPreview');if(!box)return;box.replaceChildren();let ok=0,raw=0,key=0,unsupported={};if(!rows.length){const d=document.createElement('div');d.className='muted mini';d.textContent='No importable commands found on this page. Try another model page or learn the command manually.';box.append(d);box.classList.remove('hidden');updateIrdPayload();irdbStatus('no importable commands found');irdbLog('parsed 0 importable commands');return;}rows.forEach(r=>{const name=safeImportName(r.name),label=document.createElement('label'),cb=document.createElement('input'),span=document.createElement('span'),supported=!!(r.keycode||r.raw);cb.type='checkbox';cb.checked=supported;cb.disabled=!supported;cb.dataset.name=name;cb.dataset.keycode=r.keycode||'';cb.dataset.raw=r.raw||'';cb.dataset.mode=r.raw?'raw':'keycode';span.textContent=name+' / '+(r.meta||'unknown')+(supported?'':' / unsupported');label.append(cb,span);box.append(label);if(supported){ok++;if(r.raw)raw++;else key++;}else{const m=r.meta||'unknown';unsupported[m]=(unsupported[m]||0)+1;}});box.classList.remove('hidden');box.querySelectorAll('input').forEach(x=>x.addEventListener('change',updateIrdPayload));updateIrdPayload();const bad=rows.length-ok;irdbStatus(ok+' importable commands from '+rows.length+' rows ('+key+' compact, '+raw+' timing, '+bad+' unsupported)');irdbLog('parsed '+rows.length+' rows: '+ok+' importable ('+key+' compact, '+raw+' timing), '+bad+' unsupported');const top=Object.entries(unsupported).sort((a,b)=>b[1]-a[1]).slice(0,4).map(x=>x[0]+' x'+x[1]).join(', ');if(top)irdbLog('unsupported protocols: '+top);}"
        "function renderIrdResults(rows){const box=$('irdbResults'),dl=$('irdbMatches');if(dl)dl.replaceChildren();if(!box)return;box.replaceChildren();if(!rows.length){box.classList.add('hidden');return;}rows.slice(0,100).forEach(r=>{const label=r.title? r.title+' - '+r.path:r.path;if(dl){const o=document.createElement('option');o.value=r.path;o.label=sourceLabel(r.source);dl.append(o);}const b=document.createElement('button');b.type='button';b.className='match';b.innerHTML='<strong>'+escHtml(sourceLabel(r.source))+'</strong> '+escHtml(label)+'<div class=\"queue-meta\">score '+Math.round(r.score||0)+' - '+escHtml(r.path)+'</div>';b.addEventListener('click',()=>pickIrdResult(r,true));box.append(b);});box.classList.remove('hidden');}"
        "function updateIrdMatches(){const q=$('irdbSearch')?.value||'',path=$('irdbPath'),rows=rankIrdEntries(irdbIndex,q);renderIrdResults(rows);if(path&&irdbIndex.some(r=>r.path===q)){const r=irdbIndex.find(r=>r.path===q);pickIrdResult(r,false);}else if(q&&rows.length){if(path&&!path.value){path.value=rows[0].path;path.dataset.source=rows[0].source;}if(rows.fallback)irdbLog('no exact match for '+JSON.stringify(q)+'; showing broader manufacturer/category matches');irdbStatus((rows.fallback?'no exact model match; showing ':'found ')+rows.length+' candidate files'+(rows.dupes?' after '+rows.dupes+' duplicate paths skipped':'')+' ('+sourceBreakdown(rows)+')');}else if(q){irdbStatus('no matching files');}}"
        "function pickSourceForPath(path){const p=$('irdbPath'),forced=p?.dataset.source,src=$('irdbSource')?.value||'all',low=String(path||'').toLowerCase();if(forced)return forced;if(src!=='all')return src;if(low.includes('remotecentral.com/cgi-bin/codes/')||low.startsWith('/cgi-bin/codes/'))return'remotecentral';if(low.includes('lirc-remotes')||/\\.(conf|lircd|lirc)$/.test(low))return'lirc';if(low.includes('smarthomehub/smartir')||/codes\\/.+\\.json$/.test(low))return'smartir';return low.endsWith('.ir')?'flipper':(low.endsWith('.csv')?'irdb':'custom');}"
        "function pickIrdResult(r,fetchNow){const p=$('irdbPath');if(p){p.value=r.path;p.dataset.source=r.source;}irdbStatus('selected '+sourceLabel(r.source)+' '+r.path);if(fetchNow)fetchIrdPath();}"
        "async function runIrdSearch(){try{const src=$('irdbSource')?.value||'all',q=$('irdbSearch')?.value||'';clearIrdPreview();irdbStatus('searching libraries...');irdbLog('search '+(q.trim()?JSON.stringify(q.trim()):'all files')+' in '+src);await loadIrdIndexes();irdbLog('loaded '+irdbIndex.length+' local database file entries ('+sourceBreakdown(irdbIndex)+')');if((src==='all'||src==='remotecentral')&&q.trim()){try{const rr=await remoteCentralEntries(q);irdbIndex=irdbIndex.concat(rr);irdbLog('RemoteCentral candidates: '+rr.length);}catch(e){irdbStatus('RemoteCentral search skipped: '+(e.message||e));irdbLog('RemoteCentral skipped: '+(e.message||e));}}irdbIndex=dedupeIrdEntries(irdbIndex);if(irdbIndex.dupes)irdbLog('skipped '+irdbIndex.dupes+' duplicate candidate paths');updateIrdMatches();if(!q.trim())irdbStatus('loaded '+irdbIndex.length+' library files ('+sourceBreakdown(irdbIndex)+')');}catch(e){irdbStatus('library search failed: '+(e.message||e));irdbLog('search failed: '+(e.message||e));}}"
        "const loadIndex=$('irdbLoadIndex');if(loadIndex){loadIndex.addEventListener('click',()=>{syncProfileToSearch(false);runIrdSearch();});}"
        "async function searchFromProfile(saveFirst){const d=profileData(),q=profileQuery();if(!q){irdbStatus('enter manufacturer and model first');return;}try{if(saveFirst){wizStatus('profileStatus','saving device...');await loadWizardInventory();let id=findProfileDeviceId(d);if(!id){await postForm('/ir/new-device',d);await loadWizardInventory();id=findProfileDeviceId(d);}selectDeviceEverywhere(id,d.name);wizStatus('profileStatus',id?'device saved and selected':'device saved; choose it from the device list');}const src=$('irdbSource'),s=$('irdbSearch');if(src)src.value='all';if(s)s.value=q;const p=$('irdbPath'),r=$('irdbResults');if(p){p.value='';delete p.dataset.source;}if(r){r.replaceChildren();r.classList.add('hidden');}clearIrdPreview();showWizardPanel('library');await runIrdSearch();}catch(e){wizStatus('profileStatus','device save failed: '+(e.message||e));}}"
        "const profileSearch=$('profileSearch');if(profileSearch){profileSearch.addEventListener('click',()=>searchFromProfile(false));}"
        "const profileForm=$('profileDeviceForm');if(profileForm){profileForm.addEventListener('submit',async e=>{e.preventDefault();const d=profileData();if(!d.name||!d.manufacturer||!d.model){wizStatus('profileStatus','enter name, manufacturer, and model first');return;}wizStatus('profileStatus','creating device...');try{const res=await postForm(profileForm);wizStatus('profileStatus',res.message||'device created');wizardInventory=null;await loadWizardInventory();if(res.deviceId)selectDeviceEverywhere(res.deviceId,d.name);setTimeout(()=>{wizStatus('profileStatus','');showWizardPanel('library');},1200);}catch(err){wizStatus('profileStatus','create failed: '+(err.message||err));}});}"
        "const search=$('irdbSearch');if(search){search.addEventListener('input',()=>{if(irdbIndex.length)updateIrdMatches();});search.addEventListener('change',()=>{if(irdbIndex.length)updateIrdMatches();});}"
        "const source=$('irdbSource');if(source)source.addEventListener('change',()=>{irdbIndex=[];syncProfileToSearch(false);const p=$('irdbPath'),s=$('irdbSearch'),r=$('irdbResults');if(p){p.value='';delete p.dataset.source;}if(r){r.replaceChildren();r.classList.add('hidden');}clearIrdPreview();if(s&&s.value.trim())runIrdSearch();else irdbStatus('choose what to search for');});"
        "const prefix=$('irdbPrefix');if(prefix)prefix.addEventListener('input',updateIrdPayload);"
        "async function fetchIrdPath(){const pathBox=$('irdbPath');let path=(pathBox?.value||'').trim(),source=pickSourceForPath(path);if(!path){irdbStatus('choose a file, page, URL, or paste codes below');return;}try{clearIrdPreview();irdbLog('fetch '+sourceLabel(source)+' '+path);if(source==='remotecentral'){irdbStatus('fetching RemoteCentral Pronto hex...');const html=await rcFetch(path);const rows=parseIrText(html,source,path);irdbLog('downloaded RemoteCentral page, '+html.length+' bytes');renderIrdRows(rows);return;}irdbStatus('fetching commands...');let url=path;if(!/^https?:/i.test(url)){url=url.replace(/^\\//,'');if(source==='flipper')url=FLIPPER_BASE+url;else if(source==='lirc')url=LIRC_BASE+url;else if(source==='smartir')url=SMARTIR_BASE+url;else url=IRDB_BASE+url.replace(/^codes\\//,'');}const r=await fetch(url);if(!r.ok)throw new Error('http '+r.status);const text=await r.text();irdbLog('downloaded '+text.length+' bytes from '+sourceLabel(source));renderIrdRows(parseIrText(text,source,path));}catch(e){irdbStatus('command fetch failed: '+(e.message||e));irdbLog('fetch failed: '+(e.message||e));}}"
        "const pathBox=$('irdbPath');if(pathBox)pathBox.addEventListener('input',()=>{delete pathBox.dataset.source;});"
        "const fetchBtn=$('irdbFetch');if(fetchBtn){fetchBtn.addEventListener('click',fetchIrdPath);}"
        "const irdbFile=$('irdbFile');if(irdbFile){irdbFile.addEventListener('change',()=>{const file=irdbFile.files&&irdbFile.files[0];if(!file)return;const reader=new FileReader();reader.onload=()=>{const p=$('irdbPaste');if(p)p.value=reader.result||'';irdbStatus('loaded file '+file.name+'; click Parse Pasted Codes');};reader.readAsText(file);});}"
        "const parsePaste=$('irdbParsePaste');if(parsePaste){parsePaste.addEventListener('click',()=>{const text=$('irdbPaste')?.value||'',path=$('irdbFile')?.files?.[0]?.name||$('irdbPath')?.value||'pasted codes',source=pickSourceForPath(path);clearIrdPreview();if(!text.trim()){irdbStatus('paste a code file or choose a file first');return;}const rows=parseIrText(text,source,path);irdbLog('parsed pasted/file input as '+sourceLabel(source)+': '+rows.length+' commands');renderIrdRows(rows);});}"
        "const UPDATE_NAMES=['codex_daemon','codex_btstack','codex_sntp','codex_portal','codex_dhcpd'];const UPDATE_API='https://api.github.com/repos/Ripthulhu/harmony-hub-control/contents/payload/bin/';const UPDATE_DEFAULT_RAW='https://raw.githubusercontent.com/Ripthulhu/harmony-hub-control/main/payload/bin/';const UPDATE_CDN='https://cdn.jsdelivr.net/gh/Ripthulhu/harmony-hub-control@main/payload/bin/';const UPDATE_CDN_FAST='https://fastly.jsdelivr.net/gh/Ripthulhu/harmony-hub-control@main/payload/bin/';const UPDATE_AUTO_INTERVAL=6*60*60*1000;const HEX=Array.from({length:256},(_,i)=>i.toString(16).padStart(2,'0'));"
        "function updateLog(t){const el=$('updateLog');if(el)el.textContent=t||'';}function updateAppend(t){const el=$('updateLog');if(!el)return;let p=el.textContent||'';if(/^Ready\\./.test(p))p='';el.textContent=(p?p+'\\n':'')+String(t||'');el.scrollTop=el.scrollHeight;}"
        "function updateBase(){let b=($('updateRepo')?.value||'').trim()||UPDATE_DEFAULT_RAW;return b.endsWith('/')?b:b+'/';}function updateToken(){return($('updateToken')?.value||'').trim();}function updateUsesDefaultRepo(){return updateBase().toLowerCase()===UPDATE_DEFAULT_RAW.toLowerCase();}function updateBases(){const a=[updateBase()];if(updateUsesDefaultRepo())a.push(UPDATE_CDN,UPDATE_CDN_FAST);return Array.from(new Set(a.map(x=>x.endsWith('/')?x:x+'/')));}"
        "function parseUpdateManifest(t){return String(t||'').replace(/\\r/g,'').split('\\n').map(line=>{const m=line.match(/^([0-9a-fA-F]{32})\\s+(\\S+)$/);return m?{md5:m[1].toLowerCase(),name:m[2]}:null;}).filter(x=>x&&UPDATE_NAMES.includes(x.name));}"
        "async function updateFetchUrl(label,url,asText){const ctrl=new AbortController(),timer=setTimeout(()=>ctrl.abort(),25000);try{const r=await fetch(url,{cache:'no-store',signal:ctrl.signal});if(!r.ok)throw new Error('http '+r.status);return asText?await r.text():await r.arrayBuffer();}catch(e){throw new Error(label+' '+((e&&e.name)==='AbortError'?'timeout':(e&&e.message?e.message:e)));}finally{clearTimeout(timer);}}"
        "async function updateFetch(name,token){const errs=[],asText=name==='MANIFEST.txt';if(token||updateUsesDefaultRepo()){try{const headers={'Accept':'application/vnd.github+json'};if(token)headers.Authorization='Bearer '+token;const r=await fetch(UPDATE_API+encodeURIComponent(name)+'?ref=main&t='+Date.now(),{headers:headers,cache:'no-store'});if(!r.ok)throw new Error('http '+r.status+([401,403,404].includes(r.status)?' - token required for private repos or exhausted anonymous API access':''));const j=await r.json(),bin=atob(String(j.content||'').replace(/\\s/g,'')),u=new Uint8Array(bin.length);for(let i=0;i<bin.length;i++)u[i]=bin.charCodeAt(i);return asText?new TextDecoder().decode(u):u.buffer;}catch(e){errs.push('GitHub API '+(e.message||e));if(token)throw new Error(errs.join('; '));}}for(const base of updateBases()){try{return await updateFetchUrl(base.replace(/^https?:\\/\\//,''),base+encodeURIComponent(name)+'?t='+Date.now(),asText);}catch(e){errs.push(e.message||String(e));}}throw new Error(errs.join('; ')||'no update source configured');}"
        "function updateAgeText(ts){const d=Math.max(0,Date.now()-Number(ts||0)*1000),m=Math.floor(d/60000),h=Math.floor(d/3600000),days=Math.floor(d/86400000);if(!ts)return'not checked';if(days>0)return'checked '+days+'d ago';if(h>0)return'checked '+h+'h ago';if(m>0)return'checked '+m+'m ago';return'checked just now';}"
        "function updateDashboardBadge(st){const b=$('dashUpdateBadge'),d=$('dashUpdateDetail');if(!b||!d)return;st=st||{};const checked=Number(st.checkedAt||0),changes=Number(st.changes||0);b.classList.remove('ok','warn','bad');if(!checked){b.classList.add('warn');b.textContent='not checked';d.textContent='Checks automatically while this page is open.';return;}if(changes<0){b.classList.add('bad');b.textContent='check failed';}else if(st.available){const n=changes||1;b.classList.add('warn');b.textContent=n+' update'+(n===1?'':'s');}else{b.classList.add('ok');b.textContent='current';}d.textContent=updateAgeText(checked)+'. '+(st.message||'');}"
        "async function updateLoadState(){try{const r=await fetch('/api/update-check-state',{cache:'no-store'});if(!r.ok)throw new Error('http '+r.status);const j=await r.json();updateDashboardBadge(j);return j;}catch(e){return null;}}"
        "async function updateSaveState(st){st=st||{};try{await postJson('/api/update-check-state',{checkedAt:String(st.checkedAt||Math.floor(Date.now()/1000)),available:st.available?'1':'0',changes:String(st.changes||0),message:st.message||'',source:st.source||updateBase()});updateDashboardBadge(st);}catch(e){}}"
        "function chunkHex(bytes,start,end){let s='';for(let i=start;i<end;i++)s+=HEX[bytes[i]];return s;}async function updateLocalStatus(){const j=await(await fetch('/api/update-status')).json();const lines=['Local control binaries:'];(j.files||[]).forEach(f=>lines.push((f.present?'ok ':'missing ')+f.name+' '+(f.md5||'')+' '+(f.size||0)+' bytes'));updateLog(lines.join('\\n'));return j;}"
        "async function updateCheckRepo(silent){try{if(!silent)updateLog('checking repository...');const token=updateToken(),manifest=await updateFetch('MANIFEST.txt',token),entries=parseUpdateManifest(manifest),local=await(await fetch('/api/update-status')).json(),byName={};(local.files||[]).forEach(f=>byName[f.name]=f);let changes=0;const lines=['Repository files:'];entries.forEach(e=>{const cur=byName[e.name]||{},same=cur.md5&&cur.md5.toLowerCase()===e.md5;changes+=same?0:1;lines.push((same?'current ':'update  ')+e.name+' repo '+e.md5+' local '+(cur.md5||'missing'));});const msg=changes?changes+' file(s) need update.':'Already current.';lines.push(msg);const st={checkedAt:Math.floor(Date.now()/1000),available:changes>0,changes:changes,message:msg,source:updateBase()};await updateSaveState(st);if(!silent)updateLog(lines.join('\\n'));return st;}catch(e){const msg='update check failed: '+(e.message||e),st={checkedAt:Math.floor(Date.now()/1000),available:false,changes:-1,message:msg,source:updateBase()};await updateSaveState(st);if(!silent)updateLog(msg);return st;}}"
        "async function updateInstallRepo(){try{const token=updateToken();updateLog('loading manifest...');const manifest=await updateFetch('MANIFEST.txt',token),entries=parseUpdateManifest(manifest);if(!entries.length)throw new Error('manifest has no updateable codex binaries');const local=await(await fetch('/api/update-status')).json(),byName={};(local.files||[]).forEach(f=>byName[f.name]=f);const todo=entries.filter(e=>!(byName[e.name]&&String(byName[e.name].md5||'').toLowerCase()===e.md5));if(!todo.length){updateLog('Already current.');await updateSaveState({checkedAt:Math.floor(Date.now()/1000),available:false,changes:0,message:'Already current.',source:updateBase()});return;}await postJson('/api/update-begin',{manifest:manifest});updateLog('staging '+todo.length+' file(s)...');for(const e of todo){updateAppend('fetch '+e.name);const buf=await updateFetch(e.name,token),bytes=new Uint8Array(buf);let off=0;while(off<bytes.length){const end=Math.min(off+24576,bytes.length),hex=chunkHex(bytes,off,end);await postJson('/api/update-chunk',{file:e.name,offset:String(off),hex:hex});off=end;updateAppend('  '+e.name+' '+off+' / '+bytes.length);} }const j=await postJson('/api/update-apply',{restart:'1'});updateAppend('installed: '+j.updated);updateAppend('backup: '+j.backupDir);updateAppend('services are restarting; refresh in about 5 seconds.');await updateSaveState({checkedAt:Math.floor(Date.now()/1000),available:false,changes:0,message:'Update installed; services are restarting.',source:updateBase()});setTimeout(updateLocalStatus,6000);}catch(e){updateAppend('update failed: '+(e.message||e));}}"
        "const updateRefresh=$('updateRefresh');if(updateRefresh)updateRefresh.addEventListener('click',updateLocalStatus);const updateCheck=$('updateCheck');if(updateCheck)updateCheck.addEventListener('click',()=>updateCheckRepo(false));const updateInstall=$('updateInstall');if(updateInstall)updateInstall.addEventListener('click',updateInstallRepo);if($('updateLog'))updateLocalStatus();updateLoadState().then(st=>{const last=Number(st&&st.checkedAt||0)*1000;if(!last||Date.now()-last>UPDATE_AUTO_INTERVAL)updateCheckRepo(true);});setInterval(()=>updateCheckRepo(true),UPDATE_AUTO_INTERVAL);"
        "function btAppend(t){const el=$('btLog');if(!el)return;let p=el.textContent||'';if(/^Ready\\./.test(p))p='';if(p.length>6000)p=p.slice(-5000);el.textContent=(p?p+'\\n':'')+String(t||'');el.scrollTop=el.scrollHeight;}"
        "const btClearLogBtn=$('btClearLogBtn');if(btClearLogBtn)btClearLogBtn.addEventListener('click',()=>{const el=$('btLog');if(el)el.textContent='Ready.';});"
        "function renderBtDevicesList(devs,hosts){const el=$('btDevicesList');if(!el)return;if(!devs||!devs.length){el.innerHTML=\"<div class='muted mini'>No Bluetooth devices in inventory.</div>\";return;}let h=\"<div style='display:grid;gap:8px;'>\";devs.forEach(d=>{const isConn=!!d.connected,hasAddr=!!(d.bdaddr&&d.bdaddr.length);h+=\"<div style='display:flex;align-items:center;justify-content:space-between;gap:12px;padding:8px 12px;background:var(--wash);border:1px solid var(--line);border-radius:6px;flex-wrap:wrap;'>\";h+=\"<div><strong>\"+(d.name||d.id)+\"</strong>\";if(d.model||d.manufacturer)h+=\" <span class='muted mini'>(\"+(d.manufacturer?d.manufacturer+' ':'')+(d.model||'')+\")</span>\";h+=\"<div class='muted mini' style='margin-top:2px;'>\";if(hasAddr)h+=\"MAC: <code>\"+d.bdaddr+\"</code>\";else h+=\"<span style='color:var(--warn);'>No Bluetooth MAC paired yet</span>\";h+=\" &bull; ID: \"+d.id+\"</div></div>\";h+=\"<div style='display:flex;align-items:center;gap:8px;'>\";if(isConn){h+=\"<span class='pill ok mini'>Connected</span>\";h+=\"<button type='button' class='danger mini' onclick=\\\"btDisconnectHost('\"+d.bdaddr+\"')\\\">Disconnect</button>\";}else if(hasAddr){h+=\"<span class='pill mini'>Standby</span>\";h+=\"<button type='button' class='mini' onclick=\\\"btConnectHost('\"+d.bdaddr+\"')\\\">Connect</button>\";}else{h+=\"<span class='pill warn mini'>Unpaired</span>\";}h+=\"</div></div>\";});h+=\"</div>\";el.innerHTML=h;}"
        "async function btConnectHost(addr){try{btAppend('connecting to host '+addr+'...');await postJson('/api/bt-connect',{bdaddr:addr});await pollBtStatus();}catch(e){btAppend('connect failed: '+(e.message||e));}}"
        "async function btDisconnectHost(addr){if(!confirm('Disconnect Bluetooth host '+addr+'?'))return;try{btAppend('disconnecting host '+addr+'...');await postJson('/api/bt-disconnect',{bdaddr:addr});await pollBtStatus();}catch(e){btAppend('disconnect failed: '+(e.message||e));}}"
        "async function pollBtStatus(){try{const r=await fetch('/api/bt-status',{cache:'no-store'});if(!r.ok)throw new Error('http '+r.status);const j=await r.json(),host=j.host||{},hosts=j.hosts||[],devs=j.devices||[],top=$('btTopPill'),badge=$('btStatusBadge'),desc=$('btHostDesc'),pairBtn=$('btPairToggleBtn'),discBtn=$('btDisconnBtn');const running=j.ok&&(j.running||(j.state!=='stopped'&&j.state!=='error'));const connectedHosts=hosts.filter(h=>h.connected);const anyConnected=host.connected||connectedHosts.length>0;if(top){if(!running){top.className='pill bad';top.textContent='BTstack Offline';}else if(anyConnected){top.className='pill ok';top.textContent=connectedHosts.length>1?(connectedHosts.length+' Hosts Connected'):('Connected: '+(host.name||host.addr||'Host'));}else if(host.pairing){top.className='pill warn';top.textContent='Pairing Mode Active';}else{top.className='pill';top.textContent='BTstack Ready (Standby)';}}if(badge){if(!running){badge.className='pill bad';badge.textContent='Offline';}else if(anyConnected){badge.className='pill ok';badge.textContent=connectedHosts.length>1?(connectedHosts.length+' Connected'):'Connected';}else if(host.pairing){badge.className='pill warn';badge.textContent='Pairing Mode';}else{badge.className='pill';badge.textContent='Standby (0 Connected)';}}if(desc){if(!running){desc.textContent='BTstack daemon not running or initializing.';}else if(host.pairing){desc.textContent='Hub discoverable as \\\"'+(host.name||$('btPairName')?.value||'Harmony Keyboard')+'\\\". Pair from host device.';}else if(anyConnected){const names=connectedHosts.map(h=>h.name||h.addr).join(', ');desc.textContent='Active connection with: '+names+'. Ready for keys/text.';}else{const knownCount=devs.filter(d=>d.bdaddr).length;desc.textContent='BTstack daemon active. '+(knownCount>0?(knownCount+' paired host(s) in standby. Reconnects on power-on or click Connect.'):'Click Start Pairing Mode to connect.');}}if(pairBtn){pairBtn.textContent=host.pairing?'Stop Pairing Mode':'Start Pairing Mode';pairBtn.className=host.pairing?'danger':'';}if(discBtn){discBtn.style.display=anyConnected?'inline-block':'none';}renderBtDevicesList(devs,hosts);const targetSel=$('btTargetHostSelect');if(targetSel){const curVal=targetSel.value;targetSel.replaceChildren();const optAuto=document.createElement('option');optAuto.value='';optAuto.textContent='Auto (Active Host)';targetSel.appendChild(optAuto);(devs||[]).forEach(d=>{if(d.bdaddr){const opt=document.createElement('option');opt.value=d.bdaddr;opt.textContent=(d.name||d.id)+' ('+(d.connected?'Connected':'Standby')+')';targetSel.appendChild(opt);}});(hosts||[]).forEach(h=>{if(h.addr&&!(devs||[]).some(d=>d.bdaddr&&d.bdaddr.toUpperCase()===h.addr.toUpperCase())){const opt=document.createElement('option');opt.value=h.addr;opt.textContent=(h.name||h.addr)+(h.connected?' (Connected)':'');targetSel.appendChild(opt);}});if(curVal)targetSel.value=curVal;}return j;}catch(e){const top=$('btTopPill'),badge=$('btStatusBadge'),desc=$('btHostDesc');if(top){top.className='pill bad';top.textContent='Status Error';}if(badge){badge.className='pill bad';badge.textContent='Error';}if(desc)desc.textContent='Failed to poll status: '+(e.message||e);}}"
        "function startBtPolling(){if(!btPollTimer){pollBtStatus();btPollTimer=setInterval(pollBtStatus,2500);}}"
        "function stopBtPolling(){if(btPollTimer){clearInterval(btPollTimer);btPollTimer=null;}}"
        "const btRefreshBtn=$('btRefreshBtn');if(btRefreshBtn)btRefreshBtn.addEventListener('click',()=>pollBtStatus());"
        "const btPairToggleBtn=$('btPairToggleBtn');if(btPairToggleBtn){btPairToggleBtn.addEventListener('click',async()=>{const isPairing=btPairToggleBtn.textContent.includes('Stop'),name=($('btPairName')?.value||'Harmony Keyboard').trim(),devId=$('btPairTargetDev')?.value||'';try{btAppend(isPairing?'stopping pairing mode...':'starting pairing mode as \\\"'+name+'\\\"'+(devId?' (linking to device '+devId+')':'')+'...');await postJson('/api/bt-pairing',{enable:isPairing?'0':'1',name:name,deviceId:devId});await pollBtStatus();}catch(e){btAppend('pairing failed: '+(e.message||e));}});}"
        "const btDisconnBtn=$('btDisconnBtn');if(btDisconnBtn){btDisconnBtn.addEventListener('click',async()=>{if(!confirm('Disconnect all active Bluetooth hosts?'))return;try{btAppend('disconnecting all hosts...');await postJson('/api/bt-disconnect',{});await pollBtStatus();}catch(e){btAppend('disconnect failed: '+(e.message||e));}});}"
        "const btSendTextBtn=$('btSendTextBtn'),btClearTextBtn=$('btClearTextBtn');if(btSendTextBtn){btSendTextBtn.addEventListener('click',async()=>{const el=$('btDirectText'),st=$('btTextStatus'),text=el?el.value:'',tgt=($('btTargetHostSelect')?.value||'').trim();if(!text){btAppend('text box is empty');return;}btSendTextBtn.disabled=true;if(st)st.textContent='Sending...';try{const j=await postJson('/api/bt-text',{text:text,target:tgt});btAppend('sent '+(j.bytes||text.length)+' bytes text'+(tgt?' to '+tgt:''));if(st){st.textContent='Sent!';setTimeout(()=>{st.textContent='';},2000);}}catch(e){btAppend('send text failed: '+(e.message||e));if(st){st.textContent='Error';setTimeout(()=>{st.textContent='';},2500);}}finally{btSendTextBtn.disabled=false;}});if(btClearTextBtn){btClearTextBtn.addEventListener('click',()=>{const el=$('btDirectText');if(el)el.value='';});}}"
        "let btScriptRunning=false;const btRunScriptBtn=$('btRunScriptBtn'),btStopScriptBtn=$('btStopScriptBtn');if(btRunScriptBtn){btRunScriptBtn.addEventListener('click',async()=>{if(btScriptRunning)return;const script=($('btMacroScript')?.value||'').trim(),delay=parseInt($('btScriptDelay')?.value||'35',10)||35,st=$('btScriptStatus'),tgt=($('btTargetHostSelect')?.value||'').trim();if(!script){btAppend('macro script is empty');return;}btScriptRunning=true;btRunScriptBtn.disabled=true;if(btStopScriptBtn)btStopScriptBtn.disabled=false;if(st)st.textContent='Running...';btAppend('running macro script (delay: '+delay+'ms)...');try{const j=await postJson('/api/bt-script',{script:script,delayMs:String(delay),target:tgt});btAppend('script finished: '+(j.message||'ok')+(j.reply?' ('+j.reply+')':''));if(st){st.textContent='Finished!';setTimeout(()=>{st.textContent='';},2500);}}catch(e){btAppend('script error: '+(e.message||e));if(st){st.textContent='Error';setTimeout(()=>{st.textContent='';},2500);}}finally{btScriptRunning=false;btRunScriptBtn.disabled=false;if(btStopScriptBtn)btStopScriptBtn.disabled=true;}});if(btStopScriptBtn){btStopScriptBtn.addEventListener('click',()=>{btAppend('stop requested (sending release all)');sendBtKey('release_all');btScriptRunning=false;btRunScriptBtn.disabled=false;btStopScriptBtn.disabled=true;const st=$('btScriptStatus');if(st)st.textContent='Stopped';});}}"
        "const btSaveCommandBtn=$('btSaveCommandBtn');if(btSaveCommandBtn){btSaveCommandBtn.addEventListener('click',async()=>{const devId=$('btSaveDeviceSelect')?.value||'',name=($('btSaveCommandName')?.value||'').trim(),script=$('btMacroScript')?.value||'',delay=$('btScriptDelay')?.value||'35',st=$('btSaveCommandStatus');if(!devId){btAppend('select a saved target device first');return;}if(!name){btAppend('enter a command name first');return;}if(!script.trim()){btAppend('macro script is empty');return;}if(st)st.textContent='Saving...';try{await postJson('/bt/command',{deviceId:devId,name:name,script:script,delayMs:delay});btAppend('saved macro command \"'+name+'\"');if(st){st.textContent='Saved!';setTimeout(()=>{st.textContent='';},2500);}setTimeout(()=>location.reload(),800);}catch(e){btAppend('save command failed: '+(e.message||e));if(st){st.textContent='Error';setTimeout(()=>{st.textContent='';},2500);}}});}"
        "const btNewDevToggleBtn=$('btNewDevToggleBtn'),btNewDevBox=$('btNewDevBox');if(btNewDevToggleBtn&&btNewDevBox){btNewDevToggleBtn.addEventListener('click',()=>{const on=btNewDevBox.style.display==='none';btNewDevBox.style.display=on?'block':'none';btNewDevToggleBtn.textContent=on?'Cancel':'+ Add New Target';});}const btCancelNewDevBtn=$('btCancelNewDevBtn');if(btCancelNewDevBtn&&btNewDevBox){btCancelNewDevBtn.addEventListener('click',()=>{btNewDevBox.style.display='none';if(btNewDevToggleBtn)btNewDevToggleBtn.textContent='+ Add New Target';});}const btSaveNewDevBtn=$('btSaveNewDevBtn');if(btSaveNewDevBtn){btSaveNewDevBtn.addEventListener('click',async()=>{const name=($('btNewDevName')?.value||'').trim(),addr=($('btNewDevAddr')?.value||'').trim().toUpperCase(),st=$('btNewDevStatus');if(!name||!addr){btAppend('device name and MAC address required');return;}if(!/^[0-9A-F]{2}(:[0-9A-F]{2}){5}$/.test(addr)){btAppend('invalid MAC address format (must be AA:BB:CC:DD:EE:FF)');return;}if(st)st.textContent='Saving...';try{await postJson('/bt/device',{name:name,type:'btkeyboard',bdaddr:addr});btAppend('saved target device \"'+name+'\" ('+addr+')');if(st)st.textContent='Saved!';setTimeout(()=>location.reload(),800);}catch(e){btAppend('save target failed: '+(e.message||e));if(st){st.textContent='Error';setTimeout(()=>{st.textContent='';},2500);}}});}"
        "async function deleteBtDevice(devId){if(!confirm('Delete this saved target device and all its macro commands?'))return;try{await postJson('/bt/delete-device',{deviceId:devId});btAppend('deleted target device '+devId);location.reload();}catch(e){btAppend('delete device failed: '+(e.message||e));}}"
        "async function runBtCommand(devId,cmdName){try{btAppend('running saved command \"'+cmdName+'\"...');const j=await postJson('/api/bt-saved-command',{deviceId:devId,command:cmdName});btAppend('command \"'+cmdName+'\": '+(j.message||'ok'));}catch(e){btAppend('command error: '+(e.message||e));}}"
        "async function deleteBtCommand(devId,cmdName){if(!confirm('Delete command \"'+cmdName+'\"?'))return;try{await postJson('/bt/delete-command',{deviceId:devId,command:cmdName});btAppend('deleted command \"'+cmdName+'\"');location.reload();}catch(e){btAppend('delete command failed: '+(e.message||e));}}"
        "const kbMods={ctrl:false,shift:false,alt:false,win:false};function kbFlashKey(el){if(el){el.classList.add('kb-on');setTimeout(()=>el.classList.remove('kb-on'),120);}}function kbUpdateModUI(){document.querySelectorAll('[data-mod]').forEach(b=>{const m=b.dataset.mod;b.classList.toggle('kb-on',!!kbMods[m]);});}function kbClearMods(){kbMods.ctrl=false;kbMods.shift=false;kbMods.alt=false;kbMods.win=false;kbUpdateModUI();}"
        "async function sendBtKey(code,action='tap'){if(!code)return;let fullKey='';if(code!=='release_all'){if(kbMods.ctrl)fullKey+='ctrl+';if(kbMods.alt)fullKey+='alt+';if(kbMods.win)fullKey+='win+';if(kbMods.shift)fullKey+='shift+';}fullKey+=code;if(action==='tap'||action==='up')kbClearMods();const tgt=($('btTargetHostSelect')?.value||'').trim();try{if(wsConnected){try{await sendWsCommand('bt_key',{key:fullKey,keyAction:action,target:tgt});btAppend((action==='up'?'rel: ':'key: ')+fullKey);return;}catch(_e){}}await postJson('/api/bt-key',{key:fullKey,action:action,target:tgt});btAppend((action==='up'?'rel: ':'key: ')+fullKey);}catch(e){btAppend('key '+fullKey+' failed: '+(e.message||e));}}"
        "document.querySelectorAll('[data-key]').forEach(btn=>{const k=btn.dataset.key;const onDown=e=>{e.preventDefault();btn.classList.add('kb-on');sendBtKey(k,'down');};const onUp=e=>{e.preventDefault();btn.classList.remove('kb-on');sendBtKey(k,'up');};btn.addEventListener('pointerdown',onDown);btn.addEventListener('pointerup',onUp);btn.addEventListener('pointerleave',onUp);btn.addEventListener('pointercancel',onUp);});"
        "document.querySelectorAll('[data-mod]').forEach(btn=>{btn.addEventListener('click',e=>{e.preventDefault();const m=btn.dataset.mod;if(m&&Object.prototype.hasOwnProperty.call(kbMods,m)){kbMods[m]=!kbMods[m];kbUpdateModUI();}});});"
        "const btReleaseAllBtn=$('btReleaseAll');if(btReleaseAllBtn)btReleaseAllBtn.addEventListener('click',()=>{kbClearMods();sendBtKey('release_all','up');});"
        "let btFwdActive=false;const btFwdToggle=$('btFwdToggle');if(btFwdToggle){btFwdToggle.addEventListener('click',()=>{btFwdActive=!btFwdActive;btFwdToggle.textContent=btFwdActive?'Stop forwarding':'Forward physical keyboard';btFwdToggle.classList.toggle('danger',btFwdActive);btFwdToggle.classList.toggle('secondary',!btFwdActive);btAppend(btFwdActive?'keyboard forwarding on':'keyboard forwarding off');if(!btFwdActive)sendBtKey('release_all','up');});}"
        "const domKeyMap={'ArrowUp':'up','ArrowDown':'down','ArrowLeft':'left','ArrowRight':'right','Enter':'enter','Backspace':'backspace','Tab':'tab','Escape':'escape','Delete':'delete','Home':'home','End':'end','PageUp':'pageup','PageDown':'pagedown','CapsLock':'capslock',' ':'space'};"
        "const activeFwdKeys=new Set();function getFwdCombo(e){let k=e.key;if(['Control','Shift','Alt','Meta'].includes(k))return '';let code='';if(domKeyMap[k])code=domKeyMap[k];else if(/^F([1-9]|1[0-2])$/i.test(k))code=k.toLowerCase();else if(k.length===1)code=k;if(!code)return '';let combo='';if(e.ctrlKey)combo+='ctrl+';if(e.altKey)combo+='alt+';if(e.metaKey)combo+='win+';if(e.shiftKey&&!/^[A-Z]$/.test(k)&&code.length>1)combo+='shift+';combo+=code;return combo;}"
        "document.addEventListener('keydown',e=>{if(!btFwdActive)return;const tag=(e.target&&e.target.tagName||'').toLowerCase();if(tag==='input'||tag==='textarea'||tag==='select')return;const combo=getFwdCombo(e);if(!combo)return;e.preventDefault();if(e.repeat)return;activeFwdKeys.add(combo);const rawKey=domKeyMap[e.key]||(e.key.length===1?e.key:combo);const btn=document.querySelector('[data-key=\"'+rawKey+'\"]');if(btn)btn.classList.add('kb-on');sendBtKey(combo,'down');});"
        "document.addEventListener('keyup',e=>{if(!btFwdActive)return;const tag=(e.target&&e.target.tagName||'').toLowerCase();if(tag==='input'||tag==='textarea'||tag==='select')return;const combo=getFwdCombo(e);if(!combo)return;e.preventDefault();activeFwdKeys.delete(combo);const rawKey=domKeyMap[e.key]||(e.key.length===1?e.key:combo);const btn=document.querySelector('[data-key=\"'+rawKey+'\"]');if(btn)btn.classList.remove('kb-on');sendBtKey(combo,'up');});"
        "window.addEventListener('blur',()=>{if(btFwdActive&&activeFwdKeys.size>0){activeFwdKeys.clear();document.querySelectorAll('.kb-key.kb-on').forEach(b=>b.classList.remove('kb-on'));sendBtKey('release_all','up');}});"
        "async function refreshBtBackend(){try{const j=await (await fetch('/api/bt-status')).json();const g=$('b25BtstackGuard'),w=$('b25ControlsWrapper');if(j&&j.ok){if(g)g.style.display='none';if(w)w.style.display='';}else{if(g)g.style.display='block';if(w)w.style.display='none';}}catch(e){}}"
        "const LAB_AUTO_DEVICE='__auto_lab__';const lab={queue:[],index:[],cursor:0,key:'',running:false,stop:false,imported:false,runId:'',streaming:false,seenCodes:new Set(),dupes:0};"
        "function labStatus(t){const s=$('labStatus');if(s)s.textContent=t||'';}function labSleep(ms){return new Promise(r=>setTimeout(r,ms));}"
        "function labNum(id,def,min,max){let v=parseInt($(id)?.value||def,10);if(!Number.isFinite(v))v=def;v=Math.max(min,Math.min(max,v));const e=$(id);if(e)e.value=String(v);return v;}"
        "function labLog(t){const l=$('labLog');if(!l)return;l.textContent=(t?new Date().toLocaleTimeString()+'  '+t+'\\n':'')+l.textContent.slice(0,5000);}"
        "function labMeter(done,total){const m=$('labMeter');if(m)m.style.width=(total?Math.round(done*100/total):0)+'%';}"
        "function labSelectDevice(id,name){['labDevice','irdbDevice','wizardDevice','verifyDevice'].forEach(selId=>{const s=$(selId);if(!s)return;if(!Array.from(s.options).some(o=>o.value===id)){const o=document.createElement('option');o.value=id;o.textContent=(name||'Temporary IR Test')+' ('+id+')';s.append(o);}s.value=id;});populateVerifyCommands();}"
        "async function labResolveDevice(){const sel=$('labDevice');let id=sel?sel.value:'';if(id&&id!==LAB_AUTO_DEVICE)return id;labStatus('preparing temporary test device...');const j=await postJson('/api/ir-lab-target',{});labSelectDevice(j.deviceId,j.name);await loadWizardInventory();labLog((j.created?'created ':'using ')+(j.name||'Temporary IR Test')+' '+j.deviceId);return j.deviceId;}"
        "function labTerms(id){return String($(id)?.value||'').split(',').map(x=>normText(x)).filter(Boolean);}function labMatchName(name){const toks=labTerms('labCommandFilter');if(!toks.length)return true;const n=normText(name);return toks.some(t=>n.includes(t));}"
        "function labSummaryText(){const s=$('labSummary');if(!s)return;const checked=document.querySelectorAll('#labQueue input.labPick:checked').length,total=lab.queue.length,stored=lab.queue.filter(x=>x.stored).length,raw=lab.queue.filter(x=>x.raw).length,dupes=lab.dupes||0;s.textContent=checked+' selected / '+total+' queued / '+stored+' saved / '+raw+' timing'+(dupes?' / '+dupes+' duplicate codes skipped':'');}"
        "function labKey(){return [$('labSource')?.value||'all',$('labPathFilter')?.value||'',$('labCommandFilter')?.value||'',$('labRcPath')?.value||''].join('|');}"
        "function labSourceList(){const s=$('labSource')?.value||'all';if(s==='all')return['irdb','flipper','lirc','smartir'];return(['irdb','flipper','lirc','smartir'].includes(s))?[s]:[];}"
        "function labPathTitle(path){const p=String(path||'').replace(/\\.(csv|ir|json|conf|lircd|lirc|girr|xml|irc)$/i,'').split('/').filter(Boolean);return p.slice(-3).join(' / ')||path;}"
        "function labRenderCandidates(rows){const box=$('labCandidates');if(!box)return;box.replaceChildren();if(!rows.length){box.classList.add('hidden');return;}rows.slice(0,300).forEach(r=>{const b=document.createElement('button');b.type='button';b.className='match';b.innerHTML='<strong>'+escHtml(sourceLabel(r.source))+'</strong> '+escHtml(labPathTitle(r.path))+'<div class=\"queue-meta\">score '+Math.round(r.score||0)+' - '+escHtml(r.path)+'</div>';b.addEventListener('click',()=>{const src=$('labSource'),q=$('labPathFilter');if(src)src.value=r.source;if(q)q.value=r.path;lab.key=labKey();lab.index=[{source:r.source,path:r.path,score:r.score||1,man:r.man||'',model:r.model||''}];lab.cursor=0;labLog('selected '+sourceLabel(r.source)+' '+r.path);labStatus('selected file; click Load commands');});box.append(b);});box.classList.remove('hidden');}"
        "async function labFindCandidates(){try{const src=$('labSource')?.value||'all',q=($('labPathFilter')?.value||'').trim();labStatus('searching device files...');labLog('device search '+(q?JSON.stringify(q):'all files')+' in '+src);let rows=[];const sourceNames=labSourceList();if(sourceNames.length){const files=(await loadIrdSources(sourceNames,labLog)).map(r=>({...r,title:labPathTitle(r.path)})),ranked=q?rankIrdEntries(files,q):files.map(r=>({...r,score:1}));rows=rows.concat(ranked);labLog((q&&ranked.fallback?'no exact local match; broadened to manufacturer/category. ':'')+'loaded local indexes: '+sourceBreakdown(ranked));}if(q&&(src==='all'||src==='remotecentral')){try{const rr=await remoteCentralEntries(q);rows=rows.concat(rr);labLog('RemoteCentral candidates: '+rr.length);}catch(e){labLog('RemoteCentral search skipped: '+(e.message||e));}}rows=dedupeIrdEntries(rows).sort((a,b)=>(b.score||0)-(a.score||0)||String(a.path).length-String(b.path).length);labRenderCandidates(rows);if(rows.length){labStatus((q&&rows.some(r=>r.score&&r.score<60)?'showing likely ':'found ')+rows.length+' possible device files'+(rows.dupes?' after '+rows.dupes+' duplicate paths skipped':'')+' ('+sourceBreakdown(rows)+')');labLog('showing first '+Math.min(rows.length,300)+' candidates');}else labStatus(q?'no matching device files found':'no database files loaded');}catch(e){labStatus('device search failed: '+(e.message||e));labLog('device search failed: '+(e.message||e));}}"
        "function labUniqueName(base){base=safeImportName(base);let name=base,n=2,seen=new Set(lab.queue.map(x=>x.name));while(seen.has(name)){name=(base+' '+n++).slice(0,96);}return name;}function labCodeFingerprint(r){const raw=String(r&&r.raw||'').replace(/\\s+/g,'').toLowerCase(),key=String(r&&r.keycode||'').replace(/\\s+/g,'').toLowerCase();return raw?'raw:'+raw:(key?'key:'+key:'');}function labResetSeen(){lab.seenCodes=new Set();lab.dupes=0;}function labPrimeSeen(){if(!lab.seenCodes)lab.seenCodes=new Set();lab.queue.forEach(x=>{const fp=labCodeFingerprint(x);if(fp)lab.seenCodes.add(fp);});}"
        "function labSelected(){return Array.from(document.querySelectorAll('#labQueue input.labPick:checked')).map(cb=>lab.queue[Number(cb.dataset.i)]).filter(Boolean);}"
        "function labPayloadLines(rows){return (rows||labSelected()).filter(r=>!r.stored&&(r.keycode||r.raw)).map(r=>r.raw?r.name+'|raw|'+r.raw:r.name+'|'+r.keycode);}function labPayload(rows){return labPayloadLines(rows).join('\\n');}"
        "function labRender(){const box=$('labQueue');if(!box)return;box.replaceChildren();if(!lab.queue.length){const d=document.createElement('div');d.className='muted mini';d.textContent='No commands queued.';box.append(d);labMeter(0,1);labSummaryText();return;}lab.queue.forEach((r,i)=>{const row=document.createElement('label'),cb=document.createElement('input'),main=document.createElement('div'),tag=document.createElement('span');row.className='queue-row';cb.className='labPick';cb.type='checkbox';cb.checked=r.checked!==false;cb.dataset.i=i;cb.addEventListener('change',()=>{r.checked=cb.checked;labSummaryText();});const strong=document.createElement('strong'),meta=document.createElement('div');strong.textContent=r.name;meta.className='queue-meta';meta.textContent=(r.stored?'Saved':'Queued')+' / '+sourceLabel(r.source||'irdb')+' / '+(r.meta||r.path||'');tag.className='badge';tag.textContent=r.raw?'timing':(r.stored?'saved':'code');main.append(strong,meta);row.append(cb,main,tag);box.append(row);});labMeter(lab.queue.length,labNum('labMaxCommands',900,1,2048));labSummaryText();}"
        "function labAddRows(rows,source,path){const max=labNum('labMaxCommands',900,1,2048);let added=0;labPrimeSeen();for(const r of rows){if(lab.queue.length>=max)break;if(!(r.keycode||r.raw)||!labMatchName(r.name))continue;const codeFp=labCodeFingerprint(r);if(!codeFp)continue;if(lab.seenCodes.has(codeFp)){lab.dupes=(lab.dupes||0)+1;continue;}const src=r.source||source,pth=r.path||path;lab.seenCodes.add(codeFp);lab.queue.push({source:src,path:pth,name:labUniqueName(r.name),meta:r.meta||'',keycode:r.keycode||'',raw:r.raw||'',stored:false,checked:true});added++;}lab.imported=false;labRender();return added;}"
        "async function labFetchEntry(e){if(e.source==='remotecentral'){const html=await rcFetch(e.path);return parseIrText(html,'remotecentral',e.path).map(r=>({...r,source:'remotecentral',path:e.path}));}let url=e.path.replace(/^\\//,'');if(e.source==='flipper')url=FLIPPER_BASE+url;else if(e.source==='lirc')url=LIRC_BASE+url;else if(e.source==='smartir')url=SMARTIR_BASE+url;else url=IRDB_BASE+url.replace(/^codes\\//,'');const r=await fetch(url);if(!r.ok)throw new Error('http '+r.status);const text=await r.text();return parseIrText(text,e.source,e.path).map(r=>({...r,source:e.source,path:e.path}));}"
        "async function labEnsureIndex(){const key=labKey();if(key===lab.key&&lab.index.length)return;lab.key=key;lab.cursor=0;lab.index=[];const src=$('labSource')?.value||'all',q=($('labPathFilter')?.value||'').trim(),rc=($('labRcPath')?.value||'').trim();let rows=[];if(src==='remotecentral'&&rc){lab.index=[{source:'remotecentral',path:rc,score:1}];labLog('using direct RemoteCentral path '+rc);return;}const sourceNames=labSourceList();if(sourceNames.length){const files=(await loadIrdSources(sourceNames,labLog)).map(r=>({...r,title:labPathTitle(r.path)})),ranked=q?rankIrdEntries(files,q):files.map(r=>({...r,score:1}));rows=rows.concat(ranked);labLog((q&&ranked.fallback?'no exact local match; broadened to manufacturer/category. ':'')+'matched local indexes: '+sourceBreakdown(ranked));}if(q&&(src==='all'||src==='remotecentral')){try{const rr=await remoteCentralEntries(q);rows=rows.concat(rr);labLog('RemoteCentral candidates: '+rr.length);}catch(e){labLog('RemoteCentral search skipped: '+(e.message||e));}}rows=dedupeIrdEntries(rows).sort((a,b)=>(b.score||0)-(a.score||0)||String(a.path).length-String(b.path).length);lab.index=rows;if(rows.dupes)labLog('skipped '+rows.dupes+' duplicate candidate paths');labLog('search index ready: '+rows.length+' files ('+sourceBreakdown(rows)+')');}"
        "async function labScanBatch(){try{const maxFiles=labNum('labMaxFiles',600,1,5000),pause=labNum('labFetchDelay',0,0,3000),maxCmd=labNum('labMaxCommands',900,1,2048),workers=labNum('labWorkers',14,1,24),before=lab.queue.length,beforeDupes=lab.dupes||0;labStatus('loading search index...');await labEnsureIndex();if(lab.cursor>=lab.index.length){labStatus('end of matching library files');return;}const state={files:0,rows:0,supported:0,added:0};async function worker(){while(lab.cursor<lab.index.length&&state.files<maxFiles&&lab.queue.length<maxCmd){const e=lab.index[lab.cursor++],n=++state.files;labStatus('fetching '+n+' / '+maxFiles+' with '+workers+' workers - '+e.path);try{const rows=await labFetchEntry(e);state.rows+=rows.length;state.supported+=rows.filter(x=>x.keycode||x.raw).length;state.added+=labAddRows(rows,e.source,e.path);}catch(err){labLog('skip '+e.path+': '+(err.message||err));}labMeter(lab.cursor,lab.index.length);if(pause)await labSleep(pause);}}await Promise.all(Array.from({length:workers},worker));const dupes=(lab.dupes||0)-beforeDupes,unsupported=Math.max(0,state.rows-state.supported);labStatus('queued '+(lab.queue.length-before)+' new commands'+(dupes?'; skipped '+dupes+' duplicate codes':'')+' from '+state.files+' files; cursor '+lab.cursor+' / '+lab.index.length);labLog('loaded '+state.files+' files, parsed '+state.rows+' commands, '+state.supported+' supported, '+unsupported+' unsupported, '+state.added+' queued');}catch(e){labStatus(e.message||String(e));labLog('load failed: '+(e.message||e));}}"
        "async function labUseStored(){if(!wizardInventory)await loadWizardInventory();const devId=await labResolveDevice(),dev=(wizardInventory&&wizardInventory.devices||[]).find(d=>d.id===devId),cmds=await loadDeviceCommands(devId);lab.queue=[];labResetSeen();cmds.forEach(c=>{if(!labMatchName(c.name))return;const row={source:'stored',stored:true,name:c.name,meta:'saved on '+((dev&&dev.name)||devId),keycode:c.keycode||'',raw:''},fp=labCodeFingerprint(row);if(fp&&lab.seenCodes.has(fp)){lab.dupes++;return;}if(fp)lab.seenCodes.add(fp);lab.queue.push(row);});lab.imported=true;labRender();labStatus('loaded '+lab.queue.length+' saved matching commands'+(lab.dupes?' and skipped '+lab.dupes+' duplicate codes':''));}"
        "async function labImportQueue(rowsArg){const rows=rowsArg||labSelected(),lines=labPayloadLines(rows),dev=await labResolveDevice();if(!lines.length){lab.imported=true;labStatus('selected commands are already saved');return true;}labStatus('saving '+lines.length+' commands...');const limit=420000;let part=[],size=0,done=0,last='';async function flush(){if(!part.length)return;let j;try{j=await postJson('/api/irdb-import',{deviceId:dev,payload:part.join('\\n')});}catch(e){if(!/No supported new/i.test(e.message||''))throw e;j={message:'No new commands were saved; using existing saved names.'};}done+=part.length;last=j.message||last;labLog((j.message||'saved')+' ('+done+' / '+lines.length+')');part=[];size=0;}for(const line of lines){const n=line.length+1;if(part.length&&size+n>limit)await flush();part.push(line);size+=n;}await flush();lab.imported=true;labStatus(last||'save complete');await labSleep(1000);await loadWizardInventory();const saved=await loadDeviceCommands(dev),names=new Set(saved.map(c=>c.name));rows.forEach(r=>{if(names.has(r.name))r.stored=true;});labLog('verified '+names.size+' saved commands on '+dev);labRender();return true;}"
        "async function labClearLabTarget(dev){try{const j=await postJson('/api/ir-lab-clear',{deviceId:dev});labLog(j.message||'temporary device cleared');await labSleep(700);await loadWizardInventory();lab.queue.forEach(r=>{r.stored=false;});lab.imported=false;labRender();return true;}catch(e){labLog('temporary device clear failed: '+(e.message||e));return false;}}"
        "async function labCancelRun(){const id=lab.runId;if(!id)return;try{await postJson('/api/ir-cancel',{runId:id});labLog('cancel sent for '+id);}catch(e){labLog('cancel failed: '+(e.message||e));}}"
        "function labServerChunk(requested,delay){const maxHold=4000,byTime=Math.max(1,Math.floor(maxHold/Math.max(40,delay)));return Math.max(1,Math.min(requested,byTime,1024));}"
        "async function labRunRows(rows,dev,dry,delay,requestedChunk,offset,total){const chunk=labServerChunk(requestedChunk,delay);let done=0;labLog('run '+lab.runId+' using hub chunk '+chunk+' (requested '+requestedChunk+')');for(let i=0;i<rows.length;i+=chunk){if(lab.stop){labStatus('stopped after '+(offset+done)+' commands');break;}const slice=rows.slice(i,i+chunk),label=(dry?'dry-running ':'sending ')+(i+1)+'-'+(i+slice.length)+' / '+rows.length;labStatus((total&&total>rows.length?('stream '+(offset+done)+' sent, '+label):label));const j=await postJson('/api/ir-batch-send',{deviceId:dev,commands:slice.map(r=>r.name).join('\\n'),delayMs:delay,dryRun:dry?'1':'0',runId:lab.runId});done+=j.sent||0;labMeter(total?Math.min(offset+done,total):done,total||rows.length);const failText=j.failed?(', '+j.failed+' failed'):'';labLog((dry?'dry run ':'batch sent ')+(j.sent||0)+' commands'+failText+' in '+(j.elapsedMs||0)+' ms; last '+String(j.lastReply||'').slice(0,120));if(j.canceled){lab.stop=true;labStatus('stopped after '+(offset+done)+' commands');break;}}return done;}"
        "async function labRunQueue(){if(lab.running)return;const rows=labSelected(),dry=$('labDryRun')?.checked,delay=labNum('labSendDelay',80,40,10000),requestedChunk=labNum('labBatchSize',100,1,1024);if(!rows.length){labStatus('select at least one command');return;}try{const dev=await labResolveDevice();if(!dry&&rows.some(r=>!r.stored))await labImportQueue(rows);lab.running=true;lab.stop=false;lab.runId='run_'+Date.now().toString(36)+'_'+Math.random().toString(36).slice(2,8);const ready=dry?rows:rows.filter(r=>r.stored);const done=await labRunRows(ready,dev,dry,delay,requestedChunk,0,ready.length);if(!lab.stop)labStatus((dry?'dry run complete: ':'send complete: ')+done+' commands');labMeter(done,ready.length);}catch(e){labStatus('send failed: '+(e.message||e));}finally{lab.running=false;lab.runId='';}}"
        "function labSetFilter(text,msg){const e=$('labCommandFilter');if(e)e.value=text;lab.index=[];lab.cursor=0;lab.key='';labStatus(msg||'filter updated');}"
        "const labOff=$('labOffFilter');if(labOff)labOff.addEventListener('click',()=>labSetFilter('off, power off, poweroff, standby, shutdown','off-oriented filter loaded'));const labPower=$('labPowerFilter');if(labPower)labPower.addEventListener('click',()=>labSetFilter('power, toggle, on, off, standby','power filter loaded'));const labVol=$('labVolumeFilter');if(labVol)labVol.addEventListener('click',()=>labSetFilter('volume, mute, vol up, vol down','volume filter loaded'));const labInput=$('labInputFilter');if(labInput)labInput.addEventListener('click',()=>labSetFilter('input, hdmi, source, aux, optical, bluetooth','input filter loaded'));const labClearFilter=$('labClearFilter');if(labClearFilter)labClearFilter.addEventListener('click',()=>labSetFilter('','filter cleared'));"
        "const labCandidatesBtn=$('labCandidatesBtn');if(labCandidatesBtn)labCandidatesBtn.addEventListener('click',labFindCandidates);const labScan=$('labScan');if(labScan)labScan.addEventListener('click',labScanBatch);const labStored=$('labStored');if(labStored)labStored.addEventListener('click',labUseStored);const labClear=$('labClear');if(labClear)labClear.addEventListener('click',()=>{lab.queue=[];lab.index=[];lab.cursor=0;lab.key='';labResetSeen();lab.imported=false;labRender();labStatus('queue cleared');});const labImport=$('labImport');if(labImport)labImport.addEventListener('click',()=>labImportQueue().catch(e=>labStatus('import failed: '+(e.message||e))));const labRun=$('labRun');if(labRun)labRun.addEventListener('click',labRunQueue);const labStop=$('labStop');if(labStop)labStop.addEventListener('click',()=>{lab.stop=true;labStatus('stop requested');labCancelRun();});const labSelectAll=$('labSelectAll');if(labSelectAll)labSelectAll.addEventListener('click',()=>{lab.queue.forEach(r=>r.checked=true);labRender();});const labSelectNone=$('labSelectNone');if(labSelectNone)labSelectNone.addEventListener('click',()=>{lab.queue.forEach(r=>r.checked=false);labRender();});const labDropUnchecked=$('labDropUnchecked');if(labDropUnchecked)labDropUnchecked.addEventListener('click',()=>{lab.queue=lab.queue.filter(r=>r.checked!==false);labRender();});['labSource','labPathFilter','labCommandFilter','labRcPath'].forEach(id=>{const e=$(id);if(e)e.addEventListener('change',()=>{lab.index=[];lab.cursor=0;lab.key='';});});labRender();"
        "const irdbForm=$('irdbImportForm');if(irdbForm){irdbForm.addEventListener('submit',async e=>{e.preventDefault();updateIrdPayload();const payload=$('irdbPayload')?.value||'';if(!payload){irdbStatus('select at least one supported command');irdbLog('import blocked: no supported checked commands');return;}const count=payload.split('\\n').filter(Boolean).length;irdbLog('submitting '+count+' selected commands...');irdbStatus('importing '+count+' commands...');const btn=irdbForm.querySelector('button[type=submit]');if(btn)btn.disabled=true;try{const res=await postForm(irdbForm);irdbLog(res.message||'Import completed successfully');irdbStatus(res.message||'Import completed');wizardInventory=null;await loadWizardInventory();}catch(err){irdbLog('import failed: '+(err.message||err));irdbStatus('import failed');}finally{if(btn)btn.disabled=false;}});}"
        "async function loadActivities(){try{"
        "const[actRes]=await Promise.all([fetch('/api/activities').then(r=>r.json()),(!wizardInventory?loadWizardInventory():Promise.resolve())]);"
        "if(!actRes||!actRes.ok)return;"
        "activityInventory=actRes;"
        "const curId=String(actRes.currentId||'-1'),curName=actRes.currentName||'PowerOff';"
        "const isTrans=Boolean(actRes.isTransitioning),transTarget=actRes.transitionTarget||'';"
        "const curNameEl=$('actCurrentName');if(curNameEl)curNameEl.textContent=isTrans?('Switching to '+(transTarget||'activity')+'...'):curName;"
        "const curMetaEl=$('actCurrentMeta');if(curMetaEl)curMetaEl.textContent=isTrans?'Transition in progress':'Activity ID: '+curId;"
        "const curBadgeEl=$('actCurrentStatusBadge');if(curBadgeEl){curBadgeEl.textContent=isTrans?'Switching':(curId==='-1'?'Off':'Active');curBadgeEl.className=isTrans?'badge warn':(curId==='-1'?'badge warn':'badge ok');}"
        "const dashName=$('dashActiveActName');if(dashName){dashName.textContent=isTrans?'Switching...':curName;dashName.className=isTrans?'badge warn':(curId==='-1'?'badge warn':'badge ok');}"
        "const dashDetail=$('dashActiveActDetail');if(dashDetail)dashDetail.textContent=isTrans?('Target ID: '+transTarget):('ID: '+curId);"
        "const grid=$('actGrid');if(!grid)return;grid.replaceChildren();"
        "const allActs=(actRes.activities||[]).slice();"
        "if(!allActs.some(a=>String(a.id)==='-1')){"
        "allActs.unshift({id:'-1',name:'PowerOff',type:'PowerOff',order:0,deviceIds:[],startSteps:[],stopSteps:[]});"
        "}"
        "allActs.forEach(act=>{"
        "const isActive=String(act.id)===curId,card=document.createElement('div');"
        "card.className='panel';card.style.position='relative';"
        "if(isActive){card.style.borderColor='var(--accent)';card.style.background='var(--soft2)';}"
        "const head=document.createElement('div');head.style.display='flex';head.style.justifyContent='space-between';head.style.alignItems='start';head.style.gap='8px';"
        "const badgeText=isTrans&&(String(act.id)===transTarget)?'Starting...':(isActive?'Active':'Idle');"
        "head.innerHTML='<div><strong>'+escHtml(act.name)+'</strong><div class=\"muted mini\">'+escHtml(act.type||'Activity')+' \u2022 Order: '+act.order+'</div></div><span class=\"badge '+(isActive?'ok':'ghost')+'\">'+badgeText+'</span>';"
        "const meta=document.createElement('div');meta.className='muted mini';meta.style.margin='8px 0';"
        "meta.textContent=(act.deviceIds||[]).length+' device(s) \u2022 '+(act.startSteps||[]).length+' start step(s) \u2022 '+(act.stopSteps||[]).length+' stop step(s)';"
        "const actions=document.createElement('div');actions.className='actions';actions.style.marginTop='10px';"
        "if(isActive&&String(act.id)!=='-1'){"
        "const stopBtn=document.createElement('button');stopBtn.type='button';stopBtn.className='danger mini';stopBtn.textContent='Power Off';"
        "if(isTrans){stopBtn.disabled=true;stopBtn.style.opacity='0.6';}else{stopBtn.addEventListener('click',stopActivity);}"
        "actions.append(stopBtn);"
        "}else if(!isActive&&String(act.id)==='-1'){"
        "const offBtn=document.createElement('button');offBtn.type='button';offBtn.className='danger mini';offBtn.textContent='Power Off';"
        "if(isTrans){offBtn.disabled=true;offBtn.style.opacity='0.6';}else{offBtn.addEventListener('click',stopActivity);}"
        "actions.append(offBtn);"
        "}else if(!isActive){"
        "const startBtn=document.createElement('button');startBtn.type='button';startBtn.className='mini';startBtn.textContent='Start';"
        "if(isTrans){startBtn.disabled=true;startBtn.style.opacity='0.6';}else{startBtn.addEventListener('click',()=>startActivity(act.id));}"
        "actions.append(startBtn);"
        "}"
        "const editBtn=document.createElement('button');editBtn.type='button';editBtn.className='secondary mini';editBtn.textContent='Edit';editBtn.addEventListener('click',()=>openActivityEditor(act.id));actions.append(editBtn);"
        "if(String(act.id)!=='-1'){"
        "const delBtn=document.createElement('button');delBtn.type='button';delBtn.className='ghost mini';delBtn.textContent='Delete';delBtn.addEventListener('click',()=>deleteActivity(act.id,act.name));actions.append(delBtn);"
        "}"
        "card.append(head,meta,actions);grid.append(card);"
        "});updateTransitionDropdowns();}catch(e){console.error('loadActivities error',e);}}"
        "function pollActivityTransition(targetId,attempts){"
        "attempts=attempts!==undefined?attempts:30;"
        "if(actPollTimer){clearTimeout(actPollTimer);actPollTimer=null;}"
        "loadActivities().then(()=>{"
        "const isTrans=Boolean(activityInventory?.isTransitioning);"
        "const cur=String(activityInventory?.currentId||'-1');"
        "if(attempts>0&&(isTrans||(targetId!==null&&cur!==targetId))){"
        "actPollTimer=setTimeout(()=>pollActivityTransition(targetId,attempts-1),600);"
        "}"
        "});}"
        "async function startActivity(id){try{if(wsConnected){try{const j=await sendWsCommand('start_activity',{id:String(id)});if(j.error&&!j.ok){alert(j.error);return;}pollActivityTransition(String(id));return;}catch(_e){}}const j=await postJson('/api/activity-start',{id:id});if(j.error&&!j.ok){alert(j.error);return;}pollActivityTransition(String(id));}catch(e){alert('Failed to start activity: '+(e.message||e));}}"
        "async function stopActivity(){try{if(wsConnected){try{const j=await sendWsCommand('stop_activity',{});if(j.error&&!j.ok){alert(j.error);return;}pollActivityTransition('-1');return;}catch(_e){}}const j=await postJson('/api/activity-stop',{});if(j.error&&!j.ok){alert(j.error);return;}pollActivityTransition('-1');}catch(e){alert('Failed to stop activity: '+(e.message||e));}}"
        "function renderStepRow(container,step){step=step||{type:'IRCommand',deviceId:'',command:'',delay:0};"
        "const row=document.createElement('div');row.className='queue-row';row.style.gridTemplateColumns='auto 1fr auto';row.style.alignItems='center';row.style.gap='8px';"
        "const typeSelect=document.createElement('select');typeSelect.style.width='120px';"
        "typeSelect.innerHTML='<option value=\"IRCommand\" '+(step.type!=='Delay'?'selected':'')+'>Command</option><option value=\"Delay\" '+(step.type==='Delay'?'selected':'')+'>Delay Only</option>';"
        "const center=document.createElement('div');center.style.display='flex';center.style.flexWrap='wrap';center.style.gap='6px';center.style.alignItems='center';"
        "const devSelect=document.createElement('select');devSelect.style.width='140px';"
        "(wizardInventory?.devices||[]).forEach(d=>{"
        "const opt=document.createElement('option');opt.value=d.id;opt.textContent=d.name;if(String(d.id)===String(step.deviceId))opt.selected=true;devSelect.append(opt);"
        "});if(!devSelect.value&&devSelect.options.length)devSelect.selectedIndex=0;"
        "const cmdSelect=document.createElement('select');cmdSelect.style.width='150px';"
        "function updateCmdOptions(){cmdSelect.replaceChildren();const selDevId=devSelect.value,dev=(wizardInventory?.devices||[]).find(d=>String(d.id)===String(selDevId));"
        "let matched=false;(dev?.commands||[]).forEach(c=>{"
        "const opt=document.createElement('option');opt.value=c.name;opt.textContent=c.name;if(c.name===step.command){opt.selected=true;matched=true;}cmdSelect.append(opt);"
        "});if(step.command&&!matched){const customOpt=document.createElement('option');customOpt.value=step.command;customOpt.textContent=step.command;customOpt.selected=true;cmdSelect.prepend(customOpt);}}"
        "devSelect.addEventListener('change',updateCmdOptions);updateCmdOptions();"
        "const delayWrap=document.createElement('div');delayWrap.style.display='inline-flex';delayWrap.style.alignItems='center';delayWrap.style.gap='4px';"
        "delayWrap.innerHTML='<span class=\"muted mini\">Wait:</span>';"
        "const delayInput=document.createElement('input');delayInput.type='number';delayInput.min='0';delayInput.max='10000';delayInput.step='100';delayInput.style.width='70px';"
        "delayInput.value=(step.delay!==undefined)?step.delay:((step.delay_ms!==undefined)?step.delay_ms:(step.type==='Delay'?1000:0));"
        "delayWrap.append(delayInput,document.createTextNode('ms'));"
        "function refreshVis(){const isCmd=(typeSelect.value!=='Delay');devSelect.style.display=isCmd?'':'none';cmdSelect.style.display=isCmd?'':'none';}"
        "typeSelect.addEventListener('change',refreshVis);refreshVis();"
        "center.append(devSelect,cmdSelect,delayWrap);"
        "const actions=document.createElement('div');actions.style.display='flex';actions.style.gap='4px';"
        "const upBtn=document.createElement('button');upBtn.type='button';upBtn.className='ghost mini';upBtn.innerHTML='&uarr;';"
        "upBtn.addEventListener('click',()=>{if(row.previousElementSibling)container.insertBefore(row,row.previousElementSibling);});"
        "const downBtn=document.createElement('button');downBtn.type='button';downBtn.className='ghost mini';downBtn.innerHTML='&darr;';"
        "downBtn.addEventListener('click',()=>{if(row.nextElementSibling)container.insertBefore(row.nextElementSibling,row);});"
        "const delBtn=document.createElement('button');delBtn.type='button';delBtn.className='danger mini';delBtn.innerHTML='&times;';"
        "delBtn.addEventListener('click',()=>row.remove());"
        "actions.append(upBtn,downBtn,delBtn);"
        "row.getStepData=()=>{"
        "const t=(typeSelect.value==='Delay')?'Delay':'IRCommand';"
        "let did='',cmd='';"
        "if(t==='IRCommand'){did=devSelect.value;cmd=cmdSelect.value;}"
        "const del=parseInt(delayInput.value,10)||0;"
        "return t+'|'+did+'|'+cmd+'|'+del;"
        "};"
        "row.append(typeSelect,center,actions);container.append(row);}"
        "function serializeContainerSteps(containerId){const el=$(containerId);if(!el)return '';const lines=[];el.querySelectorAll('.queue-row').forEach(r=>{if(r.getStepData)lines.push(r.getStepData());});return lines.join('\\n');}"
        "async function testActivitySeq(containerId){const steps=serializeContainerSteps(containerId),st=$('actEditorStatus');if(!steps){if(st)st.textContent='No steps to test.';return;}if(st)st.textContent='Testing sequence live...';try{const j=await postJson('/api/activity-test-sequence',{steps:steps});if(st)st.textContent='Test result: '+(j.message||'done');}catch(e){if(st)st.textContent='Test failed: '+(e.message||e);}}"
        "function renderActDeviceRow(cont,did){const d=(wizardInventory?.devices||[]).find(x=>String(x.id)===String(did)),name=d?d.name:('Device '+did),row=document.createElement('div');row.className='queue-row act-dev-row';row.dataset.devId=String(did);row.style.cssText='display:flex;align-items:center;gap:8px;padding:4px 8px;margin-bottom:4px;';const num=document.createElement('span');num.className='muted mini act-dev-num';num.style.minWidth='24px';const nm=document.createElement('span');nm.style.flex='1';nm.style.fontWeight='600';nm.textContent=name;const acts=document.createElement('div');acts.className='actions';acts.style.cssText='margin:0;display:flex;gap:4px;';const upBtn=document.createElement('button');upBtn.type='button';upBtn.className='ghost mini';upBtn.innerHTML='&uarr;';upBtn.title='Move Up';upBtn.addEventListener('click',()=>{if(row.previousElementSibling){cont.insertBefore(row,row.previousElementSibling);updateActDevNums(cont);}});const downBtn=document.createElement('button');downBtn.type='button';downBtn.className='ghost mini';downBtn.innerHTML='&darr;';downBtn.title='Move Down';downBtn.addEventListener('click',()=>{if(row.nextElementSibling){cont.insertBefore(row.nextElementSibling,row);updateActDevNums(cont);}});const delBtn=document.createElement('button');delBtn.type='button';delBtn.className='danger ghost mini';delBtn.innerHTML='&times;';delBtn.title='Remove';delBtn.addEventListener('click',()=>{row.remove();updateActDevNums(cont);});acts.append(upBtn,downBtn,delBtn);row.append(num,nm,acts);cont.append(row);updateActDevNums(cont);}"
        "function updateActDevNums(cont){if(!cont)return;cont.querySelectorAll('.act-dev-row').forEach((r,i)=>{const n=r.querySelector('.act-dev-num');if(n)n.textContent='#'+(i+1);});}"
        "async function openActivityEditor(actId){if(!wizardInventory)await loadWizardInventory();const box=$('actEditorBox');if(!box)return;"
        "const title=$('actEditorTitle'),idInput=$('actEditId'),nameInput=$('actEditName'),typeSelect=$('actEditType'),orderInput=$('actEditOrder'),delBtn=$('actDeleteBtn'),status=$('actEditorStatus'),startList=$('actStartStepsList'),stopList=$('actStopStepsList');"
        "if(status)status.textContent='';if(startList)startList.replaceChildren();if(stopList)stopList.replaceChildren();"
        "const devBox=$('actDevicesCheckboxes');let listWrap=null;if(devBox){devBox.replaceChildren();devBox.style.cssText='display:flex;flex-direction:column;gap:6px;width:100%;';listWrap=document.createElement('div');listWrap.id='actDevOrderedList';listWrap.className='queue-list';listWrap.style.cssText='min-height:40px;max-height:180px;margin-bottom:6px;';const addRow=document.createElement('div');addRow.style.cssText='display:flex;gap:6px;align-items:center;margin-top:4px;';const addSel=document.createElement('select');addSel.style.flex='1';(wizardInventory?.devices||[]).forEach(d=>{const opt=document.createElement('option');opt.value=d.id;opt.textContent=d.name;addSel.append(opt);});const addBtn=document.createElement('button');addBtn.type='button';addBtn.className='secondary mini';addBtn.textContent='+ Add Device to Order';addBtn.addEventListener('click',()=>{const did=addSel.value;if(!did)return;const exists=Array.from(listWrap.querySelectorAll('.act-dev-row')).some(r=>r.dataset.devId===String(did));if(exists){alert('Device already added.');return;}renderActDeviceRow(listWrap,did);});addRow.append(addSel,addBtn);const hint=document.createElement('div');hint.className='muted mini';hint.textContent='Start/Stop Order: Devices power ON in order (1..N) and power OFF in reverse (N..1).';devBox.append(listWrap,addRow,hint);}"
        "if(!actId){if(title)title.textContent='Create New Activity';if(idInput)idInput.value='';if(nameInput)nameInput.value='';if(typeSelect)typeSelect.value='VirtualTelevision';if(orderInput)orderInput.value=String((activityInventory?.activities?.length||1));if(delBtn)delBtn.style.display='none';}"
        "else{const acts=(activityInventory?.activities||[]).slice();if(!acts.some(a=>String(a.id)==='-1'))acts.unshift({id:'-1',name:'PowerOff',type:'PowerOff',order:0,deviceIds:[],startSteps:[],stopSteps:[]});const act=acts.find(a=>String(a.id)===String(actId));if(!act)return;if(title)title.textContent=(String(act.id)==='-1')?'Edit PowerOff Settings':('Edit Activity: '+act.name);if(idInput)idInput.value=act.id;if(nameInput)nameInput.value=act.name;if(typeSelect)typeSelect.value=act.type||'VirtualTelevision';if(orderInput)orderInput.value=String(act.order||1);if(delBtn)delBtn.style.display=(String(act.id)==='-1')?'none':'';"
        "if(listWrap){(act.deviceIds||[]).forEach(did=>renderActDeviceRow(listWrap,did));}"
        "(act.startSteps||[]).forEach(s=>renderStepRow(startList,s));(act.stopSteps||[]).forEach(s=>renderStepRow(stopList,s));}"
        "box.style.display='block';box.scrollIntoView({behavior:'smooth',block:'start'});}"
        "async function saveActivity(){const id=($('actEditId')?.value||'').trim(),name=($('actEditName')?.value||'').trim(),type=($('actEditType')?.value||'').trim(),order=($('actEditOrder')?.value||'1').trim(),listWrap=$('actDevOrderedList'),deviceIds=listWrap?Array.from(listWrap.querySelectorAll('.act-dev-row')).map(r=>r.dataset.devId).join(','):'',startSteps=serializeContainerSteps('actStartStepsList'),stopSteps=serializeContainerSteps('actStopStepsList'),status=$('actEditorStatus');"
        "if(!name){if(status)status.textContent='Activity name is required.';return;}if(status)status.textContent='Saving activity...';"
        "try{const j=await postJson('/api/activity-save',{id:id,name:name,type:type,order:order,deviceIds:deviceIds,startSteps:startSteps,stopSteps:stopSteps});if(status)status.textContent='Saved: '+(j.message||'ok');await loadActivities();setTimeout(()=>{const b=$('actEditorBox');if(b)b.style.display='none';if(status)status.textContent='';},800);}catch(e){if(status)status.textContent='Save failed: '+(e.message||e);}}"
        "async function deleteActivity(id,name){if(String(id)==='-1'){alert('PowerOff activity cannot be deleted.');return;}if(!confirm('Are you sure you want to delete \"'+(name||id)+'\"?'))return;try{const j=await postJson('/api/activity-delete',{id:id});await loadActivities();const curId=$('actEditId')?.value;if(curId===id){const b=$('actEditorBox');if(b)b.style.display='none';}}catch(e){alert('Delete failed: '+(e.message||e));}}"
        "const actRefreshBtn=$('actRefreshBtn');if(actRefreshBtn)actRefreshBtn.addEventListener('click',loadActivities);"
        "const actNewBtn=$('actNewBtn');if(actNewBtn)actNewBtn.addEventListener('click',()=>openActivityEditor(null));"
        "const actPowerOffBtn=$('actPowerOffBtn');if(actPowerOffBtn)actPowerOffBtn.addEventListener('click',stopActivity);"
        "const actSaveBtn=$('actSaveBtn');if(actSaveBtn)actSaveBtn.addEventListener('click',saveActivity);"
        "const actCancelBtn=$('actCancelBtn');if(actCancelBtn)actCancelBtn.addEventListener('click',()=>{const b=$('actEditorBox');if(b)b.style.display='none';});"
        "const actEditorCloseBtn=$('actEditorCloseBtn');if(actEditorCloseBtn)actEditorCloseBtn.addEventListener('click',()=>{const b=$('actEditorBox');if(b)b.style.display='none';});"
        "const actAddStartCmdBtn=$('actAddStartCmdBtn');if(actAddStartCmdBtn)actAddStartCmdBtn.addEventListener('click',()=>renderStepRow($('actStartStepsList'),{type:'IRCommand',delay:0}));"
        "const actAddStartDelayBtn=$('actAddStartDelayBtn');if(actAddStartDelayBtn)actAddStartDelayBtn.addEventListener('click',()=>renderStepRow($('actStartStepsList'),{type:'Delay',delay:1000}));"
        "const actTestStartSeqBtn=$('actTestStartSeqBtn');if(actTestStartSeqBtn)actTestStartSeqBtn.addEventListener('click',()=>testActivitySeq('actStartStepsList'));"
        "const actAddStopCmdBtn=$('actAddStopCmdBtn');if(actAddStopCmdBtn)actAddStopCmdBtn.addEventListener('click',()=>renderStepRow($('actStopStepsList'),{type:'IRCommand',delay:0}));"
        "const actAddStopDelayBtn=$('actAddStopDelayBtn');if(actAddStopDelayBtn)actAddStopDelayBtn.addEventListener('click',()=>renderStepRow($('actStopStepsList'),{type:'Delay',delay:1000}));"
        "const actTestStopSeqBtn=$('actTestStopSeqBtn');if(actTestStopSeqBtn)actTestStopSeqBtn.addEventListener('click',()=>testActivitySeq('actStopStepsList'));"
        "function updateTransitionDropdowns(){const fromSel=$('previewFromAct'),toSel=$('previewToAct');if(!fromSel||!toSel)return;const curFrom=fromSel.value,curTo=toSel.value;fromSel.replaceChildren();toSel.replaceChildren();const acts=(activityInventory?.activities||[]).slice();if(!acts.some(a=>String(a.id)==='-1'))acts.unshift({id:'-1',name:'PowerOff'});acts.forEach(a=>{const opt1=document.createElement('option');opt1.value=String(a.id);opt1.textContent=a.name+(String(a.id)==='-1'?' (Power Off)':'');const opt2=document.createElement('option');opt2.value=String(a.id);opt2.textContent=a.name+(String(a.id)==='-1'?' (Power Off)':'');fromSel.append(opt1);toSel.append(opt2);});const curId=String(activityInventory?.currentId||'-1');fromSel.value=curFrom||curId;if(curTo){toSel.value=curTo;}else{const other=acts.find(a=>String(a.id)!==curId);if(other)toSel.value=String(other.id);}}"
        "async function simulateActivityTransition(){if(!wizardInventory)await loadWizardInventory();const fromId=$('previewFromAct')?.value,toId=$('previewToAct')?.value,box=$('previewResultsBox'),summary=$('previewSummary'),list=$('previewTimeline');if(!box||!summary||!list)return;list.replaceChildren();if(!fromId||!toId){summary.textContent='Select source and target activities.';box.style.display='block';return;}const acts=(activityInventory?.activities||[]).slice();if(!acts.some(a=>String(a.id)==='-1'))acts.unshift({id:'-1',name:'PowerOff',startSteps:[],stopSteps:[],deviceIds:[]});const fromAct=acts.find(a=>String(a.id)===String(fromId))||{id:'-1',name:'PowerOff',startSteps:[],stopSteps:[],deviceIds:[]},toAct=acts.find(a=>String(a.id)===String(toId))||{id:'-1',name:'PowerOff',startSteps:[],stopSteps:[],deviceIds:[]},devMap={};(wizardInventory?.devices||[]).forEach(d=>{devMap[String(d.id)]=d.name||('Device '+d.id);});if(fromId===toId){summary.innerHTML='Source and target are identical ('+escHtml(fromAct.name)+'). No transition steps required.';box.style.display='block';return;}const getDev=(id)=>(wizardInventory?.devices||[]).find(x=>String(x.id)===String(id));const isAlwaysOn=(id)=>{const d=getDev(id);return !!(d&&(d.isPowerAlwaysOn===true||d.is_power_always_on===true||d.isPowerAlwaysOn==='true'||d.is_power_always_on==='true'||d.isPowerAlwaysOn===1||d.is_power_always_on===1));};const fromDevs=(fromAct.deviceIds||[]).map(String),toDevs=(toAct.deviceIds||[]).map(String),devsToOff=fromDevs.filter(d=>!toDevs.includes(d)&&!isAlwaysOn(d)),devsToOn=(String(toId)==='-1')?[]:toDevs.filter(d=>!fromDevs.includes(d)&&!isAlwaysOn(d)),planned=[];(fromAct.stopSteps||[]).forEach(s=>{planned.push({phase:'Leave Sequence',dev:devMap[String(s.deviceId)]||(s.deviceId?'Device '+s.deviceId:'Hub'),cmd:s.command||s.type,delay:Number(s.delay)||0});});const inPowerOffSeq=(did)=>{if(String(toId)!=='-1')return false;return(toAct.startSteps||[]).some(s=>String(s.deviceId)===String(did));};devsToOff.slice().reverse().forEach(did=>{if(inPowerOffSeq(did))return;const d=getDev(did),dName=devMap[did]||('Device '+did),steps=d?.powerOffSteps||[];if(steps.length){steps.forEach(st=>{const isDel=String(st.type).toLowerCase()==='delay';planned.push({phase:'Power Off Sequence',dev:dName,cmd:isDel?('Delay '+(st.delayMs||1000)+'ms'):(st.command||st.type),delay:Number(st.delayMs)||(isDel?1000:250)});});}else{const cmds=d?.commands||[],pOff=cmds.find(c=>c.name&&c.name.toLowerCase()==='poweroff')||cmds.find(c=>c.name&&(c.name.toLowerCase()==='powertoggle'||c.name.toLowerCase()==='power')),cmdName=pOff?pOff.name:'PowerOff',delay=Number(d?.interDeviceDelay)||250;planned.push({phase:'Power Off Device',dev:dName,cmd:cmdName,delay:delay});}});let maxWarmup=0;devsToOn.forEach(did=>{const d=getDev(did),dName=devMap[did]||('Device '+did),steps=d?.powerOnSteps||[];if(steps.length){steps.forEach(st=>{const isDel=String(st.type).toLowerCase()==='delay';planned.push({phase:'Power On Sequence',dev:dName,cmd:isDel?('Delay '+(st.delayMs||1000)+'ms'):(st.command||st.type),delay:Number(st.delayMs)||(isDel?1000:250)});});}else{const cmds=d?.commands||[],pOn=cmds.find(c=>c.name&&c.name.toLowerCase()==='poweron')||cmds.find(c=>c.name&&(c.name.toLowerCase()==='powertoggle'||c.name.toLowerCase()==='power')),cmdName=pOn?pOn.name:'PowerOn',delay=Number(d?.interDeviceDelay)||250;planned.push({phase:'Power On Device',dev:dName,cmd:cmdName,delay:delay});}const pod=Number(d?.powerOnDelay)||0;if(pod>maxWarmup)maxWarmup=pod;});if(maxWarmup>0&&devsToOn.length>0){planned.push({phase:'Warmup Delay',dev:'Hub',cmd:'Warmup '+maxWarmup+'ms',delay:maxWarmup});}(toAct.startSteps||[]).forEach(s=>{planned.push({phase:'Enter Sequence',dev:devMap[String(s.deviceId)]||(s.deviceId?'Device '+s.deviceId:'Hub'),cmd:s.command||s.type,delay:Number(s.delay)||0});});let elapsedMs=0;planned.forEach((st,idx)=>{elapsedMs+=st.delay;const row=document.createElement('div');row.className='queue-row';row.style.cssText='display:grid;grid-template-columns:30px 140px 1fr 1fr 90px 80px;align-items:center;gap:8px;padding:6px 8px;';row.innerHTML='<div class=\"muted mini\">#'+(idx+1)+'</div><div><span class=\"badge ghost mini\">'+escHtml(st.phase)+'</span></div><div><strong>'+escHtml(st.dev)+'</strong></div><div>'+escHtml(st.cmd)+'</div><div class=\"muted mini\">'+st.delay+' ms</div><div class=\"muted mini\" style=\"text-align:right\">+'+(elapsedMs/1000).toFixed(2)+'s</div>';list.append(row);});if(!planned.length){list.innerHTML='<div class=\"muted mini\" style=\"padding:8px\">No steps required for this transition.</div>';}summary.innerHTML='Transition from <strong>'+escHtml(fromAct.name)+'</strong> to <strong>'+escHtml(toAct.name)+'</strong>: <strong>'+planned.length+' step(s)</strong>, estimated total delay <strong>'+(elapsedMs/1000).toFixed(2)+' seconds</strong>.';box.style.display='block';box.scrollIntoView({behavior:'smooth',block:'nearest'});}"
        "const prevSimBtn=$('previewSimulateBtn');if(prevSimBtn)prevSimBtn.addEventListener('click',simulateActivityTransition);"
        "document.addEventListener('click',e=>{"
        "const up=e.target.closest('.power-step-up');"
        "if(up){const r=up.closest('.power-step-row');if(r&&r.previousElementSibling)r.parentNode.insertBefore(r,r.previousElementSibling);return;}"
        "const down=e.target.closest('.power-step-down');"
        "if(down){const r=down.closest('.power-step-row');if(r&&r.nextElementSibling)r.parentNode.insertBefore(r.nextElementSibling,r);return;}"
        "const del=e.target.closest('.power-step-del');"
        "if(del){const r=del.closest('.power-step-row');if(r)r.remove();return;}"
        "const addCmd=e.target.closest('.power-add-cmd');"
        "if(addCmd){const f=addCmd.closest('.device-power-form'),t=addCmd.dataset.seqType,l=f?.querySelector('.power-seq-list[data-seq-type=\"'+t+'\"]'),d=f?.dataset.deviceId||'';"
        "if(l){const r=document.createElement('div');r.className='queue-row power-step-row';r.style.cssText='display:flex;align-items:center;gap:8px;margin-bottom:6px;';"
        "r.innerHTML='<span class=\"pill mini\" style=\"min-width:65px;text-align:center;\">Command</span><input type=\"text\" list=\"device-cmds-'+d+'\" class=\"power-step-cmd\" value=\"\" placeholder=\"Command name (e.g. PowerToggle)\" style=\"flex:1;\"><div class=\"actions\" style=\"margin-left:auto;display:flex;gap:4px;\"><button type=\"button\" class=\"ghost mini power-step-up\" title=\"Move Up\">&uarr;</button><button type=\"button\" class=\"ghost mini power-step-down\" title=\"Move Down\">&darr;</button><button type=\"button\" class=\"danger ghost mini power-step-del\" title=\"Delete\">&times;</button></div>';"
        "l.appendChild(r);}return;}"
        "const addDel=e.target.closest('.power-add-delay');"
        "if(addDel){const f=addDel.closest('.device-power-form'),t=addDel.dataset.seqType,l=f?.querySelector('.power-seq-list[data-seq-type=\"'+t+'\"]');"
        "if(l){const r=document.createElement('div');r.className='queue-row power-step-row';r.style.cssText='display:flex;align-items:center;gap:8px;margin-bottom:6px;';"
        "r.innerHTML='<span class=\"pill mini\" style=\"min-width:65px;text-align:center;\">Delay</span><input type=\"number\" class=\"power-step-delay\" min=\"50\" max=\"60000\" step=\"50\" value=\"1000\" style=\"width:120px;\"> ms<div class=\"actions\" style=\"margin-left:auto;display:flex;gap:4px;\"><button type=\"button\" class=\"ghost mini power-step-up\" title=\"Move Up\">&uarr;</button><button type=\"button\" class=\"ghost mini power-step-down\" title=\"Move Down\">&darr;</button><button type=\"button\" class=\"danger ghost mini power-step-del\" title=\"Delete\">&times;</button></div>';"
        "l.appendChild(r);}return;}"
        "});"
        "function serializePowerSeq(cont){if(!cont)return '';const lines=[];cont.querySelectorAll('.power-step-row').forEach(r=>{const di=r.querySelector('.power-step-delay'),ci=r.querySelector('.power-step-cmd');if(di){lines.push('Delay|'+(parseInt(di.value,10)||1000));}else if(ci){const v=ci.value.trim();if(v)lines.push('Command|'+v);}});return lines.join('\\n');}"
        "document.querySelectorAll('.device-power-form').forEach(f=>{"
        "f.addEventListener('submit',async e=>{"
        "e.preventDefault();const devId=f.dataset.deviceId,onL=f.querySelector('.power-seq-list[data-seq-type=\"on\"]'),offL=f.querySelector('.power-seq-list[data-seq-type=\"off\"]');"
        "const onSeq=serializePowerSeq(onL),offSeq=serializePowerSeq(offL);"
        "const onH=f.querySelector('.power-seq-hidden[data-seq-type=\"on\"]'),offH=f.querySelector('.power-seq-hidden[data-seq-type=\"off\"]');"
        "if(onH)onH.value=onSeq;if(offH)offH.value=offSeq;"
        "const delay=parseInt(f.querySelector('.device-power-delay')?.value,10)||1500,alwaysOn=!!f.querySelector('.device-power-always-on')?.checked,st=f.querySelector('.power-save-status');"
        "if(st)st.textContent='Saving...';"
        "try{const res=await postJson('/api/device-power-save',{deviceId:devId,powerOnDelay:delay,isPowerAlwaysOn:alwaysOn?'1':'0',powerOnSeq:onSeq,powerOffSeq:offSeq});"
        "if(st)st.textContent=(res.message||'Saved power settings!');setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Save failed: '+(err.message||err);}"
        "});});"
        "document.querySelectorAll('.device-mqtt-form').forEach(f=>{"
        "f.addEventListener('submit',async e=>{"
        "e.preventDefault();const devId=f.dataset.deviceId,en=!!f.querySelector('.device-mqtt-enabled')?.checked,topic=f.querySelector('.device-mqtt-topic')?.value.trim()||'',pulse=parseInt(f.querySelector('.device-mqtt-pulse')?.value,10)||1000,st=f.querySelector('.mqtt-save-status');"
        "if(st)st.textContent='Saving...';"
        "try{const res=await postJson('/api/device-mqtt',{deviceId:devId,mqttEnabled:en?'1':'0',mqttTopic:topic,mqttPulseMs:pulse});"
        "if(st)st.textContent=(res.message||'Saved MQTT settings!');await loadWizardInventory();setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Save failed: '+(err.message||err);}"
        "});});"
        "document.querySelectorAll('.mqtt-test-btn').forEach(b=>{"
        "b.addEventListener('click',async e=>{"
        "const devId=b.dataset.deviceId,f=b.closest('.device-mqtt-form'),st=f?.querySelector('.mqtt-save-status');"
        "if(st)st.textContent='Sending test pulse...';"
        "try{const res=await postJson('/api/device-mqtt-test',{deviceId:devId});"
        "if(st)st.textContent=(res.message||'Test pulse sent!');setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Test failed: '+(err.message||err);}"
        "});});"
        "document.querySelectorAll('.device-details-form').forEach(f=>{"
        "f.addEventListener('submit',async e=>{"
        "e.preventDefault();const devId=f.dataset.deviceId,st=f.querySelector('.details-save-status'),btn=f.querySelector('button[type=submit]');"
        "const fd=new FormData(f);const data=Object.fromEntries(fd.entries());"
        "if(st)st.textContent='Saving...';if(btn)btn.disabled=true;"
        "try{const res=await postJson('/api/device-save',data);"
        "if(st)st.textContent=(res.message||'Saved device details!');await loadWizardInventory();setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Save failed: '+(err.message||err);}"
        "finally{if(btn)btn.disabled=false;}"
        "});});"
        "const mf=$('mqttForm');if(mf)mf.addEventListener('submit',async e=>{e.preventDefault();const st=$('mqttSaveStatus'),btn=mf.querySelector('button[type=submit]');if(st)st.textContent='Saving...';if(btn)btn.disabled=true;try{const res=await postForm(mf);if(st)st.textContent=(res.message||'Saved MQTT settings!');setTimeout(()=>{if(st)st.textContent='';},3000);refreshMqttStatus();}catch(err){if(st)st.textContent='Save failed: '+(err.message||err);}finally{if(btn)btn.disabled=false;}});"
        "async function refreshMqttStatus(){try{const r=await fetch('/api/mqtt-status');const j=await r.json();if(!j||!j.ok)return;const bDash=$('dashMqttBadge'),bConn=$('mqttConnBadge'),dDash=$('dashMqttDetail');const isUp=!!j.connected;if(bDash){bDash.className='badge '+(isUp?'ok':'bad');bDash.textContent=isUp?'connected':'not connected';}if(bConn){bConn.className='badge '+(isUp?'ok':'bad');bConn.textContent=isUp?'connected':'not connected';}if(dDash){dDash.textContent=j.enabled?'bridge enabled':'bridge disabled';}}catch(e){}}"
        "const wf=$('wifiForm');if(wf)wf.addEventListener('submit',async e=>{e.preventDefault();const applyVal=e.submitter?.value||'save';const st=$('wifiSaveStatus'),btn=e.submitter;if(st)st.textContent=applyVal==='reboot'?'Saving and rebooting...':'Saving...';if(btn)btn.disabled=true;try{const res=await postForm(wf,{apply:applyVal});if(st)st.textContent=(res.message||(applyVal==='reboot'?'Wi-Fi saved. Rebooting hub...':'Wi-Fi saved!'));if(applyVal!=='reboot')setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Save failed: '+(err.message||err);}finally{if(btn&&applyVal!=='reboot')btn.disabled=false;}});"
        "const ef=$('ethernetForm');if(ef)ef.addEventListener('submit',async e=>{e.preventDefault();const applyVal=e.submitter?.value||'save';const st=$('ethSaveStatus'),btn=e.submitter;if(st)st.textContent=applyVal==='reboot'?'Saving and rebooting...':(applyVal==='reconfigure'?'Saving and applying...':'Saving...');if(btn)btn.disabled=true;try{const res=await postForm(ef,{apply:applyVal});if(st)st.textContent=(res.message||(applyVal==='reboot'?'Ethernet saved. Rebooting hub...':'Ethernet saved!'));if(applyVal!=='reboot')setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Save failed: '+(err.message||err);}finally{if(btn&&applyVal!=='reboot')btn.disabled=false;}});"
        "const em=$('ethModeSelect');if(em)em.addEventListener('change',()=>{const sf=$('ethStaticFields');if(sf)sf.classList.toggle('hidden',em.value!=='static');});"
        "const bf=$('backupImportForm');if(bf)bf.addEventListener('submit',async e=>{e.preventDefault();const st=$('backupImportStatus'),btn=bf.querySelector('button[type=submit]');if(st)st.textContent='Restoring...';if(btn)btn.disabled=true;try{const res=await postForm(bf);if(st)st.textContent=(res.message||'Backup restored!');wizardInventory=null;await loadWizardInventory();setTimeout(()=>{if(st)st.textContent='';},4000);}catch(err){if(st)st.textContent='Restore failed: '+(err.message||err);}finally{if(btn)btn.disabled=false;}});"
        "const saf=$('systemAuthForm');if(saf)saf.addEventListener('submit',async e=>{e.preventDefault();const st=$('authSaveStatus'),btn=saf.querySelector('button[type=submit]');if(st)st.textContent='Saving...';if(btn)btn.disabled=true;try{const res=await postForm(saf,{action:'auth'});if(st)st.textContent=(res.message||'Saved sign-in settings!');setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Save failed: '+(err.message||err);}finally{if(btn)btn.disabled=false;}});"
        "const syf=$('systemActionsForm');if(syf)syf.addEventListener('submit',async e=>{e.preventDefault();const actVal=e.submitter?.value||'rediscover';const st=$('sysActionStatus'),btn=e.submitter;if(st)st.textContent=actVal==='reboot'?'Rebooting hub...':'Refreshing discovery...';if(btn)btn.disabled=true;try{const res=await postForm(syf,{action:actVal});if(st)st.textContent=(res.message||(actVal==='reboot'?'Rebooting...':'Discovery refreshed!'));if(actVal!=='reboot')setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Action failed: '+(err.message||err);}finally{if(btn&&actVal!=='reboot')btn.disabled=false;}});"
        "document.addEventListener('submit',async e=>{const form=e.target;if(!form)return;"
        "if(form.classList.contains('ir-command-form')){e.preventDefault();const st=form.querySelector('.save-status'),btn=form.querySelector('button[type=submit]');if(st)st.textContent='Saving...';if(btn)btn.disabled=true;try{const res=await postForm(form);if(st)st.textContent=(res.message||'Saved!');wizardInventory=null;await loadWizardInventory();setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Save failed: '+(err.message||err);}finally{if(btn)btn.disabled=false;}}"
        "else if(form.classList.contains('ir-delete-device-form')){e.preventDefault();if(!confirm('Are you sure you want to delete this device?'))return;const st=form.querySelector('.save-status'),btn=form.querySelector('button[type=submit]');if(st)st.textContent='Deleting...';if(btn)btn.disabled=true;try{await postForm(form);const card=form.closest('.ir-stored-layout')||form.closest('.card')||form.closest('.panel');if(card)card.remove();wizardInventory=null;await loadWizardInventory();}catch(err){if(st)st.textContent='Delete failed: '+(err.message||err);if(btn)btn.disabled=false;}}"
        "else if(form.classList.contains('ir-delete-command-form')){e.preventDefault();const st=form.querySelector('.save-status'),btn=form.querySelector('button[type=submit]');if(st)st.textContent='...';if(btn)btn.disabled=true;try{await postForm(form);const row=form.closest('.ir-command-row');if(row)row.remove();wizardInventory=null;await loadWizardInventory();}catch(err){if(st)st.textContent='Failed: '+(err.message||err);if(btn)btn.disabled=false;}}"
        "else if(form.classList.contains('bt-command-form')){e.preventDefault();const st=form.querySelector('.save-status'),btn=form.querySelector('button[type=submit]');if(st)st.textContent='Saving...';if(btn)btn.disabled=true;try{const res=await postForm(form);if(st)st.textContent=(res.message||'Saved!');wizardInventory=null;await loadWizardInventory();setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Save failed: '+(err.message||err);}finally{if(btn)btn.disabled=false;}}"
        "else if(form.classList.contains('bt-device-form')){e.preventDefault();const st=form.querySelector('.save-status'),btn=form.querySelector('button[type=submit]');if(st)st.textContent='Saving...';if(btn)btn.disabled=true;try{const res=await postForm(form);if(st)st.textContent=(res.message||'Saved!');wizardInventory=null;await loadWizardInventory();setTimeout(()=>{if(st)st.textContent='';},3000);}catch(err){if(st)st.textContent='Save failed: '+(err.message||err);}finally{if(btn)btn.disabled=false;}}"
        "else if(form.classList.contains('bt-delete-device-form')){e.preventDefault();if(!confirm('Are you sure you want to delete this Bluetooth device?'))return;const st=form.querySelector('.save-status'),btn=form.querySelector('button[type=submit]');if(st)st.textContent='Deleting...';if(btn)btn.disabled=true;try{await postForm(form);const card=form.closest('.bt-device-card');if(card)card.remove();wizardInventory=null;await loadWizardInventory();}catch(err){if(st)st.textContent='Delete failed: '+(err.message||err);if(btn)btn.disabled=false;}}"
        "else if(form.classList.contains('bt-send-command-form')){e.preventDefault();const st=form.querySelector('.save-status'),btn=form.querySelector('button[type=submit]');if(st)st.textContent='Running...';if(btn)btn.disabled=true;try{const res=await postForm(form);if(st)st.textContent=(res.message||'Sent!');setTimeout(()=>{if(st)st.textContent='';},2000);}catch(err){if(st)st.textContent='Failed: '+(err.message||err);}finally{if(btn)btn.disabled=false;}}"
        "else if(form.classList.contains('bt-delete-command-form')){e.preventDefault();const st=form.querySelector('.save-status'),btn=form.querySelector('button[type=submit]');if(st)st.textContent='...';if(btn)btn.disabled=true;try{await postForm(form);const row=form.closest('.command');if(row)row.remove();wizardInventory=null;await loadWizardInventory();}catch(err){if(st)st.textContent='Failed: '+(err.message||err);if(btn)btn.disabled=false;}}"
        "});"
        "setInterval(()=>{const a=$('view-activities'),d=$('view-overview'),m=$('view-mqtt');if((a&&a.classList.contains('active'))||(d&&d.classList.contains('active')))loadActivities();if((d&&d.classList.contains('active'))||(m&&m.classList.contains('active')))refreshMqttStatus();},4000);"
        "const B25_BTN_LABELS={power:'Power',input:'Input / Source','1':'1','2':'2','3':'3','4':'4','5':'5','6':'6','7':'7','8':'8','9':'9',guide:'Guide','0':'0',info:'Info',red:'Red',green:'Green',yellow:'Yellow',blue:'Blue',watchlist:'Watchlist',assistant:'Assistant',settings:'Settings',up:'Up',down:'Down',left:'Left',right:'Right',select:'Select / OK',back:'Back',home:'Home',live_tv:'Live TV',vol_up:'Vol Up',vol_down:'Vol Down',mute:'Mute',ch_up:'CH Up',ch_down:'CH Down',youtube:'YouTube',netflix:'Netflix',prime_video:'Prime Video',google_play:'Google Play'};"
        "async function loadRemoteMappings(){try{"
        "await loadWizardInventory();"
        "const[mapRes,actRes]=await Promise.all([fetch('/api/remote-mapping').then(r=>r.json()),fetch('/api/activities').then(r=>r.json())]);"
        "if(mapRes&&mapRes.ok){b25MapData=mapRes.mapping||{remote:{name:'Homatics B25'},activities:{}};"
        "if(!b25MapData.activities)b25MapData.activities={};"
        "const logEl=$('b25PairLog');if(logEl)logEl.textContent='Daemon: '+(mapRes.running?'RUNNING':'STOPPED')+' | Remote: '+(mapRes.connected?'CONNECTED':'DISCONNECTED / SLEEPING')+(b25MapData.remote?.bdaddr?(' | Paired: '+b25MapData.remote.bdaddr):'');}"
        "const actSel=$('b25ActivitySelect');if(actSel){const curVal=actSel.value||'-1';actSel.replaceChildren();const optOff=document.createElement('option');optOff.value='-1';optOff.textContent='Off / Idle (Default)';actSel.append(optOff);"
        "(actRes?.activities||[]).forEach(a=>{if(String(a.id)==='-1')return;const opt=document.createElement('option');opt.value=a.id;opt.textContent=a.name+' (ID: '+a.id+')';actSel.append(opt);});actSel.value=curVal;}"
        "const tgtActSel=$('b25TargetActivity');if(tgtActSel){tgtActSel.replaceChildren();(actRes?.activities||[]).forEach(a=>{if(String(a.id)==='-1')return;const opt=document.createElement('option');opt.value=a.id;opt.textContent=a.name;tgtActSel.append(opt);});}"
        "populateB25DeviceDropdowns();updateB25HotspotBadges();selectB25Button(b25SelectedBtn);checkPairStatusOnLoad();"
        "}catch(e){console.error('loadRemoteMappings error',e);}}"
        "function populateB25DeviceDropdowns(){const devs=wizardInventory?.devices||[];const tgtDevSel=$('b25TargetDevice'),presetDevSel=$('b25PresetBtDevice');"
        "if(tgtDevSel){tgtDevSel.replaceChildren();devs.forEach(d=>{const opt=document.createElement('option');opt.value=d.id;opt.textContent=d.name+(d.transport===32?' [BT]':(d.mqtt?' [MQTT]':' [IR]'));tgtDevSel.append(opt);});updateB25CommandDropdown();}"
        "if(presetDevSel){presetDevSel.replaceChildren();devs.forEach(d=>{const opt=document.createElement('option');opt.value=d.id;opt.textContent=d.name+(d.transport===32?' [BT]':(d.mqtt?' [MQTT]':' [IR]'));presetDevSel.append(opt);});}}"
        "function updateB25CommandDropdown(){const devId=$('b25TargetDevice')?.value,cmdSel=$('b25TargetCommand');if(!cmdSel||!devId)return;cmdSel.replaceChildren();"
        "const dev=(wizardInventory?.devices||[]).find(d=>String(d.id)===String(devId));"
        "(dev?.commands||[]).forEach(c=>{const opt=document.createElement('option');opt.value=c.name;opt.textContent=c.name;cmdSel.append(opt);});}"
        "function updateB25HotspotBadges(){const actId=$('b25ActivitySelect')?.value||'-1',actMap=b25MapData.activities?.[actId]||{};"
        "document.querySelectorAll('.b25-hotspot').forEach(el=>{const btnId=el.dataset.btn,mapped=!!(actMap[btnId]&&actMap[btnId].action&&actMap[btnId].action!=='unmapped');el.classList.toggle('mapped',mapped);});}"
        "function selectB25Button(btnId){b25SelectedBtn=btnId;document.querySelectorAll('.b25-hotspot').forEach(el=>{el.classList.toggle('selected',el.dataset.btn===btnId);});"
        "const label=B25_BTN_LABELS[btnId]||btnId,nameEl=$('b25CurrentBtnName');if(nameEl)nameEl.textContent=label;"
        "const actId=$('b25ActivitySelect')?.value||'-1',actMap=b25MapData.activities?.[actId]||{},cfg=actMap[btnId]||{action:'unmapped'};"
        "const curAct=(cfg.action==='passthrough_bt'?'device_cmd':cfg.action)||'unmapped';"
        "const typeSel=$('b25ActionType');if(typeSel)typeSel.value=curAct;"
        "const tgtDev=$('b25TargetDevice');if(tgtDev&&cfg.targetDevice){tgtDev.value=cfg.targetDevice;updateB25CommandDropdown();}"
        "const tgtCmd=$('b25TargetCommand');if(tgtCmd&&cfg.command)tgtCmd.value=cfg.command;"
        "const tgtAct=$('b25TargetActivity');if(tgtAct&&(cfg.activityId||cfg.targetDevice))tgtAct.value=cfg.activityId||cfg.targetDevice;"
        "toggleB25ActionFields();}"
        "function toggleB25ActionFields(){const aType=$('b25ActionType')?.value||'unmapped',wrapDev=$('b25WrapTargetDevice'),wrapCmd=$('b25WrapTargetCommand'),wrapAct=$('b25WrapTargetActivity');"
        "if(wrapDev)wrapDev.classList.toggle('hidden',aType!=='device_cmd');"
        "if(wrapCmd)wrapCmd.classList.toggle('hidden',aType!=='device_cmd');"
        "if(wrapAct)wrapAct.classList.toggle('hidden',aType!=='activity_start');}"
        "function applyB25Button(){const actId=$('b25ActivitySelect')?.value||'-1';if(!b25MapData.activities[actId])b25MapData.activities[actId]={};"
        "const aType=$('b25ActionType')?.value||'unmapped';"
        "if(aType==='unmapped'){delete b25MapData.activities[actId][b25SelectedBtn];}"
        "else if(aType==='activity_start'){b25MapData.activities[actId][b25SelectedBtn]={action:'activity_start',activityId:$('b25TargetActivity')?.value||''};}"
        "else if(aType==='activity_stop'){b25MapData.activities[actId][b25SelectedBtn]={action:'activity_stop'};}"
        "else if(aType==='device_cmd'){b25MapData.activities[actId][b25SelectedBtn]={action:'device_cmd',targetDevice:$('b25TargetDevice')?.value||'',command:$('b25TargetCommand')?.value||''};}"
        "updateB25HotspotBadges();}"
        "function quickPassthroughAll(){const devId=$('b25PresetBtDevice')?.value;if(!devId){alert('Please select a target device.');return;}const actId=$('b25ActivitySelect')?.value||'-1';"
        "if(!b25MapData.activities[actId])b25MapData.activities[actId]={};"
        "const dev=(wizardInventory?.devices||[]).find(d=>String(d.id)===String(devId)),cmds=dev?.commands||[];"
        "const norm=s=>String(s||'').toLowerCase().replace(/[^a-z0-9]/g,'');"
        "const findC=names=>{for(const n of names){const t=norm(n);const m=cmds.find(c=>norm(c.name)===t);if(m)return m.name;}return null;};"
        "const btnDefs={up:['DirectionUp','Up','CursorUp'],down:['DirectionDown','Down','CursorDown'],left:['DirectionLeft','Left','CursorLeft'],right:['DirectionRight','Right','CursorRight'],select:['Select','OK','Enter'],back:['Back','Return','Exit','Escape'],home:['Home','Menu','TopMenu'],settings:['Settings','Menu','Options','Setup','Preferences'],guide:['Guide','EPG','ProgramGuide'],info:['Info','Display'],live_tv:['LiveTv','Live TV','Guide','TV'],vol_up:['VolumeUp','VolUp','Volume Up'],vol_down:['VolumeDown','VolDown','Volume Down'],mute:['Mute','VolumeMute'],ch_up:['ChannelUp','CHUp','PageUp','Next'],ch_down:['ChannelDown','CHDown','PageDown','Previous'],power:['PowerToggle','Power Toggle','Power','PowerOn'],input:['InputNext','Input','Source','InputSource'],red:['Red','ColorRed','ButtonRed'],green:['Green','ColorGreen','ButtonGreen'],yellow:['Yellow','ColorYellow','ButtonYellow'],blue:['Blue','ColorBlue','ButtonBlue'],'1':['Number1','1','Digit1'],'2':['Number2','2','Digit2'],'3':['Number3','3','Digit3'],'4':['Number4','4','Digit4'],'5':['Number5','5','Digit5'],'6':['Number6','6','Digit6'],'7':['Number7','7','Digit7'],'8':['Number8','8','Digit8'],'9':['Number9','9','Digit9'],'0':['Number0','0','Digit0'],youtube:['YouTube','Youtube'],netflix:['Netflix'],prime_video:['PrimeVideo','Prime Video','Amazon'],google_play:['GooglePlay','Google Play','PlayStore']};"
        "let count=0;Object.entries(btnDefs).forEach(([btn,names])=>{const cmd=findC(names);if(cmd){b25MapData.activities[actId][btn]={action:'device_cmd',targetDevice:devId,command:cmd};count++;}});"
        "updateB25HotspotBadges();selectB25Button(b25SelectedBtn);alert('Auto-mapped '+count+' button(s) to '+(dev?.name||devId)+' as Send Device Command (IR/BT).');}"
        "async function saveRemoteMappings(){const btn=$('b25SaveAllBtn');if(btn)btn.disabled=true;"
        "try{const r=await fetch('/api/remote-mapping-save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b25MapData)});const j=await r.json();if(!r.ok||j.ok===false)throw new Error(j.message||j.error||('http '+r.status));alert(j.message||'Remote mappings saved successfully!');await loadRemoteMappings();}catch(e){alert('Failed to save mappings: '+(e.message||e));}finally{if(btn)btn.disabled=false;}}"
        "async function scanB25Remote(){const btn=$('b25ScanBtn'),log=$('b25PairLog');if(btn)btn.disabled=true;if(log)log.textContent='Scanning for Bluetooth devices... (may take up to 10s)';"
        "try{await postJson('/api/remote-scan',{});let j=null;for(let i=0;i<20;i++){await new Promise(r=>setTimeout(r,500));try{const res=await fetch('/api/remote-scan-result');j=await res.json();if(!j||j.scanning===false)break;}catch(err){}}const list=$('b25DiscoveredList');if(list){list.replaceChildren();const devs=(j&&j.devices)||[];if(!devs.length){const opt=document.createElement('option');opt.value='';opt.textContent='No devices discovered';list.append(opt);}else{devs.forEach(d=>{const opt=document.createElement('option');opt.value=d.addr;opt.textContent=d.name+' ('+d.addr+')';list.append(opt);});}}if(log)log.textContent='Found '+((j&&j.devices)||[]).length+' Bluetooth device(s).';}catch(e){if(log)log.textContent='Scan error: '+(e.message||e);}finally{if(btn)btn.disabled=false;}}"
        "let b25PairTimer=null;"
        "function pollPairStatus(){if(b25PairTimer)clearInterval(b25PairTimer);const log=$('b25PairLog'),btn=$('b25PairBtn');b25PairTimer=setInterval(async()=>{try{const r=await fetch('/api/remote-pair-status');const j=await r.json();if(!j||!j.state||j.state==='idle'){return;}if(log){const rem=j.seconds_left>0?' ('+j.seconds_left+'s left)':'';log.textContent='['+j.state.toUpperCase()+'] '+(j.message||'')+rem;}if(j.state==='success'){clearInterval(b25PairTimer);b25PairTimer=null;if(btn)btn.disabled=false;if(log)log.textContent='[SUCCESS] Pairing complete! Remote is encrypted and ready.';b25MapData.remote.bdaddr=j.bdaddr||b25MapData.remote.bdaddr;await saveRemoteMappings();alert('Remote pairing successful!');}else if(j.state==='timeout'||j.state==='failed'){clearInterval(b25PairTimer);b25PairTimer=null;if(btn)btn.disabled=false;if(log)log.textContent='['+j.state.toUpperCase()+'] '+(j.message||'Pairing stopped.');}}catch(e){}},800);}"
        "async function checkPairStatusOnLoad(){try{const r=await fetch('/api/remote-pair-status');const j=await r.json();if(j&&j.state&&(j.state==='waiting_remote'||j.state==='negotiating_smp'||j.state==='initiating')){const btn=$('b25PairBtn');if(btn)btn.disabled=true;pollPairStatus();}}catch(e){}}"
        "async function pairB25Remote(){const list=$('b25DiscoveredList'),addr=list?.value;if(!addr){alert('Select a device to pair.');return;}const btn=$('b25PairBtn'),log=$('b25PairLog');if(btn)btn.disabled=true;if(log)log.textContent='Initiating pairing for '+addr+'... Hold Back+Home on remote until LED flashes!';try{await postJson('/api/remote-pair',{addr:addr});b25MapData.remote.bdaddr=addr;pollPairStatus();}catch(e){if(log)log.textContent='Pairing trigger failed: '+(e.message||e);if(btn)btn.disabled=false;}}"
        "document.querySelectorAll('.b25-hotspot').forEach(el=>el.addEventListener('click',()=>selectB25Button(el.dataset.btn)));"
        "const b25ActSel=$('b25ActivitySelect');if(b25ActSel)b25ActSel.addEventListener('change',()=>{updateB25HotspotBadges();selectB25Button(b25SelectedBtn);});"
        "const b25TypeSel=$('b25ActionType');if(b25TypeSel)b25TypeSel.addEventListener('change',toggleB25ActionFields);"
        "const b25DevSel=$('b25TargetDevice');if(b25DevSel)b25DevSel.addEventListener('change',updateB25CommandDropdown);"
        "const b25AppBtn=$('b25ApplyBtn');if(b25AppBtn)b25AppBtn.addEventListener('click',applyB25Button);"
        "const b25ClrBtn=$('b25ClearBtn');if(b25ClrBtn)b25ClrBtn.addEventListener('click',()=>{const s=$('b25ActionType');if(s)s.value='unmapped';applyB25Button();});"
        "const b25RunPreset=$('b25RunPresetBtn');if(b25RunPreset)b25RunPreset.addEventListener('click',quickPassthroughAll);"
        "const b25SaveBtn=$('b25SaveAllBtn');if(b25SaveBtn)b25SaveBtn.addEventListener('click',saveRemoteMappings);"
        "const b25Scan=$('b25ScanBtn');if(b25Scan)b25Scan.addEventListener('click',scanB25Remote);"
        "const b25Pair=$('b25PairBtn');if(b25Pair)b25Pair.addEventListener('click',pairB25Remote);"
        "const ELITE_BTN_LABELS={PowerOffActivity:'Power Off (All Off)',WatchTVActivity:'Activities Touch Key',Devices:'Devices Touch Key',DirectionUp:'D-PAD Up',DirectionDown:'D-PAD Down',DirectionLeft:'D-PAD Left',DirectionRight:'D-PAD Right',Select:'Select (OK)',Back:'Back',Exit:'Exit',Menu:'Menu',Info:'Info',VolumeUp:'Volume Up',VolumeDown:'Volume Down',VolumeMute:'Mute',ChannelUp:'Channel Up',ChannelDown:'Channel Down',PrevChannel:'Prev Channel',Play:'Play',Pause:'Pause',Stop:'Stop',Rewind:'Rewind',FastForward:'Fast Forward',Record:'Record',Guide:'Guide',Dvr:'DVR',Ha1:'Home Automation Light',Ha2:'Home Automation Sun',Ha3:'Home Automation Power',Ha4:'Home Automation Plug',RockerUp:'Automation Rocker Up',RockerDown:'Automation Rocker Down'};"
        "async function loadEliteMappings(){try{"
        "await loadWizardInventory();"
        "const[mapRes,actRes]=await Promise.all([fetch('/api/elite-mapping').then(r=>r.json()),fetch('/api/activities').then(r=>r.json())]);"
        "if(mapRes&&mapRes.ok){eliteMapData=mapRes.mapping||{remote:{name:'Harmony Elite'},activities:{}};}"
        "if(!eliteMapData.activities)eliteMapData.activities={};"
        "const actSel=$('eliteActivitySelect');if(actSel){const curVal=actSel.value||'-1';actSel.replaceChildren();const optOff=document.createElement('option');optOff.value='-1';optOff.textContent='Off / Idle (Default)';actSel.append(optOff);(actRes?.activities||[]).forEach(a=>{if(String(a.id)==='-1')return;const opt=document.createElement('option');opt.value=a.id;opt.textContent=a.name+' (ID: '+a.id+')';actSel.append(opt);});actSel.value=curVal;}"
        "const tgtActSel=$('eliteTargetActivity');if(tgtActSel){tgtActSel.replaceChildren();(actRes?.activities||[]).forEach(a=>{if(String(a.id)==='-1')return;const opt=document.createElement('option');opt.value=a.id;opt.textContent=a.name;tgtActSel.append(opt);});}"
        "populateEliteDeviceDropdowns();updateEliteHotspotBadges();selectEliteButton(eliteSelectedBtn);"
        "}catch(e){console.error('loadEliteMappings error',e);}}"
        "function populateEliteDeviceDropdowns(){const devs=wizardInventory?.devices||[];const tgtDevSel=$('eliteTargetDevice'),presetDevSel=$('elitePresetDev');if(tgtDevSel){tgtDevSel.replaceChildren();devs.forEach(d=>{const opt=document.createElement('option');opt.value=d.id;opt.textContent=d.name+(d.transport===32?' [BT]':(d.mqtt?' [MQTT]':' [IR]'));tgtDevSel.append(opt);});updateEliteCommandDropdown();}if(presetDevSel){presetDevSel.replaceChildren();devs.forEach(d=>{const opt=document.createElement('option');opt.value=d.id;opt.textContent=d.name+(d.transport===32?' [BT]':(d.mqtt?' [MQTT]':' [IR]'));presetDevSel.append(opt);});}}"
        "function updateEliteCommandDropdown(){const devId=$('eliteTargetDevice')?.value,cmdSel=$('eliteTargetCommand');if(!cmdSel||!devId)return;cmdSel.replaceChildren();const dev=(wizardInventory?.devices||[]).find(d=>String(d.id)===String(devId));(dev?.commands||[]).forEach(c=>{const opt=document.createElement('option');opt.value=c.name;opt.textContent=c.name;cmdSel.append(opt);});}"
        "function updateEliteHotspotBadges(){const actId=$('eliteActivitySelect')?.value||'-1',actMap=eliteMapData.activities?.[actId]||{};document.querySelectorAll('.elite-hotspot').forEach(el=>{const btnId=el.dataset.btn,mapped=!!(actMap[btnId]&&actMap[btnId].action&&actMap[btnId].action!=='unmapped');el.classList.toggle('mapped',mapped);});}"
        "function selectEliteButton(btnId){eliteSelectedBtn=btnId;document.querySelectorAll('.elite-hotspot').forEach(el=>el.classList.toggle('selected',el.dataset.btn===btnId));const label=ELITE_BTN_LABELS[btnId]||btnId,nameEl=$('eliteCurrentBtnName');if(nameEl)nameEl.textContent=label;const actId=$('eliteActivitySelect')?.value||'-1',actMap=eliteMapData.activities?.[actId]||{},cfg=actMap[btnId]||{action:'unmapped'};const curAct=(cfg.action==='passthrough_bt'?'device_cmd':cfg.action)||'unmapped';const typeSel=$('eliteActionType');if(typeSel)typeSel.value=curAct;const tgtDev=$('eliteTargetDevice');if(tgtDev&&cfg.targetDevice){tgtDev.value=cfg.targetDevice;updateEliteCommandDropdown();}const tgtCmd=$('eliteTargetCommand');if(tgtCmd&&cfg.command)tgtCmd.value=cfg.command;const tgtAct=$('eliteTargetActivity');if(tgtAct&&(cfg.activityId||cfg.targetDevice))tgtAct.value=cfg.activityId||cfg.targetDevice;toggleEliteActionFields();}"
        "function toggleEliteActionFields(){const aType=$('eliteActionType')?.value||'unmapped',wrapDev=$('eliteWrapTargetDevice'),wrapCmd=$('eliteWrapTargetCommand'),wrapAct=$('eliteWrapTargetActivity');if(wrapDev)wrapDev.classList.toggle('hidden',aType!=='device_cmd');if(wrapCmd)wrapCmd.classList.toggle('hidden',aType!=='device_cmd');if(wrapAct)wrapAct.classList.toggle('hidden',aType!=='activity_start');}"
        "function applyEliteButton(){const actId=$('eliteActivitySelect')?.value||'-1';if(!eliteMapData.activities[actId])eliteMapData.activities[actId]={};const aType=$('eliteActionType')?.value||'unmapped';if(aType==='unmapped'){delete eliteMapData.activities[actId][eliteSelectedBtn];}else if(aType==='activity_start'){eliteMapData.activities[actId][eliteSelectedBtn]={action:'activity_start',activityId:$('eliteTargetActivity')?.value||''};}else if(aType==='activity_stop'){eliteMapData.activities[actId][eliteSelectedBtn]={action:'activity_stop'};}else if(aType==='device_cmd'){eliteMapData.activities[actId][eliteSelectedBtn]={action:'device_cmd',targetDevice:$('eliteTargetDevice')?.value||'',command:$('eliteTargetCommand')?.value||''};}updateEliteHotspotBadges();}"
        "function quickPassthroughElite(){const devId=$('elitePresetDev')?.value;if(!devId){alert('Please select a target device.');return;}const actId=$('eliteActivitySelect')?.value||'-1';if(!eliteMapData.activities[actId])eliteMapData.activities[actId]={};const dev=(wizardInventory?.devices||[]).find(d=>String(d.id)===String(devId)),cmds=dev?.commands||[];const norm=s=>String(s||'').toLowerCase().replace(/[^a-z0-9]/g,'');const findC=names=>{for(const n of names){const ta=norm(n);const m=cmds.find(c=>norm(c.name)===ta);if(m)return m.name;}return null;};const btnDefs={DirectionUp:['DirectionUp','Up','CursorUp'],DirectionDown:['DirectionDown','Down','CursorDown'],DirectionLeft:['DirectionLeft','Left','CursorLeft'],DirectionRight:['DirectionRight','Right','CursorRight'],Select:['Select','OK','Enter'],Back:['Back','Return','Exit','Escape'],Menu:['Menu','Home','TopMenu','Options'],Exit:['Exit','Back','Home'],Info:['Info','Display'],Guide:['Guide','EPG'],Dvr:['Dvr','List','RecordedTV'],VolumeUp:['VolumeUp','VolUp','Volume Up'],VolumeDown:['VolumeDown','VolDown','Volume Down'],VolumeMute:['Mute','VolumeMute'],ChannelUp:['ChannelUp','CHUp','PageUp','Next'],ChannelDown:['ChannelDown','CHDown','PageDown','Previous'],PrevChannel:['PrevChannel','PreviousChannel','Last','Back'],Play:['Play','PlayPause'],Pause:['Pause','PlayPause'],Stop:['Stop'],Rewind:['Rewind','Reverse','ScanDown'],FastForward:['FastForward','Forward','ScanUp'],Record:['Record','Rec']};let count=0;Object.entries(btnDefs).forEach(([btn,names])=>{const cmd=findC(names);if(cmd){eliteMapData.activities[actId][btn]={action:'device_cmd',targetDevice:devId,command:cmd};count++;}});updateEliteHotspotBadges();selectEliteButton(eliteSelectedBtn);alert('Auto-mapped '+count+' button(s) to '+(dev?.name||devId)+' for this activity.');}"
        "async function saveEliteMappings(){const btn=$('eliteSaveAllBtn');if(btn)btn.disabled=true;try{const r=await fetch('/api/elite-mapping-save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(eliteMapData)});const j=await r.json();if(!r.ok||j.ok===false)throw new Error(j.message||j.error||('http '+r.status));alert(j.message||'Harmony Elite mappings saved successfully!');await loadEliteMappings();}catch(e){alert('Failed to save mappings: '+(e.message||e));}finally{if(btn)btn.disabled=false;}}"
        "document.querySelectorAll('.elite-hotspot').forEach(el=>el.addEventListener('click',()=>selectEliteButton(el.dataset.btn)));"
        "const elActSel=$('eliteActivitySelect');if(elActSel)elActSel.addEventListener('change',()=>{updateEliteHotspotBadges();selectEliteButton(eliteSelectedBtn);});"
        "const elTypeSel=$('eliteActionType');if(elTypeSel)elTypeSel.addEventListener('change',toggleEliteActionFields);"
        "const elDevSel=$('eliteTargetDevice');if(elDevSel)elDevSel.addEventListener('change',updateEliteCommandDropdown);"
        "const elAppBtn=$('eliteApplyBtn');if(elAppBtn)elAppBtn.addEventListener('click',applyEliteButton);"
        "const elClrBtn=$('eliteClearBtn');if(elClrBtn)elClrBtn.addEventListener('click',()=>{const s=$('eliteActionType');if(s)s.value='unmapped';applyEliteButton();});"
        "const elRunPre=$('eliteRunPresetBtn');if(elRunPre)elRunPre.addEventListener('click',quickPassthroughElite);"
        "const elSaveBtn=$('eliteSaveAllBtn');if(elSaveBtn)elSaveBtn.addEventListener('click',saveEliteMappings);"
        "async function refreshRfStatus(){try{const r=await fetch('/api/rf/status');const j=await r.json();const badge=$('rfStatusBadge'),addr=$('rfPairedAddr'),fw=$('rfFwVer'),state=$('rfPairState'),pBtn=$('rfPairBtn'),sBtn=$('rfStopPairBtn'),msg=$('rfPairMsg');if(j){if(addr)addr.textContent=j.paired?(j.rf_address||'Paired'):'None';if(fw)fw.textContent=j.fw_version||'Unknown';if(state)state.textContent=j.pairing_active?'PAIRING ACTIVE':'IDLE';if(badge){badge.textContent=j.paired?'PAIRED':'NOT PAIRED';badge.style.background=j.paired?'#027a48':'#b42318';}if(pBtn)pBtn.style.display=j.pairing_active?'none':'inline-block';if(sBtn)sBtn.style.display=j.pairing_active?'inline-block':'none';if(msg){if(j.pairing_active){msg.style.display='block';msg.textContent='Pairing mode active on Hub! Put remote in pairing mode.';}else msg.style.display='none';}}}catch(e){const badge=$('rfStatusBadge');if(badge){badge.textContent='OFFLINE';badge.style.background='#b42318';}}}"
        "async function pairRfRemote(){try{await fetch('/api/rf/pair',{method:'POST'});await refreshRfStatus();}catch(e){alert('Pair failed: '+e);}}"
        "async function stopPairRfRemote(){try{await fetch('/api/rf/stop-pair',{method:'POST'});await refreshRfStatus();}catch(e){alert('Stop pair failed: '+e);}}"
        "async function unpairRfRemote(){if(!confirm('Unpair Elite RF remote?'))return;try{await fetch('/api/rf/unpair',{method:'POST'});await refreshRfStatus();}catch(e){alert('Unpair failed: '+e);}}"
        "</script>",
        f);
    fputs("</div></main></body></html>", f);
}

static void status_panel(FILE *f, const struct mqtt_config *mqtt) {
    char uptime[128], version[128], activity[1024];
    char uptime_label[80], inventory_label[80];
    char update_badge[48], update_detail[224], update_age[64];
    char hub_id[64];
    struct update_check_state update_state;
    const char *update_class;
    int hub_id_ok;
    int mqtt_up = mqtt_is_connected() || tcp_established(mqtt->host, mqtt->port);
    int device_count = -1, command_count = 0;
    long now = time(NULL), age;
    read_text("/proc/uptime", uptime, sizeof(uptime));
    read_text("/etc/version", version, sizeof(version));
    chomp(uptime);
    chomp(version);
    {
        long seconds = atol(uptime);
        long days = seconds / 86400;
        long hours = (seconds % 86400) / 3600;
        long minutes = (seconds % 3600) / 60;
        if (days > 0) snprintf(uptime_label, sizeof(uptime_label), "%ldd %ldh %ldm", days, hours, minutes);
        else snprintf(uptime_label, sizeof(uptime_label), "%ldh %ldm", hours, minutes);
    }
    hub_id_ok = load_hub_id(hub_id, sizeof(hub_id));
    if (scan_ir_resource_stats(&device_count, &command_count, NULL, NULL) != 0) {
        device_count = -1;
        command_count = 0;
    }
    if (device_count < 0) snprintf(inventory_label, sizeof(inventory_label), "unknown");
    else snprintf(inventory_label, sizeof(inventory_label), "%d devices", device_count);
    load_update_state(&update_state);
    if (update_state.checked_at <= 0) {
        snprintf(update_badge, sizeof(update_badge), "not checked");
        snprintf(update_detail, sizeof(update_detail), "Checks automatically while this page is open.");
        update_class = "warn";
    } else {
        age = now - update_state.checked_at;
        if (age < 0) age = 0;
        if (age >= 86400) snprintf(update_age, sizeof(update_age), "%ldd ago", age / 86400);
        else if (age >= 3600) snprintf(update_age, sizeof(update_age), "%ldh ago", age / 3600);
        else if (age >= 60) snprintf(update_age, sizeof(update_age), "%ldm ago", age / 60);
        else snprintf(update_age, sizeof(update_age), "just now");
        if (update_state.changes < 0) {
            snprintf(update_badge, sizeof(update_badge), "check failed");
            update_class = "bad";
        } else if (update_state.available) {
            snprintf(update_badge, sizeof(update_badge), "%d update%s", update_state.changes, update_state.changes == 1 ? "" : "s");
            update_class = "warn";
        } else {
            snprintf(update_badge, sizeof(update_badge), "current");
            update_class = "ok";
        }
        snprintf(update_detail, sizeof(update_detail), "Checked %s. %s", update_age, update_state.message[0] ? update_state.message : "No details.");
    }
    {
        char cur_id[32];
        if (hub_id_ok) {
            activity_query_current(cur_id, sizeof(cur_id), activity, sizeof(activity));
        } else {
            activity[0] = '\0';
        }
    }
    fprintf(f, "<section id='view-overview' data-view='overview' class='section active'><div class='section-head'><div><h2>Dashboard</h2><div class='section-lead'>Check hub health, saved devices, and service status before changing settings.</div></div></div>");
    fprintf(f, "<div class='cards dashboard-cards'>");
    fprintf(f, "<div class='stat'><div class='label'>Firmware</div><div class='value'>"); html(f, version[0] ? version : "unknown"); fprintf(f, "</div></div>");
    fprintf(f, "<div class='stat'><div class='label'>Uptime</div><div class='value'>"); html(f, uptime_label); fprintf(f, "</div></div>");
    fprintf(f, "<div class='stat'><div class='label'>MQTT</div><div class='value'><span id='dashMqttBadge' class='badge %s'>%s</span></div><div id='dashMqttDetail' class='muted mini'>%s</div></div>",
        mqtt_up ? "ok" : "bad", mqtt_up ? "connected" : "not connected", mqtt->enabled ? "bridge enabled" : "bridge disabled");
    fprintf(f, "<div class='stat'><div class='label'>IR Inventory</div><div class='value'>");
    html(f, inventory_label);
    fprintf(f, "</div><div class='muted mini'>%d commands</div></div>", command_count);
    fprintf(f, "<div class='stat'><div class='label'>Active Activity</div><div class='value'><span id='dashActiveActName' class='badge ok'>PowerOff</span></div><div id='dashActiveActDetail' class='muted mini'>ID: -1</div></div>");
    fprintf(f, "<div class='stat'><div class='label'>Software update</div><div class='value'><span id='dashUpdateBadge' class='badge %s'>", update_class);
    html(f, update_badge);
    fprintf(f, "</span></div><div id='dashUpdateDetail' class='muted mini'>");
    html(f, update_detail);
    fprintf(f, "</div></div>");
    fprintf(f, "</div><div class='quick-actions'><button type='button' data-view-target='activities'><strong>Activities</strong><div class='muted mini'>Run activities, edit start/stop sequences and delays.</div></button><button type='button' data-view-target='control'><strong>Use a remote</strong><div class='muted mini'>Send saved buttons from the remote skin or command list.</div></button><button type='button' data-view-target='ir'><strong>Add or edit remotes</strong><div class='muted mini'>Create devices, search databases, learn buttons, and edit commands.</div></button><button type='button' data-view-target='lab'><strong>Bulk test IR codes</strong><div class='muted mini'>Search many code files, skip duplicates, then send a queue.</div></button><button type='button' data-view-target='mqtt'><strong>Set up Home Assistant</strong><div class='muted mini'>Configure MQTT topics, discovery, and state publishing.</div></button><button type='button' data-view-target='network'><strong>Network settings</strong><div class='muted mini'>Configure Wi-Fi, Ethernet adapter, and failover.</div></button><button type='button' data-view-target='backup'><strong>Back up settings</strong><div class='muted mini'>Download a restore point before larger changes.</div></button></div>");
    fprintf(f, "</section>");
}

static void mqtt_form(FILE *f, const struct mqtt_config *cfg) {
    int mqtt_up = mqtt_is_connected() || tcp_established(cfg->host, cfg->port);
    fprintf(f, "<div class='panel'><div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;'><h3>Broker connection</h3><span id='mqttConnBadge' class='badge %s'>%s</span></div><div class='help'>Connect the hub to your MQTT broker. Leave the password blank when you only want to change topics or timing.</div><form id='mqttForm' method='post' action='/mqtt#mqtt'>",
        mqtt_up ? "ok" : "bad", mqtt_up ? "connected" : "not connected");
    fprintf(f, "<label><input type='checkbox' name='enabled' value='1' %s> Enabled</label>", cfg->enabled ? "checked" : "");
    fprintf(f, "<div class='row'><div><label>MQTT broker address</label><input name='host' value='"); html(f, cfg->host); fprintf(f, "'><div class='help'>IP address or DNS name of the MQTT broker.</div></div>");
    fprintf(f, "<div><label>MQTT broker port</label><input name='port' inputmode='numeric' value='%d'><div class='help'>Usually 1883 unless your broker is configured differently.</div></div></div>", cfg->port);
    fprintf(f, "<div class='row'><div><label>Username</label><input name='username' autocomplete='username' value='"); html(f, cfg->username); fprintf(f, "'></div>");
    fprintf(f, "<div><label>Password</label><input name='password' type='password' autocomplete='current-password' placeholder='Leave blank to keep current'><label><input type='checkbox' name='keep_password' value='1' checked> Keep current password when blank</label></div></div>");
    fprintf(f, "<div class='row'><div><label>MQTT base topic</label><input name='baseTopic' value='"); html(f, cfg->base_topic); fprintf(f, "'></div>");
    fprintf(f, "<div><label>Home Assistant discovery prefix</label><input name='discoveryPrefix' value='"); html(f, cfg->discovery_prefix); fprintf(f, "'><div class='help'>Home Assistant commonly uses homeassistant.</div></div></div>");
    fprintf(f, "<div class='row'><div><label>MQTT client ID</label><input name='clientId' value='"); html(f, cfg->client_id); fprintf(f, "'></div>");
    fprintf(f, "<div><label>Display name</label><input name='name' value='"); html(f, cfg->name); fprintf(f, "'></div></div>");
    fprintf(f, "<div class='row'><div><label>State update interval (seconds)</label><input name='pollSeconds' inputmode='numeric' value='%d'></div>", cfg->poll_seconds);
    fprintf(f, "<div><label>MQTT keepalive (seconds)</label><input name='keepAlive' inputmode='numeric' value='%d'></div></div>", cfg->keep_alive);
    fprintf(f, "<label><input type='checkbox' name='haDiscovery' value='1' %s> Create Home Assistant entities automatically</label>", cfg->ha_discovery ? "checked" : "");
    fprintf(f, "<label><input type='checkbox' name='btRemoteEvents' value='1' %s> Publish physical Bluetooth remote buttons to MQTT ({baseTopic}/bt_remote/button)</label>", cfg->bt_remote_events ? "checked" : "");
    fprintf(f, "<div class='actions'><button type='submit'>Save MQTT settings</button><span id='mqttSaveStatus' class='save-status subtle' style='margin-left:8px;align-self:center;'></span></div></form></div>");
}

static void mqtt_ha_integration_panel(FILE *f, const struct mqtt_config *cfg) {
    const char *base = (cfg && cfg->base_topic[0]) ? cfg->base_topic : "harmony/hub";
    const char *hname = (cfg && cfg->name[0]) ? cfg->name : "Harmony Hub";
    char safe_ent[64];
    size_t si = 0;
    for (size_t i = 0; hname[i] && si < sizeof(safe_ent) - 1; i++) {
        char c = hname[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) safe_ent[si++] = c;
        else if (c >= 'A' && c <= 'Z') safe_ent[si++] = c + 32;
        else if (c == ' ' || c == '-' || c == '_') {
            if (si > 0 && safe_ent[si-1] != '_') safe_ent[si++] = '_';
        }
    }
    safe_ent[si] = '\0';
    if (!safe_ent[0]) strcpy(safe_ent, "harmony_hub");

    fprintf(f, "<div class='panel'><h3>Home Assistant Integration</h3>"
               "<div class='help'>Send arbitrary commands, change channels, and sync activities with the same parameters as the official Home Assistant Harmony integration (aioharmony).</div>"
               "<div style='margin-top:10px;'><strong>Supported Topics</strong></div>"
               "<pre style='margin-top:6px;font-size:12px;background:#111;color:#eee;padding:8px;border-radius:6px;overflow-x:auto;'>"
               "Send Command:    %s/send_command (or %s/command)\n"
               "Change Channel:  %s/channel/set\n"
               "Hub Sync:        %s/sync\n"
               "Set Activity:    %s/activity/set\n"
               "Full Catalog:    %s/config (retained)\n"
               "Hub State:       %s/state (retained)</pre>",
               base, base, base, base, base, base, base);
    (void)safe_ent;
    fprintf(f, "<div style='margin-top:10px;'><strong>Home Assistant Send Command Script (scripts.yaml)</strong></div>"
               "<pre style='margin-top:6px;font-size:12px;background:#111;color:#eee;padding:8px;border-radius:6px;overflow-x:auto;'>"
               "harmony_send_command:\n"
               "  alias: \"%s Send Command\"\n"
               "  description: \"Send IR/BT command to device via Harmony Hub MQTT\"\n"
               "  mode: queued\n"
               "  max: 20\n"
               "  fields:\n"
               "    device:\n"
               "      description: \"Target device name or ID\"\n"
               "      example: \"Tv Samsung\"\n"
               "      required: true\n"
               "      selector:\n"
               "        text: {}\n"
               "    command:\n"
               "      description: \"Command or list of commands\"\n"
               "      example: \"InputHdmi2\"\n"
               "      required: true\n"
               "    delay_secs:\n"
               "      description: \"Delay between commands in seconds\"\n"
               "      example: 0.4\n"
               "      default: 0.4\n"
               "      selector:\n"
               "        number:\n"
               "          min: 0\n"
               "          max: 10\n"
               "          step: 0.1\n"
               "          unit_of_measurement: \"s\"\n"
               "    hold_secs:\n"
               "      description: \"Hold duration in seconds\"\n"
               "      example: 0\n"
               "      default: 0\n"
               "      selector:\n"
               "        number:\n"
               "          min: 0\n"
               "          max: 5\n"
               "          step: 0.1\n"
               "          unit_of_measurement: \"s\"\n"
               "    num_repeats:\n"
               "      description: \"Times to repeat command\"\n"
               "      example: 1\n"
               "      default: 1\n"
               "      selector:\n"
               "        number:\n"
               "          min: 1\n"
               "          max: 20\n"
               "          step: 1\n"
               "  sequence:\n"
               "    - action: mqtt.publish\n"
               "      data:\n"
               "        topic: \"%s/send_command\"\n"
               "        payload: >-\n"
               "          {{ {\n"
               "            'device': device,\n"
               "            'command': [command] if command is string else command,\n"
               "            'delay_secs': delay_secs | default(0.4, true) | float,\n"
               "            'hold_secs': hold_secs | default(0, true) | float,\n"
               "            'num_repeats': num_repeats | default(1, true) | int\n"
               "          } | to_json }}</pre>",
               hname, base);
    fprintf(f, "<div style='margin-top:10px;'><strong>Example Automation / Action</strong></div>"
               "<pre style='margin-top:6px;font-size:12px;background:#111;color:#eee;padding:8px;border-radius:6px;overflow-x:auto;'>"
               "- action: script.harmony_send_command\n"
               "  data:\n"
               "    device: <Device Name>\n"
               "    command:\n"
               "      - <Command Name>\n"
               "    delay_secs: 1\n"
               "    hold_secs: 0</pre></div>");
}

static void network_panel(FILE *f, const struct wifi_config *wifi, const struct ethernet_config *eth, const struct network_status *st) {
    fprintf(f, "<section id='view-network' data-view='network' class='section'><div class='section-head'><div><h2>Network</h2><div class='section-lead'>Configure Wi-Fi connection, USB Ethernet adapter, and automatic network fallback.</div></div></div>");
    fprintf(f, "<div class='grid'>");

    /* 1. Current Network Status Card */
    fprintf(f, "<div class='panel'><h3>Current network status</h3><div class='help'>Active connection and network interface details on the hub.</div>");
    fprintf(f, "<div class='kv' style='margin-top:12px;'>");
    fprintf(f, "<div><strong>Active Connection</strong></div><div><span class='badge %s'>%s</span></div>",
            strcmp(st->active_interface, "none") != 0 ? "ok" : "bad",
            st->connection_type[0] ? st->connection_type : "None");
    fprintf(f, "<div><strong>Active Interface</strong></div><div><code>%s</code></div>",
            st->active_interface[0] ? st->active_interface : "none");
    fprintf(f, "<div><strong>IP Address</strong></div><div><code>%s</code></div>",
            st->ip[0] ? st->ip : "None assigned");
    fprintf(f, "<div><strong>Subnet Mask</strong></div><div><code>%s</code></div>",
            st->netmask[0] ? st->netmask : "-");
    fprintf(f, "<div><strong>Default Gateway</strong></div><div><code>%s</code></div>",
            st->gateway[0] ? st->gateway : "-");
    fprintf(f, "<div><strong>MAC Address</strong></div><div><code>%s</code></div>",
            st->mac[0] ? st->mac : "-");
    fprintf(f, "<div><strong>USB Controller</strong></div><div>%s</div>",
            st->usb_host_mode ? "<span class='badge ok'>Host Mode (Ethernet)</span>" :
            (st->usb_serial_active ?
                (st->usb_pc_connected ? "<span class='badge ok'>Gadget Mode: USB Serial Console (PC Connected)</span>" : "<span class='badge'>Gadget Mode: USB Serial Console (No PC)</span>") :
                (st->usb_pc_connected ? "<span class='badge ok'>Gadget Mode (PC Connected)</span>" : "<span class='badge'>Gadget Mode (No PC)</span>")));
    fprintf(f, "<div><strong>Ethernet Adapter</strong></div><div>%s</div>",
            st->usb_host_mode ?
                (st->eth_present ?
                    (st->eth_carrier ? "<span class='badge ok'>Connected (Link Up)</span>" : "<span class='badge warn'>No Carrier / Cable Unplugged</span>")
                    : "<span class='badge'>Not Detected</span>")
                : "<span class='badge'>Disabled (USB Host Off)</span>");
    fprintf(f, "<div><strong>Wi-Fi Status</strong></div><div>%s</div>",
            st->wifi_connected ? "<span class='badge ok'>Connected</span>" : "<span class='badge'>Standby / Disconnected</span>");
    fprintf(f, "</div>");
    fprintf(f, "<div class='help' style='margin-top:12px;'>Ethernet takes priority when connected. If Ethernet cable is unplugged or fails, traffic automatically routes through Wi-Fi when fallback is enabled.</div>");
    fprintf(f, "</div>");

    /* 2. Ethernet & USB Host Settings Card */
    fprintf(f, "<div class='panel'><h3>Ethernet &amp; USB Gadget</h3><div class='help'>Enable USB Host mode for USB Ethernet adapters (ASIX, RTL8152, CDC-Ether, etc.). On reboot, USB Gadget mode runs for 15s first to preserve PC recovery access if connected.</div>");
    fprintf(f, "<form id='ethernetForm' method='post' action='/ethernet#network'>");
    fprintf(f, "<label><input type='checkbox' name='eth_enabled' value='1' %s id='ethEnabledCheck'> <strong>Enable Ethernet / USB Host</strong></label>", eth->enabled ? "checked" : "");
    fprintf(f, "<div class='help' style='margin-bottom:10px;'>When enabled, the hub switches USB to host mode to support USB network adapters.</div>");
    fprintf(f, "<label><input type='checkbox' name='eth_fallback_wifi' value='1' %s> <strong>Fallback to Wi-Fi if Ethernet unavailable</strong></label>", eth->fallback_wifi ? "checked" : "");
    fprintf(f, "<div class='help' style='margin-bottom:10px;'>If no Ethernet link or DHCP lease can be established, the hub automatically falls back to Wi-Fi.</div>");
    fprintf(f, "<label>IP Assignment Mode</label>");
    fprintf(f, "<select name='eth_mode' id='ethModeSelect'>");
    fprintf(f, "<option value='dhcp' %s>DHCP (Automatic - Recommended)</option>", !eth->is_static ? "selected" : "");
    fprintf(f, "<option value='static' %s>Static IP</option>", eth->is_static ? "selected" : "");
    fprintf(f, "</select>");
    fprintf(f, "<div id='ethStaticFields' class='%s' style='margin-top:8px;'>", eth->is_static ? "" : "hidden");
    fprintf(f, "<label>Static IP Address</label><input name='eth_ip' placeholder='e.g. 192.168.1.50' value='"); html(f, eth->ip); fprintf(f, "'>");
    fprintf(f, "<label>Subnet Mask</label><input name='eth_netmask' placeholder='e.g. 255.255.255.0' value='"); html(f, eth->netmask); fprintf(f, "'>");
    fprintf(f, "<label>Default Gateway</label><input name='eth_gateway' placeholder='e.g. 192.168.1.1' value='"); html(f, eth->gateway); fprintf(f, "'>");
    fprintf(f, "<label>DNS Server</label><input name='eth_dns' placeholder='e.g. 1.1.1.1' value='"); html(f, eth->dns); fprintf(f, "'>");
    fprintf(f, "</div>");
    fprintf(f, "<div class='actions'><button name='apply' value='save' type='submit'>Save Ethernet</button><button name='apply' value='reconfigure' type='submit' class='secondary'>Save and apply</button><button name='apply' value='reboot' type='submit' class='secondary'>Save and reboot</button><span id='ethSaveStatus' class='save-status subtle' style='margin-left:8px;align-self:center;'></span></div>");
    fprintf(f, "</form></div>");

    /* 3. Wi-Fi Settings Card */
    fprintf(f, "<div class='panel'><h3>Wi-Fi connection</h3><div class='help'>Wi-Fi network credentials. Serves as wireless connection or automatic fallback when Ethernet is not connected.</div>");
    fprintf(f, "<form id='wifiForm' method='post' action='/wifi#network'>");
    fprintf(f, "<label>Wi-Fi network name (SSID)</label><input name='ssid' autocomplete='off' required value='"); html(f, wifi->ssid); fprintf(f, "'><div class='help'>The hub will join this network.</div>");
    fprintf(f, "<label>Password</label><input name='password' type='password' autocomplete='current-password' placeholder='Leave blank to keep current'>");
    fprintf(f, "<label><input type='checkbox' name='keep_password' value='1' checked> Keep current password when blank</label>");
    fprintf(f, "<label><input type='checkbox' name='hidden' value='1' %s> Hidden network</label>", wifi->hidden ? "checked" : "");
    fprintf(f, "<label><input type='checkbox' name='open' value='1' %s> No password / open network</label>", wifi->open ? "checked" : "");
    fprintf(f, "<div class='actions'><button name='apply' value='save' type='submit'>Save Wi-Fi</button><button name='apply' value='reconfigure' type='submit' class='secondary'>Save and apply</button><button name='apply' value='reboot' type='submit' class='secondary'>Save and reboot</button><span id='wifiSaveStatus' class='save-status subtle' style='margin-left:8px;align-self:center;'></span></div>");
    fprintf(f, "</form></div>");

    fprintf(f, "</div></section>");
}

static void backup_panel(FILE *f) {
    fprintf(f, "<section id='view-backup' data-view='backup' class='section'><div class='section-head'><div><h2>Backup and restore</h2><div class='section-lead'>Download a restore point before large edits, database imports, or network changes. Imports create an automatic timestamped backup first.</div></div></div>");
    fprintf(f, "<div class='grid'><div class='panel'><h3>Download backup</h3><div class='help'>Use Full backup for normal restore points. Use the smaller downloads only when you want one part of the configuration.</div><div class='export-list'>");
    fprintf(f, "<a class='button' href='/export/bundle'>Full backup</a>");
    fprintf(f, "<a class='button' href='/export/activities'>Activities</a>");
    fprintf(f, "<a class='button' href='/export/devices'>Devices</a>");
    fprintf(f, "<a class='button' href='/export/functions'>Functions</a>");
    fprintf(f, "<a class='button' href='/export/protocols'>Protocols</a>");
    fprintf(f, "<a class='button' href='/export/mqtt'>MQTT</a>");
    fprintf(f, "<a class='button' href='/export/wifi'>Wi-Fi</a>");
    fprintf(f, "<a class='button' href='/export/network'>Ethernet</a>");
    fprintf(f, "<a class='button' href='/export/bluetooth'>Bluetooth devices</a>");
    fprintf(f, "<a class='button' href='/export/remote-mapping'>BT remote mapping</a>");
    fprintf(f, "<a class='button' href='/export/auth'>WebUI auth</a>");
    fprintf(f, "<a class='button' href='/export/hub-id'>Hub ID</a>");
    fprintf(f, "</div></div>");
    fprintf(f, "<div class='panel'><h3>Restore from backup</h3><div class='help'>Choose what the pasted backup contains. Wi-Fi and Ethernet restores are saved immediately but do not take effect until reboot.</div><form id='backupImportForm' method='post' action='/import#backup'>");
    fprintf(f, "<label for='backupTarget'>Backup type</label><select id='backupTarget' name='target'>");
    fprintf(f, "<option value='bundle'>Full backup bundle</option>");
    fprintf(f, "<option value='activities'>ActivityList.json</option>");
    fprintf(f, "<option value='devices'>DeviceList.json</option>");
    fprintf(f, "<option value='functions'>FunctionList.json</option>");
    fprintf(f, "<option value='protocols'>ProtocolList.json</option>");
    fprintf(f, "<option value='mqtt'>MQTT config</option>");
    fprintf(f, "<option value='wifi'>Wi-Fi config</option>");
    fprintf(f, "<option value='ethernet'>Ethernet config</option>");
    fprintf(f, "<option value='bluetooth'>Bluetooth devices</option>");
    fprintf(f, "<option value='remote-mapping'>bt_remote_map.json</option>");
    fprintf(f, "<option value='auth'>webui_auth.conf</option>");
    fprintf(f, "<option value='hub_id'>hub_id</option>");
    fprintf(f, "</select>");
    fprintf(f, "<label for='importFile'>Backup file</label><input id='importFile' type='file'>");
    fprintf(f, "<label for='backupPayload'>Backup contents</label><textarea id='backupPayload' class='textarea-tall' name='payload' spellcheck='false' required></textarea>");
    fprintf(f, "<div class='actions'><button type='submit'>Restore backup</button><span id='backupImportStatus' class='save-status subtle' style='margin-left:8px;align-self:center;'></span></div>");
    fprintf(f, "</form></div></div></section>");
}

static void ir_device_options(FILE *f, const struct ir_inventory *inv, const char *selected) {
    int i;
    for (i = 0; i < inv->device_count; i++) {
        const struct ir_device *dev = &inv->devices[i];
        fprintf(f, "<option value='");
        html(f, dev->id);
        fprintf(f, "'%s>", selected && strcmp(selected, dev->id) == 0 ? " selected" : "");
        html(f, dev->name[0] ? dev->name : dev->id);
        fprintf(f, " (");
        html(f, dev->id);
        fprintf(f, ")</option>");
    }
}

static void ir_command_editor(FILE *f, const char *device_id, const struct ir_command *cmd, int edit) {
    char protocol[24];
    const char *mode = "keycode";
    const char *name = "";
    const char *keycode = "";
    const char *raw = "";
    snprintf(protocol, sizeof(protocol), "%d", cmd ? (cmd->protocol_id > 0 ? cmd->protocol_id : 2) : 2);
    if (cmd) {
        name = cmd->name;
        keycode = cmd->keycode;
        raw = cmd->raw;
        mode = cmd->has_raw ? "raw" : "keycode";
    }
    fprintf(f, "<details class='command-editor'><summary>%s</summary><form class='ir-command-form' method='post' action='%s#ir'>",
        edit ? "Edit command" : "Add command manually",
        edit ? "/ir/update-command" : "/ir/command");
    fprintf(f, "<input type='hidden' name='deviceId' value='");
    html(f, device_id);
    fprintf(f, "'>");
    if (edit) {
        fprintf(f, "<input type='hidden' name='oldName' value='");
        html(f, name);
        fprintf(f, "'>");
    }
    fprintf(f, "<div class='row'><div><label>Command name</label><input name='name' value='");
    html(f, name);
    fprintf(f, "' required placeholder='Power, Input HDMI 1, Volume Up'></div><div><label>Save format</label><select name='mode'>");
    fprintf(f, "<option value='keycode'%s>Harmony compact code</option>", strcmp(mode, "keycode") == 0 ? " selected" : "");
    fprintf(f, "<option value='nec'>NEC 32-bit code</option>");
    fprintf(f, "<option value='raw'%s>Raw timing replay</option>", strcmp(mode, "raw") == 0 ? " selected" : "");
    fprintf(f, "</select></div></div>");
    fprintf(f, "<div class='row'><div><label>Protocol id</label><input name='protocol' value='");
    html(f, protocol);
    fprintf(f, "' placeholder='2'></div><div><label>NEC hex code</label><input name='nec' placeholder='E0E040BF'></div></div>");
    fprintf(f, "<label>Harmony compact code</label><input name='keycode' value='");
    html(f, keycode);
    fprintf(f, "' placeholder='G:Toshiba 32 Bit:(0xE0E040BF)(Repeat)():3'>");
    fprintf(f, "<label>Raw timing data</label><textarea name='raw' class='textarea-tall' spellcheck='false' placeholder='F38000P...'>");
    html(f, raw);
    fprintf(f, "</textarea><div class='actions'><button type='submit'>%s</button><span class='save-status subtle' style='margin-left:8px;align-self:center;'></span></div></form></details>",
        edit ? "Save command changes" : "Add command");
}

struct ir_remote_button {
    const char *label;
    const char *aliases;
    double x, y, w, h;
};

static const struct ir_remote_button IR_REMOTE_BUTTONS[] = {
    {"Power off", "poweroff|power off|standby|shutdown|off|powertoggle|power toggle|power", 13.55, 0.45, 18.60, 3.35},
    {"Music", "music|audio", 8.80, 6.85, 26.75, 5.10},
    {"TV", "tv|television|watchtv|display", 35.50, 6.85, 27.10, 5.10},
    {"Movie", "movie|video|media", 62.60, 6.85, 27.10, 5.10},
    {"Rewind", "rewind|rev|skipback|previous track", 8.80, 14.90, 27.95, 5.25},
    {"Play", "play", 41.45, 15.00, 16.25, 5.40},
    {"Forward", "fastforward|forward|ffwd|next track|skipforward", 62.45, 14.85, 27.95, 5.30},
    {"Record", "record|rec", 8.95, 21.80, 27.90, 5.35},
    {"Pause", "pause", 41.45, 21.80, 16.25, 5.35},
    {"Stop", "stop", 62.45, 21.80, 27.95, 5.35},
    {"Red", "red", 9.30, 30.00, 18.60, 3.75},
    {"Green", "green", 30.10, 30.00, 18.95, 3.75},
    {"Yellow", "yellow", 51.80, 30.00, 18.15, 3.75},
    {"Blue", "blue", 73.25, 30.00, 17.45, 3.75},
    {"DVR", "dvr|recordings", 9.00, 36.95, 27.40, 5.05},
    {"Guide", "guide|epg", 36.40, 36.95, 26.40, 5.05},
    {"Info", "info|information|displayinfo|settings|setting|setup|option|options", 62.80, 36.95, 26.75, 5.05},
    {"Exit", "exit|clear|cancel", 9.10, 45.45, 36.05, 4.95},
    {"Menu", "menu|home", 53.95, 45.45, 35.70, 4.95},
    {"Up", "up|directionup|arrowup|cursorup", 36.40, 49.85, 27.60, 6.30},
    {"Left", "left|directionleft|arrowleft|cursorleft", 28.80, 54.70, 14.90, 12.10},
    {"Right", "right|directionright|arrowright|cursorright", 56.30, 54.70, 14.90, 12.10},
    {"Down", "down|directiondown|arrowdown|cursordown", 36.40, 64.55, 27.60, 6.20},
    {"OK", "ok|select|enter", 40.10, 56.10, 19.80, 9.10},
    {"Volume up", "volumeup|volup|vol up|vol_up|volume up", 9.30, 52.20, 19.30, 8.85},
    {"Volume down", "volumedown|voldown|voldn|vol down|vol_down|vol_dn|volume down", 9.30, 61.05, 19.30, 9.05},
    {"Channel up", "channelup|chup|chnext|ch_next|ch up|channel next|channelnext|pageup|pgup", 70.20, 52.20, 19.15, 8.85},
    {"Channel down", "channeldown|chdown|chdn|chprev|ch_prev|ch down|ch_dn|channel prev|channel previous|channel down|channelprev|channeldn|pagedown|pgdown", 70.20, 61.05, 19.15, 9.05},
    {"Mute", "mute", 9.30, 71.70, 36.20, 5.30},
    {"Back", "back|return|previous", 54.15, 71.70, 35.55, 5.30},
    {"1", "1|digit1|number1|num1", 9.30, 80.45, 27.10, 3.45},
    {"2", "2|digit2|number2|num2", 36.40, 80.45, 26.25, 3.45},
    {"3", "3|digit3|number3|num3", 62.60, 80.45, 27.05, 3.45},
    {"4", "4|digit4|number4|num4", 9.30, 85.55, 27.10, 3.40},
    {"5", "5|digit5|number5|num5", 36.40, 85.55, 26.25, 3.40},
    {"6", "6|digit6|number6|num6", 62.60, 85.55, 27.05, 3.40},
    {"7", "7|digit7|number7|num7", 9.30, 90.60, 27.10, 3.35},
    {"8", "8|digit8|number8|num8", 36.40, 90.60, 26.25, 3.35},
    {"9", "9|digit9|number9|num9", 62.60, 90.60, 27.05, 3.35},
    {"Dash", "dash|hyphen|separator|dot|period|minus", 9.30, 95.70, 27.10, 3.35},
    {"0", "0|digit0|number0|num0", 36.40, 95.70, 26.25, 3.35},
    {"Enter", "enter|e", 62.60, 95.70, 27.05, 3.35}
};

static void ir_command_key(const char *s, char *out, size_t outlen) {
    size_t w = 0;
    if (!outlen) return;
    while (s && *s && w + 1 < outlen) {
        unsigned char c = (unsigned char)*s++;
        if (isalnum(c)) out[w++] = (char)tolower(c);
    }
    out[w] = 0;
}

static int ir_alias_match(const char *cmd_key, const char *aliases) {
    char alias[96], alias_key[96];
    const char *p = aliases, *start;
    size_t n;
    if (!cmd_key[0]) return 0;
    while (p && *p) {
        while (*p == '|') p++;
        start = p;
        while (*p && *p != '|') p++;
        n = (size_t)(p - start);
        if (n >= sizeof(alias)) n = sizeof(alias) - 1;
        memcpy(alias, start, n);
        alias[n] = 0;
        ir_command_key(alias, alias_key, sizeof(alias_key));
        if (alias_key[0] && strcmp(cmd_key, alias_key) == 0) return 1;
        if (strlen(alias_key) > 4 && strstr(cmd_key, alias_key) != NULL) return 1;
    }
    return 0;
}

static const struct ir_command *ir_find_remote_command(const struct ir_device *dev, const char *aliases) {
    int i;
    char key[160];
    for (i = 0; i < dev->command_count; i++) {
        ir_command_key(dev->commands[i].name, key, sizeof(key));
        if (ir_alias_match(key, aliases)) return &dev->commands[i];
    }
    return NULL;
}

static int ir_command_has_remote_button(const struct ir_command *cmd) {
    size_t i;
    char key[160];
    ir_command_key(cmd->name, key, sizeof(key));
    for (i = 0; i < sizeof(IR_REMOTE_BUTTONS) / sizeof(IR_REMOTE_BUTTONS[0]); i++) {
        if (ir_alias_match(key, IR_REMOTE_BUTTONS[i].aliases)) return 1;
    }
    return 0;
}

static void ir_send_hidden_inputs(FILE *f, const char *device_id, const char *command) {
    fprintf(f, "<input type='hidden' name='deviceId' value='");
    html(f, device_id);
    fprintf(f, "'><input type='hidden' name='command' value='");
    html(f, command);
    fprintf(f, "'>");
}

static void ir_quick_send_button(FILE *f, const struct ir_device *dev, const struct ir_command *cmd) {
    fprintf(f, "<form class='ir-send-form' method='post' action='/ir/send#ir'>");
    ir_send_hidden_inputs(f, dev->id, cmd->name);
    fprintf(f, "<button type='submit'>");
    html(f, cmd->name);
    fprintf(f, "</button></form>");
}

static void ir_vslot_btn(FILE *f, const char *dev_id, const char *slot, const char *label, const char *extra_cls, const struct ir_device *dev, const char *aliases) {
    const struct ir_command *cmd = aliases ? ir_find_remote_command(dev, aliases) : NULL;
    const char *cmd_name = cmd ? cmd->name : "";
    int unmapped = !cmd;
    fprintf(f, "<button type='button' class='vrem-btn%s%s%s' data-vslot='%s' data-device-id='",
        extra_cls ? " " : "", extra_cls ? extra_cls : "", unmapped ? " unmapped" : "", slot);
    html(f, dev_id);
    fprintf(f, "' data-slot-label='");
    html(f, label);
    fprintf(f, "' data-cmd-name='");
    html(f, cmd_name);
    fprintf(f, "' title='");
    html(f, label);
    if (!unmapped) {
        fprintf(f, ": ");
        html(f, cmd_name);
    } else {
        fprintf(f, " (Unmapped)");
    }
    fprintf(f, "'>");
    html(f, label);
    fprintf(f, "</button>");
}

static void ir_render_vector_remote(FILE *f, const struct ir_device *dev) {
    fprintf(f, "<div class='vremote-body' data-vremote-shell='");
    html(f, dev->id);
    fprintf(f, "'>");

    /* Row 1: Power & Input */
    fprintf(f, "<div class='vrem-row'>");
    ir_vslot_btn(f, dev->id, "power", "Power", "vrem-btn-power", dev, "poweroff|power off|standby|shutdown|off|powertoggle|power toggle|power");
    ir_vslot_btn(f, dev->id, "input", "Input", NULL, dev, "input|source|inputnext|inputsource");
    fprintf(f, "</div>");

    /* Row 2: Navigation utility */
    fprintf(f, "<div class='vrem-row'>");
    ir_vslot_btn(f, dev->id, "back", "Back", NULL, dev, "back|return|exit|escape|previous");
    ir_vslot_btn(f, dev->id, "home", "Home", NULL, dev, "home|menu|topmenu");
    ir_vslot_btn(f, dev->id, "menu", "Menu", NULL, dev, "menu|settings|setup|options|option");
    ir_vslot_btn(f, dev->id, "info", "Info", NULL, dev, "info|information|display|guide|epg");
    fprintf(f, "</div>");

    /* Row 3: D-Pad */
    fprintf(f, "<div class='vrem-dpad'>");
    ir_vslot_btn(f, dev->id, "up", "\xe2\x96\xb2", "vrem-dpad-up", dev, "up|directionup|arrowup|cursorup");
    ir_vslot_btn(f, dev->id, "left", "\xe2\x97\x80", "vrem-dpad-left", dev, "left|directionleft|arrowleft|cursorleft");
    ir_vslot_btn(f, dev->id, "select", "OK", "vrem-dpad-ok", dev, "ok|select|enter");
    ir_vslot_btn(f, dev->id, "right", "\xe2\x96\xb6", "vrem-dpad-right", dev, "right|directionright|arrowright|cursorright");
    ir_vslot_btn(f, dev->id, "down", "\xe2\x96\xbc", "vrem-dpad-down", dev, "down|directiondown|arrowdown|cursordown");
    fprintf(f, "</div>");

    /* Row 4: Volume & Channel Rockers */
    fprintf(f, "<div class='vrem-row' style='align-items:stretch;'>");
    fprintf(f, "<div class='vrem-rocker'>");
    ir_vslot_btn(f, dev->id, "vol_up", "+", "vrem-rocker-top", dev, "volumeup|volup|vol up|vol_up|volume up");
    fprintf(f, "<div class='vrem-rocker-lbl'>VOL</div>");
    ir_vslot_btn(f, dev->id, "vol_down", "\xe2\x88\x92", "vrem-rocker-bot", dev, "volumedown|voldown|voldn|vol down|vol_down|vol_dn|volume down");
    fprintf(f, "</div>");

    fprintf(f, "<div style='display:flex;flex-direction:column;justify-content:center;gap:6px;'>");
    ir_vslot_btn(f, dev->id, "mute", "Mute", NULL, dev, "mute|volumemute");
    fprintf(f, "</div>");

    fprintf(f, "<div class='vrem-rocker'>");
    ir_vslot_btn(f, dev->id, "ch_up", "+", "vrem-rocker-top", dev, "channelup|chup|chnext|ch_next|ch up|pageup|pgup");
    fprintf(f, "<div class='vrem-rocker-lbl'>CH</div>");
    ir_vslot_btn(f, dev->id, "ch_down", "\xe2\x88\x92", "vrem-rocker-bot", dev, "channeldown|chdown|chdn|chprev|ch_prev|ch down|pagedown|pgdown");
    fprintf(f, "</div>");
    fprintf(f, "</div>");

    /* Row 5: Playback Controls */
    fprintf(f, "<div class='vrem-row'>");
    ir_vslot_btn(f, dev->id, "rewind", "\xc2\xab", NULL, dev, "rewind|rev|skipback");
    ir_vslot_btn(f, dev->id, "play", "\xe2\x96\xb6", NULL, dev, "play");
    ir_vslot_btn(f, dev->id, "pause", "\xe2\x8f\xb8", NULL, dev, "pause");
    ir_vslot_btn(f, dev->id, "stop", "\xe2\x96\xaa", NULL, dev, "stop");
    ir_vslot_btn(f, dev->id, "forward", "\xc2\xbb", NULL, dev, "fastforward|forward|ffwd|skipforward");
    fprintf(f, "</div>");

    /* Row 6: Colored buttons */
    fprintf(f, "<div class='vrem-colors'>");
    ir_vslot_btn(f, dev->id, "red", "Red", "vrem-color-btn vrem-color-red", dev, "red|colorred");
    ir_vslot_btn(f, dev->id, "green", "Green", "vrem-color-btn vrem-color-green", dev, "green|colorgreen");
    ir_vslot_btn(f, dev->id, "yellow", "Yellow", "vrem-color-btn vrem-color-yellow", dev, "yellow|coloryellow");
    ir_vslot_btn(f, dev->id, "blue", "Blue", "vrem-color-btn vrem-color-blue", dev, "blue|colorblue");
    fprintf(f, "</div>");

    fprintf(f, "</div>");
}

static void ir_render_vector_numpad(FILE *f, const struct ir_device *dev) {
    fprintf(f, "<div class='vremote-body' data-vremote-shell='");
    html(f, dev->id);
    fprintf(f, "' style='max-width:280px;'><div class='vrem-numpad'>");
    ir_vslot_btn(f, dev->id, "1", "1", NULL, dev, "1|digit1|number1|num1");
    ir_vslot_btn(f, dev->id, "2", "2", NULL, dev, "2|digit2|number2|num2");
    ir_vslot_btn(f, dev->id, "3", "3", NULL, dev, "3|digit3|number3|num3");
    ir_vslot_btn(f, dev->id, "4", "4", NULL, dev, "4|digit4|number4|num4");
    ir_vslot_btn(f, dev->id, "5", "5", NULL, dev, "5|digit5|number5|num5");
    ir_vslot_btn(f, dev->id, "6", "6", NULL, dev, "6|digit6|number6|num6");
    ir_vslot_btn(f, dev->id, "7", "7", NULL, dev, "7|digit7|number7|num7");
    ir_vslot_btn(f, dev->id, "8", "8", NULL, dev, "8|digit8|number8|num8");
    ir_vslot_btn(f, dev->id, "9", "9", NULL, dev, "9|digit9|number9|num9");
    ir_vslot_btn(f, dev->id, "dash", "\xe2\x88\x92", NULL, dev, "dash|hyphen|dot|period|minus");
    ir_vslot_btn(f, dev->id, "0", "0", NULL, dev, "0|digit0|number0|num0");
    ir_vslot_btn(f, dev->id, "enter", "\xe2\x86\xb5", NULL, dev, "enter|e|select|ok");
    fprintf(f, "</div></div>");
}

static void ir_render_device_workspace(FILE *f, const struct ir_device *dev, int active) {
    int j;
    fprintf(f, "<div class='ir-device-workspace%s' data-ir-device='", active ? " active" : "");
    html(f, dev->id);
    fprintf(f, "'><div class='ir-work-head'><div><h3>");
    html(f, dev->name[0] ? dev->name : "Unnamed device");
    fprintf(f, "</h3><div class='muted mini'>");
    html(f, dev->manufacturer);
    fprintf(f, " ");
    html(f, dev->model);
    fprintf(f, " / ");
    html(f, dev->type);
    fprintf(f, " / %d saved commands</div></div><div class='actions'><button type='button' class='secondary' data-next-step='library'>Add from database</button><button type='button' class='ghost' data-next-step='learn'>Learn button</button></div></div>", dev->command_count);
    fprintf(f, "<div class='ir-status muted mini' data-ir-status='");
    html(f, dev->id);
    fprintf(f, "'>Ready.</div>");

    /* Sub-tabs header */
    fprintf(f, "<div class='vremote-tabs'>");
    fprintf(f, "<div class='vremote-tab-group'>");
    fprintf(f, "<button type='button' class='vtab-btn active' data-vtab-target='tactile'>Tactile Remote</button>");
    fprintf(f, "<button type='button' class='vtab-btn' data-vtab-target='all'>All Commands (%d)</button>", dev->command_count);
    fprintf(f, "<button type='button' class='vtab-btn' data-vtab-target='numpad'>Number Pad</button>");
    fprintf(f, "</div>");
    fprintf(f, "<div class='actions' style='margin:0;'>");
    fprintf(f, "<button type='button' class='secondary vtab-edit-toggle' data-device-edit='");
    html(f, dev->id);
    fprintf(f, "'>Edit Layout</button>");
    fprintf(f, "<button type='button' class='ghost vtab-save-hub' data-device-save='");
    html(f, dev->id);
    fprintf(f, "'>Save to Hub</button>");
    fprintf(f, "</div></div>");

    /* Panel 1: Tactile Vector Remote */
    fprintf(f, "<div class='vtab-panel vtab-panel-tactile active'>");
    fprintf(f, "<div class='vremote-stage' data-remote-stage='");
    html(f, dev->id);
    fprintf(f, "'>");
    ir_render_vector_remote(f, dev);
    fprintf(f, "<div class='vslot-inspector hidden' data-inspector-device='");
    html(f, dev->id);
    fprintf(f, "'><div class='panel'>");
    fprintf(f, "<div style='display:flex;justify-content:space-between;align-items:center;gap:8px;'>");
    fprintf(f, "<h3 style='margin:0;'>Button: <span class='vslot-target-name' style='color:var(--accent);'>Select a button</span></h3>");
    fprintf(f, "<span class='vslot-target-status badge'>unmapped</span></div>");
    fprintf(f, "<div class='help'>Click any button on the remote to inspect or assign its IR/BT command.</div>");
    fprintf(f, "<div style='margin-top:10px;'><label>Search Commands</label><input type='text' class='vslot-search' placeholder='Filter commands (e.g. power, hdmi, vol)...'></div>");
    fprintf(f, "<div style='margin-top:8px;'><label>Assigned Command</label><select class='vslot-cmd-select'><option value=''>-- Choose Command --</option></select></div>");
    fprintf(f, "<div class='actions' style='margin-top:10px;'><button type='button' class='vslot-assign-btn'>Assign</button><button type='button' class='vslot-test-btn secondary'>Test Send</button><button type='button' class='vslot-clear-btn danger'>Clear</button></div>");
    fprintf(f, "<div style='margin-top:14px;border-top:1px solid var(--line);padding-top:10px;'><label>Command Palette (Click to assign)</label><div class='vslot-chips-grid preview' style='max-height:140px;display:flex;flex-wrap:wrap;gap:6px;margin-top:4px;'></div></div>");
    fprintf(f, "<div style='margin-top:12px;border-top:1px solid var(--line);padding-top:10px;display:flex;gap:8px;flex-wrap:wrap;'><button type='button' class='vslot-automap-btn secondary mini' data-device-id='");
    html(f, dev->id);
    fprintf(f, "'>Auto-Map Standard</button><button type='button' class='vslot-reset-btn ghost mini' data-device-id='");
    html(f, dev->id);
    fprintf(f, "'>Reset Defaults</button></div>");
    fprintf(f, "</div></div></div></div>");

    /* Panel 2: All Commands */
    fprintf(f, "<div class='vtab-panel vtab-panel-all hidden'>");
    fprintf(f, "<div class='ir-command-tools'><div><label>Find saved command</label><input class='ir-command-filter' data-command-filter='");
    html(f, dev->id);
    fprintf(f, "' placeholder='power, hdmi, volume, menu'></div>");
    fprintf(f, "<div class='ir-quick-grid' data-command-list='");
    html(f, dev->id);
    fprintf(f, "'>");
    for (j = 0; j < dev->command_count; j++) ir_quick_send_button(f, dev, &dev->commands[j]);
    if (dev->command_count == 0) fprintf(f, "<div class='muted mini'>No commands saved yet. Search a database or learn buttons above.</div>");
    fprintf(f, "</div></div></div>");

    /* Panel 3: Number Pad */
    fprintf(f, "<div class='vtab-panel vtab-panel-numpad hidden'>");
    ir_render_vector_numpad(f, dev);
    fprintf(f, "</div>");

    fprintf(f, "</div>");
}

static void ir_render_device_editor(FILE *f, const struct ir_device *dev, int active) {
    int j;
    fprintf(f, "<div class='ir-device-workspace%s' data-ir-device='", active ? " active" : "");
    html(f, dev->id);
    fprintf(f, "'><div class='ir-work-head'><div><h3>");
    html(f, dev->name[0] ? dev->name : "Unnamed device");
    fprintf(f, "</h3><div class='muted mini'>");
    html(f, dev->manufacturer);
    fprintf(f, " ");
    html(f, dev->model);
    fprintf(f, " / ");
    html(f, dev->type);
    fprintf(f, " / %d saved commands", dev->command_count);
    if (dev->transport != 32) {
        int cp = dev->control_port > 0 ? dev->control_port : 7;
        const char *b_desc = "All Blasters";
        if (cp == 4) b_desc = "Hub Internal Only";
        else if (cp == 1) b_desc = "Blaster 1 Only";
        else if (cp == 2) b_desc = "Blaster 2 Only";
        else if (cp == 3) b_desc = "Blaster 1+2";
        else if (cp == 5) b_desc = "Hub+Blaster 1";
        else if (cp == 6) b_desc = "Hub+Blaster 2";
        fprintf(f, " / <span class='badge mini'>%s</span>", b_desc);
    }
    fprintf(f, "</div></div><div class='actions'><button type='button' class='secondary' data-next-step='library'>Add commands from database</button><button type='button' class='ghost' data-next-step='learn'>Learn button</button><button type='button' class='ghost' data-view-target='control'>Open remote control</button></div></div>");
    fprintf(f, "<h3>Device details</h3><form method='post' action='/ir/device#ir' class='device-details-form' data-device-id='");
    html(f, dev->id);
    fprintf(f, "'><input type='hidden' name='deviceId' value='");
    html(f, dev->id);
    fprintf(f, "'><div class='row'><div><label>Name</label><input name='name' value='");
    html(f, dev->name);
    fprintf(f, "'></div><div><label>Type</label><input name='type' value='");
    html(f, dev->type);
    fprintf(f, "'></div></div><div class='row'><div><label>Manufacturer</label><input name='manufacturer' value='");
    html(f, dev->manufacturer);
    fprintf(f, "'></div><div><label>Model</label><input name='model' value='");
    html(f, dev->model);
    fprintf(f, "'></div></div>");

    if (dev->transport != 32) {
        int cp = dev->control_port > 0 ? dev->control_port : 7;
        fprintf(f, "<input type='hidden' name='has_blaster_config' value='1'>"
                   "<div style='margin-top:12px'><label>IR Blaster Assignment</label>"
                   "<div style='display:flex;gap:18px;flex-wrap:wrap;align-items:center;margin-top:6px;background:var(--soft);border:1px solid var(--line);border-radius:6px;padding:10px 12px;'>"
                   "<label style='display:inline-flex;align-items:center;gap:6px;margin:0;cursor:pointer;font-weight:550;'>"
                   "<input type='checkbox' name='blaster_internal' value='4'%s> Hub Internal</label>"
                   "<label style='display:inline-flex;align-items:center;gap:6px;margin:0;cursor:pointer;font-weight:550;'>"
                   "<input type='checkbox' name='blaster_port1' value='1'%s> Blaster Port 1</label>"
                   "<label style='display:inline-flex;align-items:center;gap:6px;margin:0;cursor:pointer;font-weight:550;'>"
                   "<input type='checkbox' name='blaster_port2' value='2'%s> Blaster Port 2</label>"
                   "</div>"
                   "<div class='subtle'>Select which physical IR blaster transmitter(s) fire when sending commands to this device (hub internal emitters, mini-jack blaster 1, blaster 2, or any combination).</div>"
                   "</div>",
                   (cp & 4) ? " checked" : "",
                   (cp & 1) ? " checked" : "",
                   (cp & 2) ? " checked" : "");
    }

    fprintf(f, "<div class='actions'><button type='submit'>Save device details</button><span class='details-save-status subtle' style='margin-left:8px;align-self:center;'></span></div></form>");
    fprintf(f, "<form class='ir-delete-device-form' method='post' action='/ir/delete-device#ir'><input type='hidden' name='deviceId' value='");
    html(f, dev->id);
    fprintf(f, "'><div class='actions'><button class='danger' type='submit'>Delete device</button><span class='save-status subtle' style='margin-left:8px;align-self:center;'></span></div></form>");

    /* Datalist of device commands */
    fprintf(f, "<datalist id='device-cmds-%s'>", dev->id);
    for (j = 0; j < dev->command_count; j++) {
        fprintf(f, "<option value='");
        html(f, dev->commands[j].name);
        fprintf(f, "'>");
    }
    fprintf(f, "</datalist>");

    /* Power Management Card */
    fprintf(f, "<div class='card' style='margin-top:16px'><h3>Power Management</h3>"
               "<div class='help'>Configure how the hub powers this device on or off during activity changes and reboot. "
               "To stop the hub from pressing buttons (e.g. Home) on boot, remove the step from Turn-On Sequence below.</div>"
               "<form method='post' action='/ir/device-power#ir' class='device-power-form' data-device-id='%s'>"
               "<input type='hidden' name='deviceId' value='%s'>"
               "<div class='row' style='margin-top:8px'><label style='display:flex;align-items:center;gap:8px;cursor:pointer;'>"
               "<input type='checkbox' name='isPowerAlwaysOn' class='device-power-always-on' value='1'%s>"
               "<strong>Always On (leave powered on between activities)</strong></label></div>"
               "<div class='row' style='margin-top:8px'><div><label>Power-On Warmup Delay (ms)</label>"
               "<input type='number' name='powerOnDelay' class='device-power-delay' min='0' max='60000' step='100' value='%d'>"
               "<div class='subtle'>Delay before sending other activity commands while device powers up.</div></div></div>",
               dev->id, dev->id,
               dev->is_power_always_on ? " checked" : "",
               dev->power_on_delay > 0 ? dev->power_on_delay : 1500);

    /* Turn-On Sequence */
    fprintf(f, "<h4 style='margin-top:16px'>Turn-On Sequence (PowerOnActions)</h4>"
               "<div class='subtle' style='margin-bottom:8px'>Commands or delays executed when starting an activity using this device.</div>"
               "<div class='power-seq-list' data-seq-type='on' data-device-id='%s'>", dev->id);
    for (j = 0; j < dev->power_on_count; j++) {
        struct activity_step *st = &dev->power_on_steps[j];
        int is_delay = (strcasecmp(st->type, "Delay") == 0);
        fprintf(f, "<div class='queue-row power-step-row' style='display:flex;align-items:center;gap:8px;margin-bottom:6px;'>"
                   "<span class='pill mini' style='min-width:65px;text-align:center;'>%s</span>",
                   is_delay ? "Delay" : "Command");
        if (is_delay) {
            fprintf(f, "<input type='number' class='power-step-delay' min='50' max='60000' step='50' value='%d' style='width:120px;'> ms",
                    st->delay_ms > 0 ? st->delay_ms : 1000);
        } else {
            fprintf(f, "<input type='text' list='device-cmds-%s' class='power-step-cmd' value='", dev->id);
            html(f, st->command);
            fprintf(f, "' placeholder='Command name (e.g. PowerOn, Home)' style='flex:1;'>");
        }
        fprintf(f, "<div class='actions' style='margin-left:auto;display:flex;gap:4px;'>"
                   "<button type='button' class='ghost mini power-step-up' title='Move Up'>&uarr;</button>"
                   "<button type='button' class='ghost mini power-step-down' title='Move Down'>&darr;</button>"
                   "<button type='button' class='danger ghost mini power-step-del' title='Delete'>&times;</button>"
                   "</div></div>");
    }
    fprintf(f, "</div>"
               "<div class='actions' style='margin-top:6px;display:flex;gap:6px;'>"
               "<button type='button' class='secondary mini power-add-cmd' data-seq-type='on'>+ Add Command</button>"
               "<button type='button' class='secondary mini power-add-delay' data-seq-type='on'>+ Add Delay</button>"
               "</div>"
               "<textarea name='powerOnSeq' style='display:none;' class='power-seq-hidden' data-seq-type='on'></textarea>");

    /* Turn-Off Sequence */
    fprintf(f, "<h4 style='margin-top:16px'>Turn-Off Sequence (PowerOffActions)</h4>"
               "<div class='subtle' style='margin-bottom:8px'>Commands or delays executed when powering off or switching away.</div>"
               "<div class='power-seq-list' data-seq-type='off' data-device-id='%s'>", dev->id);
    for (j = 0; j < dev->power_off_count; j++) {
        struct activity_step *st = &dev->power_off_steps[j];
        int is_delay = (strcasecmp(st->type, "Delay") == 0);
        fprintf(f, "<div class='queue-row power-step-row' style='display:flex;align-items:center;gap:8px;margin-bottom:6px;'>"
                   "<span class='pill mini' style='min-width:65px;text-align:center;'>%s</span>",
                   is_delay ? "Delay" : "Command");
        if (is_delay) {
            fprintf(f, "<input type='number' class='power-step-delay' min='50' max='60000' step='50' value='%d' style='width:120px;'> ms",
                    st->delay_ms > 0 ? st->delay_ms : 1000);
        } else {
            fprintf(f, "<input type='text' list='device-cmds-%s' class='power-step-cmd' value='", dev->id);
            html(f, st->command);
            fprintf(f, "' placeholder='Command name (e.g. PowerOff, Sleep)' style='flex:1;'>");
        }
        fprintf(f, "<div class='actions' style='margin-left:auto;display:flex;gap:4px;'>"
                   "<button type='button' class='ghost mini power-step-up' title='Move Up'>&uarr;</button>"
                   "<button type='button' class='ghost mini power-step-down' title='Move Down'>&darr;</button>"
                   "<button type='button' class='danger ghost mini power-step-del' title='Delete'>&times;</button>"
                   "</div></div>");
    }
    fprintf(f, "</div>"
               "<div class='actions' style='margin-top:6px;display:flex;gap:6px;'>"
               "<button type='button' class='secondary mini power-add-cmd' data-seq-type='off'>+ Add Command</button>"
               "<button type='button' class='secondary mini power-add-delay' data-seq-type='off'>+ Add Delay</button>"
               "</div>"
               "<textarea name='powerOffSeq' style='display:none;' class='power-seq-hidden' data-seq-type='off'></textarea>");

    /* Save button and status */
    fprintf(f, "<div class='actions' style='margin-top:16px;display:flex;align-items:center;gap:10px;'>"
               "<button type='submit' class='power-save-btn'>Save Power Settings</button>"
               "<span class='power-save-status mini subtle'></span>"
               "</div></form></div>");

    /* MQTT Connection Card */
    fprintf(f, "<div class='card' style='margin-top:16px'><h3>MQTT Connection</h3>"
               "<div class='help'>When enabled, pressing any button on this remote (via physical BT remote, Control view, or in an Activity sequence) publishes a momentary pulse (1 then 0) to MQTT.</div>"
               "<form method='post' action='/ir/device-mqtt#ir' class='device-mqtt-form' data-device-id='%s'>"
               "<input type='hidden' name='deviceId' value='%s'>"
               "<div class='row' style='margin-top:8px'><label style='display:flex;align-items:center;gap:8px;cursor:pointer;'>"
               "<input type='checkbox' name='mqttEnabled' class='device-mqtt-enabled' value='1'%s>"
               "<strong>Publish button presses to MQTT</strong></label></div>"
               "<div class='row' style='margin-top:8px'><div style='flex:2;'><label>MQTT Topic Template</label>"
               "<input type='text' name='mqttTopic' class='device-mqtt-topic' value='",
               dev->id, dev->id,
               dev->mqtt_enabled ? " checked" : "");
    html(f, dev->mqtt_topic[0] ? dev->mqtt_topic : "{root}/button/{device}/{command}");
    fprintf(f, "' placeholder='{root}/button/{device}/{command}'>"
               "<div class='subtle'>Default: <code>{root}/button/{device}/{command}</code>. Placeholders: <code>{root}</code>, <code>{device}</code>, <code>{command}</code>.</div></div>"
               "<div style='flex:1;'><label>Pulse Duration (ms)</label>"
               "<input type='number' name='mqttPulseMs' class='device-mqtt-pulse' min='20' max='5000' step='10' value='%d'>"
               "<div class='subtle'>Sends 1 on press, then 0 after pulse delay.</div></div></div>"
               "<div class='actions' style='margin-top:12px;display:flex;align-items:center;gap:10px;'>"
               "<button type='submit' class='mqtt-save-btn'>Save MQTT Settings</button>"
               "<button type='button' class='secondary mqtt-test-btn' data-device-id='%s'>Test MQTT Pulse</button>"
               "<span class='mqtt-save-status mini subtle'></span>"
               "</div></form></div>",
               dev->mqtt_pulse_ms > 0 ? dev->mqtt_pulse_ms : 1000,
               dev->id);
    fprintf(f, "<h3 style='margin-top:16px'>Commands</h3><div class='help'>Add, edit, or delete saved commands.</div>");
    ir_command_editor(f, dev->id, NULL, 0);
    fprintf(f, "<details open><summary>Saved commands</summary><div class='command-list mini' data-command-list='");
    html(f, dev->id);
    fprintf(f, "'>");
    for (j = 0; j < dev->command_count; j++) {
        const struct ir_command *cmd = &dev->commands[j];
        fprintf(f, "<div class='ir-command-row' data-command-name='");
        html(f, cmd->name);
        fprintf(f, "'><div><strong>");
        html(f, cmd->name);
        fprintf(f, "</strong><div class='muted'>Protocol %d / learned from remote: %s</div><div class='muted'>",
            cmd->protocol_id, cmd->learned ? "yes" : "no");
        if (cmd->has_raw) fprintf(f, "raw timing recording");
        else html(f, cmd->keycode);
        fprintf(f, "</div></div><div class='actions'>");
        ir_quick_send_button(f, dev, cmd);
        fprintf(f, "<form class='ir-delete-command-form' method='post' action='/ir/delete-command#ir'><input type='hidden' name='deviceId' value='");
        html(f, dev->id);
        fprintf(f, "'><input type='hidden' name='command' value='");
        html(f, cmd->name);
        fprintf(f, "'><button class='danger' type='submit'>Delete</button><span class='save-status subtle' style='margin-left:6px;align-self:center;'></span></form></div>");
        ir_command_editor(f, dev->id, cmd, 1);
        fprintf(f, "</div>");
    }
    if (dev->command_count == 0) fprintf(f, "<div class='muted'>No commands saved yet.</div>");
    fprintf(f, "</div></details></div>");
}

static void ir_setup_flow(FILE *f, const struct ir_inventory *inv) {
    fprintf(f, "<div class='setup-shell'><div class='wizard-top'><div><h3>Add or find remote commands</h3><div class='subtle'>Save the device, find codes, learn missing buttons, then test.</div></div><span class='pill'>%d devices</span></div>", inv->device_count);
    fprintf(f, "<div class='wizard-grid'><div class='stepper'>");
    fprintf(f, "<button type='button' class='step active' data-step-target='device'><span>1</span><div>Device<div class='subtle'>What it is</div></div></button>");
    fprintf(f, "<button type='button' class='step' data-step-target='library'><span>2</span><div>Search<div class='subtle'>Find codes</div></div></button>");
    fprintf(f, "<button type='button' class='step' data-step-target='learn'><span>3</span><div>Learn<div class='subtle'>Record remote</div></div></button>");
    fprintf(f, "<button type='button' class='step' data-step-target='verify'><span>4</span><div>Test<div class='subtle'>Try buttons</div></div></button>");
    fprintf(f, "</div><div class='wizard-body'>");

    fprintf(f, "<div class='wizard-panel active' data-panel='device'><div class='callout'><strong>Start with the device profile.</strong>Enter brand and model. Search uses this.</div><form id='profileDeviceForm' method='post' action='/ir/new-device#ir'>");
    fprintf(f, "<div class='row'><div><label>Device name</label><input id='newDeviceName' name='name' required placeholder='Living Room TV'></div><div><label>Device type</label><select id='newDeviceType' name='type'><option value='HomeAppliance'>Home appliance</option><option value='Amplifier'>Receiver / amplifier</option><option value='Television'>Television</option><option value='Media Player'>Media player</option><option value='Game Console'>Game console</option><option value='MQTT'>MQTT virtual device</option></select></div></div>");
    fprintf(f, "<div class='row'><div><label>Manufacturer</label><input id='newDeviceManufacturer' name='manufacturer' required placeholder='Pioneer'></div><div><label>Model</label><input id='newDeviceModel' name='model' required placeholder='VSX-D1'></div></div>");
    fprintf(f, "<div id='profileStatus' class='wizard-status mini'></div>");
    fprintf(f, "<div class='actions'><button type='submit'>Create new device</button></div></form></div>");

    fprintf(f, "<div class='wizard-panel' data-panel='learn'><div class='callout'><strong>Learn only when search does not find the button.</strong>Point the original remote at the hub, capture the button, test it here, then save it when it works.</div><form id='wizardLearnForm' method='post' action='/ir/command#ir'>");
    fprintf(f, "<label>Device</label><select id='wizardDevice' name='deviceId'>");
    ir_device_options(f, inv, NULL);
    fprintf(f, "</select><label>Command name</label><input id='wizardCommandName' name='name' required placeholder='Power Toggle'>");
    fprintf(f, "<div class='row'><div><label>Save format</label><select id='wizardMode' name='mode'><option value='auto' selected>Choose automatically</option><option value='keycode'>Harmony compact code</option><option value='nec'>NEC 32-bit code</option><option value='raw'>Raw timing recording</option></select></div>");
    fprintf(f, "<div><label>Protocol</label><select id='wizardProtocol' name='protocol'><option value='2'>NEC-compatible</option><option value='679'>MemorexO1 32 Bit</option></select></div></div>");
    fprintf(f, "<label>NEC hex code</label><input id='wizardNec' name='nec' placeholder='E0E040BF'>");
    fprintf(f, "<label>Harmony compact code</label><input id='wizardKeycode' name='keycode' placeholder='G:Toshiba 32 Bit:(0xE0E040BF)(Repeat)():3'>");
    fprintf(f, "<label>Captured signal data</label><textarea id='wizardRaw' name='raw' placeholder=''></textarea>");
    fprintf(f, "<div id='wizardLearnStatus' class='wizard-status mini'></div>");
    fprintf(f, "<div class='actions'><button id='wizardCapture' type='button'>Capture from remote</button><button id='wizardLearnTest' type='button' class='secondary'>Test captured button</button><button type='submit' class='secondary'>Save command</button><button type='button' class='ghost' data-next-step='library'>Search databases</button><button type='button' class='ghost' data-next-step='verify'>Test saved commands</button></div></form></div>");

    fprintf(f, "<div class='wizard-panel' data-panel='verify'><div class='callout'><strong>Test before adding lots of commands.</strong>Send one saved command and confirm the real device reacts the way you expect.</div>");
    fprintf(f, "<div class='device-sync'><div><label>Device</label><select id='verifyDevice'>");
    ir_device_options(f, inv, NULL);
    fprintf(f, "</select></div><button id='wizardTest' type='button'>Send test command</button></div>");
    fprintf(f, "<label>Command</label><select id='verifyCommand'></select>");
    fprintf(f, "<div id='wizardVerifyStatus' class='wizard-status mini'></div>");
    fprintf(f, "<div class='actions'><button type='button' class='secondary' data-next-step='learn'>Learn another button</button><button type='button' class='ghost' data-next-step='library'>Search more codes</button></div></div>");

    fprintf(f, "<div class='wizard-panel' data-panel='library'><div class='callout'><strong>Search code databases before learning.</strong>Try the manufacturer and model first. Choose a match, load its commands, then test one or two before saving a large set.</div><form id='irdbImportForm' method='post' action='/ir/irdb-import#ir'>");
    fprintf(f, "<label>Save commands to</label><select id='irdbDevice' name='deviceId'>");
    ir_device_options(f, inv, NULL);
    fprintf(f, "</select><label>Database source</label><select id='irdbSource'><option value='all'>All databases</option><option value='irdb'>probonopd/irdb CSV</option><option value='flipper'>Flipper-IRDB .ir</option><option value='lirc'>LIRC remotes</option><option value='smartir'>SmartIR JSON</option><option value='remotecentral'>RemoteCentral Pronto hex</option><option value='custom'>Pasted file or URL</option></select>");
    fprintf(f, "<label>Search terms</label><input id='irdbSearch' list='irdbMatches' placeholder='Pioneer VSX-D1, LG C5, Samsung TV, BeoVision'><datalist id='irdbMatches'></datalist>");
    fprintf(f, "<div id='irdbResults' class='preview hidden'></div>");
    fprintf(f, "<label>Selected file, page, or URL</label><input id='irdbPath' placeholder='Samsung/TV/7,7.csv, TVs/Samsung/Samsung_TV.ir, /cgi-bin/codes/pioneer/receiver/, or a raw URL'>");
    fprintf(f, "<label>Paste IR codes or file contents</label><textarea id='irdbPaste' placeholder='Paste Flipper .ir, LIRC, Pronto Hex, Global Cache sendir, GIRR XML, BroadLink, .irc, CSV, or JSON'></textarea><input id='irdbFile' type='file' accept='.ir,.conf,.lircd,.lirc,.txt,.xml,.girr,.irc,.csv,.json'>");
    fprintf(f, "<label>Command name prefix</label><input id='irdbPrefix' placeholder='TV '>");
    fprintf(f, "<textarea id='irdbPayload' name='payload' class='hidden'></textarea>");
    fprintf(f, "<div id='irdbStatus' class='muted mini'></div><div id='irdbPreview' class='preview hidden'></div><label>Search and import log</label><pre id='irdbLog' class='mini' style='max-height:170px'>Ready.</pre>");
    fprintf(f, "<div class='actions'><button id='irdbLoadIndex' type='button' class='secondary'>Search databases</button><button id='irdbFetch' type='button' class='secondary'>Load selected file</button><button id='irdbParsePaste' type='button' class='secondary'>Parse pasted codes</button><button type='submit'>Save selected commands</button></div></form></div>");
    fprintf(f, "</div></div></div>");
}

static void ir_control_panel(FILE *f) {
    struct ir_inventory *inv = (struct ir_inventory *)calloc(1, sizeof(*inv));
    int i;
    if (!inv) return;
    fprintf(f, "<section id='view-control' data-view='control' class='section'><div class='section-head'><div><h2>Remote control</h2><div class='section-lead'>Choose a saved device, then send its buttons.</div></div></div>");
    if (load_ir_inventory(inv) != 0) {
        fprintf(f, "<div class='panel'><pre>Unable to read ");
        html(f, DEVICE_LIST);
        fprintf(f, "</pre></div></section>");
        destroy_ir_inventory(inv);
        return;
    }
    if (inv->device_count > 0) {
        fprintf(f, "<div class='ir-stored-layout'><div class='panel'><h3>Stored remotes</h3><div class='muted mini'>Choose a saved remote.</div><div class='ir-device-picks'>");
        for (i = 0; i < inv->device_count; i++) {
            const struct ir_device *dev = &inv->devices[i];
            fprintf(f, "<button type='button' class='ir-device-pick%s' data-ir-device-pick='",
                i == 0 ? " active" : "");
            html(f, dev->id);
            fprintf(f, "'><div><strong>");
            html(f, dev->name[0] ? dev->name : "Unnamed device");
            fprintf(f, "</strong><div class='muted mini'>");
            html(f, dev->manufacturer);
            fprintf(f, " ");
            html(f, dev->model);
            fprintf(f, "</div></div><span class='badge'>%d</span></button>", dev->command_count);
        }
        fprintf(f, "</div></div><div class='panel'>");
        for (i = 0; i < inv->device_count; i++) {
            ir_render_device_workspace(f, &inv->devices[i], i == 0);
        }
        fprintf(f, "</div></div>");
    } else {
        fprintf(f, "<div class='panel'><h3>No stored remotes yet</h3><div class='muted'>Create a device in IR Setup, search a code database, or learn commands from an existing remote.</div><div class='actions'><button type='button' data-view-target='ir'>Open IR Setup</button></div></div>");
    }
    fprintf(f, "</section>");
    destroy_ir_inventory(inv);
}

static void ir_panel(FILE *f) {
    struct ir_inventory *inv = (struct ir_inventory *)calloc(1, sizeof(*inv));
    int i;
    if (!inv) return;
    fprintf(f, "<section id='view-ir' data-view='ir' class='section'><div class='section-head'><div><h2>IR setup</h2><div class='section-lead'>Create or edit devices, find codes, and learn missing buttons.</div></div></div>");
    if (load_ir_inventory(inv) != 0) {
        fprintf(f, "<div class='panel'><pre>Unable to read ");
        html(f, DEVICE_LIST);
        fprintf(f, "</pre></div></section>");
        destroy_ir_inventory(inv);
        return;
    }
    ir_setup_flow(f, inv);
    if (inv->device_count > 0) {
        fprintf(f, "<div class='ir-stored-layout'><div class='panel'><h3>Edit existing devices</h3><div class='muted mini'>Choose a saved device to edit details or commands.</div><div class='ir-device-picks'>");
        for (i = 0; i < inv->device_count; i++) {
            const struct ir_device *dev = &inv->devices[i];
            fprintf(f, "<button type='button' class='ir-device-pick%s' data-ir-device-pick='",
                i == 0 ? " active" : "");
            html(f, dev->id);
            fprintf(f, "'><div><strong>");
            html(f, dev->name[0] ? dev->name : "Unnamed device");
            fprintf(f, "</strong><div class='muted mini'>");
            html(f, dev->manufacturer);
            fprintf(f, " ");
            html(f, dev->model);
            fprintf(f, "</div></div><span class='badge'>%d</span></button>", dev->command_count);
        }
        fprintf(f, "</div></div><div class='panel'>");
        for (i = 0; i < inv->device_count; i++) {
            ir_render_device_editor(f, &inv->devices[i], i == 0);
        }
        fprintf(f, "</div></div>");
    } else {
        fprintf(f, "<div class='panel'><h3>No stored remotes yet</h3><div class='muted'>Create a device above, search a code database, or learn commands from an existing remote.</div></div>");
    }
    fprintf(f, "</section>");
    destroy_ir_inventory(inv);
}

static void ir_lab_panel(FILE *f) {
    struct ir_inventory *inv = (struct ir_inventory *)calloc(1, sizeof(*inv));
    if (!inv) return;
    fprintf(f, "<section id='view-lab' data-view='lab' class='section'><div class='section-head'><div><h2>Bulk IR test</h2><div class='section-lead'>Search code libraries, load matching commands into a queue, then save or send them at a controlled pace.</div></div><span class='pill'>up to %d saved commands per device</span></div>", MAX_IR_STORED_COMMANDS);
    if (load_ir_inventory(inv) != 0) {
        fprintf(f, "<div class='panel'><pre>Unable to read ");
        html(f, DEVICE_LIST);
        fprintf(f, "</pre></div></section>");
        destroy_ir_inventory(inv);
        return;
    }
    fprintf(f, "<div class='lab-layout'><div class='panel'><h3>Find command files</h3><div class='callout'><strong>Search first, send second.</strong>Start with a device, brand, model, or database path. Search shows matching files; Load commands reads those files and adds usable, non-duplicate commands to the queue.</div>");
    fprintf(f, "<div class='lab-toolbar'><div><label>Device to save or test on</label><select id='labDevice'><option value='__auto_lab__' selected>Temporary test device</option>");
    ir_device_options(f, inv, NULL);
    fprintf(f, "</select></div><div><label>Database source</label><select id='labSource'><option value='all'>All databases</option><option value='irdb'>probonopd/irdb</option><option value='flipper'>Flipper-IRDB</option><option value='lirc'>LIRC remotes</option><option value='smartir'>SmartIR JSON</option><option value='remotecentral'>RemoteCentral</option></select></div></div>");
    fprintf(f, "<div class='row'><div><label>Device, brand, model, or path</label><input id='labPathFilter' placeholder='air conditioner, Pioneer, Samsung/TV'></div><div><label>Button names to include</label><input id='labCommandFilter' placeholder='off, power off, standby'></div></div>");
    fprintf(f, "<div class='lab-presets'><button id='labOffFilter' type='button' class='ghost'>Power off</button><button id='labPowerFilter' type='button' class='ghost'>Any power</button><button id='labVolumeFilter' type='button' class='ghost'>Volume</button><button id='labInputFilter' type='button' class='ghost'>Inputs</button><button id='labClearFilter' type='button' class='ghost'>Clear</button></div>");
    fprintf(f, "<details class='lab-advanced'><summary>Search limits and timing</summary><label>RemoteCentral page path</label><input id='labRcPath' placeholder='Optional direct page, e.g. /cgi-bin/codes/pioneer/receiver/'>");
    fprintf(f, "<div class='lab-toolbar'><div><label>Files to scan per click</label><input id='labMaxFiles' inputmode='numeric' value='600'></div><div><label>Parallel file reads</label><input id='labWorkers' inputmode='numeric' value='14'></div><div><label>Maximum commands in queue</label><input id='labMaxCommands' inputmode='numeric' value='900'></div><div><label>Delay between file reads (ms)</label><input id='labFetchDelay' inputmode='numeric' value='0'></div></div>");
    fprintf(f, "<div class='lab-toolbar'><div><label>Delay between IR sends (ms)</label><input id='labSendDelay' inputmode='numeric' value='80'></div><div><label>Commands sent per request</label><input id='labBatchSize' inputmode='numeric' value='100'></div></div>");
    fprintf(f, "<label class='inline-check'><input id='labDryRun' type='checkbox'> Dry run (do not send IR)</label></details>");
    fprintf(f, "<div class='actions'><button id='labCandidatesBtn' type='button' class='secondary'>Find matching files</button><button id='labScan' type='button'>Load commands</button><button id='labStored' type='button' class='secondary'>Use saved commands</button><button id='labClear' type='button' class='ghost'>Clear search and queue</button></div>");
    fprintf(f, "<div id='labStatus' class='wizard-status mini'></div><div class='meter'><span id='labMeter'></span></div><div id='labCandidates' class='preview hidden' style='margin-top:10px'></div></div>");
    fprintf(f, "<div class='panel'><h3>Command queue</h3><div class='help'>Selected commands are saved if needed, then sent in small groups so Stop can cancel quickly. Duplicate IR codes are skipped even when databases use different names.</div><div class='lab-summary'><span id='labSummary'>0 queued</span><span>send group limit %d</span></div><div class='queue-tools'><button id='labSelectAll' type='button' class='ghost'>Select all</button><button id='labSelectNone' type='button' class='ghost'>Select none</button><button id='labDropUnchecked' type='button' class='ghost'>Remove unchecked</button></div><div id='labQueue' class='queue-list'><div class='muted mini'>No commands queued.</div></div>", MAX_IR_BATCH_COMMANDS);
    fprintf(f, "<div class='actions'><button id='labImport' type='button' class='secondary'>Save selected</button><button id='labRun' type='button'>Send selected</button><button id='labStop' type='button' class='danger'>Stop sending</button></div>");
    fprintf(f, "<pre id='labLog' class='mini' style='margin-top:12px;max-height:180px'>Ready.</pre></div></div></section>");
    destroy_ir_inventory(inv);
}


struct bt_quick_key {
    const char *code;
    const char *label;
};



static void bt_type_options(FILE *f, const char *selected) {
    const char *types[] = {"btkeyboard", "btkeyboard-nexus", "fire", "ps3", "wii"};
    size_t i;
    for (i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        fprintf(f, "<option value='");
        html(f, types[i]);
        fprintf(f, "'%s>", selected && strcmp(selected, types[i]) == 0 ? " selected" : "");
        html(f, bt_type_label(types[i]));
        fprintf(f, "</option>");
    }
}

static void bt_device_options(FILE *f, const struct bt_inventory *inv) {
    int i;
    for (i = 0; i < inv->device_count; i++) {
        fprintf(f, "<option value='");
        html(f, inv->devices[i].id);
        fprintf(f, "'>");
        html(f, inv->devices[i].name);
        fprintf(f, " (");
        html(f, inv->devices[i].bdaddr);
        fprintf(f, ")</option>");
    }
}

static void bt_harmony_device_options(FILE *f) {
    fprintf(f, "<option value=''>-- Generic Bluetooth Host --</option>");
    char *dl_raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, NULL);
    if (!dl_raw) return;
    cJSON *root = cJSON_Parse(dl_raw);
    free(dl_raw);
    if (!root) return;
    cJSON *dwf = cJSON_GetObjectItemCaseSensitive(root, "DevicesWithFeatures");
    if (dwf && cJSON_IsArray(dwf)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, dwf) {
            cJSON *dev = cJSON_GetObjectItemCaseSensitive(item, "Device");
            if (!dev) continue;
            cJSON *jid = cJSON_GetObjectItemCaseSensitive(dev, "Id-");
            if (!jid) jid = cJSON_GetObjectItemCaseSensitive(dev, "Id");
            if (!jid) jid = cJSON_GetObjectItemCaseSensitive(dev, "id");
            char id_str[64] = {0};
            if (jid) {
                if (cJSON_IsNumber(jid)) snprintf(id_str, sizeof(id_str), "%ld", (long)jid->valuedouble);
                else if (cJSON_IsString(jid) && jid->valuestring) strncpy(id_str, jid->valuestring, sizeof(id_str) - 1);
            }
            cJSON *jname = cJSON_GetObjectItemCaseSensitive(dev, "Name");
            const char *name_str = (jname && cJSON_IsString(jname)) ? jname->valuestring : "";
            cJSON *jmodel = cJSON_GetObjectItemCaseSensitive(dev, "Model");
            const char *model_str = (jmodel && cJSON_IsString(jmodel)) ? jmodel->valuestring : "";
            if (id_str[0] && name_str[0]) {
                fprintf(f, "<option value='");
                html(f, id_str);
                fprintf(f, "'>");
                html(f, name_str);
                if (model_str[0]) {
                    fprintf(f, " (");
                    html(f, model_str);
                    fprintf(f, ")");
                }
                fprintf(f, "</option>");
            }
        }
    }
    cJSON_Delete(root);
}

static void bt_saved_devices_panel(FILE *f, const struct bt_inventory *inv) {
    int i, j;
    fprintf(f, "<div class='panel bt-saved'>"
               "<div style='display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px;'>"
               "<div><h3 style='margin:0 0 4px;'>Saved Bluetooth Targets & Commands</h3>"
               "<div class='help'>Store paired devices and saved macro commands to trigger them from activities or automations.</div></div>"
               "<button id='btNewDevToggleBtn' type='button' class='secondary'>+ Add New Target</button></div>");

    fprintf(f, "<div id='btNewDevBox' style='display:none;background:var(--wash);border:1px solid var(--line);border-radius:8px;padding:14px;margin-top:14px;'>"
               "<h4 style='margin:0 0 8px;'>Add Bluetooth Target</h4>"
               "<div class='row'><div><label>Device Name</label><input id='btNewDevName' placeholder='e.g. Living Room Shield'></div>"
               "<div><label>Bluetooth Address</label><input id='btNewDevAddr' placeholder='AA:BB:CC:DD:EE:FF'></div></div>"
               "<div class='actions' style='margin-top:10px;'><button id='btSaveNewDevBtn' type='button'>Save Target</button>"
               "<button id='btCancelNewDevBtn' type='button' class='secondary'>Cancel</button><span id='btNewDevStatus' class='save-status'></span></div></div>");

    if (inv->device_count == 0) {
        fprintf(f, "<div id='btNoDevicesMsg' class='muted mini' style='margin-top:14px;'>No Bluetooth devices saved yet. Pair a target above, or click '+ Add New Target'.</div></div>");
        return;
    }
    fprintf(f, "<div id='btSavedList' style='margin-top:14px;display:grid;gap:14px;'>");
    for (i = 0; i < inv->device_count; i++) {
        const struct bt_saved_device *dev = &inv->devices[i];
        fprintf(f, "<div class='panel' style='background:var(--wash);border:1px solid var(--line);border-radius:8px;padding:14px;'>"
                   "<div style='display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:8px;'>"
                   "<div><h4 style='margin:0 0 2px;'>");
        html(f, dev->name);
        fprintf(f, "</h4><div class='muted mini'>");
        html(f, dev->bdaddr);
        fprintf(f, " &bull; ID: %s &bull; %d saved command%s</div></div>", dev->id, dev->command_count, dev->command_count == 1 ? "" : "s");
        fprintf(f, "<div class='actions' style='margin:0;'><button type='button' class='danger mini' onclick=\"deleteBtDevice('%s')\">Delete Device</button></div></div>", dev->id);

        if (dev->command_count > 0) {
            fprintf(f, "<div style='display:grid;gap:8px;margin-top:12px;'>");
            for (j = 0; j < dev->command_count; j++) {
                const struct bt_saved_command *cmd = &dev->commands[j];
                fprintf(f, "<div style='display:flex;align-items:center;justify-content:space-between;gap:10px;padding:8px 10px;background:#fff;border:1px solid var(--line);border-radius:6px;flex-wrap:wrap;'>"
                           "<div><strong>");
                html(f, cmd->name);
                fprintf(f, "</strong> <span class='muted mini'>%d ms gap</span><pre class='mini' style='margin-top:4px;padding:4px 8px;max-height:60px;'>", cmd->delay_ms);
                html(f, cmd->script);
                fprintf(f, "</pre></div><div class='actions' style='margin:0;'>"
                           "<button type='button' class='mini' onclick=\"runBtCommand('%s','%s')\">Run</button>"
                           "<button type='button' class='danger mini' onclick=\"deleteBtCommand('%s','%s')\">Delete</button></div></div>",
                           dev->id, cmd->name, dev->id, cmd->name);
            }
            fprintf(f, "</div>");
        } else {
            fprintf(f, "<div class='muted mini' style='margin-top:10px;'>No macro commands saved for this device yet. Use the Script Studio above to save commands.</div>");
        }
        fprintf(f, "</div>");
    }
    fprintf(f, "</div></div>");
}

static void bluetooth_panel(FILE *f) {
    struct bt_inventory btinv;
    load_bt_inventory(&btinv);

    fprintf(f, "<section id='view-bluetooth' data-view='bluetooth' class='section'>"
               "<div class='section-head'><div><h2>Bluetooth Keyboard & Remote</h2>"
               "<div class='section-lead'>Control devices via Bluetooth HID. Pair the hub to your TV, Shield, or PC, then send buttons, type text, or run macro scripts.</div></div>"
               "<span id='btTopPill' class='pill'>Checking status...</span></div>");

    /* Status & Pairing Banner */
    fprintf(f, "<div class='panel'>"
               "<div style='display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:12px;'>"
               "<div><h3 style='margin:0 0 4px;'>Bluetooth Host Connection</h3>"
               "<div id='btHostDesc' class='muted mini'>Checking BTstack daemon...</div></div>"
               "<div id='btStatusBadge' class='pill'>Checking...</div></div>"
               "<div style='margin-top:14px;display:flex;gap:10px;align-items:center;flex-wrap:wrap;'>"
               "<div style='display:flex;align-items:center;gap:8px;'>"
               "<label style='margin:0;white-space:nowrap;'>Hub Name:</label>"
               "<input id='btPairName' value='Harmony Keyboard' style='width:180px;min-height:36px;padding:6px 10px;' maxlength='48'></div>"
               "<div style='display:flex;align-items:center;gap:8px;'>"
               "<label style='margin:0;white-space:nowrap;'>Link to Device:</label>"
               "<select id='btPairTargetDev' style='min-height:36px;padding:6px 10px;'>");
    bt_harmony_device_options(f);
    fprintf(f, "</select></div>"
               "<button id='btPairToggleBtn' type='button'>Start Pairing Mode</button>"
               "<button id='btDisconnBtn' type='button' class='danger' style='display:none;'>Disconnect All</button>"
               "<button id='btRefreshBtn' type='button' class='secondary'>Refresh Status</button></div>"
               "<div id='btPairHelp' class='help' style='margin-top:8px;'>"
               "Select target device, click <strong>Start Pairing Mode</strong>, then open Bluetooth settings on your host device and select the hub.</div>"
               "<div style='border-top:1px solid var(--line);margin-top:14px;padding-top:12px;'>"
               "<h4 style='margin:0 0 8px;'>Device Connections & Status</h4>"
               "<div id='btDevicesList'><div class='muted mini'>Loading devices...</div></div></div></div>");

    fprintf(f, "<div class='bt-layout'>");

    /* Full-width panel: Virtual Remote & On-Screen Keyboard */
    fprintf(f, "%s",
               "<div class='panel'>"
               "<div style='display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap;'>"
               "<h3 style='margin:0;'>Remote &amp; Keyboard</h3>"
               "<div class='actions' style='margin:0;display:flex;align-items:center;gap:6px;flex-wrap:wrap;'>"
               "<label class='mini muted' for='btTargetHostSelect' style='margin:0;'>Target:</label>"
               "<select id='btTargetHostSelect' style='font-size:12px;padding:4px 8px;min-height:30px;min-width:160px;'><option value=''>Auto (Active Host)</option></select>"
               "<button id='btFwdToggle' type='button' class='secondary' style='font-size:12px;padding:5px 9px;min-height:30px;'>Forward physical keyboard</button>"
               "<button id='btReleaseAll' type='button' class='danger' style='font-size:12px;padding:5px 9px;min-height:30px;'>Release all</button></div></div>"
               "<div id='btFwdNote' class='help' style='margin-top:4px;'>Click keys or toggle physical keyboard forwarding to type directly into the host.</div>"

                "<div style='display:flex;gap:14px;margin-top:12px;align-items:flex-start;overflow-x:auto;padding-bottom:4px;'>"
                "<div style='flex:0 0 auto;'><div class='kb-panel'>");

    /* Full Keyboard Rows */
    fprintf(f, "<div class='kb-row'>"
               "<button type='button' class='kb-key kb-15' data-key='escape'>Esc</button>"
               "<button type='button' class='kb-key' data-key='f1'>F1</button><button type='button' class='kb-key' data-key='f2'>F2</button>"
               "<button type='button' class='kb-key' data-key='f3'>F3</button><button type='button' class='kb-key' data-key='f4'>F4</button>"
               "<button type='button' class='kb-key' data-key='f5'>F5</button><button type='button' class='kb-key' data-key='f6'>F6</button>"
               "<button type='button' class='kb-key' data-key='f7'>F7</button><button type='button' class='kb-key' data-key='f8'>F8</button>"
               "<button type='button' class='kb-key' data-key='f9'>F9</button><button type='button' class='kb-key' data-key='f10'>F10</button>"
               "<button type='button' class='kb-key' data-key='f11'>F11</button><button type='button' class='kb-key' data-key='f12'>F12</button>"
               "<button type='button' class='kb-key' data-key='insert'>Ins</button><button type='button' class='kb-key' data-key='delete'>Del</button></div>"
               "<div class='kb-row'>"
               "<button type='button' class='kb-key' data-key='grave'>`</button>"
               "<button type='button' class='kb-key' data-key='1'>1</button><button type='button' class='kb-key' data-key='2'>2</button>"
               "<button type='button' class='kb-key' data-key='3'>3</button><button type='button' class='kb-key' data-key='4'>4</button>"
               "<button type='button' class='kb-key' data-key='5'>5</button><button type='button' class='kb-key' data-key='6'>6</button>"
               "<button type='button' class='kb-key' data-key='7'>7</button><button type='button' class='kb-key' data-key='8'>8</button>"
               "<button type='button' class='kb-key' data-key='9'>9</button><button type='button' class='kb-key' data-key='0'>0</button>"
               "<button type='button' class='kb-key' data-key='minus'>-</button><button type='button' class='kb-key' data-key='equal'>=</button>"
               "<button type='button' class='kb-key kb-2' data-key='backspace'>&#9003; Back</button>"
               "<button type='button' class='kb-key' data-key='home'>Home</button></div>"
               "<div class='kb-row'>"
               "<button type='button' class='kb-key kb-15' data-key='tab'>Tab</button>"
               "<button type='button' class='kb-key' data-key='q'>Q</button><button type='button' class='kb-key' data-key='w'>W</button>"
               "<button type='button' class='kb-key' data-key='e'>E</button><button type='button' class='kb-key' data-key='r'>R</button>"
               "<button type='button' class='kb-key' data-key='t'>T</button><button type='button' class='kb-key' data-key='y'>Y</button>"
               "<button type='button' class='kb-key' data-key='u'>U</button><button type='button' class='kb-key' data-key='i'>I</button>"
               "<button type='button' class='kb-key' data-key='o'>O</button><button type='button' class='kb-key' data-key='p'>P</button>"
               "<button type='button' class='kb-key' data-key='leftbracket'>[</button><button type='button' class='kb-key' data-key='rightbracket'>]</button>"
               "<button type='button' class='kb-key' data-key='backslash'>\\</button><button type='button' class='kb-key' data-key='end'>End</button></div>"
               "<div class='kb-row'>"
               "<button type='button' class='kb-key kb-2' data-key='capslock'>Caps</button>"
               "<button type='button' class='kb-key' data-key='a'>A</button><button type='button' class='kb-key' data-key='s'>S</button>"
               "<button type='button' class='kb-key' data-key='d'>D</button><button type='button' class='kb-key' data-key='f'>F</button>"
               "<button type='button' class='kb-key' data-key='g'>G</button><button type='button' class='kb-key' data-key='h'>H</button>"
               "<button type='button' class='kb-key' data-key='j'>J</button><button type='button' class='kb-key' data-key='k'>K</button>"
               "<button type='button' class='kb-key' data-key='l'>L</button><button type='button' class='kb-key' data-key='semicolon'>;</button>"
               "<button type='button' class='kb-key' data-key='apostrophe'>'</button>"
               "<button type='button' class='kb-key kb-225' data-key='enter'>Enter &#8629;</button>"
               "<button type='button' class='kb-key' data-key='pageup'>PgUp</button></div>"
               "<div class='kb-row'>"
               "<button type='button' class='kb-key kb-225 kb-mod' data-mod='shift'>&#8679; Shift</button>"
               "<button type='button' class='kb-key' data-key='z'>Z</button><button type='button' class='kb-key' data-key='x'>X</button>"
               "<button type='button' class='kb-key' data-key='c'>C</button><button type='button' class='kb-key' data-key='v'>V</button>"
               "<button type='button' class='kb-key' data-key='b'>B</button><button type='button' class='kb-key' data-key='n'>N</button>"
               "<button type='button' class='kb-key' data-key='m'>M</button><button type='button' class='kb-key' data-key='comma'>,</button>"
               "<button type='button' class='kb-key' data-key='period'>.</button><button type='button' class='kb-key' data-key='slash'>/</button>"
               "<button type='button' class='kb-key kb-225 kb-mod' data-mod='shift'>&#8679; Shift</button>"
               "<button type='button' class='kb-key' data-key='up'>&#9650;</button>"
               "<button type='button' class='kb-key' data-key='pagedown'>PgDn</button></div>"
               "<div class='kb-row'>"
               "<button type='button' class='kb-key kb-15 kb-mod' data-mod='ctrl'>Ctrl</button>"
               "<button type='button' class='kb-key kb-125 kb-mod' data-mod='win'>Win</button>"
               "<button type='button' class='kb-key kb-125 kb-mod' data-mod='alt'>Alt</button>"
               "<button type='button' class='kb-key kb-sp' data-key='space'>Space</button>"
               "<button type='button' class='kb-key kb-125 kb-mod' data-mod='alt'>Alt</button>"
               "<button type='button' class='kb-key kb-125 kb-mod' data-mod='win'>Win</button>"
               "<button type='button' class='kb-key kb-15 kb-mod' data-mod='ctrl'>Ctrl</button>"
               "<button type='button' class='kb-key' data-key='left'>&#9664;</button>"
               "<button type='button' class='kb-key' data-key='down'>&#9660;</button>"
               "<button type='button' class='kb-key' data-key='right'>&#9654;</button></div></div></div>");

    /* Unified D-Pad & Navigation + Media controls placed alongside keyboard */
    fprintf(f, "<div style='flex:0 0 auto;display:flex;gap:14px;background:var(--soft);border:1px solid var(--line);border-radius:8px;padding:12px 14px;align-items:center;'>"
               "<div style='display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;'>"
               "<div style='display:grid;grid-template-columns:repeat(3,40px);gap:4px;'>"
               "<div></div><button type='button' class='kb-key' style='width:40px;height:36px;min-width:40px;padding:0;' data-key='up'>&#9650;</button><div></div>"
               "<button type='button' class='kb-key' style='width:40px;height:36px;min-width:40px;padding:0;' data-key='left'>&#9664;</button>"
               "<button type='button' class='kb-key' style='width:40px;height:36px;min-width:40px;padding:0;font-weight:700;' data-key='select'>OK</button>"
               "<button type='button' class='kb-key' style='width:40px;height:36px;min-width:40px;padding:0;' data-key='right'>&#9654;</button>"
               "<div></div><button type='button' class='kb-key' style='width:40px;height:36px;min-width:40px;padding:0;' data-key='down'>&#9660;</button><div></div></div>"
               "<div style='display:flex;gap:5px;'>"
               "<button type='button' class='kb-key' style='height:30px;min-width:38px;padding:0 6px;font-size:11px;' data-key='back'>&#8617; Back</button>"
               "<button type='button' class='kb-key' style='height:30px;min-width:38px;padding:0 6px;font-size:11px;' data-key='home'>Home</button>"
               "<button type='button' class='kb-key' style='height:30px;min-width:38px;padding:0 6px;font-size:11px;' data-key='menu'>Menu</button></div></div>"
               "<div style='width:1px;align-self:stretch;background:var(--line);margin:0 2px;'></div>"
               "<div style='display:grid;grid-template-columns:repeat(2,64px);gap:6px;align-content:center;'>"
               "<button type='button' class='kb-key' style='height:34px;min-width:64px;padding:0 4px;font-size:11px;' data-key='vol_up'>Vol +</button>"
               "<button type='button' class='kb-key' style='height:34px;min-width:64px;padding:0 4px;font-size:11px;' data-key='vol_down'>Vol -</button>"
               "<button type='button' class='kb-key' style='height:34px;min-width:64px;padding:0 4px;font-size:11px;' data-key='mute'>Mute</button>"
               "<button type='button' class='kb-key' style='height:34px;min-width:64px;padding:0 4px;font-size:11px;' data-key='play_pause'>Play/Pause</button>"
               "<button type='button' class='kb-key' style='height:34px;min-width:64px;padding:0 4px;font-size:11px;' data-key='rewind'>&#9664;&#9664; Rew</button>"
               "<button type='button' class='kb-key' style='height:34px;min-width:64px;padding:0 4px;font-size:11px;' data-key='fastforward'>Fwd &#9654;&#9654;</button></div>"
               "</div></div></div>");

    /* Type Text & Scripting placed below Remote & Keyboard */
    fprintf(f, "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:16px;'>"
               "<div class='panel'><h3>Type Text</h3>"
               "<div class='help'>Send raw text strings or URLs directly to the active host.</div>"
               "<textarea id='btDirectText' placeholder='Type or paste text to send...' style='min-height:80px;margin-top:8px;'></textarea>"
               "<div class='actions' style='margin-top:8px;'>"
               "<button id='btSendTextBtn' type='button'>Send Text</button>"
               "<button id='btClearTextBtn' type='button' class='secondary'>Clear</button>"
               "<span id='btTextStatus' class='save-status'></span></div></div>"

               "<div class='panel'><h3>Macro Script Studio</h3>"
               "<div class='help'>Run sequence of keys, delays, and text. Supported: <code>TEXT &lt;words&gt;</code>, <code>KEY &lt;name&gt;</code>, <code>WAIT &lt;ms&gt;</code>, <code>COMBO &lt;ctrl+c&gt;</code>.</div>"
               "<textarea id='btMacroScript' class='textarea-tall' spellcheck='false' style='margin-top:8px;' placeholder='TEXT hello world&#10;WAIT 300&#10;KEY enter&#10;COMBO ctrl+l'></textarea>"
               "<div style='display:flex;align-items:center;gap:10px;margin-top:10px;flex-wrap:wrap;'>"
               "<div style='display:flex;align-items:center;gap:6px;'><label style='margin:0;'>Delay (ms):</label>"
               "<input id='btScriptDelay' type='number' value='35' style='width:75px;min-height:36px;padding:6px 8px;'></div>"
               "<button id='btRunScriptBtn' type='button'>Run Script</button>"
               "<button id='btStopScriptBtn' type='button' class='danger' disabled>Stop</button>"
               "<span id='btScriptStatus' class='save-status'></span></div>"
               "<div style='border-top:1px solid var(--line);margin-top:14px;padding-top:12px;'>"
               "<h4 style='margin:0 0 6px;'>Save as Device Command</h4>"
               "<div style='display:grid;grid-template-columns:1fr 1fr;gap:8px;'>"
               "<select id='btSaveDeviceSelect'>");
    bt_device_options(f, &btinv);
    fprintf(f, "</select>"
               "<input id='btSaveCommandName' placeholder='Command Name (e.g. Open App)'></div>"
               "<div class='actions' style='margin-top:8px;'>"
               "<button id='btSaveCommandBtn' type='button' class='secondary'>Save Command</button>"
               "<span id='btSaveCommandStatus' class='save-status'></span></div></div></div></div>");

    fprintf(f, "</div>"); /* bt-layout */

    /* Saved Devices & Commands Panel */
    bt_saved_devices_panel(f, &btinv);

    /* Diagnostics Log */
    fprintf(f, "<details style='margin-top:14px;'><summary>Bluetooth Activity & Diagnostics Log</summary>"
               "<div style='display:flex;justify-content:flex-end;margin-bottom:6px;'>"
               "<button id='btClearLogBtn' type='button' class='secondary mini' style='min-height:26px;padding:3px 8px;'>Clear Log</button></div>"
               "<pre id='btLog' class='mini' style='max-height:220px;'>Ready.</pre></details>");

    fprintf(f, "</section>");
}

static void elite_rf_panel(FILE *f) {
    fprintf(f, "<section id='view-rf-remote' data-view='rf-remote' class='section'>");
    fprintf(f, "<div class='section-head'><div><h2>Logitech Harmony Elite Remote</h2><div class='section-lead'>Direct 2.4GHz CC2544 RF link with full button and touchscreen mapping per activity.</div></div><span class='pill'>RF & Screen Mapping</span></div>");

    /* Status & Pairing Panel */
    fprintf(f, "<div class='panel' style='margin-bottom:20px;border-left:4px solid var(--accent);'>");
    fprintf(f, "<div style='display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;'>");
    fprintf(f, "<div><h3 style='margin:0 0 4px 0;'>RF Link Status</h3>");
    fprintf(f, "<div class='muted mini'>Hardware CC2544 RF transceiver with codex_elite remote firmware.</div></div>");
    fprintf(f, "<span id='rfStatusBadge' class='pill' style='background:#027a48;color:#fff;'>Checking RF...</span>");
    fprintf(f, "</div>");

    fprintf(f, "<div class='grid three' style='margin-top:14px;'>");
    fprintf(f, "<div class='stat-card'><small>Paired Remote RF Address</small><strong id='rfPairedAddr'>--</strong></div>");
    fprintf(f, "<div class='stat-card'><small>CC2544 FW Version</small><strong id='rfFwVer'>--</strong></div>");
    fprintf(f, "<div class='stat-card'><small>Pairing State</small><strong id='rfPairState'>--</strong></div>");
    fprintf(f, "</div>");

    fprintf(f, "<div class='actions' style='margin-top:14px;gap:8px;'>");
    fprintf(f, "<button type='button' id='rfPairBtn' class='primary' onclick='pairRfRemote()'>Start RF Pairing (30s)</button>");
    fprintf(f, "<button type='button' id='rfStopPairBtn' class='secondary' onclick='stopPairRfRemote()' style='display:none;'>Cancel Pairing</button>");
    fprintf(f, "<button type='button' id='rfUnpairBtn' class='danger' onclick='unpairRfRemote()'>Unpair Remote</button>");
    fprintf(f, "<button type='button' class='secondary' onclick='refreshRfStatus()'>Refresh Status</button>");
    fprintf(f, "</div>");
    fprintf(f, "<div id='rfPairMsg' class='help' style='margin-top:10px;display:none;'></div>");
    fprintf(f, "</div>");

    /* Remote Visual & Mapping Editor */
    fprintf(f, "<div class='elite-layout'>");

    /* Left column: Visual Harmony Elite remote */
    fprintf(f, "<div class='ir-remote-card'><div class='elite-remote-shell'>");
    fprintf(f, "<div class='elite-remote-skin'>");
    fprintf(f, "<img data-remote-elite-skin alt='Logitech Harmony Elite Remote'>");

    /* Top: Power Button */
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:38%%;top:9.2%%;width:24%%;height:2.6%%;' data-btn='PowerOffActivity' title='Power Off (All Off)'></button>");

    /* Touchscreen display area */
    fprintf(f, "<div class='elite-hotspot rect' style='left:33%%;top:13%%;width:34%%;height:25.5%%;border-color:rgba(0,255,180,.2);background:rgba(0,255,180,.03);' title='Touchscreen Display (240x320)'></div>");

    /* Capacitive touch buttons below screen */
    fprintf(f, "<button type='button' class='elite-hotspot rect' style='left:33%%;top:39.8%%;width:16%%;height:2.2%%;' data-btn='WatchTVActivity' title='Activities Touch Key'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot rect' style='left:51%%;top:39.8%%;width:16%%;height:2.2%%;' data-btn='Devices' title='Devices Touch Key'></button>");

    /* Transport row 1 */
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:33%%;top:42.6%%;width:10.5%%;height:2.4%%;' data-btn='Rewind' title='Rewind'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:44.75%%;top:42.6%%;width:10.5%%;height:2.4%%;' data-btn='Play' title='Play / Pause'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:56.5%%;top:42.6%%;width:10.5%%;height:2.4%%;' data-btn='FastForward' title='Fast Forward'></button>");

    /* Transport row 2 */
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:33%%;top:45.5%%;width:10.5%%;height:2.4%%;' data-btn='Record' title='Record'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:44.75%%;top:45.5%%;width:10.5%%;height:2.4%%;' data-btn='Pause' title='Pause'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:56.5%%;top:45.5%%;width:10.5%%;height:2.4%%;' data-btn='Stop' title='Stop'></button>");

    /* Skip / Chapter row */
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:33%%;top:48.5%%;width:16%%;height:2.4%%;' data-btn='PrevChannel' title='Skip Prev'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:51%%;top:48.5%%;width:16%%;height:2.4%%;' data-btn='ChannelUp' title='Skip Next'></button>");

    /* D-PAD Ring & OK */
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:42%%;top:50.5%%;width:16%%;height:3.8%%;' data-btn='DirectionUp' title='Up'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:42%%;top:54.6%%;width:16%%;height:4.2%%;' data-btn='Select' title='OK / Select'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:42%%;top:59.2%%;width:16%%;height:3.8%%;' data-btn='DirectionDown' title='Down'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:33%%;top:54.6%%;width:8.5%%;height:4.2%%;' data-btn='DirectionLeft' title='Left'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:58.5%%;top:54.6%%;width:8.5%%;height:4.2%%;' data-btn='DirectionRight' title='Right'></button>");

    /* Back & Exit */
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:33%%;top:63.5%%;width:16%%;height:2.8%%;' data-btn='Back' title='Back'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:51%%;top:63.5%%;width:16%%;height:2.8%%;' data-btn='Exit' title='Exit'></button>");

    /* Volume & Channel & Mute */
    fprintf(f, "<button type='button' class='elite-hotspot rect' style='left:33%%;top:67%%;width:11%%;height:3.5%%;' data-btn='VolumeUp' title='Volume Up'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot rect' style='left:33%%;top:70.7%%;width:11%%;height:3.5%%;' data-btn='VolumeDown' title='Volume Down'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:45%%;top:68.5%%;width:10%%;height:2.8%%;' data-btn='VolumeMute' title='Mute'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot rect' style='left:56%%;top:67%%;width:11%%;height:3.5%%;' data-btn='ChannelUp' title='Channel Up'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot rect' style='left:56%%;top:70.7%%;width:11%%;height:3.5%%;' data-btn='ChannelDown' title='Channel Down'></button>");

    /* Guide / Info / Menu / DVR */
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:33%%;top:74.8%%;width:10.5%%;height:2.5%%;' data-btn='Guide' title='Guide'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:44.75%%;top:74.8%%;width:10.5%%;height:2.5%%;' data-btn='Info' title='Info'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:56.5%%;top:74.8%%;width:10.5%%;height:2.5%%;' data-btn='Menu' title='Menu'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:56.5%%;top:77.7%%;width:10.5%%;height:2.5%%;' data-btn='Dvr' title='DVR'></button>");

    /* Home Automation */
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:33%%;top:80.8%%;width:7.5%%;height:2.5%%;' data-btn='Ha1' title='HA Light'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:41.5%%;top:80.8%%;width:7.5%%;height:2.5%%;' data-btn='Ha2' title='HA Sun/Brightness'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot rect' style='left:50%%;top:80.8%%;width:8.5%%;height:2.5%%;' data-btn='RockerDown' title='Automation Rocker Down'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot rect' style='left:59%%;top:80.8%%;width:8.5%%;height:2.5%%;' data-btn='RockerUp' title='Automation Rocker Up'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:33%%;top:83.8%%;width:16%%;height:2.8%%;' data-btn='Ha3' title='HA Power'></button>");
    fprintf(f, "<button type='button' class='elite-hotspot' style='left:51%%;top:83.8%%;width:16%%;height:2.8%%;' data-btn='Ha4' title='HA Plug/Socket'></button>");

    fprintf(f, "</div>"); /* elite-remote-skin */
    fprintf(f, "</div><div class='help'>Click any button or area on the remote graphic to inspect or customize its mapping.</div></div>");

    /* Right column: Editor & Presets */
    fprintf(f, "<div>");

    /* 1. Activity Selector */
    fprintf(f, "<div class='panel'><h3>1. Select Activity Context</h3>");
    fprintf(f, "<div class='help'>Button mappings are configured separately for each Activity, plus a default mode when the hub is Off / Idle.</div>");
    fprintf(f, "<div class='row' style='margin-top:10px;'><div><label>Current Activity Mapping Mode</label><select id='eliteActivitySelect'><option value='-1'>Off / Idle (Default)</option></select></div>");
    fprintf(f, "<div style='display:flex;align-items:flex-end;'><button id='eliteSaveAllBtn' type='button' style='width:100%%;'>Save All Mappings</button></div></div>");
    fprintf(f, "</div>");

    /* 2. Button Action Config */
    fprintf(f, "<div class='panel' style='margin-top:14px;'><h3>2. Configure Button: <span id='eliteCurrentBtnName' style='color:var(--accent);'>Select (OK)</span></h3>");
    fprintf(f, "<label>Action Type</label><select id='eliteActionType'><option value='unmapped'>Unmapped (Do Nothing)</option><option value='device_cmd'>Send Device Command (IR/BT)</option><option value='activity_start'>Start Activity</option><option value='activity_stop'>Power Off (Stop Activity)</option></select>");
    fprintf(f, "<div id='eliteWrapTargetDevice' class='hidden'><label>Target Device</label><select id='eliteTargetDevice'></select></div>");
    fprintf(f, "<div id='eliteWrapTargetCommand' class='hidden'><label>Target Command</label><select id='eliteTargetCommand'></select></div>");
    fprintf(f, "<div id='eliteWrapTargetActivity' class='hidden'><label>Target Activity to Launch</label><select id='eliteTargetActivity'></select></div>");
    fprintf(f, "<div class='actions' style='margin-top:14px;'><button id='eliteApplyBtn' type='button' class='primary'>Apply to Selected Button</button><button id='eliteClearBtn' type='button' class='ghost'>Clear / Unmap</button></div>");
    fprintf(f, "</div>");

    /* 3. Quick Auto-Map Preset */
    fprintf(f, "<div class='panel' style='margin-top:14px;'><h3>3. Quick Device Auto-Map</h3>");
    fprintf(f, "<div class='help'>Instantly map all navigation (D-PAD, OK, Back, Menu, Exit), Volume, and Transport keys to a selected target device for this activity.</div>");
    fprintf(f, "<div class='row' style='margin-top:10px;'><div><label>Select Target Device to Auto-Map</label><select id='elitePresetDev'></select></div>");
    fprintf(f, "<div style='display:flex;align-items:flex-end;'><button id='eliteRunPresetBtn' type='button' class='secondary' style='width:100%%;'>Auto-Map All Standard Keys</button></div></div>");
    fprintf(f, "</div>");

    fprintf(f, "</div>"); /* Right column */
    fprintf(f, "</div>"); /* elite-layout */
    fprintf(f, "</section>");
}

static void remotes_panel(FILE *f) {
    int btstack_on = is_btstack_running();
    fprintf(f, "<section id='view-remotes' data-view='remotes' class='section'>");
    fprintf(f, "<div class='section-head'><div><h2>Physical Bluetooth Remote</h2><div class='section-lead'>Pair a physical Bluetooth remote (Homatics B25 / Android TV BLE remote) and map buttons per running activity or power-off state.</div></div><span class='pill'>BLE Mapping</span></div>");
    fprintf(f, "<div id='b25BtstackGuard' class='callout' style='%sborder-color:#b42318;background:#fef3f2;color:#912018;margin-bottom:16px;'>"
               "<strong>BTstack Service Not Running</strong>"
               "Physical Bluetooth Remote control (BLE / HOGP bonding) requires the <strong>BTstack</strong> daemon to be active."
               "</div>",
               btstack_on ? "display:none;" : "");
    fprintf(f, "<div id='b25ControlsWrapper' style='%s'>", btstack_on ? "" : "display:none;");
    fprintf(f, "<div class='b25-layout'>");

    /* Left column: Visual Remote */
    fprintf(f, "<div class='ir-remote-card'><div class='b25-remote-shell'>");
    fprintf(f, "<div class='b25-remote-skin'>");
    fprintf(f, "<img data-remote-b25-skin alt='Homatics B25 Remote'>");

    /* Top row */
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:13.1%%;top:3.3%%;width:14.7%%;height:3.1%%;' data-btn='power' title='Power'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:72.8%%;top:3.3%%;width:14.7%%;height:3.1%%;' data-btn='input' title='Input / Source'></button>");

    /* Number keypad */
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:13.1%%;top:8.9%%;width:14.7%%;height:3.1%%;' data-btn='1' title='1'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:42.4%%;top:8.9%%;width:14.7%%;height:3.1%%;' data-btn='2' title='2'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:72.8%%;top:8.9%%;width:14.7%%;height:3.1%%;' data-btn='3' title='3'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:13.1%%;top:13.5%%;width:14.7%%;height:3.1%%;' data-btn='4' title='4'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:42.4%%;top:13.5%%;width:14.7%%;height:3.1%%;' data-btn='5' title='5'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:72.8%%;top:13.5%%;width:14.7%%;height:3.1%%;' data-btn='6' title='6'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:13.1%%;top:17.8%%;width:14.7%%;height:3.1%%;' data-btn='7' title='7'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:42.4%%;top:17.8%%;width:14.7%%;height:3.1%%;' data-btn='8' title='8'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:72.8%%;top:17.8%%;width:14.7%%;height:3.1%%;' data-btn='9' title='9'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:13.1%%;top:22.4%%;width:14.7%%;height:3.1%%;' data-btn='guide' title='Guide'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:42.4%%;top:22.4%%;width:14.7%%;height:3.1%%;' data-btn='0' title='0'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:72.8%%;top:22.4%%;width:14.7%%;height:3.1%%;' data-btn='info' title='Info'></button>");

    /* Colors */
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:12.6%%;top:26.8%%;width:13.6%%;height:2.7%%;' data-btn='red' title='Red'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:33.0%%;top:26.8%%;width:13.6%%;height:2.7%%;' data-btn='green' title='Green'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:53.4%%;top:26.8%%;width:13.6%%;height:2.7%%;' data-btn='yellow' title='Yellow'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:73.8%%;top:26.8%%;width:13.6%%;height:2.7%%;' data-btn='blue' title='Blue'></button>");

    /* Function keys */
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:15.2%%;top:34.0%%;width:15.7%%;height:3.3%%;' data-btn='watchlist' title='Watchlist'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:39.8%%;top:32.0%%;width:20.9%%;height:4.5%%;' data-btn='assistant' title='Google Assistant'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:69.1%%;top:34.0%%;width:15.7%%;height:3.3%%;' data-btn='settings' title='Settings'></button>");

    /* D-Pad */
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:34.6%%;top:42.1%%;width:31.4%%;height:6.7%%;' data-btn='select' title='Select / OK'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:31.4%%;top:37.4%%;width:37.7%%;height:4.7%%;' data-btn='up' title='Up'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:31.4%%;top:48.8%%;width:37.7%%;height:4.7%%;' data-btn='down' title='Down'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:12.0%%;top:41.4%%;width:22.0%%;height:8.0%%;' data-btn='left' title='Left'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:66.5%%;top:41.4%%;width:22.0%%;height:8.0%%;' data-btn='right' title='Right'></button>");

    /* Lower Navigation */
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:14.7%%;top:54.0%%;width:15.7%%;height:3.3%%;' data-btn='back' title='Back'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:39.8%%;top:55.0%%;width:20.9%%;height:4.5%%;' data-btn='home' title='Home'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:69.1%%;top:54.0%%;width:15.7%%;height:3.3%%;' data-btn='live_tv' title='Live TV'></button>");

    /* Volume / Channel / Mute */
    fprintf(f, "<button type='button' class='b25-hotspot rect' style='left:14.7%%;top:61.0%%;width:16.8%%;height:4.5%%;' data-btn='vol_up' title='Volume Up'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot rect' style='left:14.7%%;top:67.3%%;width:16.8%%;height:4.5%%;' data-btn='vol_down' title='Volume Down'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot' style='left:41.9%%;top:64.7%%;width:15.7%%;height:3.3%%;' data-btn='mute' title='Mute'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot rect' style='left:68.6%%;top:61.0%%;width:16.8%%;height:4.5%%;' data-btn='ch_up' title='Channel Up'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot rect' style='left:68.6%%;top:67.3%%;width:16.8%%;height:4.5%%;' data-btn='ch_down' title='Channel Down'></button>");

    /* App shortcut pills */
    fprintf(f, "<button type='button' class='b25-hotspot rect' style='left:12.6%%;top:74.2%%;width:33.5%%;height:3.2%%;' data-btn='youtube' title='YouTube'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot rect' style='left:54.5%%;top:74.2%%;width:33.5%%;height:3.2%%;' data-btn='netflix' title='Netflix'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot rect' style='left:12.6%%;top:78.6%%;width:33.5%%;height:3.2%%;' data-btn='prime_video' title='Prime Video'></button>");
    fprintf(f, "<button type='button' class='b25-hotspot rect' style='left:54.5%%;top:78.6%%;width:33.5%%;height:3.2%%;' data-btn='google_play' title='Google Play'></button>");

    fprintf(f, "</div>"); /* b25-remote-skin */
    fprintf(f, "</div><div class='help'>Click any button on the remote graphic to inspect or customize its mapping.</div></div>");

    /* Right column: Editor & Pairing */
    fprintf(f, "<div>");

    /* Activity selector panel */
    fprintf(f, "<div class='panel'><h3>1. Select Activity Context</h3>");
    fprintf(f, "<div class='help'>Button mappings are configured separately for each Activity, plus a default mode when the hub is Off / Idle.</div>");
    fprintf(f, "<div class='row' style='margin-top:10px;'><div><label>Current Activity Mapping Mode</label><select id='b25ActivitySelect'><option value='-1'>Off / Idle (Default)</option></select></div>");
    fprintf(f, "<div style='display:flex;align-items:flex-end;'><button id='b25SaveAllBtn' type='button' style='width:100%%;'>Save All Mappings</button></div></div>");
    fprintf(f, "</div>");

    /* Button Action Configuration panel */
    fprintf(f, "<div class='panel' style='margin-top:14px;'><h3>2. Button Configuration: <span id='b25CurrentBtnName' style='color:var(--accent);'>Select a button</span></h3>");
    fprintf(f, "<div class='row'><div><label>Action Type</label><select id='b25ActionType'>");
    fprintf(f, "<option value='unmapped'>Unmapped (Do Nothing)</option>");
    fprintf(f, "<option value='device_cmd'>Send Device Command (IR/BT)</option>");
    fprintf(f, "<option value='activity_start'>Start Activity</option>");
    fprintf(f, "<option value='activity_stop'>Stop Activity (Power Off)</option>");
    fprintf(f, "</select></div>");

    /* Action specific selectors */
    fprintf(f, "<div id='b25WrapTargetDevice'><label>Target Device</label><select id='b25TargetDevice'></select></div>");
    fprintf(f, "<div id='b25WrapTargetCommand'><label>Command</label><select id='b25TargetCommand'></select></div>");
    fprintf(f, "<div id='b25WrapTargetActivity' class='hidden'><label>Activity to Start</label><select id='b25TargetActivity'></select></div>");
    fprintf(f, "</div>");

    fprintf(f, "<div class='actions' style='margin-top:14px;'><button id='b25ApplyBtn' type='button'>Apply to Selected Button</button>");
    fprintf(f, "<button id='b25ClearBtn' type='button' class='danger'>Clear Button</button></div>");

    /* Quick Preset Tool */
    fprintf(f, "<details style='margin-top:16px;'><summary>Quick Preset: Auto-map Device Buttons (IR/BT)</summary>");
    fprintf(f, "<div class='help'>Automatically assigns all standard buttons (D-pad, OK, Back, Home, Volume, Channel, Numbers, Colors, and Power) for the selected activity to the chosen device using <strong>Send Device Command (IR/BT)</strong>.</div>");
    fprintf(f, "<div class='row' style='margin-top:10px;'><div><label>Target Device</label><select id='b25PresetBtDevice'></select></div>");
    fprintf(f, "<div style='display:flex;align-items:flex-end;'><button id='b25RunPresetBtn' type='button' class='secondary' style='width:100%%;'>Auto-map buttons to device</button></div></div>");
    fprintf(f, "</details>");

    fprintf(f, "</div>"); /* panel */

    /* Pairing Panel */
    fprintf(f, "<div class='panel' style='margin-top:14px;'><h3>3. Pair Physical Remote</h3>");
    fprintf(f, "<div class='help'>Hold <strong>Back + Home</strong> on the Homatics remote for 3 seconds until the LED flashes to enter pairing mode, then click Scan.</div>");
    fprintf(f, "<div class='row' style='margin-top:10px;'><div><button id='b25ScanBtn' type='button' class='secondary' style='width:100%%;'>Scan for Remote</button></div>");
    fprintf(f, "<div><select id='b25DiscoveredList'><option value=''>No devices found yet</option></select></div></div>");
    fprintf(f, "<div class='actions'><button id='b25PairBtn' type='button'>Pair Selected Device</button></div>");
    fprintf(f, "<pre id='b25PairLog' class='mini' style='margin-top:10px;'>Remote listener daemon: check status...</pre>");
    fprintf(f, "</div>"); /* panel */

    fprintf(f, "</div>"); /* right col */
    fprintf(f, "</div>"); /* b25-layout */
    fprintf(f, "</div>"); /* b25ControlsWrapper */
    fprintf(f, "</section>");
}

static void system_panel(FILE *f) {
    char info[8192], logs[8192], netinfo[4096];
    struct webui_auth_config auth;
    load_webui_auth(&auth);
    run_cmd("echo '--- uname ---'; uname -a; echo; echo '--- memory ---'; cat /proc/meminfo; echo; echo '--- mounts ---'; mount; echo; echo '--- processes ---'; ps", info, sizeof(info));
    run_cmd("echo '--- sent bt keycodes ---'; cat /tmp/bt_sent.log 2>/dev/null; echo; echo '--- bt remote events ---'; cat /tmp/bt_remote_events.log 2>/dev/null; echo; echo '--- ir events log ---'; cat /tmp/ir-events.log 2>/dev/null; echo; echo '--- btstack log ---'; cat /tmp/codex-btstack.log 2>/dev/null; echo; echo '--- bt remote log ---'; cat /tmp/codex-bt-remote.log 2>/dev/null; echo; echo '--- bt reconnect log ---'; cat /tmp/codex-bt-reconnect.log 2>/dev/null; echo; echo '--- bluetooth connections ---'; hcitool con 2>/dev/null; echo; echo '--- startup log ---'; sed -n '1,40p' /tmp/codex-init.log 2>/dev/null; echo; echo '--- recovery log ---'; cat /tmp/codex-recovery.log 2>/dev/null; echo; echo '--- local service syslog ---'; logread 2>/dev/null | grep -i 'codex\\|mqtt' 2>/dev/null; echo; echo '--- bluetooth dmesg ---'; dmesg 2>/dev/null | grep -i 'blue\\|hci' | sed -e :a -e '$q;N;26,$D;ba'", logs, sizeof(logs));
    run_cmd("echo '--- ifconfig ---'; ifconfig 2>/dev/null; echo; echo '--- routing table ---'; route -n 2>/dev/null", netinfo, sizeof(netinfo));
    fprintf(f, "<section id='view-system' data-view='system' class='section'><div class='section-head'><div><h2>System</h2><div class='section-lead'>Check device information, read logs, update the web interface, or reboot when a saved change needs it.</div></div></div>");
    fprintf(f, "<div class='grid three'><details><summary>System information</summary><pre>");
    html(f, info);
    fprintf(f, "</pre></details><details><summary>Logs</summary><pre>");
    html(f, logs[0] ? logs : "no matching logs");
    fprintf(f, "</pre></details><details><summary>Network details</summary><pre>");
    html(f, netinfo[0] ? netinfo : "network info not available");
    fprintf(f, "</pre></details></div>");
    int dbg_logging = (access(DEBUG_LOG_CONFIG, F_OK) == 0 || access(BT_DEBUG_FLAG, F_OK) == 0);
    fprintf(f, "<div class='panel' style='margin-top:12px'><h3>Debug logging</h3><div class='help'>Enable detailed verbose debug logging (raw BLE GATT packet notifications, HID report hex dumps, and verbose dispatching) in volatile /tmp logs. Leave off during normal use to keep memory logs clean.</div><form id='systemDebugForm' method='post' action='/system#system' autocomplete='off'>");
    fprintf(f, "<label class='inline-check'><input type='checkbox' name='debugLogging' value='1' %s> Enable verbose debug logging</label>", dbg_logging ? "checked" : "");
    fprintf(f, "<div class='actions'><button name='action' value='debug_logging' type='submit'>Save logging setting</button></div></form></div>");
    fprintf(f, "<div class='panel' style='margin-top:12px'><h3>Web UI sign-in</h3><div class='help'>Optional HTTP Basic authentication for every page, API call, and export. Leave it off for a trusted local-only hub, or enable it when the hub is reachable by guests or other devices.</div><form id='systemAuthForm' method='post' action='/system#system' autocomplete='off'>");
    fprintf(f, "<label class='inline-check'><input type='checkbox' name='authEnabled' value='1' %s> Require username and password</label>", auth.enabled ? "checked" : "");
    fprintf(f, "<div class='grid two'><div><label>Username</label><input name='authUsername' autocomplete='username' required value='");
    html(f, auth.username[0] ? auth.username : "admin");
    fprintf(f, "'><div class='help'>Use plain text without a colon.</div></div><div><label>New password</label><input name='authPassword' type='password' autocomplete='new-password' placeholder='Leave blank to keep current password'><div class='help'>Required the first time you enable sign-in.</div></div></div>");
    fprintf(f, "<div class='help'>Current mode: <strong>%s</strong>.</div>", auth.enabled ? "sign-in required" : "open on local network");
    fprintf(f, "<div class='actions'><button name='action' value='auth' type='submit'>Save sign-in setting</button><span id='authSaveStatus' class='save-status subtle' style='margin-left:8px;align-self:center;'></span></div></form></div>");
    fprintf(f, "<div class='panel' style='margin-top:12px'><h3>Software update</h3><div class='help'>Check the public release files, copy newer binaries to the hub, verify checksums, and restart the local services. SSH access is not changed. The default public repository tries GitHub and CDN mirrors without a token. Use the token field only for private repositories.</div><form id='updateForm' autocomplete='off' onsubmit='return false'><div class='grid two'><div><label for='updateRepo'>Optional update mirror URL</label><input id='updateRepo' autocomplete='url' value='https://raw.githubusercontent.com/Ripthulhu/harmony-hub-control/main/payload/bin/'></div><div><label for='updateToken'>GitHub token (private repos only)</label><input id='updateToken' type='password' autocomplete='new-password' placeholder='optional; used only by this browser'></div></div><div class='actions'><button id='updateCheck' type='button' class='secondary'>Check for updates</button><button id='updateInstall' type='button'>Install update</button><button id='updateRefresh' type='button' class='secondary'>Show installed versions</button></div></form><pre id='updateLog' class='mini'>Ready. Check the public repo, or paste a token if the repo is private.</pre></div><div class='panel' style='margin-top:12px'><div class='help'>Refresh Home Assistant discovery if new devices or commands do not appear after changes.</div><form id='systemActionsForm' method='post' action='/system#system'><div class='actions'><button name='action' value='rediscover' type='submit'>Refresh Home Assistant discovery</button><button name='action' value='reboot' type='submit' class='secondary'>Reboot hub</button><span id='sysActionStatus' class='save-status subtle' style='margin-left:8px;align-self:center;'></span></div></form></div></section>");
}

static void activities_panel(FILE *f) {
    fprintf(f,
        "<section id='view-activities' data-view='activities' class='section'>"
        "<div class='section-head'><div><h2>Activities</h2>"
        "<div class='section-lead'>Run activities, switch inputs, control power sequences, and customize start/stop button commands and delays.</div></div>"
        "<div class='actions'>"
        "<button type='button' id='actRefreshBtn' class='ghost'>Refresh</button>"
        "<button type='button' id='actNewBtn' class='secondary'>+ New Activity</button>"
        "<button type='button' id='actPowerOffBtn' class='danger'>Power Off Hub</button>"
        "</div></div>"
        "<div class='panel' id='actCurrentBanner' style='display:flex;align-items:center;justify-content:space-between;gap:12px;background:var(--soft2);border-color:#b9d8d3'>"
        "<div><div class='muted mini' style='text-transform:uppercase;letter-spacing:.04em'>Current Hub State</div>"
        "<div style='font-size:18px;font-weight:700;margin-top:2px' id='actCurrentName'>PowerOff</div>"
        "<div class='muted mini' id='actCurrentMeta'>Activity ID: -1</div></div>"
        "<div><span class='badge ok' id='actCurrentStatusBadge'>Idle</span></div></div>"
        "<div class='panel'><h3>Configured Activities</h3>"
        "<div class='cards' id='actGrid' style='grid-template-columns:repeat(auto-fit,minmax(220px,1fr));margin-top:10px'>"
        "<div class='muted'>Loading activities...</div></div></div>"
        "<div class='panel' id='actEditorBox' style='display:none'>"
        "<div style='display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:12px;border-bottom:1px solid var(--line);padding-bottom:10px'>"
        "<div><h3 id='actEditorTitle' style='margin:0'>Edit Activity</h3>"
        "<div class='muted mini' id='actEditorSubtitle'>Configure activity settings and command sequences</div></div>"
        "<button type='button' id='actEditorCloseBtn' class='ghost'>Close Editor</button></div>"
        "<form id='actForm' onsubmit='return false;'><input type='hidden' id='actEditId' name='id' value=''>"
        "<div class='row'><div><label for='actEditName'>Activity Name</label>"
        "<input type='text' id='actEditName' name='name' required placeholder='e.g. Watch TV, Play PS5'></div>"
        "<div><label for='actEditType'>Activity Type / Icon</label>"
        "<select id='actEditType' name='type'>"
        "<option value='VirtualTelevision'>Watch TV</option>"
        "<option value='VirtualGeneric'>Watch Movie / Stream</option>"
        "<option value='VirtualMusic'>Listen to Music</option>"
        "<option value='VirtualGameConsole'>Play Game</option>"
        "<option value='Custom'>Custom Activity</option>"
        "<option value='PowerOff'>PowerOff</option>"
        "</select></div></div>"
        "<div class='row' style='margin-top:10px'><div><label for='actEditOrder'>Display Order</label>"
        "<input type='number' id='actEditOrder' name='order' min='0' max='99' value='1'></div>"
        "<div><label>Participating Devices</label><div id='actDevicesCheckboxes' style='display:flex;flex-wrap:wrap;gap:10px;padding-top:6px'></div></div></div>"
        "<div style='margin-top:18px;border-top:1px solid var(--line);padding-top:14px'>"
        "<div style='display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:8px'>"
        "<div><strong>Turn-On Sequence (StartSequence)</strong>"
        "<div class='muted mini'>Commands sent in order when entering this activity. Add delays if TVs or AVRs take time to boot.</div></div>"
        "<div class='actions' style='margin:0'>"
        "<button type='button' id='actAddStartCmdBtn' class='secondary mini'>+ Add Command</button>"
        "<button type='button' id='actAddStartDelayBtn' class='ghost mini'>+ Add Delay</button>"
        "<button type='button' id='actTestStartSeqBtn' class='ghost mini'>Test Live</button>"
        "</div></div>"
        "<div id='actStartStepsList' class='queue-list' style='min-height:60px;max-height:280px'>"
        "<div class='muted mini' style='padding:8px'>No start steps configured.</div></div></div>"
        "<div style='margin-top:18px;border-top:1px solid var(--line);padding-top:14px'>"
        "<div style='display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:8px'>"
        "<div><strong>Turn-Off Sequence (StopSequence)</strong>"
        "<div class='muted mini'>Commands sent in order when leaving this activity or powering off.</div></div>"
        "<div class='actions' style='margin:0'>"
        "<button type='button' id='actAddStopCmdBtn' class='secondary mini'>+ Add Command</button>"
        "<button type='button' id='actAddStopDelayBtn' class='ghost mini'>+ Add Delay</button>"
        "<button type='button' id='actTestStopSeqBtn' class='ghost mini'>Test Live</button>"
        "</div></div>"
        "<div id='actStopStepsList' class='queue-list' style='min-height:60px;max-height:280px'>"
        "<div class='muted mini' style='padding:8px'>No stop steps configured.</div></div></div>"
        "<div class='actions' style='margin-top:18px;border-top:1px solid var(--line);padding-top:14px;display:flex;justify-content:space-between'>"
        "<div class='actions' style='margin:0'>"
        "<button type='button' id='actSaveBtn'>Save Activity</button>"
        "<button type='button' id='actCancelBtn' class='ghost'>Cancel</button></div>"
        "<button type='button' id='actDeleteBtn' class='danger' style='display:none'>Delete Activity</button></div>"
        "<div id='actEditorStatus' class='subtle' style='margin-top:8px'></div>"
        "</form></div>"
        "<div class='panel' style='margin-top:16px'>"
        "<div style='display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:6px'>"
        "<div><h3 style='margin:0'>Activity Transition Preview</h3>"
        "<div class='muted mini'>Simulate switching between activities (or PowerOff) to inspect command sequence and delay timeline.</div></div></div>"
        "<div class='grid two' style='margin-top:10px'>"
        "<div><label for='previewFromAct'>From Activity</label><select id='previewFromAct'></select></div>"
        "<div><label for='previewToAct'>To Activity</label><select id='previewToAct'></select></div>"
        "</div>"
        "<div class='actions' style='margin-top:10px'>"
        "<button type='button' id='previewSimulateBtn' class='secondary'>Simulate Transition</button>"
        "</div>"
        "<div id='previewResultsBox' style='display:none;margin-top:14px'>"
        "<div id='previewSummary' class='help' style='margin-bottom:8px'></div>"
        "<div id='previewTimeline' class='queue-list' style='max-height:360px'></div>"
        "</div></div>"
        "</section>\n");
}

void render_page(int fd, const struct request *req, const char *message) {
    if (req && req->is_ajax) {
        int ok = 1;
        if (message && (
            strcasestr(message, "failed") ||
            strcasestr(message, "required") ||
            strcasestr(message, "invalid") ||
            strcasestr(message, "error") ||
            strcasestr(message, "too large") ||
            strcasestr(message, "unknown") ||
            strcasestr(message, "out of memory")
        )) {
            ok = 0;
        }
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", ok);
        cJSON_AddStringToObject(resp, "message", message ? message : (ok ? "ok" : "error"));
        send_cjson_resp(fd, ok ? "200 OK" : "400 Bad Request", resp);
        cJSON_Delete(resp);
        return;
    }
    struct mqtt_config mqtt;
    struct wifi_config wifi;
    struct ethernet_config eth;
    struct network_status net_st;
    char *body = NULL;
    size_t body_len = 0;
    char hdr[200];
    /* Buffer the page so we can send a Content-Length. If a render is ever cut
     * short (e.g. an out-of-memory kill), no headers are emitted and the client
     * sees a reset/short read instead of silently rendering a half page. */
    FILE *f = open_memstream(&body, &body_len);
    if (!f) return;
    load_mqtt(&mqtt);
    load_wifi(&wifi);
    load_ethernet(&eth);
    get_network_status(&net_st);
    page_head(f, "Harmony Hub Control");
    if (message && message[0]) {
        fprintf(f, "<div class='msg'>");
        html(f, message);
        fprintf(f, "</div>");
    }
    status_panel(f, &mqtt);
    activities_panel(f);
    fprintf(f, "<section id='view-mqtt' data-view='mqtt' class='section'><div class='section-head'><div><h2>MQTT</h2><div class='section-lead'>Connect the hub to Home Assistant through MQTT. The hub can publish its state and listen for activity or IR commands.</div></div></div><div class='grid'>");
    mqtt_form(f, &mqtt);
    mqtt_ha_integration_panel(f, &mqtt);
    fprintf(f, "</div></section>");
    network_panel(f, &wifi, &eth, &net_st);
    ir_control_panel(f);
    ir_panel(f);
    ir_lab_panel(f);
    bluetooth_panel(f);
    elite_rf_panel(f);
    remotes_panel(f);
    backup_panel(f);
    system_panel(f);
    page_end(f);
    if (fclose(f) != 0 || !body) { free(body); return; }
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-store\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
        (unsigned long)body_len);
    send_all(fd, hdr, strlen(hdr));
    send_all(fd, body, body_len);
    free(body);
}

