#pragma once
/* login.h — CABAL_Login: driver de login automático (C, Windows).
 * Le contas de um .txt, digita as credenciais na tela de login do cliente
 * (SendInput) e avança a máquina de estados até entrar no mundo. */

#include <windows.h>
#include <stdbool.h>
#include <stdio.h>

/* Conta do arquivo: "usuario:senha:nome_da_char" (charname opcional).
 * Ou formato BR SUB: "Conta N / Usuario: / Senha: / subsenha: / Server: / NV:" */
typedef struct {
    char user[64];
    char pass[64];
    char subsenha[64];   /* subsenha do formato BR (protecao da conta) */
    char charname[64];
    int  nivel;          /* campo NV do arquivo (referencia) */
} LoginAccount;

/* Carrega accounts.txt (formato "user:pass[:char]"). Retorna o numero de contas. */
int login_load_accounts(const char* path, LoginAccount* out, int max);

/* Carrega o arquivo BR ("Cabal BR SUB.txt": Conta N / Usuario: / Senha: / subsenha: / NV:).
 * Retorna o numero de contas. */
int login_parse_br(const char* path, LoginAccount* out, int max);

/* Digita texto via SendInput (KEYEVENTF_UNICODE). */
void login_type_text(const char* s, int delay_ms);

/* Digita user<Tab>pass<Enter> na tela de login. Retorna true se enviou. */
bool login_do_credentials(const LoginAccount* a);

/* Digita VK_RETURN (selecionar/entrar). */
void login_press_enter(void);
void login_press_tab(void);

/* Clique do mouse em (x,y) em PIXELS da tela (menu do Cabal e DirectX, sem
 * botao Win32 — cliques usam coordenadas). Escala p/ MOUSEEVENTF_ABSOLUTE. */
void login_click(int x, int y);

/* Posicao do botao "Entrar" da tela de login (pixels). Se definida, o driver
 * clica nela apos digitar a senha (alem do Enter). -1 desabilita. */
void login_set_entrar(int x, int y);

/* CHAMA a funcao de toggle de UI do proprio jogo (sem teclado/mouse):
 *   mode 0 = inventario (tecla I), 2 = menu (tecla O), 1 = ?.
 * Base do CabalMain + 0x549FD8 com o objeto global base+0x138ABF0.
 * Retorna false se o modulo nao for achado. */
#define LOGIN_UI_INVENTORY 0
#define LOGIN_UI_MENU      2
bool login_toggle_ui(int mode);

/* CHAMA a funcao de DESCONECTAR do jogo (base+0x356B30): fecha as DUAS
 * conexoes (login + world) e volta pra tela de login — mesmo efeito de
 * clicar em "Desconectar" > "Sim" no menu principal.
 * this = buffer zerado (o jogo so usa [this+0x458] p/ release opcional;
 * zerado => pula). Protegido por SEH (nunca derruba o jogo). */
bool login_do_disconnect(void);

/* CHAMA a funcao de SELECIONAR SERVIDOR do jogo (base+0x34F878): prepara o
 * estado do servidor (global 0x140F5DAEC) e fecha SO a conexao do mundo
 * (0x1413E6318), mantendo o login — volta pra tela de selecao de servidor.
 * Sem argumentos (void). Protegido por SEH. */
bool login_do_server_select(void);

/* Ações de seleção do fluxo principal. Devem usar somente o dispatch interno
 * de UI; retornam false enquanto a assinatura do handler não estiver confirmada.
 * Coordenadas e envio/craft de pacotes não fazem parte deste projeto. */
bool login_select_server(void);
bool login_select_channel(void);
bool login_start_character(void);

/* ── VALIDAÇÃO DE ESTADO (automação por máquina de estados) ──
 * Regra: NUNCA disparar o proximo passo antes de confirmar o atual.
 * Sinal base: socket da conexao de login ([conn1+0x08]):
 *   -1 / 0  => desconectado / tela de LOGIN
 *   outro   => conectado (selecao de servidor / personagem / mundo)
 */
