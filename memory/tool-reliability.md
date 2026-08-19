---
name: tool-reliability
description: Quais tools do MCP connect-game funcionam e quais retornam lixo (contaminação do connect_game.dll)
metadata: 
  node_type: memory
  type: reference
  originSessionId: 04c6c9c3-c44d-436a-8f7f-168dfb529690
  modified: 2026-08-08T23:05:43.831Z
---

# Confiabilidade das tools do connect-game MCP (sessão 2026-08-08, modo injetado)

## FUNCIONAM (confiáveis)
- `read_memory` / `read_pointer` / `read_region` — leitura direta.
- `disassemble` / `describe_function` — desassembly.
- `xref` — refs RIP-relativas no módulo (funciona para globals, ex.: 0x140F64D78 tem 200+ refs).
- `find_callers` — varre .text por E8/FF15 (funciona; ex.: GetProperty tem 29 callers).
- `aob_scan` — busca de bytes no módulo (módulo tem ~32MB).
- `scan_string` / `strings_scan` / `list_modules` / `list_sections` / `list_exports` / `list_imports` / `list_threads`.

## NÃO CONFIÁVEIS (retornam lixo)
- **`pointer_scan`** — retorna endereços de scratch da própria `connect_game.dll` (a DLL de bridge), não objetos do jogo. Para QUALQUER valor pesquisado devolve os mesmos endereços (~0x672f...). Confirmado: o conteúdo lido nesses endereços aponta para connect_game.dll, e `scan_value` na mesma faixa não encontra os valores.
- **`scan_value` / `next_scan`** — mesmos sintomas: hits que não contêm o valor procurado no momento da leitura; resultados inconsistentes entre scans (heap em churn + contaminação).

## Causa provável
A `connect_game.dll` injetada usa memória própria (heap ~0x672f..., 0x58e..., 0x5a1...) e os scans de ponteiro/valor estão vazando/escrevendo scratch da própria bridge em vez de varrer o processo alvo de verdade.

## Consequência
- NÃO confiar em nenhum objeto de heap achado por pointer_scan/scan_value (ex.: os "objetos de botão" em 0x672f... eram scratch da bridge).
- Para achar objetos do jogo, usar apenas: rótulos via scan_string (estáveis, ex.: "Desconectar" em 0xdf0732) + estrutura derivada de CÓDIGO (vtable/offsets), ou observação via hook.

Ver [[ui-framework-callbacks]].
