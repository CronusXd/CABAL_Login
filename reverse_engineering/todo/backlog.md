# Backlog priorizado — itens que destravam mais

> Prioridade própria do mandato: Dispatchers > Event systems > UI handlers > VTables > Core managers > gaps > desconhecidas.

## [HIGH] (bloqueiam vários)
1. **Dispatcher de clique da UI** — o código .text que processa input→controle→EventSet (não é o VM do input bruto; é o código que invoca o std::function). Alvo: achar onde `std::function::operator()` é chamado com `EventArgs` e controlo sob o cursor.
2. **Vftable do CButton** via RTTI COL → métodos OnClick/OnMouseDown/ProcessInput em .text (o dispatch exato por botão). Caminho: achar o COL (estático) p/ CButton (type_info 0x140f0ad08) → vftable runtime → slots.
3. **Handler do clique no canal** (a função de UI que o canal chama → conecta). Candidatos: 0x140A01A00 / o caller do 0x8C. Preciso do objeto de sessão do canal (não abriu fixo).
4. **Assinatura exata do "selecionar servidor" ~0x34F890** (camplo callable + arg) para ligar na DLL.

## [MEDIUM]
5. **Handlers "Sim"/"Não"** dos diálogos (CDialog + botões CButton → ação).
6. **Estado server/channel selecionados** — onde fica a seleção do servidor (é UI, não [game-obj]).
7. **EventSet dispatch** (como o std::function é invocado) — o loop genérico.
8. **Vtable estática do objeto global 0x140F73AE0** ([0]=0x140C98688 não é vtable limpa; confirmar).

## [LOW]
9. Enumerar TODAS as classes RTTI de UI (CButton, CDialog, ..., dezenas) → vftables → handlers.
10. Mapear inventário/skills/mapa/config (interfaces fora do fluxo de login).

## Definido (não refazer)
- Todos os itens CONFIRMED do `../FLUXO_UI.md` e do inventário estão preservados.
- DLL compilada: `x64\Release\CABAL_Login.dll` com `login_do_disconnect()` (0x356B30).