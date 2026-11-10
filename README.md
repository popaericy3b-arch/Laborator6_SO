# Contor partajat prin memorie mapată + semafor

Tema la **Sisteme de Operare**: două procese numără împreună de la 1 la 1000
într-un obiect mapat în memorie.

Fiecare proces repetă:

1. **rezervă semaforul** – de aici memoria e doar a lui;
2. **citește memoria** (ultimul număr scris);
3. **dă cu banul** (`rand() % 2 + 1`) – cât timp cade 2, scrie în memorie următorul număr;
4. **eliberează semaforul**.

## De ce e nevoie de semafor

Memoria partajată este „teren neutru”: ambele procese o pot citi/scrie în același timp.
Fără sincronizare, un proces ar putea citi valoarea `5`, apoi sistemul de operare îl
întrerupe, celălalt scrie `6`, iar primul revine și scrie tot `6` (sau scrie peste o
valoare pe jumătate actualizată). Semaforul binar (valoare inițială 1, maxim 1) garantează
că doar un proces la un moment dat atinge memoria, deci aceasta e mereu într-o stare stabilă.

## Procese vs. fire de execuție

- **Procesele** sunt aplicații individuale pentru sistemul de operare, fiecare cu spațiul
  său de adrese. De aceea au nevoie de un mecanism explicit (aici: obiect mapat în memorie)
  ca să comunice. Un crash într-un proces nu le afectează pe celelalte – exemplu: Chrome
  pornește câte un proces per tab.
- **Firele de execuție** (threads) ale aceluiași proces împart memoria și sunt strâns legate;
  dacă unul crapă sau îngheață, de obicei cade tot procesul – exemplu: Skype trimite mesaje
  pe un fir și le primește pe altul.

În acest proiect parintele pornește **două procese separate** (`CreateProcess` pe Windows,
`fork` + `exec` pe Linux), nu fire de execuție.

## Structura

```
windows/contor.c   – Windows API: CreateFileMapping, MapViewOfFile, CreateSemaphore
linux/contor.c     – POSIX (Linux si macOS): shm_open, mmap, sem_open
```

## Compilare și rulare

**Windows** (MinGW sau Visual Studio):

```
gcc windows\contor.c -o contor.exe
contor.exe
```

sau `cl windows\contor.c` din Developer Command Prompt.

**Linux / macOS**:

```
make
./linux/contor
```

## Exemplu de ieșire

```
Pornit procesul 0 cu PID 229
Pornit procesul 1 cu PID 230
[proces 1 | PID   230] scrie 1
[proces 1 | PID   230] scrie 2
...
[proces 0 | PID   229] scrie 7
[proces 0 | PID   229] scrie 8
[proces 1 | PID   230] scrie 9
...
Rezultat final: 1000 (proces 0 a scris 499, proces 1 a scris 501)
```

Numerele apar în ordine, fără goluri și fără dubluri, chiar dacă procesele se alternează aleator.
