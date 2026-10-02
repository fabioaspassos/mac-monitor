# Tela Claude Code · Dados que o agente precisa fornecer

Este documento especifica a nova feature do agente Python no Mac que alimenta a tela "Claude Code" do monitor (artboard `Claude.dc.html` no canvas da fase 5). Ele cobre as fontes de dados, o cálculo de cada métrica, o formato da mensagem enviada ao ESP32, a configuração e o tratamento de credenciais.

## 1. O que a tela exibe

| Elemento da tela | Exemplo | Campo do protocolo (seção 5) |
| --- | --- | --- |
| Arco "BLOCO 5 H" | 62% | `b5.pct` |
| Texto abaixo do arco | "reinicia em 1h48" | `b5.rst` (segundos até reiniciar) |
| Bloco atual | 412 k tok | `tb` |
| Hoje | 1,84 M tok | `td` |
| Ritmo agora | 3,2 k/min | `rate` |
| Semana (linha 4 do card) | 41% · qui 21h | `w7.pct`, `w7.wd`, `w7.wh` |
| Sessões ativas (linha 4 do card quando não há `w7`) | 2 | `ses` |
| Barras "tokens por hora · hoje" | 24 barras | `h` (array de 24 valores) |
| Rótulo da hora atual | "15h · 62 k" | `hr` + `h[hr]` |

## 2. De onde vem cada dado

Existem três fontes possíveis. Só as duas primeiras são necessárias para a tela.

### 2.1 Status line do Claude Code (percentual oficial do limite)

O percentual do bloco de 5 horas e o horário de reinício só existem de forma oficial no JSON que o Claude Code entrega ao comando de status line, no campo `rate_limits`:

| Campo do status line | Uso |
| --- | --- |
| `rate_limits.five_hour.used_percentage` | Percentual do bloco de 5 h (0 a 100) |
| `rate_limits.five_hour.resets_at` | Reinício do bloco, em epoch (segundos) |
| `rate_limits.seven_day.used_percentage` | Percentual da semana (reservado para uso futuro) |
| `rate_limits.seven_day.resets_at` | Reinício da semana |
| `session_id` | Identifica a sessão (para contar sessões ativas) |
| `model.display_name` | Modelo em uso (reservado para uso futuro) |

Limitações conhecidas:

- `rate_limits` só aparece para assinantes Pro e Max (ou atrás de um gateway com limite de gasto). Com login por chave de API ele não vem
- O Claude Code só chama o status line enquanto há uma sessão aberta. Sem sessão, o último valor fica parado, e o agente precisa tratar isso (seção 4.1)
- Há relatos de versões em que o campo veio ausente. Se faltar, o agente cai no modo estimado (seção 4.1)

Como capturar: um script pequeno, `agent/claude_statusline.py`, configurado como status line em `~/.claude/settings.json`:

```json
{
  "statusLine": {
    "type": "command",
    "command": "python3 ~/mac-monitor/agent/claude_statusline.py"
  }
}
```

O script:

1. Lê o JSON do stdin
2. Grava **apenas** os campos acima, mais o horário atual (`seen_at`), em `~/Library/Caches/mac-monitor/claude/<session_id>.json`. Grava num arquivo temporário e renomeia, para o agente nunca ler um arquivo pela metade
3. Imprime uma linha curta para o próprio status line do terminal (por exemplo `Opus · 62% do bloco`). Se você já usa outro status line, o script pode chamar o anterior e repassar a saída dele
4. Termina rápido (meta: menos de 50 ms) e nunca falha com erro: qualquer exceção é ignorada e ele imprime uma linha vazia

O JSON de entrada também traz `cwd`, `transcript_path` e nome do repositório. Esses campos não são gravados.

### 2.1b Endpoint de uso do Claude Code (fonte principal do percentual)

`GET https://api.anthropic.com/api/oauth/usage`, com `Authorization: Bearer <token OAuth do Claude Code>` e `anthropic-beta: oauth-2025-04-20`. É o que o `/usage` do Claude Code consulta. Implementado em `agent/claude_oauth.py`.

| Campo da resposta | Uso |
| --- | --- |
| `five_hour.utilization` | Percentual do bloco de 5 h, de 0 a 100 (bate com `limits[].percent`) |
| `five_hour.resets_at` | Reinício do bloco, ISO 8601 com fuso |
| `seven_day.utilization` / `seven_day.resets_at` | Semana |

