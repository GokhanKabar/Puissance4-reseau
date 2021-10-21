#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <poll.h>

#define DEFAULT_PORT 2020
#define MAX_CONNEXIONS 200
#define MAX_LIGNE 1024
#define MAX_LENGTH_CHAINE 255

static int ecoute;

//Permet l'arret propre du serveur après un Ctrl+C
void recepctrlC(int sig)
{
    close(ecoute);
    printf("arret du serveur UDP SIGINT \n");
    pthread_exit(0);
}

//Fonction expliquant briévement comment lancer le serveur
void Mauvais_argument(){
    printf("Les arguments rentrés sont erronés, voici les commandes possibles : \n-p <port> --port <port> (pour choisir le port du serveur)\n");
}

char* remplissage(int taille, char* message){
    int manque = taille - strlen(message);
    manque = manque/2;
    char* toAdd = malloc(sizeof(char)*manque);
    char* response = malloc(sizeof(char)*taille);
    for(int i = 0; i<manque ; i++){
        strcat(toAdd," ");
        strcat(message," ");
    }
    
    if(taille > (strlen(message) + strlen(toAdd))){
        strcat(message," ");
    }
    sprintf(response,"%s%s",toAdd,message);
    return response;
}

char* formationTLVMessage(unsigned int type, char* message){
    char* firstLigne = malloc(sizeof(char)*MAX_LIGNE);
    char* secondLigne = malloc(sizeof(char)*MAX_LIGNE);
    char* thirdLigne = malloc(sizeof(char)*MAX_LIGNE);
    char* principaleLigne = malloc(sizeof(char)*MAX_LIGNE);
    char possible[2] = {'+','-'};
    char* typePart = malloc(sizeof(char)*16);
    char* lengthPart = malloc(sizeof(char)*16);
    int len = 0;
    
    sprintf(typePart,"Type = %d",type);
    
    if(type == 4 || type == 5){
        len = type - 3;
        sprintf(lengthPart,"Length = %d",len);
        len = strlen(message);
    }else{
        sprintf(lengthPart,"Length = 0");
    }
    
    lengthPart = remplissage(15,lengthPart);
    typePart = remplissage(15,typePart);
    sprintf(principaleLigne,"|%s|%s|",typePart,lengthPart);
    
    if(type == 4 || type == 5){
        len++;
        sprintf(principaleLigne,"%s%s|",principaleLigne,message);
    }
    
    int taille = 16 + 16+1 + len;
    int premiereLigne = 0;
    int deuxiemeLigne = 0;
    for(int i = 0; i < taille; i++){
        thirdLigne[i] = possible[i%2];
        if(possible[i%2] == '+' && i < taille-1){
            if(deuxiemeLigne == 0){
                sprintf(firstLigne,"%s%d",firstLigne,premiereLigne++);
                premiereLigne = premiereLigne % 10;
            }
            else{
                strcat(firstLigne," ");
            }
            sprintf(secondLigne,"%s%d",secondLigne,deuxiemeLigne++);
            deuxiemeLigne = deuxiemeLigne % 10;
        }
        else{
            strcat(firstLigne," ");
            strcat(secondLigne," ");
        }
    }
    
    char* response = malloc(sizeof(char)*MAX_LIGNE*5);
    sprintf(response,"%s\n%s\n%s\n%s\n%s\n", firstLigne,secondLigne,thirdLigne,principaleLigne,thirdLigne);
    return response;
}

int lectureTLVMessage(char* response){
    int Type = 0;
    char *infos = strstr(response, "Type = ");
    Type = infos[7] - 48;
    if(Type < 2 || Type > 4){
        return 2;
    }
    return Type;
}

int lectureTLVMessageType(char* response){
    int Type = 0;
    char *infos = strstr(response, "Type = ");
    Type = infos[7] - 48;
    return Type;
}

