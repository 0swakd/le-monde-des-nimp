
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "mapsrv.h"



#define MAP_DATAS "./mapdatas.dat"
/* considering the \n in block size for now convenience */
#define MAP_DATAS_BLOCK_SIZE 32 
#define READ_BLOCK_SIZE 512



typedef struct {
    map_cell_t *mapcell;
    int mapcellsize;
} srv_datas_t;



int service(srv_datas_t *datas);
int init(srv_datas_t **pdatas);
void del(srv_datas_t **pdatas);
int loaddatafile(srv_datas_t *datas);



int main(int argc, char **argv) {
    pid_t piddeamon = 0;
    int retdaemon = 0;
    int retinitsrv = 0;
    srv_datas_t *datas;
    
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

    retdaemon = service(datas);

    if (retdaemon != 0) {
        del(&datas);
        return(retdaemon);
    }

    del(&datas);

    fprintf(stdout, "Map server daemon terminating\n");

    return(0);
}



int service(srv_datas_t *datas) {


    fprintf(stdout, "Map server daemon starting to serv\n");
    
    fprintf(stdout, "Map server daemon finishing to serv\n");

    return(0);
}



int init(srv_datas_t **pdatas) {
    srv_datas_t *datas = (*pdatas);
    int retload = 0;


    fprintf(stdout, "Map server daemon initialisation\n");

    datas = calloc(1, sizeof(srv_datas_t));

    if (datas == NULL) {
        fprintf(stderr, "Map server daemon initialisation, datas struct callocing failed\n");
        return(1);
    }

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



/* TODO: regler la segfault qui apparait ici, je pense que ça vient de ma gestion magnifique de la donn�e */
void del(srv_datas_t **pdatas) {
    srv_datas_t *datas = (*pdatas);


    fprintf(stdout, "Map server daemon, cleaning datas\n");
    if (datas != NULL) {
/*
        if (datas->mapcell != NULL) {
            free(datas->mapcell);
            datas->mapcell = NULL;
        }
        free(datas);
        datas = NULL;
*/
    }
}



int loaddatafile(srv_datas_t *datas) {
    FILE* file = NULL;
    int readsize = 0;
    char readbuff[READ_BLOCK_SIZE + 1];
    char *readdatas = NULL, *tmpreaddatas = NULL;
    int readdatassize = 0;
    int endoffile = 0;


    fprintf(stdout, "Map server daemon loading datas from %s\n", MAP_DATAS);

    file = fopen(MAP_DATAS, "r");

    if (file == NULL) {
        fprintf(stderr, "Map server daemon, error opening %s file to load datas (errno:%d)\n", MAP_DATAS, errno);
        return(1);
    }

    /* Now its a loading with preload, may not be suitable for really big datas and should be avoided if not enough memory is available */
    readbuff[READ_BLOCK_SIZE] = '\0';
    
    do {
        readsize = fread(readbuff, sizeof(*readbuff), READ_BLOCK_SIZE, file);    

        readbuff[readsize] = '\0';

        if (readsize < READ_BLOCK_SIZE) {
            if (feof(file) == 0) {
                fprintf(stderr, "Map server daemon, error reading %s file (errno : %d)\n", MAP_DATAS, errno);
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

//        fprintf(stdout, "Turn :\n- readsize : %d\n- strlen rb : %d\n- sizeof rb : %d\n- readdatassize : %d\n- strlen rd : %d\n\n", readsize, strlen(readbuff), sizeof(readbuff), readdatassize, strlen(readdatas));
    } while (endoffile != 1);

//    fprintf(stdout, "Read from file %s : \n\n__\n%s\n__\nSize :\n- calculated : %d\n- strlened : %d\n- sizeofed : %d\n", MAP_DATAS, readdatas, readdatassize, strlen(readdatas), sizeof(readdatas));
     
    fclose(file);
    file = NULL;

    fprintf(stdout, "Map server daemon, file %s successfully loaded\n", MAP_DATAS);

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
        datas->mapcell[cell].type = 0;
        datas->mapcell[cell].reserved = 0;
        cell++;
    }

    fprintf(stdout, "Map server daemon, map datas successfully mapped\n");

    free(readdatas);
    readdatas = NULL;

    return(0);
}

