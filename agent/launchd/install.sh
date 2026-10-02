#!/bin/sh
# Instala o agente como LaunchAgent: sobe no login e é relançado se morrer.
#
#   ./install.sh          # USB serial
#   ./install.sh --ble    # Bluetooth LE
#
# Os argumentos vão direto para o agent.py. Rodar de novo reinstala
# (útil para trocar USB/BLE); o código novo só exige `launchctl kickstart`.
#
# O agente roda através de um app mínimo (launcher.c) para o macOS poder
# conceder o Bluetooth: o python3 da Apple sozinho é sempre negado.
set -eu

LABEL="local.mac-monitor.agent"
AGENT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PYTHON="$AGENT_DIR/.venv/bin/python"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
LOG_DIR="$HOME/Library/Logs/mac-monitor"
APP="$HOME/Library/Application Support/mac-monitor/MacMonitorAgent.app"
LAUNCHER="$APP/Contents/MacOS/mac-monitor-agent"
DOMAIN="gui/$(id -u)"

if [ ! -x "$PYTHON" ]; then
    echo "venv não encontrado em $AGENT_DIR/.venv (veja o CLAUDE.md)" >&2
    exit 1
fi

# um <string> por argumento extra do agent.py
EXTRA_ARGS=""
for arg in "$@"; do
    EXTRA_ARGS="$EXTRA_ARGS
        <string>$arg</string>"
done

mkdir -p "$LOG_DIR" "$HOME/Library/LaunchAgents" "$APP/Contents/MacOS"

# Recompila só se o launcher.c mudou: assinatura ad hoc nova = o macOS pede
# a permissão de Bluetooth de novo.
if [ ! -x "$LAUNCHER" ] || [ "$AGENT_DIR/launchd/launcher.c" -nt "$LAUNCHER" ]; then
    cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>$LABEL</string>
    <key>CFBundleName</key>
    <string>Mac Monitor Agent</string>
    <key>CFBundleExecutable</key>
    <string>mac-monitor-agent</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSBackgroundOnly</key>
    <true/>
    <key>NSBluetoothAlwaysUsageDescription</key>
    <string>Envia as métricas do Mac para o monitor ESP32 por Bluetooth.</string>
</dict>
</plist>
EOF
    cc -O2 -o "$LAUNCHER" "$AGENT_DIR/launchd/launcher.c"
    codesign --force --sign - --identifier "$LABEL" "$APP"
fi

# o launchd não expande ~ nem $HOME: todos os caminhos saem absolutos daqui
cat > "$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>$LABEL</string>
    <key>ProgramArguments</key>
    <array>
        <string>$LAUNCHER</string>
        <string>$PYTHON</string>
        <string>-u</string>
        <string>$AGENT_DIR/agent.py</string>$EXTRA_ARGS
    </array>
    <key>WorkingDirectory</key>
    <string>$AGENT_DIR</string>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>ProcessType</key>
    <string>Interactive</string>
    <key>StandardOutPath</key>
    <string>$LOG_DIR/agent.log</string>
    <key>StandardErrorPath</key>
    <string>$LOG_DIR/agent.log</string>
</dict>
</plist>
EOF
plutil -lint "$PLIST" >/dev/null

# reinstalação: tira a versão carregada antes (falha se não houver, tudo bem)
launchctl bootout "$DOMAIN/$LABEL" 2>/dev/null || true
# o bootout é assíncrono; o bootstrap falha (erro 5) se o anterior ainda existir
i=0
while launchctl print "$DOMAIN/$LABEL" >/dev/null 2>&1 && [ $i -lt 50 ]; do
    sleep 0.2
    i=$((i + 1))
done
launchctl bootstrap "$DOMAIN" "$PLIST"

echo "Instalado: $PLIST"
echo "Log: $LOG_DIR/agent.log"
echo "Com --ble, permita o Bluetooth para \"Mac Monitor Agent\" se o macOS perguntar"
echo "(ou em Ajustes do Sistema > Privacidade e Segurança > Bluetooth)."
