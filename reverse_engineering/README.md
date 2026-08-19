# /reverse_engineering — Base de conhecimento do CabalMain.exe (EP33)

> RE contínua: UI → Evento → Handler → Função → Subfunções → Assembly → Efeito.
> **REGRA**: nunca reiniciar. Ler estes arquivos antes de qualquer investigação nova.

## Navegação
- [functions/inventory.md](functions/inventory.md) — inventário de funções (endereço, status, confiança)
- [ui/ui_map.md](ui/ui_map.md) — mapa de elementos da UI (tela → clicável → handler)
- [confirmed/architecture.md](confirmed/architecture.md) — arquitetura confirmada (Snake UI, RTTI, canal, dispatchers)
- [todo/backlog.md](todo/backlog.md) — TODO priorizado (o que desbloqueia mais)
- [hypotheses/open.md](hypotheses/open.md) — hipóteses não validadas
- [callgraph/](callgraph/) / [xrefs/](xrefs/) / [vtables/](vtables/) / [dispatchers/](dispatchers/) — preenchido durante a RE

## Regra de ouro
- Toda descoberta vira **endereço + status + confiança + evidência**. NUNCA virar hipótese em confirmação sem evidência.
- Confiança: 0-30 UNKNOWN, 31-60 HYPOTHESIS, 61-80 LIKELY, 81-95 HIGH, 96-99 VERY HIGH, 100 CONFIRMED.
- O trabalho já feito está em `../FLUXO_UI.md`, `../FLUXO_LOGIN.md`, `memory/`, `src/login.c`.
- Base do módulo: `0x140000000`. Themida empacotado; input handler no VM (0x141A00000+, ofuscado); vtables runtime (heap).

## Ferramentas (conecte o MCP antes de analisar ao vivo)
- `mcp__connect-game__*` — o jogo precisa estar ABERTO + `connect_game.dll` injetada.
- Tools RELIABLE: read_memory, read_pointer, disassemble, aob_scan (bytes), scan_string, find_callers (E8/FF15), xref (RIP-rel).
- Tools NÃO-confiáveis: pointer_scan, scan_value (retornam scratch da própria bridge — ignorar).