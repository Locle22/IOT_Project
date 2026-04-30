/**
 * ESP32 Smart Hub - Dashboard Logic
 */


// --- Global State ---
let fanState = 'OFF';
let fanSpeed = 255;
let backlightState = true;
let ws = null;


// WEBSOCKET & NETWORK
function connectWS() {
    ws = new WebSocket('ws://' + location.host + '/ws');
    ws.onopen = () => console.log('[WS] Connected to ESP32');
    ws.onclose = () => { 
        console.log('[WS] Disconnected, retrying in 3s...'); 
        setTimeout(connectWS, 3000); 
    };
    ws.onerror = e => console.error('[WS] Error', e);
}

connectWS();

function Send_Data(data) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(data);
        console.log("📤 Gửi WS:", data);
    } else {
        alert("⚠️ Mất kết nối WebSocket tới thiết bị!");
    }
}


// NAVIGATION & MODALS
function switchView(viewName) {
    const dashboardView = document.getElementById('dashboard-view');
    const logView = document.getElementById('log-view');
    const navDashboard = document.getElementById('nav-dashboard');
    const navLog = document.getElementById('nav-log');

    if (viewName === 'dashboard') {
        dashboardView.classList.remove('hidden');
        logView.classList.add('hidden');
        
        navDashboard.className = 'text-primary border-b-2 border-primary pb-1 font-h3 font-semibold cursor-pointer transition-colors';
        navLog.className = 'text-slate-500 hover:text-primary transition-colors font-h3 font-medium cursor-pointer';
    } else if (viewName === 'log') {
        dashboardView.classList.add('hidden');
        logView.classList.remove('hidden');
        
        navLog.className = 'text-primary border-b-2 border-primary pb-1 font-h3 font-semibold cursor-pointer transition-colors';
        navDashboard.className = 'text-slate-500 hover:text-primary transition-colors font-h3 font-medium cursor-pointer';
        
        fetchPredictLogs();
    }
}

function toggleModal(id) {
    const modal = document.getElementById(id);
    if (modal.classList.contains('hidden')) {
        modal.classList.remove('hidden');
        setTimeout(() => modal.classList.add('active'), 10);
    } else {
        modal.classList.remove('active');
        setTimeout(() => modal.classList.add('hidden'), 300);
    }
}


// REAL-TIME CLOCK
function updateClock() {
    const now = new Date();
    
    const hh = String(now.getHours()).padStart(2, '0');
    const mm = String(now.getMinutes()).padStart(2, '0');
    const ss = String(now.getSeconds()).padStart(2, '0');
    document.getElementById('time-display').innerText = `${hh}:${mm}:${ss}`;
    
    const days = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
    const dd = String(now.getDate()).padStart(2, '0');
    const mo = String(now.getMonth() + 1).padStart(2, '0');
    const yyyy = now.getFullYear();
    document.getElementById('date-display').innerText = `${days[now.getDay()]}, ${dd}/${mo}/${yyyy}`;
}
setInterval(updateClock, 1000);
updateClock();


// DATA POLLING
function fetchSensorData() {
    fetch('/sensor')
        .then(res => res.json())
        .then(data => updateEnvironmentUI(data.temp, data.hum))
        .catch(err => console.error("Sensor fetch error:", err));
        // .catch(() => {
        //     updateEnvironmentUI((28 + Math.random() * 2).toFixed(1), (60 + Math.random() * 5).toFixed(1));
        // });
}

function updateEnvironmentUI(temp, hum) {
    document.getElementById('temp-val').innerText = temp;
    document.getElementById('hum-val').innerText = hum;

    const tempRatio = Math.min(1, Math.max(0, temp / 50)); 
    const humRatio = Math.min(1, Math.max(0, hum / 100)); 
    
    document.getElementById('temp-ring').style.strokeDashoffset = 440 * (1 - tempRatio);
    document.getElementById('hum-ring').style.strokeDashoffset = 440 * (1 - humRatio);
}

