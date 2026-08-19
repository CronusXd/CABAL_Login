# Hipóteses em aberto (NÃO promovidas sem evidência)

| Hipótese | Status | Por que não confirmada | Como validar |
|---|---|---|---|
| `~0x34F890` = "selecionar servidor" (fecha só o world) | HYPOTHESIS (70%) | função começa no meio de bloco; args incertos | achar o prólogo + o chamador do menu O |
| `0x140A01A00` = handler do clique no canal | LIKELY (85%) | é método grande; this/sessão não mapeado | rastrear o objeto de sessão do canal em runtime |
| `{8,9}` em 0x4644AF68/6C = servidor/canal | REFUTADA | Mercúrio 9 vs Vênus 2 → iguais | — (já refutado por diff) |
| A seleção de servidor mora num campo pequeno do game-obj 0x140F73AE0 | REFUTADA | diff Mercúrio/Vênus 0x0-0x17F idêntico | — |
| Vtable do CButton está no .rdata estático | REFUTADA | aob_scan de função como qword = 0 hits | — |
| O dispatcher de clique está fora do VM (código .text limpo) | LIKELY | métodos estão em seções 1-3; só o input bruto é VM | achar a invocação do std::function |