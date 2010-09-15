#include "dce-manager.h"
#include "process.h"
#include "utils.h"
#include "dce-errno.h"
#include "dce-signal.h"
#include "dce-netdb.h"
#include "dce-unistd.h"
#include "dce-time.h"
#include "sys/dce-socket.h"
#include "dce-pthread.h"
#include "dce-stdio.h"
#include "dce-stdarg.h"
#include "dce-stdlib.h"
#include "sys/dce-ioctl.h"
#include "dce-sched.h"
#include "arpa/dce-inet.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include "dce-random.h"
#include "net/dce-if.h"
#include "ns3/names.h"
#include "ns3/random-variable.h"
#include "ns3/ipv4-l3-protocol.h"


NS_LOG_COMPONENT_DEFINE ("Simu");

using namespace ns3;

int *dce_get_errno (void)
{
  GET_CURRENT_NOLOG ();
  return &current->err;
}
pid_t dce_getpid (void)
{
  GET_CURRENT_NOLOG ();
  return current->process->pid;
}
pid_t dce_getppid (void)
{
  GET_CURRENT_NOLOG ();
  return current->process->ppid;
}
uid_t dce_getuid (void)
{
  GET_CURRENT_NOLOG ();
  return current->process->ruid;
}
uid_t dce_geteuid (void)
{
  GET_CURRENT_NOLOG ();
  return current->process->euid;
}
static bool is_ucapable (void)
{
  GET_CURRENT_NOLOG ();
  return current->process->euid == 0;
}
static bool is_gcapable (void)
{
  GET_CURRENT_NOLOG ();
  return current->process->egid == 0;
}
static bool is_set_ucapable (uid_t uid)
{
  GET_CURRENT (uid);
  return is_ucapable () ||
    current->process->euid == uid ||
    current->process->ruid == uid ||
    current->process->suid == uid;
}
static bool is_set_gcapable (gid_t gid)
{
  GET_CURRENT (gid);
  return is_gcapable () ||
    current->process->egid == gid ||
    current->process->rgid == gid ||
    current->process->sgid == gid;
}

int dce_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
  GET_CURRENT (ruid << euid << suid);
  if (ruid != (uid_t)-1 &&
      !is_set_ucapable (ruid))
    {
      current->err = EPERM;
      return -1;
    }
  if (euid != (uid_t)-1 &&
      !is_set_ucapable (euid))
    {
      current->err = EPERM;
      return -1;
    }
  if (suid != (uid_t)-1 &&
      !is_set_ucapable (suid))
    {
      current->err = EPERM;
      return -1;
    }
  if (ruid != (uid_t)-1)
    {
      current->process->ruid = ruid;
    }
  if (euid != (uid_t)-1)
    {
      current->process->euid = euid;
    }
  if (suid != (uid_t)-1)
    {
      current->process->suid = suid;
    }

  return 0;
}
int dce_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
  GET_CURRENT (rgid << egid << sgid);
  if (rgid != (gid_t)-1 &&
      !is_set_ucapable (rgid))
    {
      current->err = EPERM;
      return -1;
    }
  if (egid != (gid_t)-1 &&
      !is_set_ucapable (egid))
    {
      current->err = EPERM;
      return -1;
    }
  if (sgid != (gid_t)-1 ||
      !is_set_ucapable (sgid))
    {
      current->err = EPERM;
      return -1;
    }
  if (rgid != (gid_t)-1)
    {
      current->process->rgid = rgid;
    }
  if (egid != (gid_t)-1)
    {
      current->process->egid = egid;
    }
  if (sgid != (gid_t)-1)
    {
      current->process->sgid = sgid;
    }
  return 0;
}
int dce_setreuid(uid_t ruid, uid_t euid)
{
  GET_CURRENT (ruid << euid);
  return dce_setresuid (ruid,euid,-1);
}
int dce_setregid(gid_t rgid, gid_t egid)
{
  GET_CURRENT (rgid << egid);
  return dce_setresgid (rgid,egid,-1);
}

