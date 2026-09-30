#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

int main()
{
    int fd = open("/dev/pir", O_RDONLY);
    if (fd < 0)
    {
        perror("failed to open /dev/pir");
        return -1;
    }

    struct pollfd fds[1];
    fds[0].fd = fd;
    fds[0].events = POLLIN;

    printf("Waiting for PIR motion trigger (blocking) ... Press Ctrl+C to stop.\n");

    while (1)
    {
        int ret = poll(fds, 1, -1);
        if (ret > 0)
        {
            if (fds[0].revents & POLLIN)
            {
                int event = 0;
                read(fd, &event, sizeof(event));
                printf(">>> Motion Detected!  High pulse captured via IRQ! <<<\n");
            }
        }
    }

    close(fd);
    return 0;
}
