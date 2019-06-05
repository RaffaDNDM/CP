#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#define SIZE 60

int findKMP(const char *text, const char *pattern, int m, const int *fail);
void computeFailKMP(char * pattern, int m, int *fail);

int main (int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Request req;
    MPI_Status stat;
    int *size_buff;
    int size_text=SIZE;
    int size_cycle2;
    int *fail;
    char *text,*text2, *pattern= argv[2];
    int m = strlen(pattern);
    int rank_size=0, last_extra=0, rcv_size=0;
    fail=malloc(sizeof(int)*m);

    /**
    *************************** RANK 0 ********************************
    */
    if (rank==0)
    {
        if(argc<2)
        {
            printf("Mancanti parametri di input (nome_programma filepath pattern)");
            exit(1);
        }

        text = malloc(sizeof(char)*size_text);
        FILE *fp= fopen(argv[1], "r");
        int i=0;

        //printf("\n pattern %s\n", pattern);
        while(!feof(fp))
        {
            if(i==size_text)
            {
                size_text=size_text*2;
                text=realloc(text, sizeof(char)*size_text);
            }

            text[i]=fgetc(fp);
            i++;
        }

        size_text=i-1;
        //size_text=i-1;//tolto l'a capo a mano, ma vedere di fare meglio
        text=realloc(text, sizeof(char)*(size_text+1));
        //inizio verifica
        text[size_text]=0;

        //fine verifica
        printf("\n%s\n", text);
        printf("size_text %d\n", size_text);


        i=0;
        for(;i<m;i++)
        {
            fail[i]=0;
        }
        computeFailKMP(pattern, m, fail);

        rank_size=(size_text-m+1)/size;
        last_extra=(size_text-m+1)%size;

        int begin_r=-1;
        int begin_w=-1;
        size_cycle2=size*((2*(m-1)))+1;
        text2=malloc(sizeof(char)*size_cycle2);

        for (i=0; i<size-1; i++)
        {
            int j=0;
            begin_r=((i+1)*rank_size)-m+1;
            begin_w=i*2*(m-1);
            for(;j<2*(m-1);j++)
            {
                text2[begin_w+j]=text[begin_r+j];
            }
        }
        begin_r=size_text-(2*(m-1));
        begin_w=i*(2*(m-1));
        int j=0;
        for (; j<2*(m-1); j++)
        {
            text2[begin_w+j]=text[begin_r+j];
        }
        text2[size_cycle2-1]=0;

        printf("\ntext2: \n%s\n", text2);



        MPI_Isend(&last_extra, 1, MPI_INT, size-1, 1, MPI_COMM_WORLD, &req);
    }

    MPI_Bcast(&rank_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    rcv_size=rank_size;
    if (rank==size-1)
    {
        MPI_Recv(&last_extra, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &stat);
        rcv_size=rcv_size+last_extra;
    }

    printf("\nrank %d rcv_size %d\n", rank, rcv_size );
    char *rcv_buff=malloc(sizeof(char)*(rcv_size+1));
    char *rcv_buff2=malloc(sizeof(char)*((2*m)-1));
    //printf("rank=%d\n",rank);
    //MPI_Barrier(MPI_COMM_WORLD);
    //passaggio di N/numero_processori elementi a ciascun processore
    MPI_Scatter(text, rank_size, MPI_CHAR, rcv_buff, rank_size, MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    printf("\n%ld\n",strlen(rcv_buff));
    //passaggio del resto di elementi all'ultimo processore
    //(nel caso in cui il numero di caratteri del testo non sia multiplo del numero dei processori)
    if(rank==0)
    {
        MPI_Isend(&text[size*rank_size], last_extra, MPI_CHAR, size-1, 1, MPI_COMM_WORLD, &req);
    }

    if(rank==size-1)
    {
        MPI_Recv(&rcv_buff[rank_size], last_extra, MPI_CHAR, 0, 1, MPI_COMM_WORLD, &stat);
    }

    printf("\n%d %ld %s\n",rank, strlen(rcv_buff) , rcv_buff);

    //PASSARE FAIL A TUTTI
    MPI_Bcast(fail, m, MPI_INT, 0, MPI_COMM_WORLD);
    //MPI_Barrier(MPI_COMM_WORLD);

    MPI_Scatter(text, rcv_size, MPI_CHAR, rcv_buff, rcv_size, MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Scatter(text2, 2*(m-1), MPI_CHAR, rcv_buff2, 2*(m-1), MPI_CHAR, 0, MPI_COMM_WORLD);

    //--------------Ciclo 1-----------------------
    int result_cycle1= findKMP(rcv_buff, pattern, m, fail);
    if(result_cycle1!=-1)
    {
        result_cycle1=result_cycle1+(rank_size*rank);
    }

    //------------Ciclo 2-------------------------
    int result_cycle2= findKMP(rcv_buff2, pattern, m, fail);
    if(result_cycle2!=-1)
    {
        result_cycle2=result_cycle2+(rank_size*rank);//DA CORREGGERE
    }

    int result=-1;
    MPI_Reduce(&result_cycle1, &result, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);

    if(rank==0)
    {
        printf("\nresult: %d\n",result);
    }

    if (rank==0)
    {
        free(text);
        free(fail);
    }
    getchar();
    MPI_Finalize();
    return 0;
}

int findKMP(const char *text, const char *pattern, int m, const int *fail)
{
  int n = strlen(text);
  int j=0; //indice che scandisce il testo
  int k=0; //indice che scandisce il pattern

  while (j<n)
  {
    if(text[j]==pattern[k])
    {
      if(k==m-1)// se ho raggiunto la fine del pattern restituisco l'indice di inizio dell'occorrenza
      {
        return j-m+1;
      }

      j=j+1;
      k=k+1;
    }
    else if (k>0) //se non ho corrispondenza ma non sono al primo elemento del pattern
    {
      k=fail[k-1];
    }
    else //se sono all'inizio del pattern e non ho corrispondenza
      {
        j++;
      }
  }
  return -1;//non ha trovato occorrenze
}

void computeFailKMP(char * pattern, int m, int *fail)
{
   int j=1;
   int k=0;

   while (j<m)
   {
     if (pattern[j]==pattern[k])
     {
       fail[j]=k+1;
       j=j+1;
       k=k+1;
     }
     else if (k>0)
     {
       k=fail[k-1];
     }
     else
     {
       j=j+1;
     }
   }
}
