#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define RELAY_MAGIC 'R'
#define RELAY_CMD_OFF _IO(RELAY_MAGIC, 0)
#define RELAY_CMD_ON _IO(RELAY_MAGIC, 1)

int main()
{
    int fd = open("/dev/relay", O_RDWR);
    if (fd < 0)
    {
        perror("Failed to open /dev/relay");
        return -1;
    }

    printf("Turning Relay ON ... \n");
    ioctl(fd, RELAY_CMD_ON);
    sleep(2);

    printf("Turning Relay OFF ... \n");
    ioctl(fd, RELAY_CMD_OFF);

    close(fd);
    return 0;
}

