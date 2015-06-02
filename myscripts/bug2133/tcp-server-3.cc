#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <sys/fcntl.h>
#include <assert.h>

#define SERVER_PORT 2000

int
main (int argc, char *argv[])
{
  int sock;
  sock = socket (PF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons (SERVER_PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  fcntl(sock, F_SETFL, O_NONBLOCK);

  int status;
  status = bind (sock, (const struct sockaddr *) &addr, sizeof(addr));
  status = listen (sock, 1);
  
  fd_set fds;
  FD_ZERO (&fds);
  FD_SET (sock, &fds);
  
  int nfds = select (sock + 1, &fds, NULL, NULL, NULL);

  int fd = accept (sock, 0, 0);
  /*
  * The select should return a socket ready for the accept.
  */
  if (fd == -1)
    {
      std::cout << " ACCEPT FAIL: " << fd << std::endl;
      exit (1);
    }
  std::cout << " accept -> " << fd << std::endl;

  uint8_t buf[500];

  memset (buf, 0, 500);

  fd_set rfds;
  FD_ZERO (&rfds);
  FD_SET (fd, &rfds);

  struct timeval timeOut;
  timeOut.tv_sec = 6;
  timeOut.tv_usec = 0;
  ssize_t bytes_read = 0;

  nfds = select (fd + 1, &rfds, NULL, NULL, &timeOut);
  if (nfds > 0)
    bytes_read = read (fd, &buf, 500);

  std::cout << "read:" << bytes_read << std::endl;

  close (fd);
  
  sleep(100);
  close (sock);

  return 0;
}
