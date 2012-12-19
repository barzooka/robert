#include <stdio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdarg.h>

typedef long long usecs_t;

extern int gDebug = 0;

#define MAX_REPEAT 2147483640

#define EVIOCGVERSION _IOR('E', 0x01, int) /* get driver version */

#define PRINT_IF(a, ...) if (a) printf(__VA_ARGS__)


typedef struct {
    char* devpath;
    int fd;
} device_fd_map_t;

#define MAX_FD 256
device_fd_map_t gFds[MAX_FD];
int gFdOpened = 0;

int getOrOpenFd(char* device)
{
    for (int i = 0; i < gFdOpened; i++)
    {
        if (!strcmp(gFds[i].devpath, device))
            return gFds[i].fd;
    }

    // open fd
    if (gFdOpened == MAX_FD)
        return -1;

    int fd = open(device, O_RDWR);

    if(fd < 0) {
        fprintf(stderr, "could not open %s, %s\n", device, strerror(errno));
        return -1;
    }

    int version;
    if (ioctl(fd, EVIOCGVERSION, &version)) {
        fprintf(stderr, "could not get driver version for %s, %s\n", device, strerror(errno));
        return -1;
    }

    gFds[gFdOpened].devpath = strdup(device);
    gFds[gFdOpened].fd = fd;
    gFdOpened++;
    return fd;
}

void clearFds()
{
    for (int i = 0; i < gFdOpened; i++)
    {
        free(gFds[i].devpath);
        close(gFds[i].fd);
    }
}

// from sendevent.cpp
int sendevent(int fd, int type, int code, int value);

// from getevent.cpp
int recordEvent(char* recFile);

void print_usage()
{
    printf("\n");
    printf("robert -rec:<output file>\n");
    printf("    Record event queue for playback:");
    printf("\n");
    printf("    To record a event queue, use command below:\n");
    printf("        robert -rec:/data/eventfile\n");
    printf("             This will record your actions in to /data/eventfile.\n");
    printf("             Use Ctrl+C to stop the recording.\n");
    printf("\n");
    printf("robert [eventfile] [-r:REPEAT_TIMES] [-i:INTERVAL] [-ri:REPEAT_INTERVAL]\n");
    printf("    Playback eventqueue stored in eventfile.\n");
    printf("\n");
    printf("    eventfile:              File path which stores the event queue. If this parameter is not present, standart input will be used.\n");
    printf("                            NOTE: this parameter must be the first param of this tool.\n");
    printf("    -r:REPEAT_TIMES:        Indicate the repeat times of playback.\n");
    printf("                            NOTE: You can specify a value lower than 0 to repeat forever. eg. -r:-1\n");
    printf("    -i:INTERVAL:            Indicate the interval between two events, in MILLISECONDS.\n");
    printf("                            If INTERVAL is lower than 0, robert will use the time stamp in recorded in the eventfile\n");
    printf("    -ri:REPEAT_INTERVAL:    Indicate the interval between repeats, in MILLISECONDS\n");
    printf("\n");
    printf("    Example:\n");
    printf("        robert /data/eventfile -r:5 -i:100\n");
    printf("    This will playback the recorded event queue for 5 times, and with interval of 100ms between two events.\n\n");
    printf("    And the following command does the same work:\n");
    printf("        cat /data/eventfile | robert -r:5 -i:100\n");
    printf("    You can use grep to filter the events:\n");
    printf("        cat /data/eventfile | grep event0 | robert");
    printf("\n");
    printf("Common parameters:\n");
    printf("    -d:                     Print extra debug informations.\n");
    printf("    -h:                     Print this message.\n");
    printf("\n");
}

usecs_t combineTime(int sec, int usec)
{
    return sec * 1000 * 1000 + usec;
}

usecs_t systemTime()
{
    timeval tv;
    gettimeofday(&tv, NULL);
    return combineTime(tv.tv_sec, tv.tv_usec);
}

void splite(char* buffer, char** first, char** last)
{
    int len = strlen(buffer);
    for (char* p = buffer + len; p != buffer; p--)
    {
        if (*p == ':')
        {
            *p = '\0';
            *first = buffer;
            *last = p + 1;
            return;
        }
    }
    *first = *last = buffer;
}

