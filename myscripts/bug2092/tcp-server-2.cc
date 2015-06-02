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

  uint8_t buf[10240];

  memset (buf, 0, 10240);
  ssize_t tot = 0;

  for (uint32_t i = 0; i < 100; i++)
    {
      ssize_t n = 10240;
      while (n > 0)
        {
          ssize_t bytes_read = read (fd, &buf[10240 - n], n);

          if (bytes_read > 0)
            {
              n -= bytes_read;

              std::cout << "read:" << bytes_read << " n:" << n << std::endl;

              tot += bytes_read;
            }
          else
            {
              break;
            }

        }
         sleep (1);
    }

  std::cout << "did read all buffers tot:" << tot << std::endl;

  close (sock);
  close (fd);

  return 0;
}
