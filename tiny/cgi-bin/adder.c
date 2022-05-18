/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char *method; // 연습문제 11.11
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');
    *p = '\0';

    /* 연습문제 11.10: 인자를 직접 입력받아 동적컨텐츠 처리하기 */
    sscanf(buf, "first=%d", &n1);
    sscanf(p+1, "second=%d", &n2);

    // strcpy(arg1, buf);
    // strcpy(arg2, p+1);
    // n1 = atoi(arg1);
    // n2 = atoi(arg2);
  }

  method = getenv("REQUEST_METHOD");

  /* Make the response body */
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n"); // 클라이언트 에서 \r\n으로 빈줄을 만들어 주면 그 빈줄을 기점으로 윗 부분이 헤더, 아랫 부분이 바디가 된다.
  // 기본 printf는 FILE NUMBER로 1, 즉 스탠다드 아웃이지만, 현재 adder는 fork로 connfd가 넘겨진 상태이므로 fd가 3인 상태이므로 클라이언트의 html에 printf된다.
  // 그래서 위의 printf 두 라인은 클라이언트로 접속했을때는 출력 되지 않는다. 하지만 telnet으로 요청을 보내게 되면 헤더 정보까지 포함하여 클라이언트에게 보내준다.
  if (strcasecmp(method, "HEAD") != 0) // 연습문제 11.11
    printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */