# FLUXO_UI.md — Mapa completo das chamadas de UI do CabalMain.exe (EP33)

> Compilado 2026-08-08 via MCP connect-game + RTTI + desassembly.
> Objetivo: saber ONDE está cada botão/clicável e COMO chamá-lo de dentro do jogo (a DLL).
> Endereços com base 0x140000000 (CabalMain.exe, Themida). Conexões: conn1=0x1413E6310 (login), conn2=0x1413E6318 (world).

## Arquitetura do framework (resumo)
- UI = **"Snake UI"** (namespace Snake), C++, RTTI completo no .rdata (~0x140efe000+).
- Botão = classe **CButton** (type_info 0x140f0ad08). Janelas = CDialog / CInfoDialog / CCtrlEntity / CChildHolderEntity / CTabPanel.
- Cada controle tem um **EventSet** (`Snake::UI::Event`) = mapa de handlers `std::function<bool(const EventArgs&)>`.
- O clique é disparado: input → UI manager acha o controle sob o cursor → invoca o handler do EventSet → roda a AÇÃO.
- **Handler de input está no VM do Themida** (0x141A00000+, ofuscado) — não desmonta como assembly limpo.
- **Vtables são runtime** (heap); os MÉTODOS que elas apontam estão em .text limpo (seções 1-3).

---

## FLUXO DE LOGIN — tela por tela, botão por botão

### 1) TELA DE LOGIN
Botões/clicáveis:
| Elemento | Rótulo (memória) | Posição (login.cfg) | Ação |
|---|---|---|---|
| Campo ID | — | — | digitar usuário |
| Campo Senha | — | — | digitar senha |
| **Entrar** (login) | "Entrar" wide @0x463f13e0 | entrar 1280 727 | envia auth (0x65 Connect2Svr + 0x67 AuthAccount) |
| (Enter) | — | — | mesmo efeito do Entrar |

**Como a DLL faz:** digitando user/Tab/senha/Enter (SendInput scancode) — **JÁ FUNCIONA** (independente de resolução).
Ação de fundo: conectar ao login server (0x65), CheckVersion (0x7A), PreServerRequest (0x7D2, envia USERNAME), PublicKey (0x7D1), AuthAccount (0x67), desafio 0x7D6.

Resultado → tela de SELEÇÃO DE SERVIDOR (Mercúrio/Vênus + canais).

### 2) SELEÇÃO DE SERVIDOR
Botões/clicáveis:
| Elemento | Rótulo | Posição | Ação |
|---|---|---|---|
| **Mercúrio** | (nome do servidor) | servidor 2222 399 | seleciona servidor → mostra os canais |
| **Vênus** | (nome do servidor) | — | idem |
| **Canal** (1-9) | (número do canal) | canal 2266 742 | conecta ao world (envia **0x8C**) |
| (setas + Enter) | — | — | navegação por teclado **FUNCIONA** |

**Ação do canal:** handler ~0x140A01A00 — monta e envia `0x8C` (Connect2Svr world) + `0x48C` + `0x29C`. 
Builders do 0x8C achados: `0x1401f1aa6` (envio genérico, opcode no stack arg) e `0x140a01b34` (fluxo de canal).
Outros builders do 0x8C: 10 sites no total (sessão anterior).

**MECANISMO DO CANAL (0x8C) — detalhado:**
- O builder do 0x8C (`0x140740f82`) lê **`[objeto+0x198]` = índice do canal selecionado** e o **gerenciador de servidores/canais `0x14137DFB0`** (via `0x1404DEAA4` = GetChannel(índice)).
- O índice é procurado no gerenciador; se ok, chama método do UI manager (`[0x140F64D78]->0x140448924`) com o canal.
- **Objeto global do jogo (this): `0x140F73AE0`** — struct grande (vetores {ptr,count}).
- **Dispatch de canal `0x1404A49D4(0x140F73AE0, canal)`** — mapeia canal→opcode/dado:
  - canal 1→0x3CE, 2→0x3CD, 3→0x3CF, 4→0x3D0, 5→0x3D1, 6→0x3D2 (opcode de lobby)
  - canal 7→`0x140C98BF8`, 8→`0x140C98BFC`, 9→`0x140C98C00`, 10+→`0x140C3ECE8` (ponteiros de dados)
- Depois chama `0x1404A5368` = findChannel (busca o nó do canal na lista `[0x140F73AE0+0x80]`).
- O envio real do 0x8C usa `0x1404A5368(0x140F73AE0, opcode, canal)` + `0x14005B898`.
- Resumo do caminho: `clique no canal → estado [obj+0x198]=canal → GetChannel → dispatch(opcode) → findChannel → enviar 0x8C`.
- **Para "entrar no Mercúrio canal 9" pela DLL: setar servidor + canal no objeto do jogo + disparar o connect. Os campos exatos de "servidor selecionado" e "canal selecionado" são os próximos a confirmar ao vivo (tela de seleção aberta).**

