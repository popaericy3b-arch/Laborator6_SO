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
