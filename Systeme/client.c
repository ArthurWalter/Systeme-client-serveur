#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <stdlib.h>
#define PORT 6000
#define MAX_BUFFER 100

void main_client(); // on aurait pu ne pas faire de fonction
void red(); // texte en rouge dans le terminal
void reset(); // texte normal dans le terminal
void saisie_buffer(char buffer[]);
void afficher(); // affiche la matrice des places
void reserver(int fd_socket);
void annuler(int fd_socket);

char buffer[MAX_BUFFER];// mémoire tampon
int place_concert[10][10];

int main(int argc , char const *argv[]) {
  main_client();
}

void red (){
    printf("\033[1;31m"); // pour le fun
}
void reset (){
    printf("\033[0m");
}

void reserver(int fd_socket){

  send(fd_socket, buffer, MAX_BUFFER, 0); // le serveur réserve
  printf("timelapse serveur...\n");
  printf("Entrer votre nom d'utilisateur :\n");
  saisie_buffer(buffer);
  send(fd_socket, buffer, MAX_BUFFER, 0); // envoie du nom
  int buffer_recv = recv(fd_socket, buffer, MAX_BUFFER, 0);

  if(buffer_recv>0){ // <-- if1
    printf("Veuillez réserver une place (0 place libre, 1 place occupé) : \n");
    for(int i=0;i<10;i++){
      for(int j=0;j<10;j++){
        place_concert[i][j]=buffer[i*10+j];//remplir les donnees a la place_concert[][]
      }
    }
    afficher();
    printf("Quelle place voulez-vous réserver ? (\"ligne, colonne\")\n");
    int occupe=1;
    while(occupe==1){ // boucle infinie avec condition d'arrêt
      saisie_buffer(buffer);
      char copie[5];
      strcpy(copie, buffer); // variable de save pour le buffer
      char *ligne, *colonne;
      ligne=strtok(copie, ", ");//token par "," et " "
      colonne=strtok(NULL, ", ");//NULL récupère la chaîne précédente
      int j=atoi(colonne); // string -> int
      int i=atoi(ligne);
      // vérification de la place saisie :
      if(place_concert[i-1][j-1]==1){
        printf("Place déjà occupée, veuillez saisir une nouvelle place :\n");
      }else{ // else1
        occupe=0; // <--- condition d'arrêt du while
        printf("timelapse reservation...\n");
        send(fd_socket, buffer, MAX_BUFFER, 0); // send coordonnées de la place saisie
        printf("timelapse serveur...\n");
        int buffer_recv = recv(fd_socket, buffer, MAX_BUFFER, 0);
        if (buffer_recv > 0) { //if2
          if(strcmp(buffer, "occupe")==0){
            printf("Place déjà occupée, veuillez saisir une nouvelle place :\n");
          }else{ // else2
            printf("La place (%d, %d) est réservée avec succès\n", i, j);
            printf("Votre numéro de dossier est %s\n", buffer);
          } // fin else2
        }// fin if1
      } // fin else1
    } // fin while(1)
  } // fin if1
}// fin reserver

void main_client(){

  struct sockaddr_in coord_serveur;
  int buffer_recv;
  int fd_socket;
  int long_adresse;
  fd_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_socket < 0) {
      printf("socket incorrecte\n");
      exit(EXIT_FAILURE);
  }
  else{
      printf("Client\n");
  }
  // On prépare les coordonnées du serveur
  long_adresse = sizeof(struct sockaddr_in);
  memset(&coord_serveur, 0x00, long_adresse);
  coord_serveur.sin_family = PF_INET;
  // adresse du serveur
  inet_aton("127.0.0.1", &coord_serveur.sin_addr);
  // de l'adresse IP a network byte
  // toutes les interfaces locales disponibles
  coord_serveur.sin_port = htons(PORT);
  if (connect(fd_socket, (struct sockaddr *) &coord_serveur,
          sizeof(coord_serveur)) == -1) {
      printf("Connexion impossible\n");
      exit(EXIT_FAILURE);
  }
  else{
       printf("Connexion ok\n");
  }

  printf("Projet Systeme S3 \n");
  while(1){ // boucle infinie avec condition d'arrêt
    printf("Menu de Saisie :\n");
      printf("- (0) : annuler\n");
        printf("- (1) : réserver\n");
          printf("- (2) quitter\n");
    saisie_buffer(buffer);

    if(strcmp(buffer, "0")==0){
      annuler(fd_socket);
    } else if(strcmp(buffer, "1")==0){
      reserver(fd_socket);
    } else if(strcmp(buffer, "2")==0){ // <--- condition d'arrêt
      close(fd_socket);
      printf("Fin du programme\n");
      exit(0);
    }
  }
}

void saisie_buffer(char buffer[]){
    printf("Saisir : ");
    fgets(buffer, MAX_BUFFER, stdin);
    strtok(buffer, "\n");
}

void afficher(){
  int i, j;
  red();
  printf("    1 2 3 4 5 6 7 8 9 10\n");
  for(i=0;i<10;i++){
    if(i==9){
      printf("10| ");
    } else {
      printf(" %d| ", i+1);
    }
    for(j=0;j<10;j++){
      printf("%d ", place_concert[i][j]);
    }
    printf("\n");//saut de ligne
  }
  reset();
}

void annuler(int fd_socket){

  strcpy(buffer, "0");
  send(fd_socket, buffer, MAX_BUFFER, 0); // le serveur annule
  printf("timelapse...\n");
  int buffer_recv = recv(fd_socket, buffer, MAX_BUFFER, 0);

  if(strcmp(buffer, "oui")==0 && buffer_recv>0 ){
    printf("Saisir votre nom et numéro de dossier (\"nom\", \"numero\")\n");
    saisie_buffer(buffer);
    send(fd_socket, buffer, MAX_BUFFER, 0);//envoyer nom et numero au serveur
    printf("timelapse...\n");
  }

  buffer_recv = recv(fd_socket, buffer, MAX_BUFFER, 0);
  if(buffer_recv>0){ // if
    if(strcmp(buffer, "oui")==0){
      printf("Place annulée\n");
    } else if(strcmp(buffer, "non")==0){
      printf("Place toujours réservée\n");
    }// fin else if
  }//fin if
} // fin annuler
