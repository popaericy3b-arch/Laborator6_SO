/*
 * Comunicare intre doua procese printr-un obiect mapat in memorie (POSIX: Linux si macOS).
 *
 * Aceeasi logica ca varianta Windows:
 *   1. rezerva semaforul (sem_wait)
 *   2. citeste numarul din memorie
 *   3. da cu banul (rand() % 2 + 1); cat timp cade 2, scrie in memorie urmatorul numar
 *   4. elibereaza semaforul (sem_post)
 *
 * Rulare:  ./contor          -> parintele creeaza memoria (shm_open + mmap) si semaforul,
 *                               apoi porneste 2 procese copil separate (fork + exec)
 *          ./contor copil N  -> folosit intern de parinte
 *
 * Compilare:  gcc contor.c -o contor -pthread   (pe glibc vechi: adaugati -lrt)
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NUME_MEMORIE  "/contor_memorie_partajata"
#define NUME_SEMAFOR  "/contor_semafor"
#define LIMITA        1000

typedef struct {
    int   numar;          /* ultimul numar scris */
    pid_t ultimul_pid;    /* procesul care a scris ultimul numar */
    int   scrieri[2];     /* cate numere a scris fiecare copil */
} ZonaPartajata;

static int proces_copil(int index)
{
    int fd = shm_open(NUME_MEMORIE, O_RDWR, 0);
    if (fd < 0) { perror("shm_open (copil)"); return 1; }

    ZonaPartajata *zona = mmap(NULL, sizeof(ZonaPartajata), PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, 0);
    close(fd);
    if (zona == MAP_FAILED) { perror("mmap (copil)"); return 1; }

    sem_t *sem = sem_open(NUME_SEMAFOR, 0);
    if (sem == SEM_FAILED) { perror("sem_open (copil)"); return 1; }

    pid_t pid = getpid();
    srand((unsigned)(time(NULL) ^ (pid << 8)));

    for (;;) {
        /* 1. Rezerva semaforul */
        sem_wait(sem);

        /* 2. Citeste memoria */
        int curent = zona->numar;
        if (curent >= LIMITA) {
            sem_post(sem);
            break;
        }

        /* 3. Da cu banul: cat timp cade 2, scrie urmatorul numar */
        while (curent < LIMITA && (rand() % 2 + 1) == 2) {
            curent++;
            zona->numar = curent;
            zona->ultimul_pid = pid;
            zona->scrieri[index]++;
            printf("[proces %d | PID %5d] scrie %d\n", index, (int)pid, curent);
        }
        fflush(stdout);

        /* 4. Elibereaza semaforul */
        sem_post(sem);

        /* Cedam procesorul ca sa aiba si celalalt proces o sansa */
        usleep(1000);
    }

    printf("[proces %d | PID %5d] gata, am scris %d numere\n", index, (int)pid,
           zona->scrieri[index]);
    fflush(stdout);

    sem_close(sem);
    munmap(zona, sizeof(ZonaPartajata));
    return 0;
}

static int proces_parinte(const char *cale_exe)
{
    /* Curatam eventuale resturi de la o rulare anterioara oprita fortat */
    shm_unlink(NUME_MEMORIE);
    sem_unlink(NUME_SEMAFOR);

    int fd = shm_open(NUME_MEMORIE, O_CREAT | O_RDWR, 0600);
    if (fd < 0) { perror("shm_open"); return 1; }
    if (ftruncate(fd, sizeof(ZonaPartajata)) < 0) { perror("ftruncate"); return 1; }

    ZonaPartajata *zona = mmap(NULL, sizeof(ZonaPartajata), PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, 0);
    close(fd);
    if (zona == MAP_FAILED) { perror("mmap"); return 1; }
    memset(zona, 0, sizeof(ZonaPartajata));

    /* Semafor binar: valoare initiala 1 */
    sem_t *sem = sem_open(NUME_SEMAFOR, O_CREAT, 0600, 1);
    if (sem == SEM_FAILED) { perror("sem_open"); return 1; }

    pid_t copii[2];
    for (int i = 0; i < 2; i++) {
        copii[i] = fork();
        if (copii[i] < 0) { perror("fork"); return 1; }
        if (copii[i] == 0) {
            /* exec => procesul devine o aplicatie independenta */
            char idx[4];
            snprintf(idx, sizeof(idx), "%d", i);
            execlp(cale_exe, cale_exe, "copil", idx, (char *)NULL);
            perror("execlp");
            _exit(1);
        }
        printf("Pornit procesul %d cu PID %d\n", i, (int)copii[i]);
        fflush(stdout);
    }

    for (int i = 0; i < 2; i++)
        waitpid(copii[i], NULL, 0);

    printf("\nRezultat final: %d (proces 0 a scris %d, proces 1 a scris %d)\n",
           zona->numar, zona->scrieri[0], zona->scrieri[1]);

    sem_close(sem);
    sem_unlink(NUME_SEMAFOR);
    munmap(zona, sizeof(ZonaPartajata));
    shm_unlink(NUME_MEMORIE);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc >= 3 && strcmp(argv[1], "copil") == 0)
        return proces_copil(atoi(argv[2]) ? 1 : 0);

    /* argv[0] = calea programului curent (merge pe Linux si pe macOS) */
    return proces_parinte(argv[0]);
}
