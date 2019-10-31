/*
 * Copyright 2005-2008 Tail-F Systems AB
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/time.h>
#include <confd_lib.h>
#include <confd_dp.h>
#include "smp.h"

FILE *trace_fd;
FILE *trace_mod_fd;
int c_trace_level = 0;
int myLoggerSys = 0;

void mylogger(int syslogprio, const char *fmt, va_list ap) {

  char *buf;
  char *bufs;
  char mbuf[BUFSIZ];
  struct timeval now;
  struct tm *logtime;
  unsigned long delta;
  int buflen;

  if (myLoggerSys) {
    char bufs[BUFSIZ];
    sprintf(bufs, "MYLOG:(%d) ", syslogprio);
    strcat(bufs, fmt);
    vfprintf(trace_mod_fd, bufs, ap);
    fflush(trace_mod_fd);
  } else {
    
    buflen = 54 + strlen(fmt);
    buf = (char *) malloc(buflen * sizeof(char));
    assert(buf != NULL);
    gettimeofday(&now,NULL);
    logtime = localtime(&(now.tv_sec));
    strftime(buf, 20, "%d-%b-%y::%T", logtime);
    snprintf(mbuf,32,".%06ld  ", (long)now.tv_usec );
    strcat(buf, mbuf);
    strcat(buf, fmt);
    
  // Find out if there is a newline at the end of the printout. 
    {
      va_list ap1;
      char *fbuf;
      int fmtdlen;
      va_copy(ap1, ap);
      fmtdlen = vsnprintf(mbuf,0,fmt,ap1);

      fbuf = (char *) malloc((fmtdlen + 1) * sizeof(char));
      assert(fbuf != NULL);
      va_copy(ap1, ap);
      vsnprintf(fbuf, fmtdlen + 1, fmt, ap1);
      va_end(ap1);
      if (fbuf[fmtdlen - 1] != '\n') {
	strcat(buf, "\n");
      }
      free(fbuf);
      }

    vfprintf(trace_mod_fd, buf, ap);   
    fflush(trace_mod_fd);
       
    free(buf);

  }
}


/* this code is an external database, the data model */
/* that we handle reside in smp.yang */

/* This _is_ our database */
struct server {

  char name[505];
  struct in_addr ip;
  unsigned int port;
};

static struct server running_db[256];
static int num_servers = 0;
static int lock = 0;

/* Our daemon context as a global variable */
/* as well as the ConfD callback function structs */

static struct confd_daemon_ctx *dctx;
static struct confd_trans_cbs trans;
static struct confd_data_cbs  data;
static int ctlsock;
static int workersock;


/* Find a specific server */
static struct server *find_server(confd_value_t *v) {
  int i;

  for (i=0; i< num_servers; i++) {
    if (confd_svcmp(running_db[i].name, v) == 0)
      return &running_db[i];
  }
  return NULL;
}

static int remove_server(confd_value_t *key) {
  int i, j;
  
    for (i=0; i< num_servers; i++) {
      if (confd_svcmp(running_db[i].name, key) == 0) {
            /* found the elem to remove, now shuffle the */
            /* remaining elems in the array one step */
	for (j=i+1; j< num_servers; j++) {
	  running_db[j-1] = running_db[j];
	}
	num_servers--;
	return CONFD_OK;
      }
    }
    return CONFD_OK;
}


/* Help functions to add a new server */

static struct server *add_server(char *name) {
  int i, j;
  
  for (i=0; i < num_servers; i++) {
    if (strcmp(running_db[i].name, name) > 0) {
            /* found the position to add at, now shuffle the */
            /* remaining elems in the array one step */
      for (j = num_servers; j > i; j--) {
	running_db[j] = running_db[j-1];
      }
      break;
    }
  }
  num_servers++;
  memset(&running_db[i], 0, sizeof(struct server));
  strcpy(running_db[i].name, name);
  return &running_db[i];
}

static struct server *new_server(char *name, char *ip, char *port) {

