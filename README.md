# Mac Monitor

Monitor de hardware do Mac numa telinha à parte. Um agente em Python roda no Mac, lê as métricas do sistema e manda uma linha JSON por segundo para um ESP32 com display TFT, que desenha tudo com LVGL. A conexão pode ser por cabo USB ou por Bluetooth LE, e o firmware pode ser atualizado pela rede Wi-Fi (OTA).

![Tela Geral](docs/fase5/1%20%C2%B7%20Vis%C3%A3o%20geral%401x.png)

## O que aparece na tela

São quatro abas, trocadas pelo toque na barra inferior ou deslizando o dedo:

| Aba | Conteúdo |
| --- | --- |
| **Geral** | CPU, memória, GPU e disco em arcos, mais rede (download/upload) e disco (leitura/escrita) |
| **Histórico** | Gráfico de CPU e GPU ao longo do tempo |
| **Núcleos** | Uma barra por núcleo da CPU (até 20) |
| **Rede** | Download e upload em tempo real, com gráfico |

A barra de status mostra se a placa está recebendo dados (ao vivo), a pressão de memória e o uso de swap. Se o Mac para de mandar dados por 3 s, a tela mostra "Aguardando o Mac...".

## Hardware

- **ESP32-2432S028R** ("Cheap Yellow Display", CYD): ESP32 sem PSRAM, tela ILI9341 de 320x240 e toque resistivo XPT2046
- Cabo USB (o conversor USB-serial da placa é um CH340)
- Um Mac com Apple Silicon (o agente usa APIs do macOS)

## Estrutura

```
agent/      Agente Python que roda no Mac
firmware/   Firmware do ESP32 (PlatformIO + Arduino + LVGL)
docs/       Requisitos e telas de referência da interface
```

## Firmware

Precisa do [PlatformIO](https://platformio.org/) (CLI `pio` ou a extensão do VS Code).

### Configuração

Copie o arquivo de exemplo e preencha com a sua rede Wi-Fi e uma senha para o OTA:

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

O `secrets.ini` fica fora do git.

### Gravar pelo USB

```bash
pio run -t upload        # compila e grava
pio device monitor       # acompanha o log da placa
```

Se a gravação falhar, baixe o `upload_speed` no `platformio.ini` de 460800 para 115200.

### Gravar pela rede (OTA)

Depois da primeira gravação pelo USB, a placa entra no Wi-Fi e aceita firmware novo pela rede no endereço `macmonitor.local`:

```bash
pio run -e esp32dev_ota -t upload
```

Durante a atualização a tela mostra uma barra de progresso, e a placa reinicia sozinha no fim. Sem Wi-Fi, a placa continua funcionando normalmente pelo USB ou Bluetooth.

## Agente (Mac)

```bash
cd agent
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

```bash
python agent.py          # envia pelo USB
python agent.py --ble    # envia pelo Bluetooth LE
```

No modo USB o agente acha a placa sozinho pelo chip CH340. No modo Bluetooth ele procura um dispositivo chamado "Mac Monitor". Na primeira vez o macOS pede permissão de Bluetooth para o app do terminal. Nos dois modos o agente tenta de novo a cada 2 s até conectar e reconecta se a conexão cair.

### Dados de teste

Para testar a tela sem carga real no Mac, pare o `agent.py` (só um processo usa a conexão por vez) e rode:

```bash
python fake.py            # valores variando, para os gráficos terem forma
python fake.py alta       # carga alta fixa
python fake.py nucleos    # alterna entre 4, 10, 14 e 20 núcleos
python fake.py --ble      # qualquer cenário também funciona pelo Bluetooth
```

## Protocolo

O agente manda uma linha JSON por segundo terminada em `\n`. Os nomes dos campos são curtos porque o link serial é lento:

```json
{"v": 1, "cpu": 37.5, "cores": [62, 40, 18, 9], "mem": 71.2, "swap": 0.8, "press": 1,
 "temp": 0, "gpu": 18, "rx": 125000, "tx": 32000, "disk": 64.1, "rd": 0, "wr": 52000}
```

| Campo | Significado |
| --- | --- |
| `v` | Versão do protocolo |
| `cpu`, `cores` | Uso da CPU total e por núcleo (%) |
| `mem`, `swap` | Uso de memória e de swap (%) |
| `press` | Pressão de memória do macOS (`kern.memorystatus_vm_pressure_level`) |
| `temp` | Estado térmico do macOS, de 0 (normal) a 3 (crítico). Não é uma temperatura em °C: o macOS não expõe temperatura sem root no Apple Silicon. Ainda não aparece na tela |
| `gpu` | Uso da GPU (%), lido do `ioreg` |
| `rx`, `tx` | Rede: download e upload (bytes/s) |
| `disk` | Espaço usado no disco (%) |
| `rd`, `wr` | Disco: leitura e escrita (bytes/s) |

Para mudar o protocolo, altere os três lugares juntos: `agent/metrics.py`, `firmware/src/line_reader.cpp` e `firmware/src/metrics.h`.
