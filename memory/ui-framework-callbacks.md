---
name: ui-framework-callbacks
description: "Mapa do framework de UI do CabalMain.exe — dispatch por eventos nomeados, tudo virtual, endereços confirmados"
metadata: 
  node_type: memory
  type: project
  originSessionId: 04c6c9c3-c44d-436a-8f7f-168dfb529690
  modified: 2026-08-09T13:18:20.048Z
---

# Framework de UI do CabalMain.exe (EP33, base 0x140000000, empacotado Themida)

Investigado 2026-08-08 (sessão dedicada à RE dos callbacks). Confirmado ao vivo via MCP connect-game.

## IDENTIDADE DO FRAMEWORK: "Snake UI" (namespace Snake) — RTTI COMPLETO presente!
- O cliente tem **RTTI MSVC completo** no .rdata (nomes `.?AV...@@`). As classes de UI estão em ~0x140efe000+.
- Namespace: `Snake::UI::Event::{EventArgs, MouseEventArgs, EventSet, ...}`.
- **Eventos = `std::function<bool(const EventArgs&)>`** registrados num `EventSet` (tipo_info 0x140f06250). O dispatch chama os handlers std::function.
- Classes de controle (prefixo C, herança C-style): **CButton** (type_info 0x140f0ad08), **CButtonGroup** (0x140f0f588), CDialog (0x140f06208), CInfoDialog (0x140f062b0), CCtrlEntity (0x140f05c40), CChildHolderEntity (0x140f06220), CTabPanel (0x140f05c68).
- Names RTTI relevantes achados: `.?AVCButton@@`, `.?AVCButtonGroup@@`, `.?AVEventSet@Event@UI@Snake@@`, `.?AVMouseEventArgs@Event@UI@Snake@@`, `.?AVCDialog@@`, `.?AVCCtrlEntity@@`, `.?AVCChildHolderEntity@@`, `.?AVCTabPanel@@`.
- Layout do type_info (padrão MSVC): vftable ptr em type_info+0x00 (=0x140D16580 p/ todos), spare +0x08, nome +0x10.
- Nenhuma classe "UIManager" no RTTI (o manager global base+0x140F64D78 pode ser CWindowManager ou equivalente — a confirmar).

## Como achar a VTABLE de uma classe (próximo passo)
- Via `_RTTICompleteObjectLocator` (COL): estrutura de 24 bytes `{sig=1, offset=0, cdOffset=0, pTypeDescriptor(rel), pClassDescriptor(rel), pBaseClassArray(rel)}`, armazenada IMEDIATAMENTE ANTES da vftable. Codificação x64: `target = &campo + campo` (32-bit signed).
- pTypeDescriptor aponta pro type_info. A vftable = COL + 0x18.
- COL do CButton → vftable do CButton → métodos virtuais (OnClick/OnMouseDown/ProcessInput = o dispatch de clique).

## DESCOBERTA ARQUITETURAL DEFINITIVA (2026-08-08)
- **As VTABLES de classe e as tabelas de dispatch são construídas EM RUNTIME** (Themida decifra no heap). Confirmado: `aob_scan` de QUALQUER endereço de função como qword retorna 0 no módulo (ex.: GetProperty 0x14086CE50, socket-close 0x140389484). Mas o RTTI (type_infos, `0x140D16580` em 3522 lugares) É estático.
- **O CÓDIGO do dispatch É estático no .text** — só as chamadas são indiretas (via runtime tables). Logo: o dispatch existe em .text e pode ser achado, só não via `find_callers`.
- Implicação: para achar o dispatch, não adianta procurar tabelas estáticas. Caminhos:
  1. **Posição do mouse global** (lida pelo hit-test) → xref → função do dispatch.
  2. **Código que invoca o `std::function` do EventSet** (padrão `mov rax,[fn]; call [rax+off]`) → a função que dispara o evento.
  3. RTTI COLs → vftables runtime (precisa achar o COL estático e ler a vftable decifrada).

## Arquitetura (confirmada)
- O framework de UI é **baseado em EVENTOS NOMEADOS** (não callbacks simples). Nomes de evento em .rdata em `0x140cb2778`+:
  `EventMouseMove, EventMouseWheel, EventMouseButtonDown, EventMouseButtonUp, EventMouseClick, EventMouseLButtonClick` (0x140cb27d8), `EventMouseRButtonClick, EventMouseEnter, EventMouseLeave, EventFrameMove, EventShow, EventHidden, EventKeyUp, EventKeyDown, EventOnCharacter, EventPostRender, EventPost2DFx, EventOnChecked, EventClickOutOfCtrl, EventBtnClick` (0x140cb28d8), `EventHideDlg`.
- **TODOS os métodos do framework são VIRTUAIS** — `find_callers` retorna 0 para toggle_ui, para a iteração de filhos, para o disconnect etc. Não dá para rastrear o dispatch por callers estáticos. O dispatch é indireto (vtable/function-pointer), consistente com o sistema de eventos.
- **Janelas são objetos no heap com vtable em [obj]**. Filhos via virtual `[vtable+0x120] = GetChild(this, index, r8b=true)` (padrão visto em 0x14043F700 e em 0x14000dc30, que itera filhos do container).
- Objetos de botão/janela guardam ponteiros para o nome do evento (`EventBtnClick`) e para o rótulo do texto — o dispatch procura o handler pelo NOME do evento.

