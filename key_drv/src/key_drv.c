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
 ** ��   ��   ��: key_drv_all2.c
 **
 ** ��   ��   ��: Liu.Jin (����)
 **
 ** �ļ���������: 2017 �� 08 �� 03 ��
 **
 ** ��        ��: KEY����ģ��
 *********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include <SylixOS.h>
#include <module.h>
#include "key_drv.h"
/*********************************************************************************************************
 ȫ�ֱ���
 *********************************************************************************************************/
static INT              __G_iKeyDrvNum   = -1;                          /*  KEY ���豸��                 */
/*********************************************************************************************************
 �궨��
 *********************************************************************************************************/
#define IMX6Q_GPIO_NUMR(bank, gpio)     (32 * (bank - 1) + (gpio))      /*  ���GPIO��                   */
#define KEY_THREAD_PRIO                 LW_PRIO_HIGH                    /*  key �������ȼ�               */
#define KEY_NAME                        "/dev/key"                      /*  key �豸����                 */
/*********************************************************************************************************
 key �豸�ṹ
 *********************************************************************************************************/
typedef struct key_dev {
    LW_DEV_HDR          KEY_devhdr;                                     /*  �豸ͷ                       */
    LW_LIST_LINE_HEADER KEY_fdNodeHeader;
    UINT                KEY_uiGpio[7];                                  /*  GPIO��                       */
    UINT                KEY_iIrqNum;                                    /*  �жϺ�                       */
    LW_OBJECT_HANDLE    KEY_msgQueue;                                   /*  ��Ϣ����                     */
    LW_SEL_WAKEUPLIST   KEY_selList;
    LW_OBJECT_HANDLE    KEY_hThread;                                    /*  KEY �߳�                     */
    BOOL                KEY_bQuit;                                      /*  �߳��˳���ʶ                 */

    time_t              KEY_timeCreate;                                 /*  �豸����ʱ��                 */
} KEY_DEV;
typedef KEY_DEV *PKEY_DEV;
/*********************************************************************************************************
** ��������: keyThread
** ��������: KEY ��ѯ����״̬�߳�
** �䡡��  : pkey       ���ƽṹ
** �䡡��  : NONE
** ȫ�ֱ���:
** ����ģ��:
*********************************************************************************************************/
static VOID  keyThread (PKEY_DEV  pkey)
{
    KEY_MSG kmsg;
    INT     i;

    for (;;) {
        if (pkey->KEY_bQuit) {
            break;
        }

        for (i = 0; i < 7; i++) {
            kmsg.KEY_num  = i + 1;
            kmsg.KEY_stat = gpioGetValue(pkey->KEY_uiGpio[i]);
            if (kmsg.KEY_stat == 0) {
                SEL_WAKE_UP_ALL(&pkey->KEY_selList, SELREAD);
            }
            API_MsgQueueSend(pkey->KEY_msgQueue,
                             &kmsg, sizeof(KEY_MSG));
        }
    }
}
/*********************************************************************************************************
 ** ��������: keyIsr
 ** ��������: KEY ���ж���Ӧ����
 ** �䡡��  : pvArg             ���ƽṹ
 **           ulVector          �ж�������
 ** �䡡��  : ERROR_CODE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
static irqreturn_t  keyIsr (PVOID  pvArg, ULONG  ulVector)
{
    KEY_MSG  kmsg;
    INT      i;
    PKEY_DEV pkey = (PKEY_DEV)pvArg;

    if (pkey == LW_NULL) {
        return  (LW_IRQ_NONE);
    }

    for (i = 0; i < 7; i++) {
        if (API_GpioSvrIrq(pkey->KEY_uiGpio[i])) {
            kmsg.KEY_num  = i+1;
            kmsg.KEY_stat = 0;
            API_GpioClearIrq(pkey->KEY_uiGpio[i]);
        }
    }

    API_MsgQueueSend2(pkey->KEY_msgQueue,
                      &kmsg, sizeof(KEY_MSG), LW_OPTION_NOT_WAIT);
    SEL_WAKE_UP_ALL(&pkey->KEY_selList, SELREAD);

    return  (LW_IRQ_HANDLED);
}
/*********************************************************************************************************
 ** ��������: keyIntDisable
 ** ��������: KEY �жϽ���
 ** �䡡��  : pkey             ���ƽṹ
 ** �䡡��  : NONE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
static VOID  keyIntDisable (PKEY_DEV  pkey)
{
    if (pkey->KEY_iIrqNum != -1) {
        API_InterVectorDisable((ULONG)pkey->KEY_iIrqNum);
        API_InterVectorDisconnect((ULONG)pkey->KEY_iIrqNum,
                                  (PINT_SVR_ROUTINE)keyIsr, (PVOID)pkey);
        pkey->KEY_iIrqNum = -1;
    }
}
/*********************************************************************************************************
 ** ��������: keyThrDisable
 ** ��������: KEY �߳��˳�
 ** �䡡��  : pkey             ���ƽṹ
 ** �䡡��  : NONE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
static VOID  keyThrDisable (PKEY_DEV  pkey)
{
    if (pkey->KEY_hThread != -1) {
        pkey->KEY_bQuit = LW_TRUE;

        API_ThreadJoin(pkey->KEY_hThread, LW_NULL);
        pkey->KEY_hThread = -1;
    }
}
/*********************************************************************************************************
 ** ��������: keyClose
 ** ��������: KEY �ر�
 ** �䡡��  : pfdentry       �ļ��ṹ
 ** �䡡��  : ERROR_CODE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
static INT  keyClose (PLW_FD_ENTRY  pfdentry)
{
    PKEY_DEV    pkey    = (PKEY_DEV)pfdentry->FDENTRY_pdevhdrHdr;
    PLW_FD_NODE pfdnode = (PLW_FD_NODE)pfdentry->FDENTRY_pfdnode;
    INT         i;

    if (pfdentry && pfdnode) {
        API_IosFdNodeDec(&pkey->KEY_fdNodeHeader, pfdnode, NULL);

        if (LW_DEV_DEC_USE_COUNT(&pkey->KEY_devhdr) == 0) {
            keyIntDisable(pkey);
            keyThrDisable(pkey);

            for (i = 0; i < 7; i++) {
                gpioFree(pkey->KEY_uiGpio[i]);
            }
            API_MsgQueueClear(pkey->KEY_msgQueue);
            API_MsgQueueDelete(&pkey->KEY_msgQueue);

            SEL_WAKE_UP_TERM(&pkey->KEY_selList);
        }
    }
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** ��������: keyRead
** ��������: ��ȡ����״̬
** �䡡��  : pfdentry    �ļ��ṹ
**           usrKeyMsg   �ش����û�����Ϣ
**           size        �ش���Ϣ�Ĵ�С
** �䡡��  : ERROR_CODE
** ȫ�ֱ���:
** ����ģ��:
*********************************************************************************************************/
static ssize_t  keyRead (PLW_FD_ENTRY  pfdentry, PKEY_MSG  usrKeyMsg, ssize_t  size)
{
    KEY_MSG kmsg;
    INT     ret;
    PKEY_DEV    pkey = (PKEY_DEV)pfdentry->FDENTRY_pdevhdrHdr;

    if (pfdentry->FDENTRY_iFlag & O_NONBLOCK) {
        ret = API_MsgQueueTryReceive(pkey->KEY_msgQueue, &kmsg,
                                     sizeof(KEY_MSG), LW_NULL);
        if (ret == ERROR_THREAD_WAIT_TIMEOUT) {
            kmsg.KEY_num  = 0;
            kmsg.KEY_stat = 1;
        } else if (ret != ERROR_NONE){
            return  (PX_ERROR);
        }
    } else {
        if (API_MsgQueueReceive(pkey->KEY_msgQueue, &kmsg,
                                sizeof(KEY_MSG), LW_NULL,
                                LW_OPTION_WAIT_INFINITE) != ERROR_NONE) {
            return  (PX_ERROR);
        }
    }
    *usrKeyMsg = kmsg;

    return  (ERROR_NONE);
}
/*********************************************************************************************************
 ** ��������: keyOpen
 ** ��������: KEY�豸��
 ** �䡡��  : pkey        ���ƽṹ
 **           pcName      key ����
 **           iFlags      ��־
 **           iMode       ģʽ
 ** �䡡��  : ERROR_CODE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
static LONG  keyOpen (PKEY_DEV  pkey, CHAR  *pcName, INT  iFlags,
                      INT  iMode)
{
    PLW_FD_NODE         pfdnode;
    BOOL                bIsNew;
    INT                 i;

    if (pcName == LW_NULL) {
        _ErrorHandle(ERROR_IO_NO_DEVICE_NAME_IN_PATH);
        printk("device is not find!\n");
        return (PX_ERROR);
    } else {
        pfdnode = API_IosFdNodeAdd(&pkey->KEY_fdNodeHeader,
                                   (dev_t)pkey, 0,iFlags,
                                   iMode, 0, 0, 0, LW_NULL, &bIsNew);
        if (pfdnode == LW_NULL) {
            printk(KERN_ERR "keyOpen() failed to add fd node!\n");
            return  (PX_ERROR);
        }

        if (LW_DEV_INC_USE_COUNT(&pkey->KEY_devhdr) == 1) {
            for (i = 0; i < 7; i++) {
                if (gpioRequestOne(pkey->KEY_uiGpio[i], LW_GPIOF_DIR_IN, "key")) {
                    LW_DEV_DEC_USE_COUNT(&pkey->KEY_devhdr);
                    API_IosFdNodeDec(&pkey->KEY_fdNodeHeader, pfdnode, NULL);
                    printk("gpio request failed!\n");
                    return  (PX_ERROR);
                }
            }

            pkey->KEY_msgQueue = API_MsgQueueCreate("q_key",7,
                                                 sizeof(KEY_MSG),
                                                 LW_OPTION_WAIT_FIFO,
                                                 LW_NULL);
            if (pkey->KEY_msgQueue == LW_OBJECT_HANDLE_INVALID) {
                __SHEAP_FREE(pkey);
                return  (PX_ERROR);
            }

            SEL_WAKE_UP_LIST_INIT(&pkey->KEY_selList);
        }
        return  ((LONG) pfdnode);
    }
}
/*********************************************************************************************************
 ** ��������: keyIntInit
 ** ��������: KEY �жϳ�ʼ��
 ** �䡡��  : pkey    ���ƽṹ
 ** �䡡��  : ERROR_CODE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
static INT  keyIntInit (PKEY_DEV  pkey)
{
    INT i, iError;

    keyThrDisable(pkey);
    API_MsgQueueClear(pkey->KEY_msgQueue);

    if (pkey->KEY_iIrqNum == -1) {
        for (i = 0; i < 7; i++) {
            API_GpioClearIrq(pkey->KEY_uiGpio[i]);
            pkey->KEY_iIrqNum = gpioSetupIrq(pkey->KEY_uiGpio[i], LW_FALSE, 0);
                                                                        /*  �½��ش���                   */
            if (pkey->KEY_iIrqNum == PX_ERROR) {
                printk("failed to setup gpio %d irq!\n", pkey->KEY_uiGpio[i]);
                return  (PX_ERROR);
            }
            iError = API_InterVectorConnect((ULONG)pkey->KEY_iIrqNum,
                                            (PINT_SVR_ROUTINE)keyIsr,
                                            (PVOID)pkey, "keyIsr");
            if (iError != ERROR_NONE) {
                printk("failed to connect GpioKey!\n");
                return  (PX_ERROR);
            }

            API_InterVectorEnable(pkey->KEY_iIrqNum);
        }
    }
    return  (ERROR_NONE);
}
/*********************************************************************************************************
 ** ��������: keyPollInit
 ** ��������: KEY ��ѯģʽ�̳߳�ʼ��
 ** �䡡��  : pkey    ���ƽṹ
 ** �䡡��  : ERROR_CODE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
static INT  keyPollInit (PKEY_DEV  pkey)
{
    LW_CLASS_THREADATTR threadAttr;

    keyIntDisable(pkey);
    API_MsgQueueClear(pkey->KEY_msgQueue);

    if (pkey->KEY_hThread == -1) {
        pkey->KEY_bQuit = LW_FALSE;

        threadAttr = API_ThreadAttrGetDefault();

        threadAttr.THREADATTR_pvArg      = (VOID *)pkey;
        threadAttr.THREADATTR_ucPriority = KEY_THREAD_PRIO;
        threadAttr.THREADATTR_ulOption  |= LW_OPTION_OBJECT_GLOBAL;

        pkey->KEY_hThread = API_ThreadCreate("t_key", (PTHREAD_START_ROUTINE)keyThread,
                                             &threadAttr, LW_NULL);
        if (!pkey->KEY_hThread) {
            return (LW_OBJECT_HANDLE_INVALID);
        }
    }
    return  (ERROR_NONE);
}
/*********************************************************************************************************
 ** ��������: keyIoctl
 ** ��������: KEY ����
 ** �䡡��  : pfdentry    ���ƽṹ
 **           iCmd        ����
 **           lArg        ����
 ** �䡡��  : ERROR_CODE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
static INT  keyIoctl (PLW_FD_ENTRY  pfdentry, INT  iCmd, LONG  lArg)
{
    struct stat       *pstat;
    PLW_SEL_WAKEUPNODE pselwunNode;
    PKEY_DEV           pkey = (PKEY_DEV)pfdentry->FDENTRY_pdevhdrHdr;

    switch (iCmd) {
    case FIOFSTATGET:
        pstat = (struct stat *)lArg;
        if (pstat) {
            pstat->st_dev     = (dev_t)pkey;
            pstat->st_ino     = (ino_t)0;
            pstat->st_mode    = (S_IRUSR | S_IRGRP | S_IROTH);
            pstat->st_nlink   = 1;
            pstat->st_uid     = 0;
            pstat->st_gid     = 0;
            pstat->st_rdev    = 0;
            pstat->st_size    = 0;
            pstat->st_blksize = 0;
            pstat->st_blocks  = 0;
            pstat->st_atime   = pkey->KEY_timeCreate;
            pstat->st_mtime   = pkey->KEY_timeCreate;
            pstat->st_ctime   = pkey->KEY_timeCreate;
            return (ERROR_NONE);
        }
        return  (PX_ERROR);
    case CMD_CTL_INT:
        keyIntInit(pkey);
        break;
    case CMD_CTL_POLL:
        keyPollInit(pkey);
        break;
    case FIOSELECT:
        pselwunNode = (PLW_SEL_WAKEUPNODE) lArg;
        SEL_WAKE_NODE_ADD(&pkey->KEY_selList, pselwunNode);
        break;
    case FIOUNSELECT:
        pselwunNode = (PLW_SEL_WAKEUPNODE) lArg;
        SEL_WAKE_NODE_DELETE(&pkey->KEY_selList, pselwunNode);
        break;
    default:
        printk("ioctl cmd :%d is not defined!\n", iCmd);
        break;
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
 ** ��������: keyLstat
 ** ��������: ��� KEY �豸״̬
 ** �䡡��  : pDev                  �豸ͷ
 **           pcName                �豸����
 **           pstat                 stat �ṹָ��
 ** �䡡��  : ERROR_CODE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
static INT  keyLstat (PLW_DEV_HDR  pDev, PCHAR  pcName, struct stat  *pstat)
{
    PKEY_DEV pkey = (PKEY_DEV)pDev;

    if (pstat) {
        pstat->st_dev     = (dev_t)pkey;
        pstat->st_ino     = (ino_t)0;
        pstat->st_mode    = (S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH);
        pstat->st_nlink   = 1;
        pstat->st_uid     = 0;
        pstat->st_gid     = 0;
        pstat->st_rdev    = 0;
        pstat->st_size    = 0;
        pstat->st_blksize = 0;
        pstat->st_blocks  = 0;
        pstat->st_atime   = pkey->KEY_timeCreate;
        pstat->st_mtime   = pkey->KEY_timeCreate;
        pstat->st_ctime   = pkey->KEY_timeCreate;

        return  (ERROR_NONE);
    }

    return  (PX_ERROR);
}
/*********************************************************************************************************
 ** ��������: keyDrv
 ** ��������: ���� KEY ��������
 ** �䡡��  : NONE
 ** �䡡��  : ERROR_CODE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
INT  keyDrv (VOID)
{
    struct file_operations fileop;

    if (__G_iKeyDrvNum > 0) {
        return (ERROR_NONE);
    }

    lib_memset(&fileop, 0, sizeof(struct file_operations));

    fileop.owner     = THIS_MODULE;
    fileop.fo_open   = keyOpen;
    fileop.fo_read   = keyRead;
    fileop.fo_ioctl  = keyIoctl;
    fileop.fo_lstat  = keyLstat;
    fileop.fo_close  = keyClose;

    __G_iKeyDrvNum = iosDrvInstallEx2(&fileop, LW_DRV_TYPE_NEW_1);

    DRIVER_LICENSE(__G_iKeyDrvNum, "Dual BSD/GPL->Ver 1.0");
    DRIVER_AUTHOR(__G_iKeyDrvNum, "Liu.Jin");
    DRIVER_DESCRIPTION(__G_iKeyDrvNum, "key driver.");

    return ((__G_iKeyDrvNum > 0) ? (ERROR_NONE) : (PX_ERROR));
}
/*********************************************************************************************************
 ** ��������: keyDevCreate
 ** ��������: ���� KEY �豸
 ** �䡡��  : cpcName         �豸��
 **           uiGpio          GPIO ���
 ** �䡡��  : ERROR_CODE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
INT  keyDevCreate (CPCHAR  cpcName)
{
    PKEY_DEV pkey;

    pkey = (PKEY_DEV)__SHEAP_ALLOC(sizeof(KEY_DEV));
    if (!pkey) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory");
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (PX_ERROR);
    }

    lib_memset(pkey, 0, sizeof(KEY_DEV));

    pkey->KEY_uiGpio[0] = IMX6Q_GPIO_NUMR(2, 4);
    pkey->KEY_uiGpio[1] = IMX6Q_GPIO_NUMR(2, 6);
    pkey->KEY_uiGpio[2] = IMX6Q_GPIO_NUMR(2, 7);
    pkey->KEY_uiGpio[3] = IMX6Q_GPIO_NUMR(6, 10);
    pkey->KEY_uiGpio[4] = IMX6Q_GPIO_NUMR(6, 15);
    pkey->KEY_uiGpio[5] = IMX6Q_GPIO_NUMR(1, 4);
    pkey->KEY_uiGpio[6] = IMX6Q_GPIO_NUMR(6, 9);

    pkey->KEY_iIrqNum    = -1;
    pkey->KEY_hThread    = -1;
    pkey->KEY_timeCreate = time(LW_NULL);

    if (iosDevAddEx(&pkey->KEY_devhdr, cpcName, __G_iKeyDrvNum,
                    DT_CHR) != ERROR_NONE) {
        __SHEAP_FREE(pkey);
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
 ** ��������: module_init
 ** ��������: ģ�鰲װ����
 ** �䡡��  : NONE
 ** �䡡��  : ERROR_CODE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
INT  module_init (VOID)
{
    keyDrv();

    keyDevCreate(KEY_NAME);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
 ** ��������: module_exit
 ** ��������: ģ���˳�����
 ** �䡡��  : NONE
 ** �䡡��  : NONE
 ** ȫ�ֱ���:
 ** ����ģ��:
 *********************************************************************************************************/
VOID  module_exit (VOID)
{
    PLW_DEV_HDR pdevHdr;

    pdevHdr = iosDevFind(KEY_NAME, LW_NULL);
    if (pdevHdr) {
        iosDevDelete(pdevHdr);
    }

    iosDrvRemove(__G_iKeyDrvNum, LW_TRUE);
}
