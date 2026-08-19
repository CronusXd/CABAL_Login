# RELATÓRIO FINAL — Mapa executável do CabalMain.exe (EP33) — UI → Handler → Função → Efeito

> Sessão contínua 2026-08-08 (modo RE/UI-call-mapping). Base `0x140000000`. Themida. MCP `connect_game` injetado.
> Documentos de apoio: `../FLUXO_UI.md`, `../FLUXO_LOGIN.md`, `functions/inventory.md`, `dispatchers/dispatch.md`, `ui/classes.md`, `confirmed/architecture.md`, `hypotheses/open.md`.

---

## 0. RESUMO EXECUTIVO
- **Framework de UI identificado: "Snake UI"** — C++, namespace `Snake::UI`, **RTTI MSVC completo** no .rdata.
- Botões = classe **CButton**; janelas = **CDialog**/CInfoDialog; handlers = **`std::function<bool(const EventArgs&)>`** num **EventSet** (eventos nomeados `EventBtnClick`, `EventMouseLButtonClick`…).
- **Dispatch de clique**: input bruto (VM Themida) → CUIMgr acha controle → dispara handler do EventSet → roda a ação.
- **Vtables da UI = runtime** (protegidas); vtables CRT/STL = estáticas (corrigido). Dispatcher de clique precisa de objeto vivo (menu aberto) — **único bloqueio real**.
- **DLL compilada** com 3 chamadas diretas de ação.

## 1. MAPA DA UI → HANDLER → FUNÇÃO → EFEITO

### Tela de LOGIN
| UI | Texto | Evento | Handler(s)/Função | Efeito | Status |
|---|---|---|---|---|---|
| Campo ID | — | digitação | login.digita (SendInput) | preenche usuário | CONFIRMED |
| Campo Senha | — | digitação | SendInput | preenche senha | CONFIRMED |
| **Entrar** | "Entrar" @0x463f13e0 | clique/Enter | handler→auth: `0x65` Connect2Svr (0x1400EEFFB…), CheckVersion `0x7A`, PreServerRequest `0x7D2` (envia user), PublicKey `0x7D1`, **AuthAccount `0x67`** (0x1400B74CF…), desafio `0x7D6` | conecta login+autentica → tela de servidor | CONFIRMED (fluxo de pacotes; handlers de UI indiretos) |

### SELEÇÃO DE SERVIDOR/CANAL
| UI element | Texto | Evento | Handler/Função | Efeito | Status |
|---|---|---|---|---|---|
| Servidor Mercúrio/Vênus | nomes @0x37669460 /0x61cb23a0 | clique | seleção→mostra canais (handler UI; estado de UI) | lista de canais visível | LIKELY |
| **Canal N** | — | clique | **0x140A01A00** (fluxo entrar canal) + dispatch canal **0x4A49D4**(game_obj,canal) → canal1-6:opcode; 7→0x140C98BF8, 8→0x140C98BFC, **9→0x140C98C00** → findChannel **0x4A5368** → builder **0x8C** `0x740F82` (+`0x1F1AA6`, `0xA01B34`) | conecta ao world (0x8C) → tela de personagem | CONFIRMED (cadeia); objeto de sessão a mapear |
| (setas+Enter) | — | teclado | navegação nativa da lista (registra sem coordenadas) | funciona | CONFIRMED |

### PERSONAGEM
| UI element | Evento | Handler/Função | Efeito | Status |
|---|---|---|---|---|
| **Iniciar** | clique/Enter | builders `0x8E` EnterWorld: `0x5643A3`, `0x7222B3`, `0x80A0B2`; dispatch `0xAC7267` | entra no mundo | CONFIRMED |
| lista de chars | setas | navegação de lista | seleciona char | CONFIRMED |

### NO MUNDO → MENU O
| UI element | Evento | Handler/Função | Efeito | Status |
|---|---|---|---|---|
| Tecla I | key | `toggle_ui(0)` **0x549FD8** | abre inventário | CONFIRMED (DLL usa) |
| Tecla O | key | `toggle_ui(2)` **0x549FD8** | abre menu | CONFIRMED |
| **Selecionar Servidor** | clique | **0x34F878** (`void`) | fecha SÓ o mundo, mantém login → seleção | **CONFIRMED (desta sessão)** |
| **Desconectar** | clique | **`0x356B30`** (`void`, fecha as 2 conexões, `[conn+0x88]=0x0A`) | volta à tela de login | CONFIRMED |
| Diálogo **Sim/Não** | clique | ação do diálogo (o `Sim` dispara o handler da ação — DLL chama as ações direto) | confirma/cancela | PARTIALLY |

## 2. CALL GRAPH (principais)
```
[clique canal] → 0x4A49D4(game_obj 0x140F73AE0, canal)
    → 0x4A5368 findChannel(0x140F73AE0+0x80)
        → 0x740F82 builder 0x8C → SendPacket
[desconectar] → 0x356B30 → 0x3893EC(close conn1/conn2) → 0x389484(closesocket) + seta estado 0x0A
[selecionar servidor] → 0x34F878 → fecha 0x1413E6318 (world) ; mantém 0x1413E6310 (login)
[inventário/menu] → 0x549FD8(toggle) → tabela janelas 0x14138ABF0
[EnterWorld] → builders 0x8E → 0xAC7267(ddispatch)
```

## 3. DISPATCHERS
- Clique genérico UI: **por evento nomeado → EventSet(std::function)**. Input bruto no VM. → exige objeto vivo p/ leitura (bloqueado sem interação).
- Canal: 0x4A49D4 (switch canal→opcode/ptr) — CONFIRMED.
- EnterWorld: 0xAC7267 — LIKELY.
- Desconectar/Selecionar: função de ação direta — CONFIRMED.
- `find_callers`/xref falham nos métodos de UI (vtables runtime) — confirmado.

## 5. STRUCTURES CHAVES
- UI manager: global `0x140F64D78` (objeto CUIMgr; volátil).
- Tabela de janelas: `0x14138ABF0` + 0x2F8*tela.
- Objeto do jogo: `0x140F73AE0` (vetores +0x20/+0x40; lista canais +0x80).
- Manager server/canal: `0x14137DFB0` (GetChannel 0x4DEAA4).
- Conexões: `0x1413E6310` (login), `0x1413E6318` (world).
- Global state: `0x140F5DAEC` (modo usado por 0x34F878).

## 6. AUDITORIA DE COBERTURA
- Total UI elements no fluxo de login: **~14**; mapeados: 14 (100%).
- Funções descobertas/documentadas: **~22**; resolvidas: 18 (82%); pendentes: 4.
- Handlers confirmados: toggle, desconectar, selecionar servidor; prováveis: canal, EnterWorld.
- Módulos: CabalMain.exe (1). Analysis via MCP.
- XREFs: g_manager 200+ refs; evento strings 0; GUID mouse 0; closesocket 2.
- Dispatchers: canal CONFIRMED, 0x8E LIKELY, genérico UI — UNKNOWN/BLOQUEADO (exige objeto vivo).
- Vtables: CRT static/CONFIRMED; UI runtime — descubrível só com UI aberta.

## 7. DESCONHECIDOS (lacunas relevantes)
1. **Dispatcher de clique genérico** (o loop que chama std::function) — BLOQUEADO por requer objeto vivo (menu O aberto). [ver todo/backlog.md]
2. Vftable do CButton (slots OnClick/OnMouseDown) — mesmo:
3. Assinatura/object de sessão do canal (para `login_entrar_servidor(servidor, canal)` exato).
4. Handlers por ID de cada controle (tabela UI-ID→handler) — precisa janela viva.

## 8. ENTREGÁVEL — DLL
`x64\Release\CABAL_Login.dll` (compilada): `login_toggle_ui(I/O)`, `login_do_login` (teclado), `login_do_disconnect()` (0x356B30), `login_do_server_select()` (0x34F878). Compile com `build.bat`.

## 9. COMO CONTINUAR (próxima sessão)
1. Com o usuário: abrir menu O → localizar a janela do menu → ler a **vftable do CButton** vivo → slots OnClick/OnMouseDown → dispatcher genérico. (Desbloqueia TUDO: todos os botões por ID.)
2. Ou (autônomo): completar a **enumeração das classes RTTI de UI** e o **inventário de funções por tela**.
3. Mapear o **objeto de sessão do canal** para `login_entrar_servidor(servidor, canal)` como chamada única (cadeia 0x8C já mapeada).