char** lectureTLVMessageFull(char* message){
    int tailleLigne = 64 + 2;
    int debutLigne = 0;
    int type = lectureTLVMessageType(message);
    char** response;
    response = malloc(sizeof(char*)*6);
    for(int i = 0 ; i < 6 ; i++){
        response[i] = malloc(sizeof(char)*MAX_LIGNE);
    }
    
    response[0][0] = type;
    printf("\nMessage recu : \n%s\n type : '%d'\n",message, (int) response[0][0]);
    if(type > 5){
        return response;
    }
    
    for(int i = 180; i < 250; i++){
        //if(message[i] == '\n')
         //   printf("place '%d', char message '%c'\n",i,message[i]);
    }
    
    if(type == 4){
        tailleLigne = 24*2 + 2;
        response[1][0] = message[(tailleLigne + debutLigne)*3+debutLigne + 40] - 48;
        printf("(Type 4) Colonne '%d' selectionne\n",(int)response[1][0]);
        return response;
    }
    
    if(type == 5){
        response[1][0] = message[(tailleLigne + debutLigne)*3+debutLigne + 40] - 48;
        response[2][0] = message[(tailleLigne + debutLigne)*3+debutLigne + 56] - 48;
        printf("(Type 5) Colonne '%d' selectionne et acceptation : '%d'\n",(int)response[1][0], (int) response[2][0]);
        return response;
    }
    
    while(message[debutLigne] != '0'){
        debutLigne++;
    }
    printf("(Type < 4 ) debutLigne '%d' selectionne\n",debutLigne);
    
    int placeTailleMessage = 3*(tailleLigne + debutLigne) + debutLigne + 16*2 - 2;
    int placeDebutMessage = 5*(tailleLigne + debutLigne) + debutLigne - 2;
    int len = 0;
    
    if(type == 2){
        response[1][0] = message[(tailleLigne + debutLigne)*3-3+debutLigne + 41] - 48;
        printf("(Type 2) couleur joueur '%d'\n",(int)response[1][0]);
        placeTailleMessage += 16;
    }
    
    while(message[placeTailleMessage] < 48 || message[placeTailleMessage] > 57){
        placeTailleMessage++;
    }
    printf("(Type < 4 ) placeTailleMessage '%d' selectionne\n",placeTailleMessage);
    
    
    while(message[placeTailleMessage] >=48 && message[placeTailleMessage] <=57 ){
        len =len*10 +( (int ) message[placeTailleMessage]-48);
        placeTailleMessage++;
        printf("len : '%d'\n",len);
    }
    printf("(Type < 4 ) TailleMessage trouvée '%d' et message[%d] : '%c'\n",placeTailleMessage, placeTailleMessage, message[placeTailleMessage]);
    
    
    if(type == 3){
        sprintf(response[5], "%d", len);
        len = 42;
    }
    
    for(int i = 0; i < len; i++){
        placeDebutMessage++;
        if(placeDebutMessage == '|'){
            placeDebutMessage  += tailleLigne + debutLigne + 3 + debutLigne;
        }
        response[3][i] = message[placeDebutMessage];
    }
    printf("(Type < 4 ) Message trouvée '%s'\n",response[3]);

    if(type == 2){
        placeTailleMessage = 37*(tailleLigne + debutLigne) + debutLigne -2;
        placeDebutMessage = 39*(tailleLigne + debutLigne) + debutLigne -2;
        
        while(message[placeTailleMessage] < 48 || message[placeTailleMessage] > 57){
            placeTailleMessage++;
        }
        
        len = 0;
        while(message[placeTailleMessage] >=48 && message[placeTailleMessage] <=57 ){
            len =len*10 +( (int ) message[placeTailleMessage]-48);
            placeTailleMessage++;
            printf("len : '%d'\n",len);
        }
        
        for(int i = 0; i < len; i++){
            placeDebutMessage++;
            if(placeDebutMessage == '|'){
                placeDebutMessage  += tailleLigne + debutLigne + 3 + debutLigne;
            }
            response[4][i] = message[placeDebutMessage];
        }
        printf("(Type < 4 ) Message trouvée '%s'\n",response[4]);
    }
        
    return response;
}

