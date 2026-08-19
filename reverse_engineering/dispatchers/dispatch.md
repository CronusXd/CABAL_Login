# Dispatchers — mecanismos de dispatch confirmados/hipótese

> Atualizado 2026-08-08. Status/CONF conforme regra. Base 0x140000000.

## 1. Dispatch de input bruto (teclado/mouse) → UI
- **Localizado no VM do Themida** (0x141A00000+, ofuscado). NÃO desmonta limpo.
- **Status: CONFIRMED** (por exclusão e evidência de checagens de tecla caindo no VM). Não é alvo direto.

## 2. Arquitetura do dispatch de clique (hipótese alta)
```
input brutal
  → UI manager (0x140F64D78, volátil)
  → acha o controle sob o cursor (hit-test; GetChild vtable+0x120)
  → dispara o evento do controle (EventSet::Snake::UI::Event::…)
  → invoca handlers std::function<bool(const EventArgs&)>
  → handler (lambda .text limpo) → ação
```
- **Status: LIKELY (85%)** por arquitetura (RTTI + EventSet + eventos nomeados).

## 3. Dispatch de CANAL → opcode → conectar (CONFIRMED)
```
[clique no canal] → [obj?+0x198]=canal → dispatch 0x4A49D4(0x140F73AE0, canal)
   canal 1-6 → opcode 0x3CE..0x3D2 → findChannel 0x4A5368 (lista [game_obj+0x80])
   canal 7 → 0x140C98BF8 | 8 → 0x140C98BFC | 9 → 0x140C98C00 | 10+ → 0x140C3ECE8
→ 0x8C builder 0x740F82 (GetChannel em 0x14137DFB0 via 0x4DEAA4) → envia 0x8C
```

## 3. Dispatch do EnterWorld (0x8E) — LIKELY
- 0xAC7267 retorna opcode 0x8E. Builders: 0x5643A3 / 0x7222B3 / 0x80A0B2.

## 4. Dispatch de vendas/servidor (menu O)
- "Desconectar" → **0x356B30** (fecha as 2 conns) — CONFIRMED, chamável.
- "Selecionar servidor" → **~0x34F890** (fecha só world) — HYPOTHESIS 70%.

## Binance do dispatcher da UI (o ALVO do ciclo atual)
- A invocação do `std::function` é o nó que destrava TODOS os handlers. Procurar o padrão de `call [vtable]` no código dos controles.
- Próximo: usar uma janela viva (ex.: menu O aberto) → botão → EventSet → vftable → método.

## XREFs coletados (fonte de pendências)
- `0x140F64D78` (g_manager): 200+ refs `mov rcx,[0x140F64D78]` (leitura, méthodes da manager).
- Event strings (EventBtnClick 0xCB28D8 etc.): NENHUM ref de código (RIP-rel/imm64 = 0) → usadas só por objetos runtime.
- GUIDs DirectInput: SysMouse 0x140cdd0a0 (sem ref de código).
- closesocket: 0x0EAEC0, 0x389484.