  struct server *sp = add_server(name);
  sp->ip.s_addr = inet_addr(ip);
  sp->port = atoi(port);
  return sp;
}

/* help function which restores the DB from a FILE* */

static int restore(char *filename) {
  char buf[BUFSIZ];
  FILE *fp;
  char *tokptr = NULL;
  
  if ((fp = fopen(filename, "r")) == NULL)
    return CONFD_ERR;
  num_servers = 0;
  while (fgets(&buf[0], BUFSIZ, fp) != NULL) {
    char *name, *ip, *port;
    if ((name = strtok_r(buf, " \r\n", &tokptr)) != NULL &&
	((ip = strtok_r(NULL, " \r\n", &tokptr)) != NULL) &&
	((port = strtok_r(NULL, " \r\n", &tokptr)) != NULL)) {
      new_server(name, ip, port);
    }
  }
  return CONFD_OK;
}


static int save(char *filename) {
    FILE *fp;
    int i;
    
    if ((fp = fopen(filename, "w")) == NULL)
      return CONFD_ERR;
    for (i=0; i<num_servers; i++) {
      fprintf(fp, "%s %s %d\n",
                running_db[i].name,
	      inet_ntoa(running_db[i].ip),
	      running_db[i].port);
    }
    fclose(fp);
    return CONFD_OK;
}

/* transaction callbacks  */

static int t_init(struct confd_trans_ctx *tctx) {
  char buf[INET6_ADDRSTRLEN];
  
  inet_ntop(tctx->uinfo->af, &tctx->uinfo->ip, buf, sizeof(buf));
  confd_trans_set_fd(tctx, workersock);
  
  return CONFD_OK;
}


static int t_write_lock(struct confd_trans_ctx *tctx) {

  lock = 1;
  return CONFD_OK;
}

static int t_write_unlock(struct confd_trans_ctx *tctx) {

  lock = 0;
  return CONFD_OK;
}

static int t_abort(struct confd_trans_ctx *tctx) {

  restore("running.DB");
  unlink("running.prep");
  
  return CONFD_OK;
}

static int t_prepare(struct confd_trans_ctx *tctx) {
  
  struct server *s;
  struct confd_tr_item *item = tctx->accumulated;
  while (item) {
    confd_hkeypath_t *keypath = item->hkp;
    confd_value_t *leaf = &(keypath->v[0][0]);
    switch(item->op) {
    case C_SET_ELEM:
      s = find_server(&(keypath->v[1][0]));
      if (s == NULL)
	break;
      switch (CONFD_GET_XMLTAG(leaf)) {
      case smp_ip:
	s->ip = CONFD_GET_IPV4(item->val);
	break;
      case smp_port:
	s->port = CONFD_GET_UINT16(item->val);
	break;
	
      }
      break;
    case C_CREATE:
      add_server((char *)CONFD_GET_BUFPTR(leaf));
      break;
    case C_REMOVE:
      remove_server(leaf);
      break;
    default:
      return CONFD_ERR;
    }
    item = item->next;
  }
  return save("running.prep");
}

static int t_commit(struct confd_trans_ctx *tctx) {
  if (rename("running.prep", "running.DB") == 0)
    return CONFD_OK;
  else
    return CONFD_ERR;
}


static int t_finish(struct confd_trans_ctx *tctx) {
    return CONFD_OK;
}

/* data callbacks that manipulate the db */

static int get_next(struct confd_trans_ctx *tctx,
                    confd_hkeypath_t *keypath,
                    long next) {
  confd_value_t v;
  
  if (next == -1) { /* Get first key */
    if (num_servers == 0) {  /* Db is empty */
      confd_data_reply_next_key(tctx, NULL, -1, -1);
      return CONFD_OK;
    }
    CONFD_SET_STR(&v, running_db[0].name);
    confd_data_reply_next_key(tctx, &v, 1, 1);
    return CONFD_OK;
  }
  if (next == num_servers) {  /* Last elem */
    confd_data_reply_next_key(tctx, NULL, -1, -1);
    return CONFD_OK;
  }
  CONFD_SET_STR(&v, running_db[next].name);
  confd_data_reply_next_key(tctx, &v, 1, next+1);
  return CONFD_OK;
}



