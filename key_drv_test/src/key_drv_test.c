/*********************************************************************************************************
 **
 **                                    �й������Դ��֯
 **
 **                                   Ƕ��ʽʵʱ����ϵͳ
 **
 **                                SylixOS(TM)  LW : long wing
 **
 **                               Copyright All Rights Reserved
 **
 **--------------�ļ���Ϣ--------------------------------------------------------------------------------
 **
 ** ��   ��   ��: key_drv_test.c
 **
 ** ��   ��   ��: Liu.Jin (����)
 **
 ** �ļ���������: 2017 ��  08��  17��
 **
 ** ��        ��: �����������Գ���
 *********************************************************************************************************/
#include <stdio.h>
#include "../../key_drv/src/key_drv.h"
/*********************************************************************************************************
** ��������: printMsg
** ��������: ��ӡ������Ϣ
** �䡡��  : fd �ļ�������
** �䡡��  : ERROR_CODE
** ȫ�ֱ���:
** ����ģ��:
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
** ��������: modPoll
** ��������: ������ѯģʽ
** �䡡��  : fd �ļ�������
** �䡡��  : ERROR_CODE
** ȫ�ֱ���:
** ����ģ��:
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
** ��������: modInt
** ��������: �����ж�ģʽ
** �䡡��  : fd �ļ�������
** �䡡��  : ERROR_CODE
** ȫ�ֱ���:
** ����ģ��:
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
** ��������: main
** ��������: ����������
** �䡡��  : ��
** �䡡��  : ERROR_CODE
** ȫ�ֱ���:
** ����ģ��:
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

    printf("������ģʽ����ѯ:\n");
    modPoll(fd[0]);

    printf("������ģʽ���ж�:\n");
    modInt(fd[0]);

    printf("����ģʽ����ѯ:\n");
    modPoll(fd[1]);

    printf("����ģʽ���ж�:\n");
    modInt(fd[1]);

    printf("select���ܲ���:\n");
    FD_ZERO(&fdset);

    ptmvalTO.tv_sec  = 1;
    ptmvalTO.tv_usec = 0;

    for (i = 0; i < 10; i++){
        FD_SET(fd[1], &fdset);

        ret = select(fd[1] + 1, &fdset, NULL, NULL, &ptmvalTO);         /*  �ȴ�һ��                    */
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
