# Fase 5 · Interface LVGL do monitor: requisitos

Este documento descreve a interface da fase 5 para implementação no firmware. A referência visual é o canvas "Monitor do Mac · Fase 5", com as artboards Main (Geral), Historico, Nucleos e Rede, todas desenhadas em 320x240 px, na escala 1:1 da tela.

O módulo serial, o parser JSON e a `struct Metrics` das fases 3 e 4 continuam valendo. Esta fase troca apenas a camada de interface (TFT_eSPI direto) por LVGL.

## 1. Contexto e escopo

| Item | Valor |
| --- | --- |
| Placa | ESP32-2432S028R (CYD), sem PSRAM |
| Tela | 320x240 em paisagem, 16 bits de cor |
| Toque | Resistivo (XPT2046), alvos com pelo menos 40 px |
| Biblioteca | `lvgl/lvgl@9.2.2`, `lv_conf.h` da mesma versão, `LV_USE_TFT_ESPI 1`, `LV_COLOR_DEPTH 16` |
| Buffer de desenho | Parcial, 1/10 da tela (cerca de 15 KB) |
| Entrada de dados | Uma linha JSON por segundo, protocolo v1 da fase 3 |

Escopo desta entrega: barra de status, barra de abas e as 4 telas (Geral, Histórico, Núcleos, Rede). O que fica de fora está na seção 10.

A interface pode ser escrita à mão ou montada no EEZ Studio. As medidas deste documento valem nos dois casos, e a lógica de atualização (seção 7) fica em C++ de qualquer forma.

Os nomes de função citados são da API do LVGL 9. Algumas funções de estilo e de animação mudaram de nome entre as versões 9.0, 9.2 e 9.3, então confirme cada nome na documentação da 9.2 antes de usar.

## 2. Estrutura geral

Todas as telas dividem a mesma moldura:

```text
y=0    +--------------------------------------------------+
       | o ao vivo              o Pressão normal · swap 0,8% |   Barra de status (22 px)
y=22   +--------------------------------------------------+
       |                                                  |
       |          Conteúdo da aba ativa (178 px)          |   Página do lv_tabview
       |                                                  |
y=200  +--------------------------------------------------+
       |  Geral   |  Histórico  |  Núcleos  |    Rede      |   Barra de abas (40 px)
y=240  +--------------------------------------------------+
```

Hierarquia de objetos:

```text
lv_screen_active()                 fundo bg
├── status_bar   lv_obj 320x22      x=0, y=0 (fora do tabview, visível em todas as telas)
└── tabview      lv_tabview 320x218 x=0, y=22, barra de abas embaixo com 40 px
    ├── aba "Geral"      → seção 6.1
    ├── aba "Histórico"  → seção 6.2
    ├── aba "Núcleos"    → seção 6.3
    └── aba "Rede"       → seção 6.4
```

A margem horizontal do conteúdo é de 10 px em todas as telas. As páginas do tabview ficam com padding 0; cada tela aplica o próprio padding descrito na seção 6.

## 3. Tokens visuais

### 3.1 Cores

Defina como constantes (`constexpr uint32_t`) e converta com `lv_color_hex()`.

| Token | Hex | Uso |
| --- | --- | --- |
| `bg` | `#0B0E13` | Fundo da tela, da barra de status e das páginas |
| `tabbar_bg` | `#0F131A` | Fundo da barra de abas |
| `tabbar_border` | `#1B212B` | Linha de 1 px no topo da barra de abas |
| `tab_active_bg` | `#1D2432` | Fundo da aba ativa |
| `card` | `#131821` | Fundo dos cards da tela Geral |
| `track` | `#1E2530` | Trilha dos arcos e das barras |
| `grid` | `#1A212B` | Linhas de grade dos gráficos |
| `baseline` | `#262E3A` | Linha de base (zero) dos gráficos |
| `text_1` | `#E9EDF3` | Valores |
| `text_active` | `#F1F4F8` | Rótulo da aba ativa |
| `text_2` | `#8D97A6` | Rótulos, status, abas inativas |
| `text_3` | `#737D8C` | Eixos dos gráficos, índices dos núcleos |
| `text_core` | `#C9D0DA` | Valores acima das barras de núcleo |
| `cpu` | `#5B9CF6` | CPU: arco, barras de núcleo, linha do histórico |
| `mem` | `#B08CF5` | Memória |
| `gpu` | `#F4A259` | GPU |
| `disk` | `#3CC8B4` | Disco |
| `net_down` | `#F28DB5` | Download |
| `net_up` | `#B05584` | Upload |
| `ok` | `#4ADE80` | "ao vivo", pressão normal, faixa baixa |
| `warn` | `#F5B83D` | Pressão em alerta, faixa média |
| `crit` | `#F25C5C` | Valor acima de 85%, pressão crítica |