Diferenças em relação ao status line: vale para a conta inteira (Claude Code, chat, Cowork) e é atual mesmo sem sessão aberta.

Cuidados:

- Não é documentado: pode mudar ou sumir sem aviso. Qualquer falha cai no status line (2.1) e depois no modo estimado
- O token fica no Keychain (item `Claude Code-credentials`), é lido a cada consulta e descartado. O agente nunca renova o token (isso invalidaria o do Claude Code); com o token vencido, espera o Claude Code renovar
- O token nunca aparece em log, em mensagem de erro nem na serial
- Consulta a cada 2 minutos (`oauth_interval_s`), numa thread separada; desliga com `oauth_usage = false`
- O uso do token da assinatura fora do Claude Code é restrito pelos termos da Anthropic; aqui ele só lê o próprio uso

### 2.2 Transcrições locais (tokens, horas, ritmo)

O Claude Code grava cada sessão em arquivos JSONL em `~/.claude/projects/<projeto>/<session_id>.jsonl` (ou em `$CLAUDE_CONFIG_DIR/projects` se essa variável estiver definida). As linhas de resposta do assistente trazem o consumo de tokens. É a mesma fonte que a ferramenta ccusage usa.

Campos usados de cada linha do tipo `assistant`:

| Campo | Uso |
| --- | --- |
| `timestamp` (ISO 8601, UTC) | Em que hora e em que bloco o consumo entra |
| `sessionId` | Sessões ativas |
| `message.id` e `requestId` | Remover duplicatas (a mesma resposta pode aparecer em mais de uma linha) |
| `message.model` | Reservado para uso futuro |
| `message.usage.input_tokens` | Tokens |
| `message.usage.output_tokens` | Tokens |
| `message.usage.cache_creation_input_tokens` | Tokens |
| `message.usage.cache_read_input_tokens` | Enviado à parte (ver seção 3) |

O formato desses arquivos não é uma API documentada e pode mudar entre versões. O leitor precisa ignorar linhas que não consiga interpretar, sem derrubar o agente.

Leitura eficiente:

- Na partida, ler só os arquivos modificados nas últimas 24 h
- Depois, guardar o tamanho já lido de cada arquivo e ler apenas o que foi acrescentado (como `tail -f`), a cada 5 s
- Manter em memória só os registros das últimas 24 h, já agregados por minuto

### 2.3 Admin API da Anthropic (opcional, não alimenta a tela)

A Anthropic oferece o endpoint `GET /v1/organizations/usage_report/claude_code` (Claude Code Analytics API), autenticado com uma **chave de administrador** de organização (`sk-ant-admin...`), não com uma chave de API comum.

Antes de investir nisso, vale saber:

- Os dados são agregados **por dia**, sem a resolução por hora ou por minuto que a tela usa
- A API não informa o percentual do bloco de 5 h nem o horário de reinício
- Há relato aberto (issue #27780 do repositório claude-code) de que ela só retorna uso de contas de API (`customer_type: "api"`), sem o uso de assinaturas Pro e Max

Por isso a chave de API não substitui as fontes 2.1 e 2.2. Ela só faz sentido se você usar Claude Code por uma organização de API e quiser, no futuro, um card de custo diário (`model_breakdown[].estimated_cost`), linhas de código ou commits. Se for implementada, siga a seção 7 para guardar a chave e consulte no máximo a cada 15 minutos.

## 3. Cálculo de cada métrica

Definição de "tokens" neste documento: `input_tokens + output_tokens + cache_creation_input_tokens`. Os tokens de leitura de cache ficam de fora porque são muito maiores e distorcem a leitura. Eles são somados à parte e enviados no campo opcional `tc`, caso a tela passe a mostrá-los.

| Campo | Cálculo |
| --- | --- |
| `b5.pct` | `rate_limits.five_hour.used_percentage` do arquivo de status mais recente (maior `seen_at` entre as sessões), arredondado para inteiro. Vale só se `resets_at` ainda estiver no futuro |
| `b5.rst` | `resets_at - agora`, em segundos. O ESP32 faz a contagem regressiva localmente entre uma mensagem e outra |
| `tb` | Soma dos tokens com `timestamp` entre `resets_at - 5 h` e agora. Sem `resets_at`, usar o bloco estimado (seção 4.1) |
| `td` | Soma dos tokens desde a meia-noite local do Mac |
| `rate` | Soma dos tokens dos últimos 5 minutos dividida por 5 (tokens por minuto) |
| `ses` | Sessões distintas com atividade nos últimos 5 minutos: um arquivo de status com `seen_at` recente ou uma linha nova na transcrição |
| `h` | 24 valores: tokens de cada hora local de hoje, em milhares (arredondado). Horas futuras vão como 0 |
| `hr` | Hora local atual do Mac (0 a 23) |
| `src` | `"api"` quando `b5` vem do endpoint de uso (2.1b); `"sl"` quando vem do status line; `"est"` quando é estimado; `"none"` sem dado |

Os horários dos JSONL estão em UTC. Converta para o fuso local do Mac antes de agrupar por hora e por dia.

## 4. Estados e casos de borda

### 4.1 Sem dado oficial do bloco

| Situação | O que enviar | O que a tela mostra |
| --- | --- | --- |
| Status line nunca rodou ou não tem `rate_limits` | `src: "est"`, `b5.pct: null`, `tb` pelo bloco estimado | Arco com "--" e texto "estimado" |
| `resets_at` já passou e não houve atividade depois | `src: "none"`, `b5: null` | Arco vazio, texto "sem bloco ativo" (cenário "Ocioso" do canvas) |
| Nenhum arquivo do Claude Code encontrado | Mensagem com `ok: 0` | Mensagem "Claude Code não encontrado" (ainda não desenhada) |

Bloco estimado (mesma lógica do ccusage): começa na hora cheia da primeira mensagem após um intervalo de mais de 5 h sem atividade e dura 5 h.

### 4.2 Outros cuidados

- Mac acordando do repouso: recalcular tudo a partir dos arquivos, sem confiar em contadores em memória
- Virada do dia: zerar `td` e `h` à meia-noite local
- Arquivos de status de sessões antigas: apagar os que tiverem `seen_at` com mais de 24 h
- Leitura dos JSONL não pode passar de 1% de CPU em média, a mesma meta da fase 2

## 5. Mensagem enviada ao ESP32

Os dados do Claude Code mudam devagar e o array de horas é grande. Por isso eles vão numa linha separada, com um campo de tipo, a cada 10 s (e logo depois de qualquer mudança em `b5.pct`). A linha de métricas de 1 s continua igual.

Novo campo comum às duas linhas: `t`, o tipo da mensagem. Linha sem `t` continua sendo a de métricas, então o firmware atual segue funcionando.

```json
{"v":1,"t":"cc","ok":1,"src":"sl","b5":{"pct":62,"rst":6480},"w7":{"pct":41,"rst":302400,"wd":4,"wh":21},"tb":412000,"td":1840000,"tc":9300000,"rate":3200,"ses":2,"hr":15,"h":[0,0,0,0,0,0,0,0,12,48,96,130,84,22,110,62,0,0,0,0,0,0,0,0]}
```

| Campo | Tipo | Unidade | Obrigatório | Descrição |
| --- | --- | --- | --- | --- |
| `v` | int | | sim | Versão do protocolo (1) |
| `t` | string | | sim | `"cc"` para esta mensagem |
| `ok` | int | | sim | 1 se encontrou dados do Claude Code; 0 caso contrário (nesse caso os demais campos podem faltar) |
| `src` | string | | sim | `"api"`, `"sl"`, `"est"` ou `"none"` |
| `b5` | objeto ou null | | sim | Bloco de 5 h |
| `b5.pct` | int ou null | % | sim | 0 a 100 |
| `b5.rst` | int | segundos | sim | Tempo até o reinício |
| `w7` | objeto ou null | | não | Semana: `pct` e `rst`, mesmo formato, mais `wd` e `wh` |
| `w7.wd` | int | | não | Dia da semana local do reinício (0 = domingo), para "qui 21h" |
| `w7.wh` | int | hora | não | Hora local do reinício (0 a 23) |
| `tb` | int | tokens | sim | Tokens no bloco atual |
| `td` | int | tokens | sim | Tokens hoje |
| `tc` | int | tokens | não | Leituras de cache hoje (reservado) |
| `rate` | int | tokens/min | sim | Média dos últimos 5 min |
| `ses` | int | | sim | Sessões ativas |
| `hr` | int | hora | sim | Hora local atual (0 a 23) |
| `h` | int[24] | milhares de tokens | sim | Tokens por hora de hoje |

A linha tem cerca de 250 bytes. No firmware:

- Aumentar o buffer de leitura da serial para pelo menos 512 bytes
- Criar uma `struct ClaudeUsage` separada de `Metrics`, preenchida quando `t == "cc"`
- Guardar o `millis()` do recebimento para descontar `b5.rst` localmente e atualizar o "reinicia em" a cada minuto
- Considerar o dado velho se nenhuma mensagem `cc` chegar em 60 s

Formatação na tela (já usada no design): tokens até 999 sem sufixo, de 1.000 a 999.999 como `412 k`, a partir de 1.000.000 como `1,84 M`; ritmo como `3,2 k/min`; reinício como `1h48`.

## 6. Configuração do agente

Arquivo `~/.config/mac-monitor/config.toml` (nenhum segredo aqui):

```toml
[claude]
enabled = true
projects_dir = "~/.claude/projects"           # respeita CLAUDE_CONFIG_DIR se definido
status_cache_dir = "~/Library/Caches/mac-monitor/claude"
send_interval_s = 10
scan_interval_s = 5
active_window_min = 5
rate_window_min = 5
count_cache_read = false
oauth_usage = true                            # endpoint de uso (2.1b)
oauth_interval_s = 120

[claude.admin_api]                            # opcional, seção 2.3
enabled = false
keychain_service = "mac-monitor-anthropic-admin"
poll_interval_min = 15
```

Dependências novas em `agent/requirements.txt`: `keyring` (só se a Admin API for usada). A leitura de TOML usa `tomllib`, que já vem no Python 3.11+.

## 7. Credenciais (somente se usar a Admin API)

- Guardar a chave no Keychain do macOS, nunca em arquivo do repositório nem no `config.toml`. Cadastro: `security add-generic-password -s mac-monitor-anthropic-admin -a "$USER" -w` (o comando pede a chave sem mostrar na tela)
- O agente lê com a biblioteca `keyring`. Como alternativa para testes, aceitar a variável de ambiente `ANTHROPIC_ADMIN_KEY`
- No LaunchAgent da fase 6, não colocar a chave no `.plist`: o agente lê do Keychain ao iniciar
- A chave nunca é enviada ao ESP32, nunca aparece em log e nunca é impressa em mensagens de erro
- Incluir `config.toml` local, caches e qualquer `.env` no `.gitignore`
- Chave de administrador tem poder sobre a organização inteira. Se for criar uma, crie uma dedicada ao monitor, para poder revogar sem afetar outros usos

## 8. Estrutura sugerida no agente

```text
agent/
├── agent.py                 laço principal (já existe), envia "t":"cc" a cada 10 s
├── metrics.py               métricas do Mac (fase 2)
├── claude_statusline.py     script do status line (seção 2.1)
├── claude_usage.py          leitura incremental dos JSONL e cálculo (seções 2.2 e 3)
├── claude_admin_api.py      opcional (seção 2.3)
└── config.py                leitura do config.toml e do Keychain
```

## 9. Critérios de aceite

- [ ] Com uma sessão do Claude Code aberta numa assinatura Pro ou Max, `b5.pct` bate com o valor mostrado por `/usage` (diferença de até 1 ponto)
- [ ] O "reinicia em" chega a zero no mesmo horário informado por `/usage`
- [ ] `td` bate com o total do dia do ccusage, considerando a mesma definição de tokens (diferença de até 2%)
- [ ] Respostas duplicadas nos JSONL não são contadas duas vezes
- [ ] Sem sessão aberta por mais de 5 h, a tela mostra "sem bloco ativo"
- [ ] Sem `rate_limits`, a tela mostra o modo estimado em vez de um número inventado
- [ ] Um JSONL com linha corrompida não derruba o agente
- [ ] O agente continua abaixo de 1% de CPU em média com a feature ligada
- [ ] Nenhum segredo aparece no repositório, nos logs ou na serial
- [ ] O firmware antigo continua funcionando ao receber linhas `"t":"cc"` (ignora as que não conhece)

## Referências

- [Status line do Claude Code: campos do JSON, incluindo `rate_limits`](https://code.claude.com/docs/en/statusline)
- [Claude Code Analytics API](https://platform.claude.com/docs/en/manage-claude/claude-code-analytics-api)
- [Issue #27780: a Analytics API não retorna usuários de assinatura](https://github.com/anthropics/claude-code/issues/27780)
- [ccusage](https://github.com/ryoppippi/ccusage): referência para leitura dos JSONL e blocos estimados
