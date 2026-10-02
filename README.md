# Mac Monitor

Uma telinha ao lado do teclado mostrando o que o Mac está fazendo, sem abrir o Monitor de Atividade e sem ocupar espaço na tela principal.

Um agente em Python roda no Mac, lê as métricas do sistema e manda uma linha JSON por segundo para um ESP32 com display de 2,8". A placa desenha tudo com LVGL. A conexão pode ser por cabo USB ou por Bluetooth LE, e o firmware se atualiza pelo Wi-Fi.

<p align="center">
  <img src="docs/screens/1%20%C2%B7%20Vis%C3%A3o%20geral%401x.png" alt="Aba Geral" width="320">
</p>

## O que ele mostra

São cinco abas, trocadas tocando na barra inferior ou deslizando o dedo:

| | |
| --- | --- |
| <img src="docs/screens/2%20%C2%B7%20Hist%C3%B3rico%401x.png" alt="Aba Histórico" width="320"> | <img src="docs/screens/3%20%C2%B7%20N%C3%BAcleos%401x.png" alt="Aba Núcleos" width="320"> |
| <img src="docs/screens/4%20%C2%B7%20Rede%401x.png" alt="Aba Rede" width="320"> | <img src="docs/screens/5%20%C2%B7%20Claude%20Code%401x.png" alt="Aba Claude" width="320"> |

- **Geral**: CPU, memória, GPU e disco em arcos, mais a taxa de rede e de leitura/escrita em disco.
- **Histórico**: CPU e GPU ao longo do tempo.
- **Núcleos**: uma barra por núcleo da CPU (até 20).
- **Rede**: download e upload em tempo real, com gráfico.
- **Claude**: quanto do bloco de 5 h do Claude Code você já usou, quando ele reinicia, tokens por hora no dia, ritmo atual e o uso da semana.

E alguns detalhes que fazem diferença no dia a dia:

- A barra de status mostra se os dados estão chegando, a pressão de memória e o uso de swap.
- O brilho se ajusta à luz do ambiente (ou fica fixo em 100/60/30%, tocando na barra de status) e cai para quase zero depois de 30 s sem dados.
- O LED da placa fica verde no normal, amarelo com pressão de memória e vermelho com pressão crítica ou CPU acima de 90% por 10 s.
- A placa lembra a última aba e o modo de brilho depois de reiniciar.
- Se o Mac para de mandar dados, aparece "Aguardando o Mac..." e a placa volta sozinha quando a conexão retorna.

## Pré-requisitos

**Hardware**
- Placa **ESP32-2432S028R**, a "Cheap Yellow Display" (CYD): ESP32, tela ILI9341 de 320x240 e toque resistivo. É barata e fácil de achar.
- Cabo USB **de dados** (alguns cabos só carregam).

