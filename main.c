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
int main(int argc,char *argv[])
{
    int sever_sock = -1;
    u_short port = 0;

    sever_sock = startup(&port);
    printf("httpd running on port %d\n", port);
    cleanup(sever_sock);
    printf("httpd stopped\n");
    return 0;
}
