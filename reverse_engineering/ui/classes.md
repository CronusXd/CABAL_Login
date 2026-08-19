# Classes de UI (RTTI) — inventário (parcial, em crescimento)
> Nome RTTI → type_info → potencial vftable (runtime p/ classes de UI; estática p/ CRT/STL).
> Tipo RTTI: `.?AV<Name>@@`.

## Controles (inputs a UI)
| Classe | type_info | Notas |
|---|---|---|
| **CButton** | 0x140f0ad08 | o botão (alvo p/ dispatcher) |
| CButtonGroup | 0x140f0f588 | grupo de botões (rádio/tab) |
| **CDialog** | 0x140f06208 | janela de diálogo (Sim/Não) |
| CInfoDialog | 0x140f062b0 | diálogo de info |
| CCtrlEntity | 0x140f05c40 | entidade de controle |
| CChildHolderEntity | 0x140f06220 | container de filhos |
| CTabPanel | 0x140f05c68 | painel de abas |
| CMultiLineChatBox | 0x140f338B8 | caixa de chat multilinha |
| CMultiLineGameBox | 0x140f338d8 | caixa de conquistas/eventos multilinha |

## Gestão
| Classe | type_info | Notas |
|---|---|---|
| **CUIMgr** | 0x140f338BC | o UI MANAGER! instância global ~0x140F64D78 |
| Snake::UI::Event::EventSet | 0x140f06250 | mapa de handlers std::function |
| Snake::UI::Event::EventArgs | (RTTI @Event@UI@Snake) | base de event args |
| Snake::UI::Event::MouseEventArgs | 0x140f32d60 | args de mouse (x,y) |

## Categorias do framework (não-UI, referência)
- `Network@Snake::IPacketSubDataSpec`, `CGameSoc`, `DelayEventSystem`, managers `*@Altar`, `AnimaMastery` etc.

## Padding
- Classes ATL/COM (`CWindowImplBaseT`, `IOleWindow`) NÃO são UI do jogo (WebBrowser). Ignorar.
- Type_info layout (padrão): vftable ptr +0x00, spare +0x08, nome +0x10.
- Para vftable da UC mimo: runtime (protegida Themida); achável só via objeto vivo (menu/inventário aberto).