Resultado → tela de PERSONAGEM.

### 3) SELEÇÃO DE PERSONAGEM
| Elemento | Rótulo | Posição | Ação |
|---|---|---|---|
| Lista de chars | (nomes) | — | setas seleciona |
| **Iniciar** | (iniciar) | comeca 2427 989 | confirma char → envia **0x8E** (EnterWorld) |
| (Enter) | — | — | **FUNCIONA** via teclado |

**Ação do Iniciar:** builders do 0x8E (11 sites): `0x1405643A3`, `0x1407222B3`, `0x14080A0B2` + cluster 0x140AC7267 (dispatch que retorna 0x8E).
Entra no mundo → ler ouro/nível/nome (base+0x137F170/0x137F1B8/0x137F190).

### 4) NO MUNDO → MENU O
| Elemento | Rótulo | Posição | Ação |
|---|---|---|---|
| Tecla O | — | — | toggle_ui(mode=2) = base+0x549FD8 |
| Tecla I | — | — | toggle_ui(mode=0) = inventário |
| Menu → **Selecionar Servidor** | "Selecionar Servidor" | selecionar_servidor 1280 482 | handler **0x140351248**(this) → checa modo 0x140F5DAEC → chama **0x14034F878()** (fecha mundo) |
| Menu → **Desconectar** | "Desconectar" wide @0xdf0732 | desconectar 2315 992 | handler 0x140356B30: fecha AS DUAS conexões → tela de login |
| Menu → outros itens | (labels no .els) | — | cada item = CButton com handler próprio |
| Diálogo confirmação → **Sim** | "Sim" | sim 1295 639 | confirma a ação do diálogo (chama o handler da ação) |
| Diálogo → **Não** | "Não" | — | cancela |

**NOTA CRÍTICA:** os itens do menu O e o "sim"/"não" **NÃO respondem a teclado** (clique é via EventSet, não tem foco de teclado nesses diálogos). É PRA ISSO que a DLL precisa chamar a ação direto.

---

## FUNÇÕES DE AÇÃO (chamáveis pela DLL)
| Ação | Função | Assinatura/notas |
|---|---|---|
| Desconectar (fecha as 2 conns) | **0x140356B30** | `(this)` — this só usado p/ [this+0x458] (release opcional, jz se null). Pode chamar com this=0 (buffer zerado). Seta [conn+0x88]=0x0A. |
| Desconectar (variante) | **0x140357428** | quase idêntica; call extra em 0x14138E840. |
| Selecionar servidor (fecha só world) | **~0x14034F890** | fecha conn2 0x1413E6318; lê dados de servidor 0x59BDA20/0x598CCE0; `ecx==6` escolhe qual. |
| Abrir menu/inventário | **0x140549FD8** (toggle_ui) | `(this=base+0x138ABF0, mode)`; mode 0=I, 2=O. |
| Fechar conexão | **0x1403893EC** | `(conn_obj)` — fecha os 2 sockets do objeto. |
| Fechar socket | **0x140389484** | wrapper shutdown+closesocket. |
| Enviar pacote genérico | **0x1403767F4** | usado pelos builders (opcode no stack arg). |
| Conectar canal (0x8C) | **0x140A01B34** | fluxo completo (0x8C+0x48C+0x29C) — handler do clique no canal. |
| EnterWorld (0x8E) | 0x1405643A3 / 0x1407222B3 / 0x14080A0B2 / 0x140AC7267 | builders do 0x8E (11 sites). |
| ENCRYPT (hook de envio) | base+0x389698 | captura SEND plaintext. |

---

## ONDE ESTÃO OS BOTÕES (CButton)
- Cada botão = objeto **CButton** no heap com: rótulo (label wide do .els), EventSet (handlers std::function), ID.
- Rótulos de texto do .els ficam mapeados em endereços baixos (ex.: "Desconectar" wide @0xdf0732, "Entrar" @0x463f13e0).
- Tabela de janelas do UI manager: base+0x138ABF0, registros de 0x2F8 por tela (state-1 indexado). Registro da tela atual tem vetores de heap com dados de layout (posições/tamanhos).
- Para mapear TODOS os clicáveis: enumerar as classes RTTI do UI (CButton, CDialog, etc.) + vftables runtime → métodos virtuais de clique.

## PRÓXIMOS PASSOS (rastreabilidade completa)
1. Achar a **vftable do CButton** (RTTI COL → vftable runtime) → métodos OnClick/OnMouseDown/ProcessInput em .text limpo → o dispatch exato.
2. Mapear os **"Sim"/"Não"** dos diálogos (cada diálogo = CDialog com botões CButton; achar o handler do "Sim" por diálogo).
3. Enumerar todos os CButton vivos (via tabela de janelas/estado) por tela → lista completa de clicáveis.
4. Fechar o loop: DLL chama as funções de ação acima (desconectar, selecionar servidor) em vez de coordenadas/teclado.
