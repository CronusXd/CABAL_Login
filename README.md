# CABAL_Login — Login automático sequencial em Python puro (sem injeção)

Script Python (stdlib, sem dependências pip) que automatiza o login de contas do
arquivo **`Cabal BR SUB.txt`** no cliente `CabalMain.exe` usando exclusivamente
**input de sistema** (cliques + teclado via `SendInput`). Nenhum módulo é injetado
no processo — o anti-cheat (XignCode3/Themida) não vê nada estranho.

## Como usar

```bat
# 1. Valida sem enviar nada (checa config + contas)
python login.py --check

# 2. Mapeia os botões no jogo (passa o mouse e aperta F9)
python login.py map

# 3. Roda o fluxo completo
python login.py run --wait      # --wait aguarda o jogo abrir
#   ou duplo clique em run_login.bat
```

No jogo: **F8** inicia o fluxo. Não clique fora do jogo durante a execução.

## Fluxo por conta

1-3. digita usuario / Tab / senha / Enter (×2)
4. clique em VENUS/MERCURIO
5. clique no canal + Enter
6. clique ENTRAR
7. clique COMEÇA (abre subsenha)
8-9. subsenha + Enter
10. aguarda mundo carregar
11-12. inventário (I) → menu (O) → selecionar servidor → Sim → desconectar
13. volta ao login → próxima conta

Todos os passos têm **≥ 2 segundos de espera** entre si (configurável em `login.cfg`).

## Arquivos

| Arquivo | Descrição |
|---------|-----------|
| `login.py` | Script principal — tudo acontece aqui |
| `login.cfg` | Coordenadas dos 7 botões (pixels) + tempos de espera (segundos) |
| `Cabal BR SUB.txt` | Contas (`Conta N / Usuario: / Senha: / subsenha: / NV:`) |
| `run_login.bat` | Atalho para `python login.py run --wait` |
| `login.current` | Índice da conta atual (criado/editado manualmente) |
| `login.n` | (opcional) limite de contas a processar |
| `login.stop` | (crie p/ pausar) — o script para antes da próxima conta |
| `login.go` | (crie p/ iniciar sem F8) — o script dispara sozinho |
| `output.txt` | Linha por conta processada com status |
| `login_<idx>.log` | Log detalhado passo-a-passo de cada conta |
| `_archive/` | Código C/objeto do sistema antigo (recuperável) |

## Configuração

Veja `login.cfg.example` para a lista completa. Ajustes podem ser feitos no
arquivo ou via modo de mapeamento (recomendado):

- **wait_action** — pausa base entre ações (mínimo 2s)
- **wait_login** — tempo para o servidor de login responder
- **wait_server** — tempo para a lista de canais carregar
- **wait_char** — tempo para a tela de personagem aparecer
- **logout** — 1 (continua loop) ou 0 (para após a 1ª conta — para validar)
- **verify** — 1 (ler nivel/ouro via ReadProcessMemory; opcional, desligado por padrão)

## Validação (fase atual)

Validando passo a passo com o jogo rodando. O que já foi confirmado:
- Coordenadas de VENUS / Canal 9 / ENTRAR / COMEÇA (login.cfg atual)
- SendInput funciona corretamente (clique em pixels + teclado direto)
- Fluxo do C portado 1:1 para o loop Python (mensagens de console a cada passo)

O que falta validar ao vivo:
- Caminho completo de retorno ao login (precisa mapear `selecionar_servidor` + `sim` + `desconectar` com F9)
- Tempos de espera para máquinas mais lentas (ajustar `wait_*` se necessário)

## Nota técnica

O script usa `ctypes` direto do Python para enviar `SendInput` (INPUT_MOUSE
para cliques, INPUT_KEYBOARD com KEYEVENTF_UNICODE para digitação). O foco da
janela do jogo é forçado via `SetForegroundWindow` antes de cada sequência de
teclado. Nenhuma leitura de memória do processo é feita por padrão (modo
`verify 0` — opcional via ReadProcessMemory, desligado para evitar alerta do
anti-cheat).

---

*Última atualização: 2026-08-17 — migração completa do C (DLL injetada) para Python puro.*