char* formationTLVMessageV2(unsigned int type, char* message, int joueur, int start, char* opposant, char* state){
    int nbLigne = 37;
    int espace = 2;
    int num = 1;
    while(nbLigne*2 > num){
        espace++;
        num *=10;
    }
    
    char* messageComplet[nbLigne];
    for(int i = 0; i < nbLigne ; i++){
        messageComplet[i] = malloc(sizeof(char)*MAX_LIGNE);
        if(i > 2 && i%2 == 1){
            char* temp = malloc(sizeof(char)*MAX_LIGNE);
            sprintf(temp, "%d", (i-3)*2 + start);
            messageComplet[i] = remplissage(espace,temp);
            free(temp);
        }else{
            for(int j = 0; j < espace ; j++){
                messageComplet[i][j] = ' ';
            }
        }
            
    }
    char* pseudo = malloc(sizeof(char)*MAX_LIGNE);
    char* contenu = malloc(sizeof(char)*MAX_LIGNE);
    char* QuatriemeCase = malloc(sizeof(char)*MAX_LIGNE);
    
    char possible[2] = {'+','-'};
    char* typePart = malloc(sizeof(char)*16);
    char* lengthPart = malloc(sizeof(char)*16);
    int len = 0;
    int largeurTotal = 64;
    int petiteLargeur = 48;
    
    sprintf(typePart,"Type = %d",type);
    if(type == 1){
        sprintf(lengthPart,"Length = 65");
        strcpy(pseudo, message);
        len = strlen(message);
        sprintf(contenu,"%d",len);
        contenu = remplissage(15, contenu);
        strcat(QuatriemeCase, "|               ");
    }else if(type == 2){
        petiteLargeur = largeurTotal;
        sprintf(lengthPart,"Length = 131");
        strcpy(pseudo, message);
        sprintf(contenu,"%d",joueur);
        contenu = remplissage(15, contenu);
        sprintf(QuatriemeCase,"%ld", strlen(pseudo));
        QuatriemeCase = remplissage(16,QuatriemeCase);
        QuatriemeCase[0] = '|';
    }else if(type == 0){
        petiteLargeur = largeurTotal/4;
        strcpy(pseudo, message);
        sprintf(contenu,"%d",joueur);
        contenu = remplissage(15, contenu);
        sprintf(QuatriemeCase,"%ld", strlen(pseudo));
        QuatriemeCase = remplissage(15,QuatriemeCase);
        //strcat(QuatriemeCase, remplissage(48," "));
    }else if(type == 3){
        petiteLargeur = largeurTotal/2;
        sprintf(lengthPart,"Length = 44");
        contenu[0] = state[0];
        contenu[1] = state[1];
        contenu[2] = '\0';
        printf("contenu : '%d'%d' et state : '%d'%d'\n",contenu[0], contenu[1], state[0], state[1]);
        contenu = remplissage(31, contenu);
        //sprintf(QuatriemeCase,"%ld", strlen(pseudo));
        //QuatriemeCase = remplissage(15,QuatriemeCase);
    }else{
        sprintf(lengthPart,"Length = 0");
    }
    
    if(type >= 1){
        lengthPart = remplissage(15,lengthPart);
        typePart = remplissage(15,typePart);
        sprintf(messageComplet[3],"%s|%s|%s|",messageComplet[3],typePart,lengthPart);
        sprintf(messageComplet[3],"%s%s%s",messageComplet[3],contenu, QuatriemeCase);
    }else if(type == 0){
        //strcat(messageComplet[3], QuatriemeCase);
        sprintf(messageComplet[3],"%s|%s|", messageComplet[3],QuatriemeCase);
        for(int i = 16 + espace +1 ; i < largeurTotal + espace;i++){
            //messageComplet[3][i] = ' ';
        }
    }
    
    int premiereLigne = 0;
    int deuxiemeLigne = 0;
    for(int i = 0; i < largeurTotal; i++){
        messageComplet[2][i+espace] = possible[i%2];
        if(possible[i%2] == '+'){
            if(deuxiemeLigne == 0){
                sprintf(messageComplet[0],"%s%d",messageComplet[0],premiereLigne++);
                premiereLigne = premiereLigne % 10;
            }
            else{
                strcat(messageComplet[0]," ");
            }
            sprintf(messageComplet[1],"%s%d",messageComplet[1],deuxiemeLigne++);
            deuxiemeLigne = deuxiemeLigne % 10;
        }
        else{
            strcat(messageComplet[0]," ");
            strcat(messageComplet[1]," ");
        }
        
        if(i < petiteLargeur ){
            messageComplet[nbLigne-3][i+espace] = ' ';
            messageComplet[nbLigne-2][i+espace] = ' ';
            messageComplet[nbLigne-1][i+espace] = possible[i%2];
            messageComplet[4][i+espace] = possible[i%2];
        }else if( i == petiteLargeur){
            messageComplet[nbLigne-3][i+espace] = possible[i%2];
            messageComplet[nbLigne-2][i+espace] = '|';
            messageComplet[nbLigne-1][i+espace] = possible[i%2];
            messageComplet[4][i+espace] = possible[i%2];
        }
        else{
            messageComplet[nbLigne-3][i+espace] = possible[i%2];
            messageComplet[nbLigne-2][i+espace] = ' ';
            messageComplet[nbLigne-1][i+espace] = ' ';
            messageComplet[4][i+espace] = ' ';
            if(type == 3){
                messageComplet[4][i+espace] = possible[i%2];
            }else{
                messageComplet[4][i+espace] = ' ';
                messageComplet[3][i+espace] = ' ';
            }
        }
    }
    messageComplet[nbLigne-2][espace] = '|';
    messageComplet[nbLigne-3][espace] = '+';
    
    if(largeurTotal == petiteLargeur){
            messageComplet[nbLigne-2][petiteLargeur+espace] = '|';
            messageComplet[nbLigne-1][petiteLargeur+espace] = '+';
    }
    
    int placeContenu = 0;
    for(int i = 5; i < nbLigne-3 ; i++){
        if(i%2 == 1){
            messageComplet[i][espace] = '|';
            for(int j = espace +1 ; j < largeurTotal + espace ; j++){
                if(placeContenu >= strlen(message)){
                    messageComplet[i][j] = ' ';
                    //break;
                }else{
                    messageComplet[i][j] = message[placeContenu];
                    placeContenu++;
                }
            }
        }
        else{
            messageComplet[i][espace] = '+';
            for(int j = espace +1 ; j < largeurTotal + espace ; j++){
                messageComplet[i][j] = ' ';
            }
        }            
    }
    
    char* response = malloc(sizeof(char)*MAX_LIGNE*5);
    int i =0;
    if(type == 0){
        i = 3;
    }
    
    for(i; i< nbLigne;i++){
        if(i>1 && i < nbLigne-2){
            if(i%2 == 1){
                strcat(messageComplet[i],"|");
            }else{
                strcat(messageComplet[i],"+");
            }
        }
        strcat(messageComplet[i], "\n");
        strcat(response,messageComplet[i]);
    }
    
    if(type == 2){
        char *SecondPart = malloc(sizeof(char)* MAX_LIGNE*5);
        SecondPart = formationTLVMessageV2(0,opposant, -1 ,68, NULL, NULL);
        strcat(response,SecondPart);
    }
    return response;
}

