#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>

#define PIR_DEV   "/dev/pir"
#define RELAY_DEV "/dev/relay"
#define UART_DEV  "/dev/serial0"

#define RELAY_MAGIC 'R'
#define RELAY_CMD_OFF _IO(RELAY_MAGIC, 0)
#define RELAY_CMD_ON  _IO(RELAY_MAGIC, 1)

int init_uart(const char *dev)
{
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) return -1;

    struct termios options;
    memset(&options, 0, sizeof(options));

    /* 115200 8N1 Raw 模式 */
    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;

    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 1;

    tcflush(fd, TCIOFLUSH);
    tcsetattr(fd, TCSANOW, &options);

    return fd;
}

int main()
{
    int fd_pir = open(PIR_DEV, O_RDONLY);
    int fd_relay = open(RELAY_DEV, O_RDWR);
    int fd_uart = init_uart(UART_DEV);

    if (fd_pir < 0 || fd_relay < 0 || fd_uart < 0) {
        perror("Failed to open device files");
        return -1;
    }

    printf("=== Heterogeneous System Online: RPi5 <-> STM32F407 ===\n");

    struct pollfd fds_pir[1];
    fds_pir[0].fd = fd_pir;
    fds_pir[0].events = POLLIN;

    while (1) {
        printf("\n[System Sleeping] Waiting for PIR Motion IRQ...\n");
        int ret = poll(fds_pir, 1, -1);

        if (ret > 0 && (fds_pir[0].revents & POLLIN)) {
            int event = 0;
            read(fd_pir, &event, sizeof(event));
            printf(">>> [Event] PIR Motion Triggered! <<<\n");

            /* 1. 触发 5V 继电器闭合 */
            ioctl(fd_relay, RELAY_CMD_ON);
            printf("[Action] 5V Relay Turned ON\n");

            /* 2. 清理串口，向 STM32 发送 3 字节命令帧 (0xAA 0x01 0x55) */
            tcflush(fd_uart, TCIFLUSH);
            unsigned char req_cmd[3] = {0xAA, 0x01, 0x55};
            write(fd_uart, req_cmd, 3);
            tcdrain(fd_uart);
            printf("[UART] Request RTC Timestamp sent to STM32\n");

            /* 3. 使用 poll 监听 STM32 的 7 字节响应帧 */
            struct pollfd fds_uart[1];
            fds_uart[0].fd = fd_uart;
            fds_uart[0].events = POLLIN;

            unsigned char rx_buf[16] = {0};
            int total_bytes = 0;
            int poll_ret = poll(fds_uart, 1, 150); // 150ms 超时保护

            if (poll_ret > 0 && (fds_uart[0].revents & POLLIN)) {
                usleep(10000); // 留 10ms 确保 7 字节传输全部入缓冲区
                total_bytes = read(fd_uart, rx_buf, sizeof(rx_buf));
            }

            /* 4. 解析 STM32 回传数据 */
            if (total_bytes >= 7 && rx_buf[0] == 0xAA && rx_buf[6] == 0x55) {
                printf("[STM32 RTC Time] %02d:%02d:%02d | Frame Valid: OK\n",
                       rx_buf[1], rx_buf[2], rx_buf[3]);
            } else if (total_bytes > 0) {
                printf("[UART RX Raw Data] Count: %d bytes | Hex: ", total_bytes);
                for (int i = 0; i < total_bytes; i++) printf("%02X ", rx_buf[i]);
                printf("\n");
            } else {
                printf("[UART Timeout] No frame from STM32\n");
            }

            /* 保持 2 秒后关闭继电器 */
            sleep(2);
            ioctl(fd_relay, RELAY_CMD_OFF);
            printf("[Action] 5V Relay Turned OFF\n");
        }
    }

    close(fd_pir);
    close(fd_relay);
    close(fd_uart);
    return 0;
}
