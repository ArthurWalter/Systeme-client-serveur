#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#define PORT 6000
#define MAX_BUFFER 100

// oui on a pas fait de modele MVC, on aurait pu mettre les strucutes dans un
// fichier struct.h ...
typedef struct {
  int bool_reserve;// booléen :, 1 = place reservée, 0 =  place libre
  char nom[30];
  char numero[10];// numero de dossier : 1 puis 7 fois 0 puis ligne, colonne
} utilisateur;

typedef struct {
  utilisateur siege[10][10];
} salle_concert;// regroupe toutes les places

char buffer[MAX_BUFFER];// mémoire buffer

void red(); // texte en rouge dans le terminal
void reset(); // texte normal dans le terminal
void init_concert(salle_concert *places);// initialiser les places
void afficher_places(salle_concert *places);// afficher les places
void reserver(int fd_socket_comm);// reserver une place
void annuler(int fd_socket_comm);// annuler une reservation
void main_serveur();// configuration du serveur
void saisie_buffer(char buffer[]);// entrer un message

int main(int argc , char const *argv[]) {
  main_serveur();
}

void red (){
    printf("\033[1;31m"); // pour le fun
}
void reset (){
    printf("\033[0m");
}

void reserver(int fd_socket_comm){
  printf("Demande de réservation\n");
  int buffer_recv = recv(fd_socket_comm, buffer, MAX_BUFFER, 0);
  if (buffer_recv > 0) {
     printf("Recu du client : %s\n", buffer);
  }
  char nom[30];
  strcpy(nom, buffer);
  printf("%s\n", nom);

  //partie memoire partagee : --------------
  //c'est compliqué
  key_t clef=ftok(".", 19);
  printf("clef=%d\n", clef);
  int shmid=shmget(clef, sizeof(salle_concert), 0666|IPC_CREAT); // le cours
  if(shmid==-1){
    printf("shm a echoue\n");
    printf("%s\n", strerror(errno));
    exit(0);
  }

  printf("shmid=%d\n", shmid);
  salle_concert* places;
  places=shmat(shmid, NULL, 0);//s'attacher a un segment de memoire partage
  if(places==(void *)(-1)){
    printf("Error 404 la memoire partagee has failed\n");
    exit(0);
  }
  //----------------------------------------
  int i, j;
  for(i=0;i<10;i++){
    for(j=0;j<10;j++){
      buffer[i*10+j]=(*places).siege[i][j].bool_reserve;//sauvegarder les donnees
    }
  }
  afficher_places(places);
  shmdt(places);//se detacher du segment de memoire partage
  send(fd_socket_comm, buffer, 100, 0);//e <-- client
  printf("timelapse...\n");

  int occupe=1;
  while(occupe==1){ // boucle infinie avec condition d'arrêt
    int buffer_recv = recv(fd_socket_comm, buffer, MAX_BUFFER, 0);
    if (buffer_recv > 0) {
       printf("Client a envoyé : %s\n", buffer);
    }
  //Cet partie est pour verifier si la place demandee est libre
    char copie[5];
    strcpy(copie, buffer); // variable de save pour le buffer
    char *ligne, *colonne;
    ligne=strtok(copie, ", ");//token par "," et " "
    colonne=strtok(NULL, ", ");//NULL récupère la chaîne précédente
    int j=atoi(colonne); // string -> int
    int i=atoi(ligne);
    places=shmat(shmid, NULL, 0);//s'attacher a un segment de memoire partage
    // vérification de la place saisie :
    if((*places).siege[i][j].bool_reserve==1){
      shmdt(places);//se detacher du segment de memoire partage
    printf("Place déjà occupée, veuillez saisir une nouvelle place :\n");
      strcpy(buffer, "occupe");
      send(fd_socket_comm, buffer, 100, 0);// send 0 -> serveur
      printf("timelapse...\n");
    } else{
      occupe=0;// <--- condition d'arrêt
      (*places).siege[i-1][j-1].bool_reserve=1;// place occupée
      strcpy((*places).siege[i-1][j-1].nom, nom);// saisie nom
      printf("La place (%d, %d) est réservée avec succès\n", i, j);

      // on genere un numero de dossier pour cette place
      sprintf((*places).siege[i-1][j-1].numero, "%d", 1000000000+i*10+j);//de int a string
      printf("Numéro de dossier : %s\n", (*places).siege[i-1][j-1].numero);
      strcpy(buffer, (*places).siege[i-1][j-1].numero);
      afficher_places(places);
      shmdt(places);//se detacher du segment de memoire partage
      send(fd_socket_comm, buffer, 100, 0);// on envoie les utilisateur de place
      printf("timelapse...\n");
    }// fin else
  }// fin while
}// fin reserver

