#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <ctype.h>
#include <sqlite3.h>

#define PORT 2908 /* portul folosit */
#define MAX_USERNAME_LEN 56
#define MAX_PLAYERS 100
#define TIMER 10 //in secunde
extern int errno;

struct Player {
int player_index;
char username[MAX_USERNAME_LEN];
int score;
bool exited ; //1 daca iese din joc
bool finished_Quizz ; //1 cand jucatorul termina intrebarile
bool winner;
};

typedef struct client_thData{
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int cl; //descriptorul intors de accept 
}client_thData;

struct Question{
  char q[256];
  char ans1[100];
  char ans2[100];
  char ans3[100];
  char ans4[100];
  int correct_ans;
} questions[50];
int number_of_questions=0;

struct Player players[MAX_PLAYERS];
int num_players=0;
int players_finished=0; 


pthread_mutex_t player_mutex;  

static void *treat(void *); 
void playGame(void *,int);
int registerPlayer(char*);
void announce_winners(void*);
void readAnswerFromPlayer(void*,int,int);
void initQuestionsFromDB();

int main ()
{

  initQuestionsFromDB();

  struct sockaddr_in server;	// structura folosita de server
  struct sockaddr_in from;	
  int nr;		//mesajul primit de trimis la client 
  int sd;		//descriptorul de socket 
  pthread_t th[100];    //Identificatorii thread-urilor care se vor crea
	int i=0;

  pthread_mutex_init(&player_mutex, NULL);

  /* crearea unui socket */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }
  /* utilizarea optiunii SO_REUSEADDR */
  int on=1;
  setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  
  /* pregatirea structurilor de date */
  bzero (&server, sizeof (server));
  bzero (&from, sizeof (from));
  
  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;	
  /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl (INADDR_ANY);
  /* utilizam un port utilizator */
    server.sin_port = htons (PORT);
  
  /* atasam socketul */
  if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server]Eroare la bind().\n");
      return errno;
    }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen (sd, 2) == -1)
    {
      perror ("[server]Eroare la listen().\n");
      return errno;
    }

  /* servim in mod concurent clientii...folosind thread-uri */
  while (1)
  {
    int client;
    client_thData * td; //parametru functia executata de thread     
    int length = sizeof (from);

    printf ("[server]Asteptam la portul %d...\n",PORT);
    fflush (stdout);

       
    /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
    if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
    {
      perror ("[server]Eroare la accept().\n");
      continue;
    }
    
          /* s-a realizat conexiunea, se astepta mesajul */

    td=(struct client_thData*)malloc(sizeof(struct client_thData));	
    td->idThread=i++;
    td->cl=client;

    pthread_create(&th[i], NULL, &treat, td);	      
          
    }  
};				



static void *treat(void * arg)
{		
		struct client_thData tdL; int pindex=-1;
		tdL= *((struct client_thData*)arg);	
		printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
		fflush (stdout);		 
		pthread_detach(pthread_self());		
    
    //trimitem nr de intrebari la client
    if (write (tdL.cl, &number_of_questions, sizeof(int)) <= 0)
		{
		 printf("[Thread %d] ",tdL.idThread);
		 perror ("[Thread]Eroare la write() -nr intrebari- catre client.\n");
		}

    /*INREGISTRAM JUCATORUL (primire username de la client)*/
    _REQUEST_INPUT:
    char username[100]="\0";
    if (read (tdL.cl, username,sizeof(username)) <= 0)
			{
			  printf("[Thread %d]\n",tdL.idThread);
			  perror ("Eroare la citire username de la client.\n");
			
      }
    if(strcmp(username,"quit\n")==0)
      {  
        close ((intptr_t)arg); 
        return (NULL);
      }
    username[strlen(username)-1]='\0'; //fac asta ca sa nu aiba si \n in username

    char invalid[100]="[SERVER]: NUMELE ACESTA DE UTILIZATOR ESTE LUAT.INTRODUCETI ALTUL:\n";
    if((pindex=registerPlayer(username))==-1)
    { 
      if (write (tdL.cl, invalid, sizeof(invalid)) <= 0)
		{
		 printf("[Thread %d] ",tdL.idThread);
		 perror ("[Thread]Eroare la write() -mesaj invalid- catre client.\n");
		}
      goto _REQUEST_INPUT;
    }
    else
    {

    char mesaj[100]="\0";
    strcpy(mesaj,"Te-ai inregistrat cu succes!");

    //trimitere mesaj la client
    if (write (tdL.cl, mesaj, sizeof(mesaj)) <= 0)
		{
		 printf("[Thread %d] ",tdL.idThread);
		 perror ("[Thread]Eroare la write() catre client.\n");
		}

    printf("S-a inregistrat jucatorul cu username-ul : %s\n",players[pindex].username);
		}

    playGame((struct client_thData*)arg,pindex);
    
    while(players_finished != num_players)
    {
      //asteptam ca toti jucatorii sa termine intrebarile
    }
    
    announce_winners((struct client_thData*)arg);
   
    /* am terminat cu acest jucator, inchidem conexiunea */
		close ((intptr_t)arg);
		return(NULL);	
  		
};


