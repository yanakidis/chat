#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <poll.h>
#include <signal.h>

#define MAX_QUEUE 5
#define MEM_INC_SIZE 8
#define BUF_SIZE 256

struct pollfd * fds, * fds_in;
int clients;

void convert(char * b, int * k, int j){
    int l, tmp = j, p=0;
    while (tmp > 9){
        b[p++] = (tmp % 10) + '0';
        tmp /= 10;
    }
    b[p++] = tmp + '0';
    b[p] = '\0';
    char t;
    for (l =0; l < p/2; l++){
        t = b[l];
        b[l] = b[p - 1 - l];
        b[p - 1 - l] = t;
    }
    *k = p;
}

void handler(int sig){
    int j;
    for (j=1; j<=clients; j++){
        write(fds[j].fd,"### Server is closing\n", strlen("### Server is closing\n"));
        shutdown(fds[j].fd,2);
        close(fds[j].fd);
    }
    shutdown(fds[0].fd,2);
    close(fds[0].fd);
    free(fds);
    free(fds_in);
    exit(0);
}

int main(int argc, char * argv[]){
    signal(SIGINT,handler);
    signal(SIGTSTP,handler);
    signal(SIGTERM,handler);
    signal(SIGPIPE,SIG_IGN);
    
    int main_socket, port, max_clients, events, events_in, temp_socket, i;
    ssize_t n_read;
    char buf[BUF_SIZE];
    struct sockaddr_in adr;
    struct pollfd * temp_fds;

    if(argc < 2){
        fprintf(stderr,"Необходимо указать номер порта в параметрах\n");
        return 1;
    }

    port = atoi(argv[1]);
     
    adr.sin_family = AF_INET;
    adr.sin_port = htons(port);
    adr.sin_addr.s_addr = INADDR_ANY;

    errno = 0;
    main_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (main_socket == -1){
        fprintf(stderr, "%s (%d): Сокет не был создан: %s\n",
                __FILE__, __LINE__ - 3,  strerror(errno));
        exit(1);
    }

    errno = 0;
    if(bind(main_socket, (struct sockaddr *) &adr, sizeof(adr)) == -1){
        fprintf(stderr, "%s (%d): Не удалось привязать к адресу: %s\n",
                __FILE__, __LINE__ - 2,  strerror(errno));  
        exit(1);
    }
    
    errno = 0;
    if(listen(main_socket, MAX_QUEUE) == -1){
        fprintf(stderr, "%s (%d): Не удалось установить сокет в режим TCP: %s\n",
                __FILE__, __LINE__ - 2,  strerror(errno));  
        exit(1);
    }

    max_clients = MEM_INC_SIZE;
    clients = 0;

    errno =  0;
    fds = malloc(sizeof(struct pollfd) * (max_clients + 1));
    fds_in = malloc(sizeof(struct pollfd) * (1));
    if(fds == NULL || fds_in == NULL){
        fprintf(stderr, "%s (%d): Структура(-ы) не была создана: %s\n",
                __FILE__, __LINE__ - 3,  strerror(errno));
        exit(1);
    }

    fds[0].fd = main_socket;
    fds[0].events = POLLIN | POLLERR | POLLPRI | POLLOUT;
    fds[0].revents = 0;
    fds_in[0].fd = 0;
    fds_in[0].events = POLLIN | POLLERR | POLLPRI | POLLHUP;
    fds_in[0].revents = 0;

    for(;;){
        events = poll(fds, clients + 1, 100);
        events_in = poll(fds_in, 1, 10);
        
        if(events == -1 || events_in == -1){
            fprintf(stderr, "%s (%d): Проблемы с poll: %s\n",
                    __FILE__, __LINE__ - 3,  strerror(errno));
            exit(1);
        }

        if(events == 0 && events_in == 0)
            continue;
        
        //printf("Events = %d\n",events);
        char b[10];
        int k,j;
        
        if(fds_in[0].revents){
            char serv_buf[BUF_SIZE];
            int kol;
            errno = 0;
            if (( (kol = read(0,serv_buf,BUF_SIZE))) - 1 == 5){ // возможно \exit
                char temp_serv_buf[6];
                int m;
                for (m=0; m<5; m++) temp_serv_buf[m] = serv_buf[m];
                temp_serv_buf[5] = '\0';
                if (strcmp(temp_serv_buf,"\\exit") == 0) handler(0);
            } else if (kol == -1){
                fprintf(stderr, "%s (%d): Не удалось считать с stdin: %s\n",
                        __FILE__, __LINE__ - 3,  strerror(errno));
                exit(1);
            }
        }
        
        if (events == 0)
            continue;
        
        if(fds[0].revents){
            temp_socket = accept(main_socket, NULL, NULL);
            if(temp_socket == -1){
                fprintf(stderr, "%s (%d): Не удалось принять: %s\n",
                        __FILE__, __LINE__ - 3,  strerror(errno));
                exit(1);
            }
            clients++;

            if(clients >= max_clients){
                max_clients += MEM_INC_SIZE;
                temp_fds = fds;
                fds = realloc(fds, sizeof(struct pollfd) * (max_clients + 1));
                if(fds == NULL){
                    fprintf(stderr, "%s (%d): Ошибка realloc: %s\n",
                            __FILE__, __LINE__ - 3,  strerror(errno));
                    free(temp_fds);
                    exit(1);
                }
            }

            fds[clients].fd = temp_socket;
            fds[clients].events = POLLIN | POLLERR | POLLPRI | POLLHUP;
            fds[clients].revents = 0;
            write(temp_socket,"\n### Welcome to CHAT\n",strlen("\n### Welcome to CHAT\n"));
            //shutdown(temp_socket, SHUT_WR);
            
            for (j=1; j<=clients; j++){
                if (fds[j].fd != -1) write(fds[j].fd,"*** Client " , strlen("*** Client "));
                convert(b,&k,clients);
                if (fds[j].fd != -1) write(fds[j].fd,b,k);
                if (fds[j].fd != -1) write(fds[j].fd," has connected\n", strlen(" has connected\n"));
            }
            //printf("***Client %d has connected\n", clients);
            fds[0].revents=0;
        }

        for(i = 1; i <= clients; i++){
            if(fds[i].revents){
                n_read = read(fds[i].fd, buf, BUF_SIZE-1);
                if(n_read == 0){
                    for (j=1; j<=clients; j++){
                        if (fds[j].fd != -1) write(fds[j].fd,"*** Client " , strlen("*** Client "));
                        convert(b,&k,i);
                        if (fds[j].fd != -1) write(fds[j].fd,b,k);
                        if (fds[j].fd != -1) write(fds[j].fd," has disconnected\n", strlen(" has disconnected\n"));
                    }
                    close(fds[i].fd);
                    fds[i].fd = -1;
                }
                if(n_read == -1){
//                    fprintf(stderr, "%s (%d): Ошибка при чтении из сокета: %s\n",
//                            __FILE__, __LINE__ - 2,  strerror(errno));
                    close(fds[i].fd);
                    fds[i].fd = -1;
                }
                if(n_read > 0 && n_read < BUF_SIZE){
                    buf[n_read]='\0';
                    
                    int g=0,flag=0; //g - первый не пробельный символ (и не ноль)
                    
                    while (buf[g] != '\0'){
                        if (buf[g] != ' ' && buf[g] != '\t' && buf[g] != '\n' && buf[g] != '\r'){
                            flag = 1;
                            break;
                        }
                        g++;
                    }
                    
                    if (!flag) continue;
                    
                    int u = 1, s = g; // s - последний не пробельный символ (и не нулевой)
                    while (buf[g+u] != '\0'){
                        if (buf[g+u] != ' ' && buf[g+u] != '\t' && buf[g+u] != '\n' && buf[g+u] != '\r'){
                            s = g+u;
                        }
                        u++;
                    }
                    
                    char new_buf[BUF_SIZE];
                    int x;
                    for (x=g; x<=s; x++){
                        new_buf[x-g] = buf[x];
                    }
                    new_buf[x-g] = '\0';
                    
                    if (strlen(new_buf)>= 5){ // уже возможно /quit
                        char temp_buf[6];
                        int m;
                        for (m=0; m<5; m++) temp_buf[m] = new_buf[m];
                        temp_buf[5] = '\0';
                        if ( ((strcmp(temp_buf,"\\quit") == 0) && (strlen(new_buf) == 5)) ||
                            ((strcmp(temp_buf,"\\quit") == 0) && (strlen(new_buf) >= 6) && (new_buf[5] == ' ')) ){ //если точно есть /quit
                            shutdown(fds[i].fd,2);
                            close(fds[i].fd);
                            fds[i].fd = -1;
                            for (j=1; j<=clients; j++)
                                if (j != i){
                                    if (fds[j].fd != -1) write(fds[j].fd,"*** Client " , strlen("*** Client "));
                                    convert(b,&k,i); // получаем номер клиента
                                    if (fds[j].fd != -1) write(fds[j].fd,b,k);
                                    if (fds[j].fd != -1) write(fds[j].fd," has disconnected\n",strlen(" has disconnected\n"));
                                    if (fds[j].fd != -1) write(fds[j].fd,"*** His last words: ",strlen("*** His last words: "));
                                    char temp_buf[BUF_SIZE];
                                    if (strlen(new_buf)>=6) {
                                        int s=6;
                                        while (new_buf[s] != '\0'){
                                            temp_buf[s-6] = new_buf[s];
                                            s++;
                                        }
                                        temp_buf[s-6] = '\0';
                                        if (fds[j].fd != -1)  write(fds[j].fd,temp_buf,s-6);
                                    }
                                    if (fds[j].fd != -1) write(fds[j].fd,"\n",strlen("\n"));
                                }
                        } else if (strlen(new_buf) >= 6){ //возможно \users, но не \quit
                            char temp_buf[7];
                            int m;
                            for (m=0; m<6; m++) temp_buf[m] = new_buf[m];
                            temp_buf[6] = '\0';
                            if (strcmp(temp_buf,"\\users") == 0) {
                                int h=6,flag=0;
                                while (new_buf[h] != '\0'){
                                    if (new_buf[h] != ' ' && new_buf[h] != '\t' && new_buf[h] != '\n' && new_buf[h] != '\r'){
                                        flag = 1;
                                        break;
                                    }
                                    h++;
                                }
                                if (!flag) {
                                    write(fds[i].fd,"### Clients online:\n" , strlen("### Clients online:\n"));
                                    int n;
                                    for (n = 1; n<=clients; n++){
                                        if (fds[n].fd != -1) {
                                            if (fds[j].fd != -1) write(fds[i].fd,"### " , strlen("### "));
                                            convert(b,&k,n);
                                            if (fds[j].fd != -1) write(fds[i].fd,b,k);
                                            if (fds[j].fd != -1) write(fds[i].fd,"\n", strlen("\n"));
                                        }
                                    }
                                    if (fds[j].fd != -1) write(fds[i].fd,"\n", strlen("\n"));
                                } else goto L;
                            } else goto L;
                        } else goto L;
                    } else {
                        L: for (j=1; j<=clients; j++){
                            convert(b,&k,i); // получаем номер клиента
                            if (fds[j].fd != -1) write(fds[j].fd,b,k);
                            if (fds[j].fd != -1) write(fds[j].fd,": ",strlen(": "));
                            if (fds[j].fd != -1) write(fds[j].fd,new_buf,strlen(new_buf));
                            if (fds[j].fd != -1) write(fds[j].fd,"\n\n",strlen("\n\n"));
                        }
                    }
                } else {
                    buf[n_read]='\0';
                    while (strlen(buf) == 256) {
                        n_read = read(fds[i].fd, buf, BUF_SIZE);
                        buf[n_read]='\0';
                    }
                    if (fds[i].fd != -1) write(fds[i].fd,"### Your message is too LONG. You need to have less than ",
                                            strlen("### Your message is too LONG. You need to have less than "));
                    convert(b,&k,BUF_SIZE);
                    if (fds[i].fd != -1) write(fds[i].fd,b,k);
                    if (fds[i].fd != -1) write(fds[i].fd," symbols",strlen(" symbols"));
                    if (fds[i].fd != -1) write(fds[i].fd,"\n\n",strlen("\n\n"));
                }
       
            }
            fds[i].revents = 0;
        }
    }
    return 100;
}