## Endereços confirmados (estáveis, no módulo)
- `toggle_ui`: base+0x549FD8 — assinatura `(this=base+0x138ABF0, mode)`; mode 0=inventário(I), 2=menu(O). Sem chamadores diretos (indireto).
- Tabela de janelas: base+0x138ABF0 — registros de 0x2F8 bytes indexados por mode (0=inv, 2=menu). Registro do menu (mode 2): `0x14138ABF0 + 0x2158 + 2*0x2F8 = 0x14138D338`.
- `GetProperty`: 0x14086CE50 — `(out*, propId)` copia 16 bytes da tabela de propriedades em 0x140F72090. 29 chamadores diretos (função concreta, não virtual).
- UI manager global: `0x140F64D78` (ponteiro — **valor muda/volátil** durante o jogo).
- `Disconnect` (fecha as DUAS conexões): `0x140356B30`. Conexões em `0x1413E6310` e `0x1413E6318`. Seta `[conn+0x88]=0x0A`. `this` só é usado para `[this+0x458]` (release opcional, testado jz) → **pode ser chamado com `this` zerado** (buffer da DLL).
- Wrapper de close do socket: 0x140389484 (shutdown+closesocket). `CloseConnection`: 0x1403893EC (fecha os 2 sockets do objeto). Chamadores: 0x140356B30, 0x140357428, 0x14034F8A1.
- Hook ENCRYPT (captura SEND plaintext): base+0x389698.
- Bypass anti-hack: patch 0x352CDB (jne->jmp), opcional 0x34D1AD.
- Rótulo "Desconectar" (wide) em dados .els mapeados: 0xdf0732 (heap baixo ~14MB, muda por processo).

## Bloqueio atual
- As strings de evento NÃO têm referência de código (nem RIP-rel, nem mov imm64) — são referenciadas só por objetos no heap (runtime). O registro evento→handler é construído em runtime.
- Como tudo é virtual, o próximo passo precisa ser **OBSERVAÇÃO**: hookar funções conhecidas (0x140356B30, 0x140389484, ENCRYPT, toggle_ui) e logar call stack quando o usuário clicar num botão — isso revela a cadeia de dispatch concreta (qual função chama o disconnect quando você clica "desconectar").
- Alternativa: achar a vtable da janela do menu (objeto do heap) lendo a lista de janelas do UI manager com o menu O ABERTO — precisa do usuário com o menu aberto.
- Ver [[tool-reliability]] para quais tools funcionam.

## O que isso permite já
- `login_toggle_ui(2)` abre/fecha o menu O (já na DLL).
- Chamar `0x140356B30(0)` = **desconectar** (fecha login+world, volta à tela de login) — mesmo efeito de clicar "desconectar"+"sim". Na DLL: `login_do_disconnect()`.
- **"selecionar servidor" = `0x14034F878(void)`** — CONFIRMADO: lê `0x140F5DAEC`, seta estado do servidor, fecha SÓ conn2/world (0x1413E6318). Na DLL: `login_do_server_select()`. (Corrige o site 0x34F890: entrada real é 0x34F878.)
- **Handler do botão "Selecionar Servidor" (menu O) = `0x140351248`** — CONFIRMADO runtime: `(this)` checa `0x140F5DAEC` → chama `0x34F878`. Fluxo validado: Selecionar Servidor → Sim → Desconectar → login (modo global 0x140F5DAEC = 0).
- Ações dos diálogos "Sim" = o handler da ação; a DLL chama a ação direto (sem clicar Sim).
- Conexões: conn1 `0x1413E6310` (login), conn2 `0x1413E6318` (world). Global state: `0x140F5DAEC`.
- Fechar conexão: `0x1403893EC`. Wrapper socket: `0x140389484`.
- DLL compilada (x64\Release\CABAL_Login.dll): toggle_ui + login_do_disconnect + login_do_server_select.

## Input protegido pelo VM do Themida
- O handler de input (teclado/mouse) está na região do VM (0x141A00000+), código ofuscado (não é assembly legível). Por isso `find_callers`/xref não acham o dispatch e checagens de tecla (F6 4X 18 80) caem em lixo do VM.
- DirectInput: GUIDs do mouse/teclado em 0x140cdd060+ (SysMouse=0x61 em 0x140cdd0a0; teclado via SysKeyboardEm 0x81). GUIDs sem xref de código (chamadas indiretas).

## SONDA DE OBSERVAÇÃO (pronta)
- `D:\projeto\CABAL_Login\probe\` (probe.c + hookstub.asm + build_probe.bat) → `x64\Release\probe.dll`.
- Hook one-shot em 0x549FD8 (toggle_ui) e 0x356B30 (disconnect): loga **return address + cadeia rbp + scan da pilha** em `D:\projeto\CABAL_Login\probe.log`, restaura o prologo original e re-entra (o jogo segue normal). Isso revela a cadeia de dispatch em assembly quando o usuário clicar.
- **BLOQUEIO**: VirtualAllocEx no processo rodando falha com erro 5 (anti-cheat/Themida protege o processo DEPOIS de inicializar). O injector `connect_game_inject.exe` falha em processo protegido "por design". A `connect_game.dll` entrou no LANÇAMENTO do jogo.
- → Para observar: o usuário precisa REINICIAR o jogo e injetar a sonda CEDO (junto com o connect_game), antes do anti-cheat travar. Depois: abrir o menu O e clicar nos botões → ler probe.log.
