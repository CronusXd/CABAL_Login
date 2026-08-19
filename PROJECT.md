# CABAL_Login — Documentação do Projeto (Python puro, sem DLL)

> **Projeto**: Script Python (stdlib/ctypes) para automação de login sequencial no CabalMain.exe EP33  
> **Objetivo**: Processar 1600 contas do arquivo `Cabal BR SUB.txt` mantendo o cliente aberto (logout → próxima conta)  
> **Estado**: fluxo implementado; validando passo a passo com coordenadas reais

---

## Arquitetura atual (2026-08-17)

**Python puro** — zero injeção, zero compilação, zero dependências pip.

O script `login.py` opera o jogo de fora como um humano: envia cliques e teclas
via `SendInput` do Windows. O anti-cheat (XignCode3/Themida) vê apenas input
normal do sistema — nenhum módulo estranho no processo do jogo.

### Componentes
- `login.py` — script único com: parse de config/contas, envio de input via ctypes, loop de contas, modo de mapeamento F9, verificação opcional via ReadProcessMemory
- `login.cfg` — coordenadas (pixels) dos 7 botões + tempos de espera (segundos)
- `Cabal BR SUB.txt` — formato BR (Conta N / Usuario: / Senha: / subsenha: / NV:)
- `run_login.bat` — launcher para duplo clique

---

## O que era C e agora é Python (ou eliminado)

| Antes (C, src/login.c) | Agora (Python, login.py) | Observação |
|-------------------------|--------------------------|------------|
| `login_toggle_ui(I/O)` — chamada direta a base+0x549FD8 | `press_key(VK_I)` / `press_key(VK_O)` | Tecla I/O equivalente ao toggle do jogo |
| `login_do_disconnect()` — chamada base+0x356B30 | Coordenada `desconectar` no menu | Precisa mapear com F9 |
| `login_do_server_select()` — chamada base+0x34F878 | Coordenada `selecionar_servidor` | Precisa mapear com F9 |
| `login_conn1_socket()` / `login_wait_*` — leitura de memória | `time.sleep(wait_*)` — tempo fixo configurável | Mais simples; sem risco de anti-cheat |
| `login_verify_enriched()` — USERDATACONTEXT + legacy | `verify_account()` via ReadProcessMemory (legado) | Opcional, desligado por padrão |
| `recvwatch.c` — hook de recv + classificação por rede | Eliminado | Classificação agora é por posição no fluxo (não há leitura de rede) |
| `login_dll.c` + injeção via CreateRemoteThread | Eliminado — nada é injetado | Jogo abre normalmente |

### Código C movido para `_archive/`
- `src/` (login.c, login.h, login_dll.c, recvwatch.c/h)
- `probe/`, `tests/`, build scripts, obj files, DLLs
- Recuperável; o código de referência (USERDATACONTEXT offsets, crypto) está em `reverse_engineering/`

---

## Validação em andamento (2026-08-17)

**Método**: validar cada passo com console imprimindo a ação +≥2s entre ações.

### Coordenadas confirmadas (login.cfg atual)
| Botão | Coordenadas (pixels) |
|-------|---------------------|
| VENUS | (2225, 340) |
| CANAL 9 | (2218, 557) |
| ENTRAR | (2431, 989) |
| COMEÇA | (2433, 987) |

### Subsenha por reconhecimento de imagem (implementado 2026-08-17)
- Pasta `imagens/` com referências 0-9.jpg (32-42px, RGB)
- **`8.jpg` AUSENTE** — precisa ser criada/colocada
- Fluxo: Click COMEÇA → espera 2s → screenshot → busca digitos na tela → clica na ordem da subsenha → clica OK
- Detecção: 3 passes — (1) busca dígit 0 para achar origem do grid, (2) grid relativo, (3) fallback busca individual
- Config no login.cfg: ok_x/ok_y (botão OK), grid_x/grid_y (origem grid), cell_w/cell_h (tamanho célula), grid_cols/grid_rows (dimensões do grid), cell_gap (espaçamento)

### Coordenadas a mapear (ainda 0,0 — precisa rodar `python login.py map`)
| Botão | Onde aparece |
|-------|-------------|
| selecionar_servidor | Menu O (tecla O no mundo) |
| sim | Diálogo de confirmação |
| desconectar | Menu O ou tela de seletor |

### Tempos de espera (login.cfg)
| Parâmetro | Default | Descrição |
|-----------|---------|-----------|
| wait_action | 2.0s | Pausa mínima entre ações |
| wait_login | 6.0s | Após login → clique no servidor |
| wait_server | 4.0s | Após VENUS → selecionar canal |
| wait_char | 8.0s | Após canal → tela de personagem |
| wait_world | 15.0s | Após subsenha → mundo carregado |
| logout | 1 | 1=continua loop; 0=para apos 1a conta |

---

## Próximos passos

1. **Validar o fluxo no jogo**: `python login.py run --wait` → F8 → acompanhar cada passo no console. O loop agora para apos a 1a conta (`logout 0` no login.cfg ou sem coords de logout mapeadas).
2. **Mapear os 3 botões do logout** com F9: `python login.py map` → navegar no jogo para cada tela e mapear `selecionar_servidor`, `sim`, `desconectar`.
3. **Testar com 10 contas**: colocar `login.n 10`, ativar `logout 1`, rodar e verificar `output.txt`.
4. **Escala 1600 contas**: remover `login.n` (ou colocar 1600), rodar `run_login.bat`, monitorar via logs e `login.stop`.

---

## Arquivos de memória

| Arquivo | Conteúdo |
|---------|----------|
| `memory/MEMORY.md` | Índice (carregado toda sessão) |
| `memory/login-flow-state.md` | Estado do fluxo e decisões |
| `memory/ui-framework-callbacks.md` | Framework Snake UI, RTTI, vtables |
| `memory/tool-reliability.md` | Ferramentas MCP confiáveis |
| `memory/cabalreverse-source.md` | USERDATACONTEXT, crypto, opcodes |
| `reverse_engineering/` | Inventário completo de funções (referência) |

---

*Documento atualizado: 2026-08-17 — migração completa para Python puro.*