int dce_seteuid(uid_t euid)
{
  GET_CURRENT (euid);
  return dce_setresuid (-1, euid, -1);
}
int dce_setegid(gid_t egid)
{
  GET_CURRENT (egid);
  return dce_setresgid (-1, egid, -1);
}
int dce_setuid(uid_t uid)
{
  GET_CURRENT (uid);
  if (is_set_ucapable (uid))
    {
      current->process->ruid = uid;
      if (current->process->euid == 0)
	{
	  current->process->euid = uid;
	  current->process->suid = uid;
	}
      return 0;
    }
  else
    {
      current->err = EPERM;
      return -1;
    }
}
int dce_setgid(gid_t gid)
{
  GET_CURRENT (gid);
  if (is_set_gcapable (gid))
    {
      current->process->rgid = gid;
      if (current->process->egid == 0)
	{
	  current->process->egid = gid;
	  current->process->sgid = gid;
	}
      return 0;
    }
  else
    {
      current->err = EPERM;
      return -1;
    }
}

unsigned int dce_sleep(unsigned int seconds)
{
  Thread *current = Current ();
  NS_LOG_FUNCTION (current << UtilsGetNodeId ());
  NS_ASSERT (current != 0);
  current->process->manager->Wait (Seconds (seconds));
  return 0;
}


int dce_kill (pid_t pid, int sig)
{
  Thread *current = Current ();
  NS_LOG_FUNCTION (current << UtilsGetNodeId () << pid << sig);
  Process *process = current->process->manager->SearchProcess (pid);

  if (process == 0)
    {
      current->err = ESRCH;
      return -1;
    }

  UtilsSendSignal (process, SIGKILL);

  return 0;
}

int dce_pause (void)
{
  //Thread *current = Current ();
  //NS_LOG_FUNCTION (current << UtilsGetNodeId ());
  //NS_ASSERT (current != 0);
  // XXX
  return 0;
}


int dce_gettimeofday (struct timeval *tv, struct timezone *tz)
{
  NS_LOG_FUNCTION (Current () << UtilsGetNodeId ());
  NS_ASSERT (Current () != 0);
  NS_ASSERT (tz == 0);
  *tv = UtilsTimeToTimeval (UtilsSimulationTimeToTime (Now ()));
  return 0;
}
time_t dce_time (time_t *t)
{
  NS_LOG_FUNCTION (Current () << UtilsGetNodeId ());
  NS_ASSERT (Current () != 0);
  time_t time = (time_t)UtilsSimulationTimeToTime (Now ()).GetSeconds ();
  if (t != 0)
    {
      *t = time;
    }
  return time;
}
int dce_nanosleep (const struct timespec *req, struct timespec *rem) {
  Thread *current = Current ();
  NS_LOG_FUNCTION (current << UtilsGetNodeId ());
  NS_ASSERT (current != 0);
  if (req == 0) {
    current->err = EFAULT;
    return -1;
  }
  if ((req->tv_sec < 0) || (req->tv_nsec < 0) || (req->tv_nsec > 999999999)) {
    current->err = EINVAL;
    return -1;
  }
  Time reqTime = UtilsTimespecToTime (*req);
  Time remTime = current->process->manager->Wait (reqTime);
  if (remTime == Seconds (0.0))
    {
      return 0;
    }
  else
    {
      current->err = EINTR;
      if (rem != 0) *rem = UtilsTimeToTimespec (remTime);
      return -1;
    }
  }

long int dce_random (void) {
  Thread *current = Current ();
  return current->process->rndVarible.GetInteger ();
}
int dce_rand (void) {
  Thread *current = Current ();
  return current->process->rndVarible.GetInteger ();
}

//ignore seeds as RandomVariable implementation ensures that we take different random streams.
//TODO: support getting the same rng stream for several processes
void dce_srandom (unsigned int seed) {
  return;
}
void dce_srand (unsigned int seed) {
  return;
}