char* formationTLVMessageV2Type5(int col, int ok){
    char* message = malloc(sizeof(char)*MAX_LIGNE);
    char* message1 = malloc(sizeof(char)*MAX_LIGNE);
    char* message2 = malloc(sizeof(char)*MAX_LIGNE);
    sprintf(message1,"%d",col);
    message1 = remplissage(15, message1);
    sprintf(message2,"%d",ok);
    message2 = remplissage(15, message2);
    
    sprintf(message, "%s|%s",message1,message2);
    return formationTLVMessage( 5,  message);
}

char* formationTLVMessageV2Type4(int col){
    char* message = malloc(sizeof(char)*MAX_LIGNE);
    sprintf(message,"%d",col);
    message = remplissage(15, message);
    return formationTLVMessage( 4,  message);
}

char* formationTLVMessageV2Type3(char* message, char* state){
    return formationTLVMessageV2( 3,  message, -1, 0, NULL, state);
}

char* formationTLVMessageV2Type2( char* message, int joueur, char* opposant){
    return formationTLVMessageV2( 2,  message, joueur, 0, opposant, NULL);
}

char* formationTLVMessageV2Type1( char* message){
    return formationTLVMessageV2( 1, message, -1 , 0, NULL, NULL);
}

    
char* AffichageCleanTable(char* infos){
    printf("Puissance 4 :\n-----------------------------\n|");
    for(int i = 5; i >= 0; i--){
        for(int j = 0; j < 7; j++){
            //temp[i][j] = infos[j*6 + i];
            printf(" %c |",infos[j*6+i]);
            //printf(" %d |", j*6+i);
        }
        printf("\n-----------------------------\n|");
    }
    return NULL;
}

