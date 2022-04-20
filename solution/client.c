#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#define BUF_SIZE 256

struct pollfd *fds = NULL;

void handler(){
    if(fds != NULL){
        shutdown(fds[0].fd, 2);
        close(fds[0].fd);
        free(fds);
    }
    exit(1);
}

int main(int argc, char *argv []){
    signal(SIGINT, handler);
    signal(SIGTSTP, handler);
    signal(SIGPIPE, handler);
    
    int client_sock, events, error_code, i;
    char buf[BUF_SIZE];
    ssize_t n_read;
    
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    
    if(argc < 3){
        fprintf(stderr, "Не хватает IP или Порта \n");
        return 1;
    }
    
    errno = 0;
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(client_sock == -1){
        fprintf(stderr, "%s (%d): Сокет не был создан: %s\n",
                __FILE__, __LINE__ - 3,  strerror(errno));
        exit(1);
    }

    errno = 0;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    error_code = getaddrinfo(argv[1], argv[2], &hints, &res);
    if(error_code != 0){
        fprintf(stderr, "%s (%d): Ошибка getaddrinfo: %s\n",
                __FILE__, __LINE__ - 3,  gai_strerror(error_code));
        freeaddrinfo(res);
        exit(1);
    }
    if(connect(client_sock, res->ai_addr, res->ai_addrlen) == -1){
        fprintf(stderr, "%s (%d): Сокет не был присоединен: %s\n",
                __FILE__, __LINE__ - 2,  strerror(errno));  
        freeaddrinfo(res);
        exit(1);
    }
    freeaddrinfo(res);
    res = NULL;

    errno = 0;
    fds = malloc(sizeof(struct pollfd) * 2);
    if(fds == NULL){
        fprintf(stderr, "%s (%d): Структура не была создана: %s\n",
                __FILE__, __LINE__ - 3,  strerror(errno));
        exit(1);
    }
    fds[0].fd = client_sock;
    fds[0].events = POLLIN | POLLERR | POLLPRI | POLLHUP;
    fds[0].revents = 0;

    fds[1].fd = 0;
    fds[1].events = POLLIN | POLLERR | POLLPRI | POLLHUP;
    fds[1].revents = 0;
    for(i = 0; i < BUF_SIZE; i++)
        buf[i] = '\0';
    
    for(;;){
        events = poll(fds, 2, 100);
        
        if(events == -1){
            fprintf(stderr, "%s (%d): Проблемы с poll: %s\n",
                    __FILE__, __LINE__ - 3,  strerror(errno));
            free(fds);
            exit(1);
        }
        
        if(events == 0)
            continue;
        
        /* server */
        if(fds[0].revents){
            fds[0].revents = 0;
            n_read = read(fds[0].fd, buf, BUF_SIZE);
            if(n_read == 0){
                shutdown(fds[0].fd, 2);
                close(fds[0].fd);
                free(fds);
                exit(0);
            }
            if(n_read == -1){
                fprintf(stderr, "%s (%d): Ошибка при чтении из сокета: %s\n",
                        __FILE__, __LINE__ - 10,  strerror(errno));
                close(fds[0].fd);
                free(fds);
                exit(1);
            }
            if(n_read > 0) {
                buf[n_read]='\0';
                printf("%s",buf);
            }
        }
        
        /* client */
        if(fds[1].revents){
            char *str;
            fds[1].revents = 0;
            str = fgets(buf, BUF_SIZE, stdin);
            buf[BUF_SIZE - 1] = '\0';
//            i = 0;
//            while(str != NULL && strchr(buf, '\n') == NULL){
//                str = fgets(buf, BUF_SIZE, stdin);
//                buf[BUF_SIZE - 1] = '\0';
//                i = -1;
//            }
//            if(i == -1){
//                printf("### too long ###\n");
//                continue;
//            }
            if(str == NULL){
                close(fds[0].fd);
                free(fds);
                exit(1);
            }
            buf[BUF_SIZE - 1] = '\0';
            write(fds[0].fd, buf, BUF_SIZE-1);
        }
    }
    return 100;
}

