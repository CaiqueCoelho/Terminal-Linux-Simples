#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h> //Biblioteca para habilitar a funcao de espera do processo fork
#include <sys/time.h> //Biblioteca para usar alarm
#include <sys/resource.h>
#include <signal.h> //Biblioteca para habilitar alarm
#include <time.h> //Biblioteca para pegar o tempo de cpu
#include <stdbool.h> //Biblioteca para usar tipo bool

#define MAX_LINE 80 /* 80 caracteres são permitidos por comando. */
#define  LIMITE_CPU 0.8 //Limite de uso da cpu
#define MAX_PROCESSOS 99 //Maximo de processos permitidos
#define NANO_SECOND_MULTIPLIER  1000000 //Unidade de conversao de nanosegundos para milissegundos


volatile sig_atomic_t flag = 0; //Flag para usar no alarme
clock_t begin = -1; //Variavel para capturar inicio de execução do processo
clock_t end = -1; //Variavel para capturar final de execução do processo ao tocar um alarme
double tempoNecesssario = -1; //Tempo necessario em milissegundos dentro de 1 um segundo que processo precisara
double tempoGasto; //Tempo real que o processo usou de cpu  (variavel begin - variavel end);
pid_t pidProcesso; //pid do processo ativo
double sleeping; //tempo que processo irá dormir
char elasticoGlobal; //variavel para verificar tipo de execucao do processo corrente
int ret; //variavel para guadar a prioridade original do processo corrente assim que ele é criado

struct estrutura_processo
{
    int id; //Id do processo
    double ms; //Tempo necessario do processo dentro de 1 segundo
    char elastico; //Se ira usar o modo elastico
};

void ALARMhandler(int sig) { // Funcao à executar ao soar o alarme


    end = clock(); //Captura tempo atual, "quando tocou o alarme"

    tempoGasto = (double)(end - begin) / CLOCKS_PER_SEC;  // Calculo tempo real que o processo usou de cpu (variavel begin - variavel end);
    tempoGasto = tempoGasto/1000;

    if(tempoGasto == tempoNecesssario){ //Verifica se o tempo gasto é igual ao necessario
        flag = 1; //seta flag para 1

        if(elasticoGlobal == 'n'){
            setpriority(PRIO_PROCESS, pidProcesso, ret); // Retorna a prioridade do processo para a prioridade original
            struct timespec sleepValue = {0};
            long INTERVAL_MS = (tempoNecesssario - 1000) * NANO_SECOND_MULTIPLIER;
            sleepValue.tv_nsec = INTERVAL_MS;
            nanosleep(&sleepValue, NULL);
        }
        else{
            setpriority(PRIO_PROCESS, pidProcesso, ret); // Retorna a prioridade do processo para a prioridade original
        }

    }
    else{ //Caso nao seja, seta novo alarme
        double tempoAlarme = tempoNecesssario - tempoGasto; //calcula tempo do proximo alarme
        alarm(tempoAlarme); //seta proximo alarme
    }
}

/**
 * setup() lê o próximo comando a partir do prompt, separando os pedaços do 
 * comando em tokens e salvando-os em args.
 * Se houver o caracter '&' no final do comando, então background recebe valor 
 * 1. Caso contrário, recebe valor 0.
 */

void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # de caracteres digitados na linha de comando */
        i,      /* índice para acessar o vetor inputBuffer */
        start,  /* índice onde um parâmetro se inicia */
        ct;     /* índice onde um novo parâmetro será salvo em args[] */
    
    ct = 0;
    
    /* lê a partir da linha de comando e salva dados em inputBuffer */
    printf("Entre com o comando do novo processo:\n");
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);  

    start = -1;
    if (length == 0)
        exit(0);            /* ^d foi digitado, final de comando */
    if (length < 0){
        perror("erro ao ler comando");
	exit(-1);           /* termina com código -1 */
    }

    /* examina cada caracter do inputBuffer */
    for (i=0;i<length;i++) { 
        switch (inputBuffer[i]){
	    case ' ':
	    case '\t' :               /* separador de argumentos */
		if(start != -1){
                    args[ct] = &inputBuffer[start];    
		    ct++;
		}
                inputBuffer[i] = '\0'; /* adiciona um caracter de fim de string */
		start = -1;
		break;

            case '\n':                 /* último caracter do comando */
		if (start != -1){
                    args[ct] = &inputBuffer[start];     
		    ct++;
		}
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* sinaliza fim do command */
		break;

	    default :             /* outros caracteres */
		if (start == -1)
		    start = i;
                if (inputBuffer[i] == '&'){
		    *background  = 1;
                    inputBuffer[i] = '\0';
		}
	} 
     }    
     args[ct] = NULL; /* se entrada > 80 */
} 

