# SD karta ze složky (`--sddir`) – návrh

Datum: 2026-11-26. Stav: schváleno uživatelem v konverzaci (malá změna, krátký návrh).

## Proč

S `--sd` musí uživatel nejdřív sestavit obraz (`picopad-sdimg create --from`) a po běhu z něj soubory, které hra zapsala, vykopírovat (`picopad-sdimg get`). Pro 99 % práce s kartou (data hry, savy, výstupy) chce pracovat s obyčejnou složkou. Složitější věci (vlastní layout, nízkoúrovňové testy, víc běhů nad jedním obrazem) zůstávají na `--sd IMG`.

## Cíle

- `--sddir DIR` (okno i `--headless`): hra vidí kartu FAT s obsahem složky DIR; co hra na kartě změní, je po ukončení emulátoru ve složce.
- Žádný soubor obrazu na disku; obraz žije jen v paměti a je řídký.
- Chování karty odpovídá skutečné `lib_fat` (ta zůstává beze změn a čte obraz po sektorech jako dnes).

## Necíle

- Průběžná synchronizace za běhu a změny ve složce během běhu (hra je nevidí).
- Dlouhá jména (LFN) – `lib_fat` je nemá.
- Přenos atributů (read-only, hidden) – přenáší se jen strom, obsah a čas změny.

## Anti-cíle

- Synchronizace smaže nebo přepíše něco ve složce, na co hra nesáhla (soubory s jménem mimo 8.3, soubory změněné uživatelem během běhu).
- Hra v emulátoru čte kartu, kterou by skutečná `lib_fat` na zařízení nepřečetla (rozjetí vlastního FAT kódu s `lib_fat`).

## Přístupy

1. **Virtuální FAT generovaný za běhu po sektorech** (jako `vvfat` v QEMU) – zamítnuto: zápis zpět vyžaduje odvozovat operace se soubory ze změn sektorů, známě nespolehlivé.
2. **Náhrada `lib_fat` hostitelskou implementací nad složkou** – zamítnuto: ~80 funkcí API a napodobování zvláštností `lib_fat`; hrozí rozdíl emulátor × zařízení.
3. **Snímek se zpětnou synchronizací** – **zvoleno uživatelem**: při startu se v paměti postaví obraz z obsahu složky, hra běží nad skutečnou `lib_fat`, při ukončení se souborový systém z obrazu promítne zpět do složky.

Jak obraz postavit a přečíst (podvolba přístupu 3):

- a) **Vlastní malý FAT zapisovač a čtečka v `platform/`** (čisté C bez SDK) – **zvoleno uživatelem**. Za běhu nic nepřidává, funguje i pro hry bez `USE_FAT`; shodu s `lib_fat` hlídají křížové testy proti `picopad-sdimg`.
- b) Kód `picopad-sdimg` (`lib_fat`) zabalený do DLL, aby se jeho symboly nebily s `lib_fat` hry – zamítnuto: další DLL u každé hry (i v cizích repech přes `PicopadEmu.cmake`) a stejně nový kód pro export stromu a zjištění smazaných souborů. Druhou kopii `lib_fat` nejde přilinkovat staticky (stejné symboly) a herní `lib_fat` nejde použít (při zavření okna / zásahu watchdogu je v rozpracovaném stavu; hra bez `USE_FAT` ji nemá).

## Návrh

**Volba.** `--sddir DIR`, okno i headless. S `--sd` se vylučuje → chyba argumentů, exit 2. DIR neexistuje / není složka → exit 2.

**Řídký obraz v paměti** (`platform/`, implementuje `plat_sd_*`). Pevná velikost 1 GiB (rozhodnutí Claude). Bloky po 64 KiB (rozhodnutí Claude) se alokují při prvním zápisu nenulových dat; nealokovaný blok se čte jako nuly. Přístup k obrazu je pod zámkem (čtení/zápis sektoru z herního vlákna × synchronizace z hlavního vlákna).

**Start.** Před spuštěním hry: naformátovat FAT32 (geometrie, kterou `lib_fat` přijme; volba velikosti clusteru je na implementátorovi, musí projít křížovým testem) a nahrát celý strom DIR, soubory souvisle. Jména: každý segment cesty musí být platné 8.3 jméno podle stejných pravidel jako `picopad-sdimg` (malá písmena se převedou na velká). Soubor/složka s neplatným jménem se nenahraje, vypíše se varování na stderr a synchronizace na ni (ani na nic pod ní) nikdy nesáhne. Čas změny se převede na FAT datum/čas (místní čas, 2 s rozlišení). Obsah se do 1 GiB nevejde → chyba, exit 2.

**Synchronizace zpět** při ukončení emulátoru, ať skončí jakkoli (hra sama, zavření okna, Ctrl+C, `--max-time`, watchdog, fatální chyba; okno i headless). Pracuje se zamčeným obrazem (herní vlákno se na dalším přístupu ke kartě zablokuje, proces pak končí). Čte souborový systém z obrazu vlastní čtečkou (procházení adresářů, řetězy clusterů), ne záznam operací – nízkoúrovňový zápis sektoru, který padne do dat souboru, se tak do složky propíše; zápis mimo soubory se neprojeví. Data, která `lib_fat` hry drží jen ve své cache (soubor neuzavřený, bez `DiskFlush`), se neuloží – stejně jako při vytažení karty ze zařízení.

- Nový nebo změněný soubor/složka (obsah, velikost nebo čas změny se liší od startu) → zapsat do DIR (u existujícího souboru do stávajícího jména, u nového velkými písmeny tak, jak je v FAT), nastavit čas změny z FAT.
- Soubor/složka, která byla při startu nahraná a v obrazu už není → smazat z DIR (složku jen pokud v DIR nezůstalo nic, co se nenahrálo; jinak varování).
- Konflikt: soubor v DIR se od startu změnil (velikost nebo čas změny), a hra ho také změnila nebo smazala → nesahat, varování (vyhrává verze ve složce).
- Chyba zápisu do DIR → varování, pokračuje se dalšími soubory; exit kód hry se nemění.

**Dokumentace.** README: nová sekce u SD karty (`--sddir`, pravidla synchronizace, omezení).

## Testy (minimum)

- Křížově s `lib_fat`: složka nahraná zapisovačem → obraz uložený do souboru (testovací režim nebo testovací exe) → `picopad-sdimg ls/get` vrátí stejný strom a obsah; obraz z `picopad-sdimg create --from` + `put` → čtečka vrátí stejný strom a obsah.
- End-to-end headless se `--sddir` (testovací hra typu SDTEST): hra přečte soubor ze složky, zapíše nový, změní existující, smaže jeden → po ukončení odpovídá složka očekávání; soubor s ne-8.3 jménem zůstal nedotčen.
- Synchronizace i při ukončení přes `--max-time` (hra stále běží).
- `--sd` + `--sddir` → exit 2; neexistující DIR → exit 2; obsah > 1 GiB řešit jednotkovým testem zapisovače (bez skutečných dat).

## Ponecháno implementátorovi

Velikost clusteru a přesné hodnoty BPB (v rámci toho, co `lib_fat` přijme), rozdělení kódu do souborů v `platform/`, formát varování, způsob, jak testy dostanou obraz ze zapisovače do souboru.