function fetchInference() {
    fetch('/api/tinyml')
        .then(res => res.json())
        .then(data => updateInferenceUI(data.class, data.confidence))
        .catch(err => console.error("Inference fetch error:", err));
        // .catch(() => {
        //     const randClass = Math.random() > 0.85 ? (Math.random() > 0.5 ? 1 : 2) : 0;
        //     const randConf = 0.85 + Math.random() * 0.14;
        //     updateInferenceUI(randClass, randConf);
        // });
}

function updateInferenceUI(predClass, conf) {
    // Styling matching your request: background/border + text color
    const config = [
        { name: "Background", color: "text-slate-600", bg: "bg-slate-50 border-slate-200", bar: "bg-slate-400", icon: "lens_blur" },
        { name: "Fire Detected", color: "text-red-600", bg: "bg-red-50 border-red-200", bar: "bg-red-500", icon: "local_fire_department" },
        { name: "Nuisance", color: "text-amber-600", bg: "bg-amber-50 border-amber-200", bar: "bg-amber-500", icon: "warning" }
    ];
    
    const ui = config[predClass];
    const confPercent = (conf * 100).toFixed(1);

    document.getElementById('ml-state').innerText = ui.name;
    document.getElementById('ml-state').className = `font-h1 text-3xl font-bold tracking-tight mb-8 transition-colors ${ui.color}`;
    
    document.getElementById('ml-status-bg').className = `flex-1 rounded-xl p-6 flex flex-col items-center justify-center text-center transition-colors duration-500 border shadow-inner ${ui.bg}`;
    
    document.getElementById('ml-icon').innerText = ui.icon;
    document.getElementById('ml-icon').className = `material-symbols-outlined text-5xl mb-4 transition-all ${ui.color}`;
    
    document.getElementById('ml-confidence-bar').style.width = `${confPercent}%`;
    document.getElementById('ml-confidence-bar').className = `h-full transition-all duration-700 ease-out ${ui.bar}`;
    document.getElementById('ml-confidence-text').innerText = `${confPercent}%`;
}

function fetchSysInfo() {
    fetch('/api/sysinfo')
        .then(res => res.json())
        .then(data => { 
            document.getElementById('heap-free').innerText = `${Math.floor(data.heap_free / 1024)} KB`; 
        })
        .catch(err => console.error("SysInfo error:", err));
        // .catch(() => {
        //     document.getElementById('heap-free').innerText = `${Math.floor(180 + Math.random() * 10)} KB`;
        // });
}

function fetchPredictLogs() {
    const tbody = document.getElementById('log-table-body');
    tbody.innerHTML = `<tr><td colspan="5" class="px-5 py-10 text-center text-sm font-medium text-gray-400">Loading data...</td></tr>`;
    
    fetch('/api/logs')
        .then(res => res.json())
        .then(data => renderLogs(data, tbody))
        .catch(err => {
            console.error("Logs fetch error:", err);
            tbody.innerHTML = `<tr><td colspan="5" class="px-5 py-10 text-center text-sm text-red-400">Failed to load logs</td></tr>`;
        });
        // .catch(() => {
        //     const mockLogs = Array.from({length: 20}, (_, i) => {
        //         const d = new Date(Date.now() - i * 2000);
        //         return {
        //             time: `${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}:${String(d.getSeconds()).padStart(2,'0')}`,
        //             temp: (28 + Math.random()*5).toFixed(1),
        //             hum: (60 + Math.random()*10).toFixed(1),
        //             class: Math.random() > 0.8 ? (Math.random() > 0.5 ? 1 : 2) : 0,
        //             conf: (0.85 + Math.random()*0.14)
        //         };
        //     });
        //     renderLogs(mockLogs, tbody);
        // });
}

