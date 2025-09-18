(() => {
  // ---------- Tema persistente ----------
  const applyTheme = (dark) => {
    document.documentElement.classList.toggle('dark', dark);
    document.documentElement.style.colorScheme = dark ? 'dark' : 'light';
    const toggle = document.getElementById('themeToggle');
    if (toggle) toggle.checked = dark;
    localStorage.setItem('themeDark', dark ? '1' : '0');
  };
  document.addEventListener('DOMContentLoaded', () => {
    applyTheme(localStorage.getItem('themeDark') === '1');
    const tgl = document.getElementById('themeToggle');
    if (tgl) tgl.addEventListener('change', e => applyTheme(e.target.checked));
  });

  // ---------- Utilidades ----------
  const pad = n => String(n).padStart(2, '0');
  const toDateKey = (d) => {
    const dt = (d instanceof Date) ? d : new Date(d);
    return `${dt.getFullYear()}-${pad(dt.getMonth()+1)}-${pad(dt.getDate())}`;
  };
  const nowTimeLabel = (d) => `${pad(d.getHours())}:${pad(d.getMinutes())}`;

  // Guardado por “calendario” en localStorage: key = dht22:YYYY-MM-DD
  const LS_PREFIX = 'dht22:';
  const loadDay = (key) => {
    try { return JSON.parse(localStorage.getItem(LS_PREFIX + key) || '[]'); } catch { return []; }
  };
  const saveDay = (key, arr) => {
    // límite razonable por día
    const trimmed = arr.slice(-1440); // máx 1440 muestras
    localStorage.setItem(LS_PREFIX + key, JSON.stringify(trimmed));
  };

  // Si no hay datos para ese día, generar sintético determinista
  const synthDay = (key) => {
    // LCG simple a partir de hash del key
    let h = 2166136261;
    for (let i = 0; i < key.length; i++) { h ^= key.charCodeAt(i); h = Math.imul(h, 16777619); }
    let seed = h >>> 0;
    const rnd = () => (seed = (1664525 * seed + 1013904223) >>> 0) / 2**32;

    const out = [];
    // puntos cada 10 min
    const baseT = 23 + (rnd()*2-1); // 22..24
    const baseH = 48 + (rnd()*4-2); // 46..50
    for (let i = 0; i < 24*6; i++) {
      const hour = Math.floor(i/6);
      const minute = (i%6)*10;
      const tCirc = Math.sin((hour+minute/60) / 24 * Math.PI*2) * 3; // ciclo diario ±3°C
      const hCirc = Math.cos((hour+minute/60) / 24 * Math.PI*2) * 5; // ciclo diario ±5%
      const t = +(baseT + tCirc + (rnd()*0.6-0.3)).toFixed(1);
      const hmd = +(baseH + hCirc + (rnd()*1.0-0.5)).toFixed(1);
      out.push({ time: `${pad(hour)}:${pad(minute)}`, temp: t, hum: hmd });
    }
    return out;
  };

  // ---------- Actualización en vivo ----------
  const tempEl = document.getElementById('tempVal');
  const humEl  = document.getElementById('humVal');

  async function pull() {
    try {
      const r = await fetch('/api/data', { cache: 'no-store' });
      if (!r.ok) throw new Error(r.status);
      const { temp, hum, ts } = await r.json();
      if (tempEl) tempEl.textContent = `${temp.toFixed(1)} °C`;
      if (humEl)  humEl.textContent  = `${hum.toFixed(1)} %`;

      // guardar en calendario
      const t = ts ? new Date(ts) : new Date();
      const key = toDateKey(t);
      const arr = loadDay(key);
      arr.push({ time: nowTimeLabel(t), temp: +temp.toFixed(1), hum: +hum.toFixed(1) });
      saveDay(key, arr);
    } catch(e){ /* silencioso */ }
    async function fetchData() {
  const response = await fetch('/api/data');
  const data = await response.json();

  // Mostrar los valores en la consola del navegador
  console.log('Datos recibidos:', data);

  document.getElementById('tempValue').textContent = data.temp + " °C";
  document.getElementById('humValue').textContent = data.hum + " %";
}

setInterval(fetchData, 2000);
  }
  document.addEventListener('DOMContentLoaded', () => {
    pull();
    setInterval(pull, 3000);
  });

  // ---------- Popup de gráficos ----------
  let chart;
  const renderChart = (container, items) => {
    const ctx = container.getContext('2d');
    if (chart) { chart.destroy(); }
    chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: items.map(x => x.time),
        datasets: [
          { label: 'Temperatura (°C)', data: items.map(x => x.temp), tension: 0.25 },
          { label: 'Humedad (%)', data: items.map(x => x.hum), tension: 0.25 }
        ]
      },
      options: { responsive: true, maintainAspectRatio: false }
    });
  };

  const chartBtn = document.getElementById('chartBtn');
  if (chartBtn) chartBtn.addEventListener('click', () => {
    const todayKey = toDateKey(new Date());
    Swal.fire({
      title: 'Histórico por fecha',
      width: Math.min(window.innerWidth - 24, 900),
      html: `
        <div style="display:flex;gap:8px;align-items:center;justify-content:flex-start;margin-bottom:8px">
          <label for="datePick" style="font-weight:600;min-width:90px">Fecha:</label>
          <input id="datePick" type="date" style="padding:6px;border-radius:8px;border:1px solid #9aa4b2" />
          <button id="loadDayBtn" class="sw-btn">Mostrar</button>
        </div>
        <div style="position:relative;height:360px"><canvas id="histChart"></canvas></div>
      `,
      didOpen: () => {
        const input = document.getElementById('datePick');
        const btn = document.getElementById('loadDayBtn');
        const c = document.getElementById('histChart');
        // default hoy
        const [y,m,d] = todayKey.split('-');
        input.value = `${y}-${m}-${d}`;
        const load = () => {
          const key = input.value || todayKey;
          let items = loadDay(key);
          if (!items.length) items = synthDay(key);
          renderChart(c, items);
        };
        btn.addEventListener('click', load);
        load();
      },
      showConfirmButton: false,
      didRender: () => {
        // estilo botón
        const style = document.createElement('style');
        style.textContent = `.sw-btn{padding:8px 12px;border-radius:10px;border:0;background:#2563eb;color:#fff;cursor:pointer}`;
        document.head.appendChild(style);
      }
    });
  });
})();
