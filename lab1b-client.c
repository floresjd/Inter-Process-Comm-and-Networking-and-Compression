//NAME:Jose Flores Martinez
//EMAIL:joseflores2395@gmail.com
//ID:404474130

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <poll.h>

#include <netdb.h>
#include <netinet/in.h>

#include "zlib.h"

#define BUF_LEN 1537
#define POLL_TIMEOUT 0

struct termios old_attr;

void get_parameters(int argc, char* argv[], int* pn, int* lfd, int* copt, int* err_no);

void restore_input_mode(void);

void set_input_mode(void);

void logsent(int fd, unsigned char* data, int len);
void logrec(int fd, unsigned char* data, int len);


int main(int argc, char* argv[]){
  int port_number = -2;
  //char* log_name = NULL;
  int log_fd = -2;
  int compress_opt = -2;
  int err_msg;
  get_parameters(argc, argv, &port_number, &log_fd, &compress_opt, &err_msg);

  //fprintf(stderr, "Port number: %d\nFilename: %d\nCompress: %d\n", port_number, log_fd, compress_opt);
  set_input_mode();  
  //fprintf(stderr, "Set input mode\n");
  
  
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  unsigned char buffer[BUF_LEN];
  
  /* Create a socket point */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0){
    fprintf(stderr, "Error opening socket\n");
    exit(1);
  }

  server = gethostbyname("localhost");
  if (server == NULL){
    fprintf(stderr, "Error, there is no such host\n");
    exit(1);
  }

  memset( (char*) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy( (char*) server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(port_number);

  if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1){
    fprintf(stderr, "Error connecting to socket: %s\n", strerror(errno));
    exit(1);
  }
  
  struct pollfd fds[2];
  fds[0].fd = STDIN_FILENO;
  fds[1].fd = sockfd;
  fds[0].events = (POLLIN | POLLHUP | POLLERR);
  fds[1].events = (POLLIN | POLLHUP | POLLERR);


  unsigned char compress_buffer[BUF_LEN];
  unsigned char input_buf[BUF_LEN];
  z_stream stdin_to_sock;
  z_stream sock_to_stdout;
  //for compression
  if (compress_opt == 1){
    //for compression 
    //z_stream stdin_to_sock;
    stdin_to_sock.zalloc = Z_NULL;
    stdin_to_sock.zfree = Z_NULL;
    stdin_to_sock.opaque = Z_NULL;
    stdin_to_sock.avail_in = BUF_LEN;
    stdin_to_sock.next_in = input_buf;
    stdin_to_sock.avail_out = BUF_LEN;
    stdin_to_sock.next_out = compress_buffer;
    
    //for decompression
    //z_stream sock_to_stdout;
    sock_to_stdout.zalloc = Z_NULL;
    sock_to_stdout.zfree = Z_NULL;
    sock_to_stdout.opaque = Z_NULL;
    sock_to_stdout.avail_in = BUF_LEN;
    sock_to_stdout.next_in = input_buf;
    sock_to_stdout.avail_out = BUF_LEN;
    sock_to_stdout.next_out = compress_buffer;
  }
  
  
  
  //fprintf(stderr, "Created the fds\n");
  int ret_poll;
  unsigned char* crnl = (unsigned char*) "\r\n";
  unsigned char* nl = (unsigned char*) "\n";
  while (1){
    ret_poll = poll(fds, 2, POLL_TIMEOUT);
    if (ret_poll == -1){
      fprintf(stderr, "Error with poll(2): %s\n", strerror(errno));
      exit(1);     
    }
    if ( (fds[1].revents & POLLHUP) || (fds[1].revents & POLLERR) ){      
      exit(0);
    }
    if (ret_poll > 0){
    /* if (1){ */
      if ( (fds[1].revents & POLLHUP) || (fds[1].revents & POLLERR) ){
	fprintf(stderr, "something from polling\r\n");
	exit(0);
      } 
      int cr;
      if (fds[0].revents & POLLIN){ //from keyboard
	cr = read(STDIN_FILENO, buffer, BUF_LEN);
	if (cr == -1){
	  fprintf(stderr, "Error reading from keyboard: %s\n", strerror(errno));
	  exit(1);	  
	}
	if (compress_opt == 1){//reset attributes
	  stdin_to_sock.zalloc = Z_NULL;
	  stdin_to_sock.zfree = Z_NULL;
	  stdin_to_sock.opaque = Z_NULL;
	  stdin_to_sock.avail_in = BUF_LEN;
	  stdin_to_sock.next_in = input_buf;
	  stdin_to_sock.avail_out = BUF_LEN;
	  stdin_to_sock.next_out = compress_buffer;
	}
	int i;
	int ci = 0;
	for (i = 0; i < cr; i++){
	  switch (buffer[i]){
	  case 0x03:
	    if (write(STDOUT_FILENO, "^C", 2) == -1){
	      fprintf(stderr, "Error writing to terminal: %s", strerror(errno));
	      exit(1);
	    }
	    if ( (compress_opt == -1) && (write(sockfd, &buffer[i], 1) == -1) ){
	      fprintf(stderr, "Error writing to terminal: %s", strerror(errno));
	      exit(1);
	    }
	    if (compress_opt == 1){	   
	      input_buf[ci] = buffer[i];
	      ci++;
	    }
	    else{
	      logsent(log_fd, &buffer[i], 1);
	    }
	    break;
	    case 0x04:
	    if (write(STDOUT_FILENO, "^D", 2) == -1){
	      fprintf(stderr, "Error writing to terminal: %s", strerror(errno));
	      exit(1);
	    }
	    if ( (compress_opt == -1) && (write(sockfd, &buffer[i], 1) == -1) ){
	      fprintf(stderr, "Error writing to terminal: %s", strerror(errno));
	      exit(1);
	    }
	    if (compress_opt == 1){	   
	      input_buf[ci] = buffer[i];
	      ci++;
	    }
	    else{
	      logsent(log_fd, &buffer[i], 1);
	    }	    
	    break;
	  case '\n':
	  case '\r':
	    if (write(STDOUT_FILENO, crnl, 2) == -1){
	      fprintf(stderr, "Error writing to terminal: %s", strerror(errno));
	      exit(1);
	    }
	    if ( (compress_opt == -1) && (write(sockfd, nl, 1) == -1) ){
	      fprintf(stderr, "Error writing to terminal: %s", strerror(errno));
	      exit(1);
	    }
	    if (compress_opt == 1){	   
	      input_buf[ci] = *nl;
	      ci++;
	    }
	    else{
	      logsent(log_fd, nl, 1);
	    }	    
	    break;
	  default:	    
	    if (write(STDOUT_FILENO, &buffer[i], 1) == -1){
	      fprintf(stderr, "Error writing to terminal: %s", strerror(errno));
	      exit(1);
	    }
	    if ( (compress_opt == -1) && (write(sockfd, &buffer[i], 1) == -1) ){
	      fprintf(stderr, "Error writing to terminal: %s", strerror(errno));
	      exit(1);
	    }
	    if (compress_opt == 1){	   
	      input_buf[ci] = buffer[i];
	      ci++;
	    }
	    else{
	      logsent(log_fd, &buffer[i], 1);
	    }	    
	    break;
	  }	  
	}
	//need to compress and send over
	if (compress_opt == 1){
	  if (deflateInit(&stdin_to_sock, Z_DEFAULT_COMPRESSION) == Z_OK){
	    do {
	      deflate(&stdin_to_sock, Z_SYNC_FLUSH);
	    }while (stdin_to_sock.avail_out > 0);
	    write(sockfd, compress_buffer, BUF_LEN - stdin_to_sock.avail_out);
	    write(log_fd, compress_buffer, BUF_LEN - stdin_to_sock.avail_out);	      
	  }
	  else{//failed need to do it again
	    fprintf(stderr, "Error compressing stdin for server\r\n");
	    exit(1);
	  }
	}
      }
      if (fds[1].revents & POLLIN){ //from server via socket
	//fprintf(stderr, "Reading from socket\n");
	cr = read(sockfd, buffer, BUF_LEN);
	if (cr == -1){
	  fprintf(stderr, "Error Reading from socket: %s", strerror(errno));
	  exit(1);
	}
	if (compress_opt == 1){//reset attributes
	  sock_to_stdout.zalloc = Z_NULL;
	  sock_to_stdout.zfree = Z_NULL;
	  sock_to_stdout.opaque = Z_NULL;
	  sock_to_stdout.avail_in = BUF_LEN;
	  sock_to_stdout.next_in = input_buf;
	  sock_to_stdout.avail_out = BUF_LEN;
	  sock_to_stdout.next_out = compress_buffer;
	}
	if (cr == 0){
	  //close(sockfd);
	  exit(0);
	}
	if ( (log_fd != -2)) {
	  logrec(log_fd, buffer, cr);
	}
	if (compress_opt == 1) {
	  inflateInit(&sock_to_stdout);
	  do{
	    inflate(&sock_to_stdout, Z_SYNC_FLUSH);
	  }while(sock_to_stdout.avail_in > 0);
	  cr = BUF_LEN - sock_to_stdout.avail_out;
	}
	
	int i;	
	for (i = 0; i < cr; i++){
	  unsigned char cc;
	  if (compress_opt == 1){
	    cc = compress_buffer[i];
	  }
	  else{
	    cc = buffer[i];
	  }
	  switch (cc){
	  case '\n':
	  case '\r':
	    if ( (compress_opt == -2) &&  (write(STDOUT_FILENO, crnl, 2) == -1) ){
	      fprintf(stderr, "Error write to terminal: %s\n", strerror(errno));
	      exit(1);
	    }
	    if ( (compress_opt == 1) && (write(STDOUT_FILENO, crnl, 2) == -1) ){
	      fprintf(stderr, "Error write to terminal: %s\n", strerror(errno));
	      exit(1);
	    }
	    //fprintf(stderr, "Newline");
	    break;
	  default:
	    if ( (compress_opt == -2) && (write(STDOUT_FILENO, &cc, 1) == -1) ){
	      fprintf(stderr, "Error write to terminal: %s\n", strerror(errno));
	      exit(1);
	    }
	    if ( (compress_opt == 1) && (write(STDOUT_FILENO, &cc, 1) == -1) ){
	      fprintf(stderr, "Error write to terminal: %s\n", strerror(errno));
	      exit(1);
	    }
	    break;
	  }
	}
      }
    }      
  }
  
  if (log_fd != -2) {
    if (close(log_fd) == -1){
      fprintf(stderr, "Could not close file specified in 'log'\n");
      exit(1);
    } 
  }
  if ( (close(sockfd)  == -1) ){
    fprintf(stderr, "Error closing socket: %s\n", strerror(errno));
    exit(1);
  }
  exit(0);
}

