#pragma once

// Wi-Fi + ArduinoOTA: grava firmware novo pela rede (pio run -e esp32dev_ota -t upload).
// Não bloqueia o boot: sem Wi-Fi, USB e BLE seguem funcionando normalmente.
void ota_link_begin();

// Chamar a cada loop(). Durante uma gravação não retorna até ela acabar
// (a tela é redesenhada pelos callbacks de progresso).
void ota_link_poll();