int main(int argc, char* argv[])
{
    if (argc > 6)
    {
        print_usage();
        return 2; 
    }

    char* eventFilePath = NULL;

    int repeat = 1;
    int interval = -1;

    int repeatInterval = 1000;

    for (int i = 1; i < argc; i++)
    {
        char* arg = *(argv + i);
        if (!strncmp(arg, "-r:", 3))
        {
            repeat = atoi(arg + 3);
        }
        else if (!strncmp(arg, "-i:", 3))
        {
            interval = atoi(arg + 3) * 1000;
        }
        else if (!strncmp(arg, "-ri:", 4))
        {
            repeatInterval = atoi(arg + 4) * 1000;
        }
        else if (!strcmp(arg, "-d"))
        {
            gDebug = 1;
        }
        else if (!strcmp(arg, "-h"))
        {
            print_usage();
            return 0;
        }
        else if (!strcmp(arg, "-rec"))
        {
            return recordEvent("./event_queue");
        }
        else if (!strncmp(arg, "-rec:", 5))
        {
            return recordEvent(arg + 5);
        }
        else
        {
            if (i == 1 && strncmp(arg, "-", 1))
            {
                eventFilePath = argv[1];
            }
            else
            {
                printf("Unkown parameter %s\n", arg);
                print_usage();
                return 2;
            }
        }
    }

    memset(gFds, 0, sizeof(device_fd_map_t) * MAX_FD);

    if (repeat >= MAX_REPEAT)
    {
        printf("Repeat times %d limit exceed.\n", MAX_REPEAT - 1);
    }

    printf("Play with REPEATE=%d, INTERVAL=%d\n", repeat, interval);

    // control the repeat
    for (int r = 0; r != repeat; r = (r + 1) % MAX_REPEAT)
    {
        // open recorded event queue.
        FILE* file = NULL;

        if (eventFilePath)
        {
            file = fopen(eventFilePath, "rt");
            if (!file)
            {
                printf("Can not open file %s, is it exists?\n", eventFilePath);
                return 1;
            }
        }
        else
        {
            printf(">> No event file specified. Using stdin instead. <<\n");
            file = stdin;
            if (repeat != 1)
            {
                printf("WARNING: -r parameters can not be used while using stdin.\n");
                printf("         Reseting to -r:1\n");
                repeat = 1;
            }
        }

        char* first,* last;
        char buffer[1024];
        int sec = 0, usec = 0;
        char devicePath[256];
        unsigned int type = 0, code = 0, arg = 0;
        usecs_t timeOffset = 0;
        bool timeOffsetSet = false;
        usecs_t lastEventTime = 0;

        // starts playback
        while (fgets(buffer, 1024, file) != NULL)
        {
            sec = usec = -1;
            PRINT_IF(gDebug, "Parsing %s\n", buffer);
            splite(buffer, &first, &last);

            // try android 4.0 output style
            sscanf(first, "%d-%d: %s", &sec, &usec, devicePath);

            if (sec == -1 || usec == -1)
            {
                //try android 4.1 & 4.2 output style
                sscanf(first, "[%d.%d] %s", &sec, &usec, devicePath);
                if (sec == -1 || usec == -1)
                {
                    printf("Skiping invalid line: %s\n", buffer);
                    continue;  // This line is not a valid event record
                }
            }

            sscanf(last, "%x %x %x", &type, &code, &arg);

            PRINT_IF(gDebug, "Event Timestamp=%d.%ds\n", sec, usec);
            int fd = getOrOpenFd(devicePath);
            // Calculating times to sleep
            usecs_t sleepTime = 0;

            // event time stamp
            usecs_t time = combineTime(sec, usec);

            if (!timeOffsetSet)
            {
                timeOffsetSet = true;
                timeOffset = systemTime() - time;
            }

            sleepTime = (time + timeOffset) - systemTime();

            if (interval > 0)
            {
                usleep(interval);
            }
            else if (interval == 0)
            {
            }
            else if (sleepTime > 0)
            {
                PRINT_IF(gDebug || sleepTime > 1000000 /* 1s */, "Sleep %lld.%llds\n", sleepTime / 1000000, sleepTime % 1000000);
                usleep(sleepTime);
            }

            PRINT_IF(gDebug, "-> %s: %d, %d, %d\n", devicePath, type, code, arg);

            if (sendevent(fd, type ,code, arg) != 0)
                exit(0);
        }
        fclose(file);
        if (repeat < 0)
            printf ("Done playback [ %d / Forever ]\n", r + 1);
        else
            printf ("Done playback [ %d / %d ]\n", r + 1, repeat);
        if (repeatInterval > 0 && r + 1 != repeat)
            usleep(repeatInterval);
    }
    clearFds();
    printf("All done\n");
    exit(0);
}


