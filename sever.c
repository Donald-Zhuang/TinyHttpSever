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
#include "ctype.h"
#include "sys/stat.h"

#define DEBUG_ENABLE (1)

int error_die(const char *errmsg)
{
    #if DEBUG_ENABLE
    printf("%s\n", errmsg);
    #endif
    perror(errmsg);
    exit(1);
}

int startup(u_short *port)
{
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if( httpd == -1 )
    {
        error_die("socket failed");
    }

    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port   = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if( bind(httpd,(const struct sockaddr *)&name, sizeof(name) ) < 0 )
    {
        error_die("bind failed");
    }

    if(*port == 0)
    {
        int namelen = sizeof(name);
        if( getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1 )
        {
            error_die("getsockname failed");
        }
        *port = ntohs(name.sin_port);
    }
    if( listen(httpd, 5) < 0 )
    {
        error_die("listen failed");
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
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Server: jdbhttpd/0.1.0\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The sever couldn't fullfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailabe or nonexistent.</P>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}
void serve_file( int client, char *path )
{

}
void execute_cgi( int client, char *path, char *method, char *query_string )
{

}
void *accept_request(void *pclient)
{
    int client = *(int*)pclient;
    char buf[1024];
    char method[1024] = {0, };
    char url[255] = {0, };
    char path[255] = {0, };
    char *query_string = NULL;
    struct stat st;
    int i = 0, j = 0, cgi = 0;
    unsigned int numofchars = 0;

    numofchars = sock_getline(client, buf, sizeof(buf));
    buf[numofchars] = '\0';

#if DEBUG_ENABLE
    printf("recieve : %s", numofchars == 0 ? "NULL" : buf);
#endif

    /*get the method and whether it's valid*/
    while( !isspace(buf[i]) && i < (sizeof(method) - 1) )
    {
        method[j++] = buf[i++];
    }
    method[i] = '\0';
    if( strcasecmp(method, "GET") && strcasecmp(method, "POST") )
    {
        printf("This is a wrong request!\n");
        close(client);
        return ;
    }
    
    /* if the method is POST, the CGI should be executed */
    cgi = strcasecmp(method, "POST") == 0 ? 1 : 0;

    /* get the url */
    while( isspace(buf[j]) && (j++ < sizeof(buf)) )
        ;
    i = 0;
    while( !isspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)) )
    {
        url[i++] = buf[j++];
    }
    url[i] = '\0';

    /* process the request */
    if(cgi == 0) /* method : GET */
    {
        query_string = url;
        while( (*query_string != '?') && (*query_string != '\0') )
        {
            query_string++;
        }
        if (*query_string == '?') /* should be process by CGI */
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    /* check whether the file exist */
    sprintf(path, "htdocs%s", url);
    if(path[strlen(path) - 1] == '/')
    {
        strcat(path, "index.html");
    }
    if(stat(path, &st) == -1)
    {
        while( (numofchars > 0) && strcmp("\n", buf) )
        {
            numofchars = sock_getline(client, buf, sizeof(buf));
        }
        not_found(client);
    }
    else
    {
        if( (st.st_mode & S_IFMT) == S_IFDIR )
        {
            strcat(path, "/index.html");
            if( (st.st_mode & S_IXUSR ) || 
                (st.st_mode & S_IXGRP ) ||
                (st.st_mode & S_IXOTH ) )
            {
                cgi = 1;
            }
        }
        if (cgi == 0)
        {
            serve_file(client, path);
        }
        else
        {
            execute_cgi(client, path, method, query_string);
        }
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
           error_die("accept failed");
       }
       if(pthread_create(&newthread, NULL, accept_request, (void *)&client_sock) != 0)
       {
           error_die("pthread_create failed");
       }
    }

    cleanup(sever_sock);
    printf("httpd stopped\n");
    return 0;
}
