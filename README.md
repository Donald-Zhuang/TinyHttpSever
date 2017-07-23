> Across the great wall, we can reach every corner in the world.
> ——中国第一封email

###TinyHttpd源码分析
[TOC]
#### 1. 背景
一直很好奇web的工作原理，加之这阵子也在学习Python爬虫，就有想法了解这部分的知识，所以买了一本[图解HTTP](https://book.douban.com/subject/25863515)。这本书简洁清晰也很形象地介绍了HTTP协议的工作流程，对零基础了解HTTP协议有着不错的引导作用。书也很薄，可以很快看完。不过纯粹通过看书学习一个协议难免会浮于表面，因此，我找了TinyHttpd的source code来了解http协议的实现和实际工作场景。

#### 2. 源码解析
声明：这篇里面的代码并不是tinyhttpd的源码，是我自己手动临摹一遍的代码，实测跑通了。一直相信代码自己码一遍会比纯看加注释收获多一些。同时，tinyhttpd只有几百行，自己码一遍也不算什么。关于阅读tinyhttpd的source code，个人觉得可以以如下顺序展开：main --> startup --> accept_request --> execute_cgi -->了解cgi实现，因此本文就按照此顺序展开分享。

##### 主体框架 -> main()
main函数是整个httpd的工作框架，具体的实现流程如下， startup创建socket通信并建立端口监听 --> accept等待客户端连接请求 --> accept_request处理客户端http请求 --> cleanup释放资源
``` c
int main(int argc,char *argv[])
{
    int sever_sock = -1;
    u_short port = 5277;
    int client_sock = -1;
    struct sockaddr_in client_name;
    unsigned int client_name_len = sizeof(client_name);
    pthread_t newthread;
 
    sever_sock = startup(&port); //建立socket通讯，并进行端口监听
    printf("httpd running on port %d\n", port);
 
    while(1)
    {
       client_sock = accept(sever_sock,
                            (struct sockaddr *)&client_name,
                            &client_name_len); // 接受客户端请求
       if(client_sock == -1)
       {
           error_die("accept failed");
       }
       if(pthread_create(&newthread, NULL, accept_request, (void *)&client_sock) != 0) // 创建子线程处理客户端请求
       {
           perror("pthread_create failed");
       }
    }
 
    cleanup(sever_sock); // 关闭socket，释放相关资源
    printf("httpd stopped\n");
    return 0;
}
```

##### 基础通讯实现 -> startup()
HTTP是一个应用层协议，通过TCP/IP进行传输的。HTTP协议规定，连接请求从客户端发起，服务端提供资源响应。在客户端无请求的情况下，服务端不会主动发送响应。服务端通讯建立过程： socket创建套接字 --> bind绑定套接字 --> listen监听套接字 --> accept等待客户端连接请求。
``` c
int startup(u_short *port)
{
    int httpd = 0;
    struct sockaddr_in name;
 
    // 创建socket描述符：采用TCP通讯方式，在第二个参数确定的情况下，第三个参数可以传0由函数自动匹配对应协议
    httpd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if( httpd == -1 )
    {
        error_die("socket failed");
    }
 
    // 绑定套接字：绑定IP地址和端口号
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port   = htons(*port); // 指定端口：若端口为0，则自动分配一个端口。将端口转换为网络字节序
    name.sin_addr.s_addr = htonl(INADDR_ANY); //IP地址：INADDR_ANY -> 服务器上所有的IP对应端口号都监听
    if( bind(httpd,(const struct sockaddr *)&name, sizeof(name) ) < 0 )
    {
        error_die("bind failed");
    }
 
    // 若端口为0，获取自动分配的端口号
    if(*port == 0)
    {
        int namelen = sizeof(name);
        if( getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1 ) // 获取套接字信息
        {
            error_die("getsockname failed");
        }
        *port = ntohs(name.sin_port); // 获取端口号： 网络字节序转主机字节序
    }

    // 监听socket
    if( listen(httpd, 5) < 0 ) // 监听httpd，等待客户端连接请求，并设置最大可排队连接数为5个
    {
        error_die("listen failed");
    }
    return httpd;
}
``` 

##### 请求处理 -> accept_request()
accept_request是这个httpd的主体。通过解析http请求，对应发送资源和响应。http请求报文主要由三部分组成： 报文首部（分请求起始行和可选的请求首部字段）、空行、报文主体。通常并不一定要有报文主体。请求报文中每一行都以回车换行（**CRLF**,即"\r\n"）作为结束标志。
``` html
Method URL HTTP_Version<CRLF>    // 请求起始行
Header_Name: Header_Value<CRLF>  // 请求首部字段，可选
... ...
Header_Name: Header_Value<CRLF>
<CRLF>                           // 空行，表示报文首部结束
BODY                             // 报文主体
```
下文我们用来分析的报头首部是用wireshark抓chrome访问httpd时发出的，只有报文首部，没有报文主体。不同浏览器可能有所差异，具体可用wireshark尝试分析。
TinyHttpd主要是针对请求起始行进行处理。请求起始行由Method、Request-Url和Http版本信息组成，三者通过空格隔开。如下请求起始行中"GET"就是method，表示请求访问服务器的类型，用于告知服务器访问意图。"/"为URL，表示请求访问的资源，也称作Request-URL，"HTTP/1.1"表示http版本信息，用来提示客户端使用的http协议功能。
下面的内容为请求首部字段，是可选的，在accept_request的execute_cgi中，我们只有在处理POST请求时才会去解析这部分的内容，对于GET，我们解析请求起始行后会去清除buf中的这部分数据，避免对后续处理或者下次通讯请求造成影响。
``` html
GET / HTTP/1.1                // 请求起始行
Host: 192.168.179.145:5277    // 以下为可选首部字段，格式为Header-Name： Header-Value<CRLF>
Connection: keep-alive
Cache-Control: max-age=0
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Windows NT 6.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8
Accept-Encoding: gzip, deflate, sdch
Accept-Language: zh-CN,zh;q=0.8,en;q=0.6
```
解析请求的具体实现。
``` c
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
 
    numofchars = sock_getline(client, buf, sizeof(buf)); // 获取一行请求报文，以LF（\n）作为结尾。 
#if DEBUG_ENABLE
    printf("recieve : %s", numofchars == 0 ? "NULL\r\n" : buf);
#endif

     // 对于http报文来说，第一行即为请求起始行：method url http-version
    while( !isspace((int)buf[i]) && (i < sizeof(method) - 1) // 获取请求方法
        method[j++] = buf[i++];
    method[i] = '\0';
    
    // strcasecmp为忽略大小写，比较字符串是否相同，相同则返回0，否则参数1长度大于参数2时返回正值，反之返回负值。
    // TinyHttpd只支持GET和POST两种方法
    if( strcasecmp(method, "GET") && strcasecmp(method, "POST") )
    {
        bad_request(client);
        return ;
    }
 
    // 检测请求是POST还是GET，若为POST则需要CGI处理，置起对应标志
    cgi = strcasecmp(method, "POST") == 0 ? 1 : 0; 
 
    //清除多余空格
    while( isspace((int)buf[j]) && (j++ < sizeof(buf)) )  
        ;
    i = 0;
    //获取URL，用于确定访问什么资源
    while( !isspace((int)buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)) )
    {
        url[i++] = buf[j++];
    }
    url[i] = '\0';
#if DEBUG_ENABLE
    printf("Request-URL： %s\r\n", url);
#endif
    
    /* process the request */
    if(cgi == 0) /* method : GET */
    {
        query_string = url;
        // 若GET请求的URL带?,则表明有查询参数，须CGI处理
        while( (*query_string != '?') && (*query_string != '\0') ) 
            query_string++;
        if (*query_string == '?') /* should be process by CGI */
        {
            cgi = 1;
            *query_string = '\0';
            query_string++; //截取查询的字符串
        }
    }
    /*以上为请求起始行的解析过程。*/

    // 将URL转化为本地资源路径path
    sprintf(path, "htdocs%s", url);

    // 如果path为目录则返回首页路径
    if(path[strlen(path) - 1] == '/')
    {
        strcat(path, "index.html");
    } 
#if DEBUG_ENABLE
    printf("request path: %s\r\n", path);
#endif
     
    //检测请求文件是否存在
    if(stat(path, &st) == -1)
    {    
        //文件不存在则清除剩余header信息，即可选首部字段部分。
        while( (numofchars > 0) && strcmp("\n", buf) )
        {
            numofchars = sock_getline(client, buf, sizeof(buf));
        }
        not_found(client); // 向浏览器声明没有相应资源
    }
    else
    {
        // 若请求URL为路径，则返回首页
        // warning： 这里有一个bug，假设URL为"htdocs/index"，本地存在这个目录，
        // 但不存在"htdocs/index/index.html"这里会合成之后的路径就是错的
        if( (st.st_mode & S_IFMT) == S_IFDIR )
        {
            strcat(path, "/index.html");
        }
       // 检测到文件具备可执行权限，当请求文件为可执行程序，则应执行对应程序获取执行结果
        if( (st.st_mode & S_IXUSR ) ||    // 文件所有者具备执行权限
            (st.st_mode & S_IXGRP ) ||    // 用户组具备执行权限
            (st.st_mode & S_IXOTH ) )     // 其他用户具备可执行权限
        {
            cgi = 1;
        }
 
#if DEBUG_ENABLE
        printf("cgi[%d]: goto %s\r\n", cgi, cgi == 0 ? "serve_file":"execute_cgi");
#endif
 
        if (cgi == 0)
        {
            serve_file(client, path); // 请求文件存在且非执行，则发送文件内容
        }
        else
        {
            execute_cgi(client, path, method, query_string); // 需执行CGI获取内容的
        }
    }
    close(client);     //释放客户端套接字，通讯结束
}
```
#####执行CGI

``` c
void execute_cgi( int client, const char *path, const char *method, const char *query_string )
{
    char buf[1024]= {'A', 0,};
    int cgi_in[2]={0,0}, cgi_out[2] = {0,0}; //声明管道通讯，用于父子进程之间的通讯
    unsigned int content_length = -1, numofchars = 1;
    char ch = '\0';
    pid_t pid = -1;
    int i = 0, status;
 
    if (strcasecmp(method, "GET") == 0)
    {
        //如果是GET方法，则清除剩余http头
        while( (numofchars > 0) && strcmp("\n",buf) ) //clean the header
        {
            sock_getline(client, buf, sizeof(buf));
        }
    }
    else
    {
        while((numofchars > 0) && strcmp(buf, "\n")) // 解析终止条件：HTTP请求头部解析完
        {
            buf[14] = '\0';  //strlen("content-length") == 14
            // 解析http头请求字段，获取content-length字段值，即实体主体大小
            if( 0 == strcasecmp(buf, "content-length") ) 
            {
                content_length = atoi(&buf[16]);
            }
            numofchars = sock_getline(client, buf, sizeof(buf));
        }
        if(content_length == -1) 
        {
            //如果没有成功解析到，则表明这是一个错误请求
            bad_request(client);
            return ;
        }
    }
    // 响应报文，返回正确响应码200
    send_str(client, "HTTP/1.0 200 OK\r\n");
 
    //pipe操作必须在fork之前，这边子进程才能继承到两组文件描述符，实现父子进程之间的通讯
    if( (pipe(cgi_out) < 0) || (pipe(cgi_in) < 0) )
    {
        //创建管道，fd[0]-->读 fd[1]<--写，创建失败则返回信息给客户端
        cannot_execute(client);
        return ;
    }
 
    if( (pid = fork()) < 0 )
    {
        cannot_execute(client);
        return ;
    }
 
//为方便理解和阅读代码，加的定义
#define DEFINE_STDIN    (0)
#define DEFINE_STDOUT   (1)
#define DEFINE_STDERR   (2)
 
    if(pid == 0)
    {
        char meth_env[255], query_env[255], length_env[255];
        dup2(cgi_out[1], DEFINE_STDOUT); // dup2将系统标准输出定义到cgi_out[1]
        close(cgi_out[0]);               // 关闭cgi_out[0],避免误操作
        dup2(cgi_in[0], DEFINE_STDIN);   // 将系统标准输入定义到cgi[0]上
        close(cgi_out[1]);
 
        sprintf(meth_env, "REQUEST_METHOD=%s", method); //将请求方法保存在进程所在的环境变量中
        putenv(meth_env);
 
        if( strcasecmp(method,"GET") == 0 )
        {
            sprintf(query_env, "QUERY_STRING=%s", query_string); // GET方法需提供查询的信息
            putenv(query_env);
        }else{
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length); // POST方法提供主题的大小
            putenv(length_env);
        }
        execl(path, path, NULL); // 执行CGI程序,同时继承了子进程的文件描述符
        exit(0);
    }else{
        // 关闭两个不会操作到的pipe，避免误操作
        close(cgi_in[0]);
        close(cgi_out[1]);

        if(strcasecmp(method, "POST") == 0)
        {
            for(i = 0; i < content_length; i++)
            {
                recv(client, &ch, 1, 0); // POST方法需要解析报文主体实体，然后发给CGI程序
                write(cgi_in[1], &ch, 1);
                #if DEBUG_ENABLE
                    printf("%c", ch);
                #endif
            }
        }
        while(read(cgi_out[0], &ch, 1) > 0) // 获取CGI执行结果，并通过Socket返回客户端
        {
        #if DEBUG_ENABLE
            printf("%c", ch);
        #endif
            send(client, &ch, 1, 0);
        }
        close(cgi_out[0]);
        close(cgi_in[1]);
        waitpid(pid, &status, 0); // 等待所有子进程执行完毕
    }
}
```
