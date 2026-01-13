# Charging Portal (Arduino x Web Serial)

Arduino UNO のシリアル(JSON 1行)を Chrome の Web Serial API で読み取り、
充電状態に応じてUI（図形/色/文字）を変化させるデモです。

## Files
- index.html
- styles.css
- app.js

## How to run (local)
- Chromeで開く（推奨：VS Code Live Server）
- Connectボタン → Arduinoポートを選択

## Arduino serial format (example)
{"state":"IDLE","progress":0}
{"state":"CHARGING","progress":120}
{"state":"COMPLETE","progress":255}
