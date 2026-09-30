# 基于 RPi 5 与 STM32F407 的异构实时控制系统
## 目录结构

```text
.
├── 003/         # STM32F407 侧工程代码 (STM32CubeIDE / HAL)
└── rpi/         # 树莓派侧代码
```

## 如何使用
### 1. STM32 侧 (/003)

使用 STM32CubeIDE 打开 /003 目录下的工程。

配置 USART1/2 波特率为 115200 8N1，开启全局中断。

编译并烧录至 STM32F407 开发板。

### 2. 树莓派侧 (/rpi)
(1) 加载内核驱动
```Bash
cd rpi/driver
make
sudo dtoverlay my_devices.dtbo
sudo insmod pir_driver.ko
sudo insmod relay_driver.ko
```
(2) 编译并运行主程序
```Bash
cd rpi/app
gcc main.c -o app
sudo ./app
```