/* Le o socket da conexao de login (0 se nao conseguir). */
long long login_conn1_socket(void);

/* Aguarda ate ficar CONECTADO (socket valido). true se alcancou. */
bool login_wait_logged_in(int timeout_ms);

/* Aguarda ate voltar pra tela de LOGIN (socket fechado). */
bool login_wait_login_screen(int timeout_ms);

/* Aguarda a tela de PERSONAGEM (char data com nome preenchido em base+0x137F190). */
bool login_wait_char_ready(int timeout_ms);

/* Aguarda o botao do mundo / char carregado (ouro != 0 OU nome != 0 em base+0x137F1xx). */
bool login_wait_world(int timeout_ms);

/* Console em tempo real: se `c` for setado, login.c espelha os logs nele
 * (alem do arquivo). Chamar com stdout apos AllocConsole. */
void login_set_console(FILE* c);

/* Le login.cfg (D:\projeto\CABAL_Login\login.cfg) e carrega as posicoes dos
 * 7 botoes (entrar, servidor, canal, comeca, selecionar_servidor, sim,
 * desconectar). Chamar apos injetar. */
void login_read_cfg(void);

/* Loga em `f` (arquivo) + console (se setado). */
void login_log(FILE* f, const char* fmt, ...);

/* Modo de mapeamento: o usuario passa o mouse sobre cada botao e aperta F9;
 * grava as posicoes (pixels) no login.cfg. Retorna o numero de botoes mapeados. */
int login_map_buttons(void);

/* Maquina de estados (usada pela thread da DLL). */
typedef enum {
    LOGIN_ST_IDLE = 0,   /* aguardando sinal */
    LOGIN_ST_LOGIN,      /* digitar credenciais */
    LOGIN_ST_CHARSEL,    /* selecionar personagem */
    LOGIN_ST_INWORLD,    /* entrou no mundo (done) */
    LOGIN_ST_DONE
} LoginState;

/* Roda um ciclo. Retorna true somente se a conta entrou no mundo e foi
 * processada; false em qualquer bloqueio/falha (nao gravar como sucesso). */
bool login_run_account(const LoginAccount* a, FILE* lf);

/* ── USERDATACONTEXT (mapa do CABALREVERSE EP33; offsets internos de struct,
 * que NAO dependem de bitness). O finder resolve o ponteiro em runtime:
 * primeiro a hipotese de RVA (base+0x8ECAC4), depois scan do .text por
 * "mov rax,[rip+disp32]" + deref, validando heurístico (nome/nível/onLogged).
 * Se o build atual não casar os offsets, as funções caem no legado
 * base+0x137F1xx — sem regressão. */
#define UDC_NAME      0x2528   /* char[] nome da char */
#define UDC_NAMELEN   0x2538
#define UDC_GUILD     0x24D0
#define UDC_GUILDLEN  0x24E0
#define UDC_NATION    0x4A4
#define UDC_CLASS     0x280C
#define UDC_RANK      0x2810
#define UDC_LEVEL     0x2D90
#define UDC_MAP       0x5710
#define UDC_LOGGED    0x2F58   /* 1 = dentro do mundo */

/* Encontra o USERDATACONTEXT do processo (cache). NULL se nao achar. */
void* login_find_userdata(void);

/* Leituras SEH dos campos do userdata (0/"" se invalido). */
int  login_udc_int(void* ud, DWORD off);
int  login_udc_str(void* ud, DWORD off, DWORD lenoff, char* out, int max);

/* Aguarda onLogged (+0x2F58) ficar 1 — sinal do PRÓPRIO jogo de "entrou no
 * mundo" (substitui esperar cegamente). false se userdata nao encontrado ou
 * timeout. */
bool login_wait_onlogged(int timeout_ms);

/* Verificação ENRIQUECIDA: nível/nome/ouro + classe/nação/rank/mapa/guild.
 * Usa USERDATACONTEXT se achado, senao o legado base+0x137F1xx.
 * Grava a linha de resultado em `out` (append). Devolve 1 se leu dados. */
int login_verify_enriched(const LoginAccount* a, FILE* out);