void playGame(void *arg,int player_index)
{
  int i;
	struct client_thData tdL; 
	tdL= *((struct client_thData*)arg);

  for(i=1;i<=number_of_questions && players[player_index].exited==false ;i++)
  {  
    //pregatire si trimitere intrebare
     char QandA[1000]="\0";
     strcat(QandA, questions[i].q);
     strcat(QandA,"\n");
     strcat(QandA,questions[i].ans1);
     strcat(QandA,"\n");
     strcat(QandA,questions[i].ans2);
     strcat(QandA,"\n");
     strcat(QandA,questions[i].ans3);
     strcat(QandA,"\n");
     strcat(QandA, questions[i].ans4);
     strcat(QandA,"\n");
     QandA[strlen(QandA)]='\0'; 

     if(write(tdL.cl,QandA,strlen(QandA)+1)<=0)
    {
       printf("[Thread %d] ",tdL.idThread);
       perror ("[Thread]Eroare la write() catre client.\n");
    }
  
    readAnswerFromPlayer((struct client_thData*)arg,i,player_index);
    if(players[player_index].exited==true) goto _REMOVE_PLAYER;

  }
    pthread_mutex_lock (&player_mutex);
    players[player_index].finished_Quizz=true;
    players_finished++;
    pthread_mutex_unlock (&player_mutex);

    _REMOVE_PLAYER:
    if(players[player_index].exited==true)
    {
      pthread_mutex_lock (&player_mutex);
      printf("[SERVER: %s a parasit jocul.\n", players[player_index].username);
      for(int j = player_index; j < num_players; j++)
            { 
                players[j] = players[j+1];
            }
            num_players--;
      pthread_mutex_unlock (&player_mutex);
    }

}

int registerPlayer(char name[100])//returneaza indexul playerului
    {     
      for(int i=0;i<num_players;i++)
      {
        if(strcmp(players[i].username,name)==0)
        return -1;
      }

      pthread_mutex_lock (&player_mutex);
      players[num_players].player_index=num_players;
      int pindex=players[num_players].player_index;
      players[pindex].winner=false;
      num_players++; 
      strcpy(players[pindex].username,name);
      players[pindex].score=0;
      players[pindex].exited=false;
      players[pindex].finished_Quizz=false;
      pthread_mutex_unlock (&player_mutex);

      return pindex; //indexul playerului


    }

void announce_winners(void *arg)
{
  struct client_thData tdL; 
	tdL= *((struct client_thData*)arg);
  struct Player aux;
  char mes[1500]="PUNCTAJELE OBTINUTE SUNT:\n";
  
  pthread_mutex_lock (&player_mutex);
    //ordoneaza vectorul descrescator in functie de punctaj
    for(int i=0;i<num_players-1;i++)
    {
      for(int j=i+1;j<num_players;j++)
      if(players[i].score<players[j].score)
      {
        aux=players[i];
        players[i]=players[j];
        players[j]=aux;
      }
    }
  pthread_mutex_unlock (&player_mutex);
    
    for(int i=0;i<num_players;i++)
    {
      strcat(mes,players[i].username);
      strcat(mes,"--- PUNCTAJ: ");
      char b[30]; sprintf(b,"%d",players[i].score);
      strcat(mes,b);
      strcat(mes,"\n");
    }

      if (write (tdL.cl, mes, sizeof(mes)) <= 0)
      {
        printf("[Thread %d] ",tdL.idThread);
        perror ("[Thread]Eroare la write() -anuntare punctaje- catre client.\n");
      }

}