const char *dce_inet_ntop(int af, const void *src,
			   char *dst, socklen_t cnt)
{
  NS_LOG_FUNCTION (Current () << UtilsGetNodeId () << af << src << dst << cnt);
  Thread *current = Current ();
  const char *retval = inet_ntop (af, src, dst, cnt);
  if (retval == 0)
    {
      current->err = errno;
    }
  return retval;
}





int dce_getopt_r (int argc, char * const argv[], const char *optstring, 
		   char **poptarg, int *poptind, int *popterr, int *poptopt)
{
  NS_LOG_FUNCTION (Current () << UtilsGetNodeId () << argc << argv << optstring << poptarg << 
		   poptind << popterr << poptopt);
  NS_ASSERT (Current () != 0);
  NS_LOG_DEBUG ("optind=" << *poptind << 
		" opterr=" << *popterr << 
		" optopt=" << *poptopt);
  /* The following is pretty evil but it all comes down to the fact
   * that the libc does not export getopt_internal_r which is really the
   * function we want to call here.
   */
  char *optargsaved = optarg;
  int optindsaved = optind;
  int opterrsaved = opterr;
  int optoptsaved = optopt;
  optarg = *poptarg;
  optind = *poptind;
  opterr = *popterr;
  optopt = *poptopt;
  int retval = getopt (argc, argv, optstring);
  *poptarg = optarg;
  *poptind = optind;
  *popterr = opterr;
  *poptopt = optopt;
  optarg = optargsaved;
  optind = optindsaved;
  opterr = opterrsaved;
  optopt = optoptsaved;
  return retval;
}
int dce_getopt_long_r (int argc, char * const argv[], const char *optstring, 
                        const struct option *longopts, int *longindex,
                        char **poptarg, int *poptind, int *popterr, int *poptopt)
{
  NS_LOG_FUNCTION (Current () << "node" << UtilsGetNodeId () << argc << argv << optstring << 
                   longopts << longindex);
  NS_ASSERT (Current () != 0);
  /* The following is pretty evil but it all comes down to the fact
   * that the libc does not export getopt_internal_r which is really the
   * function we want to call here.
   */
  char *optargsaved = optarg;
  int optindsaved = optind;
  int opterrsaved = opterr;
  int optoptsaved = optopt;
  optarg = *poptarg;
  optind = *poptind;
  opterr = *popterr;
  optopt = *poptopt;
  int retval = getopt_long (argc, argv, optstring, longopts, longindex);
  *poptarg = optarg;
  *poptind = optind;
  *popterr = opterr;
  *poptopt = optopt;
  optarg = optargsaved;
  optind = optindsaved;
  opterr = opterrsaved;
  optopt = optoptsaved;
  return retval;
}
int dce_sched_yield (void)
{
  Thread *current = Current ();
  NS_LOG_FUNCTION (current << UtilsGetNodeId ());
  NS_ASSERT (current != 0);
  current->process->manager->Yield ();
  return 0;
}
static void Itimer (Process *process)
{
  if (!process->itimerInterval.IsZero ())
    {
      process->itimer = Simulator::Schedule (process->itimerInterval,
					     &Itimer, process);
    }
  // wakeup one thread
  UtilsSendSignal (process, SIGALRM);
}
int dce_getitimer(int which, struct itimerval *value)
{

  Thread *current = Current ();
  NS_LOG_FUNCTION (current << UtilsGetNodeId () << which << value);
  NS_ASSERT (current != 0);
  if (value == 0)
    {
      current->err = EFAULT;
      return -1;
    }
  // We don't support other kinds of timers.
  NS_ASSERT (which == ITIMER_REAL);
  value->it_interval = UtilsTimeToTimeval (current->process->itimerInterval);
  value->it_value = UtilsTimeToTimeval (Simulator::GetDelayLeft (current->process->itimer));
  return 0;
}
int dce_setitimer(int which, const struct itimerval *value,
		   struct itimerval *ovalue)
{
  Thread *current = Current ();
  NS_LOG_FUNCTION (current << UtilsGetNodeId () << which << value << ovalue);
  NS_ASSERT (current != 0);
  if (value == 0)
    {
      current->err = EINVAL;
      return -1;
    }
  // We don't support other kinds of timers.
  NS_ASSERT (which == ITIMER_REAL);
  if (ovalue != 0)
    {
      ovalue->it_interval = UtilsTimeToTimeval (current->process->itimerInterval);
      ovalue->it_value = UtilsTimeToTimeval (Simulator::GetDelayLeft (current->process->itimer));
    }

