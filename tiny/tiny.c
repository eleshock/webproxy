/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */

#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);

// void serve_static(int fd, char *filename, int filesize);
/* 연습문제 11.11 HEAD method 추가 */
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
// void serve_dynamic(int fd, char *filename, char *cgiargs);
/* 연습문제 11.11 HEAD method 추가 */
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void echo(int connfd);
void sigchld_handler(int sig);

/* ./tiny(프로그램명) 8000(port번호)와 같이 인자 2개를 받아 실행 */
int main(int argc, char **argv){ // argc : 인자의 개수, argv : 인자의 배열
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2){ // 입력된 인자의 갯수가 2개가 아니라면 표준 오류 출력
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* Open_listenfd 함수를 호출해서 듣기 소켓을 오픈한다. 인자로 포트 번호를 넘겨준다. */
  listenfd = Open_listenfd(argv[1]);

  /* 반복적으로 클라이언트의 연결 요청을 접수하고 새로 connfd(서버 연결 식별자)를 만들어 doit() 호출 */
  while (1){
    clientlen = sizeof(clientaddr);
    /* Accept 함수는 1. 듣기 식별자, 2. 소켓 주소 구조체의 주소, 3. 주소(소켓 구조체)의 길이를 인자로 받음 */
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    
    /* Getnameinfo는 소켓 주소 구조체를 스트링 표시로 변환 */
    /* 연결이 성공했다는 메시지를 위해, Getnameinfo 함수를 호출하면서 hostname과 port가 채워짐 */
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    doit(connfd);
    Close(connfd);
  }
}