int CheckWin(char* infos){
    //check les lignes
    for(int i = 5; i >= 0; i--){
        for(int j = 0; j < 4; j++){
            int somme = infos[j*6 + i] + infos[(j+1)*6 + i] +infos[(j+2)*6 + i] +infos[(j+3)*6 + i];
            if(infos[j*6 + i] == infos[(j+1)*6 + i] && infos[(j+1)*6 + i] == infos[(j+2)*6 + i] && infos[(j+2)*6 + i] == infos[(j+3)*6 + i]){
                if(somme == 0 + 48*4){
                    return 0;
                }
                
                if(somme == 4+ 48*4){
                    return 1;
                }
            }
        }
    }
    
    //check sur les Colonnes
    for(int i = 5; i >= 3; i--){
        for(int j = 0; j < 7; j++){
            int somme = infos[j*6 + i] + infos[j*6 + i - 1 ] +infos[j*6 + i - 2] +infos[j*6 + i - 3];
            if(infos[j*6 + i] == infos[j*6 + i - 1 ] && infos[j*6 + i - 1 ] == infos[j*6 + i - 2] && infos[j*6 + i - 2] == infos[j*6 + i - 3]){
                if(somme == 0 + 48*4){
                    return 0;
                }
                
                if(somme == 4+ 48*4){
                    return 1;
                }
            }
        }
    }
    
    //check les diagonales vers le haut
    for(int i = 2; i >= 0; i--){
        for(int j = 0; j < 4; j++){
            int somme = infos[j*6 + i] + infos[(j+1)*6 + i + 1] +infos[(j+2)*6 + i + 2] +infos[(j+3)*6 + i + 3];
            if(infos[j*6 + i] == infos[(j+1)*6 + i + 1] && infos[(j+1)*6 + i + 1] == infos[(j+2)*6 + i + 2] && infos[(j+2)*6 + i + 2] == infos[(j+3)*6 + i + 3]){
                if(somme == 0 + 48*4){
                    return 0;
                }
                
                if(somme == 4+ 48*4){
                    return 1;
                }
            }
        }
    }
    
    //check les diagonales vers le bas
    for(int i = 5; i >= 3; i--){
        for(int j = 0; j < 4; j++){
            int somme = infos[j*6 + i] + infos[(j+1)*6 + i - 1] +infos[(j+2)*6 + i - 2] +infos[(j+3)*6 + i - 3];
            if(infos[j*6 + i] == infos[(j+1)*6 + i - 1] && infos[(j+1)*6 + i - 1] == infos[(j+2)*6 + i - 2] && infos[(j+2)*6 + i - 2] == infos[(j+3)*6 + i - 3]){
                if(somme == 0 + 48*4){
                    return 0;
                }
                
                if(somme == 4+ 48*4){
                    return 1;
                }
            }
        }
    }
    
    //check si il reste de la place dans la grille
    for(int i = 5; i >= 0; i--){
        for(int j = 0; j < 7; j++){
            if(infos[j*6 + i] == '2'){
                return -1;
            }
        }
    }
    
    return -2;
}

char* creationState(int etatGrille, int joueur){
    char* state = malloc(sizeof(char)*2);
    state[0] = etatGrille + 48;
    state[1] = joueur + 48;
    return state;
}

int AddPiece(int joueur,int col, char* puissance4Table){
    if(col > 6 || col < 0){
        return 0;
    }
    for(int i = 0; i < 6; i++){
        if(puissance4Table[col*6 + i] == '2'){
            puissance4Table[col*6+i] = joueur+48;
            return 1;
        }
    }
    
    return 0;
}

//Récupère les deux ports pour mettre le premier dans service et retourner le second
int analyse_syntaxe(int argc, char **argv,short int *service){
    int c;
    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"port",     required_argument, 0,  'p' },
        };
        c = getopt_long(argc, argv, "p:",long_options, &option_index);
        if (c == -1)
            break;
        
        int i = 0;
        short int port = 0;
        switch (c) {
            case 0:
                printf("option %s", long_options[option_index].name);
                if (optarg)
                    printf(" with arg %s", optarg);
                printf("\n");
                break;
                
            case 'p':
                port = 0;
                while(optarg[i] >=48 && optarg[i] <=57 ){
                    port =port*10 +( (short int ) optarg[i]-48);
                    i++;
                }
                *service = port;
                break;
                
            default:
                printf("?? getopt returned character code 0%o ??\n", c);
                Mauvais_argument();
        }
    }
    if (optind < argc) {
        Mauvais_argument();
    }
    return *service;
}

