#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define ANSWER_TIME_LIMIT 15 // in seconds
#define MAX_USERNAME_LEN 100
extern int errno;

/* portul de conectare la server*/
int port;
bool quit=false;
int q_number;
void print_welcome_message() {
printf("Bine ati venit la QuizzGame!\n");
printf("REGULAMENT:\n");
printf("1.Fiecare jucator se poate inregistra cu un username UNIC.\n");
printf("2.Apoi va raspunde la intrebari, introducand de la tastatura cifra corespunzatoare raspunsului corect.\n");
printf("3.Pentru fiecare raspuns corect veti primi (15 puncte*numarul intrebarii).\n (Dificultatea intrebarilor creste treptat.)\n"); 
printf("4.Daca doriti sa abandonati jocul introduceti cuvantul \"quit\".\n"); 
printf("5.Jocul se incheie cand toti jucatorii au terminat intrebarile, iar punctajul tuturor va fi afisat.\n\n");
}

int main (int argc, char *argv[])
{ 
  int sd;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 
  

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  /* stabilim portul */
  port = atoi (argv[2]);

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]); //127.0.0.1
  /* portul de conectare */
  server.sin_port = htons (port);
  
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("[client]Eroare la connect().\n");
      return errno;
    }

  print_welcome_message();

  //primire numar de intrebari de la server
  if(read (sd, &q_number,sizeof(int)) <= 0)
      {
        perror ("[client]Eroare la read() -question number- de la server.\n");
        return errno;
      }


  printf ("[client]Introduceti un nume de utilizator de maxim %d de caractere: \n", MAX_USERNAME_LEN);
  fflush (stdout);

  _REQUEST_INPUT:
  char buf[100]="\0";
  read (0, buf, sizeof(buf));

  /* trimitere username la server */
  if (write (sd,buf,sizeof(buf)) <= 0)
    {
      perror ("[client]Eroare la write() spre server.\n");
      return errno;
    }
  if(strcmp(buf,"quit\n")==0)
    {
      quit=true; goto _EXIT;
    }
  /* citirea raspunsului dat de server (succesful/unsuccesful registration) */ 
    if(read (sd, buf,sizeof(buf)) <= 0)
      {
        perror ("[client]Eroare la read() de la server.\n");
        return errno;
      }
    if(strcmp(buf,"[SERVER]: NUMELE ACESTA DE UTILIZATOR ESTE LUAT.INTRODUCETI ALTUL:\n")==0)
    {   
        /* afisam mesajul primit (nume invalid) */
        printf ("%s\n", buf);
        goto _REQUEST_INPUT;
    }
    else printf ("[client]Mesajul primit este: %s\n", buf);//nume valid

    printf("Esti gata sa incepi jocul!\n");

    printf("Vei avea de raspuns la %d intrebari!\n\n",q_number);

  for(int i=1;i<=q_number && quit==false;i++)
  { 
    char QandA[1000]="\0";

    //primeste intrebarea
    if( read(sd,QandA,sizeof(QandA)) <= 0 )
    {
      perror ("[client]Eroare la read() de la server.\n");
      return errno;
    }
    printf("[Intrebare]: %s\n",QandA);
    
    char buf[10]="\0";
    //alege raspunsul
    read (0, buf, sizeof(buf));
    buf[strlen(buf)-1]='\0';
    printf("ATI INTRODUS RASPUNSUL: %s\n",buf);
    
    if (write (sd,buf,sizeof(buf)) <= 0)
    {
      perror ("[client]Eroare la write() -raspuns valid- spre server.\n");
      return errno;
    }
    if(strcmp(buf,"quit")==0)
    {
      quit=true;goto _EXIT;
    }
    else //if(isdigit(buf[0])!=0)
    {
      char msg2[100]="\0";
      if (read (sd, msg2,sizeof(msg2)) <= 0)
      {
        perror ("[client]Eroare la read() -feedback raspuns-sau-timp expirat- de la server.\n");
        return errno;
      }
      printf("%s\n",msg2);
    }
    
  }
_EXIT:
  if(quit==false)
  {
    printf("Felicitari! Ai raspuns la toate intrebarile.\n");
    //PUNCTAJELE OBTINUTE
    char mes[1500]="\0";
    if (read (sd, mes,sizeof(mes)) <= 0)
      {
        perror ("[client]Eroare la read() -anuntare castigator- de la server.\n");
        return errno;
      }
    printf("%s",mes);
  }
  else printf("Ai parasit jocul.\n");
  /* inchidem conexiunea, am terminat */
  close (sd);
}