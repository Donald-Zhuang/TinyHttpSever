#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int sockfd;
    int len;
    struct sockaddr_in address;
    int result;
    char str[] = "GET /index.html HTTP/1.1\r\n";
    char buf[1024] = {'\0',};

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(5277);
    len = sizeof(address);
    result = connect(sockfd, (struct sockaddr *)&address, len);

    if (result == -1)
    {
        perror("oops: client1");
        return -1;
    }
    write(sockfd, str, sizeof(str));

    while ( 1)
    {
        if( -1 != recv(sockfd, buf, sizeof(buf), 0)  && strlen(buf))
        {
            printf("\t%s\n", buf);
            buf[0] = '\0';
            }
    }
    close(sockfd);
    return 0;
}
