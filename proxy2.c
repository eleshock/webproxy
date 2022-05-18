#include <stdio.h>
#include "csapp.h"

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n"; //User-Agent 문자열 상수로 제공
static const char *requestline_hdr_format = "GET %s HTTP/1.0\r\n"; // request 헤더 포맷
static const char *endof_hdr = "\r\n";  // HTTP 요청 "\r\n"으로 끝남
static const char *host_hdr_format = "Host: %s\r\n"; // Host header format
static const char *conn_hdr = "Connection: close\r\n"; // connection header : close
static const char *prox_hdr = "Proxy-Connection: close\r\n"; // porxy-connection header : close

static const char *host_key = "Host";
static const char *connection_key = "Connection";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *user_agent_key = "User-Agent";

void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio);
int connect_endServer(char *hostname, int port, char *http_header);
void *thread(void *vargp);


int main(int argc, char **argv) // proxy서버에서 사용할 port 번호를 인자로 받는다
{
    int listenfd, *connfd;
    socklen_t clientlen;
    char hostname[MAXLINE], port[MAXLINE];

    struct sockaddr_storage clientaddr; // 클라이언트에서 연결 요청을 보내면 알 수 있는 클라이언트 연결 소켓 주소

    pthread_t tid; 

    if (argc != 2)
    {
        // fprintf: 출력을 파일에다 씀. strerr: 파일 포인터
        fprintf(stderr, "usage: %s <port> \n", argv[0]);
        exit(1); // exit(1): 에러 시 강제 종료
    }
    // 해당 포트 번호에 해당하는 듣기 소켓 식별자를 열어준다
    listenfd = Open_listenfd(argv[1]);
    // 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit() 호출
    while (1)
    {   // 클라이언트에게서 받은 연결 요청을 accept한다. connfd = 프록시 서버와의 연결 식별자
        clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        // 연결이 성공했다는 메세지를 위해, Getnameinfo를 호출하면서 (client)hostname과 port가 채워진다
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s %s).\n", hostname, port);

        // 쓰레드 ID, 쓰레드 성질(기본: NULL), 쓰레드 함수, 쓰레드 루틴이 인자로 들어감
        Pthread_create(&tid, NULL, thread, connfd);

    }
    return 0;
}
// 클라이언트의 요청 라인을 파싱
// 1) end server의 hostname, path, port를 가져옴
// 2) end server에 보낼 요청 라인과 헤더를 만들 변수들을 만듦
// 3) 프록시 서버와 엔드 서버를 연결하고 엔드 서버의 응답 메세지를 클라이언트에 보내줌

void doit(int connfd)
{
    int end_serverfd;

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char endserver_http_header[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    int port;

    // rio: client's rio / server_rio: endserver's rio
    rio_t rio, server_rio;

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version); // read the client reqeust line

    if (strcasecmp(method, "GET")) // method 스트링이 GET이 아니면 0이 아닌 값이 나옴
    {
        printf("Proxy does not implement the method");
        return;
    }

    // parse the uri to get hostname, file path, port
    // 프록시 서버가 엔드 서버로 보낼 정보들을 파싱함
    // hostname -> localhost, path -> /home.html, port -> 8000
    parse_uri(uri, hostname, path, &port);

    // build the http header which will send to the end server
    // 프록시 서버가 엔드 서버로 보낼 요청 헤더들을 만듦.
    // endserver_http_header가 채워진다
    build_http_header(endserver_http_header, hostname, path, port, &rio);

    // connect to the end server
    // 프록시 서버와 엔드 서버를 연결함
    end_serverfd = connect_endServer(hostname, port, endserver_http_header);
    // clientfd connected from proxy to end server at proxy side
    // port : 8000
    if (end_serverfd < 0)
    {
        printf("connection failed\n");
        return;
    }
    
    Rio_readinitb(&server_rio, end_serverfd);
    // write the http header to endserver
    // 엔드 서버에 HTTP 요청 헤더를 보냄
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

    // recieve message from end server and send to the client
    // 엔드 서버로부터 응답 메세지를 받아 클라이언트에 보내준다.
    size_t n;
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0)
    {
        printf("proxy received %ld bytes, then send\n", n);
        Rio_writen(connfd, buf, n);
    }
    // 이것은 서버 단이기 때문에 프록시에서 보내주는 것만 있고, 클라이언트 단에서 읽는 과정은 생략되있는 것 같음
    Close(end_serverfd);
}

