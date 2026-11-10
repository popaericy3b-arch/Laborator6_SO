#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUME_MEMORIE  "Local\\ContorMemoriePartajata"
#define NUME_SEMAFOR  "Local\\ContorSemafor"
#define LIMITA        1000


typedef struct {
    int  numar; 
    DWORD ultimul_pid; 
    int  scrieri[2];  
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
        WaitForSingleObject(hSem, INFINITE);

        int curent = zona->numar;
        if (curent >= LIMITA) {
            ReleaseSemaphore(hSem, 1, NULL);
            break;
        }

        while (curent < LIMITA && (rand() % 2 + 1) == 2) {
            curent++;
            zona->numar = curent;
            zona->ultimul_pid = pid;
            zona->scrieri[index]++;
            printf("[proces %d | PID %5lu] scrie %d\n", index, pid, curent);
        }
        fflush(stdout);

        ReleaseSemaphore(hSem, 1, NULL);

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

    HANDLE hSem = CreateSemaphoreA(NULL, 1, 1, NUME_SEMAFOR);
    if (hSem == NULL) {
        fprintf(stderr, "CreateSemaphore a esuat: %lu\n", GetLastError());
        UnmapViewOfFile(zona);
        CloseHandle(hMap);
        return 1;
    }

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