### 3.2 Fontes

Todas em Montserrat, geradas no conversor de fontes do LVGL a partir dos TTF do Google Fonts.

| Token | Fonte | Uso |
| --- | --- | --- |
| `F10` | Montserrat SemiBold, 10 px | Barra de status, rótulos, eixos, índices e valores dos núcleos |
| `F12` | Montserrat SemiBold, 12 px | Abas, valores dos cards, legendas dos gráficos |
| `F20` | Montserrat Bold, 20 px | Número dentro dos arcos |
| `F9` (opcional) | Montserrat SemiBold, 9 px | Valores dos núcleos quando houver mais de 14 núcleos |

Configuração do conversor:

- bpp: 4 (bordas suavizadas)
- Faixa: `0x20-0x7E,0xA0-0xFF,0x2212`
- A faixa Latin-1 (`0xA0-0xFF`) cobre os acentos de "MEMÓRIA", "Histórico", "Núcleos" e "Pressão", e também o ponto médio "·" (`0xB7`)
- `0x2212` é o sinal de menos de "−2 min". Se preferir não incluir, use o hífen ASCII nos rótulos

No `lv_conf.h`, declare as fontes com `LV_FONT_CUSTOM_DECLARE`, aponte `LV_FONT_DEFAULT` para `F12` e desligue as Montserrat embutidas que não forem usadas, para economizar flash.

### 3.3 Regra de cor dos valores

Vale para os arcos da tela Geral e para as barras da tela Núcleos. Os gráficos usam sempre a cor da métrica.

O modo é escolhido em tempo de compilação pela constante `UI_COLOR_MODE`:

| Modo | Regra |
| --- | --- |
| `UI_COLOR_PER_METRIC` (padrão) | Cor da métrica (`cpu`, `mem`, `gpu`, `disk`); acima de 85% passa para `crit` |
| `UI_COLOR_BY_RANGE` | Até 60%: `ok`. De 61% a 85%: `warn`. Acima de 85%: `crit` (regra da fase 4) |

A comparação usa o valor já arredondado.

### 3.4 Formatação de números

O separador decimal é a vírgula e não há separador de milhar. Formate com `snprintf` em buffers fixos e troque o ponto pela vírgula.

| Dado | Regra | Exemplo |
| --- | --- | --- |
| Percentuais (`cpu`, `mem`, `gpu`, `disk`, `cores`) | Inteiro arredondado, limitado a 0 a 100 | 37,5 → "38" |
| Swap | Uma casa decimal | 0,8 → "0,8%" |
| Taxa abaixo de 1.000 B/s | `N B/s` | 0 → "0 B/s" |
| Taxa abaixo de 1.000.000 B/s | `N KB/s`, inteiro | 125000 → "125 KB/s" |
| Taxa abaixo de 10.000.000 B/s | `N,N MB/s` | 2400000 → "2,4 MB/s" |
| Taxa a partir de 10.000.000 B/s | `N MB/s`, inteiro | 18000000 → "18 MB/s" |
| Eixo Y da rede (valor em KB/s) | Abaixo de 1000: `NK`; a partir de 1000: `NM` com vírgula | 500 → "500K", 2500 → "2,5M" |

As taxas usam base 1000. Escolha a unidade depois de arredondar: 999.600 B/s vira "1,0 MB/s" em vez de "1000 KB/s".

## 4. Navegação

- Widget: `lv_tabview` com 4 abas nesta ordem: Geral, Histórico, Núcleos, Rede. A aba inicial é Geral
- Barra embaixo: `lv_tabview_set_tab_bar_position(tv, LV_DIR_BOTTOM)` e `lv_tabview_set_tab_bar_size(tv, 40)`
- A troca de aba acontece pelo toque no botão ou pelo deslize horizontal no conteúdo. Mantenha o deslize padrão do tabview ativo e a transição animada
- Barra de abas: fundo `tabbar_bg`, borda superior de 1 px em `tabbar_border`, padding 4 px e 4 px entre botões. Cada botão fica com cerca de 75x31 px e raio 8
- Área de toque: cada botão precisa responder na célula inteira (80x40). Use `lv_obj_set_ext_click_area(btn, 4)` para compensar o padding
- Aba inativa: sem fundo, texto `F12` em `text_2`
- Aba ativa (`LV_STATE_CHECKED`): fundo `tab_active_bg`, texto `F12` em `text_active`
- Aba pressionada: não foi desenhada. Sugestão: fundo `#171D28`
- As páginas não rolam: `lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE)`, padding 0, sem barra de rolagem. Assim o deslize vai direto para o tabview
- Só os botões de aba são interativos. Arcos, barras e gráficos ficam sem `LV_OBJ_FLAG_CLICKABLE`
- No `LV_EVENT_VALUE_CHANGED` do tabview, guarde o índice da aba ativa. A fase 6 vai gravar esse valor com `Preferences`

