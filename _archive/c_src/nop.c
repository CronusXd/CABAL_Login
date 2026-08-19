/* nop.c — DLL de TESTE: DllMain vazio, NAO cria thread, NAO patcha, NAO faz nada.
 * Para isolar: se injetar este DLL e o jogo FECHAR, o problema e a INJECAO
 * (anti-cheat detecta). Se o jogo ficar aberto, o problema e o codigo da
 * CABAL_Login.dll. */
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID l) {
    (void)h; (void)r; (void)l;
    return TRUE;   /* nao faz nada */
}