void get_parameters(int argc, char* argv[], int* pn, int* lfd, int* copt, int* err_no){
  while(1){
    int c, option_index;
    static struct option long_opt[] = {
      {"port", required_argument, 0, 'p'},
      {"log", required_argument, 0 , 'l'},
      {"compress", no_argument, 0, 'c'},
      {0,0,0,0}
    };
    //opterr = 0;
    c = getopt_long(argc, argv, "", long_opt, &option_index);
    if (c == -1) { break; }   
    switch(c){
    case 'p':
      *pn = atoi(optarg);
      //fprintf(stderr, "Port number: %d\n", (*pn) );
      break;
    case 'l':
      //lfd = optarg;
      /* *lfd = creat(optarg, 0666); */
      *err_no = errno;
      if ( (*lfd = creat(optarg, 0666)) == -1){
      	fprintf(stderr, "Error: With %s: %s\n", optarg, strerror(errno));
      	exit(1);
      }
      break;
    case 'c':
      *copt = 1;
      break;
    case '?':
      //fprintf(stderr, "Unrecognized arguement\n:");
      exit(1);
      break;
    }
  }
  //fprintf(stderr, "-Port number: %d\n-File?: %d\n", (*pn), (*lfd) );
}


void restore_input_mode(void){
  tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);
}

