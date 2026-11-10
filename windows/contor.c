/*
 * Comunicare intre doua procese printr-un obiect mapat in memorie (Windows API).
 *
 * Doua procese numara impreuna de la 1 la 1000 intr-o zona de memorie partajata:
 *   1. rezerva semaforul (WaitForSingleObject)
 *   2. citeste numarul din memorie
 *   3. da cu banul (rand() % 2 + 1); cat timp cade 2, scrie in memorie urmatorul numar
 *   4. elibereaza semaforul (ReleaseSemaphore)
 *
 * Rulare:  contor.exe          -> procesul parinte creeaza memoria + semaforul
 *                                 si porneste 2 procese copil (CreateProcess)
 *          contor.exe copil    -> folosit intern de parinte
 *
 * Compilare (MinGW):  gcc contor.c -o contor.exe
 * Compilare (MSVC):   cl contor.c
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUME_MEMORIE  "Local\\ContorMemoriePartajata"
#define NUME_SEMAFOR  "Local\\ContorSemafor"
#define LIMITA        1000

/* Structura care traieste in memoria partajata ("terenul neutru") */
typedef struct {
    int  numar;            /* ultimul numar scris */
    DWORD ultimul_pid;     /* procesul care a scris ultimul numar */
    int  scrieri[2];       /* cate numere a scris fiecare copil */
} ZonaPartajata;

static int proces_copil(int index)
{
    HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, NUME_MEMORIE);
    if (hMap == NULL) {
        fprintf(stderr, "[copil %d] OpenFileMapping a esuat: %lu\n", index, GetLastError());
        return 1;
    }

    ZonaPartajata *zona = (ZonaPartajata *)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0,
                                                         sizeof(ZonaPartajata));
    if (zona == NULL) {
        fprintf(stderr, "[copil %d] MapViewOfFile a esuat: %lu\n", index, GetLastError());
        CloseHandle(hMap);
        return 1;
    }

    HANDLE hSem = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, NUME_SEMAFOR);
    if (hSem == NULL) {
        fprintf(stderr, "[copil %d] OpenSemaphore a esuat: %lu\n", index, GetLastError());
        UnmapViewOfFile(zona);
        CloseHandle(hMap);
        return 1;
    }

    DWORD pid = GetCurrentProcessId();
    srand((unsigned)(GetTickCount() ^ (pid << 8)));

    for (;;) {
        /* 1. Rezerva semaforul: de aici memoria e doar a noastra */
        WaitForSingleObject(hSem, INFINITE);

        /* 2. Citeste memoria */
        int curent = zona->numar;
        if (curent >= LIMITA) {
            ReleaseSemaphore(hSem, 1, NULL);
            break;
        }

        /* 3. Da cu banul: cat timp cade 2, scrie urmatorul numar */
        while (curent < LIMITA && (rand() % 2 + 1) == 2) {
            curent++;
            zona->numar = curent;
            zona->ultimul_pid = pid;
            zona->scrieri[index]++;
            printf("[proces %d | PID %5lu] scrie %d\n", index, pid, curent);
        }
        fflush(stdout);

        /* 4. Elibereaza semaforul */
        ReleaseSemaphore(hSem, 1, NULL);

        /* Cedam procesorul ca sa aiba si celalalt proces o sansa */
        Sleep(1);
    }

    printf("[proces %d | PID %5lu] gata, am scris %d numere\n", index, pid, zona->scrieri[index]);

    CloseHandle(hSem);
    UnmapViewOfFile(zona);
    CloseHandle(hMap);
    return 0;
}

static int proces_parinte(const char *cale_exe)
{
    /* Obiectul mapat in memorie, sustinut de pagefile (nu de un fisier pe disc) */
    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                     0, sizeof(ZonaPartajata), NUME_MEMORIE);
    if (hMap == NULL) {
        fprintf(stderr, "CreateFileMapping a esuat: %lu\n", GetLastError());
        return 1;
    }

    ZonaPartajata *zona = (ZonaPartajata *)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0,
                                                         sizeof(ZonaPartajata));
    if (zona == NULL) {
        fprintf(stderr, "MapViewOfFile a esuat: %lu\n", GetLastError());
        CloseHandle(hMap);
        return 1;
    }
    ZeroMemory(zona, sizeof(ZonaPartajata));

    /* Semafor binar: valoare initiala 1, maxim 1 */
    HANDLE hSem = CreateSemaphoreA(NULL, 1, 1, NUME_SEMAFOR);
    if (hSem == NULL) {
        fprintf(stderr, "CreateSemaphore a esuat: %lu\n", GetLastError());
        UnmapViewOfFile(zona);
        CloseHandle(hMap);
        return 1;
    }

    /* Pornim doua procese separate (aplicatii individuale pentru SO) */
    PROCESS_INFORMATION pi[2];
    for (int i = 0; i < 2; i++) {
        char linie[MAX_PATH + 32];
        snprintf(linie, sizeof(linie), "\"%s\" copil %d", cale_exe, i);

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi[i], sizeof(pi[i]));

        if (!CreateProcessA(NULL, linie, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi[i])) {
            fprintf(stderr, "CreateProcess a esuat: %lu\n", GetLastError());
            return 1;
        }
        printf("Pornit procesul %d cu PID %lu\n", i, pi[i].dwProcessId);
    }

    HANDLE copii[2] = { pi[0].hProcess, pi[1].hProcess };
    WaitForMultipleObjects(2, copii, TRUE, INFINITE);

    printf("\nRezultat final: %d (proces 0 a scris %d, proces 1 a scris %d)\n",
           zona->numar, zona->scrieri[0], zona->scrieri[1]);

    for (int i = 0; i < 2; i++) {
        CloseHandle(pi[i].hProcess);
        CloseHandle(pi[i].hThread);
    }
    CloseHandle(hSem);
    UnmapViewOfFile(zona);
    CloseHandle(hMap);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc >= 3 && strcmp(argv[1], "copil") == 0)
        return proces_copil(atoi(argv[2]) ? 1 : 0);

    char cale_exe[MAX_PATH];
    GetModuleFileNameA(NULL, cale_exe, MAX_PATH);
    return proces_parinte(cale_exe);
}