/* Thread routine */
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

// 클라이언트로부터 받은 요청 헤더를 정제해서 프록시 서버가 엔드 서버에 보낼 요청 헤더를 만듦
// 각 변수들의 값 -> 클라이언트 request가 "GET http://localhost:8000/home.html HTTP:/1.1"
// request_hdr = "GET /home.html HTTP/1.0\r\n"
// host_hdr = "Host : localhost:8000"
// conn_hdr = "Connection: close\r\n"
// prox_hdr = "Porxy-Connection: close\r\n"
// user_agent_hdr = "User-Agent: ...."
// otehr_hdr = Connection, Proxy-Connection, User-Agent가 아닌 모든 헤더
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio)
{
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

    // request line
    // 응답라인 만들기
    sprintf(request_hdr, requestline_hdr_format, path);

    // get other request header for client rio and change it
    // 클라이언트 요청 헤더들에서 Host header와 나머지 header들을 구분해서 넣어줌
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0)
    {
        if (strcmp(buf, endof_hdr) == 0) // "\r\n"고 같으면 0출력
            break; // EOF

        if (!strncasecmp(buf, host_key, strlen(host_key))) // 대소문자 구분하지 않고 비교 + strlen길이까지만 비교
        {                                                  // 일치하면 0인데 여기서는 !를 붙여서 일치하면 if문으로 들어감
            strcpy(host_hdr, buf); //host_hdr에 buf에 있는 문자를 복사
            continue;
        }

        if (!strncasecmp(buf, connection_key, strlen(connection_key)) && !strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key)) && !strncasecmp(buf, user_agent_key, strlen(user_agent_key)))
        {
            strcat(other_hdr, buf); // otehr_hdr 문자열 뒤쪽에 buf를 이어 붙인다
        }
    }
    if (strlen(host_hdr) == 0)
    {
        sprintf(host_hdr, host_hdr_format, hostname);
    }
    //프록시 서버가 엔드 서버로 보낼 요청 헤더 작성
    sprintf(http_header, "%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            conn_hdr,
            prox_hdr,
            user_agent_hdr,
            other_hdr,
            endof_hdr);
    return;
}

// Connect to the end server
// 프록시 서버와 엔드 서버를 연결한다
int connect_endServer(char *hostname, int port, char *http_header)
{
    char portStr[100];
    sprintf(portStr, "%d", port);
    return Open_clientfd(hostname, portStr);
    // sprintf(portStr, "%d", 8000);
    // return Open_clientfd("13.124.242.141", portStr);
}

// parse the uri to get hostname, file path, port
// 클라이언트의 uri를 파싱해 서버의 hostname, path, port를 찾는다
// 변수들 -> 클라이언트 request가 "GET http://localhost:8000/home.html HTTP/1.1"
// hostname = localhost
// path = /home.html
// port = 8000
void parse_uri(char *uri, char *hostname, char *path, int *port)
{
    // if (uri[0] == '/') {
    //   sprintf(uri, "/home.html");
    // }
    // printf("%s\n", uri);
    *port = 80; // 기본 http 포트인 80으로 초기화
    char *pos = strstr(uri, "//"); // http:// 이후의 string
    pos = pos != NULL ? pos + 2 : uri; // http:// 없어도 가능
    char *pos2 = strstr(pos, ":");  // port와 path를 파싱

    if (pos2 != NULL) // localhost:8000/home.html
    {
        *pos2 = '\0';   
        sscanf(pos, "%s", hostname);
        // port change from 80 to client-specifying port
        sscanf(pos2 + 1, "%d%s", port, path); 
    }
    else
    {
        pos2 = strstr(pos, "/");
        if (pos2 != NULL) // localhost/home.html
        {
            *pos2 = '\0'; // 중간에 문자열 끊어주기
            sscanf(pos, "%s", hostname);
            *pos2 = '/';
            sscanf(pos2, "%s", path);
        }
        else // localhost
        {
            sscanf(pos, "%s", hostname);
        }
    }
    return;
}