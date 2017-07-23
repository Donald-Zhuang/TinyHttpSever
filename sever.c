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
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
int error_die(const char *errmsg)
{
    perror(errmsg);
    exit(1);
}

int startup(u_short *port)
{
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
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
    return close(sock);
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
                if( (n > 0) && (ch == '\n') )
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

void send_str(int client, const char *str)
{
    unsigned int ret = send(client, str, strlen(str), 0);
#if DEBUG_ENABLE
    ret == strlen(str) ?  0 : printf("send_str error[ret = 0x%02x].\r\n", ret);
#endif
}

void not_found(int client)
{
#if DEBUG_ENABLE
        printf("not found.\r\n");
#endif
    send_str(client, "HTTP/1.0 404 NOT FOUND\r\n");
    send_str(client, SERVER_STRING);
    send_str(client, "Content-Type: text/html\r\n");
    send_str(client, "\r\n");
    send_str(client, "<HTML><TITLE>NOT FOUND</TITLE>"
                     "<BODY><P> the sever couldn't fullfill"
                     "your request because the resource specified"
                     "is unavailable or nonexistence."
                     "</BODY></HTML>\r\n");
}
void headers( int client, const char *filename )
{
    (void)filename;
    send_str(client, "HTTP/1.0 200 OK\r\n");
    send_str(client, SERVER_STRING);
    send_str(client, "Content-Type: text/html\r\n");
    send_str(client, "\r\n");
}
void cat( int client, FILE *resource )
{
    char buf[1024];

    fgets( buf, sizeof(buf), resource );
    while(!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}
void serve_file( int client, const char *filename )
{
    FILE *resource = NULL;
    int numofchars = 1;
    char buf[1024] = {'A', '\0',};

    /* read & discard headers */
    while( (numofchars > 0) && strcmp("\n", buf) )
    {
        numofchars = sock_getline( client, buf, sizeof(buf) );
    }

    resource = fopen(filename, "r");
    if( resource == NULL )
    {
        not_found(client);
    }
    else
    {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}
void bad_request(int client)
{
#if DEBUG_ENABLE
    printf("bad request.\r\n");
#endif
    send_str(client, "HTTP/1.0 400 BAD REQUEST\r\n");
    send_str(client, "Content-Type: text/html\r\n");
    send_str(client, "\r\n");
    send_str(client, "<P> Your browse sent a bad request,"
            "such as a POST without a Content-Length.\r\n");
}
void cannot_execute(int client)
{
#if DEBUG_ENABLE
        printf("can not execute.\r\n");
#endif
    send_str(client, "HTTP/1.0 500 Internal Server Error\r\n");
    send_str(client, "Content-Type: text/html\r\n");
    send_str(client, "\r\n");
    send_str(client, "<p>error prohibited CGI execution.\r\n");
}
void execute_cgi( int client, const char *path, const char *method, const char *query_string )
{
    char buf[1024]= {'A', 0,};
    int cgi_in[2]={0,0}, cgi_out[2] = {0,0};
    unsigned int content_length = -1, numofchars = 1;
    char ch = '\0';
    pid_t pid = -1;
    int i = 0, status;

    if (strcasecmp(method, "GET") == 0)
    {
        while( (numofchars > 0) && strcmp("\n",buf) ) //clean the header
        {
            sock_getline(client, buf, sizeof(buf));
        }
    }
    else
    {
        while((numofchars > 0) && strcmp(buf, "\n"))
        {
            buf[14] = '\0';
            if( 0 == strcasecmp(buf, "content-length") )
            {
                content_length = atoi(&buf[16]);
            }
            numofchars = sock_getline(client, buf, sizeof(buf));
        }
        if(content_length == -1)
        {
            bad_request(client);
            return ;
        }
    }
    send_str(client, "HTTP/1.0 200 OK\r\n");

    if( (pipe(cgi_out) < 0) || (pipe(cgi_in) < 0) )
    {
        cannot_execute(client);
        return ;
    }

    if( (pid = fork()) < 0 )
    {
        cannot_execute(client);
        return ;
    }

#define DEFINE_STDIN    (0)
#define DEFINE_STDOUT   (1)
#define DEFINE_STDERR   (2)

    if(pid == 0)
    {
        char meth_env[255], query_env[255], length_env[255];
        dup2(cgi_out[1], DEFINE_STDOUT);
        close(cgi_out[0]);
        dup2(cgi_in[0], DEFINE_STDIN);
        close(cgi_out[1]);

        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);

        if( strcasecmp(method,"GET") == 0 )
        {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }else{
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, path, NULL);
        exit(0);
    }
    else
    {
        close(cgi_in[0]);
        close(cgi_out[1]);
        if(strcasecmp(method, "POST") == 0)
        {
            for(i = 0; i < content_length; i++)
            {
                recv(client, &ch, 1, 0);
                write(cgi_in[1], &ch, 1);
                #if DEBUG_ENABLE
                    printf("%c", ch);
                #endif
            }
        }
        while(read(cgi_out[0], &ch, 1) > 0)
        {
        #if DEBUG_ENABLE
            printf("%c", ch);
        #endif
            send(client, &ch, 1, 0);
        }
        close(cgi_out[0]);
        close(cgi_in[1]);
        waitpid(pid, &status, 0);
    }
}
void *accept_request(void *pclient)
{
    int client = *(int*)pclient;
    char buf[1024];
    char method[255] = {0, };
    char url[255] = {0, };
    char path[255] = {0, };
    char *query_string = NULL;
    struct stat st;
    int i = 0, j = 0, cgi = 0;
    unsigned int numofchars = 0;

    numofchars = sock_getline(client, buf, sizeof(buf));

#if DEBUG_ENABLE
    printf("recieve : %s", numofchars == 0 ? "NULL\r\n" : buf);
#endif

    /*get the method and whether it's valid*/
    while( !isspace((int)buf[i]) && (i < sizeof(method) - 1) )
    {
        method[j++] = buf[i++];
    }
    method[i] = '\0';
    if( strcasecmp(method, "GET") && strcasecmp(method, "POST") )
    {
        bad_request(client);
        return ;
    }

    /* if the method is POST, the CGI should be executed */
    cgi = strcasecmp(method, "POST") == 0 ? 1 : 0;

    /* get the url */
    while( isspace((int)buf[j]) && (j++ < sizeof(buf)) )
        ;
    i = 0;
    while( !isspace((int)buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)) )
    {
        url[i++] = buf[j++];
    }
    url[i] = '\0';
#if DEBUG_ENABLE
    printf("Browser Request-URL: %s\r\n", url);
#endif
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

#if DEBUG_ENABLE
    printf("Request-Path: %s\r\n", path);
#endif

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
        }
        if( (st.st_mode & S_IXUSR ) ||
            (st.st_mode & S_IXGRP ) ||
            (st.st_mode & S_IXOTH ) )
        {
            cgi = 1;
        }

#if DEBUG_ENABLE
        printf("cgi[%d]: goto %s\r\n", cgi, cgi == 0 ? "serve_file":"execute_cgi");
#endif

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
           perror("pthread_create failed");
       }
    }

    cleanup(sever_sock);
    printf("httpd stopped\n");
    return 0;
}
