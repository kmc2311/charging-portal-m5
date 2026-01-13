const hostInput = document.getElementById("host");
const btnConnect = document.getElementById("btnConnect");
const btnDisconnect = document.getElementById("btnDisconnect");

const title = document.getElementById("title");
const sub = document.getElementById("sub");
const sigilBox = document.getElementById("sigilBox");
const barFill = document.getElementById("barFill");
const log = document.getElementById("log");
const badge = document.getElementById("connBadge");

function colorFromProgress(p255) {
  const p = Math.max(0, Math.min(255, Number(p255) || 0));
  const g = p;
  const r = 255 - p;
  return `rgb(${r}, ${g}, 40)`;
}

function setState(state, progress) {
  sigilBox.classList.remove("blink");

  if (state === "IDLE") {
    title.textContent = "待機中";
    sub.textContent = "スマホを置くと「充電中!!」になります";
    sigilBox.style.opacity = "0";
    sigilBox.style.filter = "none";
    barFill.style.width = "0%";
    return;
  }

  if (state === "CHARGING") {
    title.textContent = "充電中!!";
    sub.textContent = "充電が進むほど色が変化します";
    sigilBox.style.opacity = "1";
    sigilBox.style.filter = "drop-shadow(var(--glow))";
    sigilBox.style.color = colorFromProgress(progress);
    barFill.style.width = `${Math.round((progress / 255) * 100)}%`;
    return;
  }

  if (state === "COMPLETE") {
    title.textContent = "充電完了！";
    sub.textContent = "完了の合図です";
    sigilBox.style.opacity = "1";
    sigilBox.style.filter = "drop-shadow(var(--glow))";
    sigilBox.style.color = "rgb(30,255,90)";
    barFill.style.width = "100%";
    sigilBox.classList.add("blink");
  }
}

setState("IDLE", 0);

let ws = null;

function setConnectedUI(connected) {
  badge.textContent = connected ? "接続中" : "未接続";
  badge.style.color = connected ? "rgb(120,255,170)" : "var(--muted)";
  btnDisconnect.disabled = !connected;
  btnConnect.disabled = connected;
}

btnConnect.addEventListener("click", () => {
  const host = (hostInput.value || "").trim();
  if (!host) {
    alert("host（例: 192.168.0.25）を入力してください");
    return;
  }

  // M5Stack側で WebSocket をポート81で動かす想定
  const url = `ws://${host}:81/`;
  log.textContent = `接続中: ${url}`;

  ws = new WebSocket(url);

  ws.onopen = () => {
    setConnectedUI(true);
    log.textContent = `接続しました: ${url}`;
  };

  ws.onclose = () => {
    setConnectedUI(false);
    log.textContent = "切断しました";
    ws = null;
  };

  ws.onerror = () => {
    setConnectedUI(false);
    log.textContent = "接続エラー：host/同一Wi-Fi/ポートを確認してください";
  };

  ws.onmessage = (ev) => {
    // 期待： {"state":"CHARGING","progress":123}
    try {
      const obj = JSON.parse(ev.data);
      setState(obj.state || "IDLE", Number(obj.progress ?? 0));
      log.textContent = `受信: ${ev.data}`;
    } catch {
      // 無視
    }
  };
});

btnDisconnect.addEventListener("click", () => {
  if (ws) ws.close();
});
