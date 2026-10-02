#!/bin/sh
# Para o agente e remove o LaunchAgent e o app lançador
# (o log em ~/Library/Logs/mac-monitor fica).
set -eu

LABEL="local.mac-monitor.agent"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"

launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
rm -f "$PLIST"
rm -rf "$HOME/Library/Application Support/mac-monitor/MacMonitorAgent.app"
echo "Removido: $PLIST"