function renderLogs(logs, tbody) {
    tbody.innerHTML = '';
    logs.forEach(log => {
        let badge = '';
        if(log.class === 0) badge = '<span class="px-2.5 py-1 rounded-md bg-slate-100 text-slate-600 text-xs font-bold uppercase">Background</span>';
        if(log.class === 1) badge = '<span class="px-2.5 py-1 rounded-md bg-red-100 text-red-700 text-xs font-bold uppercase flex items-center gap-1 w-max"><span class="material-symbols-outlined text-[14px]">local_fire_department</span> Fire</span>';
        if(log.class === 2) badge = '<span class="px-2.5 py-1 rounded-md bg-amber-100 text-amber-700 text-xs font-bold uppercase flex items-center gap-1 w-max"><span class="material-symbols-outlined text-[14px]">warning</span> Nuisance</span>';

        const confPct = (log.conf * 100).toFixed(1);
        
        tbody.innerHTML += `
            <tr class="hover:bg-slate-50 transition-colors">
                <td class="px-5 py-4 font-code text-xs text-slate-600">${log.time}</td>
                <td class="px-5 py-4 text-sm text-slate-800 font-medium">${log.temp}</td>
                <td class="px-5 py-4 text-sm text-slate-800 font-medium">${log.hum}</td>
                <td class="px-5 py-4">${badge}</td>
                <td class="px-5 py-4">
                    <div class="flex items-center gap-2">
                        <span class="text-xs text-slate-600 font-medium w-10">${confPct}%</span>
                        <div class="w-16 h-1.5 bg-slate-200 rounded-full overflow-hidden"><div class="h-full bg-slate-400" style="width: ${confPct}%"></div></div>
                    </div>
                </td>
            </tr>
        `;
    });
}



// CONTROLS
function setFanState(mode) {
    const btns = {
        'OFF': document.getElementById('fan-off'),
        'ON': document.getElementById('fan-on'),
        'AUTO': document.getElementById('fan-auto')
    };

    Object.keys(btns).forEach(key => {
        if(key === mode) {
            btns[key].className = `flex-1 py-2 font-h3 text-sm font-semibold rounded-lg transition-all bg-white shadow-sm text-slate-900 border border-slate-200`;
        } else {
            btns[key].className = `flex-1 py-2 font-h3 text-sm font-semibold rounded-lg transition-all text-slate-500 hover:text-slate-900 border border-transparent bg-transparent`;
        }
    });

    fetch(`/action?dev=fan&state=${mode}`).catch(console.error);
}

function updateFanSpeed(val) {
    document.getElementById('speed-display').innerText = val;
}

function setFanSpeed(val) {
    updateFanSpeed(val);
    fetch(`/action?dev=fan&state=SPEED&value=${val}`).catch(console.error);
}

function toggleBacklight() {
    backlightState = !backlightState;
    const btn = document.getElementById('backlight-toggle');
    const thumb = btn.querySelector('div');
    
    if(backlightState) {
        btn.classList.replace('bg-slate-300', 'bg-blue-600');
        thumb.classList.add('translate-x-full');
    } else {
        btn.classList.replace('bg-blue-600', 'bg-slate-300');
        thumb.classList.remove('translate-x-full');
    }
    
    fetch(`/action?dev=lcd&state=${backlightState ? 'ON' : 'OFF'}`).catch(console.error);
}

// Init
setFanState('OFF');
document.getElementById('backlight-toggle').classList.replace('bg-slate-300', 'bg-blue-600');
document.getElementById('backlight-toggle').querySelector('div').classList.add('translate-x-full');

setInterval(fetchSensorData, 2000);
setInterval(fetchInference, 2000);
setInterval(fetchSysInfo, 2000);
fetchSensorData();
fetchInference();
fetchSysInfo();

document.getElementById("settingsForm").addEventListener("submit", function (e) {
    e.preventDefault();

    const ssid = document.getElementById("wifi-ssid").value.trim();
    const password = document.getElementById("wifi-password").value.trim();
    const token = document.getElementById("coreiot-token").value.trim();
    const server = document.getElementById("server-ip").value.trim();
    const port = document.getElementById("server-port").value.trim();

    const settingsJSON = JSON.stringify({
        page: "setting",
        value: {
            ssid: ssid,
            password: password,
            token: token,
            server: server,
            port: port
        }
    });

    Send_Data(settingsJSON);
    alert("Cấu hình đã được gửi đến thiết bị!");
});