void main_serveur(){
  int fd_socket_comm;// pour accept()
  int fd_socket_attente;// pour socket()
  int long_adresse;
  int buffer_recv;
  struct sockaddr_in coord_serveur;
  struct sockaddr_in coord_appelant;

  fd_socket_attente = socket(PF_INET, SOCK_STREAM, 0);
  if (fd_socket_attente < 0) {
      printf("socket incorrecte\n");
      exit(EXIT_FAILURE);
  }
  else{
      printf("Serveur\n");// test
  }
  long_adresse = sizeof(struct sockaddr_in);
  memset(&coord_serveur, 0x00, long_adresse);
  coord_serveur.sin_family = PF_INET;
  coord_serveur.sin_port = htons(PORT);
// tous les appareils connectés au réseau
  coord_serveur.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd_socket_attente, (struct sockaddr *) &coord_serveur, sizeof(coord_serveur)) == -1) {
      // coord_serveur sockaddr_in -> sockaddr
      printf("bind error, (faut attendre...)\n");
      exit(EXIT_FAILURE);
  }
  if (listen(fd_socket_attente, 5) == -1) {
      printf("erreur de listen\n");
      exit(EXIT_FAILURE);
  } else{
      printf("Serveur operationnel\n");
  }
  int pid=fork();
  socklen_t tailleCoord = sizeof(coord_appelant);
  if(pid!=0){// le pere

    //partie memoire partage : ------------
    // c'est compliqué
    key_t clef=ftok(".", 19);
    int shmid=shmget(clef, sizeof(salle_concert), 0666|IPC_CREAT);
    if(shmid==-1){
      printf("shm a echoue\n");
      printf("%s\n", strerror(errno));
      exit(0);
    }
    salle_concert *places; // la struct
    places=shmat(shmid, NULL, 0);//s'attacher a un segment de memoire partage
    if(places!=(void *)(-1)){
      init_concert(places);
      afficher_places(places);
      shmdt(places);//se detacher du segment de memoire partage
    }
    //--------------------------------------
    while(pid!=0){
      if ((fd_socket_comm = accept(fd_socket_attente, (struct sockaddr *) &coord_appelant, &tailleCoord)) == -1) {
          printf("accept error\n");
      }
      else{
          char *ip;
          ip=inet_ntoa(coord_appelant.sin_addr);
          printf("Client connecté %s: %d\n",
          ip, ntohs(coord_appelant.sin_port));
          pid=fork();
      }
    }
  }
  if(pid==0){//le fils
    while(1){
      buffer_recv = recv(fd_socket_comm, buffer, MAX_BUFFER, 0);
      if (buffer_recv > 0) {
        buffer[buffer_recv] = 0;
        char *ip;
        ip=inet_ntoa(coord_appelant.sin_addr);

        if(strcmp(buffer,"1")==0){ // reserver
          reserver(fd_socket_comm);
        } else if(strcmp(buffer, "0")==0){
          annuler(fd_socket_comm);
        } else{
          buffer[buffer_recv] = 0;
          char *ip;
          ip=inet_ntoa(coord_appelant.sin_addr);
          //de network byte a adresse IP
          if(strcmp(buffer,"1")==0){//demande de reservation une place
            reserver(fd_socket_comm);
          } else if(strcmp(buffer, "0")==0){
            annuler(fd_socket_comm);
          } else{
            printf("Recu du client %s: %s\n", ip, buffer);
            // on lit et envoie un message au client
            saisie_buffer(buffer);
            send(fd_socket_comm, buffer, strlen(buffer), 0);
            printf("timelapse...\n");
            printf("Recu du client %s: %s\n", ip, buffer);
            // on lit et envoie un message au client
            saisie_buffer(buffer);
            send(fd_socket_comm, buffer, strlen(buffer), 0);
            printf("timelapse...\n");
          }
        }
      }
    }
  }
  close(fd_socket_comm);
  close(fd_socket_attente);
}// fin main_serveur

void saisie_buffer(char buffer[]){
   printf("Entrer: ");
   fgets(buffer, MAX_BUFFER, stdin);
   strtok(buffer, "\n");
}

void init_concert(salle_concert *places){
  int i, j;
  red();
  for(i=0;i<10;i++){
    for(j=0;j<10;j++){
      if(i==j){
        (*places).siege[i][j].bool_reserve=1;// place occupe
      }
      else{
        (*places).siege[i][j].bool_reserve=0;// place libre
      }
    }
  }
}

void afficher_places(salle_concert *places){
  int i, j;
  red();
  for(i=0;i<10;i++){
    for(j=0;j<10;j++){
      printf("%d ", (*places).siege[i][j].bool_reserve);
    }
    printf("\n");//saut de ligne
  }
  reset();
}

void annuler(int fd_socket_comm){
  printf("Annulation de la place réservée :\n");
  strcpy(buffer, "oui");
  send(fd_socket_comm, buffer, 100, 0);
  int buffer_recv = recv(fd_socket_comm, buffer, MAX_BUFFER, 0);
  if (buffer_recv > 0) {//  <-- if---
    printf("%s\n", buffer);
    char *nom, *numero;
    nom=strtok(buffer, ", ");//token
    numero=strtok(NULL, ", ");
    int i=(atoi(numero)-1000000000)/10-1;//string -> int
    int j=(atoi(numero)-1000000000)%10-1;
    //partie memoire partagee : -----------------
    // c'est compliqué
    //mais cest toujours la même chose 2 fois
    key_t clef=ftok(".", 19);
    int shmid=shmget(clef, sizeof(salle_concert), 0666|IPC_CREAT);

    if(shmid==-1){
        printf("shm a echoue\n");
      printf("%s\n", strerror(errno));
      exit(0);
    }

    salle_concert* places;
    places=shmat(shmid, NULL, 0);//s'attacher a un segment de memoire partage
    if(shmat(shmid, NULL, 0)==(void *)(-1)){
      printf("Error 404 la memoire partagee has failed\n");
      exit(0);
    }
    //------------------------------------------
    if(strcmp((*places).siege[i][j].nom, nom)==0 && strcmp((*places).siege[i][j].numero, numero)==0){
      // ---
      (*places).siege[i][j].bool_reserve=0;
      strcpy((*places).siege[i][j].numero, "");
      strcpy((*places).siege[i][j].nom, "");


      afficher_places(places);
      printf("Place (%d, %d) annulée\n", i+1, j+1);
      shmdt(places);
      strcpy(buffer, "oui");
      send(fd_socket_comm, buffer, 100, 0);//send client
    }
    strcpy(buffer, "non");
    send(fd_socket_comm, buffer, 100, 0);//send client
  }// fin if---
}// fin annuler
