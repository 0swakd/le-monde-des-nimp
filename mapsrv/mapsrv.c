
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#include "mapsrv.h"



#define MAP_DATAS_FILE "./mapdatas.dat"
/* considering the \n in block size for now convenience */
#define MAP_DATAS_BLOCK_SIZE 32 
#define READ_BLOCK_SIZE 512
#define MAP_BORDER_SIZE 64

#define MAP_BLOCK_TYPE_SIZE 2
#define MAP_BLOCK_RESERVED_SIZE 29

#define BACKLOG 4



typedef struct {
    map_cell_t *mapcell;
    int mapcellsize;
} srv_datas_t;



int service(srv_datas_t *datas);
int init(srv_datas_t *datas);
void del(srv_datas_t *datas);
int loaddatafile(srv_datas_t *datas);
int savedatasfile(srv_datas_t *datas);
void printmap(FILE* fd, srv_datas_t *datas);



int main(int argc, char **argv) {
    pid_t piddeamon = 0;
    int retdaemon = 0;
    int retinitsrv = 0;
    int retsavedatasfile = 0;
    srv_datas_t datas;
    
    fprintf(stdout, "Map server starting\n");
    
    fprintf(stdout, "Map server starting deamon\n");
    piddeamon = fork();

    if (piddeamon < 0) {
        fprintf(stderr, "Map server failed to start deamon\n");
        fprintf(stdout, "Map server failed to fork, now terminating\n");
        return(-1);
    }

    if (piddeamon > 0) {
        return(0);
    }

    retinitsrv = init(&datas);

    if (retinitsrv != 0) {
        del(&datas);
        return(retinitsrv);
    }

    retdaemon = service(&datas);

    if (retdaemon != 0) {
        del(&datas);
        return(retdaemon);
    }

    retsavedatasfile = savedatasfile(&datas);

    if (retsavedatasfile != 0) {
        del(&datas);
        return(retsavedatasfile);
    }

    del(&datas);

    fprintf(stdout, "Map server daemon terminating\n");

    return(0);
}