static int get_elem(struct confd_trans_ctx *tctx,
                    confd_hkeypath_t *keypath) {
  confd_value_t v;
  struct server* s = find_server(&(keypath->v[1][0]));

  if (s ==  NULL) {
    confd_data_reply_not_found(tctx);
    return CONFD_OK;
  }
  
  /* switch on xml elem tag */
  switch (CONFD_GET_XMLTAG(&(keypath->v[0][0]))) {
  case smp_name:
    CONFD_SET_STR(&v, s->name);
        break;
  case smp_ip:
    CONFD_SET_IPV4(&v, s->ip);
    break;
  case smp_port:
    CONFD_SET_UINT16(&v, s->port);
    break;
  default:
    confd_trans_seterr(tctx, "xml tag not handled");
    return CONFD_ERR;
  }
  confd_data_reply_value(tctx, &v);
  return CONFD_OK;
}


static int set_elem(struct confd_trans_ctx *tctx,
                    confd_hkeypath_t *keypath,
                    confd_value_t *newval) {

  return CONFD_ACCUMULATE;
}

static int create(struct confd_trans_ctx *tctx,
                  confd_hkeypath_t *keypath) {

  return CONFD_ACCUMULATE;
}

static int find_next(struct confd_trans_ctx *tctx,
                     confd_hkeypath_t *kp,
                     enum confd_find_next_type type,
                     confd_value_t *keys, int nkeys) {
  confd_value_t v;
  char keybuf[20];
  int pos = -1;
  int i, siz;
    
  confd_pp_value(&keybuf, 20, keys);
  siz=strlen(keybuf);
  switch (nkeys) {
  case 0:
    /* no keys provided => the first entry will always be "after" */
    if (num_servers > 0) {
      pos = 0;
    }
    break;
  case 1:
    /* key is provided => find first entry "after" or "same",
       depending on 'type' */
    switch (type) {
    case CONFD_FIND_NEXT:
      /* entry must be "after" */
      for (i = 0; i < num_servers; i++) {
	if (strncmp(running_db[i].name, keybuf, siz) > 0 ) {
	  pos = i;
	  break;
	}
      }
      break;
    case CONFD_FIND_SAME_OR_NEXT:
      /* entry must be "same" or "after" */
      for (i = 0; i < num_servers; i++) {
	if (strncmp(running_db[i].name, keybuf,siz) > 0) {
	  pos = i;
	  break;
	}
      }
      break;
    default:
      confd_trans_seterr(tctx, "invalid number of keys: %d", nkeys);
      return CONFD_ERR;
    }
  }
  if (pos >= 0) {
    /* matching entry found - return its keys and 'pos' for next entry */
    CONFD_SET_UINT32(&v, running_db[pos].name);
    confd_data_reply_next_key(tctx, &v, 1, (long)(pos + 1));
  } else {
    /* no matching entry - i.e. end-of-list */
    confd_data_reply_next_key(tctx, NULL, -1, -1);
  }
  return CONFD_OK;

}

static int doremove(struct confd_trans_ctx *tctx,
                  confd_hkeypath_t *keypath) {

  return CONFD_ACCUMULATE;
}


/* Initialize db to 3 servers */