/*Ouverture du serveur sur le port "port". Renvoie le file descriptor de la socket du serveur */
int initialisationServeur_TCP(short int *port,int connexions)
{
    int s;
    struct sockaddr_in adresse;
    socklen_t taille=sizeof adresse;
    int statut;
    
    /* Creation d'une socket */
    s=socket(PF_INET,SOCK_STREAM,0);
    if(s<0){
        perror("initialisationServeur.socket");
        exit(-1);
    }
    int vrai =1;
    if(setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&vrai,sizeof(vrai))<0){
        perror("initialisationServeur.setsockopt (REUSEADDR)");
        exit(-1);
    }
    if(setsockopt(s,SOL_SOCKET,SO_REUSEPORT,&vrai,sizeof(vrai))<0){
        perror("initialisationServeur.setsockopt (REUSEADDR)");
        exit(-1);
    }
    
    /* Specification de l'adresse de la socket */
    adresse.sin_family=AF_INET;
    adresse.sin_addr.s_addr=INADDR_ANY;
    adresse.sin_port=htons(*port);
    statut=bind(s,(struct sockaddr *)&adresse,sizeof(adresse));
    if(statut<0){
        perror("initialisationServeur.bind");
        return -1;
    }
    /* On recupere le numero du port utilise */
    statut=getsockname(s,(struct sockaddr *)&adresse,&taille);
    if(statut<0){
        perror("initialisationServeur.getsockname");
        exit(-1);
    }
    *port=ntohs(adresse.sin_port);
    
    
    /* Taille de la queue d'attente */
    statut=listen(s,connexions);
    if(statut<0){
        perror("initialisationServeur.listen");
        return -1;
    }
    return s;
}

void ServeurTCP(short int* port){
    ecoute = initialisationServeur_TCP(port, MAX_CONNEXIONS);
    
    int serveurOuvert = 1;
    while(serveurOuvert == 1){
        
        struct sockaddr* addr1;
        socklen_t size1 = sizeof(struct sockaddr_in);
        addr1 = (struct sockaddr*) malloc(sizeof(struct sockaddr));

        printf("Attente de la connexion du premier client\n");
        
        //Ouvre le flux de discussion avec le premier client
        int dialogueClient1=accept(ecoute,(struct sockaddr *) &(addr1),&size1);
        
        //envoie du message d'attente
        write(dialogueClient1, formationTLVMessage(2,""), MAX_LIGNE*5);
        
        struct sockaddr* addr2;
        socklen_t size2 = sizeof(struct sockaddr_in);
        addr2 = (struct sockaddr*) malloc(sizeof(struct sockaddr));

        printf("Attente de la connexion du second client\n");
        
        //Ouvre le flux de discussion avec le second client
        int dialogueClient2=accept(ecoute,(struct sockaddr *) &(addr2),&size2);
        
        //Envoie du message GO
        char *message = formationTLVMessage(1, "");
        write(dialogueClient2, message, MAX_LIGNE*5);
        
        int connectee = 1;
        int type = 2;
        while(connectee == 1){
            char* response = malloc(sizeof(char)*MAX_LIGNE*5);

            type = 2;
            while(type == 2){
                int len=read(dialogueClient2,response,MAX_LIGNE*5);
                if(len == -1){
                    dialogueClient2=accept(ecoute,(struct sockaddr *) &(addr2),&size2);
                    message = formationTLVMessage(1,"");
                    write(dialogueClient2, message, MAX_LIGNE*5);
                    read(dialogueClient2,response,MAX_LIGNE*5);
                }
                
                type = lectureTLVMessage(response);
            }
            if(type == 4){
                close(dialogueClient2);
                write(dialogueClient1, response, MAX_LIGNE*5);
                close(dialogueClient1);
                connectee = 0;
                continue;
            }
            
            int ok = write(dialogueClient1, response, MAX_LIGNE*5);
            free(response);
            if(ok == -1){
                dialogueClient1=accept(ecoute,(struct sockaddr *) &(addr1),&size1);
                message = formationTLVMessage(1,"");
                write(dialogueClient1, message, MAX_LIGNE*5);
            }
                
            char* response2 = malloc(sizeof(char)*MAX_LIGNE*5);
            type = 2;
            while(type == 2){
                int len = read(dialogueClient1,response2,MAX_LIGNE*5);
                if(len == -1){
                    dialogueClient1=accept(ecoute,(struct sockaddr *) &(addr1),&size1);
                    message = formationTLVMessage(1,"");
                    write(dialogueClient1, message, MAX_LIGNE*5);
                    read(dialogueClient1,response2,MAX_LIGNE*5);
                }
                
                type = lectureTLVMessage(response2);
            }
            
            if(type == 4){
                close(dialogueClient1);
                write(dialogueClient2, response2, MAX_LIGNE*5);
                close(dialogueClient2);
                connectee = 0;
                continue;
            }
            
            ok = write(dialogueClient2, response2, MAX_LIGNE*5);
            free(response2);
            if(ok == -1){
                dialogueClient2=accept(ecoute,(struct sockaddr *) &(addr2),&size2);
                message = formationTLVMessage(1,"");
                write(dialogueClient2, message, MAX_LIGNE*5);
            }
        }
    }
}