void verifyAnswer(void *arg,char buffer[10],int player_index,int i)
{   
    int nr;char mesaj[100];
    struct client_thData tdL; 
    tdL= *((struct client_thData*)arg);

    if(strcmp(buffer,"quit")==0) 
    {  
        pthread_mutex_lock (&player_mutex);
        players[player_index].exited=true;
        pthread_mutex_unlock (&player_mutex);
        goto _EXIT;

    }
  else 
    {
      nr=atoi(buffer); //printf("NUMARUL DUPA ATOI: %d\n",nr);
       /*pregatim mesajul de raspuns */
      if(nr==questions[i].correct_ans) 
      {
        strcpy(mesaj,"Raspuns corect!");
        pthread_mutex_lock(&player_mutex);
        players[player_index].score+=((i)*15);
        pthread_mutex_unlock(&player_mutex);
      }
      else
      {
        strcpy(mesaj,"Raspuns gresit!"); printf("Cel corect era:%d \n",questions[i].correct_ans);
      }
      printf("[Thread %d]Trimitem mesajul inapoi:%s\n",tdL.idThread, mesaj);
    
      /* trimitem feedback la raspuns */
    if (write (tdL.cl, mesaj, sizeof(mesaj)) <= 0)
      {
      printf("[Thread %d] ",tdL.idThread);
      perror ("[Thread]Eroare la write() catre client.\n");
      }
    else
      printf ("[Thread %d]Mesajul de feedback a fost trasmis cu succes.\n",tdL.idThread);	
    }
  _EXIT:
}

void readAnswerFromPlayer(void* arg,int i,int player_index) 
{
    struct client_thData tdL; 
    tdL= *((struct client_thData*)arg);
	  char buf2[10]="\0"; 

    fd_set fds;
    int n;
    struct timeval tv;
    // set up the file descriptor set
    FD_ZERO(&fds);
    FD_SET(tdL.cl, &fds);
    // set up the struct timeval for the timeout
    tv.tv_sec = TIMER; //10 sec
    tv.tv_usec = 0;
    // wait until timeout or data received
    n = select(tdL.cl+1, &fds, NULL, NULL, &tv);
    if (n == -1) 
    {   printf("[Thread %d]\n",tdL.idThread);
        perror ("Eroare la select().\n");
    }
    else if (n == 0) //timeout
    {
      //jucatorul a trimis o varianta de raspuns DAR TIMPUL A EXPIRAT
      if ( read (tdL.cl, buf2,sizeof(buf2)) <= 0)
          {
            printf("[Thread %d]\n",tdL.idThread);
            perror ("Eroare la read() -select timeout- de la client.\n");
          
          }

        char mesaj[100]="\0";
        strcpy(mesaj,"TIMPUL A EXPIRAT!RASPUNSUL NU SE VA LUA IN CONSIDERARE!\n");
        if (write (tdL.cl, mesaj, sizeof(mesaj)) <= 0)
                {
                  printf("[Thread %d] ",tdL.idThread);
                  perror ("[Thread]Eroare la write() catre client.\n");
                }

}
else
{
  //jucatorul a trimis o varianta de raspuns
	if ( read (tdL.cl, buf2,sizeof(buf2)) <= 0)
			{
			  printf("[Thread %d]\n",tdL.idThread);
			  perror ("Eroare la read() -select NONtimeout- de la client.\n");
			
			}
	printf ("[Thread %d]Mesajul a fost receptionat:%s\n",tdL.idThread, buf2);

	//TRIMITE MESAJ LA CLIENT CU RASPUNS CORECT SAU RASPUNS GRESIT
  verifyAnswer((struct client_thData*)arg,buf2,player_index,i);
}

}
void initQuestionsFromDB()
{
  sqlite3 *db;
  char *err_msg = 0;
  sqlite3_stmt *res;
    
  int rc = sqlite3_open("data.db", &db);
    
  if (rc != SQLITE_OK) 
  {
      fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
      sqlite3_close(db);
  }
  char* sql_stmt = "SELECT * FROM Questions";

  rc = sqlite3_prepare_v2(db, sql_stmt, -1, &res, 0);

  while (sqlite3_step(res) == SQLITE_ROW) 
  {
    number_of_questions++;
  }
  sqlite3_reset(res);   

  char *sql = "SELECT Id, Question, answer1, answer2, answer3, answer4, right_answer FROM Questions WHERE Id = ?";   

  for(int i=1;i<=number_of_questions;i++)
{
      rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
      if (rc == SQLITE_OK) 
  {
      sqlite3_bind_int(res, 1, i);
  } 
  else 
  {
    fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
  }
      int step = sqlite3_step(res);
      if (step == SQLITE_ROW) {
        
        strcpy(questions[i].q,sqlite3_column_text(res, 1));
        strcpy(questions[i].ans1,sqlite3_column_text(res, 2));
        strcpy(questions[i].ans2,sqlite3_column_text(res, 3));
        strcpy(questions[i].ans3,sqlite3_column_text(res, 4));
        strcpy(questions[i].ans4,sqlite3_column_text(res, 5));
        questions[i].correct_ans=atoi(sqlite3_column_text(res, 6));
        
    } 

}
    
    sqlite3_finalize(res);
    sqlite3_close(db);


}
