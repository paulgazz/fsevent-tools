#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <CoreServices/CoreServices.h> 

/* the latency in seconds receiving the event */
#define LATENCY .5

/* the maximum number of event flags to check for */
#define NEVENTFLAGS 20

int exec_flag, monitor_flag;
char **exec_args;

static struct option longopts[] = {
  { "monitor", no_argument, NULL, 'm' },
  { "exec", required_argument, NULL, 'e' },
  { NULL, 0, NULL, 0 }
};

char *get_event_name(FSEventStreamEventFlags flag)
{
  switch (flag) {
  case kFSEventStreamEventFlagMustScanSubDirs:   return "MUSTSCANSUBDIRS"; 
  case kFSEventStreamEventFlagUserDropped:       return "USERDROPPED";
  case kFSEventStreamEventFlagKernelDropped:     return "KERNELDROPPED";
  case kFSEventStreamEventFlagEventIdsWrapped:   return "IDSWRAPPED";
  case kFSEventStreamEventFlagHistoryDone:       return "HISTORYDONE";
  case kFSEventStreamEventFlagRootChanged:       return "ROOTCHANGED";
  case kFSEventStreamEventFlagMount:             return "MOUNT";
  case kFSEventStreamEventFlagUnmount:           return "UNMOUNT";
  case kFSEventStreamEventFlagItemCreated:       return "CREATED";
  case kFSEventStreamEventFlagItemRemoved:       return "REMOVED";
  case kFSEventStreamEventFlagItemInodeMetaMod:  return "INODEMETAMOD";
  case kFSEventStreamEventFlagItemRenamed:       return "RENAMED";
  case kFSEventStreamEventFlagItemModified:      return "MODIFIED";
  case kFSEventStreamEventFlagItemFinderInfoMod: return "FINDERINFOMOD";
  case kFSEventStreamEventFlagItemChangeOwner:   return "CHANGEOWNER";
  case kFSEventStreamEventFlagItemXattrMod:      return "XATTRMOD";
  case kFSEventStreamEventFlagItemIsFile:        return "ISFILE";
  case kFSEventStreamEventFlagItemIsDir:         return "ISDIR";
  case kFSEventStreamEventFlagItemIsSymlink:     return "SYMLINK";
  default:                                       return NULL;
  }
}

void emit_event_info(
  ConstFSEventStreamRef streamRef, 
  void *clientCallBackInfo, 
  size_t numEvents, 
  void *eventPaths, 
  const FSEventStreamEventFlags eventFlags[], 
  const FSEventStreamEventId eventIds[])
{ 
  int i;

  for (i = 0; i < numEvents; i++) {
    char *path = ((char **) eventPaths)[i];
    FSEventStreamEventFlags flags = eventFlags[i];
    int isfile = flags & kFSEventStreamEventFlagItemIsFile;
    char delim = ' ';
    int r;

    if (isfile)
      printf("%s", dirname(path));
    else
      printf("%s", path);

    for (r = 0; r < NEVENTFLAGS; r++) {
      if (flags & 0x1) {
        char *event_name;

        event_name = get_event_name(0x1 << r);
        if (NULL == event_name) {
          fprintf(stderr, "warning: unsupported event flag\n");
          event_name = "UNKNOWN";
        }

        printf("%c%s", delim, event_name);
        delim = ',';
      }
      flags >>= 1;
    }

    if (isfile)
      printf(" %s\n", basename(path));
    else
      printf("\n");

    fflush(stdout);
  }

  if (exec_flag) {
    pid_t pid;

    pid = fork();
    if (pid < 0) {
      perror("fork");
      exit(1);
    } else if (pid == 0) {
      if (execv(exec_args[0], exec_args) < 0) {
        perror("execv");
        exit(1);
      }
    } else {
      int status;

      if (wait(&status) < 0) {
        perror("wait");
        exit(1);
      }
    }
  }

  if (! monitor_flag) {
    exit(0);
  }
}

void print_usage()
{
  printf("USAGE\n  fseventwait [options] file1 [file2] ...\n\n");
  printf("OPTIONS\n");
  printf("  -m, --monitor            listen forever instead of exiting after one event\n");
  printf("  -e, --exec \"cmd [args]\"  execute shell command and its args after each event\n");
}

int main(int argc, char **argv)
{
  int ch;

  /* collect arguments */
  exec_flag = 0;
  monitor_flag = 0;
  while ((ch = getopt_long(argc, argv, "me:", longopts, NULL)) != -1)
    switch (ch) {
    case 'm':
      monitor_flag = 1;
      break;
    case 'e':
      exec_flag = 1;
      exec_args = malloc(sizeof(char *) * 4);
      exec_args[0] = "/bin/bash";
      exec_args[1] ="-c";
      exec_args[2] = strdup(optarg);
      exec_args[3] = 0;
      break;
    default:
      print_usage();
      exit(1);
    }
  argc -= optind;
  argv += optind;

  if (argc == 0) {
    print_usage();
    exit(1);
  }

  /* the remaining arguments are the paths to watch */
  CFMutableArrayRef pathsToWatch = CFArrayCreateMutable(NULL, argc, NULL);
  int i;
  for (i = 0; i < argc; i++) {
    CFStringRef mypath = CFStringCreateWithCString(
      NULL,
      argv[i],
      kCFStringEncodingUTF8);
    CFArrayAppendValue(pathsToWatch, mypath);
  }

  /* setup the event stream */
  FSEventStreamRef stream = FSEventStreamCreate(
    NULL, // null allocator
    &emit_event_info,
    NULL, // null context
    pathsToWatch,
    kFSEventStreamEventIdSinceNow,
    LATENCY,
    kFSEventStreamCreateFlagFileEvents); 

  FSEventStreamScheduleWithRunLoop(
    stream,
    CFRunLoopGetCurrent(),
    kCFRunLoopDefaultMode); 

  FSEventStreamStart(stream);
  CFRunLoopRun();
}
