# Functions Inventory — CabalMain.exe (`base+0x`)

> Status: CONFIRMED (evidência direta) / HIGH (várias evidências) / LIKELY / HYPOTHESIS / UNKNOWN

| Endereço | Nome hipotético | Categoria | Assinatura | Status | Confiança | Evidência |
|---|---|---|---|---|---|---|
| `0x549FD8` | toggle_ui | UI | `(this=0x14138ABF0, mode)` mode 0=I 2=O | CONFIRMED | 96% | desmontada; DLL chama e funciona |
| `0x356B30` | disconnect (fecha 2 conns) | Rede/UI | `(this)` this p/ [this+0x45)R opcional zerado | CONFIRMED | 95% | desam; fecha conn1+conn2; seta [conn+0x88]=0x0A; chamável |
| `0x357428` | disconnect variante | Rede | mesmosite | LIKELY | 75% | idêntica; call extra 0x14138E840 |
| `0x549FD8` | toggle_ui | UI | `(this=0x14138ABF0, mode)` mode 0=I 2=O | CONFIRMED | 96% | DLL usa; desmontada |
| `0x356B30` | `disconnect` (fecha 2 conns→login) | Rede/UI | `(void)` usa globais+this zerável | CONFIRMED | 95% | fecha conn1/2; `[conn+0x88]=0x0A`. DLL: `login_do_disconnect()` |
| `0x357428` | disconnect variante | Rede | `(void)` | LIKELY | 75% | idêntica; call extra 0x14138E840 |
| `0x34F878` | `selecionar_servidor` (fecha SÓ o mundo) | UI/Net | **`void(void)`** — lê `0x140F5DAEC`, seta [res+0x198]=1, fecha conn2  | CONFIRMED | 90% | desam 0x34f878-0x34f8eb; DLL `login_do_server_select()`. **CORRIGE a entrada 0x34F890 (site interno) → entrada real = 0x34F878** |
| `0x351248` | handler do botão "Selecionar Servidor" (menu O) | UI/Net | `(this)` checa `0x140F5DAEC`→chama `0x34F878`; flags/cooldown | CONFIRMED | 88% | desam 0x351248; **chamador direto de 0x34F878** (find_callers); runtime (clicado pelo usuário) |
| `0x3893EC` | close_connection | Net | `(conn_obj)` fecha 2 sockets | CONFIRMED | 95% | chamadores: 0x356B30, 0x357428, 0x34F898 |
| `0x389484` | socket_close | Net | `(conn)` shutdown+closesocket | CONFIRMED | 95% | 3 `ff15` closesocket |
| `0x389698` | ENCRYPT (hook) | Net | — | CONFIRMED | 100% | hook do proxy funciona |
| `0x86CE50` | GetProperty | UI | `(out*, propId)` lê 16B da tabela 0x140F72090 | CONFIRMED | 95% | 29 callers |
| `0x140F73AE0` | objeto global do jogo | Core | struct: vetores +0x20/+0x40; lista canais +0x80 | CONFIRMED | 90% | leitura + uso nos desam |
| `0x14137DFB0` | manager server/canal | Core | `GetChannel` via 0x1404DEAA4 (vetor vazio) | PARTIALLY | 75% | desam GetChannel; vetor vazio |
| `0x4A49D4` | dispatch canal→opcode | Dispatch | `(game_obj, canal)` confir | CONFIRMED | 95% | switch canal1-6→opcode; 7-9→ptr |
| `0x4A5368` | findChannel | Dispatch | busca nó na lista [game_obj+0x80] | CONFIRMED | 90% | desam lista linkada |
| `0x740F82` | builder 0x8C (conecta canal) | Net | lê [obj+0x198]=canal; GetChannel; send | CONFIRMED | 90% | desam |
| `0x1F1A6` | envio 0x8C | Net | `SendPacket(…, 0x8C)` | CONFIRMED | 90% | desam; stack arg |
| `0xA01B34` | fluxo entrar canal | Net | envia 0x8C+0x48C+0x29C | LIKELY | 85% | desam |
| `0x5643A3` | builder 0x8E (EnterWorld) | Network | — | CONFIRMED | 95% | desam + strategy.md |
| `0x7222B3` | builder 0x8E | Network | — | CONFIRMED | 95% | strategy.md |
| `0x80A0B2` | builder 0x8E | Network | — | CONFIRMED | 95% | é strategy.md |
| `0xAC7267` | dispatch 0x8E | Dispatch | retorna opcode 0x8E | LIKELY | 90% | desam sessão anterior |
| `0x3767A4` | send packet genérico | Net | opcode no stack arg | LIKELY | 80% | desam |
| `0x448924` | thunk→0x4A5368 | Dispatch | `lea 0x140F73AE0; jmp` | CONFIRMED | 95% | desam |
| `0x14` | Jogador |  | — | — | — | — |