## 5. Barra de status

`lv_obj` de 320x22 px em (0, 0), com fundo `bg`, padding horizontal de 10 px e layout flex em linha com `space-between` e itens centralizados na vertical. Ela fica fora do tabview, então aparece em todas as telas.

Lado esquerdo, indicador de conexão: ponto de 6x6 px (raio circular), 5 px de espaço e label `F10` em `text_2`.

| Estado | Ponto | Texto |
| --- | --- | --- |
| Online (pacote válido nos últimos 3 s) | `ok`, pulsando: opacidade 100% → 30% → 100% em 2 s, repetição infinita via `lv_anim` | "ao vivo" |
| Offline (3 s sem pacote válido) | `text_3`, parado | "aguardando" |

Lado direito, pressão de memória e swap: ponto de 6x6 px, 5 px de espaço e label `F10` em `text_2` no formato `{pressão} · swap {swap}`.

| `press` | Ponto | Texto da pressão |
| --- | --- | --- |
| 1 | `ok` | Pressão normal |
| 2 | `warn` | Pressão em alerta |
| 4 | `crit` | Pressão crítica |
| Outro valor | `text_3` | Pressão indisponível |

Exemplo: "Pressão normal · swap 0,8%".

## 6. Telas

As coordenadas são absolutas na tela de 320x240 e aproximadas. A tolerância é de ±2 px, porque a altura de linha das fontes geradas pode variar um pouco.

### 6.1 Geral

Resumo de uma olhada: os quatro percentuais e as taxas do momento.

A área de conteúdo tem padding de 6 px em cima, 10 px nas laterais e 10 px embaixo. Dois blocos ficam centralizados na vertical, com 12 px entre eles.

**Bloco A: quatro medidores (y ≈ 33 a 101)**

Grade de 4 colunas de 72 px com 4 px de espaço. As colunas começam em x = 10, 86, 162 e 238.

Cada medidor tem:

- `lv_arc` de 62x62 px, centralizado numa célula de 64x64 no topo da coluna
  - Ângulos de fundo 135° a 45° (`lv_arc_set_bg_angles(arc, 135, 45)`): 270° com a abertura para baixo
  - Faixa 0 a 100
  - Espessura 6 px em `LV_PART_MAIN` e `LV_PART_INDICATOR`, com pontas arredondadas (`arc_rounded`)
  - `LV_PART_MAIN` na cor `track`; `LV_PART_INDICATOR` com a cor da regra 3.3
  - Sem knob (`lv_obj_remove_style(arc, NULL, LV_PART_KNOB)`) e sem `LV_OBJ_FLAG_CLICKABLE`
- Valor dentro do arco: número em `F20` / `text_1` seguido de "%" em `F10` / `text_2`, na mesma linha de base e centralizados na horizontal. O centro vertical do número coincide com o centro do arco. Use `lv_spangroup` com dois spans, ou dois labels num flex alinhados por baixo com ajuste fino
- Nome abaixo: label `F10` / `text_2` com espaçamento entre letras 1 e centralizado na coluna. O topo do label fica 8 px acima da borda inferior da célula do arco, entrando na abertura. Textos: "CPU", "MEMÓRIA", "GPU", "DISCO"
- Atualização: o arco anima do valor atual até o novo em 300 ms, com curva ease-out (`lv_anim` chamando `lv_arc_set_value`). O número troca na hora

**Bloco B: dois cards (y ≈ 113 a 185)**

Grade de 2 colunas de 146 px com 8 px de espaço; os cards começam em x = 10 e x = 164.

- Card: fundo `card`, raio 10, sem borda, padding de 9 px na vertical e 10 px na horizontal, flex em coluna com 6 px de espaço
- Título: `F10` / `text_2`, espaçamento entre letras 1
- Duas linhas por card, cada uma em flex com `space-between`: rótulo à esquerda (`F10` / `text_2`) e valor à direita (`F12` / `text_1`)