static void init_db() {
  
  if ((restore("running.DB")) == CONFD_OK)
    return;
  num_servers = 0;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in addr;
    int debuglevel = CONFD_DEBUG;
    int oc;             /* option character */
    trace_fd = fopen("daemon.trace", "a+");
    trace_mod_fd = fopen("daemon.my.trace", "a+");

    while ((oc = getopt(argc, argv, "qdtp")) != -1) {
        switch (oc) {
        case 'q':
            debuglevel = CONFD_SILENT;
            break;
        case 'd':
            debuglevel = CONFD_DEBUG;
            break;
        case 't':
            debuglevel = CONFD_TRACE;
            break;
        case 'p':
            debuglevel = CONFD_PROTO_TRACE;
            break;
        default:
            fprintf(stderr, "usage: simple [-qdtp]\n");
            exit(1);
        }
    }

    /* Initialize our simple database  */
    init_db();

    /* Transaction callbacks */
    trans.init = t_init;
    trans.trans_lock = t_write_lock;
    trans.trans_unlock = t_write_unlock;
    trans.write_start = NULL;
    trans.prepare = t_prepare;
    trans.abort = t_abort;
    trans.commit = t_commit;
    trans.finish = t_finish;
    
    data.get_elem = get_elem;
    data.get_next = get_next;
    data.set_elem = set_elem;
    data.create   = create;
    data.find_next  = find_next;
    data.remove   = doremove;
    strcpy(data.callpoint, "simplecp");

    //openlog ("simple_withtrans", LOG_ODELAY, LOG_LOCAL2);

    //confd_lib_use_syslog = 1;
    //myLoggerSys = 1;

    confd_init("simple_withtrans", trace_fd, debuglevel);

    confd_user_log_hook = mylogger;

    if ((dctx = confd_init_daemon("simple_withtrans"))
        == NULL)
      confd_fatal("Failed to initialize confd\n");
    
    
    if ((ctlsock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
      confd_fatal("Failed to open ctlsocket\n");
    
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4565);


    if (confd_load_schemas((struct sockaddr*)&addr,
                           sizeof (struct sockaddr_in)) != CONFD_OK)
      confd_fatal("Failed to load schemas from confd\n");
    
    /* Create the first control socket, all requests to */
    /* create new transactions arrive here */

    if (confd_connect(dctx, ctlsock, CONTROL_SOCKET, (struct sockaddr*)&addr,
                      sizeof (struct sockaddr_in)) != CONFD_OK)
      confd_fatal("Failed to confd_connect() to confd \n");
    

    /* Also establish a workersocket, this is the most simple */
    /* case where we have just one ctlsock and one workersock */

    if ((workersock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
      confd_fatal("Failed to open workersocket\n");
    if (confd_connect(dctx, workersock, WORKER_SOCKET,(struct sockaddr*)&addr,
                      sizeof (struct sockaddr_in)) < 0)
      confd_fatal("Failed to confd_connect() to confd \n");

    confd_register_trans_cb(dctx, &trans);

    /* we also need to register our read/write callbacks */

    if (confd_register_data_cb(dctx, &data) == CONFD_ERR)
        confd_fatal("Failed to register data cb \n");

    if (confd_register_done(dctx) != CONFD_OK)
        confd_fatal("Failed to complete registration \n");


    
    while (1) {
        struct pollfd set[2];
        int ret;

        set[0].fd = ctlsock;
        set[0].events = POLLIN;
        set[0].revents = 0;

        set[1].fd = workersock;
        set[1].events = POLLIN;
        set[1].revents = 0;


        if (poll(set, sizeof(set)/sizeof(*set), -1) < 0) {
            perror("Poll failed:");
            continue;
        }

        /* Check for I/O */
        if (set[0].revents & POLLIN) {
            if ((ret = confd_fd_ready(dctx, ctlsock)) == CONFD_EOF) {
                confd_fatal("Control socket closed\n");
            } else if (ret == CONFD_ERR && confd_errno != CONFD_ERR_EXTERNAL) {
                confd_fatal("Error on control socket request: %s (%d): %s\n",
                     confd_strerror(confd_errno), confd_errno, confd_lasterr());
            }
        }
        if (set[1].revents & POLLIN) {
            if ((ret = confd_fd_ready(dctx, workersock)) == CONFD_EOF) {
                confd_fatal("Worker socket closed\n");
            } else if (ret == CONFD_ERR && confd_errno != CONFD_ERR_EXTERNAL) {
                confd_fatal("Error on worker socket request: %s (%d): %s\n",
                     confd_strerror(confd_errno), confd_errno, confd_lasterr());
            }
        }

    }
}

