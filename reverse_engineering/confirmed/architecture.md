# Arquitetura confirmada — framework de UI "Snake UI"

> Todas as descobertas CONFIRMED abaixo foram validadas por desassembly/leitura via MCP (sessões 2026-08-08).

## Framework
- Cliente = **CabalMain.exe** (base 0x140000000, Themida). UI = **"Snake UI"** (namespace `Snake::UI`), C++.
- **RTTI MSVC completo** no .rdata (~0x140efe000+): nomes de classe + type_infos.
- **CORREÇÃO (2026-08-08): vtables ESTÁTICAS EXISTEM.** O type_info vftable `0x140D16580` é real — o constructor `0x140B03130` faz `lea rax,[0x140D16580]; mov [rcx],rax`. A conclusão anterior ("vtables runtime") estava ERRADA: os aob_scan que retornaram 0 eram de funções que NÃO são entradas de vtable (ex.: toggle_ui, disconnect, GetProperty são funções normais ou lambdas). → análise estática de vftables É VIÁVEL.
- Classes: **CButton** (type_info 0x140f0ad08), CButtonGroup (0x140f0f588), CDialog (0x140f06208), CInfoDialog (0x140f062b0), CCtrlEntity (0x140f05c40), CChildHolderEntity (0x140f06220), CTabPanel (0x140f05c68).
- Eventos: `Snake::UI::Event::{EventArgs, MouseEventArgs, EventSet}`. Cada controle tem **EventSet** = mapa de handlers `std::function<bool(const EventArgs&)>`.
- Strings de evento no .rdata: `EventBtnClick` (0xCB28D8), `EventMouseLButtonClick` (0xCB27D8), EventMouseMove etc (0x140cb2778+).

## Dispatch (input → UI)
- UI manager global: **0x140F64D78** (ponteiro; valor VOLÁTIL runtime).
- Tabela de janelas: base+**0x138ABF0** — registros de 0x2F8 por tela (index por `[mgr+0x30EC]-1`).
- toggle_ui: **0x549FD8** `(this=0x14138ABF0, mode)` mode 0=I, 2=O.
- `GetProperty`: 0x86CE50 (tabela 0x140F72090). GetChild: vtable+0x120.
- **Vtables runtime (heap)** — aob_scan de endereço de função como qword retorna 0; execução é indireta.
- **Input handler no VM Themida** (0x141A00000+): ofuscado, não desmonta limpo.

## Rede / sessão
- Conexões: conn1=**0x1413E6310** (login), conn2=**0x1413E6318** (world).
- **disconnect** (fecha 2): **0x356B30** (this p/ [this+0x458], zerado => ok). Variante: 0x357428.
- **selecionar_servidor** (fecha world): ~**0x34F890** (assinatura a confirmar).
- close_connection: **0x3893EC**. socket_close: **0x389484**. ENCRYPT hook: **0x389698**.
- Objeto global do jogo: **0x140F73AE0** (vetores +0x20/+0x40; lista canais +0x80).
- Manager server/canal: **0x14137DFB0** (GetChannel via 0x4DEAA4; vetor atualmente vazio).
- Channel dispatch: **0x4A49D4** (canal 1-6→opcode 0x3CE-0x3D2; 7→0x140C98BF8, 8→0x140C98BFC, 9→0x140C98C00). findChannel: **0x4A5368**.
- 0x8C builder (conecta canal): **0x740F82** (lê [obj+0x198]=canal) / 0x1F1AA6 / 0xA01B34.
- 0x8E builder (EnterWorld): 0x5643A3 / 0x7222B3 / 0x80A0B2; dispatch 0xAC7267.
- 0x65/0x67/0x8C/0x8E: endereços completos em `strategy.md` (sessão anterior).

## UI (labels/states)
- Labels .els wide: "Desconectar" @0xdf0732, "Entrar" @0x463f13e0, "Mercúrio" @0x37669460.
- A seleção de servidor/canal NÃO altera [game-obj 0x…0x1FF] — é estado de UI/lista de sessão (negativo confirmado).
- `{0x8, 9}` em 0x4644AF68/6C é CONSTANTE (Mercúrio canal 9 vs Vênus canal 2 iguais) — NÃO é o canal (confirmação negativa).