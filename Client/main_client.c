#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>
#include <getopt.h>
#include <string.h>      
#include <arpa/inet.h>

#define DEFAULT_PORT 2020
#define IPV4_SIZE 4
#define MAX_LIGNE 1024
#define MAX_LENGTH_CHAINE 255

/*s et la fonction sigint_recu permettent la fermeture du socket en cas de signal SIGINT recu par le serveur */
static int s;



void Mauvais_argument(){
    printf("Les arguments rentrés sont erronés, voici les commandes possibles : -p <port> --port <port> (pour choisir le port de connexion au serveur)\n-a ou --adresse (pour définir l'adresse du serveur)\nPar défaut le client est lancé en TCP à l'adresse 127.0.0.1 sur le port 2020\n");
}

int analyse_syntaxe(int argc, char **argv,short int *service,char* adresse){
    int c;
    int sortie = 2;
    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"adresse",     required_argument, 0,  'a' },
            {"port",     required_argument, 0,  'p' },
        };
        c = getopt_long(argc, argv, "p:a:",long_options, &option_index);
        if (c == -1) break;
        
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
                while(optarg[i] >=48 && optarg[i] <=57 ){
                    port =port*10 +( (short int ) optarg[i]-48);
                    i++;
                }
                *service = port;
                break;
                
            case 'a':
                strcpy(adresse,optarg);
                break;
                
            default:
                printf("?? getopt returned character code 0%o ??\n", c);
        }
    }
    if (optind < argc) {
        sortie = 0;
        Mauvais_argument();
    }
    return sortie;
}

void sigint_recu(int sig)
{
    printf("SIGINT Recu, Deconnexion\n");
    close(s);
    pthread_exit(0);
}

/*Récupère les informations nécessaires pour la connexion au serveur et les rentre dans une struct sockaddr prête à l'emploi, il suffit de lui donner l'addresse IP du serveur au format char* */
int nomVersAdresse(char *hote,struct sockaddr_storage *padresse)
{
    struct addrinfo *resultat,*origine;
    int statut=getaddrinfo(hote,NULL,NULL,&origine);
    if(statut==EAI_NONAME) return -1;
    if(statut<0){ perror("nomVersAdresse.getaddrinfo"); exit(EXIT_FAILURE); }
    struct addrinfo *p;
    for(p=origine,resultat=origine;p!=NULL;p=p->ai_next){
        if(p->ai_family==AF_INET6){ resultat=p; break; }
        memcpy(padresse,resultat->ai_addr,resultat->ai_addrlen);
    }
    return 0;
}

/*Connexion au serveur TCP*/
int connexion_serveur(int port, char* hote)
{
    int s;
    struct sockaddr_in adresse;
    socklen_t taille=sizeof adresse;
    
    /* Creation d'une socket */
    s=socket(PF_INET,SOCK_STREAM,0);
    if(s<0){
        perror("connexionServeur.socket");
        exit(-1);
    }
    
    int vrai =1;
    if(setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&vrai,sizeof(vrai))<0){
        perror("initialisationServeur.setsockopt (REUSEADDR)");
        exit(-1);
    }
    if(setsockopt(s,SOL_SOCKET,SO_REUSEPORT,&vrai,sizeof(vrai))<0){
        perror("initialisationServeur.setsockopt (REUSEPORT)");
        exit(-1);
    }
    
    /*Récupération des informations serveur */
    if(nomVersAdresse(hote,(void *)&adresse)<0) return -1; 
    adresse.sin_port=htons(port);
    
    /* Connexion de la socket a l'hote */
    if(connect(s,(struct sockaddr *)&adresse,taille)<0) return -1;
    else return s;
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

