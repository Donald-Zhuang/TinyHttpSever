/***********************************************************/
//     File Name   : main.c
//     Author      : Donald Zhuang
//     E-Mail      : 
//     Create Time : Thu 13 Jul 2017 05:27:22 PM PDT
/**********************************************************/

#include <stdio.h>
#include "sys/socket.h"
#include "sys/types.h"
#include "strings.h"
#include "string.h"
#include "stdlib.h"
#include "netinet/in.h"
#include "unistd.h"    
#include "pthread.h"

int startup(u_short *port)
{
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if( httpd == -1 )
    {
        printf("socket error\n");
        return -1;
    }
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port   = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if( bind(httpd,(const struct sockaddr *)&name, sizeof(name) ) < 0 )
    {
        printf("bind error\n");
        return -1;
    }
    if(*port == 0)
    {
        int namelen = sizeof(name);
        if( getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1 )
        {
            printf("getsockname error\n");
            return -1;
        }
        *port = ntohs(name.sin_port);
    }
    if( listen(httpd, 5) < 0 )
    {
        printf("listen error\n");
        return -1;
    }
    return httpd;
}

int cleanup(int sock)
{
    close(sock);
}

int sock_getline(int sock, char *buf, unsigned int size)
{
    int i = 0;
    char ch = '\0';
    int n = 0;

    if((buf == NULL) && (size == 0) && (sock == -1))
    {
        printf("parameter error, please check %s[%d]\n", __func__, __LINE__);
        return -1;
    }

    while( (i < size - 1) && (ch != '\n') )
    {
        n = recv(sock, &ch, 1, 0);
        if(n > 0)
        {
            if(ch == '\r')
            {
                n = recv(sock, &ch, 1, MSG_PEEK);
                if( (n > 0) && (ch != '\n') )
                {
                    recv(sock, &ch, 1, 0);
                }
                else
                {
                    ch = '\n';
                }
            }
            buf[i] = ch;
            i++;
        }
        else
        {
            ch = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

void *accept_request(void *pclient)
{
    char buf[1024];
    unsigned int numofchars = 0;
    int client = *(int*)pclient;

    numofchars = sock_getline(client, buf, sizeof(buf));
    buf[numofchars] = '\0';
    if(numofchars != 0)
    {
        printf("accept string: %s\n", buf);
    }
    else
    {
        printf("accept nothing\n");
    }

    close(client);
}
int main(int argc,char *argv[])
{
    int sever_sock = -1;
    u_short port = 5277;
    int client_sock = -1;
    struct sockaddr_in client_name;
    unsigned int client_name_len = sizeof(client_name);
    pthread_t newthread;

    sever_sock = startup(&port);
    printf("httpd running on port %d\n", port);
    while(1)
    {
       client_sock = accept(sever_sock,
                            (struct sockaddr *)&client_name,
                            &client_name_len);
       if(client_sock == -1)
       {
           printf("accept error\n");
           return -1;
       }
       if(pthread_create(&newthread, NULL, accept_request, (void *)&client_sock) != 0)
       {
           printf("pthread_create error\n");
           return -1;
       }
    }

    cleanup(sever_sock);
    printf("httpd stopped\n");
    return 0;
}