| Card | Título | Linha 1 | Linha 2 |
| --- | --- | --- | --- |
| Rede | "REDE" | ponto 6x6 `net_down` + "Download" → `rx` | ponto 6x6 `net_up` + "Upload" → `tx` |
| Disco | "DISCO E/S" | "Leitura" → `rd` | "Escrita" → `wr` |

Os pontos da rede são a legenda de cor da tela Rede. As linhas do disco não têm ponto.

**Dados:** `cpu`, `mem`, `gpu` e `disk` alimentam os arcos. `rx`, `tx`, `rd` e `wr` vão para os cards, formatados pela seção 3.4.

### 6.2 Histórico

CPU e GPU nos últimos 2 minutos.

A área de conteúdo tem padding de 6 px em cima, 10 px nas laterais e 8 px embaixo, com flex em coluna e 6 px entre os itens.

**Legenda (y 28 a 44)**

Dois itens em linha, com 14 px entre eles. Cada item tem ponto de 8x8 px, 5 px, nome em `F10` / `text_2`, 5 px e valor atual em `F12` / `text_1`.

- Item 1: ponto `cpu`, "CPU", valor de `cpu` (ex.: "38%")
- Item 2: ponto `gpu`, "GPU", valor de `gpu` (ex.: "18%")

**Gráfico (y 50 a 174)**

`lv_chart` de 270x124 px em x = 10, seguido de 6 px e de uma coluna de eixo com 24 px em x = 286.

- Tipo linha, 120 pontos (1 por segundo), modo `LV_CHART_UPDATE_MODE_SHIFT`, eixo Y de 0 a 100
- Séries: crie primeiro a da GPU (cor `gpu`) e depois a da CPU (cor `cpu`), para a CPU ficar por cima
- Linha de 2 px, sem marcadores nos pontos (`lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR)`)
- Fundo transparente, padding 0, só a borda de baixo com 1 px em `baseline`
- Grade de 1 px em `grid`: linhas horizontais em 0, 25, 50, 75 e 100% e uma vertical no meio (−1 min). `lv_chart_set_div_line_count(chart, 5, 3)` chega perto, mas também desenha as bordas verticais; se isso incomodar, use 0 divisões verticais e desenhe a linha do meio com um `lv_line`
- Na inicialização, preencha todas as séries com `LV_CHART_POINT_NONE`. A linha aparece conforme o histórico se acumula, sem zeros falsos
- Desejável: marcador no último ponto de cada série, um círculo de 6x6 px na cor da série com anel de 2 px em `bg`. Pode ser feito com objetos pequenos posicionados por `lv_chart_get_point_pos_by_id`, ou ficar para depois

**Eixos**

- Y (`F10` / `text_3`, alinhado à esquerda na coluna de eixo): "100" centralizado na linha de cima, "50" na linha do meio e "0" na linha de base
- X (y 180 a 192, `F10` / `text_3`): "−2 min" alinhado à esquerda do gráfico, "−1 min" centralizado em x = 145 e "agora" alinhado à direita do gráfico (x = 280)

**Dados:** a cada pacote, `lv_chart_set_next_value` recebe `round(cpu)` e `round(gpu)`, e a legenda é atualizada.

### 6.3 Núcleos

Carga de cada núcleo da CPU.

A área de conteúdo tem padding de 6 px em cima, 10 px nas laterais e 8 px embaixo, com flex em coluna e 10 px entre os itens.

**Cabeçalho (y 28 a 44)**

- Esquerda: "CPU total" em `F10` / `text_2`, 6 px e o valor de `cpu` em `F12` / `text_1` (ex.: "38%")
- Direita: "{N} núcleos" em `F10` / `text_2` (ex.: "10 núcleos")

**Barras (y 54 a 190)**

Container flex em linha, com 2 px de espaço e N colunas com `flex_grow` 1. Cada coluna é um flex em coluna, centralizado, com 4 px de espaço:

1. Valor: label `F10` / `text_core` com 12 px de altura (ex.: "62")
2. Barra: `lv_bar` de L x 104 px, faixa 0 a 100, raio circular em `LV_PART_MAIN` e `LV_PART_INDICATOR`, `LV_PART_MAIN` em `track` e `LV_PART_INDICATOR` com a cor da regra 3.3 (a cor da métrica aqui é `cpu`)
3. Índice: label `F10` / `text_3` com o número do núcleo, de "1" a "N"