int service(srv_datas_t *datas) {
    register int sock;
    int retbind;
    struct sockaddr_in sa;
    int sasize = 0;
    register int c = 0;
    FILE *client = NULL;


    fprintf(stdout, "Map server daemon starting to serv\n");

    sock = socket(PF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        fprintf(stderr, "Map server daemon, impossible to create a socket\n");
        return(1);
    }

    bzero(&sa, sizeof(sa));

    sa.sin_family = AF_INET;
    sa.sin_port = htons(666);

    if (INADDR_ANY) {
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    
    retbind = bind(sock, (struct sockaddr *) &sa, sizeof(sa));

    if (retbind < 0) {
        perror("socket");
        fprintf(stderr, "Map server daemon, impossible to bind the socket\n");
        return(2);
    }

    listen(sock, BACKLOG);

    for(;;) {
        sasize = sizeof(sa); 
        
        c = accept(sock, (struct sockaddr *) &sa, &sasize);

        if (c < 0) {
            fprintf(stderr, "Map server daemon, impossible to accept connection\n");
            return(3);
        }

        client = fdopen(c, "w");

        if (client == NULL) {
            fprintf(stderr, "Map server daemon, impossible to open the socket for writing\n");
            return(4);
        }

        printmap(client, datas);

        fclose(client);
        client = NULL;
    }

    
    fprintf(stdout, "Map server daemon finishing to serv\n");

    return(0);
}


void printmap(FILE* fd, srv_datas_t *datas) {
    int i;


    for (i = 0; i < datas->mapcellsize; i++) {
        fprintf(fd, "%02d%029d\n", datas->mapcell[i].type, datas->mapcell[i].reserved);
    }
}



int init(srv_datas_t *datas) {
    int retload = 0;


    fprintf(stdout, "Map server daemon initialisation\n");

    datas->mapcell = calloc(1, sizeof(map_cell_t));

    if (datas->mapcell == NULL) {
        fprintf(stderr, "Map server daemon initialisation, data cells callocing failed\n");
        return(2);
    }

    retload = loaddatafile(datas);

    if (retload != 0) {
        return(retload);
    }

    fprintf(stdout, "Map server daemon end of initialisation\n");

    return(0);
}



void del(srv_datas_t *datas) {


    fprintf(stdout, "Map server daemon, cleaning datas\n");

    if (datas->mapcell != NULL) {
        free(datas->mapcell);
        datas->mapcell = NULL;
    }

    fprintf(stdout, "Map server daemon, end of datas cleanging\n");
}



/* TODO: avoir dans la structure une zone determinant qu'une ligne a ete 
 * modifiee et que seulement celle-la doit etre mise a jour.
 */
/* TODO: Mettre en place un systeme qui permette aussi l'enregistrement 
 * directe sans passer par un buffer
 */
int savedatasfile(srv_datas_t *datas) {
    FILE* file = NULL;
    int i = 0;
    char *savebuff = NULL;
    int printedchars = 0;


    file = fopen(MAP_DATAS_FILE, "w");
    
    if (file == NULL) {
        fprintf(stderr, "Map server daemon, save file impossible to open\n");
        /* TODO: gerer le fait que c'est impossible d'ouvrir le fichier et que l'enregistrement doit se faire 
         * dans un autre fichier ou attendre que le problème soit resolu
         */
        return(1);
    }

    savebuff = calloc(sizeof(*savebuff), datas->mapcellsize * MAP_DATAS_BLOCK_SIZE);

    if (savebuff == NULL) {
        fprintf(stderr, "Map server daemon, buffer allocation impossible\n");
        fclose(file);
        file = NULL;
        return(2);
    }


    for (i = 0; i < datas->mapcellsize; i++) {
        /* Pas forcement opti */
        sprintf(savebuff, "%s%02d%029d\n", savebuff, datas->mapcell[i].type, datas->mapcell[i].reserved);
    }

    printedchars = fwrite(savebuff, sizeof(*savebuff), datas->mapcellsize * MAP_DATAS_BLOCK_SIZE, file);

    if (printedchars != datas->mapcellsize * MAP_DATAS_BLOCK_SIZE) {
        fprintf(stderr, "Map server daemon, error occured while printing save file\n");
        fclose(file);
        file = NULL;
        free(savebuff);
        savebuff = NULL;
        return(3);
    }

    fclose(file);
    file = NULL;
    free(savebuff);
    savebuff = NULL;

    return(0);
}



int loaddatafile(srv_datas_t *datas) {
    FILE* file = NULL;
    int readsize = 0;
    char readbuff[READ_BLOCK_SIZE + 1];
    char *readdatas = NULL, *tmpreaddatas = NULL;
    int readdatassize = 0;
    int endoffile = 0;


    fprintf(stdout, "Map server daemon loading datas from %s\n", MAP_DATAS_FILE);

    file = fopen(MAP_DATAS_FILE, "r");

    if (file == NULL) {
        fprintf(stderr, "Map server daemon, error opening %s file to load datas (errno:%d)\n", MAP_DATAS_FILE, errno);
        return(1);
    }

    /* Now its a loading with preload, may not be suitable for really big datas and should be avoided if not enough memory is available */
    readbuff[READ_BLOCK_SIZE] = '\0';
    
    do {
        readsize = fread(readbuff, sizeof(*readbuff), READ_BLOCK_SIZE, file);    

        readbuff[readsize] = '\0';

        if (readsize < READ_BLOCK_SIZE) {
            if (feof(file) == 0) {
                fprintf(stderr, "Map server daemon, error reading %s file (errno : %d)\n", MAP_DATAS_FILE, errno);
                free(readdatas);
                readdatas = NULL;
                fclose(file);
                file = NULL;
                return(2);
            }

            endoffile = 1;
        }

        tmpreaddatas = (char *) realloc(readdatas, sizeof(*readdatas) * (readdatassize + readsize));

        if (tmpreaddatas == NULL) {
            fprintf(stderr, "Map server daemon, error reallocating readdatas (errno : %d)\n", errno);
            free(readdatas);
            readdatas = NULL;
            fclose(file);
            file = NULL;
            return(3);
        }

        readdatas = tmpreaddatas;

        readdatassize += readsize;

        strcat(readdatas, readbuff);

    } while (endoffile != 1);

    fclose(file);
    file = NULL;

    fprintf(stdout, "Map server daemon, file %s successfully loaded\n", MAP_DATAS_FILE);

    fprintf(stdout, "Map server daemon, starting to map datas\n");



    char *token;
    int cell;

    if (readdatassize % MAP_DATAS_BLOCK_SIZE != 0) {
        fprintf(stderr, "Map server daemon, an erreur may have occured, consistency check failed\n");
        return(5);
    }

    datas->mapcellsize = readdatassize / MAP_DATAS_BLOCK_SIZE;

    datas->mapcell = calloc(datas->mapcellsize, sizeof(*datas->mapcell));

    if (datas->mapcell == NULL) {
        fprintf(stderr, "Map server daemon, error allocating map cellsi (errno : %d)\n", errno);
        free(readdatas);
        readdatas = NULL;
        return(4);
    }

    /* Chargement des données */
    cell = 0;
    while((token = strsep(&tmpreaddatas, "\n")) != NULL) {
        /* fonction feed(mapcell[cell], line) */
        datas->mapcell[cell].type = 1;
        datas->mapcell[cell].reserved = 1;
        cell++;
    }

    fprintf(stdout, "Map server daemon, map datas successfully mapped\n");

    free(readdatas);
    readdatas = NULL;

    return(0);
}

