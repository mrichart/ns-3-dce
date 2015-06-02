#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <iostream>


int main (int argc, char *argv[])
{
  int sock;
  sock = socket (PF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons (2000);

  struct hostent *host = gethostbyname (argv[1]);
  memcpy (&addr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);

  int result;
  result = connect (sock, (const struct sockaddr *) &addr, sizeof (addr));
  
  if (result != -1)
    {
      uint8_t buf[500];

      memset (buf, 0x66, 20);
      memset (buf + 20, 0x67, 480);

      sleep(1);
      ssize_t e  = write (sock, &buf, 500);
                
      std::cout << "write: " << e << std::endl;
                
      fd_set fds;
      FD_ZERO (&fds);
      FD_SET (sock, &fds);
      
      struct timeval timeOut;
      timeOut.tv_sec = 5;
      timeOut.tv_usec = 0;
      
      int nfds = select (sock + 1, &fds, NULL, NULL, &timeOut);
      ssize_t bytes_read = 0;
      
      uint8_t bufRead[500];
      memset (bufRead, 0, 500);
      if (nfds > 0)
        bytes_read = read (sock, &bufRead, 500);
        
      std::cout << "read:" << bytes_read << std::endl;
    }
  else
    {
      std::cout << "Connection failed." << std::endl;
    }

  close (sock);

  return 0;
}