void set_input_mode(void){
  struct termios new_attr;

  //check stdin is a terminal
  if ( !isatty (STDIN_FILENO) ) {
    fprintf(stderr, "STDIN is not a terminal");
    exit(1);
  }

  //save current termial modes and set them to be restored later
  tcgetattr(STDIN_FILENO, &old_attr);
  atexit(restore_input_mode);

  
  //set the terminal modes
  tcgetattr(STDIN_FILENO, &new_attr);
  new_attr.c_iflag = ISTRIP;
  new_attr.c_oflag = 0;
  new_attr.c_lflag = 0; 
  tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);
}

void logsent(int fd, unsigned char* data, int len){
  char istr[10];
  sprintf(istr, "%d", len);
  if (write(fd, "SENT ", 5) == -1){
    fprintf(stderr, "Error writing sent to log file (%d): %s\n", fd, strerror(errno));
    exit(1);
  }
  if (write(fd, istr, strlen(istr)) == -1){
    fprintf(stderr, "Error writing sent to log file (%d): %s\n", fd, strerror(errno));
    exit(1);
  }
  if (write(fd, " bytes: ", 8) == -1){
    fprintf(stderr, "Error writing sent to log file (%d): %s\n", fd, strerror(errno));
    exit(1);
  }
  if (write(fd, data, len) == -1){
    fprintf(stderr, "Error writing sent to log file (%d): %s\n", fd, strerror(errno));
    exit(1);
  }
  if (write(fd, "\n", 1) == -1){
    fprintf(stderr, "Error writing sent to log file (%d): %s\n", fd, strerror(errno));
    exit(1);
  }
}

void logrec(int fd, unsigned char* data, int len){
  //fprintf(stderr, "in ftn fd: %d\r\n", fd);
  char istr[10];
  sprintf(istr, "%d", len);
  //fprintf(stderr, "in ftn fd: %d\r\n", fd);
  if (write(fd, "RECEIVED ", 9) == -1){
    fprintf(stderr, "Error writing1 rec to log file (%d): %s\n", fd, strerror(errno));
    exit(1);
  }
  if (write(fd, istr, strlen(istr)) == -1){
    fprintf(stderr, "Error writing2 rec to log file (%d): %s\n", fd, strerror(errno));
    exit(1);
  }
  if (write(fd, " bytes: ", 8) == -1){
    fprintf(stderr, "Error writing3 rec to log file (%d): %s\n", fd, strerror(errno));
    exit(1);
  }
  if (write(fd, data, len) == -1){
    fprintf(stderr, "Error writing4 rec to log file (%d): %s\n", fd, strerror(errno));
    exit(1);
  }
  if (write(fd, "\n", 1) == -1){
    fprintf(stderr, "Error writing5 rec to log file (%d): %s\n", fd, strerror(errno));
    exit(1);
  }
}
