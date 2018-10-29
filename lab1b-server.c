//NAME:Jose Flores Martinez
//EMAIL:joseflores2395@gmail.com
//ID:404474130


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>

#include"zlib.h"

#define BUF_LEN 1024
#define POLL_TIMEOUT 0
void get_parameters(int argc, char* argv[], int* pn, int* copt);

int main( int argc, char* argv[] ) {

  int port_number = -1;
  int compress_opt = -1;
  get_parameters(argc, argv, &port_number, &compress_opt);
  //fprintf(stderr, "Port number: %d\nCompress Option: %d\n", port_number, compress_opt);
  
  int sockfd, newsockfd;
  socklen_t clilen;
  unsigned char buffer[BUF_LEN];
  struct sockaddr_in serv_addr, cli_addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0){
    fprintf(stderr, "Error opening socket\n");
    exit(1);
  }

  memset( (char*) &serv_addr, 0, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port_number);

  if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){
    fprintf(stderr, "Error binding: %s\n", strerror(errno));
    exit(1);
  }

  listen(sockfd, 6);
  clilen = sizeof(cli_addr);
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  if (newsockfd < 0){
    fprintf(stderr, "Error on accpeting\n");
    exit(1);
  }
  
  /* Now connection has been established between server and client */
  
  
  int shell_in[2];
  int shell_out[2];
  pipe(shell_in);
  pipe(shell_out);

  struct pollfd fds[2];

  int cpid = fork();
  if (cpid < 0){
    fprintf(stderr, "Error: Fork Failed\n");
    exit(1);
  }
  if (cpid == 0){ //this is the shell
    if ( (close(shell_in[1]) == -1) || (close(shell_out[0]) == -1) ){
      fprintf(stderr, "Error closing shell pipe: %s", strerror(errno));
      exit(1);
    }
    if (dup2(shell_in[0], STDIN_FILENO) == -1){
      fprintf(stderr, "Error duping stdin: %s", strerror(errno));
      exit(1);
    }
    if (close(shell_in[0]) == -1){
      fprintf(stderr, "Error closing shell pipe: %s", strerror(errno));
      exit(1);
    }
    if (dup2(shell_out[1], STDOUT_FILENO) == -1){
      fprintf(stderr, "Error duping stdout: %s", strerror(errno));
      exit(1);
    }
    if (dup2(shell_out[1], STDERR_FILENO) == -1){
      fprintf(stderr, "Error duping stderr: %s", strerror(errno));
      exit(1);
    }
    if (close(shell_out[1]) == -1){
      fprintf(stderr, "Error closing shell pipe: %s", strerror(errno));
      exit(1);
    }
    close(sockfd);
    close(newsockfd);
    /* This will execute bash */
    //fprintf(stderr, "opening bash\n");
    int ce;
    ce = execl("/bin/bash", "bash", (char*) NULL);
    if (ce == -1){
      fprintf(stderr, "Error opening the bash: %s", strerror(errno));
      exit(1);
    }
    fprintf(stderr, "should not print\n");
  }
  else { //this is the server
    if ( (close(shell_in[0]) == -1) || (close(shell_out[1]) == -1) ){
      fprintf(stderr, "Error closing pipe from server: %s", strerror(errno));
      exit(1);
    }
    fds[0].fd = newsockfd;
    fds[1].fd = shell_out[0];
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
    
    memset(buffer, 0, BUF_LEN);
    int ret_poll;
    int shell_pipe_closed = 0;
    int client_closed = 0;
    while (1){
      int rstatus = -12;
      pid_t rcw = waitpid(0, &rstatus, WNOHANG);
      if (rcw > 0){
	fprintf(stderr, "SHELL EXIT SIGNAL=%dSTATUS=%d\n", (rstatus & 0x007f) , ((rstatus & 0xff00) >> 8));
	close(sockfd);
	close(newsockfd);
	exit(0);
      }
      
      ret_poll = poll(fds, 2, POLL_TIMEOUT);
      if (ret_poll == -1){
	fprintf(stderr, "Error with poll(2): %s\n", strerror(errno));
	exit(1);
      }      
      int cr;      
      unsigned char* nl = (unsigned char*) "\n";
      int status = 0;
      if (ret_poll > 0){
	if (client_closed == 1){
	  if (close(shell_in[1]) == -1) {
	    fprintf(stderr, "Error closing shell_in pipe: %s\n", strerror(errno));
	    exit(1);
	  }
	  int cww = 0;
	  if ((cr = read(shell_out[0], buffer, BUF_LEN)) == -1){
	    int i;	    
	    for (i = 0; i < cr; i++){
	      if (buffer[i] == (unsigned char) EOF){
		cww = wait(&status);
		if (cww == -1) {
		  fprintf(stderr, "Error waiting for child: %s\n", strerror(errno));
		  exit(1);
		}
		fprintf(stderr, "SHELL EXIT SIGNAL=%dSTATUS=%d\n", (status & 0x007f) , ((status & 0xff00) >> 8));
	      }
	    }	    
	  }
	  else if (cr == 0){
	    cww = wait(&status);
	    if (cww == -1) {
	      fprintf(stderr, "Error waiting for child: %s\n", strerror(errno));
	      exit(1);
	    }
	    fprintf(stderr, "SHELL EXIT SIGNAL=%dSTATUS=%d\n", (status & 0x007f) , ((status & 0xff00) >> 8));
	  }
	  close(newsockfd);
	  close(sockfd);
	  exit(0);
	}
      }
      if (shell_pipe_closed == 1){
	cr = read(shell_out[0], buffer, BUF_LEN);
	if (cr == -1) {
	  fprintf(stderr, "Error reading from shell pipe: %s\n", strerror(errno));
	  exit(1);
	}
	int i;
	int cww;
	int ci = 0;
	for (i = 0; i < cr; i++){
	  switch (buffer[i]){
	  case (unsigned char)EOF:
	    cww = wait(&status);
	    if (cww == -1) {
	      fprintf(stderr, "Error waiting for child: %s\n", strerror(errno));
	      exit(1);
	    }
	    fprintf(stderr, "SHELL EXIT SIGNAL=%dSTATUS=%d\n", (status & 0x007f) , ((status & 0xff00) >> 8));
	    close(newsockfd);
	    close(sockfd);
	    exit(0);
	  case '\r':
	  case '\n':
	    if ( (compress_opt == -1) && (write(newsockfd, nl, 1) == -1) ){
	      fprintf(stderr, "Error writing to socket: %s\n", strerror(errno));
	      exit(1);
	    }
	    if (compress_opt == 1){
	      input_buf[ci] = *nl;
	      ci++;
	    }
	    break;
	  default:
	    if ( (compress_opt == -1)  && (write(newsockfd, &buffer[i], 1) == -1) ){
	      fprintf(stderr, "Error writing to socket: %s\n", strerror(errno));
	      exit(1);
	    }
	    if (compress_opt == 1){
	      input_buf[ci] = buffer[i];
	      ci++;
	    }
	    break;
	  }
	}
	if (compress_opt == 1){
	  if (deflateInit(&stdin_to_sock, Z_DEFAULT_COMPRESSION) == Z_OK){
	    do {
	      deflate(&stdin_to_sock, Z_SYNC_FLUSH);
	    }while (stdin_to_sock.avail_out > 0);
	    write(newsockfd, compress_buffer, BUF_LEN - stdin_to_sock.avail_out);	    
	  }
	  else{//failed need to do it again
	    fprintf(stderr, "Error compressing stdin for server\r\n");
	    exit(1);
	  }
	}
	int cww1 = wait(&status);
	if (cww1 == -1) {
	  fprintf(stderr, "Error waiting for child: %s\n", strerror(errno));
	  exit(1);
	}
	fprintf(stderr, "SHELL EXIT SIGNAL=%dSTATUS=%d\n", (status & 0x007f) , ((status & 0xff00) >> 8));
	close(newsockfd);
	close(sockfd);
	exit(0);
      }
      if (fds[0].revents & POLLIN){ // input from socket ready
	cr = read(newsockfd, buffer, BUF_LEN);
	if (cr == -1) {
	  client_closed = 1;
	  continue;
	}
	if (cr == 0){
	  client_closed = 1;
	  continue;
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

	if (compress_opt == 1) {
	  inflateInit(&sock_to_stdout);
	  do{
	    inflate(&sock_to_stdout, Z_SYNC_FLUSH);
	  }while(sock_to_stdout.avail_in > 0);
	  cr = BUF_LEN - sock_to_stdout.avail_out;
	}
	
	
	int i;
	for (i = 0; i < cr; i++){
	  if (shell_pipe_closed == 1) { break; };
	  int cw;
	  unsigned char cc;
	  if (compress_opt == 1){
	    cc = compress_buffer[i];
	  }
	  else{
	    cc = buffer[i];
	  }
	  switch (cc){
	  case (unsigned char) EOF:
	    shell_pipe_closed = 1;
	    break;
	  case '\n':
	  case '\r':
	    cw = write(shell_in[1], nl, 1);
	    if (cw == -1){
	      fprintf(stderr, "Error writing to shell pipe: %s\n", strerror(errno));
	      exit(1);
	    }
	    else if (cw == SIGPIPE){
	      shell_pipe_closed = 1;
	    }
	    break;
	  case 0x03: // ^C
	    if (kill(cpid, SIGINT) == -1){
	      fprintf(stderr, "Error sending kill signal to shell: %s\n", strerror(errno));
	    }
	    break;
	  case 0x04: // ^D
	    if (close(shell_in[1]) == -1){
	      fprintf(stderr, "Error closing pipe to shell: %s\n", strerror(errno));
	    }
	    shell_pipe_closed = 1;
	    break;
	  default:
	    if ((cw = write(shell_in[1], &cc, 1)) == -1){
	      fprintf(stderr, "Error writing to shell pipe: %s\n", strerror(errno));
	      exit(1);
	    }
	    else if (cw == SIGPIPE){
	      shell_pipe_closed = 1;
	    }
	    break;
	  }
	}
      }
      if (fds[1].revents & POLLIN){ //input from the shell ready
	cr = read(shell_out[0], buffer, BUF_LEN);
	if (cr == -1) {
	  fprintf(stderr, "Error reading from shell pipe: %s\n", strerror(errno));
	  exit(1);
	}
	int i;
	int cww;
	int ci = 0;
	for (i = 0; i < cr; i++){
	  switch (buffer[i]){
	  case (unsigned char)EOF:
	    cww = wait(&status);
	    if (cww == -1) {
	      fprintf(stderr, "Error waiting for child: %s\n", strerror(errno));
	      exit(1);
	    }
	    fprintf(stderr, "SHELL EXIT SIGNAL=%dSTATUS=%d\n", (status & 0x007f) , ((status & 0xff00) >> 8));
	    close(newsockfd);
	    close(sockfd);
	    exit(0);
	  case '\r':
	  case '\n':
	    if ( (compress_opt == -1) && (write(newsockfd, nl, 1) == -1) ){
	      fprintf(stderr, "Error writing to socket: %s\n", strerror(errno));
	      exit(1);
	    }
	    if (compress_opt == 1){
	      input_buf[ci] = *nl;
	      ci++;
	    }
	    /* write(STDOUT_FILENO, nl, 1); */
	    break;
	  default:
	    if ( (compress_opt == -1) && (write(newsockfd, &buffer[i], 1) == -1) ){
	      fprintf(stderr, "Error writing to socket: %s\n", strerror(errno));
	      exit(1);
	    }
	    if (compress_opt == 1){
	      input_buf[ci] = buffer[i];
	    }
	    /* write(STDOUT_FILENO, &buffer[i], 1); */
	    /* break; */
	    break;
	  }
	}
	//need to compress and send over
	if (compress_opt == 1){
	  if (deflateInit(&stdin_to_sock, Z_DEFAULT_COMPRESSION) == Z_OK){
	    do {
	      deflate(&stdin_to_sock, Z_SYNC_FLUSH);
	    }while (stdin_to_sock.avail_out > 0);
	    write(newsockfd, compress_buffer, BUF_LEN - stdin_to_sock.avail_out);	      
	  }
	  else{//failed need to do it again
	    fprintf(stderr, "Error compressing stdin for server\r\n");
	    exit(1);
	  }
	}
      }
    }
  }
  exit(0);
}



void get_parameters(int argc, char* argv[], int* pn, int* copt){
  while(1){
    int c, option_index;
    static struct option long_opt[] = {
      {"port", required_argument, 0, 'p'},
      {"compress", no_argument, 0, 'c'},
      {0,0,0,0}
    };
    //opterr = 0;
    c = getopt_long(argc, argv, "", long_opt, &option_index);
    if (c == -1) { break; }

    switch(c){
    case 'p':
      *pn = atoi(optarg);
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

}