void ServeurTCPV2(short int* port){
    ecoute = initialisationServeur_TCP(port, MAX_CONNEXIONS);
    
    int serveurOuvert = 1;
    while(serveurOuvert == 1){
        char pseudoJ1[64] = "";
        char pseudoJ2[64] = "";
        
        struct sockaddr* addr1;
        socklen_t size1 = sizeof(struct sockaddr_in);
        addr1 = (struct sockaddr*) malloc(sizeof(struct sockaddr));

        printf("Attente de la connexion du premier client\n");
        int dialogueClient1;
        int len = -1;
        
        //Ouvre le flux de discussion avec le premier client
        while(len == -1){
            char* response = malloc(sizeof(char)*MAX_LIGNE*5);
            dialogueClient1 = accept(ecoute,(struct sockaddr *) &(addr1),&size1);
            len=read(dialogueClient1,response,MAX_LIGNE*5);
            if(len == -1){
                continue;
            }
            
            char** messageDecortique1 = lectureTLVMessageFull(response);
            int type = messageDecortique1[0][0];
            if(type == 1){
                strcpy(pseudoJ1, messageDecortique1[3]);
                printf("pseudoJ1 trouvée : '%s'\n", pseudoJ1);
            }
            else{
                close(dialogueClient1);
                printf("Mauvais message recu : '%s'\n", response);
                len = -1;
            }
        }
        
        struct sockaddr* addr2;
        socklen_t size2 = sizeof(struct sockaddr_in);
        addr2 = (struct sockaddr*) malloc(sizeof(struct sockaddr));

        printf("Attente de la connexion du second client\n");
        int dialogueClient2;
        len = -1;
        
        //Ouvre le flux de discussion avec le premier client
        while(len == -1){
            dialogueClient2=accept(ecoute,(struct sockaddr *) &(addr2),&size2);
            printf("pseudoJ1 trouvée : '%s'\n", pseudoJ1);
            char* response = malloc(sizeof(char)*MAX_LIGNE*5);
            len=read(dialogueClient2,response,MAX_LIGNE*5);
            if(len == -1){
                continue;
            }
            char** messageDecortique = lectureTLVMessageFull(response);
            int type = messageDecortique[0][0];
            if(type == 1){
                strcpy(pseudoJ2, messageDecortique[3]);
                printf("pseudoJ2 trouvée : '%s'\n", pseudoJ2);
            }
            else{
                close(dialogueClient2);
                printf("Mauvais message recu : '%s'\n", response);
                len = -1;
            }
        }
        
        printf("joueur1 : '%s', Joueur2 : '%s'\n",pseudoJ1, pseudoJ2);
        char* message = formationTLVMessageV2Type2(pseudoJ1, 0, pseudoJ2);
        write(dialogueClient1, message, strlen(message));        
        
        message = formationTLVMessageV2Type2(pseudoJ2, 1, pseudoJ1);
        write(dialogueClient2, message, strlen(message));
        
        char* puissance4Table = malloc(sizeof(char)*42);
        for(int i = 0; i < 42 ; i++){
            puissance4Table[i] = '2';
        }
        
        printf("TablePuissance4Créé\n");
        AffichageCleanTable(puissance4Table);
        
        char *state = creationState(0,0);
        message = formationTLVMessageV2Type3(puissance4Table,state);
        
        printf("Envoie du message\n%s\n", message);
        write(dialogueClient1,message, MAX_LIGNE*5);
        write(dialogueClient2,message, MAX_LIGNE*5);
        
        int connectee = 1;
        int type = 4;
        while(connectee == 1){
            char* response = malloc(sizeof(char)*MAX_LIGNE*5);

            type = 3;
            while(type != 4){
                len=read(dialogueClient1,response,MAX_LIGNE*5);
                if(len == -1){
                    message = formationTLVMessage(7,"");
                    write(dialogueClient2, message, MAX_LIGNE*5);
                    close(dialogueClient1);
                    close(dialogueClient2);
                    type = 7;
                    connectee = 0;
                    break;
                }
                
                char** responseDecortique = lectureTLVMessageFull(response);
                type = responseDecortique[0][0];
                
                if(type == 4){
                    int pieceAjoutee = AddPiece(0,responseDecortique[1][0], puissance4Table);
                    if(pieceAjoutee == 0){
                        type = 5;
                    }
                    
                    message = formationTLVMessageV2Type5(responseDecortique[1][0], pieceAjoutee);
                    printf("Envoie du message : '%s'\n", message);
                    write(dialogueClient1,message, strlen(message));
                }
                if(type == 6){
                    close(dialogueClient1);
                    write(dialogueClient2, response, strlen(response));
                    close(dialogueClient2);
                    connectee = 0;
                    break;
                }
            }
            
            type = 3;
            int win = CheckWin(puissance4Table);
            if(win == -2){
                state = creationState(2,0);
                type = 4;
                connectee = 3;
            }
            else if(win == -1){
                state = creationState(0,1);
            }
            else {
                state = creationState(1, win);
                type = 4;
                connectee = 3;
            }

            printf("State envoie : '%d'%d'\n", state[0],state[1]);
            message = formationTLVMessageV2Type3(puissance4Table,state);
            printf("Type : '%d', Envoie du message 1 : '%s'\n",type, message);
            write(dialogueClient1,message, strlen(message));
            printf("Envoie du message 2 : '%s'\n", message);
            write(dialogueClient2, message, strlen(message));
            
            if(type == 4){
                break;
            }
            
            while(type != 4){
                printf("entre second while\n");
                len=read(dialogueClient2,response,MAX_LIGNE*5);
                if(len == -1){
                    message = formationTLVMessage(7,"");
                    write(dialogueClient1, message, strlen(message));
                    close(dialogueClient1);
                    close(dialogueClient2);
                    type = 7;
                    connectee = 0;
                    break;
                }
                
                response[len] = '\0';
                char** responseDecortique = lectureTLVMessageFull(response);
                type = responseDecortique[0][0];
                
                if(type == 4){
                    int pieceAjoutee = AddPiece(1,responseDecortique[1][0], puissance4Table);
                    if(pieceAjoutee == 0){
                        type = 5;
                    }
                    
                    message = formationTLVMessageV2Type5(responseDecortique[1][0], pieceAjoutee);
                    write(dialogueClient2,message, strlen(message));
                }
                if(type == 6){
                    close(dialogueClient2);
                    write(dialogueClient1, response, strlen(message));
                    close(dialogueClient1);
                    connectee = 0;
                    continue;
                }
            }
            
            win = CheckWin(puissance4Table);
            if(win == -2){
                state = creationState(2,0);
                type = 4;
                connectee = 3;
            }
            else if(win == -1){
                state = creationState(0,0);
            }
            else {
                state = creationState(1, win);
                type = 4;
                connectee = 3;
            }
            
            message = formationTLVMessageV2Type3(puissance4Table,state);
            write(dialogueClient1,message, strlen(message));
            write(dialogueClient2, message, strlen(message));
        }
    }
}



int main(int argc,char *argv[])
{
    //Mise en place de la capture du signal SIGINT
	struct sigaction action;
	action.sa_handler = recepctrlC;
	sigaction(SIGINT, &action, NULL);
        
    //Récupération du port entré
    short int* service = malloc(sizeof(short int));
    *service = DEFAULT_PORT;
    analyse_syntaxe(argc,argv,service);

    //Lancement du serveur
    ServeurTCPV2(service);

    return 0;
} 