  current->process->itimer.Cancel ();
  current->process->itimerInterval = UtilsTimevalToTime (value->it_interval);
  if (value->it_value.tv_sec == 0 &&
      value->it_value.tv_usec == 0)
    {
      return 0;
    }
  current->process->itimer = Simulator::Schedule (UtilsTimevalToTime (value->it_value),
						  &Itimer, current->process);
  return 0;
}
char *dce_getcwd (char *buf, size_t size)
{
  Thread *current = Current ();
  NS_LOG_FUNCTION (current << UtilsGetNodeId () << buf << size);
  NS_ASSERT (current != 0);
  uint32_t cwd_size = current->process->cwd.size ();
  if ((buf != 0 && size < cwd_size + 1)
      || (buf == 0 && size != 0 && size < cwd_size + 1))
    {
      current->err = ERANGE;
      return 0;
    }
  // from here on, we know that we will have enough space
  // in the buffer for the strcpy
  if (buf == 0)
    {
      if (size == 0)
	{
	  buf = (char *)dce_malloc (cwd_size + 1);
	  size = cwd_size + 1;
	}
      else
	{
	  buf = (char *)dce_malloc (size);
	}
      buf[size-1] = 0;
    }
  const char *source = current->process->cwd.c_str ();
  strcpy (buf, source);
  return buf;
}
char *dce_getwd (char *buf)
{
  NS_LOG_FUNCTION (Current () << UtilsGetNodeId () << buf);
  NS_ASSERT (Current () != 0);
  Thread *current = Current ();  
  uint32_t cwd_size = current->process->cwd.size ();  
  if (PATH_MAX < cwd_size + 1)
    {
      current->err = ENAMETOOLONG;
      return 0;
    }
  const char *source = current->process->cwd.c_str ();
  strcpy (buf, source);
  return buf;  
}
char *dce_get_current_dir_name (void)
{
  NS_LOG_FUNCTION (Current () << UtilsGetNodeId ());
  NS_ASSERT (Current () != 0);
  return dce_getcwd (0, 0);
}

int dce_chdir(const char *path)
{
  Thread *current = Current ();
  NS_LOG_FUNCTION (current << UtilsGetNodeId ());
  NS_ASSERT (current != 0);

  int retval;
  std::string newCwd = UtilsGetRealFilePath (path);
  // test to see if the target directory exists
  retval = ::open (newCwd.c_str (), O_DIRECTORY | O_RDONLY);
  if (retval == -1)
    {
      current->err = errno;
      return -1;
    }
  current->process->cwd = UtilsGetVirtualFilePath (path);
  return 0;
}
int dce_fchdir(int fd)
{
  //XXX that one is not super trivial to implement.
  // this fd is coming from the function dirfd
  return 0;
}

unsigned dce_if_nametoindex (const char *ifname)
{
  int index = 0;
  Ptr<Node> node = Current ()->process->manager->GetObject<Node> ();
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();

  for (uint32_t i = 0; i < node->GetNDevices (); ++i)
    {
      Ptr<NetDevice> dev = node->GetDevice (i);
      if (ifname == Names::FindName (dev))
        {
          index = ipv4->GetInterfaceForDevice (dev);
          return index;
        }
    }
  return 0;
}