/* doit 함수는 main 함수에서 호출되며, connfd를 인자로 받음 */
void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  // 클라이언트가 보낸 요청 라인과 헤더를 읽고 분석한다.
  Rio_readinitb(&rio, fd); // rio_t 타입의 읽기 버퍼와 식별자 connfd를 연결한다. 
  Rio_readlineb(&rio, buf, MAXLINE); // 버퍼(connfd)에 있는 내용을 buf로 옮긴다.

  printf("Request headrs: \n");
  printf("%s", buf); // buf에 있는 내용을 출력한다. (최초 요청 라인 : GET / HTTP/1.1)
  sscanf(buf, "%s %s %s", method, uri, version); // buf에 있는 내용을 method, uri, version이라는 문자열에 저장한다.
  
  // strcasecmp : 문자열 비교 함수, 첫 번째 인자와 두 번째 인자가 같은 문자열이면 0을 반환한다.
  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) { // method가 "GET"이나 "HEAD"가 아니라면 clienterror 함수를 실행한다.
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  // read_requesthdrs : 요청 라인을 제외한 요청 헤더들을 읽어서 출력한다.
  read_requesthdrs(&rio); 

  /* Parse URI from GET request */
  // parse_uri : uri를 통해 filename과 cgiargs를 채워 넣는다. 또한, 리턴 값으로 정적 콘텐츠일 경우 1, 동적 콘텐츠일 경우 0을 반환한다.
  is_static = parse_uri(uri, filename, cgiargs);

  // stat : 첫 번째 인자의 파일 정보를 확인하여 두 번째 인자에 해당 정보를 채워준다. 성공 시 0, 실패 시 -1을 반환한다. 
  // 파일 정보를 못찾는 경우, clienterror를 실행한다.
  if (stat(filename, &sbuf) < 0){
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  // 앞에서 parse_uri의 리턴 값이 정적 콘텐츠일 경우
  if (is_static){
    // filename에 해당하는 파일이 일반 파일이 아니거나 읽기 권한이 없는 경우라면 clienterror 함수를 실행한다.
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    
    // 일반 파일이거나 읽기 권한이 있는 경우라면 serve_static 함수를 실행하여 클라이언트에게 정적 콘텐츠를 전달한다.
    serve_static(fd, filename, sbuf.st_size, method);
  }

  // parse_uri의 리턴 값이 동적 콘텐츠일 경우
  else {
    // filename에 해당하는 파일이 일반 파일이 아니거나 읽기 권한이 없는 경우라면 clienterror 함수를 실행한다.
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 일반 파일이거나 읽기 권한이 있는 경우라면 serve_dynamic 함수를 실행하여 클라이언트에게 동적 콘텐츠를 전달한다.
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

// clienterror : HTTP 응답을 응답 라인에 적절한 상태 코드와 메시지를 클라이언트에 보낸다. 
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  
  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);

  while (strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;

  // strstr : 첫 번째 인자에 두 번째 인자가 포함되는지 확인한다.
  // 포함되어 있다면 해당 문자열의 위치 포인터를, 포함되어 있지 않다면 NULL을 반환
  
  // 포함되어 있지 않은 경우,
  if (!strstr(uri, "cgi-bin")){
    // strcpy : 첫 번째 인자에 두 번째 인자를 복사한다.
    strcpy(cgiargs, ""); 
    strcpy(filename, ".");

    // strcat : 첫 번째 인자에 두 번째 인자를 연결한다.
    strcat(filename, uri);

    // 만약, url의 마지막이 '/'라면 filename에 "home.html"을 연결한다.
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  
  // 포함되어 있는 경우,
  else {
    // index : 첫 번째 인자에서 두 번째 인자를 찾는다. 찾으면 문자의 위치 포인터를, 못찾으면 NULL을 반환한다.
    ptr = index(uri, '?');

    // '?'이 있으면, cgiargs를 '?' 뒤의 문자열로 채운다.
    if (ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    // '?'이 없으면, cgiargs를 빈문자열로 채운다.
    else 
      strcpy(cgiargs, "");

    strcpy(filename, ".");
    strcat(filename, uri);
    
    /* 예시
      uri : /cgi-bin/adder?123&123
      -> cgiargs : 123&123
      -> filename : ./cgi-bin/adder
    */
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char *method){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF], *fbuf;

  /* Send response headers to client */
  // get_filetype : filename의 접미어를 검사해서 filetpye 결정
  get_filetype(filename, filetype);

  // 응답 라인과 헤더를 작성하는 과정
  // sprintf : 첫 번째 인자에 후속 인자들을 넣는다.
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  
  /* buf의 내용을 fd(connfd)로 복사한다. */
  Rio_writen(fd, buf, strlen(buf));
  
  /* 응답 라인과 헤더의 내용을 출력한다. */
  printf("Response headers: \n");
  printf("%s", buf);
  
  if (strcasecmp(method, "HEAD") == 0)
    return; // 연습문제 11.11

  /* open : 첫 번째 인자의 파일을 이후 인자들의 속성으로 연다.
   * filename에 해당하는 파일을 읽기 권한으로 연다.
   */
  srcfd = Open(filename, O_RDONLY, 0);

  /* 연습문제 11.9: Malloc으로 요청파일 복사 */
  srcp = (char*)Malloc(filesize); // 파일 크기만큼의 메모리 동적 할당
  Rio_readn(srcfd, srcp, filesize); // 파일의 내용을 동적할당된 메모리에 씀

  /* mmap : fd가 가리키는 객체를 가상 메모리 영역에 매핑한다. 
   * mmap 사용 후에는 munmap을 통해 매핑을 해제해주어야 한다.
   */
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);

  // srcp 내용을 fd(conned)에 복사한다.
  Rio_writen(fd, srcp, filesize);
  // Munmap(srcp, filesize);
  free(srcp); // Malloc 사용시 free 필요
}

// get_filetype : filename에서 MIME 타입에 해당하는 접미사를 찾아서 filetype에 입력해준다.
void get_filetype(char *filename, char *filetype){
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  // 연습문제 11.7
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method){
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  
  // fork : 새로운 프로세스를 생성한다.
  // fork 함수를 호출한 프로세스는 부모 프로세스가 되고 새롭게 생성된 프로세스는 자식 프로세스가 된다.
  // 이때 부모 프로세스는 자식의 PID(Process ID)를, 자식 프로세스는 0을 반환받는다.
  // 이때, fork의 값을 기준으로 분기 처리를 하면 각 프로세스의 메모리 공간이 달라진다.
  if (Fork() == 0){
    /* Child */
    /* Real server would set all CGI vars here */
    
    // setenv : 첫 번째 인자(환경변수)를 두 번째 인자(값)으로 변경한다. 
    // 세 번째 인자는 기존 환경 변수의 유무에 상관없이 값을 변경할 것이라면 1, 아니라면 0을 지정한다.
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", method, 1); // 연습문제 11.11
    // clientfd 출력을 CGI 프로그램의 표준 출력과 연결한다
    // 표준 입출력을 fd(connfd)로 재지정한다.
    Dup2(fd, STDOUT_FILENO);              /* Redirect stdout to client */

    // execve : 프로그램 실행 함수로, 첫 번재 인자는 프로그램의 경로, 세 번째 인자는 환경 변수 목록을 의미한다.
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  
  /* Parent waits for and reaps child */
  Wait(NULL);
}