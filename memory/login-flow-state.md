---
name: login-flow-state
description: "Estado do fluxo de login automático (CABAL_Login DLL) — o que funciona, o que falta, e a decisão da próxima fase"
metadata:
  node_type: memory
  type: project
  modified: 2026-08-16T00:00:00.000Z
---

# Estado do Fluxo de Login (CABAL_Login DLL, 2026-08-16)

## ✅ O que JÁ FUNCIONA na DLL (validado ao vivo 2026-08-09, MCP injetado, XignCode3 ativo)

- Injeção + thread + anti-hack patch (`0x352CDB` jne→jmp, opcional `0x34D1AD` via `login.cfg:antihack2 1`)
- Gatilho manual: F8 ou arquivo `login.go`
- Loop sequencial de contas: lê `login.current` → `login.n` (ou fim) → `for (acc = idx; acc < limit; ++acc)`
- Credenciais via SendInput (scancode Enter 0x1C, Unicode user/pass, Ctrl+A limpa campos)
- **Validação de estado por máquina de estados** (regra: nunca avançar sem confirmar o atual):
  - Socket conn1 `[base+0x13E6310+0x08]`: `-1/0 = tela LOGIN`, `vivo = conectado` (servidor/personagem/mundo)
  - Char data `base+0x137F170` (ouro), `0x137F1B8` (nível), `0x137F190` (nome) — carrega na seleção de personagem
  - `login_wait_logged_in/char_ready/world/login_screen` com timeout + SEH + log ok/FALHA por passo
- `login_toggle_ui(mode)` → `base+0x549FD8` (`this=base+0x138ABF0`, mode 0=I, 2=O) — **100%**
- **Desconectar direto**: `login_do_disconnect()` → `base+0x356B30` (`this=buffer zerado`) — fecha AS DUAS conexões (login+world) → volta ao login. **Já inclui o "Sim" interno.**
- **Selecionar servidor direto**: `login_do_server_select()` → `base+0x34F878` — fecha SÓ world (`0x1413E6318`), lê global `0x140F5DAEC` → volta à seleção de servidor. **Já dispara o "Sim" interno.**
- USERDATACONTEXT finder runtime (`login_find_userdata`): 3 estratégias (RVA fixo, scan `48 8B XD` + deref, `48 B8 <imm64>` Themida) + validação heurística (nome`+0x2528`, nível`+0x2D90`, onLogged`+0x2F58`). Exclui apenas seções EXEC. Cache em `g_udc`.
- Verificação enriquecida (`login_verify_enriched`): timestamp, user, char, nível, ouro, classe, nação, rank, mapa, guild, onLogged, src=`userdata|legacy`. Grava em `output.txt` (append).
- **recvwatch**: hook in-place (trampoline 12 bytes) em `ws2_32!recv` + `WSARecv` (fallback IAT) → reassembly por socket → decrypt keychain XOR 1ª metade → loga maincmd S2C + classifica falha (0x78/0x79 NFY_SYSTEMMESSG/SERVERSTATE) via keywords: WRONGPASS/BLOCKED/FULL/MAINT/OTHER.
- Crypto XOR **CONFIRMADA 100% ao vivo**: seed `0x8F54C37B|1`, SEND `0x7AB38CF1`, magic `0xB7E2`/ext `0xC8F3`, keytable `16384` DWORDs, LCG `0x2F6B6F5/0x14698B7/0xB327BD/0x27F41C3` == porta exata CABALREVERSE.
- Testes unitários: `test_crypto.exe` (keychain roundtrip + cruzado Python), `test_recvwatch.exe` (reassembly + classificação) — ambos passam.
- Logs tempo real: `AllocConsole` + `login_set_console` → console ANSI + arquivo `login_<idx>.log` (unbuffered).
- Marcadores: `done_<idx>` por conta, `done_all` fim do loop, `login.stop` para pausa externa.

## ❌ O que TRAVA (2 handlers faltando)

| Função | Linha | Problema |
|--------|-------|----------|
| `login_select_server()` | `login.c:295-299` | Retorna `false` — **handler do botão "Mercúrio" (servidor) na tela de seleção não confirmado** |
| `login_start_character()` | `login.c:310-314` | Retorna `false` — **handler do botão "Iniciar" (0x8E EnterWorld) na tela de personagem não confirmado** |

**Consequência**: `login_run_account` para nesses pontos (linhas 705-708 e 729-731) e a conta não completa.

## ⚠️ Surpresas Críticas (já resolvidas — NÃO precisam de handler)

- `login_do_server_select()` (base+0x34F878) **já dispara o "Sim" internamente** — fecha world → sel. servidor.
- `login_do_disconnect()` (base+0x356B30) **já faz efeito completo** de "Desconectar > Sim" — fecha 2 conns → login.
- **Não precisamos** do handler do "Sim" nem do "Desconectar" do menu O.

## 🔬 Bloqueio da RE Estática (confirmado 2026-08-08/09)