**No Mac**
- macOS em Apple Silicon.
- Python 3.9 ou mais novo.
- [PlatformIO](https://platformio.org/) (o CLI `pio` ou a extensão do VS Code), para compilar e gravar o firmware.
- [Claude Code](https://claude.com/claude-code), só se quiser a aba Claude.

## Setup

### 1. Gravar o firmware

Copie o arquivo de segredos e preencha com o seu Wi-Fi e uma senha para o OTA. Ele fica fora do git.

```bash
cd firmware
cp secrets.ini.example secrets.ini
```

```ini
[secrets]
wifi_ssid = MinhaRede
wifi_pass = senha-do-wifi
ota_password = troque-esta-senha
```

Conecte a placa no USB e grave:

```bash
pio run -t upload        # compila e grava
pio device monitor       # acompanha o log da placa (opcional)
```

Se a gravação falhar, baixe o `upload_speed` no `platformio.ini` de 460800 para 115200.

O Wi-Fi serve só para atualizar o firmware. Sem ele a placa funciona normalmente.

### 2. Instalar o agente

```bash
cd agent
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Teste rodando direto no terminal:

```bash
python agent.py          # pelo USB
python agent.py --ble    # pelo Bluetooth
```

Pelo USB o agente acha a placa sozinho. Pelo Bluetooth ele procura um dispositivo chamado "Mac Monitor", e na primeira vez o macOS pede permissão de Bluetooth. Nos dois casos ele tenta de novo a cada 2 s até conectar e reconecta se a conexão cair.

### 3. Deixar rodando sempre

Para o agente iniciar no login e voltar sozinho se cair, instale como serviço (LaunchAgent):

```bash
agent/launchd/install.sh          # pelo USB
agent/launchd/install.sh --ble    # pelo Bluetooth
```

Rode de novo para trocar entre USB e Bluetooth. Comandos úteis:

```bash
tail -f ~/Library/Logs/mac-monitor/agent.log                     # log
launchctl kickstart -k gui/$(id -u)/local.mac-monitor.agent      # reiniciar depois de mudar o código
launchctl bootout gui/$(id -u)/local.mac-monitor.agent           # parar
agent/launchd/uninstall.sh                                       # remover
```

No modo Bluetooth, o macOS pede a permissão para "Mac Monitor Agent", um pequeno app que o instalador cria para isso.

### 4. Aba Claude (opcional)

A aba funciona sem configurar nada: o agente lê os registros do Claude Code em `~/.claude/projects` e o percentual de uso da sua conta. Se esse percentual não estiver disponível, a aba mostra o bloco como "estimado" e não inventa um número.

Para ter também o percentual oficial do status line, adicione em `~/.claude/settings.json`:

```json
"statusLine": {
  "type": "command",
  "command": "python3 /caminho/para/mac-monitor/agent/claude_statusline.py"
}
```

Se você já usa um status line, mantenha o seu passando o comando depois de `--`:
`python3 .../claude_statusline.py -- ~/meu-statusline.sh`.

Os ajustes ficam em `~/.config/mac-monitor/config.toml`, seção `[claude]` (veja os padrões em `agent/config.py`). Por exemplo, para desligar a aba: `enabled = false`.

O token de acesso do Claude Code é lido do Keychain só na hora da consulta. Ele nunca é gravado, renovado, registrado no log ou enviado para a placa.

## Atualizar o firmware pelo Wi-Fi

Depois da primeira gravação pelo USB, a placa aceita firmware novo pela rede em `macmonitor.local`:

```bash
cd firmware
pio run -e esp32dev_ota -t upload
```

A tela mostra o progresso e a placa reinicia sozinha no fim.

## Testar sem carga real

O `fake.py` manda dados inventados para conferir a tela. Pare o agente antes (só um processo usa a conexão por vez):

```bash
python fake.py            # valores variando, para os gráficos terem forma
python fake.py alta       # carga alta fixa
python fake.py nucleos    # alterna entre 4, 10, 14 e 20 núcleos
python fake.py claude     # aba Claude (também: estimado, ocioso, semclaude)
python fake.py --ble      # qualquer cenário também vai pelo Bluetooth
```

## Como funciona

```
agent/      Agente Python que roda no Mac
firmware/   Firmware do ESP32 (PlatformIO + Arduino + LVGL + NimBLE)
docs/       Requisitos da interface e telas de referência
```

O agente manda uma linha JSON por segundo, com nomes de campo curtos porque o link serial é lento:

```json
{"v": 1, "cpu": 37.5, "cores": [62, 40, 18, 9], "mem": 71.2, "swap": 0.8, "press": 1,
 "temp": 0, "gpu": 18, "rx": 125000, "tx": 32000, "disk": 64.1, "rd": 0, "wr": 52000}
```

| Campo | Significado |
| --- | --- |
| `v` | Versão do protocolo |
| `cpu`, `cores` | Uso da CPU total e por núcleo (%) |
| `mem`, `swap` | Uso de memória e de swap (%) |
| `press` | Pressão de memória do macOS |
| `temp` | Estado térmico do macOS, de 0 (normal) a 3 (crítico). Não é °C: sem root, o Apple Silicon não expõe temperatura |
| `gpu` | Uso da GPU (%) |
| `rx`, `tx` | Rede: download e upload (bytes/s) |
| `disk` | Espaço usado no disco (%) |
| `rd`, `wr` | Disco: leitura e escrita (bytes/s) |

Os dados do Claude vão numa linha separada, com `"t": "cc"`, a cada 10 s (formato em [docs/claude/claude-code-dados-agente.md](docs/claude/claude-code-dados-agente.md)).

Para mudar o protocolo, altere juntos `agent/metrics.py`, `firmware/src/line_reader.cpp` e `firmware/src/metrics.h`.
