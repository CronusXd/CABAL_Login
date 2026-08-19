# CABAL Login — Automação EP33

Automação em Python puro para login em massa no CabalMain.exe EP33.
Suporta 1600+ contas com fluxo completo: login → Venus → screenshot → Mercury → desconectar.

## Requisitos

- Python 3.13+
- CabalMain.exe EP33 aberto e visível
- Arquivo `CABAL BR SUB.txt` com as contas (formato: `usuario|senha|subsenha|nivel`)
- Imagens de referência na pasta `imagens/`

## Instalação

```bash
pip install -r requirements.txt
# ou manualmente:
pip install opencv-python mss Pillow keyboard
```

## Configuração

1. Copie `login.cfg.example` para `login.cfg`
2. Abra o jogo e posicione a janela
3. Execute `python login.py` — a GUI abre e detecta as coordenadas automaticamente
4. Ajuste as coordenadas na GUI se necessário

## Uso

```bash
# Com GUI (recomendado)
python login.py

# Modo direto (sem GUI, precisa de login.cfg)
python login.py --start
```

## Fluxo do Automação

```
START
  │
  ▼
╔═════════════════════════════════════╗
║  FASE 1: Login Venus               ║
║  1. Click pre-login                ║
║  2. Digitar usuario + senha        ║
║  3. Selecionar canal Venus (IMAGEM)║
║  4. Click ENTRAR                   ║
║  5. Verificar nivel → Screenshot   ║
╚═════════════════════════════════════╝
  │
  ▼
╔═════════════════════════════════════╗
║  FASE 2: Mercury (Canal Only)      ║
║  1. O → Selecionar Servidor → sim  ║
║  2. Garantir Mercury visível       ║
║  3. Procurar Canal_Mercury         ║
║  4. Click ENTRAR → Screenshot      ║
╚═════════════════════════════════════╝
  │
  ▼
╔═════════════════════════════════════╗
║  FASE 3: Desconectar               ║
║  O → Selecionar Servidor → sim     ║
║  → Desconectar → Próxima conta     ║
╚═════════════════════════════════════╝
```

## Imagens de Referência

Todas na pasta `imagens/`:

| Imagem | Uso | Threshold |
|--------|-----|-----------|
| `Canal_venus.jpg` | Canal de Venus | 90% |
| `Canal_Mercury.jpg` | Canal de Mercury | 60% |
| `venus.jpg` / `venus2.jpg` | Servidor Venus (hover) | 90% |
| `Mercury.jpg` / `Mercury2.jpg` | Servidor Mercury (hover) | 90% |
| `nivel.jpg` | Tela de nível | 90% |
| `sim.jpg` | Botão "Sim" | 90% |
| `Selecionar Servidor.jpg` | Menu servidor | 90% |
| `0.jpg` - `9.jpg` | Dígitos da subsenha | 90% |

## Regras de Verificação de Servidor

- **Venus**: SOMENTE `venus`/`venus2` visível. ZERO `Mercury`/`Mercury2` na tela.
- **Mercury**: `Mercury`/`Mercury2` visível + `Canal_venus` NÃO visível.

## Estrutura do Projeto

```
CABAL_Login/
├── login.py              # Script principal (~1500 linhas)
├── login.cfg.example     # Template de configuração
├── run_login.bat         # Atalho pra rodar
├── imagens/              # Imagens de referência
│   ├── Canal_venus.jpg
│   ├── Canal_Mercury.jpg
│   ├── venus.jpg / venus2.jpg
│   ├── Mercury.jpg / Mercury2.jpg
│   ├── nivel.jpg
│   ├── sim.jpg
│   ├── Selecionar Servidor.jpg
│   └── 0-9.jpg
├── memory/               # Memória do projeto
├── reverse_engineering/  # Documentação RE
├── _archive/             # Código antigo (DLL/C)
├── FLUXO_ATUALIZADO.txt  # Diagrama de fluxo completo
├── FLUXO_LOGIN.md        # Documentação do fluxo
├── FLUXO_UI.md           # Mapa da UI
└── PROJECT.md            # Visão geral do projeto
```

## Variáveis de Configuração (login.cfg)

| Variável | Default | Descrição |
|----------|---------|-----------|
| `wait_action` | 2.0s | Pausa padrão após ação |
| `wait_login` | 6.0s | Após Enter de login |
| `wait_server` | 4.0s | Após clicar Venus |
| `wait_char` | 5.0s | Após confirmar canal |
| `wait_sub` | 2.5s | Janela de subsenha |
| `wait_world` | 5.0s | Após Comeca+subsenha |
| `wait_back` | 15.0s | Voltar da desconexão |
| `double_enter` | 1 | Enter duplo no login |
| `logout` | 1 | Voltar pro login |

## Troubleshooting

- **Imagem não encontrada**: Verifique se a janela do jogo está visível e não minimizada
- **Hover troca cor**: O script move o mouse pro centro antes de cada busca automaticamente
- **Canal_Mercury difícil**: Threshold reduzido pra 60% por causa do match difícil
- **Espera insuficiente**: Ajuste `wait_world` e `wait_char` no `login.cfg`

## Licença

Uso pessoal — automação para conta própria.
