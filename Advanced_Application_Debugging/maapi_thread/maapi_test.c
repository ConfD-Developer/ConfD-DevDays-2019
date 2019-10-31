#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <confd_lib.h>
#include <confd_maapi.h>
#include <confd_cdb.h>
#include<pthread.h>

pthread_t thid[2];
struct confd_ip ip;

struct thread {
  int sock[2];
  int tid[2];
};

/* include generated ns file */
#include "example.h"

#define OK(E) do {                                                      \
    int _ret = (E);                                                 \
    if (_ret != CONFD_OK) {                                         \
      struct timeval _tv;                                         \
      struct tm *_tm;                                             \
      gettimeofday(&_tv, NULL);                                   \
      _tm = localtime((time_t *)&_tv.tv_sec);                     \
      fprintf(stderr, "%02d:%02d:%02d.%06ld(%d): "                \
	      "%s-->%d, confd_errno=%d, confd_lasterr='%s'\n",    \
	      _tm->tm_hour, _tm->tm_min, _tm->tm_sec, (long)_tv.tv_usec, \
	      getpid(), #E, _ret, confd_errno, confd_lasterr());  \
      fflush(stderr);                                             \
      assert(0);                                                  \
    }                                                               \
  } while (0)


void pval(confd_value_t *v)
{
  char buf[BUFSIZ];
  memset(buf, 0, sizeof(buf));
  confd_pp_value(buf, BUFSIZ, v);
  fprintf(stderr, "%s\n", buf);
}

void *run(void *arg){
  struct thread *thr = arg;
  const char *groups[]={"admin", "oper"};
  int num0, num1;
  pthread_t id = pthread_self();  

  /* First thread */
  if(pthread_equal(id, thid[0]))
    {
      printf("\n First thread processing\n");
      if (maapi_start_user_session(thr->sock[0], "oper", "system", groups,
				   1, &ip, CONFD_PROTO_TCP) != CONFD_OK)
	confd_fatal("Failed new usess %d\n", confd_errno);
      if ((thr->tid[0] = maapi_start_trans(thr->sock[0], 
					   CONFD_RUNNING, 
					   CONFD_READ_WRITE)) < 0 ){
	fprintf(stderr, "lasterr:%s\n,tid", confd_lasterr());
	confd_fatal("Failed to start trans.\n");
      }
      
      if ((num0 = maapi_num_instances(thr->sock[0], 
				     thr->tid[0], "/obj")) < 0){
	fprintf(stderr, "confd_errno: %d:\n", confd_errno);
      }
    }
  /* Second thread */ 
  else if (pthread_equal(id,thid[1])){
    printf("\n Seconfd thread processing\n");
    
    if (maapi_start_user_session(thr->sock[1], "admin", "system", groups,
				 1, &ip, CONFD_PROTO_TCP) != CONFD_OK)
      confd_fatal("Failed new usess %d\n", confd_errno);
    
    if ((thr->tid[1] = maapi_start_trans(thr->sock[1], 
					 CONFD_RUNNING, 
					 CONFD_READ_WRITE)) < 0 ){
      fprintf(stderr, "lasterr:%s\n,tid", confd_lasterr());
      confd_fatal("Failed to start trans.\n");
    }
    if ((num1 = maapi_num_instances(thr->sock[0], 
				    thr->tid[0], "/obj")) < 0){
	fprintf(stderr, "confd_errno: %d:\n", confd_errno);
      }
  }

  return NULL;

}

int main(int argc, char **argv)
{

  int c, i=0;
  int debuglevel = CONFD_TRACE;
  int err;
  FILE *fd;
  struct sockaddr_in addr;
  
  struct thread thr;

  while ((c = getopt(argc, argv, "tdps")) != -1) {
    switch(c) {
    case 't':
      debuglevel = CONFD_TRACE;
      break;
    case 'd':
      debuglevel = CONFD_DEBUG;
      break;
    case 'p':
      debuglevel = CONFD_PROTO_TRACE;
      break;
    case 's':
      debuglevel = CONFD_SILENT;
      break;
    }
  }
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(4565);
  fd = fopen("prg_trace.txt", "a+");
  
  confd_init("maapi_test", stderr, debuglevel);
  OK(confd_load_schemas((struct sockaddr*)&addr,
			sizeof (struct sockaddr_in)));

  if ((thr.sock[0] = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
    confd_fatal("Failed to open socket\n");
  if ((thr.sock[1] = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
    confd_fatal("Failed to open socket\n");

  ip.af = AF_INET;
  inet_pton(AF_INET, "127.0.0.1", &ip.ip.v4);
  
  if (maapi_connect(thr.sock[0], (struct sockaddr*)&addr,
		    sizeof (struct sockaddr_in)) < 0)
    confd_fatal("Failed to maapi_connect() to confd \n");

  if (maapi_connect(thr.sock[1], (struct sockaddr*)&addr,
		    sizeof (struct sockaddr_in)) < 0)
    confd_fatal("Failed to maapi_connect() to confd \n");

  while(i < 2)
    {
      if ( (err = pthread_create(&(thid[i]), NULL, run, &thr)) != 0)
        printf("\ncan't create thread :[%s]", strerror(err));
      else
        printf("\n Thread created successfully\n");
      i++;
    }
  

  if(pthread_join(thid[0], NULL)) {
    fprintf(stderr, "Error joining thread [0]\n");
    return 2;
  }
  else if(pthread_join(thid[1], NULL)) {
    fprintf(stderr, "Error joining thread [1]\n");
    return 2;
  } else
    fprintf(stderr, "Both threads joined]\n");
  
  return 0;
}