A largura L depende de N: 14 px até 10 núcleos, 12 px até 14 e 10 px acima de 14. Com mais de 14 núcleos, o design usa 9 px nos valores (fonte `F9`); se preferir não gerar essa fonte, esconda os valores.

**Comportamento**

- N é o tamanho do array `cores`, até `MAX_CORES` = 20. O que passar disso é ignorado
- As barras são criadas no primeiro pacote e só são recriadas quando N muda: apague os filhos do container e crie de novo
- Atualização: `lv_bar_set_value(bar, v, LV_ANIM_ON)` com animação de 300 ms (`lv_obj_set_style_anim_duration`), atualizando também o label do valor e a cor do indicador

**Dados:** `cores[i]` vai para a barra i + 1; `cpu` vai para o cabeçalho.

### 6.4 Rede

Download e upload nos últimos 2 minutos, com escala automática.

O layout é idêntico ao do Histórico (mesmas coordenadas, gráfico, grade e eixo X). As diferenças são:

- Legenda: ponto `net_down` + "Download" + `rx` formatado (ex.: "125 KB/s"); ponto `net_up` + "Upload" + `tx` formatado (ex.: "32 KB/s")
- Séries: crie primeiro a de upload (`net_up`) e depois a de download (`net_down`), para o download ficar por cima. Os valores entram em KB/s inteiros: `round(rx / 1000)` e `round(tx / 1000)`
- Escala automática: a cada pacote, pegue o maior valor entre os pontos visíveis das duas séries (ignorando `LV_CHART_POINT_NONE`) e escolha o menor degrau que seja maior ou igual a ele, nesta lista em KB/s: 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000. Aplique com `lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, escala)`. A escala mínima é 50
- Como o máximo é calculado sobre toda a janela de 2 minutos, a escala sobe na hora e só desce quando o pico sai da janela. Isso já evita que ela fique pulando, sem precisar de timer extra
- Eixo Y: em cima `fmt(escala)`, no meio `fmt(escala / 2)` e na base "0", no formato K/M da seção 3.4 (ex.: "500K", "250K", "0"). Os rótulos podem passar dos 24 px da coluna e ocupar a margem direita, até cerca de 30 px

Observação: a lista de degraus aqui não tem 250 e 2500, que existiam no protótipo do canvas. Assim o rótulo do meio fica sempre curto ("125K" e "1,25M" não aparecem). O visual do protótipo (500K / 250K) continua válido.

## 7. Fluxo de atualização

```text
loop():
  lv_timer_handler() a cada ~5 ms
  ler a serial sem bloquear → ao receber '\n', converter o JSON para Metrics
  se o pacote for válido:
      ui_update(m)
      ultimoPacote = millis()
      se estava offline: ui_set_online(true)
  se millis() - ultimoPacote > 3000 e estava online:
      ui_set_online(false)
  enquanto offline, 1x por segundo: ui_tick_offline()
```

Regras:

- `ui_update` atualiza as 4 telas em todo pacote, mesmo as que não estão visíveis. Os gráficos precisam continuar acumulando histórico
- Offline: `ui_tick_offline` coloca `LV_CHART_POINT_NONE` em todas as séries uma vez por segundo, para o tempo sem dados aparecer como um vão na linha. Arcos, barras e textos ficam com o último valor
- Não aloque memória no caminho de atualização: use buffers fixos (`char buf[24]`) com `snprintf` (`lv_label_set_text` copia o texto). Todos os objetos são criados em `ui_init`, com exceção das barras de núcleo quando N muda
- Opcional: só chame `lv_label_set_text` quando o texto mudar, comparando com o anterior, para reduzir redesenho

## 8. Organização do código sugerida

```text
firmware/src/
├── main.cpp
├── serial_link.h / .cpp     leitura da serial e parser (fase 3)
├── metrics.h                struct Metrics
└── ui/
    ├── ui.h / ui.cpp        API pública
    ├── theme.h / .cpp       cores (3.1), fontes (3.2), estilos compartilhados
    ├── format.h / .cpp      percentuais e taxas (3.4)
    ├── status_bar.cpp
    ├── tab_overview.cpp     6.1 Geral
    ├── tab_history.cpp      6.2 Histórico
    ├── tab_cores.cpp        6.3 Núcleos
    ├── tab_network.cpp      6.4 Rede
    └── fonts/               arquivos .c gerados pelo conversor
```

API pública:

```cpp
void ui_init(lv_display_t* disp);
void ui_update(const Metrics& m);
void ui_set_online(bool online);
void ui_tick_offline();          // 1x por segundo enquanto offline
```

A `struct Metrics` precisa ter `cores[MAX_CORES]` (com `MAX_CORES = 20`) e um campo `coreCount` preenchido pelo parser.

Crie os estilos que se repetem (card, label F10/text_2, label F12/text_1, trilha) uma única vez como `lv_style_t` em `theme.cpp` e reaproveite em todos os objetos.

## 9. Dados de teste

Vale criar um script `agent/fake.py` que mande estas linhas pela serial, uma por segundo, para testar sem depender da carga real do Mac.

**Cenário normal (valores do protótipo):**

```json
{"v":1,"cpu":37.5,"cores":[62,40,18,9,71,58,44,36,25,17],"mem":71.2,"swap":0.8,"press":1,"gpu":18,"rx":125000,"tx":32000,"disk":64.1,"rd":0,"wr":52000}
```

Resultado esperado em `UI_COLOR_PER_METRIC`:

- Geral: arcos 38 (`cpu`), 71 (`mem`), 18 (`gpu`) e 64 (`disk`); cards com "125 KB/s", "32 KB/s", "0 B/s" e "52 KB/s"
- Barra de status: "ao vivo" e "Pressão normal · swap 0,8%" com ponto `ok`
- Núcleos: "CPU total 38%", "10 núcleos", barras de 14 px, todas em `cpu`

**Cenário de carga alta:**

```json
{"v":1,"cpu":91,"cores":[96,94,91,88,97,93,89,90,86,92],"mem":89,"swap":3.4,"press":2,"gpu":76,"rx":2400000,"tx":410000,"disk":64,"rd":18000000,"wr":6200000}
```

Resultado esperado em `UI_COLOR_PER_METRIC`:

- Geral: CPU 91 e MEMÓRIA 89 em `crit`, GPU 76 em `gpu`, DISCO 64 em `disk`; cards com "2,4 MB/s", "410 KB/s", "18 MB/s" e "6,2 MB/s"
- Barra de status: "Pressão em alerta · swap 3,4%" com ponto `warn`
- Rede: escala sobe para 5000 KB/s, com eixo "5M" / "2,5M" / "0"
- Núcleos: "CPU total 91%" e todas as barras em `crit`, porque todos os valores passam de 85

Teste também arrays `cores` com 4, 14 e 20 elementos, para validar a largura das barras e a recriação quando N muda.

## 10. Fora do escopo e pendências

- Tela "Aguardando o Mac..." da fase 3: ainda não foi desenhada. Até lá, use um overlay provisório em `lv_layer_top()` cobrindo a área de conteúdo (y 22 a 200), com fundo `bg` a ~90% de opacidade e o texto "Aguardando o Mac..." em `F12` / `text_2`, centralizado. A barra de status e a barra de abas continuam visíveis
- Escurecer a tela, relógio, LED de alerta e restaurar a última aba: fase 6
- Temperatura, potência, bateria e processos: fase 7
- Estado pressionado das abas: sugestão na seção 4, sem desenho final

## 11. Critérios de aceite

- [ ] As 4 telas batem com o canvas, com tolerância de ±2 px
- [ ] Os acentos aparecem corretamente em "MEMÓRIA", "Histórico", "Núcleos" e "Pressão"
- [ ] O toque em cada aba troca de tela, o deslize horizontal também, e a aba ativa fica marcada
- [ ] Toda a célula de 80x40 de cada aba responde ao toque
- [ ] Arcos e barras animam em cerca de 300 ms, sem cintilação
- [ ] Os gráficos mostram 2 minutos a 1 ponto por segundo, com o ponto novo entrando à direita, e continuam acumulando quando outra aba está aberta
- [ ] A escala da rede sobe na hora e volta a descer depois que o pico sai da janela
- [ ] O número de barras acompanha o tamanho de `cores` (testado com 4, 10, 14 e 20)
- [ ] Os dois modos de cor funcionam (`UI_COLOR_PER_METRIC` e `UI_COLOR_BY_RANGE`)
- [ ] Depois de 3 s sem dados, a barra de status mostra "aguardando" e os gráficos abrem um vão; o painel volta sozinho quando os dados retornam
- [ ] O firmware roda 24 horas sem travar, com `ESP.getFreeHeap()` estável em vez de cair aos poucos
