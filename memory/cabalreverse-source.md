---
name: cabalreverse-source
description: "D:\projeto\CABALREVERSE — fonte completa de DLL injetada p/ Cabal EP33 (x86): mapa USERDATACONTEXT, crypto XOR completa, opcodes de login, GameVersion"
metadata:
  node_type: memory
  type: reference
  modified: 2026-08-16T00:00:00.000Z
---

# CABALREVERSE (CabalRealmEP33) — Fonte-Referência para o Login

`D:\projeto\CABALREVERSE\CabalRealmEP33` = cópia do repo `ReverseAbreu/CabalRealmEP33` (DLL C++20 x86 injetada no cliente EP33) + 5 docs de análise em `ANALISE/`. **Não é drop-in** (endereços x86 de outro build) — é **blueprint**: algoritmo, structs, metodologia.

## O que Ele Confirma/Fornece (validado contra nosso proxy e login)

- **Mapa do USERDATACONTEXT** (GameDef.h): ponteiro global x86 `0x00CECAC4` (→ x64 hipótese `0x1408ECAC4`), nome `+0x2528`, nameLen `+0x2538`, nível `+0x2D90`, onLogged `+0x2F58`, mapa `+0x5710`, classe `+0x280C`, rank `+0x2810`, nação `+0x4A4`, guild `+0x24D0`, slots `+0x113C`. Offsets internos são **bitness-independentes**. Dump massivo de 136k linhas em `GameCustom/UserDataContextMapping/UserDataContext.h` (técnica: dump completo da struct p/ achar campo novo).
- **Crypto XOR completa** (PacketManager.cpp/h): seed `0x8F54C37B|1`, SEND `0x7AB38CF1`, magic `0xB7E2`/ext `0xC8F3`, keychain 2×16k DWORDs indexada `&0x3FFF`, **2ª metade re-semada por `Generate2ndXorKeyTable(seed)`** no handshake → porque replay não funciona; alavanca p/ modo híbrido futuro.
- **Opcodes login** (ProtoDefEx/ProtosDef.h): `0x65` Connect2Svr, `0x7A` CheckVersion, `0x7D2` PreServerRequest(user), `0x7D1` PublicKey, `0x67` AuthAccount, `0x7D6` desafio; modos estendidos 2001-2006. **`0x120/0x121` = NFY_SYSTEMMESSG/SERVERSTATE → motivo de falha (senha errada/bloqueado/cheio)** — classe de falha determinística pro login.
- **GameVersion** (GameVersion/): version 263, magickey 583120283, vXorKeys {0x92,0x65,0x67,0x57} (do servidor "CabalRealm" — provável **DIFERENTE** no nosso).
- **S2C_CHANNELLIST layout** (ProtosDef.h) — p/ automatizar seleção de servidor/canal sem coordenadas.
- **OnLogged** (Basic.cpp) — hook no callback do jogo (seta +0x2F58, Status 0/1) = sinal determinístico de "entrou no mundo", melhor que polling de socket.
- **Multi-client dev** (`MAX_GAME_INSTANCE=6`) e **redirect por tabela `mProxyList`** (ChatNode/AgentShop/StunSvr/WorldSvr) — conceito p/ proxy multi-serviço.
- Uma porta x86→x64: `Addr64 = 0x140000000 + (Addr86 - 0x400000)` (só p/ mesmo build).

## Validação Contra Nosso Build (x64, Themida, 2026-08-09 ao vivo)

| Item | CABALREVERSE (x86) | Nosso Build (x64) | Status |
|------|-------------------|-------------------|--------|
| Seed keychain | `0x8F54C37B|1` | **CONFIRMADO** (tool: "Cabal INITIAL_KEY") | ✅ |
| SEND_XORKEY | `0x7AB38CF1` | **CONFIRMADO** (DecodeHeader RVA `0x388A68`) | ✅ |
| Magic/Ext | `0xB7E2` / `0xC8F3` | **CONFIRMADO** | ✅ |
| Keytable size | `0x4000` (16384) | **CONFIRMADO** | ✅ |
| LCG constants | `0x2F6B6F5/0x14698B7/0xB327BD/0x27F41C3` | **CONFIRMADO** | ✅ |
| USERDATACONTEXT ptr | `0x00CECAC4` (x86) | **NÃO em `base+0x8ECAC4`** (era código) | ❌ |
| USERDATACONTEXT real | — | **RVA `0xF71A98`** (`.data`, seção 0xEF0000) | ✅ Achado ao vivo |
| Offsets internos | nome `+0x2528`, nível `+0x2D90`, onLogged `+0x2F58` | **CONFIRMADOS** (nome="TroTXD", len=6, charIdx=1, nação=0) | ✅ |
| Nível/onLogged | — | **Só válidos in-world** (fora = 0) | ⚠️ Precisa entrar no mundo |
| Opcodes login | 0x65, 0x7A, 0x7D2, 0x7D1, 0x67, 0x7D6 | **Assumidos iguais** (proto não muda) | ✅ |
| NFY_SYSTEMMESSG | 0x78 / 0x79 | **Usado no recvwatch** p/ classificar falha | ✅ |

## Aviso Importante

Nosso login lê `base+0x137F170/0x137F1B8/0x137F190` — **struct DIFERENTE** do USERDATACONTEXT (que seria ~RVA 0x8ECAC4). Testar achar o USERDATACONTEXT no nosso x64 via AOB (nome +0x2528/nível +0x2D90/onLogged +0x2F58) e reavaliar. Ver [[login-flow-state]], [[ui-framework-callbacks]], [[tool-reliability]].

## Arquivos-Chave no CABALREVERSE

```
CabalRealmEP33/
├── GameDef.h                    # USERDATACONTEXT offsets + structs
├── PacketManager.cpp/h          # Crypto XOR completa (keychain, seed, LCG)
├── ProtoDefEx/ProtosDef.h       # Opcodes login, S2C_CHANNELLIST, NFY_SYSTEMMESSG
├── GameVersion/                 # Version, magickey, vXorKeys
├── Basic.cpp                    # OnLogged callback hook
├── GameCustom/UserDataContextMapping/UserDataContext.h  # Dump 136k linhas
└── ANALISE/                     # 5 docs de análise
```

## Como Usar no Nosso Projeto

1. **Offsets USERDATACONTEXT** → já em `login.h` (`UDC_*` defines) — bitness-independentes, validados ao vivo.
2. **Crypto XOR** → já portada em `recvwatch.c` (keychain, seed, LCG, decrypt) — **100% confirmada ao vivo**.
3. **Opcodes falha login** → `recvwatch.c` classifica via 0x78/0x79 + keywords — funcionando.
4. **OnLogged** → `login_wait_onlogged()` espera `+0x2F58==1` — implementado, precisa usuário in-world p/ validar.
5. **GameVersion** → magickey/vXorKeys podem ser diferentes no nosso servidor — não usado atualmente.