int lectureTLVMessage(char* response, char* messageExtrait){
    
    int Type = 0;
    char *infos = strstr(response, "Type = ");
    Type = infos[7] - 48;
    if(Type == 3){
        sprintf(messageExtrait, "%s", infos+sizeof(char)*29);
        int i = 0;
        while(messageExtrait[i] != '\n'){
            i++;
        }
        messageExtrait[i-1] = '\0';
    }
    
    //Si le type de message n'as pas été trouvé on considére que c'est un message d'attente et on attend le message suivant
    if(Type > 4 || Type < 1){
        Type = 2;
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
    
    //int placeTailleMessage = 3*(tailleLigne + debutLigne) + debutLigne + 16*2 - 2;
    int placeTailleMessage = 0;
    while(message[placeTailleMessage] != 'L'){
        placeTailleMessage++;
    }
    //placeTailleMessage += 8;
    //int placeDebutMessage = 5*(tailleLigne + debutLigne) + debutLigne - 2;
    
    int placeDebutMessage = placeTailleMessage + tailleLigne + debutLigne;
    while(message[placeDebutMessage] != '|'){
        placeDebutMessage++;
    }
    
    printf("placeDebutMessage trouvee : '%d' correspond dans message à : '%c'\n", placeDebutMessage, message[placeDebutMessage]);
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
        while(message[placeTailleMessage] < 48 || message[placeTailleMessage] > 57){
            placeTailleMessage++;
        }
        response[5][0] = message[placeTailleMessage] - 48;
        response[5][1] = message[placeTailleMessage+1] - 48;
        printf("State trouvee : '%d'%d'\n", response[5][0], response[5][1]);
    }
    
    for(int i = 0; i < len; i++){
        placeDebutMessage++;
        if(message[placeDebutMessage] == '|'){
            placeDebutMessage++;
            while(message[placeDebutMessage] != '|'){
                placeDebutMessage++;
            }
            placeDebutMessage++;
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
        sprintf(contenu,"%s",state);
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
            //int placeLigne = espace + 1;
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

int validityCheck(int col, char* tab){
    printf("validityCheck col : '%d'\n", col);
    if(col > 6 || col < 0){
        return 0;
    }
    for(int i = 0; i < 6; i++){
        if(tab[col*6 + i] == '2'){
            return 1;
        }
    }
    
    return 0;
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


void ClientTCP(char* adresse, short int service, int s){
    char *messageComplet = malloc(sizeof(char)*MAX_LIGNE*5);
    char *message = malloc(sizeof(char)*MAX_LENGTH_CHAINE);
    
    s = connexion_serveur(service, adresse);
        
    if(s<0){ 
        fprintf(stderr,"Erreur de connexion au serveur\n");
        perror("Client.connexion_client_tcp.ClientTCP.connect");
        exit(EXIT_FAILURE); 
    }    
    
    recv(s,messageComplet, MAX_LIGNE*5, 0);
    
    int type = lectureTLVMessage(messageComplet,message);
    while(1){
        if(type == 1){
            printf("C'est à vous de parler\n");
            int taille=read(0,message,MAX_LENGTH_CHAINE);
            if(taille <= 0){
                messageComplet = formationTLVMessage(4,"");
            }
            else{
                if(message[taille-1] == '\n'){
                    message[taille-1] = '\0';
                }
                else{
                    message[taille] = '\0';
                }
                messageComplet = formationTLVMessage(3, message);
            }
            taille = strlen(messageComplet);
            write(s,messageComplet,taille);
            type = 2;
        }
        
        if(type == 2){
            read(s,messageComplet,MAX_LIGNE*5);
            type = lectureTLVMessage(messageComplet,message);
        }
        
        if(type == 3){
            printf("Message recu : '%s'\n", message);
            type = 1;
        }
        
        if(type == 4){
            break;
        }
    }
    
    free(message);
    close(s);
}


void ClientTCPV2(char* adresse, short int service, int s){
    char *messageComplet = malloc(sizeof(char)*MAX_LIGNE*5);
    char *message = malloc(sizeof(char)*MAX_LENGTH_CHAINE);
    
    char* pseudo = malloc(sizeof(char)*MAX_LENGTH_CHAINE);
    char *pseudoOpposant = malloc(sizeof(char)*MAX_LENGTH_CHAINE);
    
    s = connexion_serveur(service, adresse);
        
    if(s<0){ 
        fprintf(stderr,"Erreur de connexion au serveur\n");
        perror("Client.connexion_client_tcp.ClientTCP.connect");
        exit(EXIT_FAILURE); 
    }    
    
    int joueur = 0;
    int type = 0;
    
    printf("Entrez votre pseudo:\n");
    int taille = read(0,message,MAX_LENGTH_CHAINE);
    message[taille-1] = '\0';
    messageComplet = formationTLVMessageV2Type1(message);
    write(s,messageComplet, strlen(messageComplet));
    
    while(type != 2){
        recv(s,messageComplet, MAX_LIGNE*5, 0);
        char** messageDecortique = lectureTLVMessageFull(messageComplet);
        type = messageDecortique[0][0];
        printf("message recu : '%s'\n", messageComplet);
        if(type != 2){
            printf("Break\n");
            break;
        }
        joueur = messageDecortique[1][0];
        strcpy(pseudoOpposant, messageDecortique[4]);
    }
    
    printf("Votre opposant est : '%s'\n",pseudoOpposant);
    printf("Vous jouez en position : '%d'\n", joueur+1);
    
    char** messageDecortique;
    int enCours = 1;
    while(enCours == 1){
        while(type != 3){
            int len = recv(s,messageComplet, MAX_LIGNE*5, 0);
            if(len == -1){
                enCours = 0;
                break;
            }
            messageComplet[len] = '\0';
            messageDecortique = lectureTLVMessageFull(messageComplet);
            type = messageDecortique[0][0];
            printf("Message de type : '%d' Recu, message : '%s'\n", type, messageComplet);
        }
        if(type == 6){
            printf("Bravo vous avez gagnee, votre adversaire a abandonnee\n");
            enCours = 0;
            break;
        }
        
        if(type == 7){
            printf("Vous avez perdu la connexion avec votre adversaire\n");
            enCours = 0;
            break;
        }
        type = 0;
        AffichageCleanTable(messageDecortique[3]);
        printf("state : '%d'%d'\n", messageDecortique[5][0],messageDecortique[5][1]);
        if(messageDecortique[5][0] == 0 ){
            if(messageDecortique[5][1] == joueur){
                int check = 0;
                while(check == 0){
                    printf("C'est a vous de jouer, entrez le numero de la colonne ou vous voulez placer votre pion\n");
                    int taille=read(0,message,MAX_LENGTH_CHAINE);
                    if(taille <= 0){
                        messageComplet = formationTLVMessage(6,"");
                        write(s,messageComplet,strlen(messageComplet));
                        enCours = 0;
                        break;
                    }
                    message[taille] = '\0';
                    check = validityCheck(message[0] - 48, messageDecortique[3]);
                    
                }
                messageComplet = formationTLVMessageV2Type4(message[0]-48);
                printf("Message envoyee : '%s'\n", messageComplet);
                write(s,messageComplet,strlen(messageComplet));
                
                int len = recv(s,messageComplet, MAX_LIGNE*5, 0);
                messageComplet[len] = '\0';
                
                printf("Message recu : '%s'\n", messageComplet);
                type = 0;
            }
            else{
                printf("C'est a l'adversaire de jouer\n");
                type = 0;
            }
        }
        
        if(messageDecortique[5][0] == 1){
            if(messageDecortique[5][1] == joueur){
                printf("Vous avez gagnez\n");
            }
            else{
                printf("Vous avez perdu\n");
            }
            enCours = 0;
            break;
        }
        
        if(messageDecortique[5][0] == 2){
            printf("La grille est pleine, c'est une egalite\n");
            enCours = 0 ;
            break;
        }
    }
    /*
    int type = lectureTLVMessage(messageComplet,message);
    while(1){
        if(type == 1){
            printf("C'est à vous de parler\n");
            int taille=read(0,message,MAX_LENGTH_CHAINE);
            if(taille <= 0){
                messageComplet = formationTLVMessage(4,"");
            }
            else{
                if(message[taille-1] == '\n'){
                    message[taille-1] = '\0';
                }
                else{
                    message[taille] = '\0';
                }
                messageComplet = formationTLVMessage(3, message);
            }
            taille = strlen(messageComplet);
            write(s,messageComplet,taille);
            type = 2;
        }
        
        if(type == 2){
            read(s,messageComplet,MAX_LIGNE*5);
            type = lectureTLVMessage(messageComplet,message);
        }
        
        if(type == 3){
            printf("Message recu : '%s'\n", message);
            type = 1;
        }
        
        if(type == 4){
            break;
        }
    }
    */
    free(message);
    close(s);
}


int main(int argc,char *argv[]){
    struct sigaction action;
    action.sa_handler = sigint_recu;
    action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &action, NULL);
    
    short int service = DEFAULT_PORT;
    char* adresse = malloc(sizeof (char)*16);
    sprintf(adresse,"127.0.0.1"); 
    analyse_syntaxe(argc,argv,&service,adresse);
    
    printf("Client TCP vers serveur %s au port : %d\n",adresse,service);
    ClientTCPV2(adresse, service,s);
    
    free(adresse);
    return 0;
}