- **Tudo é virtual**: vtables construídas em runtime (heap), métodos em .text limpo. Chamadas indiretas via function-pointer.
- `find_callers`/`xref`/`aob_scan` de endereços de função retornam **0 hits**.
- Handler de input no VM Themida (0x141A00000+) — ofuscado.
- Strings de evento (`EventBtnClick` etc.) em `.rdata` sem referência de código — registro evento→handler é runtime.
- **Caminho único viável**: observação dinâmica via `probe.dll` injetada **no launch** (antes do anti-cheat travar).

## 🎯 Próxima Fase: Observação Dinâmica (precisa jogo + MCP)

**Ferramenta pronta**: `probe.dll` (`probe/probe.c` + `hookstub.asm`) — hook one-shot em:
- `base+0x549FD8` (toggle_ui)
- `base+0x356B30` (disconnect)
- `base+0x389698` (ENCRYPT)

Loga: return address + cadeia RBP + scan pilha → `D:\projeto\CABAL_Login\probe.log`

**Bloqueio conhecido**: `VirtualAllocEx` em processo **já rodando** falha erro 5 (anti-cheat protege depois de inicializar).

**Solução**: Injetar `connect_game.dll` + `probe.dll` **JUNTAS NO LAUNCH** do jogo.

**Passos**:
1. Preparar injector duplo (ou launcher Connect Game que carrega bridge no launch)
2. Abrir jogo → estabilizar → conectar MCP ao PID
3. Tela seleção servidor: **clicar "Mercúrio"**
4. Tela personagem: **clicar "Iniciar"**
5. Fechar jogo → ler `probe.log` → extrair 2 handlers → implementar nas 2 funções

## Validação Pós-Handlers

1. Compilar DLL com handlers confirmados
2. `login.n = 10` → injetar → F8 → aguardar `done_all`
3. Verificar `output.txt`: 10 linhas `verificacao=OK`, zero crashes
4. Escala: `login.n = 1600` → ~8-12h total (cliente fica aberto)

## Config/Arquivos

- `login.cfg`: 7 coordenadas (fallback legado: entrar, servidor, canal, comeca, selecionar_servidor, sim, desconectar)
- Contas: `Cabal BR SUB.txt` (1600 contas formato BR)
- Runner: **DLL faz loop interno** — Python runner removido
- DLL: `src/login.c` + `src/login_dll.c` + `src/recvwatch.c` → `x64/Release/CABAL_Login.dll`
- Probe: `probe/probe.c` + `hookstub.asm` → `x64/Release/probe.dll`

## Implementado 2026-08-09 (plano CABALREVERSE)

- **recvwatch** (src/recvwatch.c/h): hook in-place recv/WSARecv → reassembly → decrypt keychain XOR 1ª metade → classifica falha via 0x78/0x79
- **login_find_userdata()**: resolve USERDATACONTEXT runtime — hipótese RVA + scan `.text` `48 8B XD` (8 variantes) + `48 B8 <imm64>` Themida, validando heurístico (nome+0x2528, nível+0x2D90, onLogged+0x2F58) com exec-only exclusion
- **login_verify_enriched()**: grava classe/nação/rank/mapa/guild/onlogged no output.txt; src=userdata se finder achou, senão legacy
- **login_wait_onlogged()**: espera +0x2F58==1 como confirmação "no mundo"
- Testes unitários: test_crypto (keychain+roundtrip, cruzado Python) e test_recvwatch (reassembly+classificação) — ambos passam

## VALIDAÇÃO AO VIVO 2026-08-09 (MCP injetado, XignCode3 ativo — PID 26120)

- **Crypto: CONFIRMADA 100% ao vivo.** DecodeHeader do client em RVA `0x388A68` usa `xor ...,0x7AB38CF1` (SEND_XORKEY), magic `0xB7E2`/ext `0xC8F3`, index `keytable[cpPacket & 0x3FFF]`. CXorKeyTable em `0x388ACC`: seed `0x8F54C37B` (tool: "Cabal INITIAL_KEY"), `0x4000` dwords, LCG `0x2F6B6F5/0x14698B7/0xB327BD/0x27F41C3`. == nosso port.
- **USERDATACONTEXT deste build: NÃO está em RVA 0x8ECAC4** (hipótese x86 falhou — era código). Objeto real em **RVA `0xF71A98`** (`.data`, seção 0xEF0000), acessado sem slot estático no módulo (aob por bytes do endereço = 0 hits). Offsets CONFIRMADOS: nome `+0x2528`="TroTXD", len `+0x2538`=6, charIdx `+0x394`=1, nação `+0x4A4`=0.
- **Faltando (precisa usuário ENTRAR no mundo)**: nível `+0x2D90` e onLogged `+0x2F58` (hoje = 0, char fora do mundo; legado nível 200). Finder só preenche/valida objeto ativo in-world.
- **recvwatch IAT NÃO funciona neste build**: CabalMain importa de ws2_32 SÓ `bind` (0x7FFA748E09C0), de MSWSOCK `AcceptEx` — recv/WSARecv resolvidos em runtime (Themida). Prólogos recv/WSARecv (Win10) são limpos (sem RIP-relative nos 12 primeiros bytes) → hook in-place OK.
- Confirmar próximo run: log `recvwatch: N hook(s)` (espera-se 2) e `USERDATACONTEXT encontrado`; conferir seq S2C p/ calibrar keywords.