int main(void)
{
    signal(SIGALRM, ALARMhandler); //Prepara programa para atender alarmes
    char inputBuffer[MAX_LINE]; /* buffer para armazenar comando digitado */
    int background;             /* 1 se comando é seguido de '&' */
    char *args[MAX_LINE/+1];/* comando de 80 caracteres tem no máxmo 40 argumentos */
    int identificador = 0; //Identificador dos processos
    struct estrutura_processo processos[MAX_PROCESSOS]; //Lista de processos executados/executando

    //Inicializa array de procesos
    /*
    int i = 0;
    for(i = 0; i < MAX_PROCESSOS-1; i++){
        processos[i].id = -1;
        processos[i].ms = 0 ;
        processos[i].elastico = 'n';
    }
    */

    while (1) {           
    	background = 0; //Variavel para verificar background

        struct estrutura_processo processo; //cria um processo para receber informações do terminal
 
        processo.id = identificador; //seta id do processo

    	printf(" COMMAND->\n");
        setup(inputBuffer,args,&background);       /* lê próximo comando */

        printf("Entre com o tempo de cpu necessário a cada 1000 ms (milissegundos):\n");
        scanf("%lf", &processo.ms); //recebe a quantidade de ms necessario em 1s para execucao

        printf("Deseja ativar o modo elástico (y/n): \n");
        scanf(" %c", &processo.elastico); //recebe se ira executar em modo elastico ou nao

        processos[identificador] = processo; //adiciona processo a lista de processos

        identificador++; //incrementa 

        int i; //variavel aux
        double somatoria; //Variavel para realizar a somatoria do uso de cpu dos processos ativos

        for(i = 0; i < identificador; i++){ //Calcula somatoria
            somatoria += processos[i].ms/1000;
        }

        if(somatoria < LIMITE_CPU){ //Verifica se é escalonável
            //criando processo
            pid_t childpid; //Variavel para pid
            int status; //Variavel de status do processo criado

            childpid = fork(); //cria processo filho
            if(childpid == -1) //verifica se filho foi criado com sucesso
            {
                perror("Falha ao criar filho com fork()\n");
                return 1;
            }
            if(processo.elastico == 'n'){ //Verifica se o processo deve ser executado em modo elastico
                 if(childpid == 0){ //Verifica se estou no processo filho
                    while(1){
                        //executando processo
                        ret = getpriority(PRIO_PROCESS, childpid); //Captura prioridade original do proceso
                        setpriority(PRIO_PROCESS, childpid, -20); // Coloca o processo com prioridade maxima
                        double tempoAlarme = processo.ms/1000; //Calculo tempo do alarme
                        tempoNecesssario = tempoAlarme; //Seto tempo necessario
                        elasticoGlobal = processo.elastico;
                        pidProcesso = getpid(); //pego pid do processo corrente
                        alarm(tempoAlarme); //seta o tempo para chamar o alarme
                        begin = clock(); //Guardo o tempo que o processo começa de fato a executar o comando
                        
                        execvp(args[0], &args[0]); //Executo o comando passado

                        /* Codigo comentado abaixo nunca seria executado se execvp executa com sucesso
                        if(flag == 1){//Se flag é 1 processo espera até
                            flag = 0;
                            setpriority(PRIO_PROCESS, childpid, ret); // Retorna a prioridade do processo para a prioridade original
                            struct timespec sleepValue = {0};
                            long INTERVAL_MS = (tempoNecesssario - 1000) * NANO_SECOND_MULTIPLIER;
                            sleepValue.tv_nsec = INTERVAL_MS;
                            printf("processo dormindo\n");
                            nanosleep(&sleepValue, NULL);
                            printf("processo acordando\n");
                        }
                        */
                    }
                }
                else{
                    if(background == 0){
                        while(wait(&status) != childpid); //Se background for 0 processo pai espera proceso filho
                    }
                }       
            }else{ //Aqui executo em modo elastico
                if(childpid == 0){ //Verifico se estou no proceso filho
                    while(1){
                        //executando processo
                        ret = getpriority(PRIO_PROCESS, childpid); //Captura prioridade original do proceso
                        setpriority(PRIO_PROCESS, childpid, -20); // Coloca o processo com prioridade maxima
                        double tempoAlarme = processo.ms/1000; //Calculo tempo do alarme
                        tempoNecesssario = tempoAlarme; //Seto tempo necessario
                        pidProcesso = getpid(); //pego pid do processo corrente
                        alarm(tempoAlarme); // seta o tempo para chamar o alarme
                        begin = clock(); //Guardo o tempo que o processo começa de fato a executar o comando
                        
                        execvp(args[0], &args[0]); //Executo o comando passado

                        /* Codigo comentado abaixo nunca seria executado se execvp executa com sucesso
                        if(flag == 1){
                            flag = 0;
                            setpriority(PRIO_PROCESS, childpid, ret); // Retorna a prioridade do processo para a prioridade original
                        }
                        */
                    }
                }
                else{
                    if(background == 0){
                        while(wait(&status) != childpid); //Se background for 0 processo pai espera proceso filho
                    }
                }
            }
        }
        else{
            printf("Somatorio dos tempos dos processos ativos não é < (menor) que LIMITE_CPU\n");

            char fim;
            printf("Você quer encerrar a execução do shell? (s/n)\n");
            scanf(" %c", &fim);
            if(fim == 's'){
                return 0;
            }
        }
    }
}