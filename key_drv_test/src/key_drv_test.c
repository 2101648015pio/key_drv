/*********************************************************************************************************
 **
 **                                    中国软件开源组织
 **
 **                                   嵌入式实时操作系统
 **
 **                                SylixOS(TM)  LW : long wing
 **
 **                               Copyright All Rights Reserved
 **
 **--------------文件信息--------------------------------------------------------------------------------
 **
 ** 文   件   名: key_drv_test.c
 **
 ** 创   建   人: Liu.Jin (刘晋)
 **
 ** 文件创建日期: 2017 年  08月  17日
 **
 ** 描        述: 按键驱动测试程序
 *********************************************************************************************************/
#include <stdio.h>
#include "../../key_drv/src/key_drv.h"
/*********************************************************************************************************
** 函数名称: printMsg
** 功能描述: 打印按键消息
** 输　入  : fd 文件描述符
** 输　出  : ERROR_CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static int  printMsg (int fd)
{
    int     i, ret;
    KEY_MSG kmsg;

    for (i = 0; i < 10; i++) {
        ret = read(fd, &kmsg, sizeof(KEY_MSG));
        if (ret < 0) {
            printf("read error!\n");
            close(fd);
            return (-1);
        }
        if (kmsg.KEY_stat == 0) {
            printf("key%d is down!\n", kmsg.KEY_num);
        } else if (kmsg.KEY_stat == 1) {
            if ( kmsg.KEY_num == 0) {
                printf("all key is not down\n");
            } else {
                printf("key%d is not down!\n", kmsg.KEY_num);
            }
        }
    }
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: modPoll
** 功能描述: 设置轮询模式
** 输　入  : fd 文件描述符
** 输　出  : ERROR_CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static int  modPoll (int  fd)
{
    int     ret;

    ret = ioctl(fd, CMD_CTL_POLL, NULL);
    if (ret < 0) {
        printf("ioctl error!|n");
        close(fd);
        return (PX_ERROR);
    }
    printMsg(fd);

    return (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: modInt
** 功能描述: 设置中断模式
** 输　入  : fd 文件描述符
** 输　出  : ERROR_CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static int  modInt (int  fd)
{
    int     ret;

    ret = ioctl(fd, CMD_CTL_INT, NULL);
    if (ret < 0) {
        printf("ioctl error!|n");
        close(fd);
        return (PX_ERROR);
    }

    printMsg(fd);

    return (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: main
** 功能描述: 程序主函数
** 输　入  : 无
** 输　出  : ERROR_CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
int  main (int  argc, char  **argv)
{
    int            fd[2];
    int            ret, i;
    fd_set         fdset;
    struct timeval ptmvalTO;
    KEY_MSG        kmsg;

    fd[0] = open("/dev/key", O_RDONLY | O_NONBLOCK, 0444);
    fd[1] = open("/dev/key", O_RDONLY, 0444);
    if (fd[0] < 0 || fd[1] < 0) {
        printf("Failed to open /dev/key\n");
        return (-1);
    }

    printf("非阻塞模式，轮询:\n");
    modPoll(fd[0]);

    printf("非阻塞模式，中断:\n");
    modInt(fd[0]);

    printf("阻塞模式，轮询:\n");
    modPoll(fd[1]);

    printf("阻塞模式，中断:\n");
    modInt(fd[1]);

    printf("select功能测试:\n");
    FD_ZERO(&fdset);

    ptmvalTO.tv_sec  = 1;
    ptmvalTO.tv_usec = 0;

    for (i = 0; i < 10; i++){
        FD_SET(fd[1], &fdset);

        ret = select(fd[1] + 1, &fdset, NULL, NULL, &ptmvalTO);         /*  等待一秒                    */
        if (ret == 0){
            printf("Select timeout\n");
        } else if (ret < 0) {
            printf("Select error!\n");
            close(fd[0]);
            close(fd[1]);
            return (-1);
        }
        if (FD_ISSET(fd[1], &fdset)) {
            ret = read(fd[1], &kmsg, sizeof(KEY_MSG));
            if (ret < 0) {
                printf("Read error!\n");
                close(fd[0]);
                close(fd[1]);
                return (-1);
            }
            if (kmsg.KEY_stat == 0) {
                printf("Key%d is down!\n", kmsg.KEY_num);
            }
        }
    }

    close(fd[0]);
    close(fd[1]);

    return  (ERROR_NONE);
}
