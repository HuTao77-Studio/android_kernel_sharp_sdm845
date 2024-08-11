/*
 *  ml630q790.c - Linux kernel modules for Sensor Hub 
 *
 *  Copyright (C) 2012-2014 LAPIS SEMICONDUCTOR CO., LTD.
 *
 *  This file is based on :
 *    ml610q792.c - Linux kernel modules for acceleration sensor
 *    http://android-dev.kyocera.co.jp/source/versionSelect_URBANO.html
 *    (C) 2012 KYOCERA Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* Version: 0x00000013 */

#ifdef NO_LINUX
#include "test/test.h"
#else
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
//#include <linux/wakelock.h>  /* SHMDS_HUB_0111_01 del */
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/uaccess.h> 
//#include <asm/gpio.h> /* SHMDS_HUB_0111_01 del */
#endif
#include "ml630q790.h"
#if 1  // SHMDS_HUB_0102_05 mod S
#include <linux/qpnp/pin.h>
#endif // SHMDS_HUB_0102_05 mod E
// SHMDS_HUB_0109_01 mod S
//#include <sharp/sh_boot_manager.h>
// SHMDS_HUB_0109_01 mod E

/* SHMDS_HUB_0901_01 SHMDS_HUB_3703_01 add S */
#if defined(CONFIG_SHARP_SHTERM) && defined(SHUB_SW_SHTERM)
#include "misc/shterm_k.h"
#endif /* CONFIG_SHARP_SHTERM && SHUB_SW_SHTERM */
/* SHMDS_HUB_0901_01 SHMDS_HUB_3703_01 add E */
#ifdef SHUB_SW_PROX_CHECKER     // SHMDS_HUB_1701_17 mod
#ifdef SHUB_SW_PROX_TYPE_API  // SHMDS_HUB_1701_19 add
#include <sharp/proximity.h>   // SHMDS_HUB_1701_16 add
#endif /* SHUB_SW_PROX_TYPE_API */  // SHMDS_HUB_1701_19 add
#endif /* SHUB_SW_PROX_CHECKER */ // SHMDS_HUB_1701_17 mod

/* SHMDS_HUB_1701_14 add S */
#include <linux/uaccess.h>
/* SHMDS_HUB_1701_14 add E */
#if defined(CONFIG_SHARP_DISPLAY) && defined(SHUB_SW_FLBL)  // SHMDS_HUB_3003_04 add
#include <sharp/flbl.h> /* SHMDS_HUB_3003_03 add */
#endif  /* CONFIG_SHARP_DISPLAY, SHUB_SW_FLBL */  // SHMDS_HUB_3003_04 add

//#undef CONFIG_ML630Q790_DEBUG
#define CONFIG_ML630Q790_DEBUG
#ifdef CONFIG_ML630Q790_DEBUG
#define SHUB_SUSPEND        // SHMDS_HUB_0102_10 mod
#define CONFIG_SET_AXIS_VAL // SHMDS_HUB_0109_02 add

static char * shub_get_active_sensor_name(int32_t type)
{
    switch(type){
        case SHUB_ACTIVE_ACC               : return "ACTIVE_ACC";
        case SHUB_ACTIVE_PEDOM             : return "ACTIVE_PEDOM";
        case SHUB_ACTIVE_PEDOM_NO_NOTIFY   : return "ACTIVE_PEDOM_NO_NOTIFY";
        case SHUB_ACTIVE_GYRO              : return "ACTIVE_GYRO";
        case SHUB_ACTIVE_MAG               : return "ACTIVE_MAG";
        case SHUB_ACTIVE_BARO              : return "ACTIVE_BARO";  /* SHMDS_HUB_0120_01 add */
        case SHUB_ACTIVE_ORI               : return "ACTIVE_ORI";
        case SHUB_ACTIVE_GRAVITY           : return "ACTIVE_GRAVITY";
        case SHUB_ACTIVE_LACC              : return "ACTIVE_LACC";
        case SHUB_ACTIVE_RV                : return "ACTIVE_RV";
        case SHUB_ACTIVE_RV_NONMAG         : return "ACTIVE_RV_NONMAG";
        case SHUB_ACTIVE_RV_NONGYRO        : return "ACTIVE_RV_NONGYRO";
        case SHUB_ACTIVE_GYROUNC           : return "ACTIVE_GYROUNC";
        case SHUB_ACTIVE_MAGUNC            : return "ACTIVE_MAGUNC";
        case SHUB_ACTIVE_PEDODEC           : return "ACTIVE_PEDODEC";
        case SHUB_ACTIVE_PEDODEC_NO_NOTIFY : return "ACTIVE_PEDODEC_NO_NOTIFY";
        case SHUB_ACTIVE_SIGNIFICANT       : return "ACTIVE_SIGNIFICANT";
        case SHUB_ACTIVE_EXT_PEDOM         : return "ACTIVE_EXT_PEDOM";
        case SHUB_ACTIVE_ERROR             : return "ACTIVE_ERROR";
        case SHUB_ACTIVE_GDEC              : return "ACTIVE_GDEC";
        case SHUB_ACTIVE_MOTIONDEC         : return "ACTIVE_MOTIONDEC";
        case SHUB_ACTIVE_PICKUP            : return "ACTIVE_PICKUP";    /* SHMDS_HUB_1701_01 add */
        case SHUB_ACTIVE_DEVICE_ORI        : return "ACTIVE_DEVICE_ORI";    /* SHMDS_HUB_0132_01 add */
#ifdef SHUB_SW_TWIST_DETECT
        case SHUB_ACTIVE_TWIST             : return "ACTIVE_TWIST";     /* SHMDS_HUB_2301_01 add */
#endif
#ifdef SHUB_SW_FREE_FALL_DETECT
        case SHUB_ACTIVE_FREE_FALL         : return "ACTIVE_FREE_FALL"; /* SHMDS_HUB_3801_02 add */
#endif

    }
    return "NONE";
}

#define DBG_LV_PEDO    0x01 // SHMDS_HUB_0701_02 add
#define DBG_LV_INT     0x02
#define DBG_LV_TIME    0x04 /* SHMDS_HUB_0312_01 add */
#define DBG_LV_CMD     0x08 /* SHMDS_HUB_0701_06 add */
#define DBG_LV_INFO    0x10
#define DBG_LV_HOSTIF  0x20
#define DBG_LV_ERROR   0x40
#define DBG_LV_DATA    0x80 
#define DBG_LV_ALL     (0xffff)
//int32_t dbg_level = DBG_LV_ERROR; // SHMDS_HUB_0104_08 mod
int dbg_level = DBG_LV_ERROR;       // SHMDS_HUB_0104_08 mod
#define DBG(lv, msg, ...) {                            \
    if( lv & dbg_level ){                              \
        printk(KERN_ERR "[shub] " msg, ##__VA_ARGS__); \
    }                                                  \
}
#else
#define DBG(lv, msg, ...)
#endif

// SHMDS_HUB_0701_01 add S
#ifdef CONFIG_ANDROID_ENGINEERING
module_param(dbg_level, int, 0600);
#endif
// SHMDS_HUB_0701_01 add E

/*
   hostinterface register
 */
#define CFG      (0x00u)
#define INTMASK0 (0x02u)
#define INTMASK1 (0x03u)
#define STATUS   (0x09u)
#define ERROR0   (0x0Au)
#define ERROR1   (0x0Bu)
#define INTREQ0  (0x0Cu)
#define INTREQ1  (0x0Du)
#define PRM0     (0x20u)
#define CMD0     (0x30u)
#define CMD1     (0x31u)
#define CMDENTRY (0x32u)
#define FIFO     (0x10u)
#define RSLT00   (0x40u)
#define RSLT01   (0x41u)
#define RSLT02   (0x42u)
#define RSLT03   (0x43u)
#define RSLT04   (0x44u)
#define RSLT05   (0x45u)
#define RSLT06   (0x46u)
#define RSLT07   (0x47u)
#define RSLT08   (0x48u)
#define RSLT09   (0x49u)
#define RSLT0A   (0x4Au)
#define RSLT0B   (0x4Bu)
#define RSLT0C   (0x4Cu)
#define RSLT0D   (0x4Du)
#define RSLT0E   (0x4Eu)
#define RSLT0F   (0x4Fu)
#define RSLT10   (0x50u)
#define RSLT11   (0x51u)
#define RSLT12   (0x52u)
#define RSLT13   (0x53u)
#define RSLT14   (0x54u)
#define RSLT15   (0x55u)
#define RSLT16   (0x56u)
#define RSLT17   (0x57u)
#define RSLT18   (0x58u)
#define RSLT19   (0x59u)
#define RSLT1A   (0x5Au)
#define RSLT1B   (0x5Bu)
#define RSLT1C   (0x5Cu)
#define RSLT1D   (0x5Du)
#define RSLT1E   (0x5Eu)
#define RSLT1F   (0x5Fu)
#define RSLT20   (0x60u)
#define RSLT21   (0x61u)
#define RSLT22   (0x62u)
#define RSLT23   (0x63u)
#define RSLT24   (0x64u)
#define RSLT25   (0x65u)
#define RSLT26   (0x66u)
#define RSLT27   (0x67u)
#define RSLT28   (0x68u)
#define RSLT29   (0x69u)
#define RSLT2A   (0x6Au)
#define RSLT2B   (0x6Bu)
#define RSLT2C   (0x6Cu)
#define RSLT2D   (0x6Du)
#define RSLT2E   (0x6Eu)
#define RSLT2F   (0x6Fu)
#define RSLT30   (0x70u)
#define RSLT31   (0x71u)
#define RSLT32   (0x72u)
#define RSLT33   (0x73u)
#define RSLT34   (0x74u)
#define RSLT35   (0x75u)
#define RSLT36   (0x76u)
#define RSLT37   (0x77u)
#define RSLT38   (0x78u)
#define RSLT39   (0x79u)
#define RSLT3A   (0x7Au)
#define RSLT3B   (0x7Bu)
#define RSLT3C   (0x7Cu)
#define RSLT3D   (0x7Du)
#define RSLT3E   (0x7Eu)
#define RSLT3F   (0x7Fu)

/*
   INTREQ TYPE
 */
#define INTREQ_NONE      (0x0000u)
#define INTREQ_HOST_CMD  (0x0001u)
#define INTREQ_ACC       (0x0002u)
#define INTREQ_BARO      (0x0040u) /* SHMDS_HUB_0120_01 add S */
#define INTREQ_MAG       (0x0080u)
#define INTREQ_GYRO      (0x0100u)
#define INTREQ_FUSION    (0x4000u)
#define INTREQ_CUSTOMER  (0x8000u)
#define INTREQ_ERROR     (0xFFFEu)
#define INTREQ_WATCHDOG  (0xFFFFu) /* SHMDS_HUB_3301_01 add */

#define INTFLG_HOST_CMD  (0x0001u) /* SHMDS_HUB_3301_01 add */
#define INTFLG_ERROR     (0x0002u) /* SHMDS_HUB_3301_01 add */
#define INTFLG_WATCHDOG  (0x0004u) /* SHMDS_HUB_3301_01 add */

/* SHMDS_HUB_0120_01 mod S */
#define INTREQ_MASK      (INTREQ_HOST_CMD | INTREQ_ACC | INTREQ_BARO | INTREQ_MAG | \
        INTREQ_GYRO | INTREQ_FUSION | INTREQ_CUSTOMER)
/* SHMDS_HUB_0120_01 mod E */

#define STATUS_IDLE      (0x00u)
#define STATUS_HOSTCMD   (0x01u)
#define STATUS_MEASURE   (0x02u)
#define STATUS_RUNAPP    (0x04u)
#define STATUS_FUSIONCAL (0x08u)
#define STATUS_INIT      (0xFEu)

/*
   INTREQ Detail
 */
#define ID_NONE                      (0x00u)
#define ID_ACC_PEDO_CNT              (0x02u)
#define ID_ACC_PEDO_MOVE             (0x04u)
#define ID_ACC_PEDO_STOP             (0x08u)
#define ID_ACC_PEDO_WALK_RUN         (0x10u)
#define ID_ACC_PEDO_PEDO_TRANS       (0x20u)
#define ID_ACC_MOVE_DETECT           (0x10u)
#define ID_ACC_XY_DETECT             (0x40u)
#define ID_ACC_Z_DETECT              (0x80u)
#define ID_ACC_PEDO_TIMER            (0x40u)
#define ID_ACC_SHAKE_GRAV            (0x02u)
#define ID_ACC_SHAKE_SNAP            (0x04u)
#define ID_ACC_SHAKE_DRAG            (0x08u)
#define ID_ACC_HW_TAP                (0x04u)
#define ID_ACC_SW_TAP                (0x80u)
#define ID_ELECT_TOUCH               (0x01u)
#define ID_ELECT_REREASE             (0x02u)
#define ID_PS_INBAG                  (0x04u)

#define ID_IDX0                      (0u)
#define ID_IDX1                      (8u)
#define ID_IDX2                      (16u)
#define ID_IDX3                      (24u)

/*
   HostCommand Number
 */
/* MCU */
#define HC_MCU_GET_VERSION           (0x0001u)
#define HC_MCU_SOFT_RESET            (0x0002u)
#define HC_MCU_GET_EX_SENSOR         (0x0003u)
#define HC_MCU_SET_PCON              (0x0004u)
#define HC_MCU_GET_PCON              (0x0005u)
#define HC_MCU_GET_INT_DETAIL        (0x0006u)
#define HC_MCU_SET_PORT_OUT          (0x0007u)
#define HC_MCU_ACCESS_SENSOR         (0x0009u)
#define HC_MCU_GET_PORT_OUT          (0x0008u)
#define HC_MCU_SELF_CHK_FW           (0x000Au)
#define HC_MCU_I2C_IO                (0x000Cu)
#define HC_MCU_SET_PERI              (0x000Du) // SHMDS_HUB_3001_01 add
#define HC_MCU_GET_PERI              (0x000Eu) // SHMDS_HUB_3001_01 add

#define HC_GET_TSK_EXECUTE           (0x0010u)
#define HC_SENSOR_TSK_SET_CYCLE      (0x0011u)
#define HC_SENSOR_TSK_GET_CYCLE      (0x0012u)
#define HC_MCU_SET_PDIR              (0x0013u) // SHMDS_HUB_2801_01 add
#define HC_MCU_GET_PDIR              (0x0014u) // SHMDS_HUB_2801_01 add
#define HC_TSK_EXECUTE               (0x000fu)
#define HC_MCU_ASSERT_INT            (0x0702u) // SHMDS_HUB_0103_01 add
#define HC_MCU_DEASSERT_INT          (0x0303u) // SHMDS_HUB_0103_01 add
#define HC_MCU_SET_INT1              (0x0304u) // SHMDS_HUB_0103_01 add

/* Firmware update */
#define HC_MCU_FUP_START             (0x0101u)
#define HC_MCU_FUP_ERASE             (0x0102u)
#define HC_MCU_FUP_WRITE             (0x0103u)
#define HC_MCU_FUP_END               (0x0104u)
#define HC_MCU_FUP_WRITE_FIFO        (0x0105u)
#define HC_MCU_FUP_SELFTEST          (0x000Au)

/* SHMDS_HUB_3001_02 add S */
/* PWM */
#define HC_MCU_SET_PWM_ENABLE       (0x0311u)
#define HC_MCU_GET_PWM_STATUS       (0x0312u)
#define HC_MCU_SET_PWM_PARAM        (0x0313u)
#define HC_MCU_GET_PWM_PARAM        (0x0314u)
#define HC_MCU_SET_BLPWM_PARAM      (0x0315u)   /* SHMDS_HUB_3003_01 add */
#define HC_MCU_GET_BLPWM_PARAM      (0x0316u)   /* SHMDS_HUB_3003_01 add */
#define HC_MCU_SET_BLPWM_PORT       (0x0317u)   /* SHMDS_HUB_3003_01 add */
#define HC_MCU_GET_BLPWM_PORT       (0x0318u)   /* SHMDS_HUB_3003_01 add */
/* SHMDS_HUB_3001_02 add E */

#define HC_ACC_SET_AUTO_MEASURE      (0x1005u)  /* SHMDS_HUB_0132_01 add */
#define HC_ACC_GET_AUTO_MEASURE      (0x1006u)  /* SHMDS_HUB_0132_01 add */

/* Accelerometer */
#define HC_ACC_SET_OFFSET            (0x1009u)
#define HC_ACC_GET_OFFSET            (0x100au)
#define HC_ACC_SET_POSITION          (0x105eu)
#define HC_ACC_GET_POSITION          (0x105fu)
#define HC_ACC_SET_OPERATION_MODE    (0x1060u)  /* SHMDS_HUB_0317_01 add */
#define HC_ACC_GET_OPERATION_MODE    (0x1061u)  /* SHMDS_HUB_0317_01 add */

/* SHMDS_HUB_0120_01 add S */
/* Barometric pressure */
#define HC_BARO_SET_OFFSET           (0x6003u)
#define HC_BARO_GET_OFFSET           (0x6004u)
/* SHMDS_HUB_0120_01 add E */
#define HC_BARO_SET_PARAM            (0x60f0u)  /* SHMDS_HUB_0120_07 add */
#define HC_BARO_GET_PARAM            (0x60f1u)  /* SHMDS_HUB_0120_07 add */

/* Magnetic */
#define HC_MAG_SET_OFFSET            (0x7005u)
#define HC_MAG_GET_OFFSET            (0x7006u)
#define HC_MAG_SET_CAL               (0x7007u)
#define HC_MAG_SET_POSITION          (0x7009u)
#define HC_MAG_GET_POSITION          (0x700au)
#define HC_MAG_SET_STATIC_MAT        (0x700du)

/* Gyro */
#define HC_GYRO_SET_OFFSET           (0x8002u)
#define HC_GYRO_GET_OFFSET           (0x8003u)
#define HC_GYRO_SET_CAL              (0x8004u)
#define HC_GYRO_SET_POSITION         (0x8009u)
#define HC_GYRO_GET_POSITION         (0x800au)
#define HC_GYRO_SET_FILTER           (0x800bu)
#define HC_GYRO_GET_FILTER           (0x800cu)
#define HC_GYRO_SET_CALIB            (0x800du) /* SHMDS_HUB_0204_16 add */
#define HC_GYRO_GET_CALIB            (0x800eu) /* SHMDS_HUB_0204_16 add */
#define HC_GYRO_SET_PARAM            (0x80f0u) /* SHMDS_HUB_2001_01 add */
#define HC_GYRO_GET_PARAM            (0x80f1u) /* SHMDS_HUB_2001_01 add */

/* G Detection */
#define HC_SET_GDETECTION_PARAM     (0x1180u)
#define HC_GET_GDETECTION_PARAM     (0x1181u)

/* SHMDS_HUB_3801_02 add S */
#define HC_FALL_SET_ENABLE          (0x1185u)
#define HC_FALL_GET_ENABLE          (0x1186u)
#define HC_FALL_GET_INFO            (0x1187u)
#define HC_FALL_GET_ACC_INFO_1      (0x1188u)
#define HC_FALL_GET_ACC_INFO_2      (0x1189u)
#define HC_FALL_REQ_RESTART         (0x118Au)
/* SHMDS_HUB_3801_02 add E */

/* SHMDS_HUB_0132_01 add S */
/* Device Orientation */
#define HC_ACC_SET_ANDROID_XY       (0x11a4u)
#define HC_ACC_GET_ANDROID_XY       (0x11a5u)
#define HC_ACC_CLEAR_ANDROID_XY     (0x11a6u)
#define HC_ACC_GET_ANDROID_XY_INFO  (0x11a7u)
/* SHMDS_HUB_0132_01 add E */

/* SHMDS_HUB_1701_01 add S */
/* Pickup */
#define HC_PICKUP_SET_ENABLE        (0x1900u)
#define HC_PICKUP_GET_ENABLE        (0x1901u)
#define HC_PICKUP_GET_ISR           (0x1902u)
#define HC_PICKUP_GET_INFO          (0x1903u)
#define HC_PICKUP_SET_PARAM1        (0x1904u)
#define HC_PICKUP_GET_PARAM1        (0x1905u)
#define HC_PICKUP_SET_PARAM2        (0x1906u)
#define HC_PICKUP_GET_PARAM2        (0x1907u)
/* SHMDS_HUB_1701_01 add E */

/* SHMDS_HUB_2301_01 add S */
#define HC_TWIST_SET_ENABLE         (0x1930u)
#define HC_TWIST_GET_ENABLE         (0x1931u)
#define HC_TWIST_GET_ISR            (0x1932u)
#define HC_TWIST_GET_INFO           (0x1933u)
/* SHMDS_HUB_2301_01 add E */

/* LowPower Mode */
#define HC_SET_LPM_PARAM (0x0306u)
#define HC_GET_LPM_PARAM (0x0307u)
#define HC_GET_LPM_INFO  (0x0308u)

/* StepCounter/StepDetector/SignificantMotion */
#define HC_SET_PEDO_STEP_PARAM             (0x1100u+0x220)
#define HC_GET_PEDO_STEP_PARAM             (0x1101u+0x220)
#define HC_SET_PEDO_CALORIE_FACTOR         (0x1104u+0x220)
#define HC_GET_PEDO_CALORIE_FACTOR         (0x1105u+0x220)
#define HC_GET_PEDO_STEP_DATA              (0x1106u+0x220)
#define HC_CLR_PEDO_STEP_DATA              (0x1107u+0x220)
#define HC_SET_PEDO_RUN_DETECT_PARAM       (0x110eu+0x220)
#define HC_GET_PEDO_RUN_DETECT_PARAM       (0x110fu+0x220)
#define HC_GET_PEDO_RUN_DETECT_DATA        (0x1110u+0x220)
#define HC_CLR_PEDO_RUN_DETECT_DATA        (0x1111u+0x220)
#define HC_SET_ACTIVITY_DETECT_PARAM       (0x1112u+0x220)
#define HC_GET_ACTIVITY_DETECT_PARAM       (0x1113u+0x220)
#define HC_GET_ACTIVITY_DETECT_DATA        (0x1116u+0x220)
#define HC_CLR_ACTIVITY_DETECT_DATA        (0x1117u+0x220)
#define HC_GET_ACTIVITY_DETECT_TIME        (0x1118u+0x220)
#define HC_CLR_ACTIVITY_DETECT_TIME        (0x1119u+0x220)
#define HC_SET_ACTIVITY_TOTAL_DETECT_PARAM (0x111au+0x220)
#define HC_GET_ACTIVITY_TOTAL_DETECT_PARAM (0x111bu+0x220)
#define HC_GET_ACTIVITY_TOTAL_DETECT_DATA  (0x111cu+0x220)
#define HC_CLR_ACTIVITY_TOTAL_DETECT_CONT  (0x111fu+0x220)
#define HC_GET_ACTIVITY_TOTAL_DETECT_INFO  (0x1120u+0x220)
#define HC_SET_PEDO_STEP_PARAM2            (0x1126u+0x220)
#define HC_GET_PEDO_STEP_PARAM2            (0x1127u+0x220)

/* Activity detection (Extension Function) */
#define HC_SET_PEDO2_STEP_PARAM             (0x1100u)
#define HC_GET_PEDO2_STEP_PARAM             (0x1101u)
#define HC_SET_PEDO2_CALORIE_FACTOR         (0x1104u)
#define HC_GET_PEDO2_CALORIE_FACTOR         (0x1105u)
#define HC_GET_PEDO2_STEP_DATA              (0x1106u)
#define HC_CLR_PEDO2_STEP_DATA              (0x1107u)
#define HC_SET_PEDO2_RUN_DETECT_PARAM       (0x110eu)
#define HC_GET_PEDO2_RUN_DETECT_PARAM       (0x110fu)
#define HC_GET_PEDO2_RUN_DETECT_DATA        (0x1110u)
#define HC_CLR_PEDO2_RUN_DETECT_DATA        (0x1111u)
#define HC_SET_ACTIVITY2_DETECT_PARAM       (0x1112u)
#define HC_GET_ACTIVITY2_DETECT_PARAM       (0x1113u)
#define HC_GET_ACTIVITY2_DETECT_DATA        (0x1116u)
#define HC_CLR_ACTIVITY2_DETECT_DATA        (0x1117u)
#define HC_GET_ACTIVITY2_DETECT_TIME        (0x1118u)
#define HC_CLR_ACTIVITY2_DETECT_TIME        (0x1119u)
#define HC_SET_ACTIVITY2_TOTAL_DETECT_PARAM (0x111au)
#define HC_GET_ACTIVITY2_TOTAL_DETECT_PARAM (0x111bu)
#define HC_GET_ACTIVITY2_TOTAL_DETECT_DATA  (0x111cu)
#define HC_CLR_ACTIVITY2_TOTAL_DETECT_CONT  (0x111fu)
#define HC_GET_ACTIVITY2_TOTAL_DETECT_INFO  (0x1120u)
#define HC_SET_ACTIVITY2_DETECT_PARAM2      (0x1121u)
#define HC_GET_ACTIVITY2_DETECT_PARAM2      (0x1122u)
#define HC_INIT_PEDO_AND_ACTIVITY_DETECT    (0x1123u)
#define HC_SET_ACTIVITY2_DETECT_PARAM3      (0x1124u)
#define HC_GET_ACTIVITY2_DETECT_PARAM3      (0x1125u)
#define HC_SET_PEDO2_STEP_PARAM2            (0x1126u)
#define HC_GET_PEDO2_STEP_PARAM2            (0x1127u)
/* SHMDS_HUB_0204_14 add S */
#define HC_SET_ACTIVITY2_PAUSE_PARAM        (0x1128u)
#define HC_GET_ACTIVITY2_PAUSE_PARAM        (0x1129u)
#define HC_SET_ACTIVITY2_PAUSE_STATUS_PARAM (0x112au)
#define HC_GET_ACTIVITY2_PAUSE_STATUS_PARAM (0x112bu)
/* SHMDS_HUB_0204_14 add E */
#define HC_CLR_ACTIVITY2_PAUSE_PARAM        (0x112cu) // SHMDS_HUB_0212_01 add
/* SHMDS_HUB_0204_15 add S */
#define HC_SET_RIDE_PEDO_PARAM               (0x112du)
#define HC_GET_RIDE_PEDO_PARAM               (0x112eu)
#define HC_SET_RIDE_PEDO2_PARAM              (0x112du+0x220)
#define HC_GET_RIDE_PEDO2_PARAM              (0x112eu+0x220)
/* SHMDS_HUB_0204_15 add E */
#define HC_ACC_PEDO_SET_ALL_STATE_2ND       (0x1133u)  /* SHMDS_HUB_2603_03 add */
#define HC_ACC_PEDO_GET_ALL_STATE_2ND       (0x1134u)  /* SHMDS_HUB_2603_03 add */

/* Logging(batch mode) */
#define HC_LOGGING_SENSOR_SET_PARAM  (0xf001u)
#define HC_LOGGING_SENSOR_GET_PARAM  (0xf002u)
#define HC_LOGGING_SENSOR_SET_CYCLE  (0xf003u)
#define HC_LOGGING_SENSOR_GET_CYCLE  (0xf004u)
#define HC_LOGGING_FUSION_SET_PARAM  (0xf005u)
#define HC_LOGGING_FUSION_GET_PARAM  (0xf006u)
#define HC_LOGGING_FUSION_SET_CYCLE  (0xf007u)
#define HC_LOGGING_FUSION_GET_CYCLE  (0xf008u)
#define HC_LOGGING_GET_RESULT        (0xf009u)
#define HC_LOGGING_DELIMITER         (0xf00au)
#define HC_SENSOR_SET_PARAM          (0xf00bu)
#define HC_SENSOR_GET_PARAM          (0xf00cu)
#define HC_SENSOR_SET_CYCLE          (0xf00du)
#define HC_SENSOR_GET_CYCLE          (0xf00eu)  /* SHMDS_HUB_3201_01 add */
#define HC_SENSOR_GET_RESULT         (0xf010u)  /* SHMDS_HUB_0120_01 add */
#define HC_LOGGING_SET_NOTIFY        (0xf01bu)
#define HC_LOGGING_GET_NOTIFY        (0xf01cu)
#define HC_LOGGING_SET_PEDO         (0xf022u)
#define HC_LOGGING_GET_PEDO         (0xf023u)
#define HC_LOGGING_SET_PEDO2        (0xf020u)
#define HC_LOGGING_GET_PEDO2        (0xf021u)
/* Fusion */
#define HC_SET_FUISON_PARAM          (0xf015u)
/* Motion Detection */
#define HC_SET_MOTDETECTION_EN             (0x1301u)
#define HC_GET_MOTDETECTION_EN             (0x1302u)
#define HC_SET_MOTDETECTION_PARAM          (0x1303u)
#define HC_GET_MOTDETECTION_PARAM          (0x1304u)
#define HC_SET_MOTDETECTION_INT            (0x1305u)
#define HC_GET_MOTDETECTION_INT            (0x1306u)
#define HC_GET_MOTDETECTION_SENS           (0x1307u)

/* SHMDS_HUB_0204_14 add S */
#define HC_SET_LOW_POWER_PARAM              (0x1310u)
#define HC_GET_LOW_POWER_PARAM              (0x1311u)
#define HC_GET_LOW_POWER_EN                 (0x1312u)  /* SHMDS_HUB_3201_01 add */
#define HC_SET_LOW_POWER_DEV_PARAM          (0x1313u)
#define HC_GET_LOW_POWER_DEV_PARAM          (0x1314u)
#define HC_CLR_LOW_POWER_MODE               (0x1315u)  /* SHMDS_HUB_0318_01 add */
/* SHMDS_HUB_0204_14 add E */

/* SHMDS_HUB_0204_15 add S */
#define HC_SET_LOW_POWER_DEV_PARAM2          (0x1316u)
#define HC_GET_LOW_POWER_DEV_PARAM2          (0x1317u)
/* SHMDS_HUB_0204_15 add E */

/*
   Exist sensor flag
 */
#define HC_INVALID                   (0x00u)
#define HC_ACC_VALID                 (0x01u)
#define HC_BARO_VALID                (0x20u)  /* SHMDS_HUB_0120_01 mod (0x08u->0x20u) */
#define HC_MAG_VALID                 (0x40u)
#define HC_GYRO_VALID                (0x80u)
#define HC_ORI_VALID                 (0x01u)
#define HC_GRAVITY_VALID             (0x02u)
#define HC_LACC_VALID                (0x04u)
#define HC_RV_VALID                  (0x08u)
#define HC_RV_NONMAG_VALID           (0x10u)
#define HC_RV_NONGYRO_VALID          (0x20u)
#define HC_ORI_MAG_VALID             (0x40u)  /* SHMDS_HUB_2902_02 */

/*
   Logging Pedometer flag
 */
#define HC_FLG_LOGGING_PEDO                   (0x0001u)
#define HC_FLG_LOGGING_TOTAL_STATUS           (0x0002u)

/*
   Task execute command parameter
 */
#define TSK_SENSOR                   (0x01)
#define TSK_APP                      (0x02)
#define TSK_FUSION                   (0x04)

/*
   Host comannd execute type
 */
#define EXE_HOST_WAIT                (1)
#define EXE_HOST_RES                 (2)
#define EXE_HOST_ERR                 (4)
#define EXE_HOST_ALL                 (EXE_HOST_WAIT|EXE_HOST_RES|EXE_HOST_ERR)
#define EXE_HOST_EX_NO_RECOVER       (16)
#define EXE_HOST_RES_RSLT            (32)
#define EXE_HOST_RES_ONLY_FIFO_SIZE  (64)
#define EXE_HOST_SKIP_MUTEX_UNLOCK   (128)
#define EXE_HOST_ALL_RSLT            (EXE_HOST_WAIT|EXE_HOST_RES_RSLT|EXE_HOST_ERR)
#define EXE_HOST_FUP_ERASE           (8)        /* SHMDS_HUB_0343_01 mod */
#define SHUB_CMD_RETRY_NUM           (3)
#define SHUB_SEND_CMD_FUP_ERASE      (0x0001u)  /* SHMDS_HUB_0343_01 mod */
#define SHUB_SEND_CMD_HOST           (0x0002u)

/*
   Firmware update command error code
 */
#define ERROR_FUP_MAXSIZE            (0x0011u)
#define ERROR_FUP_VERIFY             (0x0012u)
#define ERROR_FUP_CERTIFICATION      (0x0013u)
#define ERROR_FUP_ODDSIZE            (0x0014u)

/* SHMDS_HUB_3001_02 add S */
/* Soft PWM Error */
#define ERROR_PWM_PORT                  (0x0311u)
#define ERROR_PWM_INIT_STATE            (0x0312u)
#define ERROR_PWM_WIDTH                 (0x0313u)
#define ERROR_PWM_START                 (0x0314u)
#define ERROR_BKL_PWM_LOGIC             (0x0315u) /* SHMDS_HUB_3003_01 add S */
#define ERROR_BKL_PWM_INIT_STATE        (0x0316u) /* SHMDS_HUB_3003_01 add S */
#define ERROR_BKL_PWM_PULSE             (0x0317u) /* SHMDS_HUB_3003_01 add S */
#define ERROR_BKL_PWM_WIDTH1            (0x0318u) /* SHMDS_HUB_3003_01 add S */
#define ERROR_BKL_PWM_WIDTH2            (0x0319u) /* SHMDS_HUB_3003_01 add S */
#define ERROR_BKL_PWM_PULSE2CNT         (0x031cu) /* SHMDS_HUB_3003_02 add S */
/* SHMDS_HUB_3001_02 add E */

/*
   Driver error code
 */
#define SHUB_RC_OK                 (0)
#define SHUB_RC_ERR                (-1)
#define SHUB_RC_ERR_TIMEOUT        (-2)

/* SHMDS_HUB_3001_02 add S */

/* SHMDS_HUB_3003_02 del */

/*
   Timeout
 */
#define WAITEVENT_TIMEOUT            (2000)   /* SHMDS_HUB_0343_01 SHMDS_HUB_0348_01 mod */
#define WAIT_PEDOEVENT_TIMEOUT       (3000)
#define WAIT_CHECK_FUP_ERASE         (2000)  /* SHMDS_HUB_0343_01 mod */


/*
   Sensor Power Flag
 */
#define POWER_DISABLE       false
#define POWER_ENABLE        true

/*
   Sensor active Flag
 */
#define ACTIVE_OFF          0x00
#define ACTIVE_ON           0x01

#define ACC_SPI_RETRY_NUM    5

#define ACC_WORK_QUEUE_NUM  11 

#define SHUB_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SHUB_MAX(a, b) ((a) > (b) ? (a) : (b))

/*
   Task time 
 */
/* SHMDS_HUB_2501_01 mod S */
#define MEASURE_MAX_US            (400*1000)  /* (400000 -> 200000 -> 400000) */ /* SHMDS_HUB_0334_01 mod */
#define SENSOR_TSK_DEFALUT_US     (5000)      /* (  3750 ->   5000) */
#define FUSION_TSK_DEFALUT_US     (20000)     /* ( 15000 ->  20000) */
#define APP_TSK_DEFALUT_US        (30000)

#define SENSOR_TSK_MIN_LOGGING_US (5000)      /* (  1875 ->   5000) */
#define FUSION_TSK_MIN_US         (20000)     /* ( 15000 ->  20000) */

#define SENSOR_ACC_MIN_DELAY      (5000)      /* (  7500 ->   5000) */
/* SHMDS_HUB_2504_01 add S */
#define SENSOR_SHEX_ACC_MIN_DELAY (30000)     /* (  30000)          */
/* SHMDS_HUB_2504_01 add E */

#define SENSOR_MAG_MIN_DELAY      (20000)     /* ( 15000 ->  20000) */
#define SENSOR_MAG_MAX_DELAY      (20000)     /* (       ->  20000) */
#define SENSOR_MAGUC_MIN_DELAY    (20000)     /* ( 15000 ->  20000) */
#define SENSOR_MAGUC_MAX_DELAY    (20000)     /* (       ->  20000) */

#define SENSOR_GYRO_MIN_DELAY     (5000)      /* (  3750 ->   5000) */
#define SENSOR_GYRO_MAX_DELAY     (20000)     /* (  7500 ->  20000) */
#define SENSOR_GYROUC_MIN_DELAY   (5000)      /* (  3750 ->   5000) */
#define SENSOR_GYROUC_MAX_DELAY   (20000)     /* (  7500 ->  20000) */

#define SENSOR_BARO_MIN_DELAY     (5000)      /* (  3750 ->   5000) */ /* SHMDS_HUB_0120_01 add */
#define SENSOR_BARO_MAX_DELAY     (1000000) /* SHMDS_HUB_0120_08 add */
#define SENSOR_BARO_MID_DELAY     (200000)  /* SHMDS_HUB_0120_08 add */
#define SENSOR_BARO_LOW_DELAY     (100000)  /* SHMDS_HUB_0120_08 add */

#define FUSION_MIN_DELAY          (20000)     /* ( 15000 ->  20000) */
#define FUSION_ACC_DELAY          (5000)      /* (  7500 ->   5000) */
#define FUSION_GYRO_DELAY         (20000)     /* (  7500 ->  20000) */
#define FUSION_MAG_DELAY          (20000)     /* ( 15000 ->  20000) */
#define APP_MIN_DELAY             (30000)
#define GDEC_DEFALUT_US           (30000)
#define PICKUP_DEFALUT_US         (30000)     /* SHMDS_HUB_1701_01 add */
#define TWIST_DEFALUT_US          (30000)     /* SHMDS_HUB_2301_01 add */
#define DEVICE_ORI_DEF_US         (30000)     /* SHMDS_HUB_0132_01 add */
#define FREE_FALL_DEFALUT_US      (30000)     /* SHMDS_HUB_3801_02 add */
/* SHMDS_HUB_2501_01 mod E */

/* SHMDS_HUB_2601_01 SHMDS_HUB_2602_01 mod S */
// #define DELIMITER_LOGGING_SIZE    (458) /* xxxx - F101 */
// #define DELIMITER_LOGGING_SIZE    (454) /* F102 - F103 */
// #define DELIMITER_LOGGING_SIZE    (414) /* F104 - xxxx */
#define DELIMITER_LOGGING_SIZE    (412)    /* SHMDS_HUB_2604_01 mod */
/* SHMDS_HUB_2601_01 SHMDS_HUB_2602_01 mod E */
#define DELIMITER_TIMESTAMP_SIZE  (FIFO_SIZE - DELIMITER_LOGGING_SIZE)

#define SHUB_FW_CLOCK_THRESH      (7500)   /* SHMDS_HUB_2604_02 add */
#define SHUB_FW_CLOCK_NS_LOW      (30520)  /* SHMDS_HUB_2604_02 SHMDS_HUB_3601_01 mod */
#define SHUB_FW_CLOCK_NS_HIGH     (488352) /* SHMDS_HUB_2604_02 SHMDS_HUB_3601_01 mod */

#define LSI_ML630Q790             (0x0790)
#define LSI_ML630Q791             (0x0791)

#ifdef CONFIG_SET_AXIS_VAL
#define SHUB_ACC_AXIS_VAL           (0)
#define SHUB_MAG_AXIS_VAL           (0)
#define SHUB_GYRO_AXIS_VAL          (0)
#endif

/*
   GPIO
 */
#define USE_RESET_SIGNAL // SHMDS_HUB_0104_03 mod
#define USE_REMAP_SIGNAL // SHMDS_HUB_0104_04 mod

//#define SHUB_GPIO_INT0     (61) /* SHMDS_HUB_0104_02 del*/
#define SHUB_GPIO_INT0_NAME   "shub_hostif_int0"

#ifdef USE_RESET_SIGNAL
//#define SHUB_GPIO_RST        (120)        /* SHMDS_HUB_0104_03 del */
//#define SHUB_GPIO_RESET_NAME "shub_reset" /* SHMDS_HUB_0110_03 del */
#endif

#ifdef USE_REMAP_SIGNAL
//#define SHUB_GPIO_REMP       (120)        /* SHMDS_HUB_0104_04 del */
//#define SHUB_GPIO_REMP_NAME  "shub_remap" /* SHMDS_HUB_0110_03 del */
#endif

#define SHUB_RESET_PLUSE_WIDTH (1)

#define SHUB_RESET_TIME        (160)        /* SHMDS_HUB_0114_04 mod */
#define SHUB_RESET_RETRY_TIME        (20)   /* SHMDS_HUB_0114_04 add */

//data opecord
#define DATA_OPECORD_ACC             0x01
#define DATA_OPECORD_MAG             0x02
#define DATA_OPECORD_GYRO            0x03
#define DATA_OPECORD_MAG_CAL_OFFSET  0x04
#define DATA_OPECORD_GYRO_CAL_OFFSET 0x05
#define DATA_OPECORD_ORI             0x10
#define DATA_OPECORD_GRAVITY         0x11
#define DATA_OPECORD_LINEARACC       0x12
#define DATA_OPECORD_RVECT           0x13
#define DATA_OPECORD_GAMERV          0x14
#define DATA_OPECORD_GEORV           0x15
#define DATA_OPECORD_PEDOCNT2        0x23
#define DATA_OPECORD_TOTAL_STATUS2   0x24
#define DATA_OPECORD_PEDOCNT         0x25
#define DATA_OPECORD_TOTAL_STATUS    0x26
/* SHMDS_HUB_0120_01 add S */
#ifdef CONFIG_BARO_SENSOR                   /* SHMDS_HUB_0120_09 mod */
#define DATA_OPECORD_BARO            0x06
#endif
/* SHMDS_HUB_0120_01 add E */

#define AXIS_X (0)
#define AXIS_Y (1)
#define AXIS_Z (2)
#define INTDETAIL_GDETECT            0x00000001
#define INTDETAIL2_PEDOM_CNT         0x00000002
#define INTDETAIL2_PEDOM_STOP        0x00000008
#define INTDETAIL2_PEDOM_RUN         0x00000010
#define INTDETAIL2_PEDOM_TRANS       0x00000020
#define INTDETAIL2_PEDOM_TOTAL_STATE 0x01000000
#define INTDETAIL2_PEDOM_RIDE_PAUSE  0x02000000         // SHMDS_HUB_0209_02 add
#define INTDETAIL2_PEDOM_SIGNIFICANT 0x80000000

#define INTDETAIL_PEDOM_CNT          0x00000001
#define INTDETAIL_PEDOM_STOP         0x00000004
#define INTDETAIL_PEDOM_RUN          0x00000008
#define INTDETAIL_PEDOM_TRANS        0x00000010
#define INTDETAIL_PEDOM_TOTAL_STATE  0x00000080
#define INTDETAIL_PEDOM_SIGNIFICANT  0x00000100
#define INTDETAIL2_PEDOM_PICKUP      0x00000400         // SHMDS_HUB_1701_01 add
#define INTDETAIL2_PEDOM_DEVICE_ORI  0x00080000         /* SHMDS_HUB_0132_01 add */
#define INTDETAIL2_PEDOM_FREE_FALL1  0x00000800         // SHMDS_HUB_3801_02 add
#define INTDETAIL2_PEDOM_FREE_FALL2  0x00400000         // SHMDS_HUB_3801_02 add

/* SHMDS_HUB_2301_01 add S */
#define INT_GYRO_CALIB              0x01
#define INT_GYRO_TWIST_RIGHT        0x02            /* SHMDS_HUB_2303_01 mod */
#define INT_GYRO_TWIST_LEFT         0x04            /* SHMDS_HUB_2303_01 mod */
/* SHMDS_HUB_2301_01 add E */

#define APP_PEDOMETER               0x01
#define APP_CALORIE_FACTOR          0x02
#define APP_RUN_DETECTION           0x03
#define APP_VEICHLE_DETECTION       0x04
#define APP_TOTAL_STATUS_DETECTION  0x05
#define APP_GDETECTION              0x06
#define APP_MOTDTECTION             0x07
#define MCU_TASK_CYCLE              0x08
#define APP_TOTAL_STATUS_DETECTION_CLR_CONT_STEPS 0x09
#define APP_TOTAL_STATUS_DETECTION_CLR_CONT_STOP  0x0A
#define APP_NORMAL_PEDOMETER        0x0B
#define APP_LOW_POWER               0x0C
#define APP_VEICHLE_DETECTION2      0x0E
#define APP_CLEAR_PEDOM_AND_TOTAL_STATUS_DETECTION 0x0F
#define APP_VEICHLE_DETECTION3      0x10
#define APP_PEDOMETER2              0x11
#define APP_PEDOMETER2_2            0x12            // SHMDS_HUB_1201_02 add
#define APP_PICKUP_ENABLE           0x13            // SHMDS_HUB_1701_01 add
//#define APP_PICKUP_PARAM1           0x14            /* SHMDS_HUB_1701_01 add */ /* SHMDS_HUB_1701_06 del */
//#define APP_PICKUP_PARAM2           0x15            /* SHMDS_HUB_1701_01 add */ /* SHMDS_HUB_1701_06 del */
#define APP_PAUSE_PARAM             0x16            // SHMDS_HUB_0204_14 add
#define APP_PAUSE_STATUS_PARAM      0x17            // SHMDS_HUB_0204_14 add
#define APP_LPM_PARAM               0x18            // SHMDS_HUB_0204_14 add
#define APP_LPM_DEV_PARAM           0x19            // SHMDS_HUB_0204_14 add
#define APP_RIDE_PEDO_PARAM         0x20            // SHMDS_HUB_0204_15 add
#define APP_RIDE_PEDO2_PARAM        0x21            // SHMDS_HUB_0204_15 add
#define APP_LPM_DEV_PARAM2          0x22            // SHMDS_HUB_0204_15 add
#define APP_TOTAL_STATUS_DETECTION2 0x23            // SHMDS_HUB_2603_02 add
#define APP_CLEAR_PAUSE_PARAM       0x24            // SHMDS_HUB_0212_01 add
#define APP_TWIST_ENABLE            0x25            // SHMDS_HUB_2301_01 add
#define APP_PEDO_ALL_STATE_2ND      0x26            // SHMDS_HUB_2603_03 add
#define APP_DEVORI_ENABLE           0x27            // SHMDS_HUB_0132_01 add
#define APP_FREE_FALL_ENABLE        0x28            // SHMDS_HUB_3801_02 add

#define APP_PEDOMETER_N             (0x81)          // SHMDS_HUB_0204_02 add

/* SHMDS_HUB_1302_01 add S */
#define SHUB_FACE_CHECK_X_MIN        (-50)
#define SHUB_FACE_CHECK_X_MAX        (50)
#define SHUB_FACE_CHECK_Y_MIN        (-50)
#define SHUB_FACE_CHECK_Y_MAX        (50)
#define SHUB_FACE_CHECK_Z_MIN_DOWN   (-1100)
#define SHUB_FACE_CHECK_Z_MAX_DOWN   (-900)
#define SHUB_FACE_CHECK_Z_MIN_UP     (900)
#define SHUB_FACE_CHECK_Z_MAX_UP     (1100)
/* SHMDS_HUB_1302_01 add E */

/* SHMDS_HUB_0335_01 add S */
#define SHUB_SIZE_PEDO_STEP_PRM                 14      /* 0x1100 */
#define SHUB_SIZE_ACTIVITY2_DETECT_PARAM        14      /* 0x1112 */
#define SHUB_SIZE_ACTIVITY2_TOTAL_DETECT_PARAM  15      /* 0x111a */
#define SHUB_SIZE_ACTIVITY2_PAUSE_PARAM          3      /* 0x1128 */  /* SHMDS_HUB_3302_02 add */
#define SHUB_SIZE_GDETECTION_PARAM               9      /* 0x1180 */
#define SHUB_SIZE_MOTDETECTION_PARAM             8      /* 0x1303 */
/* SHMDS_HUB_0335_01 add E */
#define SHUB_SIZE_PWM_PARAM                      4      /* 0x0313 */
#define SHUB_SIZE_BKLPWM_PARAM                  13      /* 0x0315 */  /* SHMDS_HUB_3003_02 mod */
#define SHUB_SIZE_DEV_ORI_PARAM                  7      /* 0x11a4 */  /* SHMDS_HUB_3303_01 mod SHMDS_HUB_2605_01 mod (4->7) */

#define SHUB_PICKUP_ENABLE_ALGO_01      (0x02)      /* SHMDS_HUB_1702_01 add */
#define SHUB_PICKUP_ENABLE_PARAM_LEVEL  (0x04)      /* SHMDS_HUB_1701_15 add */
#define SHUB_PICKUP_ENABLE_ALGO_03      (0x08)      /* SHMDS_HUB_1702_01 add */
#define SHUB_PICKUP_ENABLE_PARAM_STILL  (0x10)      // SHMDS_HUB_2701_01 add

/* SHMDS_HUB_0304_02 add S */
#define SHUB_ACCESS_FW_UPDATE           (0x0001)
#define SHUB_ACCESS_RECOVERY            (0x0002)
#define SHUB_ACCESS_USER_RESET          (0x0004)

#define SHUB_FW_RESET_RETRY_NUM         (3)

#define SHUB_FW_RESET_BOOT              (0)
#define SHUB_FW_RESET_USER              (1)
/* SHMDS_HUB_0304_02 add E */

/* SHMDS_HUB_0120_10 add S */
#define SHUB_BARO_BUFFER_SIZE       (5)

#ifdef CONFIG_ANDROID_ENGINEERING
#define SHUB_BARO_FILTER_NONE       (0)
#define SHUB_BARO_FILTER_MEDIAN     (1)
static int baro_filter = SHUB_BARO_FILTER_MEDIAN;
module_param(baro_filter, int, 0600);
#endif
/* SHMDS_HUB_0120_10 add E */

///////////////////////////////////////
// union
///////////////////////////////////////
typedef union {
    uint16_t   u16;
    uint8_t    u8[2];
} Word;

typedef union {
    uint8_t    u8[PRM_SIZE];
    int8_t     s8[PRM_SIZE];
    uint16_t   u16[PRM_SIZE/2];
    int16_t    s16[PRM_SIZE/2];
    uint32_t   u32[PRM_SIZE/4];
    int32_t    s32[PRM_SIZE/4];
} HCParam;

typedef union {
    uint8_t    u8[FIFO_SIZE];
    int8_t     s8[FIFO_SIZE];
    uint16_t   u16[FIFO_SIZE/2];
    int16_t    s16[FIFO_SIZE/2];
    uint32_t   u32[FIFO_SIZE/4];
    int32_t    s32[FIFO_SIZE/4];
} HCRes;

///////////////////////////////////////
// struct
///////////////////////////////////////
typedef struct {
    Word       cmd;
    HCParam    prm;
} HostCmd;

typedef struct {
    HCRes      res;
    Word       err;
    int16_t    res_size;
} HostCmdRes;

typedef struct {
    uint32_t acc;
    uint32_t mag;
    uint32_t mag_uc;
    uint32_t gyro;
    uint32_t gyro_uc;
    uint32_t baro;  /* SHMDS_HUB_0120_01 add */
    uint32_t orien;
    uint32_t grav;
    uint32_t linear;
    uint32_t rot;
    uint32_t rot_gyro;
    uint32_t rot_mag;
    uint32_t pedocnt;
    uint32_t total_status;
    uint32_t pedocnt2;
    uint32_t total_status2;
}TotalOfDeletedTimestamp, SensorDelay;

typedef struct {
    ktime_t acc;
    ktime_t mag;
    ktime_t mag_uc;
    ktime_t gyro;
    ktime_t gyro_uc;
    ktime_t baro;  /* SHMDS_HUB_0120_01 add */
    ktime_t orien;
    ktime_t grav;
    ktime_t linear;
    ktime_t rot;
    ktime_t rot_gyro;
    ktime_t rot_mag;
    ktime_t pedocnt;
    ktime_t total_status;
    ktime_t pedocnt2;
    ktime_t total_status2;
}BaseTimestamp;

struct acceleration {
    int32_t nX;
    int32_t nY;
    int32_t nZ;
    int32_t nAccuracy;
};

struct quaternion {
    int32_t nX;
    int32_t nY;
    int32_t nZ;
    int32_t nS;
    int32_t nAccuracy;
};

struct gyroscope {
    int32_t nX;
    int32_t nY;
    int32_t nZ;
    int32_t nXOffset;
    int32_t nYOffset;
    int32_t nZOffset;
    int32_t nAccuracy;
};

struct magnetic {
    int32_t nX;
    int32_t nY;
    int32_t nZ;
    int32_t nXOffset;
    int32_t nYOffset;
    int32_t nZOffset;
    int32_t nAccuracy;
};

/* SHMDS_HUB_0120_01 add S */
struct barometric {
    uint32_t pressure;
    uint32_t buffer[SHUB_BARO_BUFFER_SIZE]; /* SHMDS_HUB_0120_10 add */
    uint8_t  pos;                           /* SHMDS_HUB_0120_10 add */
    uint8_t  num;                           /* SHMDS_HUB_0120_10 add */
};
/* SHMDS_HUB_0120_01 add E */


struct orientation {
    int32_t pitch;
    int32_t roll;
    int32_t yaw;
    int32_t nAccuracy;
};

struct stepcount {
    uint64_t step;
    uint64_t stepOffset;
    uint64_t stepDis;
};

struct micon_hostcmd_param {
    uint8_t task[3];
    uint16_t task_cycle[3]; /* SHMDS_HUB_0701_11 add */
    uint16_t sensors;
    uint32_t s_cycle[4];    /* SHMDS_HUB_0701_11 add */
    uint8_t mag_cal;
    uint8_t gyro_cal;
    uint8_t gyro_filter;
    uint8_t fusion;
    uint16_t logg_sensors;
    uint16_t logg_fusion;
    uint16_t logg_pedo;     /* SHMDS_HUB_3101_01 add */
};

/* SHMDS_HUB_3301_01 add S */
struct shub_setcmd_data {
    uint8_t     cmd_pedo2_step[ SHUB_SIZE_PEDO_STEP_PRM + 1 ];
    uint8_t     cmd_act2_det[ SHUB_SIZE_ACTIVITY2_DETECT_PARAM + 1 ];
    uint8_t     cmd_act2_tdet[ SHUB_SIZE_ACTIVITY2_TOTAL_DETECT_PARAM + 1 ];
    uint8_t     cmd_act2_pause[ SHUB_SIZE_ACTIVITY2_PAUSE_PARAM + 1 ];  /* SHMDS_HUB_3302_02 add */
    uint8_t     cmd_gdetection[ SHUB_SIZE_GDETECTION_PARAM + 1];
    uint8_t     cmd_mot_en;
    uint8_t     cmd_motdetection[ SHUB_SIZE_MOTDETECTION_PARAM + 1 ];
    uint8_t     cmd_dev_ori[ SHUB_SIZE_DEV_ORI_PARAM + 1];  /* SHMDS_HUB_3303_01 add */
};

struct shub_recovery_data {
    int32_t     bk_CurrentSensorEnable;
    int32_t     bk_CurrentLoggingSensorEnable;
    uint8_t     bk_pickup_enable;
    uint8_t     bk_pickup_setflg;
    uint8_t     bk_md_int_en;
    int         bk_md_ready_flg;
    uint64_t    bk_OffsetStep;
#ifdef CONFIG_PWM_LED
    uint8_t     bk_pwm_en;
    uint8_t     bk_pwm_param[SHUB_SIZE_PWM_PARAM];
#endif
#ifdef CONFIG_BKL_PWM_LED
    uint8_t     bk_bklpwm_en;
    uint8_t     bk_bklpwm_param[SHUB_SIZE_BKLPWM_PARAM];
    uint8_t     bk_bklpwm_port;
#endif
    struct shub_setcmd_data bk_cmd;
};
/* SHMDS_HUB_3301_01 add E */

typedef struct t_SHUB_WorkQueue {
    struct work_struct  work;
    bool                status;
} SHUB_WorkQueue;


///////////////////////////////////////
// static
///////////////////////////////////////
/* SHMDS_HUB_0350_01 mod S */
#ifdef CONFIG_ANDROID_ENGINEERING
#ifdef CONFIG_HOSTIF_I2C
static struct i2c_client *client_mcu;
#endif
#ifdef CONFIG_HOSTIF_SPI
//static struct spi_device *client_mcu; /* SHMDS_HUB_0130_01 del */
static struct device *client_mcu;       /* SHMDS_HUB_0130_01 add */
#endif
#endif
/* SHMDS_HUB_0350_01 mod E */

static SHUB_WorkQueue  s_tAccWork[ACC_WORK_QUEUE_NUM];
static int32_t g_nIntIrqFlg;
static int32_t g_nIntIrqNo;
static int32_t s_nAccWorkCnt;
static uint16_t g_hostcmdErr;
static wait_queue_head_t s_tWaitInt;

static struct workqueue_struct *accsns_wq_int;
static DEFINE_SPINLOCK(acc_lock);

static struct acceleration s_tLatestAccData;
static struct gyroscope s_tLatestGyroData;
static struct magnetic s_tLatestMagData;
static struct barometric s_tLatestBaroData;  /* SHMDS_HUB_0120_01 add */
static struct barometric s_tLoggingBaroData; /* SHMDS_HUB_0120_10 add */
static struct orientation s_tLatestOriData;
static struct acceleration s_tLatestGravityData;
static struct acceleration s_tLatestLinearAccData;
static struct quaternion s_tLatestRVectData;
static struct quaternion s_tLatestGameRVData;
static struct quaternion s_tLatestGeoRVData;
static struct stepcount s_tLatestStepCountData;

static struct micon_hostcmd_param s_micon_param;

static atomic_t g_CurrentSensorEnable;
static atomic_t g_CurrentLoggingSensorEnable;

static atomic_t g_bIsIntIrqEnable;
static atomic_t g_WakeupSensor;
static atomic_t g_FWUpdateStatus;

struct work_struct fusion_irq_work;
struct work_struct acc_irq_work;
struct work_struct significant_work;
struct work_struct gyro_irq_work;
struct work_struct mag_irq_work;
struct work_struct baro_irq_work;  /* SHMDS_HUB_0120_01 add */
struct work_struct customer_irq_work;
struct work_struct recovery_irq_work; /* SHMDS_HUB_9920_01 add */

static BaseTimestamp s_beseTime;
static BaseTimestamp s_pending_baseTime;
static SensorDelay s_sensor_delay_us;
static SensorDelay s_logging_delay_us;
static SensorDelay shub_logging_cycle; /* SHMDS_HUB_0701_11 add */
static int32_t s_sensor_task_delay_us;
static bool s_enable_notify_step;
static bool shub_connect_flg = false;
static bool shub_fw_write_flg = false;
static uint32_t s_lsi_id;
static uint32_t s_exist_sensor = 0;

/* SHMDS_HUB_0312_01 add S */
static int32_t shub_basetime_req = 0;
static ktime_t shub_baseTime;
static int32_t shub_gyro_entime_flg = 0;
static ktime_t s_gyro_enable_time_s;
static ktime_t s_gyro_enable_time_e;
/* SHMDS_HUB_0312_01 add E */

static uint8_t shub_lowpower_mode = 0;  /* SHMDS_HUB_0701_09 add */
static uint8_t shub_operation_mode = 1; /* SHMDS_HUB_0701_09 add */

static uint16_t shub_failed_init_param = 0; /* SHMDS_HUB_0319_01 add */

static uint32_t shub_err_code = SHUB_FUP_NO_COMMAND; /* SHMDS_HUB_0322_01 add */

static bool shub_mot_still_enable_flag = false; /* SHMDS_HUB_0332_01 add */

/* SHMDS_HUB_3004_01 add S */
#ifdef CONFIG_PWM_LED
static uint8_t s_pwm_param[SHUB_SIZE_PWM_PARAM] = {0};        /* SHMDS_HUB_3001_02 add */
static uint8_t shub_pwm_enable = 0;                           /* SHMDS_HUB_3001_02 add */
#endif
#ifdef CONFIG_BKL_PWM_LED
static uint8_t s_bkl_pwm_port = 0;                            /* SHMDS_HUB_3003_01 add */
static uint8_t s_bklpwm_param[SHUB_SIZE_BKLPWM_PARAM] = {0};  /* SHMDS_HUB_3003_01 add */
static uint8_t shub_bkl_pwm_enable = 0;                       /* SHMDS_HUB_3003_01 add */
#endif
/* SHMDS_HUB_3004_01 add E */

static uint8_t shub_pickup_enable = 0;                        /* SHMDS_HUB_1701_15 add */
static uint8_t shub_pickup_setflg = SHUB_PICKUP_ENABLE_PARAM; /* SHMDS_HUB_1701_15 add */

/* SHMDS_HUB_0703_01 add S */
static uint16_t s_tTimeoutCmd[2];
static uint8_t s_tTimeoutIrqEnable[2];
static uint32_t s_tTimeoutErrCnt = 0;
static u64 s_tTimeoutTimestamp[2];     /* SHMDS_HUB_0703_02 mod */

static uint8_t s_tRwErrAdr[2];
static uint16_t s_tRwErrSize[2];
static int32_t s_tRwErrCode[2];
static uint32_t s_tRwErrCnt = 0;
static u64 s_tRwErrTimestamp[2];       /* SHMDS_HUB_0703_02 mod */
/* SHMDS_HUB_0703_01 add E */

/* SHMDS_HUB_0202_01 add S */
static uint8_t oldParam9 = 0;
/* SHMDS_HUB_0202_01 add E */

static uint32_t shub_access_flg = 0;    /* SHMDS_HUB_0304_02 add */

static uint64_t shub_dbg_host_wcnt = 0; /* SHMDS_HUB_0705_01 add */
static uint64_t shub_dbg_host_rcnt = 0; /* SHMDS_HUB_0705_01 add */

static int shub_recovery_flg = 0;                 /* SHMDS_HUB_3301_01 add */
static uint32_t shub_reset_cnt = 0;               /* SHMDS_HUB_0304_02 add */
static uint32_t shub_recovery_cnt = 0;            /* SHMDS_HUB_3301_01 add */
static struct shub_setcmd_data s_setcmd_data;     /* SHMDS_HUB_3301_01 add */
static struct shub_recovery_data s_recovery_data; /* SHMDS_HUB_3301_01 add */

#ifdef SHUB_SUSPEND
static bool s_is_suspend;
#endif

/* SHMDS_HUB_0311_01 add S */
static int32_t shub_sensor_info[NUM_SHUB_SAME_NOTIFY_KIND] = {0};
int32_t shub_get_sensor_activate_info(int kind)
{
    return shub_sensor_info[kind];
}

static int32_t shub_sensor_first_measure_info[NUM_SHUB_SAME_NOTIFY_KIND] = {0};
int32_t shub_get_sensor_first_measure_info(int kind)
{
    return shub_sensor_first_measure_info[kind];
}

static int shub_sensor_same_delay_flg[NUM_SHUB_SAME_NOTIFY_KIND] = {0};
int shub_get_sensor_same_delay_flg(int kind)
{
    return shub_sensor_same_delay_flg[kind];
}
/* SHMDS_HUB_0311_01 add E */

static bool shub_acc_offset_flg = false;  /* SHMDS_HUB_0353_01 add */
static bool shub_mag_axis_flg = false;    /* SHMDS_HUB_0353_01 add */

///////////////////////////////////////
// Mutex
///////////////////////////////////////

static DEFINE_MUTEX(userReqMutex);
static DEFINE_MUTEX(s_tDataMutex);
static DEFINE_MUTEX(s_hostCmdMutex);
static DEFINE_SPINLOCK(s_intreqData);

// SHMDS_HUB_0402_01 add S
static int shub_wakelock_timeout = HZ;
#ifdef CONFIG_ANDROID_ENGINEERING
module_param(shub_wakelock_timeout, int, 0600);
#endif
static struct wake_lock shub_irq_wake_lock;
static struct wake_lock shub_int_wake_lock;
static struct wake_lock shub_acc_wake_lock;
static struct wake_lock shub_gyro_wake_lock;
static struct wake_lock shub_mag_wake_lock;
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
static struct wake_lock shub_baro_wake_lock;
static uint8_t shub_baro_odr = 0; /* SHMDS_HUB_3101_01 add */
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
static struct wake_lock shub_customer_wake_lock;
static struct wake_lock shub_fusion_wake_lock;
static struct wake_lock shub_timer_wake_lock;
static spinlock_t shub_wake_spinlock;
static bool shub_suspend_call_flg = false;
// SHMDS_HUB_0402_01 add E
static struct wake_lock shub_recovery_wake_lock; /* SHMDS_HUB_3301_01 add */

// SHMDS_HUB_1101_01 add S
#include <linux/pm_qos.h>
//#include <mach/cpuidle.h> /* SHMDS_HUB_0111_01 del */

#define SHUB_PM_QOS_LATENCY_VALUE 1
static struct pm_qos_request shub_qos_cpu_dma_latency;
static DEFINE_MUTEX(qosMutex);
static int shub_wake_lock_num = 0;
// SHMDS_HUB_1101_01 add E

/* SHMDS_HUB_0701_05 SHMDS_HUB_0353_01 mod S */
static int32_t shub_acc_offset[3] = {0};
static int32_t shub_mag_axis_interfrence[9] = {0};
/* SHMDS_HUB_0701_05 SHMDS_HUB_0353_01 mod E */

#define ENABLE_IRQ {                                                         \
    if((g_nIntIrqNo != -1) && (atomic_read(&g_bIsIntIrqEnable) == false)){   \
        atomic_set(&g_bIsIntIrqEnable,true);                                 \
        enable_irq(g_nIntIrqNo);                                             \
    }                                                                        \
}
#define DISABLE_IRQ {                                                        \
    if((g_nIntIrqNo != -1) && (atomic_read(&g_bIsIntIrqEnable) == true)){    \
        disable_irq_nosync(g_nIntIrqNo);                                     \
        atomic_set(&g_bIsIntIrqEnable,false);                                \
    }                                                                        \
}

#define ERR_WAKEUP {                                                \
    atomic_set(&g_WakeupSensor, SHUB_ACTIVE_ERROR);                 \
}

#define SUB_FW_VERSION(data) ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]))

#define U8_TO_S16(data) ((int16_t)(((uint32_t)*data ) | ((uint32_t)(*(data+1) << 8))))
#define U8_TO_U16(data) ((uint16_t)(((uint32_t)*data ) | ((uint32_t)(*(data+1) << 8))))

/* SHMDS_HUB_0120_02 add S */
#define U8_TO_U24(data) (((uint32_t)*data ) | ((uint32_t)(*(data+1) << 8))| ((uint32_t)(*(data+2) << 16)))
/* SHMDS_HUB_0120_02 add E */

#define RESU8_TO_X32(res, pos)  \
    ((res).res.u8[(pos+3)] << 24 | \
     (res).res.u8[(pos+2)] << 16 | \
     (res).res.u8[(pos+1)] << 8  | \
     (res).res.u8[(pos+0)])

#define RESU8_TO_X16(res, pos)  \
    ((res).res.u8[(pos+1)] << 8  | \
     (res).res.u8[(pos+0)])

#define RESU8_TO_X8(res, pos) (res).res.u8[(pos)]

/* SHMDS_HUB_0322_01 add S */
#define RESU8_SHUB_ERR_CODE(cmd, res, pos) ((cmd.cmd.u16 << 16) | (res.res.u8[(pos)]))
#define RESU16_SHUB_ERR_CODE(cmd, res) ((cmd.cmd.u16 << 16) | (res.err.u16))
/* SHMDS_HUB_0322_01 add E */

///////////////////////////////////////
// extern function 
///////////////////////////////////////
extern int32_t hostif_write_proc(uint8_t adr, const uint8_t *data, uint16_t size);
extern int32_t hostif_read_proc(uint8_t adr, uint8_t *data, uint16_t size);
extern int32_t initBatchingProc(void);
extern int32_t suspendBatchingProc(void);
extern int32_t resumeBatchingProc(void);

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//
// STATIC SYMBOL
//
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

// static void pending_base_time(int32_t arg_iSensType);             /* SHMDS_HUB_0312_01 del */
// static void pending_base_time(int32_t arg_iSensType, ktime_t *time); /* SHMDS_HUB_0312_01 SHMDS_HUB_2604_01 */
static void pending_base_time(int32_t arg_iSensType, ktime_t *time, uint16_t timercnt); /* SHMDS_HUB_2604_01 */
static int32_t update_base_time(int32_t arg_iSensType, ktime_t *time );
static struct timespec event_time_to_offset(int32_t arg_iSensType,int32_t eventOffsetTime);
static int32_t sensor_get_logging_data(uint8_t* buf, int32_t size);
static int32_t logging_flush_exec(void);
static int32_t shub_read_sensor_data(int32_t arg_iSensType);
static int32_t shub_hostcmd(const HostCmd *prm, HostCmdRes *res, uint8_t mode, uint8_t size);
static int32_t shub_waitcmd(uint16_t intBit);
static irqreturn_t shub_irq_handler(int32_t irq, void *dev_id);
static void shub_int_work_func(struct work_struct *work);
static void shub_int_acc_work_func(struct work_struct *work);
static void shub_int_mag_work_func(struct work_struct *work);
static void shub_int_customer_work_func(struct work_struct *work);
static void shub_int_gyro_work_func(struct work_struct *work);
static void shub_int_fusion_work_func(struct work_struct *work);
static int32_t shub_set_delay_exec(int32_t arg_iSensType, int32_t arg_iLoggingType);
static int32_t shub_activate_exec(int32_t arg_iSensType, int32_t arg_iEnable);
static int32_t shub_activate_logging_exec(int32_t arg_iSensType, int32_t arg_iEnable);
static int32_t shub_gpio_init(void);
static void shub_workqueue_init(void);
static int32_t shub_workqueue_create( struct workqueue_struct *queue, void (*func)(struct work_struct *) );
static void shub_workqueue_delete(struct work_struct *work);
static int32_t hostif_write(uint8_t adr, const uint8_t *data, uint16_t size);
static int32_t hostif_read(uint8_t adr, uint8_t *data, uint16_t size);
static int32_t shub_get_data_app_exec(int32_t arg_iType,int32_t *data);
static int32_t shub_clear_data_app_exec(int32_t type);
static int32_t shub_get_param_exec(int32_t type,int32_t *data);
static int32_t shub_set_param_exec(int32_t type,int32_t *data);
static int32_t shub_activate_significant_exec(int32_t arg_iSensType, int32_t arg_iEnable, uint8_t * notify);
static int32_t shub_activate_pedom_exec(int32_t arg_iSensType, int32_t arg_iEnable);
static int32_t shub_init_app_exec(int32_t type);
static struct timespec shub_get_timestamp(void);
#ifdef NO_LINUX
#include "test/test.c"
#endif

/* SHMDS_HUB_3801_02 add S */
#ifdef SHUB_SW_FREE_FALL_DETECT

// week info
const static const char* shub_Week[] = {
    "Sun"
   ,"Mon"
   ,"Tue"
   ,"Wed"
   ,"Thu"
   ,"Fri"
   ,"Sat"
};

// Free Fall Error Code
#define SHUB_FALL_RC_OK                 (0)
#define SHUB_FALL_RC_ERR                (-1)
#define SHUB_FALL_RC_OVER               (-2)

// Free Fall Data
#define SHUB_FALL_DATA_MAX_FILE         (200)
#define SHUB_FALL_DATA_MAX_LENGTH       (128)
#define SHUB_FALL_DATA_BUFF_MAX         (100)
#define SHUB_FALL_DATA_BEFORE_CNT       (33)
#define SHUB_FALL_DATA_AFTER_CNT        (SHUB_FALL_DATA_BUFF_MAX - SHUB_FALL_DATA_BEFORE_CNT)
#define SHUB_FALL_DATA_MEM_MAX          (24 * SHUB_FALL_DATA_BUFF_MAX + 200)
#define SHUB_FALL_DATA_FILE_PATH        "/durable/sensor/freefall/log/xx%03d.csv"

// Free Fall Hist
#define SHUB_HIST_DAT_FILE_PATH         "/durable/sensor/freefall/dat/hist.dat"
#define SHUB_HIST_TXT_FILE_PATH         "/durable/sensor/freefall/hist.txt"
#define SHUB_FALL_HIST_TXT_MEM          (1024)  // 316byte
#define SHUB_FALL_HIST_BUF_MAX          (15)
#define SHUB_FALL_HIST_CNT_MAX          (10000)
#define SHUB_FALL_DETECT_CNT_MIN        (7)
#define SHUB_FALL_DETECT_CNT_MAX        (SHUB_FALL_DETECT_CNT_MIN + SHUB_FALL_HIST_BUF_MAX - 1)

// Free Fall Event
#define SHUB_FALL_EVENT_MAX             (10000)
#define SHUB_FALL_EVENT_BUFF_MAX        (200)
#define SHUB_HIST_EVENT_FILE_PATH       "/durable/sensor/freefall/event.log"

typedef struct {
    uint16_t acc_cnt;
    int16_t  acc_buf[3][SHUB_FALL_DATA_BUFF_MAX];
} shub_fall_data;

struct shub_fall_hist {
    uint32_t h_idx;
    uint32_t h_sum[SHUB_FALL_HIST_BUF_MAX];
};

static int shub_fall_read_flag = 0;
static struct shub_fall_hist s_shub_fall_hist = {0};

#ifdef CONFIG_ANDROID_ENGINEERING
static int shub_loop_fall_cnt = 0;
module_param(shub_loop_fall_cnt, int, 0600);
#endif

//--------------------------------------------------------------------
//  shub_durable_com_read
//--------------------------------------------------------------------
static ssize_t shub_durable_com_read(struct file *fp, char *buf, size_t size)
{
    mm_segment_t old_fs;
    ssize_t res = 0;
//  loff_t fpos = offset;
    
    if( buf == NULL ){
        DBG(DBG_LV_ERROR, "%s buff null pointer \n", __func__);
        return res;
    }
    
    old_fs = get_fs();
    set_fs(get_ds());
//  res = fp->f_op->read(fp, buf, size, &fpos);
    res = __vfs_read(fp, buf, size, &fp->f_pos);
    set_fs(old_fs);
    
    return res;
}

//--------------------------------------------------------------------
//  shub_durable_com_write
//--------------------------------------------------------------------
static ssize_t shub_durable_com_write(struct file *fp, char *buf, size_t size)
{
    mm_segment_t old_fs;
    ssize_t res = 0;
    
    if( buf == NULL ){
        DBG(DBG_LV_ERROR, "%s buff null pointer \n", __func__);
        return res;
    }
    
    old_fs = get_fs();
    set_fs(get_ds());
//  res = fp->f_op->write(fp, buf, size, &fp->f_pos);
    res = __vfs_write(fp, buf, size, &fp->f_pos);
    set_fs(old_fs);
    
    return res;
}

//--------------------------------------------------------------------
//  shub_durable_read_hist_dat
//--------------------------------------------------------------------
static void shub_durable_read_hist_dat(void)
{
    struct file *fp; 
    
    if(shub_fall_read_flag != 0) {
        DBG(DBG_LV_INFO, "File open hist dat!!\n");
        return;
    }
    
    fp = filp_open(SHUB_HIST_DAT_FILE_PATH, O_RDWR, 0770);
    if (IS_ERR_OR_NULL(fp)) {
        DBG(DBG_LV_INFO, "File open Err: %s \n", SHUB_HIST_DAT_FILE_PATH);
        return;
    }
    
    shub_durable_com_read(fp, (char*)&s_shub_fall_hist, sizeof(s_shub_fall_hist));
    
//  DBG(DBG_LV_ERROR, "Free Fall hist : index=%d, tbl=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d \n", s_shub_fall_hist.h_idx, 
//                    s_shub_fall_hist.h_sum[0],s_shub_fall_hist.h_sum[1],s_shub_fall_hist.h_sum[2],s_shub_fall_hist.h_sum[3],s_shub_fall_hist.h_sum[4],
//                    s_shub_fall_hist.h_sum[5],s_shub_fall_hist.h_sum[6],s_shub_fall_hist.h_sum[7],s_shub_fall_hist.h_sum[8],s_shub_fall_hist.h_sum[9],
//                    s_shub_fall_hist.h_sum[10],s_shub_fall_hist.h_sum[11],s_shub_fall_hist.h_sum[12],s_shub_fall_hist.h_sum[13],s_shub_fall_hist.h_sum[14]);
    shub_fall_read_flag = 1;
    filp_close(fp, NULL);
}

//--------------------------------------------------------------------
//  shub_api_check_freefall_cnt
//--------------------------------------------------------------------
int shub_api_check_freefall_cnt(void)
{
    shub_durable_read_hist_dat();
    if(s_shub_fall_hist.h_idx >= SHUB_FALL_EVENT_MAX) {
        return SHUB_RC_ERR;
    }
    return SHUB_RC_OK;
}

//--------------------------------------------------------------------
//  shub_durable_write_hist_dat
//--------------------------------------------------------------------
static void shub_durable_write_hist_dat(void)
{
    struct file *fp; 
    
    fp = filp_open(SHUB_HIST_DAT_FILE_PATH, O_RDWR | O_CREAT | O_TRUNC, 0770);
    if (IS_ERR_OR_NULL(fp)) {
        DBG(DBG_LV_ERROR, "File create failed: %s \n", SHUB_HIST_DAT_FILE_PATH);
        return;
    }
    shub_durable_com_write(fp, (char*)&s_shub_fall_hist, sizeof(s_shub_fall_hist));
    
    filp_close(fp, NULL);
}

//--------------------------------------------------------------------
//  shub_durable_write_hist_txt
//--------------------------------------------------------------------
static void shub_durable_write_hist_txt(void)
{
    struct file *fp; 
    uint32_t len = 0;
    char *buf;
    uint32_t sum = 0;
    uint32_t size;
    int32_t i;
    
    size = SHUB_FALL_HIST_TXT_MEM;
    buf = (uint8_t *)kmalloc(size, GFP_KERNEL);
    if(buf == NULL){
        DBG(DBG_LV_ERROR, "error(kmalloc) : %s size=%d\n", __FUNCTION__, size);
        return;
    }
    fp = filp_open(SHUB_HIST_TXT_FILE_PATH, O_RDWR | O_CREAT | O_TRUNC, 0770);
    if (IS_ERR_OR_NULL(fp)) {
        DBG(DBG_LV_ERROR, "File create failed: %s \n", SHUB_HIST_TXT_FILE_PATH);
        kfree(buf);
        return;
    }
    for(i=0; i<SHUB_FALL_HIST_BUF_MAX; i++) {
        sum += s_shub_fall_hist.h_sum[i];
    }
//  len += snprintf((buf + len), (size - len), "No   :%7d\n", s_shub_fall_hist.h_idx);
    len += snprintf((buf + len), (size - len), "Total:%7d\n", sum);
    len += snprintf((buf + len), (size - len), "Histgram\n");
    len += snprintf((buf + len), (size - len), "----------------\n");
//  len += snprintf((buf + len), (size - len), " 21- :%7d\n", s_shub_fall_hist.h_sum[0]);
//  len += snprintf((buf + len), (size - len), " 28- :%7d\n", s_shub_fall_hist.h_sum[1]);
//  len += snprintf((buf + len), (size - len), " 35- :%7d\n", s_shub_fall_hist.h_sum[2]);
    len += snprintf((buf + len), (size - len), " 44- :%7d\n", s_shub_fall_hist.h_sum[3]);
    len += snprintf((buf + len), (size - len), " 53- :%7d\n", s_shub_fall_hist.h_sum[4]);
    len += snprintf((buf + len), (size - len), " 63- :%7d\n", s_shub_fall_hist.h_sum[5]);
    len += snprintf((buf + len), (size - len), " 74- :%7d\n", s_shub_fall_hist.h_sum[6]);
    len += snprintf((buf + len), (size - len), " 86- :%7d\n", s_shub_fall_hist.h_sum[7]);
    len += snprintf((buf + len), (size - len), " 99- :%7d\n", s_shub_fall_hist.h_sum[8]);
    len += snprintf((buf + len), (size - len), "112- :%7d\n", s_shub_fall_hist.h_sum[9]);
    len += snprintf((buf + len), (size - len), "127- :%7d\n", s_shub_fall_hist.h_sum[10]);
    len += snprintf((buf + len), (size - len), "142- :%7d\n", s_shub_fall_hist.h_sum[11]);
    len += snprintf((buf + len), (size - len), "159- :%7d\n", s_shub_fall_hist.h_sum[12]);
    len += snprintf((buf + len), (size - len), "176- :%7d\n", s_shub_fall_hist.h_sum[13]);
    len += snprintf((buf + len), (size - len), "194- :%7d\n", s_shub_fall_hist.h_sum[14]);
    len += snprintf((buf + len), (size - len), "----------------\n");
    
    shub_durable_com_write(fp, buf, len);
    kfree(buf);
    filp_close(fp, NULL);
}

//--------------------------------------------------------------------
//  shub_durable_write_hist_file
//--------------------------------------------------------------------
static void shub_durable_write_hist_file(int32_t fall_cnt)
{
    int32_t index = fall_cnt;
    
    // max check
    if(index >= SHUB_FALL_DETECT_CNT_MAX) {
        index = (SHUB_FALL_HIST_BUF_MAX - 1);
    }else{
        index -= SHUB_FALL_DETECT_CNT_MIN;
    }
    
    // count check
    if(s_shub_fall_hist.h_idx > SHUB_FALL_HIST_CNT_MAX) {
        DBG(DBG_LV_ERROR, "%s : count over!! %d\n", __FUNCTION__, (int)s_shub_fall_hist.h_idx);
        return;
    }
    s_shub_fall_hist.h_idx++;
    s_shub_fall_hist.h_sum[index]++;
    
    shub_durable_write_hist_dat();
    shub_durable_write_hist_txt();
}

//--------------------------------------------------------------------
//  shub_durable_write_fall_event
//--------------------------------------------------------------------
static void shub_durable_write_fall_event(uint32_t index, int32_t fall_cnt, const char *t_buf)
{
    struct file *fp; 
    uint32_t len = 0;
    uint32_t size;
    uint64_t h_info;
    int open_flags = (O_RDWR | O_CREAT | O_APPEND);
    char *buf;
    
    h_info = (uint64_t)((fall_cnt*3) * (fall_cnt*3));
    h_info = (uint64_t)(h_info * 4903 / 100000);
    
    // creat
    if(index == 0) {
        open_flags = (O_RDWR | O_CREAT | O_TRUNC);
    }
    // kmalloc
    size = SHUB_FALL_EVENT_BUFF_MAX;
    buf = (uint8_t *)kmalloc(size, GFP_KERNEL);
    if(buf == NULL){
        DBG(DBG_LV_ERROR, "error(kmalloc) : %s size=%d\n", __FUNCTION__, size);
        return;
    }
    // open
    fp = filp_open(SHUB_HIST_EVENT_FILE_PATH, open_flags, 0770);
    if (IS_ERR_OR_NULL(fp)) {
        DBG(DBG_LV_ERROR, "File create failed: %s \n", SHUB_HIST_EVENT_FILE_PATH);
        kfree(buf);
        return;
    }
    // set no
    len += snprintf((buf + len), (size - len), "%04d, ", (int)index);
    // set time
    len += snprintf((buf + len), (size - len), t_buf);
    // set high info
    len += snprintf((buf + len), (size - len), ", %dcm, %dms\n", (int)h_info, (int)(fall_cnt * 30));
    
    shub_durable_com_write(fp, buf, len);
    kfree(buf);
    filp_close(fp, NULL);
}

//--------------------------------------------------------------------
//  shub_durable_write_fall_data
//--------------------------------------------------------------------
static void shub_durable_write_fall_data(uint32_t index, int32_t fall_cnt, shub_fall_data *acc_info, const char *t_buf)
{
    struct file *fp; 
    uint32_t len = 0;
    uint32_t size;
    uint64_t h_info;
    int32_t i;
    char *buf;
    char f_path[80]; // 36byte
    
    h_info = (uint64_t)((fall_cnt*3) * (fall_cnt*3));
    h_info = (uint64_t)(h_info * 4903 / 100000);
    
    size = SHUB_FALL_DATA_MEM_MAX;
    buf = (uint8_t *)kmalloc(size, GFP_KERNEL);
    if(buf == NULL){
        DBG(DBG_LV_ERROR, "error(kmalloc) : %s size=%d\n", __FUNCTION__, size);
        return;
    }
    
    memset(f_path, 0x00, sizeof(f_path));
    sprintf(f_path, SHUB_FALL_DATA_FILE_PATH, (int)(index % SHUB_FALL_DATA_MAX_FILE));
    fp = filp_open(f_path, (O_RDWR | O_CREAT | O_TRUNC), 0770);
    if (IS_ERR_OR_NULL(fp)) {
        DBG(DBG_LV_ERROR, "File create failed: %s \n", f_path);
        kfree(buf);
        return;
    }
    // set no
    len += snprintf((buf + len), (size - len), "%04d, ", (int)index);
    // set time
    len += snprintf((buf + len), (size - len), t_buf);
    // set high info
    len += snprintf((buf + len), (size - len), ", %dcm, %dms\n", (int)h_info, (int)(fall_cnt * 30));
    
    // set acc info
    for(i=0; i<SHUB_FALL_DATA_BUFF_MAX; i++) {
        len += snprintf((buf + len), (size - len), "%d,%d,%d,%d\n", (int)(i+1), acc_info->acc_buf[0][i], acc_info->acc_buf[1][i], acc_info->acc_buf[2][i]);
    }
    
    shub_durable_com_write(fp, buf, len);
    kfree(buf);
    filp_close(fp, NULL);
}

//--------------------------------------------------------------------
//  shub_get_free_fall_data
//--------------------------------------------------------------------
static int32_t shub_get_free_fall_data(void)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t i;
    int32_t ret;
    int32_t fall_cnt;
    uint32_t bk_index;
    uint16_t rdata[2];
    shub_fall_data *acc_info;
    struct timeval tv;
    struct tm tm1, tm2;
    char t_buf[SHUB_FALL_DATA_MAX_LENGTH];
    
    // hist data update
    shub_durable_read_hist_dat();
    
    // get index
    bk_index = s_shub_fall_hist.h_idx;
    
    // max check
    if(bk_index >= SHUB_FALL_EVENT_MAX) {
        DBG(DBG_LV_ERROR, "%s : freefall over!! %d\n", __FUNCTION__, (int)bk_index);
        return SHUB_FALL_RC_OVER;
    }
    
    // get time
    do_gettimeofday(&tv);
    time_to_tm((time_t)tv.tv_sec, 0, &tm1);
    time_to_tm((time_t)tv.tv_sec, (sys_tz.tz_minuteswest*60*(-1)), &tm2);
    
    snprintf((char *)t_buf, SHUB_FALL_DATA_MAX_LENGTH, "%04d/%02d/%02d(%s), %02d:%02d:%02d.%03d, UTC +%02dh",
            (int)(tm2.tm_year+1900),
            tm2.tm_mon + 1,
            tm2.tm_mday,
            shub_Week[tm2.tm_wday],
            tm2.tm_hour,
            tm2.tm_min,
            tm2.tm_sec,
            (int)tv.tv_usec/1000,
            (int)((-sys_tz.tz_minuteswest) / 60));
    
    // get free_fall info
    cmd.cmd.u16 = HC_FALL_GET_INFO;
#ifdef CONFIG_ANDROID_ENGINEERING
    if(shub_loop_fall_cnt > 0) {
        fall_cnt = 18;
    }else{
#endif
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,0);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_FALL_GET_INFO err(%x)\n", res.err.u16);
        return SHUB_FALL_RC_ERR;
    }
    rdata[0] = (uint16_t)(res.res.u8[4] | (res.res.u8[5] & 0xFF) << 8);
    rdata[1] = (uint16_t)(res.res.u8[6] | (res.res.u8[7] & 0xFF) << 8);
    DBG(DBG_LV_INT, "Free Fall en=%d, stat=%d, index=%d, cnt=%d, isr1=%d, isr2=%d\n",
        res.res.u8[0],res.res.u8[1],res.res.u8[2],res.res.u8[3],rdata[0],rdata[1]);
    
    // get fall cnt
    fall_cnt = (int32_t)res.res.u8[3];
    if(fall_cnt < SHUB_FALL_DETECT_CNT_MIN) {
        DBG(DBG_LV_ERROR, "%s : fall cnt err=%d\n", __FUNCTION__, fall_cnt);
        return SHUB_FALL_RC_ERR;
    }
#ifdef CONFIG_ANDROID_ENGINEERING
    }
#endif
    
    // out hist data
    shub_durable_write_hist_file(fall_cnt);
    
    // get free_fall data
    acc_info = (shub_fall_data *)kmalloc(sizeof(shub_fall_data), GFP_KERNEL);
    if(acc_info == NULL){
        DBG(DBG_LV_ERROR, "error(kmalloc) : %s size=%d\n", __FUNCTION__, (int)sizeof(shub_fall_data));
        return SHUB_FALL_RC_ERR;
    }
    
#ifdef CONFIG_ANDROID_ENGINEERING
    if(shub_loop_fall_cnt > 0) {
        memset(acc_info, 0x11, sizeof(shub_fall_data));
    }else{
#endif
    cmd.cmd.u16 = HC_FALL_GET_ACC_INFO_1;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_FALL_GET_ACC_INFO_1 err(%x)\n", res.err.u16);
        kfree(acc_info);
        return SHUB_FALL_RC_ERR;
    }
    
    acc_info->acc_cnt = (uint16_t)RESU8_TO_X8(res,0);
    for(i=0; i<SHUB_FALL_DATA_BEFORE_CNT; i++) {
        acc_info->acc_buf[0][i] = (int16_t)RESU8_TO_X16(res,((i*6)+1));
        acc_info->acc_buf[1][i] = (int16_t)RESU8_TO_X16(res,((i*6)+3));
        acc_info->acc_buf[2][i] = (int16_t)RESU8_TO_X16(res,((i*6)+5));
    }
    
    cmd.cmd.u16 = HC_FALL_GET_ACC_INFO_2;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_FALL_GET_ACC_INFO_2 err(%x)\n", res.err.u16);
        kfree(acc_info);
        return SHUB_FALL_RC_ERR;
    }
    
    acc_info->acc_cnt = (uint16_t)RESU8_TO_X8(res,0);
    for(i=0; i<SHUB_FALL_DATA_AFTER_CNT; i++) {
        acc_info->acc_buf[0][SHUB_FALL_DATA_BEFORE_CNT+i] = (int16_t)RESU8_TO_X16(res,((i*6)+1));
        acc_info->acc_buf[1][SHUB_FALL_DATA_BEFORE_CNT+i] = (int16_t)RESU8_TO_X16(res,((i*6)+3));
        acc_info->acc_buf[2][SHUB_FALL_DATA_BEFORE_CNT+i] = (int16_t)RESU8_TO_X16(res,((i*6)+5));
    }
#ifdef CONFIG_ANDROID_ENGINEERING
    }
#endif
    
    // out event info
    shub_durable_write_fall_event(bk_index, fall_cnt, (char*)t_buf);
    
    // out acc data
    shub_durable_write_fall_data(bk_index, fall_cnt, acc_info, (char*)t_buf);
    
//  DBG(DBG_LV_ERROR, "fall point=%d\n", acc_info->acc_cnt);
//  for(i=0; i<SHUB_FALL_DATA_BUFF_MAX; i++) {
//      DBG(DBG_LV_ERROR, "[%3d] x=%d, y=%d, z=%d\n", i, acc_info->acc_buf[0][i], acc_info->acc_buf[1][i], acc_info->acc_buf[2][i]);
//  }
    
    kfree(acc_info);
    
    // count check
    if(s_shub_fall_hist.h_idx >= SHUB_FALL_EVENT_MAX) {
        DBG(DBG_LV_ERROR, "%s : count over!! %d\n", __FUNCTION__, (int)s_shub_fall_hist.h_idx);
        return SHUB_FALL_RC_OVER;
    }
    
    return SHUB_FALL_RC_OK;
}

#ifdef CONFIG_ANDROID_ENGINEERING
static int sensor_freefall_show(struct seq_file *s, void *what)
{
    int32_t i, ret;
    
    if(shub_loop_fall_cnt > 0) {
        for(i=0; i<shub_loop_fall_cnt; i++) {
            ret = shub_get_free_fall_data();
            DBG(DBG_LV_ERROR, "sensor_freefall_show : count=%d, ret=%d\n", s_shub_fall_hist.h_idx, ret);
        }
    }
    return 0;
}

static int sensor_freefall_open(struct inode *inode, struct file *file)
{
    return single_open(file, sensor_freefall_show, NULL);
}

static const struct file_operations sensor_freefall_ops = {
    .open       = sensor_freefall_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
#endif

#endif
/* SHMDS_HUB_3801_02 add E */

#ifdef SHUB_SW_PROX_CHECKER     // SHMDS_HUB_1701_19 add
#ifdef SHUB_SW_PROX_TYPE_SYSFS  // SHMDS_HUB_1701_19 add
/* SHMDS_HUB_1701_14 add S */
static int shub_ProxIndex = 0;
static int shub_ProxIndexGetFlg = 0;

#define SHUB_PROX_DEVICE_NAME       "proximity_sensor"
#define SHUB_PROX_DISABLE_ERR       "-1"

static int shub_prox_get_index(void) {
    char devname[80],name[80];
    struct file *file;
    mm_segment_t old_fs;

    if(shub_ProxIndexGetFlg  == 0){
        for(shub_ProxIndex = 0; shub_ProxIndex < 30; shub_ProxIndex++){
            memset(name, 0x00, sizeof(devname));
            memset(name, 0x00, sizeof(name));
            sprintf(devname,"/sys/devices/virtual/input/input%d/name",shub_ProxIndex);
            file = filp_open( devname, O_RDONLY, 0);
            if (IS_ERR_OR_NULL(file)) {
                printk("Cannot open file");
                continue;
            }
            old_fs = get_fs();
            set_fs(get_ds());

            if( (file->f_op->read(file, name, sizeof(name), &file->f_pos)) < 0){
                name[0] = '\0';
            }else{
                name[strlen(SHUB_PROX_DEVICE_NAME )] = '\0';
            }
            set_fs(old_fs);
            filp_close(file, NULL);
            if ( strcmp(name, SHUB_PROX_DEVICE_NAME ) == 0 )
            {
                shub_ProxIndexGetFlg = 1;
                break;
            }else{
                continue;
            }
        }
    }
    return 0;
}

static int shub_prox_distance(int *distance) {
    int ret;
    char value[4];
    char devname[80];
    struct file *file;
    mm_segment_t old_fs;

    memset(devname, 0x00, sizeof(devname));
    memset(value,0x00,sizeof(value));
    ret = shub_prox_get_index();
    sprintf(devname,"/sys/devices/virtual/input/input%d/ps_dataread",shub_ProxIndex);
    file = filp_open(devname, O_RDONLY, 0);
    if (IS_ERR_OR_NULL(file)) {
        printk("Cannot open file. err=%p \n", file);
        return -1;
    }
    old_fs = get_fs();
    set_fs(get_ds());

    ret = file->f_op->read(file, value, sizeof(value), &file->f_pos);
    set_fs(old_fs);
    filp_close(file, NULL);
    if( strcmp( value, SHUB_PROX_DISABLE_ERR ) == 0 ){
        printk("ps distance read failed = %s \n", value);
        *distance = -1;
        return -1;
    }
    *distance = simple_strtoul( value, NULL, 10 ) ;
    return ret;
}
#endif  /* SHUB_SW_PROX_TYPE_SYSFS */  // SHMDS_HUB_1701_19 add
#endif  /* SHUB_SW_PROX_CHECKER */  // SHMDS_HUB_1701_19 add
/* SHMDS_HUB_1701_14 add E */

/* SHMDS_HUB_3201_01 add S */
int shub_api_get_acc_info(struct shub_input_acc_info *info)
{
/* SHMDS_HUB_3201_02 mod S */
    int32_t xyz[5] = {0};
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);  /* SHMDS_HUB_3201_03 mod */
    
    if(info == NULL){
        DBG(DBG_LV_ERROR, "get_acc Parameter Null Error!!\n");
        return SHUB_RC_ERR;
    }
    
    memset(info, 0, sizeof(struct shub_input_acc_info));
    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "%s : FW update\n", __func__);
        return SHUB_RC_OK;
    }

/* SHMDS_HUB_0304_02 mod S */
    if(((((iCurrentSensorEnable & (SHUB_ACTIVE_ACC | SHUB_ACTIVE_DEVICE_ORI)) == 0) && (iCurrentLoggingEnable & (SHUB_ACTIVE_ACC | SHUB_ACTIVE_DEVICE_ORI)) == 0)) || s_is_suspend || shub_access_flg){  /* SHMDS_HUB_3201_03 SHMDS_HUB_0132_01 mod */
        DBG(DBG_LV_DATA, "get_acc_info(stat=0, XYZ=0, is_suspend=%d, flg=0x%x)\n", s_is_suspend, shub_access_flg);
        return SHUB_RC_OK;
    }
/* SHMDS_HUB_0304_02 mod E */
    
    shub_get_sensors_data(SHUB_ACTIVE_ACC, xyz);
    info->nX = xyz[0];
    info->nY = xyz[1];
    info->nZ = xyz[2];
    info->nStat = 1;
    if(s_micon_param.s_cycle[0] != 0){
        info->nPrd = (int32_t)(s_micon_param.s_cycle[0] / 1000);
    }
    
    DBG(DBG_LV_DATA, "get_acc_info( stat=1, X[%d] Y[%d] Z[%d] Prd[%d])\n", info->nX, info->nY ,info->nZ, info->nPrd);
/* SHMDS_HUB_3201_02 mod E */
    return SHUB_RC_OK;
}
/* SHMDS_HUB_3201_01 add E */

/* SHMDS_HUB_3001_02 add S */
int shub_api_enable_pwm(int enable)
{
#ifdef CONFIG_PWM_LED  // SHMDS_HUB_3001_03 mod
    int32_t  ret;
    HostCmd cmd;
    HostCmdRes res;

    DBG(DBG_LV_INFO, "%s( Start en=%d )\n", __func__, enable);

    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));

    if((enable != 0) && (enable != 1)) {
        DBG(DBG_LV_ERROR, "%s enable value err(%d)\n", __func__, enable);
        return SHUB_ERR_PWM_PARAM;
    }

    if((shub_pwm_enable == 1) && (enable == 1)) {
        DBG(DBG_LV_ERROR, "shub_pwm_enable start err(%d)\n", shub_pwm_enable);
        return SHUB_ERR_PWM_START;
    }

/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add S */
    if(s_is_suspend || shub_access_flg) {
        DBG(DBG_LV_ERROR, "%s Access Error!!(is_suspend=%d, flg=0x%x)\n", __func__, s_is_suspend, shub_access_flg);
        return SHUB_ERR_PWM_SUSPEND;
    }
/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add E */

    cmd.cmd.u16 = HC_MCU_SET_PWM_ENABLE;
    cmd.prm.u8[0] = (uint8_t)enable;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        if(res.err.u16 == ERROR_PWM_START) {
            DBG(DBG_LV_ERROR, "HC_MCU_SET_PWM_ENABLE START err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_ERR_PWM_STE_STA;
        }
        DBG(DBG_LV_ERROR, "HC_MCU_SET_PWM_ENABLE DEVICE err(res=0x%x,ret=%d)\n", res.err.u16, ret);
        return SHUB_ERR_PWM_DEVICE;
    }
    shub_pwm_enable = cmd.prm.u8[0];

#endif
    return SHUB_RC_OK;
}

int shub_api_set_param_pwm(struct shub_pwm_param *param)
{
#ifdef CONFIG_PWM_LED  // SHMDS_HUB_3001_03 mod
    int32_t  ret;
    HostCmd cmd;
    HostCmdRes res;

    DBG(DBG_LV_INFO, "%s(defaultStat=%d, high=%d, total=%d)\n", __func__, param->defaultStat, param->high, param->total);

    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));

    if(param == NULL) {
        DBG(DBG_LV_ERROR, "%s Parameter Null Error!!\n", __func__);
        return SHUB_ERR_PWM_PARAM;
    }

/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add S */
    if(s_is_suspend || shub_access_flg) {
        DBG(DBG_LV_ERROR, "%s Access Error!!(is_suspend=%d, flg=0x%x)\n", __func__, s_is_suspend, shub_access_flg);
        return SHUB_ERR_PWM_SUSPEND;
    }
/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add E */

    cmd.cmd.u16 = HC_MCU_GET_PWM_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PWM_PARAM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_PWM_PARAM err(res=0x%x,ret=%d)\n", res.err.u16, ret);
        return SHUB_ERR_PWM_DEVICE;
    }

    cmd.cmd.u16 = HC_MCU_SET_PWM_PARAM;
    cmd.prm.u8[0] = res.res.u8[0];
    cmd.prm.u8[1] = param->defaultStat;
    cmd.prm.u8[2] = param->high;
    cmd.prm.u8[3] = param->total;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PWM_PARAM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        if(res.err.u16 == ERROR_PWM_WIDTH) {
            DBG(DBG_LV_ERROR, "HC_MCU_SET_PWM_PARAM WIDTH err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_ERR_PWM_WIDTH;
        } else if(res.err.u16 == ERROR_PWM_INIT_STATE) {
            DBG(DBG_LV_ERROR, "HC_MCU_SET_PWM_PARAM INIT STATE err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_ERR_PWM_STE_STA;
        }
        DBG(DBG_LV_ERROR, "HC_MCU_SET_PWM_PARAM err(res=0x%x,ret=%d)\n", res.err.u16, ret);
        return SHUB_ERR_PWM_DEVICE;
    }
    memcpy(s_pwm_param, cmd.prm.u8, SHUB_SIZE_PWM_PARAM);
#endif
    return SHUB_RC_OK;
}

int shub_api_get_param_pwm(struct shub_pwm_param *param)
{
#ifdef CONFIG_PWM_LED  // SHMDS_HUB_3001_03 mod
    int32_t  ret;
    HostCmd cmd;
    HostCmdRes res;

    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));

    if(param == NULL) {
        DBG(DBG_LV_ERROR, "%s Parameter Null Error!!\n", __func__);
        return SHUB_ERR_PWM_PARAM;
    }

/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add S */
    if(s_is_suspend || shub_access_flg) {
        DBG(DBG_LV_ERROR, "%s Access Error!!(is_suspend=%d, flg=0x%x)\n", __func__, s_is_suspend, shub_access_flg);
        return SHUB_ERR_PWM_SUSPEND;
    }
/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add E */

    cmd.cmd.u16 = HC_MCU_GET_PWM_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_PWM_PARAM DEVICE err(res=0x%x,ret=%d)\n", res.err.u16, ret);
        return SHUB_ERR_PWM_DEVICE;
    }

    param->defaultStat = res.res.u8[1];
    param->high        = res.res.u8[2];
    param->total       = res.res.u8[3];
    DBG(DBG_LV_INFO, "%s(defaultStat=%d, high=%d, total=%d)\n", __func__, param->defaultStat, param->high, param->total);
#endif
    return SHUB_RC_OK;
}
/* SHMDS_HUB_3001_02 add E */

#if defined(CONFIG_SHARP_DISPLAY) && defined(SHUB_SW_FLBL)  // SHMDS_HUB_3003_04 add
/* SHMDS_HUB_3003_01 add S */
int shub_api_enable_bkl_pwm(int enable)
{
#ifdef CONFIG_BKL_PWM_LED
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    DBG(DBG_LV_INFO, "%s( Start en=%d )\n", __func__, enable);

    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));

    if((enable != 0) && (enable != 1)) {
        DBG(DBG_LV_ERROR, "%s enable value err(%d)\n", __func__, enable);
        return SHUB_ERR_PWM_PARAM;
    }

    if(shub_bkl_pwm_enable == enable) {
        DBG(DBG_LV_ERROR, "shub_bkl_pwm_enable start err(%d)\n", shub_bkl_pwm_enable);
        return SHUB_ERR_PWM_START;
    }

/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add S */
    if(s_is_suspend || shub_access_flg) {
        DBG(DBG_LV_ERROR, "%s Access Error!!(is_suspend=%d, flg=0x%x)\n", __func__, s_is_suspend, shub_access_flg);
        return SHUB_ERR_PWM_SUSPEND;
    }
/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add E */

    cmd.cmd.u16 = HC_MCU_SET_PWM_ENABLE;
    cmd.prm.u8[0] = (uint8_t)enable;

    if(cmd.prm.u8[0] == 1) {
        cmd.prm.u8[0] = 2;
    }

    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        if(res.err.u16 == ERROR_PWM_START) {
            DBG(DBG_LV_ERROR, "HC_MCU_SET_PWM_ENABLE START err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_ERR_PWM_STE_STA;
        }
        DBG(DBG_LV_ERROR, "HC_MCU_SET_PWM_ENABLE DEVICE err(res=0x%x,ret=%d)\n", res.err.u16, ret);
        return SHUB_ERR_PWM_DEVICE;
    }

    shub_bkl_pwm_enable = (uint8_t)enable;
#endif
    return SHUB_RC_OK;
}
#endif  /* CONFIG_SHARP_DISPLAY, SHUB_SW_FLBL */  // SHMDS_HUB_3003_04 add

#if defined(CONFIG_SHARP_DISPLAY) && defined(SHUB_SW_FLBL)  // SHMDS_HUB_3003_04 add
int shub_api_set_param_bkl_pwm(struct flbl_bkl_pwm_param *param)    /* SHMDS_HUB_3003_03 mod */
{
#ifdef CONFIG_BKL_PWM_LED
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));

    if(param == NULL) {
        DBG(DBG_LV_ERROR, "%s Parameter Null Error!!\n", __func__);
        return SHUB_ERR_PWM_PARAM;
    }

/* SHMDS_HUB_3003_02 mod S */
    DBG(DBG_LV_INFO, "%s(logic=%d, stat=%d, num=%d, high1=%d, total1=%d, high2=%d, total2=%d, cnt=%d)\n", 
        __func__, param->logic, param->defaultStat, param->pulseNum, param->high1, param->total1, param->high2, param->total2, param->pulse2Cnt);
/* SHMDS_HUB_3003_02 mod E */

/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add S */
    if(s_is_suspend || shub_access_flg) {
        DBG(DBG_LV_ERROR, "%s Access Error!!(is_suspend=%d, flg=0x%x)\n", __func__, s_is_suspend, shub_access_flg);
        return SHUB_ERR_PWM_SUSPEND;
    }
/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add E */

    cmd.cmd.u16 = HC_MCU_GET_BLPWM_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_BKLPWM_PARAM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_BKL_PWM_PARAM err(res=0x%x,ret=%d)\n", res.err.u16, ret);
        return SHUB_ERR_PWM_DEVICE;
    }

/* SHMDS_HUB_3003_02 add S */
    if(res.res.u8[12] == 1) { /* HTBCLK */
        param->high1  = param->high1 * 4;
        param->total1 = param->total1 * 4;
        param->high2  = param->high2 * 4;
        param->total2 = param->total2 * 4;
    } else { /* LSCLK */
        param->high1  = (param->high1 / 3 + 5) / 10;
        param->total1 = (param->total1 / 3 + 5) / 10;
        param->high2  = (param->high2 / 3 + 5) / 10;
        param->total2 = (param->total2 / 3 + 5) / 10;
    }
    DBG(DBG_LV_INFO, "%s : Set Parameter(high1=%d, total1=%d, high2=%d, total2=%d)\n", __func__, param->high1, param->total1, param->high2, param->total2);
/* SHMDS_HUB_3003_02 add E */

    cmd.cmd.u16    = HC_MCU_SET_BLPWM_PARAM;
    cmd.prm.u8[0]  = param->logic;
    cmd.prm.u8[1]  = param->defaultStat;
    cmd.prm.u8[2]  = param->pulseNum;
    cmd.prm.u8[3]  = (uint8_t)(param->high1 & 0xff);
    cmd.prm.u8[4]  = (uint8_t)((param->high1 >> 8) & 0xff);
    cmd.prm.u8[5]  = (uint8_t)(param->total1 & 0xff);
    cmd.prm.u8[6]  = (uint8_t)((param->total1 >> 8) & 0xff);
    cmd.prm.u8[7]  = (uint8_t)(param->high2 & 0xff);
    cmd.prm.u8[8]  = (uint8_t)((param->high2 >> 8) & 0xff);
    cmd.prm.u8[9]  = (uint8_t)(param->total2 & 0xff);
    cmd.prm.u8[10] = (uint8_t)((param->total2 >> 8) & 0xff);
    cmd.prm.u8[11] = param->pulse2Cnt; /* SHMDS_HUB_3003_02 add */
    cmd.prm.u8[12] = res.res.u8[12];   /* SHMDS_HUB_3003_02 add */

    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_BKLPWM_PARAM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        if((res.err.u16 == ERROR_BKL_PWM_WIDTH1) || (res.err.u16 == ERROR_BKL_PWM_WIDTH2)) {
            DBG(DBG_LV_ERROR, "HC_MCU_SET_BKL_PWM_PARAM WIDTH err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_ERR_PWM_WIDTH;
        } else if(res.err.u16 == ERROR_BKL_PWM_PULSE) {
            DBG(DBG_LV_ERROR, "HC_MCU_SET_BKL_PWM_PARAM PULSE err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_ERR_PWM_PULSE;
        } else if(res.err.u16 == ERROR_BKL_PWM_INIT_STATE) {
            DBG(DBG_LV_ERROR, "HC_MCU_SET_BKL_PWM_PARAM INIT STATE err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_ERR_PWM_STE_STA;
        } else if(res.err.u16 == ERROR_BKL_PWM_LOGIC) {
            DBG(DBG_LV_ERROR, "HC_MCU_SET_BKL_PWM_PARAM LOGIC err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_ERR_PWM_LOGIC;
/* SHMDS_HUB_3003_02 add S */
        } else if(res.err.u16 == ERROR_BKL_PWM_PULSE2CNT) {
            DBG(DBG_LV_ERROR, "HC_MCU_SET_BKL_PWM_PARAM PULSE2CNT err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_ERR_PWM_PULSE2CNT;
/* SHMDS_HUB_3003_02 add S */
        }
        DBG(DBG_LV_ERROR, "HC_MCU_SET_BKL_PWM_PARAM DEVICE err(res=0x%x,ret=%d)\n", res.err.u16, ret);
        return SHUB_ERR_PWM_DEVICE;
    }

    memcpy(s_bklpwm_param, cmd.prm.u8, SHUB_SIZE_BKLPWM_PARAM);
#endif
    return SHUB_RC_OK;
}
#endif  /* CONFIG_SHARP_DISPLAY, SHUB_SW_FLBL */  // SHMDS_HUB_3003_04 add

#if defined(CONFIG_SHARP_DISPLAY) && defined(SHUB_SW_FLBL)  // SHMDS_HUB_3003_04 add
int shub_api_get_param_bkl_pwm(struct flbl_bkl_pwm_param *param)    /* SHMDS_HUB_3003_03 mod */
{
#ifdef CONFIG_BKL_PWM_LED
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));

    if(param == NULL) {
        DBG(DBG_LV_ERROR, "%s Parameter Null Error!!\n", __func__);
        return SHUB_ERR_PWM_PARAM;
    }

/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add S */
    if(s_is_suspend || shub_access_flg) {
        DBG(DBG_LV_ERROR, "%s Access Error!!(is_suspend=%d, flg=0x%x)\n", __func__, s_is_suspend, shub_access_flg);
        return SHUB_ERR_PWM_SUSPEND;
    }
/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add E */

    cmd.cmd.u16 = HC_MCU_GET_BLPWM_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_BKLPWM_PARAM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_BKL_PWM_PARAM DEVICE err(res=0x%x,ret=%d)\n", res.err.u16, ret);
        return SHUB_ERR_PWM_DEVICE;
    }

    param->logic       = res.res.u8[0];
    param->defaultStat = res.res.u8[1];
    param->pulseNum    = res.res.u8[2];
    param->high1       = (res.res.u8[3] | (res.res.u8[4] & 0xff) << 8);
    param->total1      = (res.res.u8[5] | (res.res.u8[6] & 0xff) << 8);
    param->high2       = (res.res.u8[7] | (res.res.u8[8] & 0xff) << 8);
    param->total2      = (res.res.u8[9] | (res.res.u8[10] & 0xff) << 8);
    param->pulse2Cnt   = res.res.u8[11];  /* SHMDS_HUB_3003_02 add */

 /* SHMDS_HUB_3003_02 add S */
    DBG(DBG_LV_INFO, "%s : Get Parameter(high1=%d, total1=%d, high2=%d, total2=%d)\n", __func__, param->high1, param->total1, param->high2, param->total2);

    if(res.res.u8[12] == 1) { /* HTBCLK */
        param->high1  = param->high1 / 4;
        param->total1 = param->total1 / 4;
        param->high2  = param->high2 / 4;
        param->total2 = param->total2 / 4;
    } else { /* LSCLK */
        param->high1  = param->high1 * 30;
        param->total1 = param->total1 * 30;
        param->high2  = param->high2 * 30;
        param->total2 = param->total2 * 30;
    }
    DBG(DBG_LV_INFO, "%s(logic=%d, stat=%d, num=%d, high1=%d, total1=%d, high2=%d, total2=%d, cnt=%d)\n", 
        __func__, param->logic, param->defaultStat, param->pulseNum, param->high1, param->total1, param->high2, param->total2, param->pulse2Cnt);
 /* SHMDS_HUB_3003_02 add E */
#endif
    return SHUB_RC_OK;
}
#endif  /* CONFIG_SHARP_DISPLAY, SHUB_SW_FLBL */  // SHMDS_HUB_3003_04 add

#if defined(CONFIG_SHARP_DISPLAY) && defined(SHUB_SW_FLBL)  // SHMDS_HUB_3003_04 add
int shub_api_set_port_bkl_pwm(int port)
{
#ifdef CONFIG_BKL_PWM_LED
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    DBG(DBG_LV_INFO, "%s( port=%d )\n", __func__, port);

    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));

    if((port != 0) && (port != 1)) {
        DBG(DBG_LV_ERROR, "%s port value err(%d)\n", __func__, port);
        return SHUB_ERR_PWM_PARAM;
    }

/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add S */
    if(s_is_suspend || shub_access_flg) {
        DBG(DBG_LV_ERROR, "%s Access Error!!(is_suspend=%d, flg=0x%x)\n", __func__, s_is_suspend, shub_access_flg);
        return SHUB_ERR_PWM_SUSPEND;
    }
/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add E */

    cmd.cmd.u16 = HC_MCU_SET_BLPWM_PORT;
    cmd.prm.u8[0] = (uint8_t)port;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_MCU_SET_BKL_PWM_PORT err(res=0x%x,ret=%d)\n", res.err.u16, ret);
        return SHUB_ERR_PWM_DEVICE;
    }
    s_bkl_pwm_port = cmd.prm.u8[0];
#endif
    return SHUB_RC_OK;
}
#endif  /* CONFIG_SHARP_DISPLAY, SHUB_SW_FLBL */  // SHMDS_HUB_3003_04 add

#if defined(CONFIG_SHARP_DISPLAY) && defined(SHUB_SW_FLBL)  // SHMDS_HUB_3003_04 add
int shub_api_get_port_bkl_pwm(int *port)
{
#ifdef CONFIG_BKL_PWM_LED
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));

    if(port == NULL) {
        DBG(DBG_LV_ERROR, "%s Port Null Error!!\n", __func__);
        return SHUB_ERR_PWM_PARAM;
    }

/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add S */
    if(s_is_suspend || shub_access_flg) {
        DBG(DBG_LV_ERROR, "%s Access Error!!(is_suspend=%d, flg=0x%x)\n", __func__, s_is_suspend, shub_access_flg);
        return SHUB_ERR_PWM_SUSPEND;
    }
/* SHMDS_HUB_3002_01 SHMDS_HUB_0304_02 add E */

    cmd.cmd.u16 = HC_MCU_GET_BLPWM_PORT;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_BKL_PWM_PORT err(res=0x%x,ret=%d)\n", res.err.u16, ret);
        return SHUB_ERR_PWM_DEVICE;
    }

    *port = res.res.u8[0];
    DBG(DBG_LV_INFO, "%s(port=%d)\n", __func__, *port);
#endif
    return SHUB_RC_OK;
}
/* SHMDS_HUB_3003_01 add E */
#endif  /* CONFIG_SHARP_DISPLAY, SHUB_SW_FLBL */  // SHMDS_HUB_3003_04 add

// SHMDS_HUB_0402_01 add S
static void shub_wake_lock_init(void)
{
    spin_lock_init(&shub_wake_spinlock);
    wake_lock_init(&shub_irq_wake_lock,      WAKE_LOCK_SUSPEND, "shub_irq_wake_lock");
    wake_lock_init(&shub_int_wake_lock,      WAKE_LOCK_SUSPEND, "shub_int_wake_lock");
    wake_lock_init(&shub_acc_wake_lock,      WAKE_LOCK_SUSPEND, "shub_acc_wake_lock");
    wake_lock_init(&shub_gyro_wake_lock,     WAKE_LOCK_SUSPEND, "shub_gyro_wake_lock");
    wake_lock_init(&shub_mag_wake_lock,      WAKE_LOCK_SUSPEND, "shub_mag_wake_lock");
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    wake_lock_init(&shub_baro_wake_lock,     WAKE_LOCK_SUSPEND, "shub_baro_wake_lock");
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
    wake_lock_init(&shub_customer_wake_lock, WAKE_LOCK_SUSPEND, "shub_customer_wake_lock");
    wake_lock_init(&shub_fusion_wake_lock,   WAKE_LOCK_SUSPEND, "shub_fusion_wake_lock");
    wake_lock_init(&shub_timer_wake_lock,    WAKE_LOCK_SUSPEND, "shub_timer_wake_lock");
    wake_lock_init(&shub_recovery_wake_lock, WAKE_LOCK_SUSPEND, "shub_recovery_wake_lock"); /* SHMDS_HUB_3301_01 add */
    return;
}

static void shub_wake_lock_destroy(void)
{
/* SHMDS_HUB_0402_02 del S */
//    unsigned long flags;

//    spin_lock_irqsave(&shub_wake_spinlock, flags);
/* SHMDS_HUB_0402_02 del E */

    if (wake_lock_active(&shub_irq_wake_lock)){
        wake_unlock(&shub_irq_wake_lock);
    }
    if (wake_lock_active(&shub_int_wake_lock)){
        wake_unlock(&shub_int_wake_lock);
    }
    if (wake_lock_active(&shub_acc_wake_lock)){
        wake_unlock(&shub_acc_wake_lock);
    }
    if (wake_lock_active(&shub_gyro_wake_lock)){
        wake_unlock(&shub_gyro_wake_lock);
    }
    if (wake_lock_active(&shub_mag_wake_lock)){
        wake_unlock(&shub_mag_wake_lock);
    }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    if (wake_lock_active(&shub_baro_wake_lock)){
        wake_unlock(&shub_baro_wake_lock);
    }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
    if (wake_lock_active(&shub_customer_wake_lock)){
        wake_unlock(&shub_customer_wake_lock);
    }
    if (wake_lock_active(&shub_fusion_wake_lock)){
        wake_unlock(&shub_fusion_wake_lock);
    }
    if (wake_lock_active(&shub_timer_wake_lock)){
        wake_unlock(&shub_timer_wake_lock);
    }
/* SHMDS_HUB_3301_01 add S */
    if (wake_lock_active(&shub_recovery_wake_lock)){
        wake_unlock(&shub_recovery_wake_lock);
    }
/* SHMDS_HUB_3301_01 add E */

    wake_lock_destroy(&shub_irq_wake_lock);
    wake_lock_destroy(&shub_int_wake_lock);
    wake_lock_destroy(&shub_acc_wake_lock);
    wake_lock_destroy(&shub_gyro_wake_lock);
    wake_lock_destroy(&shub_mag_wake_lock);
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    wake_lock_destroy(&shub_baro_wake_lock);
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
    wake_lock_destroy(&shub_customer_wake_lock);
    wake_lock_destroy(&shub_fusion_wake_lock);
    wake_lock_destroy(&shub_timer_wake_lock);
    wake_lock_destroy(&shub_recovery_wake_lock); /* SHMDS_HUB_3301_01 add */

/* SHMDS_HUB_0402_02 del S */
//    spin_unlock_irqrestore(&shub_wake_spinlock, flags);
/* SHMDS_HUB_0402_02 del E */
    return;
}

static void shub_wake_lock_start(struct wake_lock *wl)
{
    unsigned long flags;

    spin_lock_irqsave(&shub_wake_spinlock, flags);


    if(!shub_suspend_call_flg
      || (strcmp(wl->ws.name,"shub_irq_wake_lock")
      &&  strcmp(wl->ws.name,"shub_int_wake_lock"))){
        if (!wake_lock_active(wl)){
            wake_lock(wl);
        }
    }

    spin_unlock_irqrestore(&shub_wake_spinlock, flags);

    return;
}

static void shub_wake_lock_end(struct wake_lock *wl)
{
    unsigned long flags;

    spin_lock_irqsave(&shub_wake_spinlock, flags);

    if(!shub_suspend_call_flg
      || (strcmp(wl->ws.name,"shub_irq_wake_lock")
      &&  strcmp(wl->ws.name,"shub_int_wake_lock"))){
        if (wake_lock_active(wl)){
            wake_unlock(wl);
        }
    }

    spin_unlock_irqrestore(&shub_wake_spinlock, flags);

    return;
}

static void shub_timer_wake_lock_start(void)
{
    unsigned long flags;

    spin_lock_irqsave(&shub_wake_spinlock, flags);

    if (wake_lock_active(&shub_timer_wake_lock)){
        wake_unlock(&shub_timer_wake_lock);
    }
    wake_lock_timeout(&shub_timer_wake_lock, shub_wakelock_timeout);

    spin_unlock_irqrestore(&shub_wake_spinlock, flags);

    return;
}
// SHMDS_HUB_0402_01 add E

// SHMDS_HUB_1101_01 add S
static void shub_qos_init(void)
{
    shub_wake_lock_num = 0;
    mutex_init(&qosMutex);
//  shub_qos_cpu_dma_latency.type = PM_QOS_REQ_AFFINE_CORES;                    /* SHMDS_HUB_1102_03 del */
//  shub_qos_cpu_dma_latency.cpus_affine.bits[0] = 0x0f; /* little cluster */   /* SHMDS_HUB_1102_03 del */
    pm_qos_add_request(&shub_qos_cpu_dma_latency, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
    return;
}

static void shub_qos_destroy(void)
{
    shub_wake_lock_num = 0;
    pm_qos_remove_request(&shub_qos_cpu_dma_latency);
    return;
}

void shub_qos_start(void)
{

    mutex_lock(&qosMutex);
    if (!shub_wake_lock_num) {
        pm_qos_update_request(&shub_qos_cpu_dma_latency, SHUB_PM_QOS_LATENCY_VALUE);
    }
    shub_wake_lock_num++;
    mutex_unlock(&qosMutex);

    return;
}

void shub_qos_end(void)
{

    mutex_lock(&qosMutex);
    shub_wake_lock_num--;
    if (!shub_wake_lock_num) {
        pm_qos_update_request(&shub_qos_cpu_dma_latency, PM_QOS_DEFAULT_VALUE);
    }
    if (shub_wake_lock_num < 0) {
        shub_wake_lock_num = 0;
    }
    mutex_unlock(&qosMutex);

    return;
}
// SHMDS_HUB_1101_01 add E

/* SHMDS_HUB_0703_01 add S */
static void collect_hostcmd_timeout_log(uint16_t cmd)
{
    if(s_tTimeoutErrCnt == 0){
        s_tTimeoutTimestamp[0] = local_clock(); /* SHMDS_HUB_0703_02 mod */
        s_tTimeoutCmd[0] = cmd;
        s_tTimeoutIrqEnable[0] = atomic_read(&g_bIsIntIrqEnable);
    }else{
        s_tTimeoutTimestamp[1] = local_clock(); /* SHMDS_HUB_0703_02 mod */
        s_tTimeoutCmd[1] = cmd;
        s_tTimeoutIrqEnable[1] = atomic_read(&g_bIsIntIrqEnable);
    }
    if(s_tTimeoutErrCnt < 0xFFFFFFFF){
        s_tTimeoutErrCnt++;
    }
}

static void collect_hosif_err_log(uint8_t adr, uint16_t size, int32_t ret)
{
    if(s_tRwErrCnt == 0){
        s_tRwErrTimestamp[0] = local_clock();   /* SHMDS_HUB_0703_02 mod */
        s_tRwErrAdr[0] = adr;
        s_tRwErrSize[0] = size;
        s_tRwErrCode[0] = ret;
    }else{
        s_tRwErrTimestamp[1] = local_clock();   /* SHMDS_HUB_0703_02 mod */
        s_tRwErrAdr[1] = adr;
        s_tRwErrSize[1] = size;
        s_tRwErrCode[1] = ret; 
    }
    if(s_tRwErrCnt < 0xFFFFFFFF){
        s_tRwErrCnt++;
    }
}
/* SHMDS_HUB_0703_01 add E */

/* SHMDS_HUB_0703_02 add E */
static void shub_print_time(struct seq_file *s, u64 ts)
{
    unsigned long rem_nsec;
    
    rem_nsec = do_div(ts, 1000000000);
    seq_printf(s, "time=%lu.%06lu", (unsigned long)ts, rem_nsec / 1000);
}
/* SHMDS_HUB_0703_02 add E */

// SHMDS_HUB_0701_03 add S
static uint64_t shub_dbg_cnt_irq;
static uint64_t shub_dbg_cnt_cmd;
static uint64_t shub_dbg_cnt_acc;
static uint64_t shub_dbg_cnt_baro;  /* SHMDS_HUB_0120_01 add */
static uint64_t shub_dbg_cnt_mag;
static uint64_t shub_dbg_cnt_gyro;
static uint64_t shub_dbg_cnt_fusion;
static uint64_t shub_dbg_cnt_cust;
static uint64_t shub_dbg_cnt_other;
static uint64_t shub_dbg_cnt_none;  /* SHMDS_HUB_0704_01 add */
static uint64_t shub_dbg_cnt_err;   /* SHMDS_HUB_3301_01 add */

static void shub_dbg_collect_irq_log(uint16_t intreq)
{
/* SHMDS_HUB_3301_01 add S */
    if((intreq == INTREQ_ERROR) ||(intreq == INTREQ_WATCHDOG)) {
        shub_dbg_cnt_err++;
        return;
    }
/* SHMDS_HUB_3301_01 add E */
    if(intreq & INTREQ_HOST_CMD) {
        shub_dbg_cnt_cmd++;
    }
    if(intreq & INTREQ_ACC) {
        shub_dbg_cnt_acc++;
    }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
//#ifdef CONFIG_BARO_SENSOR     /* SHMDS_HUB_2603_01 del */
    if(intreq & INTREQ_BARO) {
        shub_dbg_cnt_baro++;
    }
//#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
    if(intreq & INTREQ_GYRO) {
        shub_dbg_cnt_gyro++;
    }
    if(intreq & INTREQ_MAG) {
        shub_dbg_cnt_mag++;
    }
    if(intreq & INTREQ_CUSTOMER) {
        shub_dbg_cnt_cust++;
    }
    if(intreq & INTREQ_FUSION) {
        shub_dbg_cnt_fusion++;
    }
    if(intreq & ~INTREQ_MASK) {
        shub_dbg_cnt_other++;
    }
/* SHMDS_HUB_0704_01 add S */
    if(intreq == 0) {
        shub_dbg_cnt_none++;
    }
/* SHMDS_HUB_0704_01 add E */
}

static void shub_dbg_clr_irq_log(void)
{
    shub_dbg_cnt_irq = 0;
    shub_dbg_cnt_cmd = 0;
    shub_dbg_cnt_acc = 0;
    shub_dbg_cnt_baro = 0;  /* SHMDS_HUB_0120_01 add */
    shub_dbg_cnt_mag = 0;
    shub_dbg_cnt_gyro = 0;
    shub_dbg_cnt_fusion = 0;
    shub_dbg_cnt_cust = 0;
    shub_dbg_cnt_other = 0;
    shub_dbg_cnt_none = 0;  /* SHMDS_HUB_0704_01 add */
    shub_dbg_cnt_err = 0;   /* SHMDS_HUB_3301_01 add */
}

static void shub_dbg_out_irq_log(void)
{
/* SHMDS_HUB_0120_01 SHMDS_HUB_0122_01 SHMDS_HUB_0704_01 SHMDS_HUB_3301_01 S */
    printk("[shub][dbg] irq=%lld,cmd=%lld,a=%lld,b=%lld,m=%lld,g=%lld,f=%lld,c=%lld,o=%lld,no=%lld,err=%lld,en=%08x(%08x)\n",
            shub_dbg_cnt_irq, shub_dbg_cnt_cmd, shub_dbg_cnt_acc, shub_dbg_cnt_baro, shub_dbg_cnt_mag,
            shub_dbg_cnt_gyro, shub_dbg_cnt_fusion, shub_dbg_cnt_cust, shub_dbg_cnt_other, shub_dbg_cnt_none, shub_dbg_cnt_err,
            atomic_read(&g_CurrentSensorEnable), atomic_read(&g_CurrentLoggingSensorEnable));
/* SHMDS_HUB_0120_01 SHMDS_HUB_0122_01 SHMDS_HUB_0704_01 SHMDS_HUB_3301_01 E */
}
// SHMDS_HUB_0701_03 add E

/* SHMDS_HUB_3301_01 add S */
void shub_sensor_timer_start(void)
{
    shub_resume_acc();
    shub_resume_mag();
    shub_resume_mag_uncal();
    shub_resume_gyro();
    shub_resume_gyro_uncal();
    shub_resume_orien();
    shub_resume_grav();
    shub_resume_linear();
    shub_resume_rot();
    shub_resume_rot_gyro();
    shub_resume_rot_mag();
    shub_resume_exif();         // SHMDS_HUB_0203_01 add
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    shub_resume_baro();
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
}

void shub_sensor_timer_stop(void)
{
    shub_suspend_acc();
    shub_suspend_mag();
    shub_suspend_mag_uncal();
    shub_suspend_gyro();
    shub_suspend_gyro_uncal();
    shub_suspend_orien();
    shub_suspend_grav();
    shub_suspend_linear();
    shub_suspend_rot();
    shub_suspend_rot_gyro();
    shub_suspend_rot_mag();
    shub_suspend_exif();        // SHMDS_HUB_0203_01 add
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    shub_suspend_baro();
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
}

void shub_recovery_backup(void)
{
    /* common info */
    s_recovery_data.bk_CurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    s_recovery_data.bk_CurrentLoggingSensorEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    /* pickup */
    s_recovery_data.bk_pickup_enable = shub_pickup_enable;
    s_recovery_data.bk_pickup_setflg = shub_pickup_setflg;
    /* pedo */
    s_recovery_data.bk_OffsetStep = s_tLatestStepCountData.step;
    /* motion detection */
    s_recovery_data.bk_md_int_en = oldParam9;
    s_recovery_data.bk_md_ready_flg = shub_get_already_md_flg();
#ifdef CONFIG_PWM_LED
    /* pwm led */
    s_recovery_data.bk_pwm_en = shub_pwm_enable;
    memcpy(s_recovery_data.bk_pwm_param, s_pwm_param, SHUB_SIZE_PWM_PARAM);
#endif
#ifdef CONFIG_BKL_PWM_LED
    /* backlight pwm led */
    s_recovery_data.bk_bklpwm_en = shub_bkl_pwm_enable;
    memcpy(s_recovery_data.bk_bklpwm_param, s_bklpwm_param, SHUB_SIZE_BKLPWM_PARAM);
    s_recovery_data.bk_bklpwm_port = s_bkl_pwm_port;
#endif
    /* cmd */
    memcpy(&s_recovery_data.bk_cmd, &s_setcmd_data, sizeof(s_recovery_data.bk_cmd));
}

void shub_recovery_restore_first(void)
{
    atomic_set(&g_CurrentSensorEnable, s_recovery_data.bk_CurrentSensorEnable);
    atomic_set(&g_CurrentLoggingSensorEnable, s_recovery_data.bk_CurrentLoggingSensorEnable);
}

void shub_recovery_restore_second(void)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    
    /* pedo */
    if(iCurrentSensorEnable & SHUB_ACTIVE_EXT_PEDOM){
        memcpy(cmd.prm.u8, s_recovery_data.bk_cmd.cmd_pedo2_step, SHUB_SIZE_PEDO_STEP_PRM);
        cmd.cmd.u16 = HC_SET_PEDO2_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : HC_SET_PEDO2_STEP_PARAM err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
        }
    }
    /* G detection */
    if(iCurrentSensorEnable & SHUB_ACTIVE_GDEC){
        memcpy(cmd.prm.u8, s_recovery_data.bk_cmd.cmd_gdetection, SHUB_SIZE_GDETECTION_PARAM);
        cmd.cmd.u16 = HC_SET_GDETECTION_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_GDETECTION_PARAM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : HC_SET_GDETECTION_PARAM err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
        }
    }
    /* motion detection */
    shub_set_already_md_flg(s_recovery_data.bk_md_ready_flg);
    if(iCurrentSensorEnable & SHUB_ACTIVE_MOTIONDEC){
        cmd.prm.u8[0x00] = s_recovery_data.bk_cmd.cmd_mot_en;
        cmd.cmd.u16 = HC_SET_MOTDETECTION_EN;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : HC_SET_MOTDETECTION_EN err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
        }
        memcpy(cmd.prm.u8, s_recovery_data.bk_cmd.cmd_motdetection, SHUB_SIZE_MOTDETECTION_PARAM);
        cmd.cmd.u16 = HC_SET_MOTDETECTION_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_MOTDETECTION_PARAM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : HC_SET_MOTDETECTION_PARAM err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
        }
        cmd.prm.u8[0x00] = s_recovery_data.bk_md_int_en;
        cmd.cmd.u16 = HC_SET_MOTDETECTION_INT;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : HC_SET_MOTDETECTION_INT err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
        }
        oldParam9 = s_recovery_data.bk_md_int_en & 0x03;    /* SHMDS_HUB_3302_03 add */
    }
    /* pickup */
    shub_pickup_enable = s_recovery_data.bk_pickup_enable;
    shub_pickup_setflg = s_recovery_data.bk_pickup_setflg;
    if(iCurrentSensorEnable & SHUB_ACTIVE_PICKUP){
        cmd.prm.u8[0x00] = shub_pickup_enable;
        cmd.prm.u8[0x01] = shub_pickup_setflg;
        cmd.cmd.u16 = HC_PICKUP_SET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : HC_PICKUP_SET_ENABLE err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
        }
    }
/* SHMDS_HUB_3801_02 add S */
#ifdef SHUB_SW_FREE_FALL_DETECT
    /* free_fall */
    if(iCurrentSensorEnable & SHUB_ACTIVE_FREE_FALL){
        cmd.prm.u8[0x00] = 1;
        cmd.cmd.u16 = HC_FALL_SET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : HC_FALL_SET_ENABLE err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
        }
    }
#endif
/* SHMDS_HUB_3801_02 add E */
#ifdef SHUB_SW_TWIST_DETECT
    /* twist */
    if(iCurrentSensorEnable & SHUB_ACTIVE_TWIST){
        cmd.prm.u8[0x00] = 1;
        cmd.cmd.u16 = HC_TWIST_SET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : HC_TWIST_SET_ENABLE err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
        }
    }
#endif
    
    memcpy(cmd.prm.u8, s_recovery_data.bk_cmd.cmd_act2_det, SHUB_SIZE_ACTIVITY2_DETECT_PARAM);
    cmd.cmd.u16 = HC_SET_ACTIVITY2_DETECT_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_ACTIVITY2_DETECT_PARAM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "%s : HC_SET_ACTIVITY2_DETECT_PARAM err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
    }
    memcpy(s_setcmd_data.cmd_act2_det, cmd.prm.u8, SHUB_SIZE_ACTIVITY2_DETECT_PARAM);        /* SHMDS_HUB_3302_01 add */
    
    memcpy(cmd.prm.u8, s_recovery_data.bk_cmd.cmd_act2_tdet, SHUB_SIZE_ACTIVITY2_TOTAL_DETECT_PARAM);
    cmd.cmd.u16 = HC_SET_ACTIVITY2_TOTAL_DETECT_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_ACTIVITY2_TOTAL_DETECT_PARAM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "%s : HC_SET_ACTIVITY2_TOTAL_DETECT_PARAM err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
    }
    memcpy(s_setcmd_data.cmd_act2_tdet, cmd.prm.u8, SHUB_SIZE_ACTIVITY2_TOTAL_DETECT_PARAM); /* SHMDS_HUB_3302_01 add */

/* SHMDS_HUB_3302_02 add S */
    memcpy(cmd.prm.u8, s_recovery_data.bk_cmd.cmd_act2_pause, SHUB_SIZE_ACTIVITY2_PAUSE_PARAM);
    cmd.cmd.u16 = HC_SET_ACTIVITY2_PAUSE_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_ACTIVITY2_PAUSE_PARAM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "%s : HC_SET_ACTIVITY2_PAUSE_PARAM err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
    }
    memcpy(s_setcmd_data.cmd_act2_pause, cmd.prm.u8, SHUB_SIZE_ACTIVITY2_PAUSE_PARAM);
/* SHMDS_HUB_3302_02 add E */

    /* pwm enable */
#if defined (CONFIG_PWM_LED) || defined (CONFIG_BKL_PWM_LED)
#ifdef CONFIG_BKL_PWM_LED
    if(s_recovery_data.bk_bklpwm_en == 1) {
        cmd.prm.u8[0] = 2;
    }else
#endif
#ifdef CONFIG_PWM_LED
    if(s_recovery_data.bk_pwm_en == 1) {
        cmd.prm.u8[0] = 1;
    }else
#endif
    {
        cmd.prm.u8[0] = 0;
    }
    cmd.cmd.u16 = HC_MCU_SET_PWM_ENABLE;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "%s : HC_MCU_SET_PWM_ENABLE err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
    }
#ifdef CONFIG_BKL_PWM_LED
    if(s_recovery_data.bk_bklpwm_en == 1) {
        shub_bkl_pwm_enable = s_recovery_data.bk_bklpwm_en;
    }
#endif
#ifdef CONFIG_PWM_LED
    if(s_recovery_data.bk_pwm_en == 1) {
        shub_pwm_enable = s_recovery_data.bk_pwm_en;
    }
#endif
#endif /* defined (CONFIG_PWM_LED) || defined (CONFIG_BKL_PWM_LED) */

    /* pwm led */
#ifdef CONFIG_PWM_LED 
    memcpy(cmd.prm.u8, s_recovery_data.bk_pwm_param, SHUB_SIZE_PWM_PARAM);
    cmd.cmd.u16 = HC_MCU_SET_PWM_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PWM_PARAM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "%s : HC_MCU_SET_PWM_PARAM err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
    }
    memcpy(s_pwm_param, cmd.prm.u8, SHUB_SIZE_PWM_PARAM);
#endif

    /* backlight pwm led */
#ifdef CONFIG_BKL_PWM_LED
    memcpy(cmd.prm.u8, s_recovery_data.bk_bklpwm_param, SHUB_SIZE_BKLPWM_PARAM);
    cmd.cmd.u16 = HC_MCU_SET_BLPWM_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_BKLPWM_PARAM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "%s : HC_MCU_SET_BLPWM_PARAM err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
    }
    memcpy(s_bklpwm_param, cmd.prm.u8, SHUB_SIZE_BKLPWM_PARAM);

    cmd.prm.u8[0x00] = s_recovery_data.bk_bklpwm_port;
    cmd.cmd.u16 = HC_MCU_SET_BLPWM_PORT;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "%s : HC_MCU_SET_BLPWM_PORT err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
    }
    s_bkl_pwm_port = cmd.prm.u8[0x00];
#endif

/* SHMDS_HUB_3303_01 add S */
    /* device orientation */
    if(iCurrentSensorEnable & SHUB_ACTIVE_DEVICE_ORI){
        cmd.cmd.u16 = HC_ACC_GET_AUTO_MEASURE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : HC_ACC_GET_AUTO_MEASURE err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
        }else{
            cmd.cmd.u16 = HC_ACC_SET_AUTO_MEASURE;
            cmd.prm.u8[0] = res.res.u8[0];
            cmd.prm.u8[1] = (uint8_t)s_recovery_data.bk_cmd.cmd_dev_ori[0];
            cmd.prm.u8[2] = res.res.u8[2];
            cmd.prm.u8[3] = res.res.u8[3];
            cmd.prm.u8[4] = res.res.u8[4];
            cmd.prm.u8[5] = res.res.u8[5];
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "%s : HC_ACC_SET_AUTO_MEASURE err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
            }
        }

        memcpy(cmd.prm.u8, s_recovery_data.bk_cmd.cmd_dev_ori, SHUB_SIZE_DEV_ORI_PARAM);
        cmd.cmd.u16 = HC_ACC_SET_ANDROID_XY;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_DEV_ORI_PARAM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : HC_ACC_SET_ANDROID_XY err(ret=%d, res=0x%x)\n", __FUNCTION__, ret, res.err.u16);
        }
    }
/* SHMDS_HUB_3303_01 add E */
}

void shub_recovery_exe(void)
{
    int32_t ret;
    int32_t iCurrentEnable;
    uint8_t notify = 0;
    
    /* timer stop */
    suspendBatchingProc();
    shub_sensor_timer_stop();
    shub_pedo_timer_stop();    /* SHMDS_HUB_0304_04 add */
    
    /* suspend flag on */
//  s_is_suspend = true; /* SHMDS_HUB_0304_02 del */
    
    /* backup */
    shub_recovery_backup();
    
    /* initialize */
    ret = shub_initialize();
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "%s : initialize Error!\n", __FUNCTION__);
    }
    
    /* restore first */
    shub_recovery_restore_first();
    
    /* update */
    shub_activate_exec(0, 0);
    
    /* update logging */
    shub_activate_logging_exec(0, 0);
    
    /* pedometer update */
    iCurrentEnable = atomic_read(&g_CurrentSensorEnable) | atomic_read(&g_CurrentLoggingSensorEnable);
    if (iCurrentEnable & (STEPCOUNT_GROUP_MASK | STEPDETECT_GROUP_MASK)) {
        ret = shub_activate_pedom_exec(iCurrentEnable & (STEPCOUNT_GROUP_MASK | STEPDETECT_GROUP_MASK), POWER_ENABLE);
        if(SHUB_RC_OK != ret) {
            DBG(DBG_LV_ERROR, "%s : pedometer update Error!\n", __FUNCTION__);
        }
    }
    /* significant update */
    iCurrentEnable = atomic_read(&g_CurrentSensorEnable);
    if (iCurrentEnable & SHUB_ACTIVE_SIGNIFICANT) {
        iCurrentEnable &= ~SHUB_ACTIVE_SIGNIFICANT;
        atomic_set(&g_CurrentSensorEnable, iCurrentEnable);
        ret = shub_activate_significant_exec(SHUB_ACTIVE_SIGNIFICANT, POWER_ENABLE, &notify);
        if(SHUB_RC_OK != ret) {
            DBG(DBG_LV_ERROR, "%s : significant update Error!\n", __FUNCTION__);
        }
    }
    
    /* restore second */
    shub_recovery_restore_second();
    
    /* suspend flag off */
//  s_is_suspend = false; /* SHMDS_HUB_0304_02 del */
    
    /* timer start */
    shub_sensor_timer_start();
    resumeBatchingProc();
}

void shub_recovery(void)
{
    DBG(DBG_LV_ERROR, "%s : start\n", __FUNCTION__);
    
    shub_access_flg |= SHUB_ACCESS_RECOVERY;  /* SHMDS_HUB_0304_02 add */
    
    shub_recovery_exe();
    
    shub_access_flg &= ~SHUB_ACCESS_RECOVERY; /* SHMDS_HUB_0304_02 add */
    shub_recovery_cnt++;
    
    DBG(DBG_LV_ERROR, "%s : end\n", __FUNCTION__);
}

/* SHMDS_HUB_0304_02 add S */
void shub_user_reset_exe(void)
{
    int32_t ret;
    
    /* timer stop */
    suspendBatchingProc();
    shub_sensor_timer_stop();
    shub_pedo_timer_stop();    /* SHMDS_HUB_0304_04 add */
    
    /* initialize */
    ret = shub_initialize();
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "%s : initialize Error!\n", __FUNCTION__);
    }
}

void shub_user_reset(void)
{
    DBG(DBG_LV_ERROR, "%s : start\n", __FUNCTION__);
    
    shub_access_flg |= SHUB_ACCESS_USER_RESET;
    
    shub_user_reset_exe();
    
    shub_access_flg &= ~SHUB_ACCESS_USER_RESET;
    shub_reset_cnt++;
    
    DBG(DBG_LV_ERROR, "%s : end\n", __FUNCTION__);
}
/* SHMDS_HUB_0304_02 add E */

static void shub_int_recovery_work_func(struct work_struct *work)
{
    shub_recovery();
    shub_wake_lock_end(&shub_recovery_wake_lock);
}
/* SHMDS_HUB_3301_01 add E */

static int32_t shub_check_access(void);
static int32_t shub_boot_fw_check(void);
static int32_t shub_user_fw_check(void);

bool shub_fw_update_check(void)
{
    bool ret = true;
    if(shub_fw_write_flg == false){
        ret = false;
    }
    return ret;
}

bool shub_connect_check(void)
{
    bool ret = true;
    if(shub_connect_flg == false){
        ret = false;
    }
    return ret;
}

// SHMDS_HUB_0104_08 add S
#ifdef CONFIG_ANDROID_ENGINEERING
static int shub_spi_log = 0;
module_param(shub_spi_log, int, 0600);

void shub_wbuf_printk( uint8_t adr, const uint8_t *data, uint16_t size )
{
    int i, t;
    int log_size = size;
    const uint8_t *p = data;
    
    printk("write addr=0x%x, size=%d\n", adr, size);
//  if(log_size > 16){
//      log_size = 16;
//  }
    for(i=0; i<log_size; i+=8){
        t = log_size - i;
        if(t > 8){
            t = 8;
        }
        if(t == 1){
            printk("----> wBuf[%02d   ] : %02x\n", i,*p);
        }else if(t == 2){
            printk("----> wBuf[%02d-%02d] : %02x,%02x\n", i,i+t-1,*p,*(p+1));
        }else if(t == 3){
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2));
        }else if(t == 4){
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3));
        }else if(t == 5){
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4));
        }else if(t == 6){
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5));
        }else if(t == 7){
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6));
        }else{
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6),*(p+7));
        }
        p += t;
    }
}

void shub_rbuf_printk( uint8_t adr, uint8_t *data, uint16_t size )
{
    int i, t;
    int log_size = size;
    uint8_t *p = data;
    
    printk("Read  addr=0x%x, size=%d\n", adr, size);
//  if(log_size > 16){
//      log_size = 16;
//  }
    for(i=0; i<log_size; i+=8){
        t = log_size - i;
        if(t > 8){
            t = 8;
        }
        if(t == 1){
            printk("----> rBuf[%02d   ] : %02x\n", i,*p);
        }else if(t == 2){
            printk("----> rBuf[%02d-%02d] : %02x,%02x\n", i,i+t-1,*p,*(p+1));
        }else if(t == 3){
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2));
        }else if(t == 4){
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3));
        }else if(t == 5){
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4));
        }else if(t == 6){
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5));
        }else if(t == 7){
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6));
        }else{
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6),*(p+7));
        }
        p += t;
    }
}

#define SHUB_DBG_SPIW(adr, data, size) {    \
    if(shub_spi_log)                        \
        shub_wbuf_printk(adr, data, size);  \
}

#define SHUB_DBG_SPIR(adr, data, size) {    \
    if(shub_spi_log)                        \
        shub_rbuf_printk(adr, data, size);  \
}
#else
#define SHUB_DBG_SPIW(adr, data, size)
#define SHUB_DBG_SPIR(adr, data, size)
#endif
// SHMDS_HUB_0104_08 add E

int32_t shub_adjust_value(int32_t min,int32_t max,int32_t value)
{
    if(min > value) return min;
    if(max < value) return max;
    return value;
}

static struct timespec shub_get_timestamp(void)
{
    struct timespec ts;
#ifndef NO_LINUX
    ktime_get_ts(&ts);
    monotonic_to_bootbased(&ts);
#endif
    return ts;
}

/* SHMDS_HUB_0705_01 SHMDS_HUB_0705_02 mod S */
static int32_t hostif_write_exe(uint8_t adr, const uint8_t *data, uint16_t size)
{
    int32_t ret = SHUB_RC_ERR;
    int32_t shub_suspend_state = 0;

    /* suspend access check */
    if((s_is_suspend) && (!shub_suspend_call_flg)){
        shub_suspend_state = 1;
    }
    ret = hostif_write_proc(adr, data, size);

    if((ret < 0 ) && (shub_suspend_state == 1)) {
        shub_dbg_host_wcnt++;
        DBG(DBG_LV_ERROR, "%s : suspend access Err(cnt=%lld, addr=0x%02x, size=%d)\n", __func__, shub_dbg_host_wcnt, adr, size);
    }
    
    return ret;
}
/* SHMDS_HUB_0705_01 SHMDS_HUB_0705_02 mod E */

static int32_t hostif_write(uint8_t adr, const uint8_t *data, uint16_t size)
{
    int32_t i;
    int32_t ret = SHUB_RC_OK;

    SHUB_DBG_SPIW(adr, data, size); // SHMDS_HUB_0104_08 add

    for(i=0; i<ACC_SPI_RETRY_NUM; i++)
    {
        ret = hostif_write_exe(adr, data, size); /* SHMDS_HUB_0705_01 mod */
        if(ret == 0){
            return 0;

        }else if(ret == -EBUSY){
            DBG(DBG_LV_ERROR, "write EBUSY error(Retry:%d, addr=0x%02x, size=%d)\n",i,adr,size); /* SHMDS_HUB_0701_12 mod */
            usleep(100 * 1000); /* SHMDS_HUB_0102_11 mod */

        }else{
            DBG(DBG_LV_ERROR, "write Other error(addr=0x%02x, size=%d)\n",adr,size); /* SHMDS_HUB_0701_12 mod */
            break;
        }
    }
    collect_hosif_err_log(adr, size, ret);    /* SHMDS_HUB_0703_01 add */

    return ret;
}

/* SHMDS_HUB_0705_01 SHMDS_HUB_0705_02 mod S */
static int32_t hostif_read_exe(uint8_t adr, uint8_t *data, uint16_t size)
{
    int32_t ret = SHUB_RC_ERR;
    int32_t shub_suspend_state = 0;

    /* suspend access check */
    if((s_is_suspend) && (!shub_suspend_call_flg)){
        shub_suspend_state = 1;
    }
    ret = hostif_read_proc(adr, data, size);
    
    if((ret < 0 ) && (shub_suspend_state == 1)) {
        shub_dbg_host_rcnt++;
        DBG(DBG_LV_ERROR, "%s : suspend access Err(cnt=%lld, addr=0x%02x, size=%d)\n", __func__, shub_dbg_host_rcnt, adr, size);
    }
    
    return ret;
}
/* SHMDS_HUB_0705_01 SHMDS_HUB_0705_02 mod E */

static int32_t hostif_read(uint8_t adr, uint8_t *data, uint16_t size)
{
    int32_t i;
    int32_t ret = SHUB_RC_OK;

    for(i=0; i<ACC_SPI_RETRY_NUM; i++)
    {
        ret = hostif_read_exe(adr, data, size); /* SHMDS_HUB_0705_01 mod */
        if(ret == 0){
            SHUB_DBG_SPIR(adr, data, size); // SHMDS_HUB_0104_08 add
            return 0;

        }else if(ret == -EBUSY){
            DBG(DBG_LV_ERROR, "read EBUSY error(Retry:%d, addr=0x%02x, size=%d)\n",i,adr,size); /* SHMDS_HUB_0701_12 mod */
            usleep(100 * 1000); /* SHMDS_HUB_0102_11 mod */

        }else{
            DBG(DBG_LV_ERROR, "read Other error(addr=0x%02x, size=%d)\n",adr,size); /* SHMDS_HUB_0701_12 mod */
            break;
        }
    }
    collect_hosif_err_log(adr, size, ret);    /* SHMDS_HUB_0703_01 add */

    return ret;
}

#ifndef NO_LINUX

#if 1 /* SHMDS_HUB_0312_01 SHMDS_HUB_2604_01 mod */
/* SHMDS_HUB_2604_02 add S */
static uint64_t shub_get_offset_timer_cnt(int32_t timercnt)
{
    uint64_t micom_timer_base_clock_ns;
    
    if(timercnt == 0) {
        return 0;
    }
    if(s_sensor_task_delay_us <= SHUB_FW_CLOCK_THRESH){
        micom_timer_base_clock_ns = SHUB_FW_CLOCK_NS_LOW;
    }else{
        micom_timer_base_clock_ns = SHUB_FW_CLOCK_NS_HIGH;
    }
    return (uint64_t)(timercnt * micom_timer_base_clock_ns);
}
/* SHMDS_HUB_2604_02 add E */

static void pending_base_time(int32_t arg_iSensType, ktime_t *time, uint16_t timercnt)
{
    ktime_t pending_baseTime;
    ktime_t dbug_Time;
    uint64_t offsetTime = 0; /* SHMDS_HUB_2604_02 add */
    
    if(time == NULL){
        pending_baseTime = timespec_to_ktime(shub_get_timestamp());
    }else{
        pending_baseTime = *time;
    }
/* SHMDS_HUB_2604_01 SHMDS_HUB_2604_02 add S */
    if(timercnt != 0) {
        offsetTime = shub_get_offset_timer_cnt( (int32_t)timercnt );
        dbug_Time = pending_baseTime;
        pending_baseTime = ktime_sub_ns(pending_baseTime, offsetTime);
        DBG(DBG_LV_TIME, "[time] %s : %lld --> %lld, cnt=%d\n", __func__, dbug_Time.tv64, pending_baseTime.tv64, timercnt);
    }
/* SHMDS_HUB_2604_01 SHMDS_HUB_2604_02 add E */

    if(arg_iSensType & SHUB_ACTIVE_ACC){
        s_pending_baseTime.acc = pending_baseTime;
    }
    if(arg_iSensType & (SHUB_ACTIVE_GYRO | SHUB_ACTIVE_GYROUNC)){ /* SHMDS_HUB_0339_01 mod */
        s_pending_baseTime.gyro = pending_baseTime;
        DBG(DBG_LV_TIME, "[time] s_pending_baseTime( gyro=%lld )\n", s_pending_baseTime.gyro.tv64);
    }
    if(arg_iSensType & (SHUB_ACTIVE_MAG | SHUB_ACTIVE_MAGUNC)){   /* SHMDS_HUB_0339_01 mod */
        s_pending_baseTime.mag = pending_baseTime;
    }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        s_pending_baseTime.baro = pending_baseTime;
    }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
    if(arg_iSensType & SHUB_ACTIVE_ORI){
        s_pending_baseTime.orien = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        s_pending_baseTime.grav = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_LACC){
        s_pending_baseTime.linear = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV){
        s_pending_baseTime.rot = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        s_pending_baseTime.rot_gyro = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        s_pending_baseTime.rot_mag = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_PEDOM){
        s_pending_baseTime.pedocnt = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_PEDOM){
        s_pending_baseTime.pedocnt2 = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_TOTAL_STATUS){
        s_pending_baseTime.total_status2 = pending_baseTime;
    }
}
#else
static void pending_base_time(int32_t arg_iSensType)
{
    ktime_t pending_baseTime;
    pending_baseTime = timespec_to_ktime(shub_get_timestamp());

    if(arg_iSensType & SHUB_ACTIVE_ACC){
        s_pending_baseTime.acc = pending_baseTime;
    }
    if(arg_iSensType & (SHUB_ACTIVE_GYRO | SHUB_ACTIVE_GYROUNC)){ /* SHMDS_HUB_0339_01 mod */
        s_pending_baseTime.gyro = pending_baseTime;
    }
    if(arg_iSensType & (SHUB_ACTIVE_MAG | SHUB_ACTIVE_MAGUNC)){   /* SHMDS_HUB_0339_01 mod */
        s_pending_baseTime.mag = pending_baseTime;
    }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        s_pending_baseTime.baro = pending_baseTime;
    }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
    if(arg_iSensType & SHUB_ACTIVE_ORI){
        s_pending_baseTime.orien = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        s_pending_baseTime.grav = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_LACC){
        s_pending_baseTime.linear = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV){
        s_pending_baseTime.rot = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        s_pending_baseTime.rot_gyro = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        s_pending_baseTime.rot_mag = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_PEDOM){
        s_pending_baseTime.pedocnt = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_PEDOM){
        s_pending_baseTime.pedocnt2 = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_TOTAL_STATUS){
        s_pending_baseTime.total_status2 = pending_baseTime;
    }
}
#endif

/* SHMDS_HUB_0312_01 add S */
static int32_t shub_compare_gyroEnable_time(void)
{
    int ret;
    
    if(shub_gyro_entime_flg == 0){
        return -1;
    }
    ret = ktime_compare( s_beseTime.gyro, s_gyro_enable_time_s);
    if(ret < 0){
        return -1;
    }
    ret = ktime_compare( s_beseTime.gyro, s_gyro_enable_time_e);
    if(ret > 0){
        shub_gyro_entime_flg = 0;
        DBG(DBG_LV_TIME, "[time] %s : clear time = %lld\n", __func__, s_gyro_enable_time_s.tv64);
        return -1;
    }
    return 1;
}

/* SHMDS_HUB_2604_01 SHMDS_HUB_2604_02 add S */
static void shub_set_gyroEnable_time(ktime_t *time, uint16_t timercnt)
{
    uint64_t offsetTime;
    
    shub_gyro_entime_flg = 1;
    s_gyro_enable_time_s = *time;
    if(timercnt != 0) {
        offsetTime = shub_get_offset_timer_cnt( (int32_t)timercnt );
        s_gyro_enable_time_s = ktime_sub_ns(s_gyro_enable_time_s, offsetTime);
    }
    s_gyro_enable_time_e = ktime_add_us(s_gyro_enable_time_s, 130 * 1000);
    DBG(DBG_LV_TIME, "[time] %s : %lld --> %lld, cnt=%d\n", __func__, s_gyro_enable_time_s.tv64, s_gyro_enable_time_e.tv64, timercnt);
}
/* SHMDS_HUB_2604_01 SHMDS_HUB_2604_02 add E */
/* SHMDS_HUB_0312_01 add E */

static int32_t update_base_time(int32_t arg_iSensType, ktime_t *time )
{
    if(time == NULL){
        if(arg_iSensType & SHUB_ACTIVE_ACC){
            s_beseTime.acc = s_pending_baseTime.acc;
        }
        if(arg_iSensType & (SHUB_ACTIVE_GYRO | SHUB_ACTIVE_GYROUNC)){ /* SHMDS_HUB_0339_01 mod */
            s_beseTime.gyro = s_pending_baseTime.gyro;
            DBG(DBG_LV_TIME, "[time] s_beseTime null( gyro=%lld )\n", s_beseTime.gyro.tv64); /* SHMDS_HUB_0312_01 add */
        }
        if(arg_iSensType & (SHUB_ACTIVE_MAG | SHUB_ACTIVE_MAGUNC)){   /* SHMDS_HUB_0339_01 mod */
            s_beseTime.mag = s_pending_baseTime.mag;
        }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
        if(arg_iSensType & SHUB_ACTIVE_BARO){
            s_beseTime.baro = s_pending_baseTime.baro;
        }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
        if(arg_iSensType & SHUB_ACTIVE_ORI){
            s_beseTime.orien = s_pending_baseTime.orien;
        }
        if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
            s_beseTime.grav = s_pending_baseTime.grav;
        }
        if(arg_iSensType & SHUB_ACTIVE_LACC){
            s_beseTime.linear = s_pending_baseTime.linear;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV){
            s_beseTime.rot = s_pending_baseTime.rot;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
            s_beseTime.rot_gyro = s_pending_baseTime.rot_gyro;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
            s_beseTime.rot_mag = s_pending_baseTime.rot_mag;
        }
        if(arg_iSensType & SHUB_ACTIVE_PEDOM){
            s_beseTime.pedocnt = s_pending_baseTime.pedocnt;
        }
        if(arg_iSensType & SHUB_ACTIVE_EXT_PEDOM){
            s_beseTime.pedocnt2 = s_pending_baseTime.pedocnt2;
        }
        if(arg_iSensType & SHUB_ACTIVE_EXT_TOTAL_STATUS){
            s_beseTime.total_status2 = s_pending_baseTime.total_status2;
        }
    }else{
        if(arg_iSensType & SHUB_ACTIVE_ACC){
            s_beseTime.acc = *time;
        }
        if(arg_iSensType & (SHUB_ACTIVE_GYRO | SHUB_ACTIVE_GYROUNC)){ /* SHMDS_HUB_0339_01 mod */
            s_beseTime.gyro = *time;
            DBG(DBG_LV_TIME, "[time] s_beseTime addr( gyro=%lld )\n", s_beseTime.gyro.tv64); /* SHMDS_HUB_0312_01 add */
        }
        if(arg_iSensType & (SHUB_ACTIVE_MAG | SHUB_ACTIVE_MAGUNC)){   /* SHMDS_HUB_0339_01 mod */
            s_beseTime.mag = *time;
        }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
        if(arg_iSensType & SHUB_ACTIVE_BARO){
            s_beseTime.baro = *time;
        }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
        if(arg_iSensType & SHUB_ACTIVE_ORI){
            s_beseTime.orien = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
            s_beseTime.grav = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_LACC){
            s_beseTime.linear = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV){
            s_beseTime.rot = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
            s_beseTime.rot_gyro = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
            s_beseTime.rot_mag = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_PEDOM){
            s_beseTime.pedocnt = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_EXT_PEDOM){
            s_beseTime.pedocnt2 = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_EXT_TOTAL_STATUS){
            s_beseTime.total_status2 = *time;
        }
    }
    return SHUB_RC_OK;
}

/* SHMDS_HUB_0311_05 add S */
static uint64_t shub_do_div(uint64_t n, uint64_t base)
{
    uint64_t temp = n;
    do_div(temp, base);
    return temp;
}

static int32_t shub_update_time_flash_exe(ktime_t *time)
{
    int ret;
    int32_t result = 0;
    int32_t iCurrentLoggingEnable;
    uint64_t min_delay = MEASURE_MAX_US;
    uint64_t task_delay;
    uint64_t tmp;
    ktime_t oldTime = ktime_set(0, 0);
    ktime_t newTime = ktime_set(0, 0);
    ktime_t cngTime = ktime_set(0, 0);
    ktime_t sub_time = ktime_set(0, 0);
    
    task_delay = (uint64_t)(s_micon_param.task_cycle[0] * 10);
    iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    
    if(iCurrentLoggingEnable == 0){
        return result;
    }
    
    if(iCurrentLoggingEnable & SHUB_ACTIVE_ACC){
        tmp = s_logging_delay_us.acc;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.acc;
            newTime = s_pending_baseTime.acc;
        }
    }
    if(iCurrentLoggingEnable & (SHUB_ACTIVE_GYRO | SHUB_ACTIVE_GYROUNC)){
        tmp = s_logging_delay_us.gyro;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.gyro;
            newTime = s_pending_baseTime.gyro;
        }
    }
    if(iCurrentLoggingEnable & (SHUB_ACTIVE_MAG | SHUB_ACTIVE_MAGUNC)){
        tmp = s_logging_delay_us.mag;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.mag;
            newTime = s_pending_baseTime.mag;
        }
    }
#ifdef CONFIG_BARO_SENSOR /* SHMDS_HUB_0120_01 add */
    if(iCurrentLoggingEnable & SHUB_ACTIVE_BARO){
        tmp = s_logging_delay_us.baro;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.baro;
            newTime = s_pending_baseTime.baro;
        }
    }
#endif
    if(iCurrentLoggingEnable & SHUB_ACTIVE_ORI){
        tmp = s_logging_delay_us.orien;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.orien;
            newTime = s_pending_baseTime.orien;
        }
    }
    if(iCurrentLoggingEnable & SHUB_ACTIVE_GRAVITY){
        tmp = s_logging_delay_us.grav;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.grav;
            newTime = s_pending_baseTime.grav;
        }
    }
    if(iCurrentLoggingEnable & SHUB_ACTIVE_LACC){
        tmp = s_logging_delay_us.linear;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.linear;
            newTime = s_pending_baseTime.linear;
        }
    }
    if(iCurrentLoggingEnable & SHUB_ACTIVE_RV){
        tmp = s_logging_delay_us.rot;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.rot;
            newTime = s_pending_baseTime.rot;
        }
    }
    if(iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONMAG){
        tmp = s_logging_delay_us.rot_gyro;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.rot_gyro;
            newTime = s_pending_baseTime.rot_gyro;
        }
    }
    if(iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONGYRO){
        tmp = s_logging_delay_us.rot_mag;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.rot_mag;
            newTime = s_pending_baseTime.rot_mag;
        }
    }
    
    if(task_delay != min_delay){
        return result;
    }
    
    ret = ktime_compare(oldTime, newTime);
    if(ret > 0){
        sub_time = ktime_sub(oldTime, newTime);
    }else{
        sub_time = ktime_sub(newTime, oldTime);
    }
    
    min_delay = (uint64_t)(min_delay * 1000);
    tmp = (uint64_t)(shub_do_div(min_delay, 10) * 8);
    if(sub_time.tv64 < (min_delay + tmp)){
        if(ret > 0){
            cngTime = ktime_add_ns(newTime, shub_do_div(sub_time.tv64, 2));
        }else{
            cngTime = ktime_sub_ns(newTime, shub_do_div(sub_time.tv64, 2));
        }
        *time = cngTime;
        DBG(DBG_LV_TIME, "[time] flash_update : %lld --> %lld( %lld ), d=%lld, sub=%lld\n", oldTime.tv64, cngTime.tv64, newTime.tv64, task_delay, shub_do_div(sub_time.tv64, 1000));
        result = 1;
    }else{
        DBG(DBG_LV_TIME, "[time][over] flash_update : %lld --> %lld, d=%lld, sub=%lld\n", oldTime.tv64, newTime.tv64, task_delay, shub_do_div(sub_time.tv64, 1000));
    }
    return result;
}

static void shub_update_time_flash(int32_t arg_iSensType)
{
    int32_t ret;
    ktime_t newTime = ktime_set(0, 0);
    
    ret = shub_update_time_flash_exe(&newTime);
    if(ret == 0){
        update_base_time(arg_iSensType, NULL);
    }else{
        update_base_time(arg_iSensType, &newTime);
    }
}
/* SHMDS_HUB_0311_05 add E */

static struct timespec event_time_to_offset(int32_t arg_iSensType,int32_t eventOffsetTime)
{
    ktime_t baseTime = timespec_to_ktime(shub_get_timestamp());
    ktime_t eventTime = ktime_set(0, 0);
    struct timespec ts;
    uint64_t sensorTaskCycle_ns;
    uint64_t micom_timer_base_clock_ns;

    if(arg_iSensType & SHUB_ACTIVE_ACC){
        baseTime = s_beseTime.acc;
    }
    if(arg_iSensType & (SHUB_ACTIVE_GYRO | SHUB_ACTIVE_GYROUNC)){ /* SHMDS_HUB_0339_01 mod */
        baseTime =  s_beseTime.gyro;
    }
    if(arg_iSensType & (SHUB_ACTIVE_MAG | SHUB_ACTIVE_MAGUNC)){   /* SHMDS_HUB_0339_01 mod */
        baseTime =  s_beseTime.mag;
    }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        baseTime = s_beseTime.baro;
    }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
    if(arg_iSensType & SHUB_ACTIVE_ORI){
        baseTime =  s_beseTime.orien;
    }
    if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        baseTime =  s_beseTime.grav;
    }
    if(arg_iSensType & SHUB_ACTIVE_LACC){
        baseTime =  s_beseTime.linear;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV){
        baseTime =  s_beseTime.rot;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        baseTime = s_beseTime.rot_gyro;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        baseTime = s_beseTime.rot_mag;
    }
    if(arg_iSensType & SHUB_ACTIVE_PEDOM){
        baseTime = s_beseTime.pedocnt;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_PEDOM){
        baseTime = s_beseTime.pedocnt2;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_TOTAL_STATUS){
        baseTime = s_beseTime.total_status2;
    }

    s_sensor_task_delay_us = (s_sensor_task_delay_us / 10) * 10; 
    sensorTaskCycle_ns = s_sensor_task_delay_us * 1000;
    if(s_sensor_task_delay_us <= SHUB_FW_CLOCK_THRESH){    /* SHMDS_HUB_2604_02 mod */
        micom_timer_base_clock_ns = SHUB_FW_CLOCK_NS_LOW;  /* SHMDS_HUB_2604_02 mod */
    }else{
        micom_timer_base_clock_ns = SHUB_FW_CLOCK_NS_HIGH; /* SHMDS_HUB_2604_02 mod */
    }
    do_div(sensorTaskCycle_ns, micom_timer_base_clock_ns);
    sensorTaskCycle_ns*= micom_timer_base_clock_ns;

    eventTime  = ktime_add_ns(baseTime, eventOffsetTime * sensorTaskCycle_ns);
// SHMDS_HUB_0312_01 add S
//  if(arg_iSensType & (SHUB_ACTIVE_GYRO | SHUB_ACTIVE_GYROUNC)){ /* SHMDS_HUB_0339_01 mod */
//      DBG(DBG_LV_TIME, "[shub][time] Base=%lld, Cycle=%llu, Cnt=%d\n", baseTime.tv64, sensorTaskCycle_ns, eventOffsetTime);
//  }
// SHMDS_HUB_0312_01 add E
    update_base_time(arg_iSensType, &eventTime);
    ts = ns_to_timespec(eventTime.tv64);

    return ts;
}

#endif

/* SHMDS_HUB_0120_10 add S */
#ifdef CONFIG_BARO_SENSOR
static int32_t shub_baro_filter( struct barometric* baroData, uint32_t pressure ) {
    int i, j;
    uint32_t tmp = 0;
    uint32_t tmpBuf[SHUB_BARO_BUFFER_SIZE];

    baroData->buffer[baroData->pos] = pressure;
    if( baroData->num < SHUB_BARO_BUFFER_SIZE ) {
        baroData->num++;
    }
    baroData->pos++;
    if( baroData->pos >= SHUB_BARO_BUFFER_SIZE ) {
        baroData->pos = 0;
    }

    for (i = 0; i < baroData->num; ++i) {
        tmpBuf[i] = baroData->buffer[i];
    }
    for (i = 0; i < baroData->num; ++i) {
        for (j = i+1; j < baroData->num; ++j) {
            if (tmpBuf[i] > tmpBuf[j]) {
                tmp = tmpBuf[i];
                tmpBuf[i] = tmpBuf[j];
                tmpBuf[j] = tmp;
            }
        }
    }
#ifdef CONFIG_ANDROID_ENGINEERING
    if(baro_filter){
        baroData->pressure = tmpBuf[baroData->num/2];
    } else {
        baroData->pressure = pressure;
    }
#else
    baroData->pressure = tmpBuf[baroData->num/2];
#endif
    return SHUB_RC_OK;
}
#endif
/* SHMDS_HUB_0120_10 add E */

static int32_t sensor_get_logging_data(uint8_t* buf, int32_t size)
{
    int32_t    ret;
    HostCmd    cmd;
    HostCmdRes res;
    uint8_t    ucBuff[2];
    int32_t    logsize = size;
    int32_t    readsize;
    int32_t    fifosize;
    uint8_t    *wp;

    wp = buf;
    do{
        if(logsize > FIFO_SIZE){
            readsize = FIFO_SIZE;
        } else {
            readsize = logsize;
        }

        cmd.cmd.u16 = HC_LOGGING_GET_RESULT;
        cmd.prm.u8[0] = 0x00;
        cmd.prm.u8[1] = readsize & 0xFF;
        cmd.prm.u8[2] = (readsize >> 0x08) & 0xFF;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_SKIP_MUTEX_UNLOCK | EXE_HOST_WAIT|EXE_HOST_ERR, 3);  /* SHMDS_HUB_0360_01 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_GET_RESULT err (%x)\n", res.err.u16);
            mutex_unlock(&s_hostCmdMutex);  /* SHMDS_HUB_0360_01 add */
            return SHUB_RC_ERR;
        }

        ret =  hostif_read(RSLT3E, ucBuff, 2);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "RSLT3E/3F read err\n");
            mutex_unlock(&s_hostCmdMutex);  /* SHMDS_HUB_0360_01 add */
            return SHUB_RC_ERR;
        }
        fifosize =(ucBuff[0] | ucBuff[1] << 0x08);
        //size check
        if(readsize != fifosize){
            DBG(DBG_LV_ERROR, "Get readsize error. [%d != %d] \n",readsize,fifosize);
            mutex_unlock(&s_hostCmdMutex);  /* SHMDS_HUB_0360_01 add */
            return SHUB_RC_ERR;
        }

        ret = hostif_read(FIFO, wp, readsize);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "FIFO read err. remain = 0x%x\n", logsize);
            mutex_unlock(&s_hostCmdMutex);  /* SHMDS_HUB_0360_01 add */
            return SHUB_RC_ERR;
        }
        mutex_unlock(&s_hostCmdMutex);      /* SHMDS_HUB_0360_01 add */
        wp += readsize;
        logsize -= readsize;
    }while(logsize > 0);

    return SHUB_RC_OK;
}


static int32_t logging_flush_exec(void)
{
    int32_t ret = SHUB_RC_OK;
    int32_t size, size_buff, cmdsize;
    uint8_t *buf, *buf_tmp;
    int32_t iCurrentEnable;
    uint8_t op_buf;
    uint8_t data_tmp[12];
    HostCmd    cmd;
    HostCmdRes res;
    TotalOfDeletedTimestamp delTimestamp;
    uint32_t tm_tmp; 
//  uint32_t bk_time; /* SHMDS_HUB_0359_01 add */
    int32_t data[10];
    struct timespec tstamp;
    uint8_t gyro_ofs_first=1;
    uint8_t mag_ofs_first=1;
    int32_t aRet;         /* SHMDS_HUB_0312_01 add */
    uint16_t iTimerCnt = 0;   /* SHMDS_HUB_2604_01 add */

    size = LOGGING_RAM_SIZE;

    iCurrentEnable = atomic_read(&g_CurrentLoggingSensorEnable);

    buf = (uint8_t *)kmalloc(size, GFP_KERNEL );
    if(buf == NULL){
        DBG(DBG_LV_ERROR, "error(kmalloc) : %s\n", __FUNCTION__);
        update_base_time(ACTIVE_FUNC_MASK,NULL);
        return -ENOMEM;
    }else{
        // for "kfree"
        buf_tmp = buf;
    }

    memset(buf, 0xFF, size);

    cmd.cmd.u16 = HC_LOGGING_DELIMITER;
    shub_basetime_req = 1;                                   /* SHMDS_HUB_0312_01 add */
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_SKIP_MUTEX_UNLOCK |
            EXE_HOST_RES_ONLY_FIFO_SIZE | 
            EXE_HOST_WAIT |
            EXE_HOST_ERR , 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_LOGGING_DELIMITER err (%x)\n", res.err.u16);
        shub_basetime_req = 0;                               /* SHMDS_HUB_0312_01 add */
        update_base_time(ACTIVE_FUNC_MASK,NULL);
        kfree(buf_tmp);
        mutex_unlock(&s_hostCmdMutex);
        return SHUB_RC_ERR;
    }
//  iTimerCnt = (uint16_t)RESU8_TO_X16(res,98);              /* SHMDS_HUB_2604_01 SHMDS_HUB_2604_02 del */
//  pending_base_time(ACTIVE_FUNC_MASK);                     /* SHMDS_HUB_0312_01 del */
//  pending_base_time(ACTIVE_FUNC_MASK, &shub_baseTime);     /* SHMDS_HUB_0312_01 SHMDS_HUB_2604_01 */
//  pending_base_time(ACTIVE_FUNC_MASK, &shub_baseTime, iTimerCnt); /* SHMDS_HUB_2604_01 SHMDS_HUB_2604_02 del */
    if(res.res_size != 0){
        ret = hostif_read(FIFO, res.res.u8, res.res_size);
        if(ret != SHUB_RC_OK) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_DELIMITER err (%x)\n", res.err.u16);
            update_base_time(ACTIVE_FUNC_MASK,NULL);
            kfree(buf_tmp);
            mutex_unlock(&s_hostCmdMutex);
            return SHUB_RC_ERR;
        }
        iTimerCnt = (uint16_t)RESU8_TO_X16(res,98);                     /* SHMDS_HUB_2604_02 add */
        pending_base_time(ACTIVE_FUNC_MASK, &shub_baseTime, iTimerCnt); /* SHMDS_HUB_2604_02 add */
    }
    mutex_unlock(&s_hostCmdMutex);
    size = res.res.u16[0];

    if(size > DELIMITER_LOGGING_SIZE){
        memcpy(buf, (uint8_t *)&(res.res.u8[DELIMITER_TIMESTAMP_SIZE]), DELIMITER_LOGGING_SIZE);
    }else{
        memcpy(buf, (uint8_t *)&(res.res.u8[DELIMITER_TIMESTAMP_SIZE]), size);
    }
    delTimestamp.acc        = (uint32_t)RESU8_TO_X32(res,2 );
    delTimestamp.mag        = (uint32_t)RESU8_TO_X32(res,6 );
    delTimestamp.gyro       = (uint32_t)RESU8_TO_X32(res,10);
    delTimestamp.orien      = (uint32_t)RESU8_TO_X32(res,14);
    delTimestamp.grav       = (uint32_t)RESU8_TO_X32(res,18);
    delTimestamp.linear     = (uint32_t)RESU8_TO_X32(res,22);
    delTimestamp.rot        = (uint32_t)RESU8_TO_X32(res,26);
    delTimestamp.rot_gyro   = (uint32_t)RESU8_TO_X32(res,30);
    delTimestamp.rot_mag    = (uint32_t)RESU8_TO_X32(res,34);
    delTimestamp.pedocnt    = (uint32_t)RESU8_TO_X32(res,38);
    delTimestamp.total_status = (uint32_t)RESU8_TO_X32(res,42);
    delTimestamp.pedocnt2   = (uint32_t)RESU8_TO_X32(res,46);
    delTimestamp.total_status2= (uint32_t)RESU8_TO_X32(res,50);
/* SHMDS_HUB_0120_01 add S */
#ifdef CONFIG_BARO_SENSOR   /* SHMDS_HUB_0120_09 mod */
    delTimestamp.baro       = (uint32_t)RESU8_TO_X32(res,54);
#endif
/* SHMDS_HUB_0120_01 add E */

    if(size > DELIMITER_LOGGING_SIZE){
        ret = sensor_get_logging_data(&buf[DELIMITER_LOGGING_SIZE], size-DELIMITER_LOGGING_SIZE);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "sensor_get_logging_data err!!\n" );
            update_base_time(ACTIVE_FUNC_MASK,NULL);
            kfree(buf_tmp);
            return SHUB_RC_ERR;
        }
    }

    size_buff = size;
    DBG(DBG_LV_TIME, "%s : size_buff=%d\n", __func__, size_buff); /* SHMDS_HUB_0312_01 add */

    while (size_buff)
    {
        memset(data_tmp, 0, sizeof(data_tmp));
        cmdsize = 0xFF;
        op_buf = *buf;
        buf++;
        size_buff--;

        /* Length Check */
        switch(op_buf)
        {
            case DATA_OPECORD_ACC:
                cmdsize=DATA_SIZE_ACC;
                break;
            case DATA_OPECORD_MAG:
                cmdsize=DATA_SIZE_MAG;
                break;
            case DATA_OPECORD_GYRO:
                cmdsize=DATA_SIZE_GYRO;
                break;
            case DATA_OPECORD_MAG_CAL_OFFSET:
                cmdsize=DATA_SIZE_MAG_CAL_OFFSET;
                break;
            case DATA_OPECORD_GYRO_CAL_OFFSET:
                cmdsize=DATA_SIZE_GYRO_CAL_OFFSET;
                break;
            case DATA_OPECORD_ORI:
                cmdsize=DATA_SIZE_ORI;
                break;
            case DATA_OPECORD_GRAVITY:
                cmdsize=DATA_SIZE_GRAVITY;
                break;
            case DATA_OPECORD_LINEARACC:
                cmdsize=DATA_SIZE_LINEARACC;
                break;
            case DATA_OPECORD_RVECT:
                cmdsize=DATA_SIZE_RVECT;
                break;
            case DATA_OPECORD_GAMERV:
                cmdsize=DATA_SIZE_GAMERV;
                break;
            case DATA_OPECORD_GEORV:
                cmdsize=DATA_SIZE_GEORV;
                break;
            case DATA_OPECORD_PEDOCNT:
                cmdsize=DATA_SIZE_PEDOCNT;
                break;
            case DATA_OPECORD_TOTAL_STATUS:
                cmdsize=DATA_SIZE_TOTALSTATUS;
                break;
            case DATA_OPECORD_PEDOCNT2:
                cmdsize=DATA_SIZE_PEDOCNT;
                break;
            case DATA_OPECORD_TOTAL_STATUS2:
                cmdsize=DATA_SIZE_TOTALSTATUS;
                break;
/* SHMDS_HUB_0120_01 add S */
#ifdef CONFIG_BARO_SENSOR   /* SHMDS_HUB_0120_09 mod */
            case DATA_OPECORD_BARO:
                cmdsize=DATA_SIZE_BARO;
                break;
#endif
/* SHMDS_HUB_0120_01 add E */
            default:
                DBG(DBG_LV_ERROR, "logging data type non [0x%02X]\n", op_buf);
                cmd.cmd.u16 = HC_LOGGING_GET_RESULT;
                cmd.prm.u16[0] = 1;
                shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);

                kfree(buf_tmp);
                return SHUB_RC_ERR;
                break;
        }

        if (cmdsize == 0xff)
        {
            DBG(DBG_LV_ERROR, "logging format error [%x]", op_buf);
            cmd.cmd.u16 = HC_LOGGING_GET_RESULT;
            cmd.prm.u16[0] = 1;
            shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);

            kfree(buf_tmp);
            return SHUB_RC_ERR;
        }

        //check buffer over flow
        if (size_buff < cmdsize)
        {
            /* data non */
            DBG(DBG_LV_ERROR, "buffer over flow");
            kfree(buf_tmp);
            update_base_time(ACTIVE_FUNC_MASK,NULL);
            return SHUB_RC_ERR;
        }

        switch(op_buf)
        {
            case DATA_OPECORD_ACC:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_ACC);
                    buf+=DATA_SIZE_ACC;
                    size_buff-=DATA_SIZE_ACC;
                    tm_tmp = data_tmp[DATA_SIZE_ACC - 7] + delTimestamp.acc;
                    delTimestamp.acc = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_ACC, tm_tmp);
                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ACC - 6]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ACC - 4]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ACC - 2]);
                    data[3] = tstamp.tv_sec;
                    data[4] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_ACC){
                        shub_input_report_acc(data);
                    }
                }
                break;
            case DATA_OPECORD_MAG:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_MAG);
                    buf+=DATA_SIZE_MAG;
                    size_buff-=DATA_SIZE_MAG;

                    tm_tmp = data_tmp[DATA_SIZE_MAG - 8] + delTimestamp.mag;
                    delTimestamp.mag = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_MAG, tm_tmp);
                    if(iCurrentEnable & SHUB_ACTIVE_MAG){
                        data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 7]);
                        data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 5]);
                        data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 3]);
                        data[3] = (int32_t)data_tmp[DATA_SIZE_MAG - 1];
                        data[4] = tstamp.tv_sec;
                        data[5] = tstamp.tv_nsec;
                        shub_input_report_mag(data);
                    }

                    if(iCurrentEnable & SHUB_ACTIVE_MAGUNC){
                        data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 7]) + s_tLatestMagData.nXOffset;
                        data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 5]) + s_tLatestMagData.nYOffset;
                        data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 3]) + s_tLatestMagData.nZOffset;
                        data[3] = s_tLatestMagData.nXOffset;
                        data[4] = s_tLatestMagData.nYOffset;
                        data[5] = s_tLatestMagData.nZOffset;
                        data[6] = (int32_t)data_tmp[DATA_SIZE_MAG - 1];
                        data[7] = tstamp.tv_sec;
                        data[8] = tstamp.tv_nsec;
                        shub_input_report_mag_uncal(data);
                    }
                }
                break;
            case DATA_OPECORD_GYRO:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_GYRO);
                    buf+=DATA_SIZE_GYRO;
                    size_buff-=DATA_SIZE_GYRO;
                    tm_tmp = data_tmp[DATA_SIZE_GYRO - 7] + delTimestamp.gyro;
//                  bk_time = delTimestamp.gyro; /* SHMDS_HUB_0359_01 add */
                    delTimestamp.gyro = 0;       /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_GYRO, tm_tmp);
/* SHMDS_HUB_0312_01 add S */
                    aRet = shub_compare_gyroEnable_time();
                    if(aRet > 0){
                        DBG(DBG_LV_INFO, "[time] %s : Sec=%d, nanoSec=%d\n", __func__, (int)tstamp.tv_sec, (int)tstamp.tv_nsec);
                    }else{
/* SHMDS_HUB_0312_01 add E */
                        if(iCurrentEnable & SHUB_ACTIVE_GYRO){
                            data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 6]);
                            data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 4]);
                            data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 2]);
                            data[3] = s_tLatestGyroData.nAccuracy;
                            data[4] = tstamp.tv_sec;
                            data[5] = tstamp.tv_nsec;
                            shub_input_report_gyro(data);
                            //DBG(DBG_LV_INFO, "gyro sec= %d nsec=%d cnt=%d sum=%d\n"
                            //        ,tstamp.tv_sec,tstamp.tv_nsec,tm_tmp,bk_time); /* SHMDS_HUB_0359_01 mod */
                        }
                        if(iCurrentEnable & SHUB_ACTIVE_GYROUNC){
                            data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 6]) + s_tLatestGyroData.nXOffset;
                            data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 4]) + s_tLatestGyroData.nYOffset;
                            data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 2]) + s_tLatestGyroData.nZOffset;
                            data[3] = s_tLatestGyroData.nAccuracy;
                            data[4] = s_tLatestGyroData.nXOffset;
                            data[5] = s_tLatestGyroData.nYOffset;
                            data[6] = s_tLatestGyroData.nZOffset;
                            data[7] = tstamp.tv_sec;
                            data[8] = tstamp.tv_nsec;
                            shub_input_report_gyro_uncal(data);
                        }
/* SHMDS_HUB_0312_01 add S */
                    }
/* SHMDS_HUB_0312_01 add E */
                }
                break;
            case DATA_OPECORD_MAG_CAL_OFFSET:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_MAG_CAL_OFFSET);
                    buf+=DATA_SIZE_MAG_CAL_OFFSET;
                    size_buff-=DATA_SIZE_MAG_CAL_OFFSET;
                    mag_ofs_first=0;
                    s_tLatestMagData.nXOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG_CAL_OFFSET - 6]);
                    s_tLatestMagData.nYOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG_CAL_OFFSET - 4]);
                    s_tLatestMagData.nZOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG_CAL_OFFSET - 2]);
                }
                break;
            case DATA_OPECORD_GYRO_CAL_OFFSET:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_GYRO_CAL_OFFSET);
                    buf+=DATA_SIZE_GYRO_CAL_OFFSET;
                    size_buff-=DATA_SIZE_GYRO_CAL_OFFSET;
                    if(gyro_ofs_first == 0){
                        s_tLatestGyroData.nAccuracy = 3;
                    }else{
                        gyro_ofs_first=0;
                    }
                    s_tLatestGyroData.nXOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO_CAL_OFFSET - 6]);
                    s_tLatestGyroData.nYOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO_CAL_OFFSET - 4]);
                    s_tLatestGyroData.nZOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO_CAL_OFFSET - 2]);
                }
                break;
            case DATA_OPECORD_ORI:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_ORI);
                    buf+=DATA_SIZE_ORI;
                    size_buff-=DATA_SIZE_ORI;

                    tm_tmp = data_tmp[DATA_SIZE_ORI - 8] + delTimestamp.orien;
                    delTimestamp.orien = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_ORI, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ORI - 7]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ORI - 5]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ORI - 3]);
                    data[3] = (int32_t)data_tmp[DATA_SIZE_ORI - 1];
                    data[4] = tstamp.tv_sec;
                    data[5] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_ORI){
                        shub_input_report_orien(data);
                    }
                }
                break;
            case DATA_OPECORD_GRAVITY:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_GRAVITY);
                    buf+=DATA_SIZE_GRAVITY;
                    size_buff-=DATA_SIZE_GRAVITY;

                    tm_tmp = data_tmp[DATA_SIZE_GRAVITY - 7] + delTimestamp.grav;
                    delTimestamp.grav = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_GRAVITY, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GRAVITY - 6]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GRAVITY - 4]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GRAVITY - 2]);
                    data[3] = tstamp.tv_sec;
                    data[4] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_GRAVITY){
                        shub_input_report_grav(data);
                    }
                }
                break;
            case DATA_OPECORD_LINEARACC:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_LINEARACC);
                    buf+=DATA_SIZE_LINEARACC;
                    size_buff-=DATA_SIZE_LINEARACC;

                    tm_tmp = data_tmp[DATA_SIZE_LINEARACC - 7] + delTimestamp.linear;
                    delTimestamp.linear = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_LACC, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_LINEARACC - 6]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_LINEARACC - 4]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_LINEARACC - 2]);
                    data[3] = tstamp.tv_sec;
                    data[4] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_LACC){
                        shub_input_report_linear(data);
                    }
                }
                break;
            case DATA_OPECORD_RVECT:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_RVECT);
                    buf+=DATA_SIZE_RVECT;
                    size_buff-=DATA_SIZE_RVECT;

                    tm_tmp = data_tmp[DATA_SIZE_RVECT - 9] + delTimestamp.rot;
                    delTimestamp.rot = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_RV, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_RVECT - 8]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_RVECT - 6]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_RVECT - 4]);
                    data[3] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_RVECT - 2]);
                    data[4] = -1; 
                    data[5] = tstamp.tv_sec;
                    data[6] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_RV){
                        shub_input_report_rot(data);
                    }
                }
                break;
            case DATA_OPECORD_GAMERV:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_GAMERV);
                    buf+=DATA_SIZE_GAMERV;
                    size_buff-=DATA_SIZE_GAMERV;

                    tm_tmp = data_tmp[DATA_SIZE_GAMERV - 9] + delTimestamp.rot_gyro;
                    delTimestamp.rot_gyro = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_RV_NONMAG, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GAMERV - 8]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GAMERV - 6]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GAMERV - 4]);
                    data[3] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GAMERV - 2]);
                    data[4] = tstamp.tv_sec;
                    data[5] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_RV_NONMAG){
                        shub_input_report_rot_gyro(data);
                    }
                }
                break;
            case DATA_OPECORD_GEORV:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_GEORV);
                    buf+=DATA_SIZE_GEORV;
                    size_buff-=DATA_SIZE_GEORV;

                    tm_tmp = data_tmp[DATA_SIZE_GEORV - 10] + delTimestamp.rot_mag;
                    delTimestamp.rot_mag = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_RV_NONGYRO, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GEORV - 9]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GEORV - 7]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GEORV - 5]);
                    data[3] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GEORV - 3]);
                    data[4] = (int32_t)data_tmp[DATA_SIZE_GEORV - 1];
                    data[5] = tstamp.tv_sec;
                    data[6] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_RV_NONGYRO){
                        shub_input_report_rot_mag(data);
                    }
                }
                break;
            case DATA_OPECORD_PEDOCNT:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_PEDOCNT);
                    buf+=DATA_SIZE_PEDOCNT;
                    size_buff-=DATA_SIZE_PEDOCNT;

                    tm_tmp = (int32_t)(((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 10]) |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 9] & 0xFF) << 8)  |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 8] & 0xFF) << 16) |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 7] & 0xFF) << 24));
                    tm_tmp += delTimestamp.pedocnt;
//                  bk_time = delTimestamp.pedocnt; /* SHMDS_HUB_0359_01 add */
                    delTimestamp.pedocnt = 0;       /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_PEDOM, tm_tmp);
                    data[0] = (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 6]) |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 5] & 0xFF) << 8)  |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 4] & 0xFF) << 16) |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 3] & 0xFF) << 24));
                    data[1] = tstamp.tv_sec;
                    data[2] = tstamp.tv_nsec;
                    if(iCurrentEnable & (SHUB_ACTIVE_PEDOM | SHUB_ACTIVE_PEDOM_NO_NOTIFY)){
                        data[0] = data[0] + s_recovery_data.bk_OffsetStep; /* SHMDS_HUB_3301_01 add */
                        //DBG(DBG_LV_INFO, "pedom sec= %d nsec=%d cnt=%d sum=%d steps %d\n"
                        //        ,tstamp.tv_sec,tstamp.tv_nsec,tm_tmp, bk_time, data[0]); /* SHMDS_HUB_0359_01 mod */
                        s_tLatestStepCountData.step = (uint64_t)data[0]; 
                    // SHMDS_HUB_0303_01 del S
//                        data[0] -= (int32_t)s_tLatestStepCountData.stepOffset;
                    // SHMDS_HUB_0303_01 del E
                        shub_input_report_stepcnt(data);
                    }
                    if(iCurrentEnable & (SHUB_ACTIVE_PEDODEC | SHUB_ACTIVE_PEDODEC_NO_NOTIFY)){
                        //DBG(DBG_LV_INFO, "pedom sec= %d nsec=%d cnt=%d sum=%d steps %d\n"
                        //        ,tstamp.tv_sec,tstamp.tv_nsec,tm_tmp, bk_time, data[0]); /* SHMDS_HUB_0359_01 mod */
                        shub_input_report_stepdetect(data);
                    }
                    break;
                }
            case DATA_OPECORD_PEDOCNT2:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_PEDOCNT);
                    buf+=DATA_SIZE_PEDOCNT;
                    size_buff-=DATA_SIZE_PEDOCNT;

                    tm_tmp = (int32_t)(((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 10]) |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 9] & 0xFF) << 8)  |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 8] & 0xFF) << 16) |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 7] & 0xFF) << 24));
                    tm_tmp += delTimestamp.pedocnt2;
                    delTimestamp.pedocnt2 = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_EXT_PEDOM, tm_tmp);
                    data[0] = (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 6]) |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 5] & 0xFF) << 8)  |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 4] & 0xFF) << 16) |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 3] & 0xFF) << 24));
                    data[1] = (int32_t)U8_TO_U16(&data_tmp[DATA_SIZE_PEDOCNT - 2]);
                    data[2] = tstamp.tv_sec;
                    data[3] = tstamp.tv_nsec;
                    /*
                       DBG(DBG_LV_ERROR, "Steps = %d, Calorie = %d Sec=%d nanoSec=%d\n",data[0],data[1],data[2],data[3]);
                     */
                    break;
                }
            case DATA_OPECORD_TOTAL_STATUS2 :
                {
                    memcpy(data_tmp , buf, DATA_SIZE_TOTALSTATUS);
                    buf+=DATA_SIZE_TOTALSTATUS;
                    size_buff-=DATA_SIZE_TOTALSTATUS;

                    //timestamp
                    tm_tmp = (int32_t)(((uint32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 9]) |
                            (((uint32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 8] & 0xFF) << 8)  |
                            (((uint32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 7] & 0xFF) << 16) |
                            (((uint32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 6] & 0xFF) << 24));
                    tm_tmp += delTimestamp.total_status2;
                    delTimestamp.total_status2 = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_EXT_TOTAL_STATUS, tm_tmp);
                    //total status
                    data[0] = (int32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 5];
                    //bycicle rate
                    data[1] = (int32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 4];
                    //car rate
                    data[2] = (int32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 3];
                    //train rate
                    data[3] = (int32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 2];
                    //no ride rate
                    data[4] = (int32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 1];
                    //timestamp [sec]
                    data[5] = tstamp.tv_sec;
                    //timestamp [nsec]
                    data[6] = tstamp.tv_nsec;
                    /*
                       DBG(DBG_LV_ERROR, "Status  = %x \n",data[0]);
                       DBG(DBG_LV_ERROR, "Bycicle = %x \n",data[1]);
                       DBG(DBG_LV_ERROR, "Car     = %x \n",data[2]);
                       DBG(DBG_LV_ERROR, "Train   = %x \n",data[3]);
                       DBG(DBG_LV_ERROR, "No Ride = %x \n",data[4]);
                       DBG(DBG_LV_ERROR, "Sec     = %d \n",data[5]);
                       DBG(DBG_LV_ERROR, "nanoSec = %d \n",data[6]);
                     */
                    break;
                }
/* SHMDS_HUB_0120_01 add S */
#ifdef CONFIG_BARO_SENSOR   /* SHMDS_HUB_0120_09 mod */
            case DATA_OPECORD_BARO:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_BARO);
                    buf+=DATA_SIZE_BARO;
                    size_buff-=DATA_SIZE_BARO;
/* SHMDS_HUB_0120_02 mod S */
                    tm_tmp = data_tmp[DATA_SIZE_BARO - 4] + delTimestamp.baro;
                    delTimestamp.baro = 0; /* SHMDS_HUB_0359_01 add */
                    tstamp = event_time_to_offset(SHUB_ACTIVE_BARO, tm_tmp);
/* SHMDS_HUB_0120_03 SHMDS_HUB_0120_10 mod S */
                    //data[0] = U8_TO_U24(&data_tmp[DATA_SIZE_BARO - 3]);
                    shub_baro_filter( &s_tLoggingBaroData, U8_TO_U24(&data_tmp[DATA_SIZE_BARO - 3]) );
                    data[0] = s_tLoggingBaroData.pressure;
/* SHMDS_HUB_0120_03 SHMDS_HUB_0120_10 mod E */
/* SHMDS_HUB_0120_02 mod E */
                    data[1] = tstamp.tv_sec;
                    data[2] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_BARO){
                        shub_input_report_baro(data);
                    }
                }
                break;
#endif
/* SHMDS_HUB_0120_01 add E */
            default:
                DBG(DBG_LV_ERROR, "logging data type non [[0x%02X]]\n", op_buf);
                break;
        }
/* SHMDS_HUB_0359_01 del S */
//      delTimestamp.acc      = 0;
//      delTimestamp.mag      = 0;
//      delTimestamp.gyro     = 0;
//      delTimestamp.orien    = 0;
//      delTimestamp.grav     = 0;
//      delTimestamp.linear   = 0;
//      delTimestamp.rot      = 0;
//      delTimestamp.rot_gyro = 0;
//      delTimestamp.rot_mag  = 0;
//      delTimestamp.pedocnt  = 0;
//      delTimestamp.pedocnt2 = 0;
//      delTimestamp.total_status2  = 0;
//      delTimestamp.total_status   = 0;
// /* SHMDS_HUB_0120_01 add S */
// #ifdef CONFIG_BARO_SENSOR   /* SHMDS_HUB_0120_09 mod */
//      delTimestamp.baro     = 0;
// #endif
// /* SHMDS_HUB_0120_01 add E */
/* SHMDS_HUB_0359_01 del E */
    }

    if(mag_ofs_first == 0){
        cmd.cmd.u16 = HC_MAG_GET_OFFSET;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_MAG_GET_OFFSET err(%x)\n", res.err.u16);
            s_tLatestMagData.nXOffset = 0;
            s_tLatestMagData.nYOffset = 0;
            s_tLatestMagData.nZOffset = 0;
        }else{
            s_tLatestMagData.nXOffset = res.res.s16[0];
            s_tLatestMagData.nYOffset = res.res.s16[1];
            s_tLatestMagData.nZOffset = res.res.s16[2];
        }
    }

    if(gyro_ofs_first == 0){
        cmd.cmd.u16 = HC_GYRO_GET_OFFSET;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GYRO_GET_OFFSET err(%x)\n", res.err.u16);
            s_tLatestGyroData.nXOffset = 0;
            s_tLatestGyroData.nYOffset = 0;
            s_tLatestGyroData.nZOffset = 0;
        }else{
            s_tLatestGyroData.nXOffset = res.res.s16[0];
            s_tLatestGyroData.nYOffset = res.res.s16[1];
            s_tLatestGyroData.nZOffset = res.res.s16[2];
            s_tLatestGyroData.nAccuracy = 3;
        }
    }
    kfree(buf_tmp);
    
/* SHMDS_HUB_0311_05 mod S */
    if(size == 0){
        update_base_time(ACTIVE_FUNC_MASK,NULL);
    }else{
        shub_update_time_flash(ACTIVE_FUNC_MASK);
    }
/* SHMDS_HUB_0311_05 mod E */
    
    return ret; 
}

static int32_t shub_read_sensor_data(int32_t arg_iSensType)
{
    int32_t  ret = SHUB_RC_OK;
    HostCmd cmd;
    HostCmdRes res;
    uint8_t data_tmp[16];
    int32_t X,Y,Z,S,ACC = 0;
    int32_t param[] = {0,0}; // SHMDS_HUB_0701_02 add

    mutex_lock(&s_tDataMutex);

    /*get ACC sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_ACC){

        ret = hostif_read(RSLT00, data_tmp, 6);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);

        s_tLatestAccData.nX = X;
        s_tLatestAccData.nY = Y;
        s_tLatestAccData.nZ = Z;

        // SHMDS_HUB_0701_02 add S
        if(DBG_LV_PEDO & dbg_level){
            cmd.cmd.u16 = HC_GET_PEDO_STEP_DATA;
            cmd.prm.u32[0x00] = 0x0000201;
            cmd.prm.u32[0x01] = 0x0000000;
            cmd.prm.u8[0x08] = 0x00;                            /* SHMDS_HUB_2603_01 add */
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);    /* SHMDS_HUB_2603_01 mod (8->9) */
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_DATA err(%x)\n", res.err.u16);
                return SHUB_RC_ERR;
            }
            param[0] =(int32_t)(uint32_t)RESU8_TO_X32(res,0);
            param[1] =(int32_t)(uint32_t)RESU8_TO_X32(res,4);
        }
        // SHMDS_HUB_0701_02 add E

        DBG(DBG_LV_DATA, "get ACC data X[%d] Y[%d] Z[%d] stab[%d] instab[%d] offset[%lld]\n",X, Y, Z, param[0], param[1], s_recovery_data.bk_OffsetStep);
    }

    /*get GYRO sensor data*/
    if(arg_iSensType & (SHUB_ACTIVE_GYRO | SHUB_ACTIVE_GYROUNC)){

        ret = hostif_read(RSLT06, data_tmp, 6);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);

        s_tLatestGyroData.nX = X;
        s_tLatestGyroData.nY = Y;
        s_tLatestGyroData.nZ = Z;
        DBG(DBG_LV_DATA, "get GYRO data X[%d] Y[%d] Z[%d] ACC[%d]\n", X, Y, Z, s_tLatestGyroData.nAccuracy);
    }

    /*get MAG sensor data*/
    if(arg_iSensType & (SHUB_ACTIVE_MAG | SHUB_ACTIVE_MAGUNC)){
        ret = hostif_read(RSLT0C, data_tmp, 8);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);
        ACC = data_tmp[6];

        s_tLatestMagData.nAccuracy = ACC;
        s_tLatestMagData.nX = X;
        s_tLatestMagData.nY = Y;
        s_tLatestMagData.nZ = Z;

        DBG(DBG_LV_DATA, "get MAG data X[%d] Y[%d] Z[%d] ACC[%d]\n", X, Y, Z, ACC);
    }

    /*get ORIENTATION sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_ORI){
        ret = hostif_read(RSLT13, data_tmp, 7);
        ACC = data_tmp[0];
        X = (int32_t)(int16_t)((uint32_t)data_tmp[1] | ((uint32_t)data_tmp[2] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[3] | ((uint32_t)data_tmp[4] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[5] | ((uint32_t)data_tmp[6] & 0xFF) << 8);

        s_tLatestOriData.pitch = X;
        s_tLatestOriData.roll = Y;
        s_tLatestOriData.yaw = Z;
        s_tLatestOriData.nAccuracy = ACC;
        DBG(DBG_LV_DATA, "get ORIENTATION data pitch[%d] roll[%d] yaw[%d] ACC[%d]\n", X, Y, Z, ACC);
    }

    /*get GRAVITY sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        ret = hostif_read(RSLT1A, data_tmp, 6);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);

        s_tLatestGravityData.nX = X;
        s_tLatestGravityData.nY = Y;
        s_tLatestGravityData.nZ = Z;

        DBG(DBG_LV_DATA, "get GRAVITY data X[%d] Y[%d] Z[%d]\n", X, Y, Z);
    }

    /*get LINEACC sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_LACC){
        ret = hostif_read(RSLT20, data_tmp, 6);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);

        s_tLatestLinearAccData.nX = X;
        s_tLatestLinearAccData.nY = Y;
        s_tLatestLinearAccData.nZ = Z;

        DBG(DBG_LV_DATA, "get LINEACC data X[%d] Y[%d] Z[%d]\n", X, Y, Z);
    }

    /*get ROTATION_VECTOR sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_RV){
        ret = hostif_read(RSLT26, data_tmp, 7);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);
        ACC = (int32_t)(int8_t)data_tmp[6];

        s_tLatestRVectData.nX = X;
        s_tLatestRVectData.nY = Y;
        s_tLatestRVectData.nZ = Z;
        s_tLatestRVectData.nS = 0;
        s_tLatestRVectData.nAccuracy = ACC;

        DBG(DBG_LV_DATA, "get ROTATION_VECTOR data X[%d] Y[%d] Z[%d] ACC[%d]\n", X, Y, Z, ACC);
    }

    /*get MAGROT sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        ret = hostif_read(RSLT2D, data_tmp, 9);

        ACC = data_tmp[0];
        X = (int32_t)(int16_t)((uint32_t)data_tmp[1] | ((uint32_t)data_tmp[2] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[3] | ((uint32_t)data_tmp[4] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[5] | ((uint32_t)data_tmp[6] & 0xFF) << 8);
        S = (int32_t)(int16_t)((uint32_t)data_tmp[7] | ((uint32_t)data_tmp[8] & 0xFF) << 8);

        s_tLatestGeoRVData.nX = X;
        s_tLatestGeoRVData.nY = Y;
        s_tLatestGeoRVData.nZ = Z;
        s_tLatestGeoRVData.nS = S;
        s_tLatestGeoRVData.nAccuracy = ACC; 

        DBG(DBG_LV_DATA, "get MAGROT data X[%d] Y[%d] Z[%d] S[%d] ACC[%d]\n", X, Y, Z, S, ACC);
    }

    /*get GAMEROT sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        ret = hostif_read(RSLT36, data_tmp, 8);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);
        S = (int32_t)(int16_t)((uint32_t)data_tmp[6] | ((uint32_t)data_tmp[7] & 0xFF) << 8);

        s_tLatestGameRVData.nX = X;
        s_tLatestGameRVData.nY = Y;
        s_tLatestGameRVData.nZ = Z;
        s_tLatestGameRVData.nS = S;

        DBG(DBG_LV_DATA, "get GAMEROT data X[%d] Y[%d] Z[%d] S[%d]\n", X, Y, Z, S);
    }

    /*get PEDO sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_PEDOM){
        /*get data*/
        cmd.cmd.u16 = HC_GET_PEDO_STEP_DATA;
        cmd.prm.u32[0] = 1;
        cmd.prm.u32[1] = 0;
        cmd.prm.u8[0x08] = 0x00;                            /* SHMDS_HUB_2603_01 add */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);    /* SHMDS_HUB_2603_01 mod (8->9) */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_DATA err(%x)\n", res.err.u16);
        }else{
//          s_tLatestStepCountData.step = (uint64_t)res.res.u32[0];
            s_tLatestStepCountData.step = (uint64_t)res.res.u32[0] + s_recovery_data.bk_OffsetStep; /* SHMDS_HUB_3301_01 mod */
            DBG(DBG_LV_DATA, "get PEDO data step[%lld]\n", s_tLatestStepCountData.step);
        }
    }

/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    /*get Barometric pressure sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        /*get data*/
        cmd.cmd.u16 = HC_SENSOR_GET_RESULT;
        cmd.prm.u16[0] = HC_BARO_VALID;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SENSOR_GET_RESULT err(%x)\n", res.err.u16);
        }else{
            /* SHMDS_HUB_0120_10 mod S */
            // s_tLatestBaroData.pressure = res.res.u32[0];
            // /* SHMDS_HUB_0120_02 mod S */
            // /*DBG(DBG_LV_DATA, "get BARO data[%d]\n", s_tLatestBaroData.pressure);*/
            // DBG(DBG_LV_DATA, "get BARO data baro[%d], temp[%d]\n", s_tLatestBaroData.pressure, RESU8_TO_X16(res,4));
            // /* SHMDS_HUB_0120_02 mod E */
            shub_baro_filter( &s_tLatestBaroData, res.res.u32[0] );
            DBG(DBG_LV_DATA, "get BARO data baro[%d], filter[%d], temp[%d]\n", res.res.u32[0], s_tLatestBaroData.pressure, RESU8_TO_X16(res,4));
            /* SHMDS_HUB_0120_10 mod E */
        }
    }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */

    mutex_unlock(&s_tDataMutex);

    return ret;
}

#ifndef SIM_HOST
//static int32_t shub_hostcmd(const HostCmd *prm, HostCmdRes *res, uint8_t mode, uint8_t size)   // SHMDS_HUB_1001_01 mod
static int32_t shub_hostcmd_exe(const HostCmd *prm, HostCmdRes *res, uint8_t mode, uint8_t size) // SHMDS_HUB_1001_01 mod
{
    int32_t ret = SHUB_RC_OK;
    uint8_t reg[20];
    uint8_t i;

    g_hostcmdErr=0;
#ifdef NO_HOST
    res->err.u16= 0;
    return 0;
#endif

    mutex_lock(&s_hostCmdMutex);
    
/* SHMDS_HUB_3301_01 add S */
    if(shub_recovery_flg) {
        DBG(DBG_LV_ERROR, "%s : recovery req err!\n", __FUNCTION__);
        goto ERROR;
    }
/* SHMDS_HUB_3301_01 add E */
    
    res->res_size=0;
    memset(reg, 0, sizeof(reg));
    if(size > sizeof(prm->prm.u8)){
        size = sizeof(prm->prm.u8);
    }

    reg[0] = prm->cmd.u8[0];
    reg[1] = prm->cmd.u8[1];
    for(i = 0;i < size;i++){
        reg[2 + sizeof(prm->prm.u8) - i - 1] = prm->prm.u8[i];
    }
    reg[19] = 1;

    spin_lock( &s_intreqData );
    g_nIntIrqFlg = 0;
    spin_unlock( &s_intreqData );

/* SHMDS_HUB_0322_05 mod S */
    ret =  hostif_write(CMD0, reg, 2);
    if(ret != SHUB_RC_OK){
        DBG(DBG_LV_ERROR, "HostCmd error(0x%02x, ret=%x)", CMD0,ret);
        goto ERROR;
    }
    ret = hostif_write(PRM0, &reg[2], 0x10);
    if(ret != SHUB_RC_OK){
        DBG(DBG_LV_ERROR, "HostCmd error(0x%02x, ret=%x)", PRM0,ret);
        goto ERROR;
    }
    ret = hostif_write(CMDENTRY, &reg[19], 1);
    if(ret != SHUB_RC_OK){
        DBG(DBG_LV_ERROR, "HostCmd error(0x%02x, ret=%x)", CMDENTRY,ret);
        goto ERROR;
    }
/* SHMDS_HUB_0322_05 mod E */

    if((mode & EXE_HOST_WAIT) == EXE_HOST_WAIT){
        if((mode & EXE_HOST_FUP_ERASE) == EXE_HOST_FUP_ERASE){    /* SHMDS_HUB_0343_01 mod */
            ret = shub_waitcmd(SHUB_SEND_CMD_FUP_ERASE);
        }else{
            ret = shub_waitcmd(SHUB_SEND_CMD_HOST);
        }
        if(ret != SHUB_RC_OK) {
/* SHMDS_HUB_0703_01 add S */
            if(ret == SHUB_RC_ERR_TIMEOUT){
                collect_hostcmd_timeout_log(prm->cmd.u16);
            }
/* SHMDS_HUB_0703_01 add E */
            if(((mode & EXE_HOST_EX_NO_RECOVER) == EXE_HOST_EX_NO_RECOVER) && 
                    (ret == SHUB_RC_ERR_TIMEOUT)){
                goto ERROR;
            }
            DBG(DBG_LV_ERROR, "hostcmd timout error\n");
            ERR_WAKEUP;
            goto ERROR;
        }
    }

    if((mode & EXE_HOST_RES_ONLY_FIFO_SIZE) == EXE_HOST_RES_ONLY_FIFO_SIZE){
        uint8_t tmp[2];
        ret = hostif_read(RSLT3E, tmp, 2);
        if(ret != SHUB_RC_OK) {
            goto ERROR;
        }
        res->res_size = (tmp[0] | (tmp[1] << 0x08));
        if(res->res_size > sizeof(res->res.u8)){
            res->res_size = sizeof(res->res.u8);
        }
    }

    if((mode & EXE_HOST_RES) == EXE_HOST_RES){
        uint8_t tmp[2];
        ret = hostif_read(RSLT3E, tmp, 2);
        if(ret != SHUB_RC_OK) {
            goto ERROR;
        }
        res->res_size = (tmp[0] | (tmp[1] << 0x08));
        if(res->res_size > sizeof(res->res.u8)){
            res->res_size = sizeof(res->res.u8);
        }
        if(res->res_size != 0){
            ret = hostif_read(FIFO, res->res.u8, res->res_size);
            if(ret != SHUB_RC_OK) {
                goto ERROR;
            }
        }
    }

    if((mode & EXE_HOST_RES_RSLT) == EXE_HOST_RES_RSLT){
        res->res_size = 64;
        ret = hostif_read(RSLT00, res->res.u8, 64);
        if(ret != SHUB_RC_OK) {
            goto ERROR;
        }
    }

    if((mode & EXE_HOST_ERR) == EXE_HOST_ERR){
        // shub_int_work_func
        res->err.u16= g_hostcmdErr;
    }

ERROR:
    if((mode & EXE_HOST_SKIP_MUTEX_UNLOCK) != EXE_HOST_SKIP_MUTEX_UNLOCK){
        mutex_unlock(&s_hostCmdMutex);
    }
    return ret;
}

// SHMDS_HUB_1001_01 add S
static int32_t shub_hostcmd(const HostCmd *prm, HostCmdRes *res, uint8_t mode, uint8_t size)
{
    int32_t ret = SHUB_RC_OK;
    int32_t i;
    
    DBG(DBG_LV_CMD, "%s : cmd=0x%04x \n", __func__, prm->cmd.u16); /* SHMDS_HUB_0701_06 add */
    
    for(i=0; i<SHUB_CMD_RETRY_NUM; i++)
    {
        ret = shub_hostcmd_exe(prm, res, mode, size);
        if(((mode & EXE_HOST_ERR) == EXE_HOST_ERR) && (res->err.u16 != 0)) {
            if(res->err.u16 == 0x0FFE) {
                if(i >= (SHUB_CMD_RETRY_NUM - 1)){
                    DBG(DBG_LV_ERROR, "%s : RetryOver!![%d](ret=0x%x, err=0x%x, cmd=0x%x, mode=0x%x, size=%d)\n",
                        __func__, i, ret, res->err.u16, prm->cmd.u16, mode, size);
                }else{
                    DBG(DBG_LV_ERROR, "%s : Retry[%d](ret=0x%x, err=0x%x, cmd=0x%x, mode=0x%x, size=%d)\n",
                        __func__, i, ret, res->err.u16, prm->cmd.u16, mode, size);
                    usleep(1 * 1000);
                }
            }else{
                DBG(DBG_LV_ERROR, "%s : Error(ret=0x%x, err=0x%x, cmd=0x%x, mode=0x%x, size=%d)\n",
                    __func__, ret, res->err.u16, prm->cmd.u16, mode, size);
                i = SHUB_CMD_RETRY_NUM;
//              break;
            }
        }else if(ret == SHUB_RC_OK) {
            return ret;
            
        }else{
            DBG(DBG_LV_ERROR, "%s : Error(ret=0x%x, err=0x%x, cmd=0x%x, mode=0x%x, size=%d)\n",
                __func__, ret, res->err.u16, prm->cmd.u16, mode, size);
            i = SHUB_CMD_RETRY_NUM;
//          break;
        }
    }
    return ret;
}
// SHMDS_HUB_1001_01 add E

#endif

static int32_t shub_waitcmd(uint16_t intBit)
{
    int32_t ret = SHUB_RC_ERR_TIMEOUT;
    int32_t result = 0;
    long timeout;
    int32_t retry = 300;

    if(intBit == SHUB_SEND_CMD_FUP_ERASE){    /* SHMDS_HUB_0343_01 mod */
        timeout = msecs_to_jiffies(WAIT_CHECK_FUP_ERASE);
    }else{
        timeout = msecs_to_jiffies(WAITEVENT_TIMEOUT);
    }

    while(retry){

/* SHMDS_HUB_3301_01 mod S */
        result = wait_event_interruptible_timeout(s_tWaitInt, (g_nIntIrqFlg & (INTREQ_HOST_CMD | INTFLG_ERROR | INTFLG_WATCHDOG)), timeout);
        if( g_nIntIrqFlg & INTFLG_WATCHDOG ){
            spin_lock( &s_intreqData );
            g_nIntIrqFlg &= ~INTFLG_WATCHDOG;
            spin_unlock( &s_intreqData );

            DBG(DBG_LV_INT, "INTREQ0/1 -Wdt Error- \n");
            ret = SHUB_RC_ERR;
            break;

        }else if( g_nIntIrqFlg & INTFLG_ERROR ){
            spin_lock( &s_intreqData );
            g_nIntIrqFlg &= ~INTFLG_ERROR;
            spin_unlock( &s_intreqData );

            DBG(DBG_LV_INT, "INTREQ0/1 -Error- \n");
            ret = SHUB_RC_ERR;
            break;
/* SHMDS_HUB_3301_01 mod E */

        }else if( g_nIntIrqFlg & INTREQ_HOST_CMD ){
            spin_lock( &s_intreqData );
            g_nIntIrqFlg &= ~INTREQ_HOST_CMD;
            spin_unlock( &s_intreqData );

            ret = SHUB_RC_OK;
            DBG(DBG_LV_HOSTIF, "Wakeup Event... \n");
            break;
        }

        if( result == -ERESTARTSYS ) {
            DBG(DBG_LV_HOSTIF, "wait event signal received. retry = %d, g_nIntIrqFlg = %x \n", retry, g_nIntIrqFlg);
            usleep(10 * 1000); /* SHMDS_HUB_0102_11 mod */
        }

        if( result == 0 ){
            ret = SHUB_RC_ERR_TIMEOUT;
            DBG(DBG_LV_ERROR, "wait event timeout... %x \n", g_nIntIrqFlg);
            break;
        }
        retry--;
    }
    return ret;
}

// PA6 hostif interrupt
static irqreturn_t shub_irq_handler(int32_t irq, void *dev_id)
{
    shub_wake_lock_start(&shub_irq_wake_lock);      // SHMDS_HUB_0402_01 add
    if( irq != g_nIntIrqNo ){
        shub_wake_lock_end(&shub_irq_wake_lock);    // SHMDS_HUB_0402_01 add
        return IRQ_NONE;
    }

/* SHMDS_HUB_0312_01 add  S */
    if(shub_basetime_req == 1){
        shub_baseTime = timespec_to_ktime(shub_get_timestamp());
        shub_basetime_req = 0;
    }
/* SHMDS_HUB_0312_01 add  E */

    DISABLE_IRQ;
    shub_wake_lock_start(&shub_int_wake_lock);      // SHMDS_HUB_0402_01 add
    if( shub_workqueue_create(accsns_wq_int, shub_int_work_func) != SHUB_RC_OK){
        shub_wake_lock_end(&shub_int_wake_lock);    // SHMDS_HUB_0402_01 add
        ENABLE_IRQ;
    }
    shub_dbg_cnt_irq++;                             // SHMDS_HUB_0701_03 add

    shub_wake_lock_end(&shub_irq_wake_lock);        // SHMDS_HUB_0402_01 add
    return IRQ_HANDLED;
}

#ifndef SIM_HOST
// PA6 hostif interrupt substance
static void shub_int_work_func(struct work_struct *work)
{
    uint8_t    cmd_reg_clear[2]={0};
    uint8_t    err_intreq[4]={0};    /* SHMDS_HUB_0346_01 mod */
    uint16_t   intreq;
    int32_t    iCurrentEnable;

    iCurrentEnable = atomic_read(&g_CurrentSensorEnable);

    hostif_read(ERROR0, err_intreq, 4);
    g_hostcmdErr = (uint16_t)((uint16_t)err_intreq[0] | (uint16_t)(err_intreq[1] << 8));

    intreq = (uint16_t)(err_intreq[2] | (err_intreq[3] << 8));

    shub_dbg_collect_irq_log(intreq);               // SHMDS_HUB_0701_03 add

/* SHMDS_HUB_3301_01 mod S */
    if((intreq == 0) || (((intreq & ~INTREQ_MASK) != 0) && (intreq != INTREQ_ERROR) && (intreq != INTREQ_WATCHDOG))){
        DBG(DBG_LV_INT, "### shub_int_work_func : Error1 %x\n", intreq); // SHMDS_HUB_0701_01 add
        goto ERROR;
    }

    DBG(DBG_LV_INT, "### INTREQ0/1=%x iCurrentEnable=%x \n", intreq, iCurrentEnable); // SHMDS_HUB_0701_01 mod
    if(intreq == INTREQ_WATCHDOG){
        DBG(DBG_LV_ERROR, "### shub_int_work_func Error %x\n", intreq);
        shub_recovery_flg = 1;
        shub_wake_lock_start(&shub_recovery_wake_lock);
        schedule_work(&recovery_irq_work);
        spin_lock( &s_intreqData );
        g_nIntIrqFlg |= INTFLG_WATCHDOG;
        spin_unlock( &s_intreqData );
        wake_up_interruptible(&s_tWaitInt);
        shub_workqueue_delete(work);
//      ENABLE_IRQ;
        shub_wake_lock_end(&shub_int_wake_lock);             // SHMDS_HUB_0402_01 add
        return;
    }else if(intreq == INTREQ_ERROR){
        DBG(DBG_LV_ERROR, "### shub_int_work_func Error %x\n", intreq);
        spin_lock( &s_intreqData );
        g_nIntIrqFlg |= INTFLG_ERROR;
        spin_unlock( &s_intreqData );
        wake_up_interruptible(&s_tWaitInt);
        goto ERROR;
    }
/* SHMDS_HUB_3301_01 mod E */

    if(intreq & INTREQ_HOST_CMD){
        //DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_HOST_CMD g_nIntIrqFlg:%x \n", g_nIntIrqFlg);
        if(!(g_nIntIrqFlg & INTREQ_HOST_CMD)){
            spin_lock( &s_intreqData );
            g_nIntIrqFlg |= INTREQ_HOST_CMD;
            spin_unlock( &s_intreqData );
            // DBG(DBG_LV_ERROR, "CMD:%04x\n",s_lsi_id);
            if(s_lsi_id != LSI_ML630Q791){
                hostif_write(CMD0, cmd_reg_clear, sizeof(cmd_reg_clear));
            }
            wake_up_interruptible(&s_tWaitInt);
        }
    }

    if(intreq & INTREQ_ACC){
        DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_ACC iCurrentEnable:%x \n", iCurrentEnable);
        shub_wake_lock_start(&shub_acc_wake_lock);       // SHMDS_HUB_0402_01 add
        schedule_work(&acc_irq_work);
    }

    if(intreq & INTREQ_GYRO){
        DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_GYRO iCurrentEnable:%x \n", iCurrentEnable);
        shub_wake_lock_start(&shub_gyro_wake_lock);      // SHMDS_HUB_0402_01 add
        schedule_work(&gyro_irq_work);
    }

    if(intreq & INTREQ_MAG){
        DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_MAG iCurrentEnable:%x \n", iCurrentEnable);
        shub_wake_lock_start(&shub_mag_wake_lock);       // SHMDS_HUB_0402_01 add
        schedule_work(&mag_irq_work);
    }

/* SHMDS_HUB_2603_01 add S */
    if(intreq & INTREQ_BARO){
        DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_BARO iCurrentEnable:%x \n", iCurrentEnable);
    }
/* SHMDS_HUB_2603_01 add E */

    if(intreq & INTREQ_CUSTOMER){
        DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_CUSTOMER iCurrentEnable:%x \n", iCurrentEnable);
        shub_wake_lock_start(&shub_customer_wake_lock);  // SHMDS_HUB_0402_01 add
        schedule_work(&customer_irq_work);
    }

    if((intreq & INTREQ_FUSION) == INTREQ_FUSION){
        shub_wake_lock_start(&shub_fusion_wake_lock);    // SHMDS_HUB_0402_01 add
        schedule_work(&fusion_irq_work);
    }

ERROR:
    shub_workqueue_delete(work);
    ENABLE_IRQ;
    shub_wake_lock_end(&shub_int_wake_lock);             // SHMDS_HUB_0402_01 add
    return;
}
#endif

static void shub_significant_work_func(struct work_struct *work)
{
    int32_t iCurrentEnable;
    uint8_t notify=0;
    int32_t data[3]={0};
    iCurrentEnable = atomic_read(&g_CurrentSensorEnable);

    if(iCurrentEnable & SHUB_ACTIVE_SIGNIFICANT){
        DBG(DBG_LV_INT, "### SIGNIFICANT\n");
        shub_activate_significant_exec(SHUB_ACTIVE_SIGNIFICANT, 0, &notify);
        shub_get_sensors_data(SHUB_ACTIVE_SIGNIFICANT, data);
        shub_input_report_significant(data);
    }
}

static void shub_int_acc_work_func(struct work_struct *work)
{
    int32_t ret;
    int32_t iCurrentEnable;
    int32_t iCurrentLoggingEnable;
    int32_t iWakeupSensor;
    HostCmd cmd;
    HostCmdRes res;
    int32_t data[5]={0};        /* SHMDS_HUB_0132_01 mod (3->5) */
    uint32_t intdetail=0;
    uint32_t intdetail2=0;
    uint8_t notify=0;
    int32_t rideInfo[7];        // SHMDS_HUB_0209_02 add
    bool sendInfo;              // SHMDS_HUB_0209_02 add
    uint16_t rdata[2];          /* SHMDS_HUB_1701_01 add */
#ifdef SHUB_SW_PROX_CHECKER     // SHMDS_HUB_1701_17 mod
    int err;                    /* SHMDS_HUB_1701_14 add */
#ifdef SHUB_SW_TIME_API
    struct timespec tv;         /* SHMDS_HUB_1701_18 add */
    struct timespec stop;       /* SHMDS_HUB_1701_18 add */
    struct timespec df;         /* SHMDS_HUB_1701_18 add */
    u64 msec_api;               /* SHMDS_HUB_1701_18 add */
    u64 usec_api;               /* SHMDS_HUB_1701_18 add */
#endif /* SHUB_SW_TIME_API */
#endif /* SHUB_SW_PROX_CHECKER */   // SHMDS_HUB_1701_17 mod
    int prox_data = 7;          /* SHMDS_HUB_1701_14 add */
    uint8_t pickup_mask = 0xFF; /* SHMDS_HUB_1702_01 add */

    DBG(DBG_LV_INT, "### shub_int_acc_work_func In \n");

    cmd.cmd.u16 = HC_MCU_GET_INT_DETAIL;
    cmd.prm.u16[0] = INTREQ_ACC >> 1;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_INT_DETAIL err(%x)\n", res.err.u16);
        shub_wake_lock_end(&shub_acc_wake_lock);    // SHMDS_HUB_0402_01 add
        return;
    }
    iCurrentEnable = atomic_read(&g_CurrentSensorEnable);
    iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);

    intdetail = res.res.u8[0] |
        ((res.res.u8[1] << 8 ) & 0x0000ff00) | 
        ((res.res.u8[2] << 16) & 0x00ff0000) | 
        ((res.res.u8[3] << 24) & 0xff000000); 
    intdetail2 = res.res.u8[4] |
        ((res.res.u8[5] << 8 ) & 0x0000ff00) |
        ((res.res.u8[6] << 16) & 0x00ff0000);   /* SHMDS_HUB_0132_01 add */

    DBG(DBG_LV_INT, "### !! Acc Event !! %x %x\n",intdetail, intdetail2);
    iWakeupSensor = atomic_read(&g_WakeupSensor);
    atomic_set(&g_WakeupSensor,iWakeupSensor | SHUB_ACTIVE_PEDOM);

    if((iCurrentEnable & SHUB_ACTIVE_PEDOM) && 
            (iCurrentLoggingEnable & SHUB_ACTIVE_PEDOM) == 0){
        if(intdetail2 & INTDETAIL_PEDOM_CNT){
            shub_get_sensors_data(SHUB_ACTIVE_PEDOM, data);
            shub_input_report_stepcnt(data);
            shub_timer_wake_lock_start();            // SHMDS_HUB_0402_01 add
        }
    }

    if((iCurrentEnable & SHUB_ACTIVE_PEDODEC) && 
            (iCurrentLoggingEnable & SHUB_ACTIVE_PEDODEC) == 0){
        if(intdetail2 & INTDETAIL_PEDOM_CNT){
            shub_get_sensors_data(SHUB_ACTIVE_PEDODEC, data);
            shub_input_report_stepdetect(data);
            shub_timer_wake_lock_start();            // SHMDS_HUB_0402_01 add
        }
    }

    if(iCurrentEnable & SHUB_ACTIVE_SIGNIFICANT){
        int32_t enable=0;
        if(intdetail2 & INTDETAIL_PEDOM_SIGNIFICANT){
            enable=1;
        }else if(intdetail2 & INTDETAIL_PEDOM_TOTAL_STATE){
            cmd.cmd.u16 = HC_GET_ACTIVITY_TOTAL_DETECT_DATA;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,0);
            if(ret != SHUB_RC_OK) {
                DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_DATA err(%x)\n", res.err.u16);
            }
            if(res.res.u8[0] == 1){
                //the continuation of steps
                enable=1;
            }
        }
        if(enable == 1){
            shub_activate_significant_exec(SHUB_ACTIVE_SIGNIFICANT, 0, &notify);
            shub_get_sensors_data(SHUB_ACTIVE_SIGNIFICANT, data);
            shub_input_report_significant(data);
            shub_timer_wake_lock_start();           // SHMDS_HUB_0402_01 add
        }
    }

/* SHMDS_HUB_1701_01 add S */
    if(intdetail2 & INTDETAIL2_PEDOM_PICKUP){
        DBG(DBG_LV_INT, "### !! PickUp !!\n");
        
        cmd.cmd.u16 = HC_PICKUP_GET_ISR;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,8);
        if(ret != SHUB_RC_OK) {
            DBG(DBG_LV_ERROR, "HC_PICKUP_GET_ISR err(%x)\n", res.err.u16);
            shub_wake_lock_end(&shub_fusion_wake_lock);    // SHMDS_HUB_0402_01 add
        }else{
            rdata[0] = (uint16_t)(res.res.u8[4] | (res.res.u8[5] & 0xFF) << 8);
            rdata[1] = (uint16_t)(res.res.u8[6] | (res.res.u8[7] & 0xFF) << 8);
            DBG(DBG_LV_INT, "PickUp isr=%02x, en=%d, pk=%d, lv=%d, cnt=%d, rost=%d\n",
                res.res.u8[0],res.res.u8[1],res.res.u8[2],res.res.u8[3],rdata[0],rdata[1]);
        }
/* SHMDS_HUB_1701_14 mod S */
//      shub_input_report_exif_pickup_det();
//      shub_timer_wake_lock_start();              // SHMDS_HUB_0402_01 add
/* SHMDS_HUB_1701_15 mod S */
        
#ifdef CONFIG_PICKUP_PROX
        pickup_mask &= ~SHUB_PICKUP_ENABLE_PARAM_STILL;
#endif
#ifdef SHUB_SW_PICKUP_ALGO_03 /* SHMDS_HUB_1702_01 add */
        pickup_mask &= ~SHUB_PICKUP_ENABLE_ALGO_03;
#endif
#ifdef SHUB_SW_PROX_CHECKER     // SHMDS_HUB_1701_17 mod
        if((shub_pickup_setflg & pickup_mask) != SHUB_PICKUP_ENABLE_PARAM_LEVEL) {
#ifdef SHUB_SW_TIME_API /* SHMDS_HUB_1701_18 add */
            shub_dbg_timer_start(&tv);
#endif  /* SHUB_SW_TIME_API */
#ifdef SHUB_SW_PROX_TYPE_API // SHMDS_HUB_1701_19 add
            DBG(DBG_LV_INFO, "[shub][%s][PROX_dataread_func]start\n", __func__);    /* SHMDS_HUB_1701_18 add */
            err = PROX_dataread_func(&prox_data);  // SHMDS_HUB_1701_16 mod
            DBG(DBG_LV_INFO, "[shub][%s][PROX_dataread_func]end\n", __func__);  /* SHMDS_HUB_1701_18 add */
#endif  /* SHUB_SW_PROX_TYPE_API */ // SHMDS_HUB_1701_19 add
#ifdef SHUB_SW_PROX_TYPE_SYSFS // SHMDS_HUB_1701_19 add
            DBG(DBG_LV_INFO, "[shub][%s][shub_prox_distance]start\n", __func__);  // SHMDS_HUB_1701_19 add
            err = shub_prox_distance(&prox_data);  // SHMDS_HUB_1701_19 add
            DBG(DBG_LV_INFO, "[shub][%s][shub_prox_distance]end\n", __func__);  // SHMDS_HUB_1701_19 add
#endif  /* SHUB_SW_PROX_TYPE_SYSFS */ // SHMDS_HUB_1701_19 add
/* SHMDS_HUB_1701_18 add S */
#ifdef SHUB_SW_TIME_API
            getnstimeofday(&stop);
            df = timespec_sub(stop, tv);
            msec_api = timespec_to_ns(&df);
            do_div(msec_api, NSEC_PER_USEC);
            usec_api = do_div(msec_api, USEC_PER_MSEC);
            DBG(DBG_LV_INFO, "[shub][%s][PROX_dataread_func]total=%lu.%03lums\n", __func__, (unsigned long)msec_api, (unsigned long)usec_api);
#endif  /* SHUB_SW_TIME_API */
/* SHMDS_HUB_1701_18 add E */
            if(err < 0) {
                DBG(DBG_LV_ERROR, "[%s][%d]prox_distance err. !! \n", __func__, __LINE__ ) ;
                prox_data = 7;
            }
        }
#endif /* SHUB_SW_PROX_CHECKER */   // SHMDS_HUB_1701_17 mod
/* SHMDS_HUB_1701_15 mod E */
        if(prox_data == 7) {
            shub_input_report_exif_pickup_det();
            shub_timer_wake_lock_start();              // SHMDS_HUB_0402_01 add
        }else{
            DBG(DBG_LV_INT, "PickUp prox No Event!!\n");
        }
/* SHMDS_HUB_1701_14 mod E */
    }
/* SHMDS_HUB_1701_01 add E */

/* SHMDS_HUB_0132_01 add S */
    if(intdetail2 & INTDETAIL2_PEDOM_DEVICE_ORI){
        DBG(DBG_LV_INT, "### !! Device Orientation !!\n");
        // Get info
        cmd.cmd.u16 = HC_ACC_GET_ANDROID_XY_INFO;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if(ret != SHUB_RC_OK) {
            DBG(DBG_LV_ERROR, "HC_ACC_GET_ANDROID_XY_INFO err(%x)\n", res.err.u16);
        }else{
            DBG(DBG_LV_INT, "Dev Ori=%d, %d\n", res.res.u8[0],res.res.u8[1]);
        }
        // Report
        shub_get_sensors_data(SHUB_ACTIVE_DEVICE_ORI, data);
        data[0] = res.res.u8[0];
        data[1] = res.res.u8[1];
        shub_input_report_devoriect(data);
        shub_timer_wake_lock_start();
        // Isr clear
//      cmd.cmd.u16 = HC_ACC_CLEAR_ANDROID_XY;
//      ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
//      if(ret != SHUB_RC_OK) {
//          DBG(DBG_LV_ERROR, "HC_ACC_CLEAR_ANDROID_XY err(%x)\n", res.err.u16);
//      }
    }
/* SHMDS_HUB_0132_01 add E */

    sendInfo = !(intdetail & (INTDETAIL2_PEDOM_TOTAL_STATE | INTDETAIL2_PEDOM_RIDE_PAUSE));     // SHMDS_HUB_0209_02 add
    if(intdetail & INTDETAIL_GDETECT){
        DBG(DBG_LV_INT, "### !! G detection !!\n");
/* SHMDS_HUB_0201_01 add S */
        shub_input_report_exif_grav_det(sendInfo);
/* SHMDS_HUB_0201_01 add E */
        shub_timer_wake_lock_start();              // SHMDS_HUB_0402_01 add
    }

/* SHMDS_HUB_3801_02 add S */
#ifdef SHUB_SW_FREE_FALL_DETECT
    if(intdetail2 & INTDETAIL2_PEDOM_FREE_FALL1){
        DBG(DBG_LV_INT, "### !! Free Fall detection !!\n");
    }
    if(intdetail2 & INTDETAIL2_PEDOM_FREE_FALL2){
        DBG(DBG_LV_INT, "### !! Free Fall 2s !!\n");
        // get freefall data
        ret = shub_get_free_fall_data();
        if(ret == SHUB_FALL_RC_OVER) {
            // sensor stop
            data[0] = 0;
            ret = shub_set_param_exec(APP_FREE_FALL_ENABLE, data);
            if(ret != SHUB_RC_OK) {
                DBG(DBG_LV_ERROR, "APP_FREE_FALL_ENABLE err(%d)\n", ret);
            }
        }else{
            // Restart
            cmd.cmd.u16 = HC_FALL_REQ_RESTART;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,0);
            if(ret != SHUB_RC_OK) {
                DBG(DBG_LV_ERROR, "HC_FALL_REQ_RESTART err(%x)\n", res.err.u16);
            }
        }
    }
#endif
/* SHMDS_HUB_3801_02 add E */

/* SHMDS_HUB_0209_02 add S */
    sendInfo = !(intdetail & INTDETAIL2_PEDOM_TOTAL_STATE);
    if(intdetail & INTDETAIL2_PEDOM_RIDE_PAUSE) {
        mutex_lock(&userReqMutex);
        ret = shub_get_data_app_exec(APP_VEICHLE_DETECTION, rideInfo);
        mutex_unlock(&userReqMutex);
        if(ret != SHUB_RC_OK) {
            DBG(DBG_LV_ERROR, "INTDETAIL2_PEDOM_RIDE_PAUSE notify info get err.\n");
        } else {
            shub_input_report_exif_ride_pause_det(sendInfo, rideInfo[6]);
            shub_timer_wake_lock_start();
        }
    }
/* SHMDS_HUB_0209_02 add E */

/* SHMDS_HUB_0201_01 add S */
    if(intdetail & INTDETAIL2_PEDOM_TOTAL_STATE){
        shub_input_report_exif_judge();
        shub_timer_wake_lock_start();             // SHMDS_HUB_0402_01 add
    }
/* SHMDS_HUB_0201_01 add E */
    shub_workqueue_delete(work);
    shub_wake_lock_end(&shub_acc_wake_lock);      // SHMDS_HUB_0402_01 add
    return;
}

static void shub_int_mag_work_func(struct work_struct *work)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    DBG(DBG_LV_INT, "### shub_int_mag_work_func In \n");

    cmd.cmd.u16 = HC_MCU_GET_INT_DETAIL;
    cmd.prm.u16[0] = INTREQ_MAG >> 1;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_INT_DETAIL err(%x)\n", res.err.u16);
        shub_wake_lock_end(&shub_mag_wake_lock);   // SHMDS_HUB_0402_01 add
        return;
    }

    if(res.res.u8[0] == 0x01){
        cmd.cmd.u16 = HC_MAG_GET_OFFSET;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        mutex_lock(&s_tDataMutex);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_MAG_GET_OFFSET err(%x)\n", res.err.u16);
            s_tLatestMagData.nXOffset = 0;
            s_tLatestMagData.nYOffset = 0;
            s_tLatestMagData.nZOffset = 0;
        }else{
            s_tLatestMagData.nXOffset = res.res.s16[0];
            s_tLatestMagData.nYOffset = res.res.s16[1];
            s_tLatestMagData.nZOffset = res.res.s16[2];

            DBG(DBG_LV_INFO, "get MAG Cal data X[%d] Y[%d] Z[%d]\n",
                    s_tLatestMagData.nXOffset,
                    s_tLatestMagData.nYOffset, 
                    s_tLatestMagData.nZOffset);
        }
        mutex_unlock(&s_tDataMutex);
        DBG(DBG_LV_INFO, "[DBG] Update Mag offset ");
    }else{
        DBG(DBG_LV_ERROR, "[DBG] other Mag IRQ detail(%x)\n", res.res.u8[0]);
    }
    shub_wake_lock_end(&shub_mag_wake_lock);    // SHMDS_HUB_0402_01 add
} 

static void shub_int_customer_work_func(struct work_struct *work)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    DBG(DBG_LV_INT, "### shub_int_customer_work_func In \n");

    cmd.cmd.u16 = HC_MCU_GET_INT_DETAIL;
    cmd.prm.u16[0] = INTREQ_CUSTOMER;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_INT_DETAIL err(%x)\n", res.err.u16);
        shub_wake_lock_end(&shub_customer_wake_lock);    // SHMDS_HUB_0402_01 add
        return;
    }
/* SHMDS_HUB_0201_01 add S */
    if(res.res.u8[1] & 0x03) {
        shub_input_report_exif_mot_det(res.res.u8[1]);
        shub_timer_wake_lock_start();                   // SHMDS_HUB_0402_01 add
    }
/* SHMDS_HUB_0201_01 add E */

    DBG(DBG_LV_INT, "### !! Motion detection !! %x %x\n", res.res.u8[0], res.res.u8[1]);
    shub_wake_lock_end(&shub_customer_wake_lock);    // SHMDS_HUB_0402_01 add
} 

static void shub_int_gyro_work_func(struct work_struct *work)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;
#ifdef SHUB_SW_TWIST_DETECT
    uint16_t rdata[3];          /* SHMDS_HUB_2301_01 add */
    uint8_t info;               /* SHMDS_HUB_2301_01 add */
#endif

    DBG(DBG_LV_INT, "### shub_int_gyro_work_func In \n");

    cmd.cmd.u16 = HC_MCU_GET_INT_DETAIL;
    cmd.prm.u16[0] = INTREQ_GYRO >> 1;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_INT_DETAIL err(%x)\n", res.err.u16);
        shub_wake_lock_end(&shub_gyro_wake_lock);    // SHMDS_HUB_0402_01 add
        return;
    }

    if(res.res.u8[0] & INT_GYRO_CALIB){ /* SHMDS_HUB_2301_01 mod */
        cmd.cmd.u16 = HC_GYRO_GET_OFFSET;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        mutex_lock(&s_tDataMutex);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GYRO_GET_OFFSET err(%x)\n", res.err.u16);
            s_tLatestGyroData.nXOffset = 0;
            s_tLatestGyroData.nYOffset = 0;
            s_tLatestGyroData.nZOffset = 0;
        }else{
            s_tLatestGyroData.nXOffset = res.res.s16[0];
            s_tLatestGyroData.nYOffset = res.res.s16[1];
            s_tLatestGyroData.nZOffset = res.res.s16[2];
            s_tLatestGyroData.nAccuracy = 3;
            DBG(DBG_LV_INFO, "get GYRO Cal data X[%d] Y[%d] Z[%d] Acc[%d]\n",
                    s_tLatestGyroData.nXOffset,
                    s_tLatestGyroData.nYOffset, 
                    s_tLatestGyroData.nZOffset,
                    s_tLatestGyroData.nAccuracy
               );
        }
        mutex_unlock(&s_tDataMutex);
        DBG(DBG_LV_INFO, "[DBG] Update Gyro offset\n");
/* SHMDS_HUB_2301_01 add S */
#ifdef SHUB_SW_TWIST_DETECT
    }else if(res.res.u8[0] & (INT_GYRO_TWIST_LEFT | INT_GYRO_TWIST_RIGHT)) {
        info = (res.res.u8[0] & (INT_GYRO_TWIST_LEFT | INT_GYRO_TWIST_RIGHT));
        DBG(DBG_LV_INT, "### !! Twist isr=%d !!\n", res.res.u8[0]);
        
        cmd.cmd.u16 = HC_TWIST_GET_ISR;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,9);
        if(ret != SHUB_RC_OK) {
            DBG(DBG_LV_ERROR, "HC_TWIST_GET_ISR err(%x)\n", res.err.u16);
            shub_wake_lock_end(&shub_fusion_wake_lock);    // SHMDS_HUB_0402_01 add
        }else{
            rdata[0] = (uint16_t)(res.res.u8[2] | (res.res.u8[3] & 0xFF) << 8);
            rdata[1] = (uint16_t)(res.res.u8[4] | (res.res.u8[5] & 0xFF) << 8);
            rdata[2] = (uint16_t)(res.res.u8[6] | (res.res.u8[7] & 0xFF) << 8);
            DBG(DBG_LV_INT, "Twist isr=%02x, en=%d, Left=%d, Right=%d, rost=%d, stat=%d\n",
                res.res.u8[0],res.res.u8[1],rdata[0],rdata[1],rdata[2],res.res.u8[8]);
        }
        shub_input_report_exif_twist_det(info);
        shub_timer_wake_lock_start();              // SHMDS_HUB_0402_01 add
#endif
/* SHMDS_HUB_2301_01 add E */
    }else{
        DBG(DBG_LV_INFO, "[DBG] other Gyro IRQ detail(%x)\n", res.res.u8[0]);
    }
    shub_wake_lock_end(&shub_gyro_wake_lock);    // SHMDS_HUB_0402_01 add
} 

static void shub_int_fusion_work_func(struct work_struct *work)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    DBG(DBG_LV_INT, "### shub_int_fusion_work_func In \n");

    cmd.cmd.u16 = HC_MCU_GET_INT_DETAIL;
    cmd.prm.u16[0] = INTREQ_FUSION;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_INT_DETAIL err(%x)\n", res.err.u16);
        shub_wake_lock_end(&shub_fusion_wake_lock);    // SHMDS_HUB_0402_01 add
        return;
    }

    if((res.res.u8[0] == 0x01) || (res.res.u8[0] == 0x02) || (res.res.u8[0] == 0x03)){
        DBG(DBG_LV_INFO, "[DBG]shub_logging_flush by fifo full start\n");
        suspendBatchingProc();
        shub_logging_flush();
        resumeBatchingProc();
        shub_timer_wake_lock_start();                // SHMDS_HUB_0402_01 add
        DBG(DBG_LV_INFO, "[DBG]shub_logging_flush by fifo full end\n");
    }else{
        DBG(DBG_LV_ERROR, "[DBG] other Fusion IRQ detail(%x)\n", res.res.u8[0]);
    }
    shub_wake_lock_end(&shub_fusion_wake_lock);    // SHMDS_HUB_0402_01 add
}

static int32_t shub_calc_sensortask_period_us(int32_t period_us)
{
/* SHMDS_HUB_2501_01 mod S */
//  if(period_us <= 7*1000){ //0-7
//      return 1875;
//  }
//  if(period_us <= 14*1000){ //8-14
//      return 3750;
//  }
//  if(period_us <= 29*1000){//15-29
//      return 7500;
//  }
//  if(period_us <= 59*1000){//30-59
//      return 30000;
//  }
//  if(period_us <= 99*1000){//60-99
//      return 60000;
//  }
//  return 100000;
    if(period_us <= 29*1000){  // 0-29
        return 5000;
    }
    if(period_us < 100*1000){ // 30-100
        return (period_us / 5000) * 5000;
    }
    return 100000;
/* SHMDS_HUB_2501_01 mod E */
}

static int32_t shub_calc_fusion_period_us(int32_t period_us)
{
/* SHMDS_HUB_2501_01 del S */
//  if(period_us <= 39*1000){
//      return 15000;
//  }
/* SHMDS_HUB_2501_01 del E */
    return 20000;
}

static uint8_t shub_calc_sensorcnt(int32_t sensor_us , int32_t task_us)
{
    int32_t tmp_cnt;

    if(task_us == 0){
        return 1;
    }

    tmp_cnt = sensor_us / task_us;

    if(tmp_cnt > 255){
        tmp_cnt=255;
    }
    if(tmp_cnt <= 0){
        tmp_cnt = 1;
    }
    return (uint8_t)tmp_cnt;
}

static uint8_t shub_calc_sensorcnt_even(int32_t sensor_us , int32_t task_us)
{
    int32_t tmp_cnt = shub_calc_sensorcnt(sensor_us, task_us);
/* SHMDS_HUB_2501_01 del S */
//  if((tmp_cnt > 1) && ((tmp_cnt % 2) != 0)){
//      tmp_cnt--;
//  }
/* SHMDS_HUB_2501_01 del E */
    return (uint8_t)tmp_cnt;
}

static int32_t shub_set_delay_exec(int32_t arg_iSensType, int32_t arg_iLoggingType)
{
    HostCmd cmd;
    HostCmdRes res;
    uint32_t sensorTaskDelay_us=MEASURE_MAX_US;
    uint32_t fusionTaskDelay_us=MEASURE_MAX_US;
    uint32_t mag_delay=MEASURE_MAX_US;
    uint32_t gyro_delay=MEASURE_MAX_US;
    uint32_t gyro_filter_delay=MEASURE_MAX_US;
    uint32_t acc_delay=MEASURE_MAX_US;
/* SHMDS_HUB_0120_08 add S */
#ifdef CONFIG_BARO_SENSOR
    uint8_t baro_odr;                           /* SHMDS_HUB_3101_01 add */
    uint32_t baro_delay=SENSOR_BARO_MAX_DELAY;  /* SHMDS_HUB_0120_01 add SHMDS_HUB_0120_08 mod */
#endif
/* SHMDS_HUB_0120_08 add E */
    uint32_t mag_logging_delay=MEASURE_MAX_US;
    uint32_t gyro_logging_delay=MEASURE_MAX_US;
    int32_t ret;
    int32_t sensorEnable = atomic_read(&g_CurrentSensorEnable);
    int32_t loggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    int32_t iCurrentSensorEnable = arg_iSensType |sensorEnable;
    int32_t iCurrentLoggingEnable = arg_iLoggingType | loggingEnable;
    SensorDelay sensor_delay_us = s_sensor_delay_us;
    SensorDelay logging_delay_us = s_logging_delay_us;

    DBG(DBG_LV_INFO, "####%s sensor=%s logging=%s\n",
            __FUNCTION__,
            shub_get_active_sensor_name(arg_iSensType),
            shub_get_active_sensor_name(arg_iLoggingType));

/* SHMDS_HUB_0206_02 mod S */
//    if((iCurrentSensorEnable & SHUB_ACTIVE_ACC) == 0){
    if((iCurrentSensorEnable & (SHUB_ACTIVE_ACC | SHUB_ACTIVE_SHEX_ACC)) == 0){
/* SHMDS_HUB_0206_02 mod E */
        sensor_delay_us.acc  = MEASURE_MAX_US;
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_MAG) == 0){
        sensor_delay_us.mag  = MEASURE_MAX_US;
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_GYRO) == 0){
        sensor_delay_us.gyro = MEASURE_MAX_US;
    }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    if((iCurrentSensorEnable & SHUB_ACTIVE_BARO) == 0){
        sensor_delay_us.baro = SENSOR_BARO_MAX_DELAY;   /* SHMDS_HUB_0120_08 mod */
    }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
/* SHMDS_HUB_2504_01 mod S */
//    if((iCurrentLoggingEnable & SHUB_ACTIVE_ACC) == 0){
    if((iCurrentLoggingEnable & (SHUB_ACTIVE_ACC | SHUB_ACTIVE_SHEX_ACC)) == 0){
/* SHMDS_HUB_2504_01 mod E */
        logging_delay_us.acc  = MEASURE_MAX_US;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_MAG) == 0){
        logging_delay_us.mag  = MEASURE_MAX_US;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_GYRO) == 0){
        logging_delay_us.gyro = MEASURE_MAX_US;
    }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    if((iCurrentLoggingEnable & SHUB_ACTIVE_BARO) == 0){
        logging_delay_us.baro = SENSOR_BARO_MAX_DELAY;   /* SHMDS_HUB_0120_08 mod */
    }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */

    //Fusion TASK Cycle
    // for sensor measure
    if((iCurrentSensorEnable & SHUB_ACTIVE_ORI) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.orien   , fusionTaskDelay_us);
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_GRAVITY) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.grav    , fusionTaskDelay_us);
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_LACC) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.linear  , fusionTaskDelay_us);
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_RV) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.rot     , fusionTaskDelay_us);
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_RV_NONMAG) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.rot_gyro, fusionTaskDelay_us);
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_RV_NONGYRO) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.rot_mag , fusionTaskDelay_us);
    }

    // for logging
    if((iCurrentLoggingEnable & SHUB_ACTIVE_ORI) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.orien   , fusionTaskDelay_us);
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_GRAVITY) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.grav    , fusionTaskDelay_us);
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_LACC) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.linear  , fusionTaskDelay_us);
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.rot     , fusionTaskDelay_us);
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONMAG) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.rot_gyro, fusionTaskDelay_us);
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONGYRO) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.rot_mag , fusionTaskDelay_us);
    }
    fusionTaskDelay_us = SHUB_MAX(FUSION_TSK_DEFALUT_US, fusionTaskDelay_us);

/* SHMDS_HUB_2502_01 del S */
//  sensor_delay_us.mag /= 2;
//  sensor_delay_us.mag_uc /= 2;
/* SHMDS_HUB_2502_01 del E */

    //Fusion task enable condition
    if((iCurrentSensorEnable & FUSION_9AXIS_GROUP_MASK) | 
            (iCurrentLoggingEnable & FUSION_9AXIS_GROUP_MASK)){
        if(iCurrentSensorEnable & SHUB_ACTIVE_ACC )
            sensor_delay_us.acc  = SHUB_MIN(FUSION_ACC_DELAY, sensor_delay_us.acc);
        else
            sensor_delay_us.acc  = FUSION_ACC_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_MAG)
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.mag);
        else
            sensor_delay_us.mag  = FUSION_MAG_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_MAGUNC)
            sensor_delay_us.mag_uc  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.mag_uc);
        else
            sensor_delay_us.mag_uc  = FUSION_MAG_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_GYRO){
            sensor_delay_us.gyro = SHUB_MIN(FUSION_GYRO_DELAY, sensor_delay_us.gyro);
            /* for gyro calibrataion */
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.gyro);
        }else{
            sensor_delay_us.gyro = FUSION_GYRO_DELAY;
            sensor_delay_us.mag  = FUSION_MAG_DELAY;
        }

        if(iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC){
            sensor_delay_us.gyro_uc = SHUB_MIN(FUSION_GYRO_DELAY, sensor_delay_us.gyro_uc);
            /* for gyro calibrataion */
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.gyro_uc);
        }else{
            sensor_delay_us.gyro_uc  = FUSION_GYRO_DELAY;
            sensor_delay_us.mag  = FUSION_MAG_DELAY;
        }
    }else if((iCurrentSensorEnable & FUSION_6AXIS_ACC_MAG_MASK) | 
            (iCurrentLoggingEnable & FUSION_6AXIS_ACC_MAG_MASK)){

        if(iCurrentSensorEnable & SHUB_ACTIVE_ACC)
            sensor_delay_us.acc  = SHUB_MIN(FUSION_ACC_DELAY, sensor_delay_us.acc);
        else
            sensor_delay_us.acc  = FUSION_ACC_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_MAG)
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.mag);
        else
            sensor_delay_us.mag  = FUSION_MAG_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_MAGUNC)
            sensor_delay_us.mag_uc  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.mag_uc);
        else
            sensor_delay_us.mag_uc  = FUSION_MAG_DELAY;
    }else if((iCurrentSensorEnable & FUSION_6AXIS_ACC_GYRO_MASK) | 
            (iCurrentLoggingEnable & FUSION_6AXIS_ACC_GYRO_MASK)){

        if(iCurrentSensorEnable & SHUB_ACTIVE_ACC)
            sensor_delay_us.acc  = SHUB_MIN(FUSION_ACC_DELAY, sensor_delay_us.acc);
        else
            sensor_delay_us.acc  = FUSION_ACC_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_GYRO){
            sensor_delay_us.gyro = SHUB_MIN(FUSION_GYRO_DELAY, sensor_delay_us.gyro);
            /* for gyro calibrataion */
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.gyro);
        }else{
            sensor_delay_us.gyro = FUSION_GYRO_DELAY;
            sensor_delay_us.mag  = FUSION_MAG_DELAY;
        }

        if(iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC){
            sensor_delay_us.gyro_uc = SHUB_MIN(FUSION_GYRO_DELAY, sensor_delay_us.gyro_uc);
            /* for gyro calibrataion */
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.gyro_uc);
        }else{
            sensor_delay_us.gyro_uc  = FUSION_GYRO_DELAY;
            sensor_delay_us.mag  = FUSION_MAG_DELAY;
        }
    }else{
        if(iCurrentSensorEnable & SHUB_ACTIVE_GYRO){
            sensor_delay_us.mag  = SHUB_MIN(sensor_delay_us.mag, sensor_delay_us.gyro);
        }
/* SHMDS_HUB_2501_01 add S */
        if(iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC){
            sensor_delay_us.mag  = SHUB_MIN(sensor_delay_us.mag, sensor_delay_us.gyro_uc);
        }
/* SHMDS_HUB_2501_01 add E */
    }

    // Sensor min cycle
    sensor_delay_us.acc     = SHUB_MAX(SENSOR_ACC_MIN_DELAY      , sensor_delay_us.acc);
    sensor_delay_us.mag     = SHUB_MAX(SENSOR_MAG_MIN_DELAY      , sensor_delay_us.mag);
    sensor_delay_us.mag     = SHUB_MIN(SENSOR_MAG_MAX_DELAY      , sensor_delay_us.mag);     /* SHMDS_HUB_2501_01 add */
    sensor_delay_us.gyro    = SHUB_MAX(SENSOR_GYRO_MIN_DELAY     , sensor_delay_us.gyro);
    sensor_delay_us.gyro    = SHUB_MIN(SENSOR_GYRO_MAX_DELAY     , sensor_delay_us.gyro);
    sensor_delay_us.mag_uc  = SHUB_MAX(SENSOR_MAGUC_MIN_DELAY    , sensor_delay_us.mag_uc );
    sensor_delay_us.mag_uc  = SHUB_MIN(SENSOR_MAGUC_MAX_DELAY    , sensor_delay_us.mag_uc ); /* SHMDS_HUB_2501_01 add */
    sensor_delay_us.gyro_uc = SHUB_MAX(SENSOR_GYROUC_MIN_DELAY   , sensor_delay_us.gyro_uc);
    sensor_delay_us.gyro_uc = SHUB_MIN(SENSOR_GYROUC_MAX_DELAY   , sensor_delay_us.gyro_uc);
    sensor_delay_us.baro    = SHUB_MAX(SENSOR_BARO_MIN_DELAY     , sensor_delay_us.baro);  /* SHMDS_HUB_0120_01 add */

    // Sensor TASK Cycle
    if(iCurrentLoggingEnable == 0){
        sensorTaskDelay_us=fusionTaskDelay_us;
/* SHMDS_HUB_0206_02 mod S */
//      if(iCurrentSensorEnable & (SHUB_ACTIVE_ACC | FUSION_GROUP_MASK)){
        if(iCurrentSensorEnable & (ACC_GROUP_MASK | FUSION_GROUP_ACC_MASK)){
/* SHMDS_HUB_0206_02 mod E */
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.acc    , sensorTaskDelay_us);
        }
        if(iCurrentSensorEnable & (SHUB_ACTIVE_MAG | FUSION_GROUP_MAG_MASK)){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.mag    , sensorTaskDelay_us);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_MAGUNC){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.mag_uc , sensorTaskDelay_us);
        }
        if(iCurrentSensorEnable & (SHUB_ACTIVE_GYRO | FUSION_GROUP_GYRO_MASK) ){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.gyro   , sensorTaskDelay_us);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.gyro_uc, sensorTaskDelay_us);
        }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
        if(iCurrentSensorEnable & SHUB_ACTIVE_BARO){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.baro , sensorTaskDelay_us);
        }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
/* SHMDS_HUB_2501_01 SHMDS_HUB_2504_01 SHMDS_HUB_2505_01 mod S */
//      if(iCurrentSensorEnable & (~APPTASK_GROUP_ACC_MASK)){
/*        if(iCurrentSensorEnable & ((~APPTASK_GROUP_ACC_MASK) & (~SHUB_ACTIVE_SHEX_ACC))){*/
        if(iCurrentSensorEnable & ((~APPTASK_GROUP_ACC_MASK) & (~SHUB_ACTIVE_SHEX_ACC) & (~SHUB_ACTIVE_GDEC))){
            sensorTaskDelay_us = SHUB_MIN(SENSOR_TSK_DEFALUT_US, sensorTaskDelay_us);
        }
/* SHMDS_HUB_2501_01 SHMDS_HUB_2504_01 SHMDS_HUB_2505_01 mod E */

/* SHMDS_HUB_2504_01 add S */
        if(iCurrentSensorEnable & SHUB_ACTIVE_SHEX_ACC){
            sensorTaskDelay_us = SHUB_MIN(SENSOR_SHEX_ACC_MIN_DELAY, sensorTaskDelay_us);
        }
/* SHMDS_HUB_2504_01 add E */

        if(iCurrentSensorEnable & SHUB_ACTIVE_MAGUNC){
            fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.mag_uc, fusionTaskDelay_us);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_MAG){
            fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.mag, fusionTaskDelay_us);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_GYRO){
            fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.gyro , fusionTaskDelay_us);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC){
            fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.gyro_uc , fusionTaskDelay_us);
        }
    }else{
        sensorTaskDelay_us = SENSOR_TSK_MIN_LOGGING_US ;
/* SHMDS_HUB_2504_01 add S */
        if(iCurrentLoggingEnable == SHUB_ACTIVE_SHEX_ACC){
            sensorTaskDelay_us = SENSOR_SHEX_ACC_MIN_DELAY ;
        }
/* SHMDS_HUB_2504_01 add E */

        if(iCurrentLoggingEnable & SHUB_ACTIVE_MAGUNC){
//          fusionTaskDelay_us = SHUB_MIN(logging_delay_us.mag_uc / 2, fusionTaskDelay_us); /* SHMDS_HUB_2502_01 mod */
            fusionTaskDelay_us = SHUB_MIN(logging_delay_us.mag_uc, fusionTaskDelay_us);     /* SHMDS_HUB_2502_01 mod */
        }
        if(iCurrentLoggingEnable & SHUB_ACTIVE_MAG){
//          fusionTaskDelay_us = SHUB_MIN(logging_delay_us.mag / 2, fusionTaskDelay_us);    /* SHMDS_HUB_2502_01 mod */
            fusionTaskDelay_us = SHUB_MIN(logging_delay_us.mag, fusionTaskDelay_us);        /* SHMDS_HUB_2502_01 mod */
        }
        if(iCurrentLoggingEnable & SHUB_ACTIVE_GYRO){
            fusionTaskDelay_us = SHUB_MIN(logging_delay_us.gyro , fusionTaskDelay_us);
        }
        if(iCurrentLoggingEnable & SHUB_ACTIVE_GYROUNC){
            fusionTaskDelay_us = SHUB_MIN(logging_delay_us.gyro_uc , fusionTaskDelay_us);
        }
    }

    //App task enable condition
    if((iCurrentLoggingEnable | iCurrentSensorEnable) & APPTASK_GROUP_ACC_MASK){ 
        sensor_delay_us.acc = SHUB_MIN(sensor_delay_us.acc, APP_TSK_DEFALUT_US);
        sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.acc , sensorTaskDelay_us);
    } 
    //G detection enable condition
    if(iCurrentSensorEnable & SHUB_ACTIVE_GDEC){
        sensor_delay_us.acc = SHUB_MIN(sensor_delay_us.acc, GDEC_DEFALUT_US);
        sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.acc , sensorTaskDelay_us);
    }

/* SHMDS_HUB_1701_01 add S */
    // pickup enable condition
    if(iCurrentSensorEnable & SHUB_ACTIVE_PICKUP){
        sensor_delay_us.acc = SHUB_MIN(sensor_delay_us.acc, PICKUP_DEFALUT_US);
        sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.acc , sensorTaskDelay_us);
    }
/* SHMDS_HUB_1701_01 add E */

/* SHMDS_HUB_0132_01 add S */
    // device orientation enable condition
    if(iCurrentSensorEnable & SHUB_ACTIVE_DEVICE_ORI){
        sensor_delay_us.acc = SHUB_MIN(sensor_delay_us.acc, DEVICE_ORI_DEF_US);
        sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.acc , sensorTaskDelay_us);
    }
/* SHMDS_HUB_0132_01 add E */

/* SHMDS_HUB_3801_02 add S */
#ifdef SHUB_SW_FREE_FALL_DETECT
    // free fall enable condition
    if(iCurrentSensorEnable & SHUB_ACTIVE_FREE_FALL){
        sensor_delay_us.acc = SHUB_MIN(sensor_delay_us.acc, FREE_FALL_DEFALUT_US);
        sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.acc , sensorTaskDelay_us);
    }
#endif
/* SHMDS_HUB_3801_02 add E */

/* SHMDS_HUB_2301_01 add S */
#ifdef SHUB_SW_TWIST_DETECT
    // twist enable condition
    if(iCurrentSensorEnable & SHUB_ACTIVE_TWIST){
        sensor_delay_us.gyro = SHUB_MIN(sensor_delay_us.gyro, TWIST_DEFALUT_US);
        sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.gyro , sensorTaskDelay_us);
    }
#endif
/* SHMDS_HUB_2301_01 add E */

    sensorTaskDelay_us = shub_calc_sensortask_period_us(sensorTaskDelay_us);
    fusionTaskDelay_us = shub_calc_fusion_period_us(fusionTaskDelay_us);
    s_sensor_task_delay_us = sensorTaskDelay_us;
    
#ifdef CONFIG_ACC_U2DH /* SHMDS_HUB_0326_01 SHMDS_HUB_2901_01 SHMDS_HUB_3101_01 */
    /***** Set Freerun *****/
    if((sensorTaskDelay_us < 8000) && (shub_operation_mode != 0)){
        cmd.cmd.u16 = HC_ACC_SET_OPERATION_MODE;
        cmd.prm.u8[0] = 0;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_SET_OPERATION_MODE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        shub_operation_mode = 0;
    }
#endif
    
    /*set task cycle*/
    cmd.cmd.u16 = HC_SENSOR_TSK_SET_CYCLE;
    cmd.prm.u16[0] = (uint16_t)(sensorTaskDelay_us / 10);
    cmd.prm.u16[1] = (uint16_t)(APP_TSK_DEFALUT_US / 10);
    cmd.prm.u16[2] = (uint16_t)(fusionTaskDelay_us / 10);
    DBG(DBG_LV_INFO, "Task Period:sens=%d app=%d fusion=%d\n"
            ,sensorTaskDelay_us,APP_TSK_DEFALUT_US,fusionTaskDelay_us);
/* SHMDS_HUB_3101_01 mod S */
    if((s_micon_param.task_cycle[0] != cmd.prm.u16[0])
    || (s_micon_param.task_cycle[1] != cmd.prm.u16[1])
    || (s_micon_param.task_cycle[2] != cmd.prm.u16[2])){
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SENSOR_TSK_SET_CYCLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
/* SHMDS_HUB_3101_01 mod E */
/* SHMDS_HUB_0701_11 add S */
    s_micon_param.task_cycle[0] = cmd.prm.u16[0];
    s_micon_param.task_cycle[1] = cmd.prm.u16[1];
    s_micon_param.task_cycle[2] = cmd.prm.u16[2];
/* SHMDS_HUB_0701_11 add E */

    if((iCurrentSensorEnable & MAG_GROUP_MASK) == MAG_GROUP_MASK){
        mag_delay = SHUB_MIN(sensor_delay_us.mag, sensor_delay_us.mag_uc);
    }else if((iCurrentSensorEnable & SHUB_ACTIVE_MAGUNC) == SHUB_ACTIVE_MAGUNC ){
        mag_delay = sensor_delay_us.mag_uc;
    }
    mag_delay = SHUB_MIN(mag_delay, sensor_delay_us.mag);

    if((iCurrentSensorEnable & GYRO_GROUP_MASK) == GYRO_GROUP_MASK){
        gyro_delay = SHUB_MIN(sensor_delay_us.gyro, sensor_delay_us.gyro_uc);
        gyro_filter_delay = SHUB_MIN(s_sensor_delay_us.gyro, s_sensor_delay_us.gyro_uc);
    }else if((iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC) == SHUB_ACTIVE_GYROUNC ){
        gyro_delay = sensor_delay_us.gyro_uc;
        gyro_filter_delay = s_sensor_delay_us.gyro_uc;
/* SHMDS_HUB_2503_01 add S */
    }else if((iCurrentSensorEnable & SHUB_ACTIVE_ORI) == SHUB_ACTIVE_ORI ){
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_sensor_delay_us.orien);
/* SHMDS_HUB_2503_01 add E */
    }else if((iCurrentSensorEnable & SHUB_ACTIVE_GYRO) == SHUB_ACTIVE_GYRO ){
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_sensor_delay_us.gyro);
    }

    gyro_delay = SHUB_MIN(gyro_delay, sensor_delay_us.gyro);


    if((iCurrentLoggingEnable & MAG_GROUP_MASK) == MAG_GROUP_MASK){
        mag_logging_delay = SHUB_MIN(logging_delay_us.mag, logging_delay_us.mag_uc);
    }else if((iCurrentLoggingEnable & SHUB_ACTIVE_MAGUNC) == SHUB_ACTIVE_MAGUNC ){
        mag_logging_delay = logging_delay_us.mag_uc;
    }
    mag_logging_delay = SHUB_MIN(mag_logging_delay, logging_delay_us.mag);

    if((iCurrentLoggingEnable & GYRO_GROUP_MASK) == GYRO_GROUP_MASK){
        gyro_logging_delay = SHUB_MIN(logging_delay_us.gyro, logging_delay_us.gyro_uc);
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_logging_delay_us.gyro);
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_logging_delay_us.gyro_uc);
    }else if((iCurrentLoggingEnable & SHUB_ACTIVE_GYROUNC) == SHUB_ACTIVE_GYROUNC ){
        gyro_logging_delay = logging_delay_us.gyro_uc;
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_logging_delay_us.gyro_uc);
/* SHMDS_HUB_2503_01 add S */
    }else if((iCurrentLoggingEnable & SHUB_ACTIVE_ORI) == SHUB_ACTIVE_ORI ){
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_logging_delay_us.orien);
/* SHMDS_HUB_2503_01 add E */
    }else if((iCurrentLoggingEnable & SHUB_ACTIVE_GYRO) == SHUB_ACTIVE_GYRO ){
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_logging_delay_us.gyro);
    }
    gyro_logging_delay = SHUB_MIN(gyro_logging_delay, logging_delay_us.gyro);
    gyro_delay = SHUB_MIN(gyro_delay, gyro_logging_delay);

    acc_delay = sensor_delay_us.acc;
    /* lsm6ds only */
    if(iCurrentLoggingEnable & (FUSION_GROUP_ACC_MASK | SHUB_ACTIVE_ACC | APPTASK_GROUP_ACC_MASK )){ 
        acc_delay = SHUB_MIN(logging_delay_us.acc, sensor_delay_us.acc);
        if(((iCurrentLoggingEnable|iCurrentSensorEnable) & (APPTASK_GROUP_ACC_MASK|SHUB_ACTIVE_GDEC)) 
                && (logging_delay_us.acc < sensor_delay_us.acc)){ 
            acc_delay =SENSOR_ACC_MIN_DELAY; 
        } 
    }

/* SHMDS_HUB_0120_08 mod S */
#ifdef CONFIG_BARO_SENSOR
    baro_delay = sensor_delay_us.baro;  /* SHMDS_HUB_0120_01 add */
    if(iCurrentLoggingEnable & SHUB_ACTIVE_BARO){
        baro_delay = SHUB_MIN(logging_delay_us.baro, sensor_delay_us.baro);
    }
    if(baro_delay >= SENSOR_BARO_MAX_DELAY) {
        baro_delay = SENSOR_BARO_MAX_DELAY;
    } else if(baro_delay >= SENSOR_BARO_MID_DELAY) {
        baro_delay = SENSOR_BARO_MID_DELAY;
    } else if(baro_delay >= SENSOR_BARO_LOW_DELAY) {
        baro_delay = SENSOR_BARO_LOW_DELAY;
    }
#endif
/* SHMDS_HUB_0120_08 mod E */

#ifdef SHUB_SW_GYRO_ENABLE
    cmd.cmd.u16 = HC_GYRO_SET_FILTER;
/* SHMDS_HUB_2503_01 mod S */
//  cmd.prm.u8[0] = 1;
//  if(gyro_filter_delay >= 200*1000){
//      cmd.prm.u8[0] = 16; /*120ms*/
//  }else if(gyro_filter_delay >= 60*1000){
//      cmd.prm.u8[0] = 7; /*52.5ms*/
//  }else if(gyro_filter_delay >= 20*1000){
//      cmd.prm.u8[0] = 2; /*15ms*/
//  }else if(gyro_filter_delay >= 5*1000){
//      cmd.prm.u8[0] = 1; /*3.75ms*/
//  }
    if((iCurrentLoggingEnable | iCurrentSensorEnable) & (GYRO_GROUP_MASK | SHUB_ACTIVE_ORI)){ 
        cmd.prm.u8[0] = shub_calc_sensorcnt_even(gyro_filter_delay, gyro_delay);
        if(cmd.prm.u8[0] > 16){
            cmd.prm.u8[0] = 16;
        }
    }else{
        cmd.prm.u8[0] = 1;
    }
/* SHMDS_HUB_2503_01 del E */
    if(s_micon_param.gyro_filter != cmd.prm.u8[0]){
        DBG(DBG_LV_INFO, "GyroFilter=%d\n", cmd.prm.u8[0]);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GYRO_SET_FILTER err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
    s_micon_param.gyro_filter = cmd.prm.u8[0];
#endif

    /*set sensor measure cycle */
    cmd.cmd.u16 = HC_SENSOR_SET_CYCLE;
    cmd.prm.u8[0] = shub_calc_sensorcnt_even(acc_delay, sensorTaskDelay_us);
/* SHMDS_HUB_0120_01 mod E  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    cmd.prm.u8[1] = shub_calc_sensorcnt_even(baro_delay, sensorTaskDelay_us);
#else
    cmd.prm.u8[1] = 12; /** !! not implement !! */
#endif
/* SHMDS_HUB_0120_01 mod E  SHMDS_HUB_0122_01 add E */
    cmd.prm.u8[2] = shub_calc_sensorcnt_even(mag_delay , sensorTaskDelay_us);
    cmd.prm.u8[3] = shub_calc_sensorcnt_even(gyro_delay , sensorTaskDelay_us);

/* SHMDS_HUB_0120_01 mod S  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    DBG(DBG_LV_INFO, "Poll Period:acc=%d baro=%d mag=%d gyro=%d\n" ,cmd.prm.u8[0],cmd.prm.u8[1],cmd.prm.u8[2],cmd.prm.u8[3]);
#else
    DBG(DBG_LV_INFO, "Poll Period:acc=%d mag=%d gyro=%d\n" ,cmd.prm.u8[0],cmd.prm.u8[2],cmd.prm.u8[3]);
#endif
/* SHMDS_HUB_0120_01 mod E  SHMDS_HUB_0122_01 add E */
/* SHMDS_HUB_3101_01 mod S */
    if((s_micon_param.s_cycle[0] != cmd.prm.u8[0]*sensorTaskDelay_us)
    || (s_micon_param.s_cycle[1] != cmd.prm.u8[1]*sensorTaskDelay_us)
    || (s_micon_param.s_cycle[2] != cmd.prm.u8[2]*sensorTaskDelay_us)
    || (s_micon_param.s_cycle[3] != cmd.prm.u8[3]*sensorTaskDelay_us)){
        // HC_SENSOR_SET_CYCLE
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SENSOR_SET_CYCLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
/* SHMDS_HUB_3101_01 mod E */
/* SHMDS_HUB_0701_11 add S */
    s_micon_param.s_cycle[0] = cmd.prm.u8[0]*sensorTaskDelay_us;
    s_micon_param.s_cycle[1] = cmd.prm.u8[1]*sensorTaskDelay_us;
    s_micon_param.s_cycle[2] = cmd.prm.u8[2]*sensorTaskDelay_us;
    s_micon_param.s_cycle[3] = cmd.prm.u8[3]*sensorTaskDelay_us;
/* SHMDS_HUB_0701_11 add E */

    /*set logging sensor cycle */
    cmd.cmd.u16 = HC_LOGGING_SENSOR_SET_CYCLE;
    cmd.prm.u8[0] = shub_calc_sensorcnt(logging_delay_us.acc , sensorTaskDelay_us);
/* SHMDS_HUB_0120_01 mod S  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    cmd.prm.u8[1] = shub_calc_sensorcnt(logging_delay_us.baro, sensorTaskDelay_us);
#else
    cmd.prm.u8[1] = 12; /** !! not implement !! */
#endif
/* SHMDS_HUB_0120_01 mod E  SHMDS_HUB_0122_01 add E */
    cmd.prm.u8[2] = shub_calc_sensorcnt(mag_logging_delay  , sensorTaskDelay_us);
    cmd.prm.u8[3] = shub_calc_sensorcnt(gyro_logging_delay , sensorTaskDelay_us);

/* SHMDS_HUB_0120_01 mod S  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    DBG(DBG_LV_INFO, "LogS Period:acc=%d baro=%d mag=%d gyro=%d\n" ,cmd.prm.u8[0],cmd.prm.u8[1],cmd.prm.u8[2],cmd.prm.u8[3]);
#else
    DBG(DBG_LV_INFO, "LogS Period:acc=%d mag=%d gyro=%d\n" ,cmd.prm.u8[0],cmd.prm.u8[2],cmd.prm.u8[3]);
#endif
/* SHMDS_HUB_0120_01 mod E  SHMDS_HUB_0122_01 add E */
    
/* SHMDS_HUB_3101_01 mod S */
    if((shub_logging_cycle.acc  != cmd.prm.u8[0]*sensorTaskDelay_us)
    || (shub_logging_cycle.baro != cmd.prm.u8[1]*sensorTaskDelay_us)
    || (shub_logging_cycle.mag  != cmd.prm.u8[2]*sensorTaskDelay_us)
    || (shub_logging_cycle.gyro != cmd.prm.u8[3]*sensorTaskDelay_us)){
        // HC_LOGGING_SENSOR_SET_CYCLE
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_SENSOR_SET_CYCLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
/* SHMDS_HUB_3101_01 mod E */
/* SHMDS_HUB_0701_11 add S */
    shub_logging_cycle.acc  = cmd.prm.u8[0]*sensorTaskDelay_us;
    shub_logging_cycle.baro = cmd.prm.u8[1]*sensorTaskDelay_us;
    shub_logging_cycle.mag  = cmd.prm.u8[2]*sensorTaskDelay_us;
    shub_logging_cycle.gyro = cmd.prm.u8[3]*sensorTaskDelay_us;
/* SHMDS_HUB_0701_11 add E */

    /*set loggin fusion cycle */
    cmd.cmd.u16 = HC_LOGGING_FUSION_SET_CYCLE;
/* SHMDS_HUB_2602_02 mod S */
//  cmd.prm.u8[0] = shub_calc_sensorcnt(logging_delay_us.orien    ,fusionTaskDelay_us);
//  cmd.prm.u8[1] = shub_calc_sensorcnt(logging_delay_us.grav     ,fusionTaskDelay_us);
//  cmd.prm.u8[2] = shub_calc_sensorcnt(logging_delay_us.linear   ,fusionTaskDelay_us);
//  cmd.prm.u8[3] = shub_calc_sensorcnt(logging_delay_us.rot      ,fusionTaskDelay_us);
//  cmd.prm.u8[4] = shub_calc_sensorcnt(logging_delay_us.rot_gyro ,fusionTaskDelay_us);
//  cmd.prm.u8[5] = shub_calc_sensorcnt(logging_delay_us.rot_mag  ,fusionTaskDelay_us);
    cmd.prm.u8[0] = shub_calc_sensorcnt(logging_delay_us.orien    ,sensorTaskDelay_us);
    cmd.prm.u8[1] = shub_calc_sensorcnt(logging_delay_us.grav     ,sensorTaskDelay_us);
    cmd.prm.u8[2] = shub_calc_sensorcnt(logging_delay_us.linear   ,sensorTaskDelay_us);
    cmd.prm.u8[3] = shub_calc_sensorcnt(logging_delay_us.rot      ,sensorTaskDelay_us);
    cmd.prm.u8[4] = shub_calc_sensorcnt(logging_delay_us.rot_gyro ,sensorTaskDelay_us);
    cmd.prm.u8[5] = shub_calc_sensorcnt(logging_delay_us.rot_mag  ,sensorTaskDelay_us);
/* SHMDS_HUB_2602_02 mod E */

    DBG(DBG_LV_INFO , "LogF Period:ori=%d grav=%d linear=%d rot=%d game=%d magrot=%d\n",
            cmd.prm.u8[0], cmd.prm.u8[1], cmd.prm.u8[2],
            cmd.prm.u8[3] , cmd.prm.u8[4], cmd.prm.u8[5]);

/* SHMDS_HUB_3101_01 mod S */
    if((shub_logging_cycle.orien    != cmd.prm.u8[0]*sensorTaskDelay_us)
    || (shub_logging_cycle.grav     != cmd.prm.u8[1]*sensorTaskDelay_us)
    || (shub_logging_cycle.linear   != cmd.prm.u8[2]*sensorTaskDelay_us)
    || (shub_logging_cycle.rot      != cmd.prm.u8[3]*sensorTaskDelay_us)
    || (shub_logging_cycle.rot_gyro != cmd.prm.u8[4]*sensorTaskDelay_us)
    || (shub_logging_cycle.rot_mag  != cmd.prm.u8[5]*sensorTaskDelay_us)){
        // HC_LOGGING_FUSION_SET_CYCLE
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6); /* SHMDS_HUB_0329_01 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_FUSION_SET_CYCLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
/* SHMDS_HUB_3101_01 mod E */
/* SHMDS_HUB_0701_11 add S */
    shub_logging_cycle.orien    = cmd.prm.u8[0]*sensorTaskDelay_us;
    shub_logging_cycle.grav     = cmd.prm.u8[1]*sensorTaskDelay_us;
    shub_logging_cycle.linear   = cmd.prm.u8[2]*sensorTaskDelay_us;
    shub_logging_cycle.rot      = cmd.prm.u8[3]*sensorTaskDelay_us;
    shub_logging_cycle.rot_gyro = cmd.prm.u8[4]*sensorTaskDelay_us;
    shub_logging_cycle.rot_mag  = cmd.prm.u8[5]*sensorTaskDelay_us;
/* SHMDS_HUB_0701_11 add E */

/* SHMDS_HUB_0120_08 SHMDS_HUB_3101_01 add S */
#ifdef CONFIG_BARO_SENSOR
    if(baro_delay == SENSOR_BARO_MAX_DELAY){
        baro_odr = 1;
    }else{
        baro_odr = 2;
    }
    if(shub_baro_odr != baro_odr){
        cmd.cmd.u16 = HC_BARO_GET_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_BARO_GET_PARAM err(%x)\n", res.err.u16);
        } else {
            cmd.cmd.u16 = HC_BARO_SET_PARAM;
            cmd.prm.u8[0] = 0;
            cmd.prm.u8[1] = res.res.u8[1];
            cmd.prm.u8[2] = 0;
            cmd.prm.u8[3] = baro_odr;
            cmd.prm.u8[4] = res.res.u8[4];
            cmd.prm.u8[5] = res.res.u8[5];
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
                DBG(DBG_LV_ERROR, "HC_BARO_SET_PARAM err(%x)\n", res.err.u16);
                return SHUB_RC_ERR;
            }
        }
    }
    shub_baro_odr = baro_odr;
#endif
/* SHMDS_HUB_0120_08 SHMDS_HUB_3101_01 add E */

    return SHUB_RC_OK;
}

static int32_t shub_set_param_exec(int32_t type , int32_t *param)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret;

    if(type == APP_PEDOMETER){
        cmd.prm.u8[0x00] = (uint8_t)param[0x00];
        cmd.prm.u8[0x01] = (uint8_t)param[0x01];
        cmd.prm.u8[0x02] = (uint8_t)param[0x02];
        cmd.prm.u8[0x03] = (uint8_t)param[0x03];
        cmd.prm.u8[0x04] = (uint8_t)param[0x04];
        cmd.prm.u8[0x05] = (uint8_t)param[0x05];
        cmd.prm.u8[0x06] = (uint8_t)param[0x06];
        cmd.prm.u8[0x07] = (uint8_t)param[0x07];
        cmd.prm.u8[0x08] = (uint8_t)param[0x08];
        cmd.prm.u8[0x09] = (uint8_t)param[0x09];
        cmd.prm.u8[0x0a] = (uint8_t)param[0x0a];
        cmd.prm.u8[0x0b] = (uint8_t)param[0x0b];
        cmd.prm.u8[0x0c] = (uint8_t)param[0x0c];
        cmd.prm.u8[0x0d] = (uint8_t)param[0x0d];        // SHMDS_HUB_0204_14 add

        ret = shub_set_delay_exec(SHUB_ACTIVE_EXT_PEDOM, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_EXT_PEDOM, param[0]);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }

        cmd.cmd.u16 = HC_SET_PEDO2_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0204_14 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO2_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        memcpy(s_setcmd_data.cmd_pedo2_step, cmd.prm.u8, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_3301_01 add */
/* SHMDS_HUB_0204_02 add S */
    }else if(type == APP_PEDOMETER_N){
        cmd.prm.u8[0x00] = (uint8_t)param[0x00];
        cmd.prm.u8[0x01] = (uint8_t)param[0x01];
        cmd.prm.u8[0x02] = (uint8_t)param[0x02];
        cmd.prm.u8[0x03] = (uint8_t)param[0x03];
        cmd.prm.u8[0x04] = (uint8_t)param[0x04];
        cmd.prm.u8[0x05] = (uint8_t)param[0x05];
        cmd.prm.u8[0x06] = (uint8_t)param[0x06];
        cmd.prm.u8[0x07] = (uint8_t)param[0x07];
        cmd.prm.u8[0x08] = (uint8_t)param[0x08];
        cmd.prm.u8[0x09] = (uint8_t)param[0x09];
        cmd.prm.u8[0x0a] = (uint8_t)param[0x0a];
        cmd.prm.u8[0x0b] = (uint8_t)param[0x0b];
        cmd.prm.u8[0x0c] = (uint8_t)param[0x0c];        // SHMDS_HUB_0204_05 add
        cmd.prm.u8[0x0d] = (uint8_t)param[0x0d];        // SHMDS_HUB_0204_14 add

        cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0204_14 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
/* SHMDS_HUB_0204_02 add E */
    }else if(type == APP_CALORIE_FACTOR){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)param[3];
        cmd.prm.u8[0x04] = (uint8_t)param[4];
        cmd.prm.u8[0x05] = (uint8_t)param[5];

        cmd.cmd.u16 = HC_SET_PEDO2_CALORIE_FACTOR;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO2_CALORIE_FACTOR err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_RUN_DETECTION){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[2] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_PEDO2_RUN_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO2_RUN_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

    }else if(type == APP_VEICHLE_DETECTION){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)param[3];
        cmd.prm.u8[0x05] = (uint8_t)param[4];
        cmd.prm.u8[0x06] = (uint8_t)param[5];
        cmd.prm.u8[0x07] = (uint8_t)param[6];
        cmd.prm.u8[0x08] = (uint8_t)param[7];
        cmd.prm.u8[0x09] = (uint8_t)param[8];
        cmd.prm.u8[0x0A] = (uint8_t)param[9];
        cmd.prm.u8[0x0B] = (uint8_t)param[10];
        cmd.prm.u8[0x0C] = (uint8_t)param[11];
        cmd.prm.u8[0x0D] = (uint8_t)param[12];            // SHMDS_HUB_0204_23 add

        cmd.cmd.u16 = HC_SET_ACTIVITY2_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_ACTIVITY2_DETECT_PARAM); // SHMDS_HUB_0204_23 add
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        memcpy(s_setcmd_data.cmd_act2_det, cmd.prm.u8, SHUB_SIZE_ACTIVITY2_DETECT_PARAM); /* SHMDS_HUB_3301_01 add */
    }else if(type == APP_VEICHLE_DETECTION2){
        cmd.prm.u8[0x00] = (uint8_t)(param[0] & 0xff);
        cmd.prm.u8[0x01] = (uint8_t)((param[0] >> 8) & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)(param[1] & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[1] >> 8) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)((param[2] >> 8) & 0xff);
/* SHMDS_HUB_0209_02 add S */
/* SHMDS_HUB_0204_14 del S */
//      cmd.prm.u8[0x06] = (uint8_t)param[3];
//      cmd.prm.u8[0x07] = (uint8_t)param[4];
//      cmd.prm.u8[0x08] = (uint8_t)(param[5] & 0xff);
//      cmd.prm.u8[0x09] = (uint8_t)((param[5] >> 8) & 0xff);
//      cmd.prm.u8[0x0A] = (uint8_t)param[6];
//      cmd.prm.u8[0x0B] = (uint8_t)param[7];
//      cmd.prm.u8[0x0C] = (uint8_t)(param[8] & 0xff);
//      cmd.prm.u8[0x0D] = (uint8_t)((param[8] >> 8) & 0xff);
/* SHMDS_HUB_0204_14 del E */
/* SHMDS_HUB_0209_02 add E */
        cmd.cmd.u16 = HC_SET_ACTIVITY2_DETECT_PARAM2;
/* SHMDS_HUB_0204_14 mod S */
//      ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 14);       // SHMDS_HUB_0209_02 mod(6->14)
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
/* SHMDS_HUB_0204_14 mod E */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY2_DETECT_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_VEICHLE_DETECTION3){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)(param[1] & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)((param[1] >> 8) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)param[3];
        cmd.prm.u8[0x06] = (uint8_t)param[4];
        cmd.prm.u8[0x07] = (uint8_t)param[5];
        cmd.prm.s8[0x08] = (int8_t)param[6];

        cmd.cmd.u16 = HC_SET_ACTIVITY2_DETECT_PARAM3;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY2_DETECT_PARAM3 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

    }else if(type == APP_TOTAL_STATUS_DETECTION){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)(param[1] & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)((param[1] >> 8) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)(param[3] & 0xff);
        cmd.prm.u8[0x06] = (uint8_t)((param[3] >> 8) & 0xff);
        cmd.prm.u8[0x07] = (uint8_t)param[4];
        cmd.prm.u8[0x08] = (uint8_t)param[5];
        cmd.prm.u8[0x09] = (uint8_t)param[6];
        cmd.prm.u8[0x0A] = (uint8_t)param[7];
/* SHMDS_HUB_2603_01 add S */
        cmd.prm.u8[0x0B] = (uint8_t)(param[8] & 0xff);
        cmd.prm.u8[0x0C] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.prm.u8[0x0D] = (uint8_t)param[9];
        cmd.prm.u8[0x0E] = (uint8_t)param[10];
/* SHMDS_HUB_2603_01 add E */

        cmd.cmd.u16 = HC_SET_ACTIVITY2_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_ACTIVITY2_TOTAL_DETECT_PARAM);  /* SHMDS_HUB_2603_01 mod (11->15) */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY2_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        memcpy(s_setcmd_data.cmd_act2_tdet, cmd.prm.u8, SHUB_SIZE_ACTIVITY2_TOTAL_DETECT_PARAM); /* SHMDS_HUB_3301_01 add */

/* SHMDS_HUB_2603_03 add S */
    }else if(type == APP_PEDO_ALL_STATE_2ND){
        cmd.prm.u8[0x00] = (uint8_t)(param[0] & 0xff);
        cmd.prm.u8[0x01] = (uint8_t)((param[0] >> 8) & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)((param[0] >>16) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[0] >>24) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)param[1];

        cmd.cmd.u16 = HC_ACC_PEDO_SET_ALL_STATE_2ND;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 5);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_PEDO_SET_ALL_STATE_2ND err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
/* SHMDS_HUB_2603_03 add E */
    }else if(type == APP_GDETECTION){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)(param[3] & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)((param[3] >> 8) & 0xff);
        cmd.prm.u8[0x06] = (uint8_t)param[4];
        cmd.prm.u8[0x07] = (uint8_t)param[5];
        cmd.prm.u8[0x08] = (uint8_t)param[6];

        ret = shub_set_delay_exec(SHUB_ACTIVE_GDEC, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_GDEC, param[0]);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        cmd.cmd.u16 = HC_SET_GDETECTION_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_GDETECTION_PARAM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_GDETECTION_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        memcpy(s_setcmd_data.cmd_gdetection, cmd.prm.u8, SHUB_SIZE_GDETECTION_PARAM); /* SHMDS_HUB_3301_01 add */
    }else if(type == APP_MOTDTECTION){
        shub_set_exif_md_mode_flg(param[31]);       /* SHMDS_HUB_0211_01 SHMDS_HUB_0211_02 add */
        shub_set_param_exif_md_mode(param);         /* SHMDS_HUB_0211_01 add */
        cmd.prm.u8[0x00] = (uint8_t)param[0];
/* SHMDS_HUB_0202_01 add S */
        if(shub_get_already_md_flg() & 0x02) {
            param[0] = 1;
            cmd.prm.u8[0x00] = 1;
        }
/* SHMDS_HUB_0202_01 add E */
        cmd.cmd.u16 = HC_SET_MOTDETECTION_EN;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_MOTDETECTION_EN err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        s_setcmd_data.cmd_mot_en = cmd.prm.u8[0x00]; /* SHMDS_HUB_3301_01 add */

        cmd.prm.u8[0x00] = (uint8_t)param[1];
        cmd.prm.u8[0x01] = (uint8_t)param[2];
        cmd.prm.u8[0x02] = (uint8_t)param[3];
        cmd.prm.u8[0x03] = (uint8_t)param[4];
        cmd.prm.u8[0x04] = (uint8_t)param[5];
        cmd.prm.u8[0x05] = (uint8_t)param[6];
        cmd.prm.u8[0x06] = (uint8_t)param[7];
        cmd.prm.u8[0x07] = (uint8_t)param[8];
        cmd.cmd.u16 = HC_SET_MOTDETECTION_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_MOTDETECTION_PARAM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_MOTDETECTION_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        memcpy(s_setcmd_data.cmd_motdetection, cmd.prm.u8, SHUB_SIZE_MOTDETECTION_PARAM); /* SHMDS_HUB_3301_01 add */

/* SHMDS_HUB_0332_01 add S */
        if (param[9] & 0x01) {
            shub_mot_still_enable_flag = true;
        } else {
            shub_mot_still_enable_flag = false;
        }
/* SHMDS_HUB_0332_01 add E */
        ret = shub_set_delay_exec(SHUB_ACTIVE_MOTIONDEC, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_MOTIONDEC, param[0]);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
/* SHMDS_HUB_0206_01 mod S */
//        cmd.prm.u8[0x00] = (uint8_t)param[9];
        cmd.prm.u8[0x00] = (uint8_t)(param[9] & 0x03);
/* SHMDS_HUB_0206_01 mod E */
/* SHMDS_HUB_0202_01 add S */
        if(shub_get_already_md_flg() & 0x02) {
            cmd.prm.u8[0x00] |= 0x02;
        }
/* SHMDS_HUB_0202_01 add E */
        cmd.cmd.u16 = HC_SET_MOTDETECTION_INT;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_MOTDETECTION_INT err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
/* SHMDS_HUB_0202_01 add S */
        if(param[9] & 0x02) {
            if((oldParam9 & 0x01) == (param[9] & 0x01)) {
                if(!(param[9] & 0x80)) {            // SHMDS_HUB_0206_01 add
                    shub_set_already_md_flg(1);
                }
            }
        }
        else if(!(param[9] & 0x02)) {
            if((oldParam9 & 0x01) == (param[9] & 0x01)) {
                shub_clr_already_md_flg(1);
            }
        }
/* SHMDS_HUB_0206_01 mod S */
//        oldParam9 = param[9];
        oldParam9 = param[9] & 0x03;
/* SHMDS_HUB_0206_01 mod S */
/* SHMDS_HUB_0202_01 add E */
    }else if(type == APP_LOW_POWER){
        cmd.cmd.u16 = HC_SET_LPM_PARAM;

        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)(param[1] & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)((param[1] >> 8) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)param[3];
        cmd.prm.u8[0x06] = (uint8_t)param[4];
        cmd.prm.u8[0x07] = (uint8_t)(param[5] & 0xff);
        cmd.prm.u8[0x08] = (uint8_t)((param[5] >> 8) & 0xff);

        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_LPM_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_PEDOMETER2){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)param[3];
        cmd.prm.u8[0x04] = (uint8_t)param[4];
        cmd.prm.u8[0x05] = (uint8_t)param[5];
        cmd.prm.u8[0x06] = (uint8_t)(param[6] & 0xff);
        cmd.prm.u8[0x07] = (uint8_t)((param[6] >> 8) & 0xff);
        cmd.prm.u8[0x08] = (uint8_t)(param[7] & 0xff);
        cmd.prm.u8[0x09] = (uint8_t)((param[7] >> 8) & 0xff);
        cmd.prm.u8[0x0a] = (uint8_t)(param[8] & 0xff);
        cmd.prm.u8[0x0b] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.prm.u8[0x0c] = (uint8_t)(param[9] & 0xff);
        cmd.prm.u8[0x0d] = (uint8_t)((param[9] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_PEDO2_STEP_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 14);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO2_STEP_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
/* SHMDS_HUB_1201_02 add S */
    }else if(type == APP_PEDOMETER2_2){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)param[3];
        cmd.prm.u8[0x04] = (uint8_t)param[4];
        cmd.prm.u8[0x05] = (uint8_t)param[5];
        cmd.prm.u8[0x06] = (uint8_t)(param[6] & 0xff);
        cmd.prm.u8[0x07] = (uint8_t)((param[6] >> 8) & 0xff);
        cmd.prm.u8[0x08] = (uint8_t)(param[7] & 0xff);
        cmd.prm.u8[0x09] = (uint8_t)((param[7] >> 8) & 0xff);
        cmd.prm.u8[0x0a] = (uint8_t)(param[8] & 0xff);
        cmd.prm.u8[0x0b] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.prm.u8[0x0c] = (uint8_t)(param[9] & 0xff);
        cmd.prm.u8[0x0d] = (uint8_t)((param[9] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 14);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
/* SHMDS_HUB_1201_02 add E */
/* SHMDS_HUB_1701_01 add S */
#ifdef SHUB_SW_PICKUP_DETECT
    }else if(type == APP_PICKUP_ENABLE){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];        /* SHMDS_HUB_1701_02 add */ /* SHMDS_HUB_1701_05 del */
//      cmd.prm.u8[0x01] = SHUB_PICKUP_ENABLE_PARAM; /* SHMDS_HUB_1701_05 add */
/* SHMDS_HUB_2701_01 add S */
#ifdef CONFIG_PICKUP_PROX
        cmd.prm.u8[0x01] |= SHUB_PICKUP_ENABLE_PARAM_STILL;
#endif
/* SHMDS_HUB_2701_01 add E */
#ifdef SHUB_SW_PICKUP_ALGO_03 /* SHMDS_HUB_1702_01 add */
        if((cmd.prm.u8[0x01] & SHUB_PICKUP_ENABLE_ALGO_01) == SHUB_PICKUP_ENABLE_ALGO_01){
            cmd.prm.u8[0x01] |= SHUB_PICKUP_ENABLE_ALGO_03;
        }
#endif
        ret = shub_set_delay_exec(SHUB_ACTIVE_PICKUP, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_PICKUP, param[0]);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        cmd.cmd.u16 = HC_PICKUP_SET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2); /* SHMDS_HUB_1701_02 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_PICKUP_SET_ENABLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        shub_pickup_enable = cmd.prm.u8[0x00]; /* SHMDS_HUB_1701_15 add */
        shub_pickup_setflg = cmd.prm.u8[0x01]; /* SHMDS_HUB_1701_15 add */
//  }else if(type == APP_PICKUP_PARAM1){ /* SHMDS_HUB_1701_06 del */
//  }else if(type == APP_PICKUP_PARAM2){ /* SHMDS_HUB_1701_06 del */
#endif
/* SHMDS_HUB_1701_01 add E */
/* SHMDS_HUB_3801_02 add S */
#ifdef SHUB_SW_FREE_FALL_DETECT
    }else if(type == APP_FREE_FALL_ENABLE){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        ret = shub_set_delay_exec(SHUB_ACTIVE_FREE_FALL, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_FREE_FALL, param[0]);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        cmd.cmd.u16 = HC_FALL_SET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1); /* SHMDS_HUB_1701_02 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_FALL_SET_ENABLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
#endif
/* SHMDS_HUB_3801_02 add E */
/* SHMDS_HUB_2301_01 add S */
#ifdef SHUB_SW_TWIST_DETECT
    }else if(type == APP_TWIST_ENABLE){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        ret = shub_set_delay_exec(SHUB_ACTIVE_TWIST, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_TWIST, param[0]);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        cmd.cmd.u16 = HC_TWIST_SET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1); /* SHMDS_HUB_1701_02 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_TWIST_SET_ENABLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
#endif
/* SHMDS_HUB_2301_01 add E */
/* SHMDS_HUB_0204_14 add S */
    }else if(type == APP_PAUSE_PARAM){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];

        cmd.cmd.u16 = HC_SET_ACTIVITY2_PAUSE_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_ACTIVITY2_PAUSE_PARAM);     /* SHMDS_HUB_3302_02 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY2_PAUSE_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        memcpy(s_setcmd_data.cmd_act2_pause, cmd.prm.u8, SHUB_SIZE_ACTIVITY2_PAUSE_PARAM); /* SHMDS_HUB_3302_02 add */
    }else if(type == APP_PAUSE_STATUS_PARAM){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)param[3];
        cmd.prm.u8[0x04] = (uint8_t)param[4];
        cmd.prm.u8[0x05] = (uint8_t)param[5];
        cmd.prm.u8[0x06] = (uint8_t)param[6];
        cmd.prm.u8[0x07] = (uint8_t)param[7];
        cmd.prm.u8[0x08] = (uint8_t)param[8];
        cmd.prm.u8[0x09] = (uint8_t)((param[9] >> 0) & 0xff);
        cmd.prm.u8[0x0a] = (uint8_t)((param[9] >> 8) & 0xff);
        cmd.prm.u8[0x0b] = (uint8_t)param[10];
        cmd.prm.u8[0x0c] = (uint8_t)((param[11] >> 0) & 0xff);
        cmd.prm.u8[0x0d] = (uint8_t)((param[11] >> 8) & 0xff);
        cmd.prm.u8[0x0e] = (uint8_t)((param[12] >> 0) & 0xff);
        cmd.prm.u8[0x0f] = (uint8_t)((param[12] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_ACTIVITY2_PAUSE_STATUS_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 16);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY2_PAUSE_STATUS_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_LPM_PARAM){
        cmd.prm.u8[0x00] = 0; /* SHMDS_HUB_0347_03 mod */
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)((param[3] >> 0) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[3] >> 8) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)param[4];
        cmd.prm.u8[0x06] = (uint8_t)((param[5] >> 0) & 0xff);
        cmd.prm.u8[0x07] = (uint8_t)((param[5] >> 8) & 0xff);
        cmd.prm.u8[0x08] = (uint8_t)param[6];
        cmd.prm.u8[0x09] = (uint8_t)param[7];
        cmd.prm.u8[0x0a] = (uint8_t)((param[8] >> 0) & 0xff);
        cmd.prm.u8[0x0b] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.prm.u8[0x0c] = (uint8_t)((param[9] >> 0) & 0xff);
        cmd.prm.u8[0x0d] = (uint8_t)((param[9] >> 8) & 0xff);
        cmd.prm.u8[0x0e] = (uint8_t)((param[10] >> 0) & 0xff);
        cmd.prm.u8[0x0f] = (uint8_t)((param[10] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_LOW_POWER_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 16);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_LOW_POWER_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        shub_lowpower_mode = 0;           /* SHMDS_HUB_0701_09 add *//* SHMDS_HUB_0347_03 mod */
    }else if(type == APP_LPM_DEV_PARAM){
        cmd.cmd.u16 = HC_SET_LOW_POWER_DEV_PARAM;
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)param[3];             /* SHMDS_HUB_0204_15 add */

#ifdef CONFIG_ACC_U2DH /* SHMDS_HUB_0204_21 SHMDS_HUB_2901_01 */
        cmd.prm.u8[0x04] = (uint8_t)param[4];
        cmd.prm.u8[0x05] = (uint8_t)param[5];
        cmd.prm.u8[0x06] = (uint8_t)param[6];
        cmd.prm.u8[0x07] = (uint8_t)param[7];
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 8);
#else
//      ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 3);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);  /* SHMDS_HUB_0204_15 mod */
#endif
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_LOW_POWER_DEV_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
/* SHMDS_HUB_0204_14 add E */
/* SHMDS_HUB_0204_15 add S */
    }else if(type == APP_RIDE_PEDO_PARAM){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];

        cmd.cmd.u16 = HC_SET_RIDE_PEDO_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_RIDE_PEDO_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_RIDE_PEDO2_PARAM){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];

        cmd.cmd.u16 = HC_SET_RIDE_PEDO2_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_RIDE_PEDO2_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_LPM_DEV_PARAM2){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
/* SHMDS_HUB_0204_16 add S */
        cmd.prm.u8[0x02] = (uint8_t)((param[2] >> 0) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[3] >> 0) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)((param[3] >> 8) & 0xff);
/* SHMDS_HUB_0204_16 add E */

        cmd.cmd.u16 = HC_SET_LOW_POWER_DEV_PARAM2;
/* SHMDS_HUB_0204_16 mod S */
//        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
/* SHMDS_HUB_0204_16 mod E */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_LOW_POWER_DEV_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
/* SHMDS_HUB_0204_15 add E */
/* SHMDS_HUB_2603_02 add S */
    }else if(type == APP_TOTAL_STATUS_DETECTION2){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)(param[1] & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)((param[1] >> 8) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)(param[3] & 0xff);
        cmd.prm.u8[0x06] = (uint8_t)((param[3] >> 8) & 0xff);
        cmd.prm.u8[0x07] = (uint8_t)param[4];
        cmd.prm.u8[0x08] = (uint8_t)param[5];
        cmd.prm.u8[0x09] = (uint8_t)param[6];
        cmd.prm.u8[0x0A] = (uint8_t)param[7];
        cmd.prm.u8[0x0B] = (uint8_t)(param[8] & 0xff);
        cmd.prm.u8[0x0C] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.prm.u8[0x0D] = (uint8_t)param[9];
        cmd.prm.u8[0x0E] = (uint8_t)param[10];

        cmd.cmd.u16 = HC_SET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 15);  /* SHMDS_HUB_2603_01 mod (11->15) */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
/* SHMDS_HUB_2603_02 add E */
/* SHMDS_HUB_0132_01 add S */
    }else if(type == APP_DEVORI_ENABLE){
        ret = shub_set_delay_exec(SHUB_ACTIVE_DEVICE_ORI, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_DEVICE_ORI, param[0]);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        // set 
        cmd.cmd.u16 = HC_ACC_GET_AUTO_MEASURE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_GET_AUTO_MEASURE err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_RC_ERR;
        }else{
            cmd.cmd.u16 = HC_ACC_SET_AUTO_MEASURE;
            cmd.prm.u8[0] = res.res.u8[0];
            cmd.prm.u8[1] = (uint8_t)param[0];
            cmd.prm.u8[2] = res.res.u8[2];
            cmd.prm.u8[3] = res.res.u8[3];
            cmd.prm.u8[4] = res.res.u8[4];
            cmd.prm.u8[5] = res.res.u8[5];
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_ACC_SET_AUTO_MEASURE err(res=0x%x,ret=%d)\n", res.err.u16, ret);
                return SHUB_RC_ERR;
            }
        }
        // set
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
//      cmd.prm.u8[0x02] = (uint8_t)param[2];  /* SHMDS_HUB_2605_01 del */
        cmd.prm.u8[0x03] = (uint8_t)param[3];  /* SHMDS_HUB_2605_01 add */
        cmd.prm.u8[0x04] = (uint8_t)param[4];  /* SHMDS_HUB_2605_01 add */
        cmd.prm.u8[0x05] = (uint8_t)param[5];  /* SHMDS_HUB_2605_01 add */
        cmd.prm.u8[0x06] = (uint8_t)param[6];  /* SHMDS_HUB_2605_01 add */
        cmd.cmd.u16 = HC_ACC_SET_ANDROID_XY;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_DEV_ORI_PARAM);  /* SHMDS_HUB_3303_01 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_SET_ANDROID_XY err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
/* SHMDS_HUB_0132_01 add E */
        memcpy(s_setcmd_data.cmd_dev_ori, cmd.prm.u8, SHUB_SIZE_DEV_ORI_PARAM);  /* SHMDS_HUB_3303_01 add */
    }else{
        return SHUB_RC_ERR;
    }
    return SHUB_RC_OK;
}

static int32_t shub_get_param_exec(int32_t type ,int32_t *param)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret;

    if(type == APP_PEDOMETER){
        cmd.cmd.u16 = HC_GET_PEDO2_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO2_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)res.res.u8[0x00];
        param[1] = (int32_t)res.res.u8[0x01];
        param[2] = (int32_t)res.res.u8[0x02];
        param[3] = (int32_t)res.res.u8[0x03];
        param[4] = (int32_t)res.res.u8[0x04];
        param[5] = (int32_t)res.res.u8[0x05];
        param[6] = (int32_t)res.res.u8[0x06];
        param[7] = (int32_t)res.res.u8[0x07];
        param[8] = (int32_t)res.res.u8[0x08];
        param[9] = (int32_t)res.res.u8[0x09];
        param[10] = (int32_t)res.res.u8[0x0a];
        param[11] = (int32_t)res.res.u8[0x0b];
        param[12] = (int32_t)res.res.u8[0x0c];
        param[13] = (int32_t)res.res.u8[0x0d];          // SHMDS_HUB_0204_14 add
/* SHMDS_HUB_0204_02 add S */
    }else if(type == APP_PEDOMETER_N){
        cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)res.res.u8[0x00];
        param[1] = (int32_t)res.res.u8[0x01];
        param[2] = (int32_t)res.res.u8[0x02];
        param[3] = (int32_t)res.res.u8[0x03];
        param[4] = (int32_t)res.res.u8[0x04];
        param[5] = (int32_t)res.res.u8[0x05];
        param[6] = (int32_t)res.res.u8[0x06];
        param[7] = (int32_t)res.res.u8[0x07];
        param[8] = (int32_t)res.res.u8[0x08];
        param[9] = (int32_t)res.res.u8[0x09];
        param[10] = (int32_t)res.res.u8[0x0a];
        param[11] = (int32_t)res.res.u8[0x0b];
        param[12] = (int32_t)res.res.u8[0x0c];          // SHMDS_HUB_0204_05 add
        param[13] = (int32_t)res.res.u8[0x0d];          // SHMDS_HUB_0204_14 add
/* SHMDS_HUB_0204_02 add E */
    }else if(type == APP_CALORIE_FACTOR){
        cmd.cmd.u16 = HC_GET_PEDO2_CALORIE_FACTOR;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_CALORIE_FACTOR err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)res.res.u8[0x00];
        param[1] = (int32_t)res.res.u8[0x01];
        param[2] = (int32_t)res.res.u8[0x02];
        param[3] = (int32_t)res.res.u8[0x03];
        param[4] = (int32_t)res.res.u8[0x04];
        param[5] = (int32_t)res.res.u8[0x05];
    }else if(type == APP_RUN_DETECTION){
        cmd.cmd.u16 = HC_GET_PEDO2_RUN_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO2_RUN_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,2);
    }else if(type == APP_VEICHLE_DETECTION){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,2);
        param[3] = (int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[7] = (int32_t)(uint8_t)RESU8_TO_X8(res,8);
        param[8] = (int32_t)(uint8_t)RESU8_TO_X8(res,9);
        param[9] = (int32_t)(uint8_t)RESU8_TO_X8(res,10);
        param[10] = (int32_t)(uint8_t)RESU8_TO_X8(res,11);
        param[11] = (int32_t)(uint8_t)RESU8_TO_X8(res,12);
        param[12] = (int32_t)(uint8_t)RESU8_TO_X8(res,13);  // SHMDS_HUB_0204_23 add
    }else if(type == APP_TOTAL_STATUS_DETECTION){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint32_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint16_t)RESU8_TO_X16(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,3);
        param[3] = (int32_t)(uint16_t)RESU8_TO_X16(res,5);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,8);
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,9);
        param[7] = (int32_t)(uint8_t)RESU8_TO_X8(res,10);
        param[8] = (int32_t)(uint16_t)RESU8_TO_X16(res,11); /* SHMDS_HUB_2603_01 add */
        param[9] = (int32_t)(uint16_t)RESU8_TO_X8(res,13);  /* SHMDS_HUB_2603_01 add */
        param[10] = (int32_t)(uint16_t)RESU8_TO_X8(res,14); /* SHMDS_HUB_2603_01 add */

/* SHMDS_HUB_2603_03 add S */
    }else if(type == APP_PEDO_ALL_STATE_2ND){
        cmd.cmd.u16 = HC_ACC_PEDO_GET_ALL_STATE_2ND;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_PEDO_GET_ALL_STATE_2ND err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint32_t)RESU8_TO_X32(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,4);
/* SHMDS_HUB_2603_03 add E */
    }else if(type == APP_VEICHLE_DETECTION2){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_DETECT_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_DETECT_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint16_t)RESU8_TO_X16(res,0);
        param[1] = (int32_t)(uint16_t)RESU8_TO_X16(res,2);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,4);
/* SHMDS_HUB_0209_02 add S */
/* SHMDS_HUB_0204_14 del S */
//      param[3] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
//      param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
//      param[5] = (int32_t)(uint16_t)RESU8_TO_X16(res,8);
//      param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,10);
//      param[7] = (int32_t)(uint8_t)RESU8_TO_X8(res,11);
//      param[8] = (int32_t)(uint16_t)RESU8_TO_X16(res,12);
/* SHMDS_HUB_0204_14 del E */
/* SHMDS_HUB_0209_02 add E */
    }else if(type == APP_VEICHLE_DETECTION3){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_DETECT_PARAM3;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_DETECT_PARAM3 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint16_t)RESU8_TO_X16(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,3);
        param[3] = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[6] = (int32_t)(int8_t)RESU8_TO_X8(res,8);
    }else if(type == APP_GDETECTION){
        cmd.cmd.u16 = HC_GET_GDETECTION_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_GDETECTION_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,2);
        param[3] = (int32_t)(uint16_t)RESU8_TO_X16(res,4);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,8);

    }else if(type == APP_MOTDTECTION){
        cmd.cmd.u16 = HC_GET_MOTDETECTION_EN;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_MOTDETECTION_EN err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);

        cmd.cmd.u16 = HC_GET_MOTDETECTION_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_MOTDETECTION_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[2] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[3] = (int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[7] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[8] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);

        cmd.cmd.u16 = HC_GET_MOTDETECTION_INT;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_MOTDETECTION_INT err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[9] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
/* SHMDS_HUB_0332_01 add S */
        if (param[9] & 0x01) {
            shub_mot_still_enable_flag = true;
        } else {
            shub_mot_still_enable_flag = false;
        }
/* SHMDS_HUB_0332_01 add E */
        param[31] = shub_get_exif_md_mode_flg();        /* SHMDS_HUB_0211_01 SHMDS_HUB_0211_02 add */
    }else if(type == MCU_TASK_CYCLE){
        cmd.cmd.u16 = HC_SENSOR_TSK_GET_CYCLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SENSOR_TSK_GET_CYCLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        param[0] = (int32_t)res.res.u16[0] * 10;
        param[1] = (int32_t)res.res.u16[1] * 10;
        param[2] = (int32_t)res.res.u16[2] * 10;
    }else if(type == APP_LOW_POWER){
        cmd.cmd.u16 = HC_GET_LPM_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_LPM_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] =(int32_t)(uint16_t)RESU8_TO_X16(res,1);
        param[2] =(int32_t)(uint16_t)RESU8_TO_X16(res,3);
        param[3] =(int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[5] =(int32_t)(uint16_t)RESU8_TO_X16(res,7);
    }else if(type == APP_PEDOMETER2){
        cmd.cmd.u16 = HC_GET_PEDO2_STEP_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO2_STEP_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] =(int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] =(int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3] =(int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[5] =(int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[6] =(int32_t)(uint16_t)RESU8_TO_X16(res,6);
        param[7] =(int32_t)(uint16_t)RESU8_TO_X16(res,8);
        param[8] =(int32_t)(uint16_t)RESU8_TO_X16(res,10);
        param[9] =(int32_t)(uint16_t)RESU8_TO_X16(res,12);
/* SHMDS_HUB_1201_02 add S */
    }else if(type == APP_PEDOMETER2_2){
        cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] =(int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] =(int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3] =(int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[5] =(int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[6] =(int32_t)(uint16_t)RESU8_TO_X16(res,6);
        param[7] =(int32_t)(uint16_t)RESU8_TO_X16(res,8);
        param[8] =(int32_t)(uint16_t)RESU8_TO_X16(res,10);
        param[9] =(int32_t)(uint16_t)RESU8_TO_X16(res,12);
/* SHMDS_HUB_1201_02 add E */
/* SHMDS_HUB_1701_01 add S */
#ifdef SHUB_SW_PICKUP_DETECT
    }else if(type == APP_PICKUP_ENABLE){
        cmd.cmd.u16 = HC_PICKUP_GET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_PICKUP_GET_ENABLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1); /* SHMDS_HUB_1701_02 add */
/* SHMDS_HUB_2701_01 add S */
#ifdef CONFIG_PICKUP_PROX
        param[1] &= ~SHUB_PICKUP_ENABLE_PARAM_STILL; /* SHMDS_HUB_1701_12 add */
#endif
#ifdef SHUB_SW_PICKUP_ALGO_03 /* SHMDS_HUB_1702_01 add */
        param[1] &= ~SHUB_PICKUP_ENABLE_ALGO_03;
#endif
/* SHMDS_HUB_2701_01 add E */
//  }else if(type == APP_PICKUP_PARAM1){ /* SHMDS_HUB_1701_06 del */
//  }else if(type == APP_PICKUP_PARAM2){ /* SHMDS_HUB_1701_06 del */
#endif
/* SHMDS_HUB_1701_01 add E */
/* SHMDS_HUB_3801_02 add S */
#ifdef SHUB_SW_FREE_FALL_DETECT
    }else if(type == APP_FREE_FALL_ENABLE){
        cmd.cmd.u16 = HC_FALL_GET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_FALL_GET_ENABLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
#endif
/* SHMDS_HUB_3801_02 add E */
/* SHMDS_HUB_2301_01 add S */
#ifdef SHUB_SW_TWIST_DETECT
    }else if(type == APP_TWIST_ENABLE){
        cmd.cmd.u16 = HC_TWIST_GET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_TWIST_GET_ENABLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
#endif
/* SHMDS_HUB_2301_01 add E */
/* SHMDS_HUB_0204_14 add S */
    }else if(type == APP_PAUSE_PARAM){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_PAUSE_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_PAUSE_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] = (int32_t)(uint8_t)RESU8_TO_X8(res,2);
    }else if(type == APP_PAUSE_STATUS_PARAM){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_PAUSE_STATUS_PARAM;
        cmd.prm.u8[0x00] = param[0];
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_PAUSE_STATUS_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2]  = (int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3]  = (int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[4]  = (int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[5]  = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[6]  = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[7]  = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[8]  = (int32_t)(uint8_t)RESU8_TO_X8(res,8);
        param[9]  = (int32_t)(uint16_t)RESU8_TO_X16(res,9);
        param[10] = (int32_t)(uint8_t)RESU8_TO_X8(res,11);
        param[11] = (int32_t)(uint16_t)RESU8_TO_X16(res,12);
        param[12] = (int32_t)(uint16_t)RESU8_TO_X16(res,14);
    }else if(type == APP_LPM_PARAM){
        cmd.cmd.u16 = HC_GET_LOW_POWER_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_LOW_POWER_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2]  = (int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3]  = (int32_t)(uint16_t)RESU8_TO_X16(res,3);
        param[4]  = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[5]  = (int32_t)(uint16_t)RESU8_TO_X16(res,6);
        param[6]  = (int32_t)(uint8_t)RESU8_TO_X8(res,8);
        param[7]  = (int32_t)(uint8_t)RESU8_TO_X8(res,9);
        param[8]  = (int32_t)(uint16_t)RESU8_TO_X16(res,10);
        param[9]  = (int32_t)(uint16_t)RESU8_TO_X16(res,12);
        param[10] = (int32_t)(uint16_t)RESU8_TO_X16(res,14);
    }else if(type == APP_LPM_DEV_PARAM){
        cmd.cmd.u16 = HC_GET_LOW_POWER_DEV_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_LOW_POWER_DEV_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2]  = (int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3]  = (int32_t)(uint8_t)RESU8_TO_X8(res,3);          /* SHMDS_HUB_0204_15 add */
#ifdef CONFIG_ACC_U2DH /* SHMDS_HUB_0204_21 SHMDS_HUB_2901_01 */
        param[4]  = (int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[5]  = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[6]  = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[7]  = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
#endif
/* SHMDS_HUB_0204_14 add E */
/* SHMDS_HUB_0204_15 add S */
    }else if(type == APP_RIDE_PEDO_PARAM){
        cmd.cmd.u16 = HC_GET_RIDE_PEDO_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_RIDE_PEDO_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
    }else if(type == APP_RIDE_PEDO2_PARAM){
        cmd.cmd.u16 = HC_GET_RIDE_PEDO2_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_RIDE_PEDO2_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
    }else if(type == APP_LPM_DEV_PARAM2){
        cmd.cmd.u16 = HC_GET_LOW_POWER_DEV_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_LOW_POWER_DEV_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2]  = (int32_t)(uint16_t)RESU8_TO_X16(res,2);      // SHMDS_HUB_0204_16 add
        param[3]  = (int32_t)(uint16_t)RESU8_TO_X16(res,4);      // SHMDS_HUB_0204_16 add
/* SHMDS_HUB_0204_15 add E */
/* SHMDS_HUB_2603_02 add S */
    }else if(type == APP_TOTAL_STATUS_DETECTION2){
        cmd.cmd.u16 = HC_GET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint32_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint16_t)RESU8_TO_X16(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,3);
        param[3] = (int32_t)(uint16_t)RESU8_TO_X16(res,5);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,8);
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,9);
        param[7] = (int32_t)(uint8_t)RESU8_TO_X8(res,10);
        param[8] = (int32_t)(uint16_t)RESU8_TO_X16(res,11);
        param[9] = (int32_t)(uint8_t)RESU8_TO_X8(res,13);
        param[10] = (int32_t)(uint8_t)RESU8_TO_X8(res,14);
/* SHMDS_HUB_2603_02 add E */
/* SHMDS_HUB_0132_01 add S */
    }else if(type == APP_DEVORI_ENABLE){
        cmd.cmd.u16 = HC_ACC_GET_ANDROID_XY;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_GET_ANDROID_XY err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
//      param[2] = (int32_t)(uint8_t)RESU8_TO_X8(res,2);  /* SHMDS_HUB_2605_01 del */
        param[3] = (int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,4);  /* SHMDS_HUB_2605_01 add */
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,5);  /* SHMDS_HUB_2605_01 add */
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);  /* SHMDS_HUB_2605_01 add */
/* SHMDS_HUB_0132_01 add E */
    }else{
        return SHUB_RC_ERR;
    }

    return SHUB_RC_OK;
}

static int32_t shub_clear_data_app_exec(int32_t type)
{
    HostCmd cmd;
    HostCmdRes res;
    uint8_t cmd_len=0;
    int32_t ret;

    if(type == APP_PEDOMETER){
        cmd.cmd.u16 = HC_CLR_PEDO2_STEP_DATA;
    }else if(type == APP_RUN_DETECTION){
        cmd.cmd.u16 = HC_CLR_PEDO2_RUN_DETECT_DATA;
    }else if(type == APP_VEICHLE_DETECTION){
        cmd.cmd.u16 = HC_CLR_ACTIVITY2_DETECT_DATA;
    }else if(type == APP_TOTAL_STATUS_DETECTION_CLR_CONT_STEPS){
        cmd.cmd.u16 = HC_CLR_ACTIVITY2_TOTAL_DETECT_CONT;
        cmd_len=1;
        cmd.prm.u8[0]= 0x01;
    }else if(type == APP_TOTAL_STATUS_DETECTION_CLR_CONT_STOP){
        cmd.cmd.u16 = HC_CLR_ACTIVITY2_TOTAL_DETECT_CONT;
        cmd_len=1;
        cmd.prm.u8[0]= 0x02;
/* SHMDS_HUB_0212_01 add S */
    }else if(type == APP_CLEAR_PAUSE_PARAM){
        cmd.cmd.u16 = HC_CLR_ACTIVITY2_PAUSE_PARAM;
/* SHMDS_HUB_0212_01 add E */
    }else{
        return SHUB_RC_ERR;
    }

    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, cmd_len);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_CLR_XXX err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    return SHUB_RC_OK;
}

static int32_t shub_init_app_exec(int32_t type)
{
    HostCmd cmd;
    HostCmdRes res;
    uint8_t cmd_len=0;
    int32_t ret;

    if(type == APP_CLEAR_PEDOM_AND_TOTAL_STATUS_DETECTION){
        cmd.cmd.u16 = HC_INIT_PEDO_AND_ACTIVITY_DETECT;
    }else{
        return SHUB_RC_ERR;
    }

    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, cmd_len);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_INIT_XXX err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    return SHUB_RC_OK;
}

static int32_t shub_get_data_app_exec(int32_t arg_iType, int32_t *param)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret;

    if(arg_iType == APP_PEDOMETER){
        cmd.cmd.u16 = HC_GET_PEDO2_STEP_DATA;
        cmd.prm.u32[0x00] = 0x0000ffff;
        cmd.prm.u32[0x01] = 0x01ffffff;
        cmd.prm.u8[0x08] = 0x00;                          /* SHMDS_HUB_2603_01 add */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);  /* SHMDS_HUB_2603_01 mod (8->9) */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO2_STEP_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint32_t)RESU8_TO_X32(res,0);
        param[1] =(int32_t)(uint32_t)RESU8_TO_X32(res,4);
        param[2] =(int32_t)(uint16_t)RESU8_TO_X16(res,8);
        param[3] =(int32_t)(uint32_t)RESU8_TO_X32(res,10);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,14);
        param[5] =(int32_t)(uint16_t)RESU8_TO_X16(res,15);
        param[6] =(int32_t)(uint16_t)RESU8_TO_X16(res,17);
        param[7] =(int32_t)(uint16_t)RESU8_TO_X16(res,19);
        param[8] =(int32_t)(uint32_t)RESU8_TO_X32(res,21);
        param[9] =(int32_t)(uint32_t)RESU8_TO_X32(res,25);
        param[10] =(int32_t)(uint32_t)RESU8_TO_X32(res,29);
        param[11] =(int32_t)(uint16_t)RESU8_TO_X16(res,33);
        param[12] =(int32_t)(uint32_t)RESU8_TO_X32(res,35);
        param[13] =(int32_t)(uint32_t)RESU8_TO_X32(res,39);
        param[14] =(int32_t)(uint32_t)RESU8_TO_X32(res,43);
        param[15] =(int32_t)(uint8_t)RESU8_TO_X8(res,47);
        param[16] =(int32_t)(uint32_t)RESU8_TO_X32(res,48);
        param[17] =(int32_t)(uint32_t)RESU8_TO_X32(res,52);
        param[18] =(int32_t)(uint32_t)RESU8_TO_X32(res,56);
        param[19] =(int32_t)(uint32_t)RESU8_TO_X32(res,60);
        param[20] =(int32_t)(uint32_t)RESU8_TO_X32(res,64);
        param[21] =(int32_t)(uint32_t)RESU8_TO_X32(res,68);
        param[22] =(int32_t)(uint32_t)RESU8_TO_X32(res,72);
        param[23] =(int32_t)(uint32_t)RESU8_TO_X32(res,76);
        param[24] =(int32_t)(uint32_t)RESU8_TO_X32(res,80);
        param[25] =(int32_t)(uint32_t)RESU8_TO_X32(res,84);
        param[26] =(int32_t)(uint32_t)RESU8_TO_X32(res,88);
        param[27] =(int32_t)(uint32_t)RESU8_TO_X32(res,92);
        param[28] =(int32_t)(uint32_t)RESU8_TO_X32(res,96);
        param[29] =(int32_t)(uint16_t)RESU8_TO_X16(res,100);
        param[30] =(int32_t)(uint16_t)RESU8_TO_X16(res,102);
        param[31] =(int32_t)(uint16_t)RESU8_TO_X16(res,104);
        param[32] =(int32_t)(uint16_t)RESU8_TO_X16(res,106);
        param[33] =(int32_t)(uint16_t)RESU8_TO_X16(res,108);
        param[34] =(int32_t)(uint16_t)RESU8_TO_X16(res,110);
        param[35] =(int32_t)(uint32_t)RESU8_TO_X32(res,112);
        param[36] =(int32_t)(uint32_t)RESU8_TO_X32(res,116);
        param[37] =(int32_t)(uint32_t)RESU8_TO_X32(res,120);
        param[38] =(int32_t)(uint32_t)RESU8_TO_X32(res,124);
        param[39] =(int32_t)(uint32_t)RESU8_TO_X32(res,128);
        param[40] =(int32_t)(uint32_t)RESU8_TO_X32(res,132);

    }else if(arg_iType == APP_NORMAL_PEDOMETER){
        cmd.cmd.u16 = HC_GET_PEDO_STEP_DATA;
        cmd.prm.u32[0x00] = 0x0000ffff;
        cmd.prm.u32[0x01] = 0x01ffffff;
        cmd.prm.u8[0x08] = 0x00;                          /* SHMDS_HUB_2603_01 add */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);  /* SHMDS_HUB_2603_01 mod (8->9) */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
//      param[0] =(int32_t)(uint32_t)RESU8_TO_X32(res,0);
        param[0] =(int32_t)((uint32_t)RESU8_TO_X32(res,0) + (uint32_t)s_recovery_data.bk_OffsetStep); /* SHMDS_HUB_3301_01 mod */
        param[1] =(int32_t)(uint32_t)RESU8_TO_X32(res,4);
        param[2] =(int32_t)(uint16_t)RESU8_TO_X16(res,8);
        param[3] =(int32_t)(uint32_t)RESU8_TO_X32(res,10);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,14);
        param[5] =(int32_t)(uint16_t)RESU8_TO_X16(res,15);
        param[6] =(int32_t)(uint16_t)RESU8_TO_X16(res,17);
        param[7] =(int32_t)(uint16_t)RESU8_TO_X16(res,19);
        param[8] =(int32_t)(uint32_t)RESU8_TO_X32(res,21);
        param[9] =(int32_t)(uint32_t)RESU8_TO_X32(res,25);
        param[10] =(int32_t)(uint32_t)RESU8_TO_X32(res,29);
        param[11] =(int32_t)(uint16_t)RESU8_TO_X16(res,33);
        param[12] =(int32_t)(uint32_t)RESU8_TO_X32(res,35);
        param[13] =(int32_t)(uint32_t)RESU8_TO_X32(res,39);
        param[14] =(int32_t)(uint32_t)RESU8_TO_X32(res,43);
        param[15] =(int32_t)(uint8_t)RESU8_TO_X8(res,47);
        param[16] =(int32_t)(uint32_t)RESU8_TO_X32(res,48);
//      param[17] =(int32_t)(uint32_t)RESU8_TO_X32(res,52);
        param[17] =(int32_t)((uint32_t)RESU8_TO_X32(res,52) + (uint32_t)s_recovery_data.bk_OffsetStep); /* SHMDS_HUB_3301_01 mod */
        param[18] =(int32_t)(uint32_t)RESU8_TO_X32(res,56);
        param[19] =(int32_t)(uint32_t)RESU8_TO_X32(res,60);
        param[20] =(int32_t)(uint32_t)RESU8_TO_X32(res,64);
        param[21] =(int32_t)(uint32_t)RESU8_TO_X32(res,68);
        param[22] =(int32_t)(uint32_t)RESU8_TO_X32(res,72);
        param[23] =(int32_t)(uint32_t)RESU8_TO_X32(res,76);
        param[24] =(int32_t)(uint32_t)RESU8_TO_X32(res,80);
        param[25] =(int32_t)(uint32_t)RESU8_TO_X32(res,84);
        param[26] =(int32_t)(uint32_t)RESU8_TO_X32(res,88);
        param[27] =(int32_t)(uint32_t)RESU8_TO_X32(res,92);
        param[28] =(int32_t)(uint32_t)RESU8_TO_X32(res,96);
        param[29] =(int32_t)(uint16_t)RESU8_TO_X16(res,100);
        param[30] =(int32_t)(uint16_t)RESU8_TO_X16(res,102);
        param[31] =(int32_t)(uint16_t)RESU8_TO_X16(res,104);
        param[32] =(int32_t)(uint16_t)RESU8_TO_X16(res,106);
        param[33] =(int32_t)(uint16_t)RESU8_TO_X16(res,108);
        param[34] =(int32_t)(uint16_t)RESU8_TO_X16(res,110);
        param[35] =(int32_t)(uint32_t)RESU8_TO_X32(res,112);
        param[36] =(int32_t)(uint32_t)RESU8_TO_X32(res,116);
        param[37] =(int32_t)(uint32_t)RESU8_TO_X32(res,120);
        param[38] =(int32_t)(uint32_t)RESU8_TO_X32(res,124);
        param[39] =(int32_t)(uint32_t)RESU8_TO_X32(res,128);
        param[40] =(int32_t)(uint32_t)RESU8_TO_X32(res,132);
    }else if(arg_iType == APP_RUN_DETECTION){
        cmd.cmd.u16 = HC_GET_PEDO2_RUN_DETECT_DATA;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_RUN_DETECT_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint32_t)RESU8_TO_X8(res,0);
    }else if(arg_iType == APP_VEICHLE_DETECTION){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_DETECT_DATA;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_DETECT_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] =(int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] =(int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3] =(int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[5] =(int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[6] =(int32_t)(uint8_t)RESU8_TO_X8(res,6);     // SHMDS_HUB_0209_02 add
    }else if(arg_iType == APP_TOTAL_STATUS_DETECTION){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_TOTAL_DETECT_DATA;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);

        cmd.cmd.u16 = HC_GET_ACTIVITY2_TOTAL_DETECT_INFO;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_INFO err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[1] =(int32_t)(uint16_t)RESU8_TO_X16(res,0);
        param[2] =(int32_t)(uint16_t)RESU8_TO_X16(res,2);
        param[3] =(int32_t)(uint32_t)RESU8_TO_X32(res,4);
        param[4] =(int32_t)(uint32_t)RESU8_TO_X32(res,8);
        param[5] =(int32_t)(uint16_t)RESU8_TO_X16(res,12);
    }else if(arg_iType == APP_MOTDTECTION){
        cmd.cmd.u16 = HC_GET_MOTDETECTION_SENS;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_MOTDETECTION_SENS err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(int16_t)RESU8_TO_X16(res,0);
        param[1] =(int32_t)(int16_t)RESU8_TO_X16(res,2);
        param[2] =(int32_t)(int16_t)RESU8_TO_X16(res,4);

    }else if(arg_iType == APP_LOW_POWER){
        cmd.cmd.u16 = HC_GET_LPM_INFO;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_LPM_INFO err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);
    }else{
        return SHUB_RC_ERR;
    }

    return SHUB_RC_OK;
}

static int32_t shub_activate_pedom_exec(int32_t arg_iSensType, int32_t arg_iEnable)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    int32_t iCurrentEnable =iCurrentSensorEnable|iCurrentLoggingEnable;
    int32_t iCurrentEnable_old =iCurrentSensorEnable|iCurrentLoggingEnable;

    bool setpCounterOn2OffFlg;
    bool setpCounterOff2OnFlg;
    bool setpCounterBatchOn2OffFlg;
    bool setpCounterBatchOff2OnFlg;

    if((arg_iSensType & (STEPCOUNT_GROUP_MASK|STEPDETECT_GROUP_MASK)) == 0){
        return SHUB_RC_OK;
    }

    if(arg_iEnable == POWER_ENABLE){
        iCurrentEnable |= arg_iSensType;
    }else{
        iCurrentEnable &= ~arg_iSensType;
    }

    setpCounterOn2OffFlg = (iCurrentEnable_old & SHUB_ACTIVE_PEDOM) != 0 && 
        (arg_iSensType & SHUB_ACTIVE_PEDOM) != 0 && 
        (arg_iEnable == POWER_DISABLE);

    setpCounterBatchOn2OffFlg = (iCurrentEnable_old & SHUB_ACTIVE_PEDOM_NO_NOTIFY) != 0 && 
        (arg_iSensType & SHUB_ACTIVE_PEDOM_NO_NOTIFY) != 0 && 
        (arg_iEnable == POWER_DISABLE);

    setpCounterOff2OnFlg = (iCurrentEnable_old & SHUB_ACTIVE_PEDOM) == 0 && 
        (arg_iSensType & SHUB_ACTIVE_PEDOM) != 0 && 
        (arg_iEnable == POWER_ENABLE);

    setpCounterBatchOff2OnFlg = (iCurrentEnable_old & SHUB_ACTIVE_PEDOM_NO_NOTIFY) == 0 && 
        (arg_iSensType & SHUB_ACTIVE_PEDOM_NO_NOTIFY) != 0 && 
        (arg_iEnable == POWER_ENABLE);


    //StepCounter ON->OFF 
    if(setpCounterOn2OffFlg || setpCounterBatchOn2OffFlg) {
        cmd.cmd.u16 = HC_GET_PEDO_STEP_DATA;
        cmd.prm.u32[0] = 1;
        cmd.prm.u32[1] = 0;
        cmd.prm.u8[0x08] = 0x00;                            /* SHMDS_HUB_2603_01 add */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);    /* SHMDS_HUB_2603_01 mod (8->9) */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        //Update offset
//      s_tLatestStepCountData.stepDis = res.res.u32[0];
        s_tLatestStepCountData.stepDis = res.res.u32[0] + s_recovery_data.bk_OffsetStep; /* SHMDS_HUB_3301_01 mod */
    }

    //StepCounter OFF->ON 
    if( setpCounterOff2OnFlg || setpCounterBatchOff2OnFlg ){
        cmd.cmd.u16 = HC_GET_PEDO_STEP_DATA;
        cmd.prm.u32[0] = 1;
        cmd.prm.u32[1] = 0;
        cmd.prm.u8[0x08] = 0x00;                            /* SHMDS_HUB_2603_01 add */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);    /* SHMDS_HUB_2603_01 mod (8->9) */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        //Update offset
//      s_tLatestStepCountData.stepOffset += (res.res.u32[0]-s_tLatestStepCountData.stepDis);
        s_tLatestStepCountData.stepOffset += ((res.res.u32[0] + s_recovery_data.bk_OffsetStep) - s_tLatestStepCountData.stepDis); /* SHMDS_HUB_3301_01 mod */
    }

    /* Enable/Disable */
    cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0335_01 mod */

    /* CountStep */
    if((iCurrentEnable & (STEPCOUNT_GROUP_MASK |
                    STEPDETECT_GROUP_MASK |
                    SHUB_ACTIVE_SIGNIFICANT)) != 0){
        cmd.prm.u8[0] = 1;
    }else{
        cmd.prm.u8[0] = 0;
    }

    /* NotifyStep */
    if(iCurrentEnable & (SHUB_ACTIVE_PEDODEC | SHUB_ACTIVE_PEDOM)){
        cmd.prm.u8[3] = 1;
        s_enable_notify_step = true;
    }else{
        cmd.prm.u8[3] = 0;
        s_enable_notify_step = false;
    }

    cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
    DBG(DBG_LV_INFO, "Enable PEDO en=%d onstep=%d\n" ,cmd.prm.u8[0],cmd.prm.u8[3]);
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0335_01 mod */
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable);
/* SHMDS_HUB_0901_01 SHMDS_HUB_3703_01 add S */
#if defined(CONFIG_SHARP_SHTERM) && defined(SHUB_SW_SHTERM)
    if(iCurrentEnable != iCurrentEnable_old){
        shterm_k_set_info(SHTERM_INFO_PEDOMETER, (cmd.prm.u8[0] || cmd.prm.u8[3] )? 1 : 0 );
    }
#endif /* CONFIG_SHARP_SHTERM && SHUB_SW_SHTERM */
/* SHMDS_HUB_0901_01 SHMDS_HUB_3703_01 add E */
    return SHUB_RC_OK;
}

static int32_t shub_activate_significant_exec(int32_t arg_iSensType, int32_t arg_iEnable, uint8_t * notify)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    int32_t iCurrentEnable_old =iCurrentSensorEnable;

    if((arg_iSensType & SHUB_ACTIVE_SIGNIFICANT) == 0){
        return SHUB_RC_OK;
    }

    if(arg_iEnable == POWER_ENABLE){
        iCurrentSensorEnable |= SHUB_ACTIVE_SIGNIFICANT;
    }else{
        iCurrentSensorEnable &= ~SHUB_ACTIVE_SIGNIFICANT;
    }

    /* Disable Step*/
    cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0335_01 mod */

    cmd.prm.u8[0] = 0;
    cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0335_01 mod */
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    /* Enable */
    if((iCurrentEnable_old & SHUB_ACTIVE_SIGNIFICANT) == 0 && 
            (arg_iSensType & SHUB_ACTIVE_SIGNIFICANT) != 0 && 
            (arg_iEnable == POWER_ENABLE)){
        /* Enable */
        cmd.cmd.u16 = HC_GET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        memcpy(cmd.prm.u8,res.res.u8, 15);  /* SHMDS_HUB_2603_01 mod (9->15) */
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] = 0xf5;
        cmd.prm.u8[2] = 0x03;
        cmd.prm.u8[3] = 21;
        cmd.prm.u8[4] = 0;

        cmd.cmd.u16 = HC_SET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 15);  /* SHMDS_HUB_2603_01 mod (9->15) */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        cmd.cmd.u16 = HC_GET_ACTIVITY_TOTAL_DETECT_DATA;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        //DBG(DBG_LV_ERROR, "status(%x)\n", res.res.u8[0]);
        if(res.res.u8[0] != 0){
            if(((iCurrentLoggingEnable | iCurrentEnable_old) & PEDOM_GROUP_MASK) != 0){
                *notify=1;
            }
        }
        //DBG(DBG_LV_ERROR, "notify(%x)\n", *notify);

        if(*notify == 0 || res.res.u8[0] == 0){
            /* Clear Run Detection */
            cmd.cmd.u16 = HC_CLR_PEDO_RUN_DETECT_DATA;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_CLR_PEDO_RUN_DETECT_DATA err(%x)\n", res.err.u16);
                return SHUB_RC_ERR;
            }

            /* Clear Transport Detection */
            cmd.cmd.u16 = HC_CLR_ACTIVITY_DETECT_DATA;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_CLR_ACTIVITY_DETECT_DATA err(%x)\n", res.err.u16);
                return SHUB_RC_ERR;
            }
        }
    }

    /* Disable */
    if((iCurrentEnable_old & SHUB_ACTIVE_SIGNIFICANT) != 0 && 
            (arg_iSensType & SHUB_ACTIVE_SIGNIFICANT) != 0 && 
            (arg_iEnable == POWER_DISABLE)){
        /* Disable */
        cmd.cmd.u16 = HC_GET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        memcpy(cmd.prm.u8,res.res.u8, 15);  /* SHMDS_HUB_2603_01 mod (9->15) */
        cmd.prm.u8[0] = 0;

        cmd.cmd.u16 = HC_SET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 15);  /* SHMDS_HUB_2603_01 mod (9->15) */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }

    /* Enable Step*/
    cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0335_01 mod */

    if(((iCurrentSensorEnable | iCurrentLoggingEnable) & (STEPCOUNT_GROUP_MASK |
                    STEPDETECT_GROUP_MASK |
                    SHUB_ACTIVE_SIGNIFICANT)) != 0){
        cmd.prm.u8[0] = 1;
    }else{
        cmd.prm.u8[0] = 0;
    }

    cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0335_01 mod */
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable);
/* SHMDS_HUB_0901_01 SHMDS_HUB_3703_01 add S */
#if defined(CONFIG_SHARP_SHTERM) && defined(SHUB_SW_SHTERM)
    if(iCurrentSensorEnable != iCurrentEnable_old){
        shterm_k_set_info(SHTERM_INFO_PEDOMETER, (cmd.prm.u8[0])? 1 : 0 );
    }
#endif /* CONFIG_SHARP_SHTERM && SHUB_SW_SHTERM */
/* SHMDS_HUB_0901_01 SHMDS_HUB_3703_01 add E */
    return SHUB_RC_OK;
}

/* SHMDS_HUB_0403_01 del S */
// #ifdef SHUB_SUSPEND
// static int32_t shub_suspend_sensor_exec(void)
// {
//     HostCmd cmd;
//     HostCmdRes res;
//     int32_t ret = SHUB_RC_OK;
//     uint8_t iEnableSensor = 0;
//     int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
//     int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
//     uint32_t enable_sensor;
// 
//     enable_sensor=iCurrentSensorEnable | iCurrentLoggingEnable;
// 
//     //check enable logging
//     if(iCurrentLoggingEnable & (FUSION_GROUP_ACC_MASK | SHUB_ACTIVE_ACC | APPTASK_GROUP_ACC_MASK)){
//         iEnableSensor |= HC_ACC_VALID;
//     }
// 
//     //check enable sensor measure 
//     //(stepcounter stepdetector singnificant motiondec gdetect extpedom)
//     if(iCurrentSensorEnable & (APPTASK_GROUP_ACC_MASK | SHUB_ACTIVE_GDEC)){
//         iEnableSensor |= HC_ACC_VALID;
//     }
// 
// /* SHMDS_HUB_2302_01 add S */
//     if(iCurrentSensorEnable & SHUB_ACTIVE_TWIST){
//         iEnableSensor |= HC_MAG_VALID;
//         iEnableSensor |= HC_GYRO_VALID;
//     }
// /* SHMDS_HUB_2302_01 add E */
// 
//     //check enable logging mag 
//     if(iCurrentLoggingEnable & ACTIVE_MAG_GROUP_MASK){
//         iEnableSensor |= HC_MAG_VALID;
//     }
// 
//     //check enable logging gyro 
//     if(iCurrentLoggingEnable & ACTIVE_GYRO_GROUP_MASK){
//         iEnableSensor |= HC_GYRO_VALID;
//     }
// 
// /* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
// #ifdef CONFIG_BARO_SENSOR
//     //check enable logging baro
//     if(iCurrentLoggingEnable & BARO_GROUP_MASK){
//         iEnableSensor |= HC_BARO_VALID;
//     }
// #endif
// /* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
// 
//     /***** Set sensor *****/
//     cmd.cmd.u16 = HC_SENSOR_SET_PARAM;
//     cmd.prm.u8[0] = iEnableSensor & s_exist_sensor;
//     cmd.prm.u8[1] = 0;
//     if(s_micon_param.sensors != cmd.prm.u8[0]){ 
//         ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
//         if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
//             DBG(DBG_LV_ERROR, "HC_SENSOR_SET_PARAM err(%x)\n", res.err.u16);
//             return SHUB_RC_ERR;
//         }
//     }
//     s_micon_param.sensors = cmd.prm.u8[0];
// 
//     /***** Set Task *****/
//     cmd.cmd.u16 = HC_TSK_EXECUTE;
//     cmd.prm.u8[0] =  (iEnableSensor != 0)?1:0;
//     //check stepcounter stepdetector singnificant motiondec extpedom 
//     cmd.prm.u8[1] = ((enable_sensor & APPTASK_GROUP_MASK) != 0)?1:0;
//     // logging only
//     cmd.prm.u8[2] = ((iCurrentLoggingEnable & ACTIVE_MAG_GROUP_MASK) != 0)?1:0;
//     if(s_micon_param.task[0] != cmd.prm.u8[0] ||
//             s_micon_param.task[1] != cmd.prm.u8[1] || 
//             s_micon_param.task[2] != cmd.prm.u8[2] ){
//         ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 3);
//         if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
//             DBG(DBG_LV_ERROR, "HC_TSK_EXECUTE err(%x)\n", res.err.u16);
//             return SHUB_RC_ERR;
//         }
//     }
//     s_micon_param.task[0] = cmd.prm.u8[0];
//     s_micon_param.task[1] = cmd.prm.u8[1];
//     s_micon_param.task[2] = cmd.prm.u8[2];
// 
//     return SHUB_RC_OK;
// }
// #endif
/* SHMDS_HUB_0403_01 del E */

static int32_t shub_activate_exec(int32_t arg_iSensType, int32_t arg_iEnable)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    uint8_t iEnableSensor = 0;
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    int32_t iCurrentSensorEnable_tmp = iCurrentSensorEnable;
    int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    uint32_t enable_sensor;
    uint32_t enable_sensor_acc;         /* SHMDS_HUB_0313_01 add */
    uint8_t setCal = 0;
    uint32_t enableGyro = 0;
    uint32_t waitFusion = 0;
    int32_t lpParam[16];            /* SHMDS_HUB_0317_01 add */
    uint8_t newDeepSleepMode = 0;   /* SHMDS_HUB_0317_01 add */
    uint8_t newAccMode = 0;         /* SHMDS_HUB_0317_01 add */

    DBG(DBG_LV_INFO, "####%s %s[%x] en=%d [logging=%x] [sensor=%x]\n",
            __FUNCTION__,
            shub_get_active_sensor_name(arg_iSensType),
            arg_iSensType, 
            arg_iEnable,
            iCurrentLoggingEnable,
            iCurrentSensorEnable
       );

    if(arg_iEnable == POWER_ENABLE){
        iCurrentSensorEnable |= arg_iSensType;
    }else{
        iCurrentSensorEnable &= ~arg_iSensType;
    }
    enable_sensor=iCurrentSensorEnable | iCurrentLoggingEnable;

/* SHMDS_HUB_0206_03 add S */
/* SHMDS_HUB_0313_01 mod S */
    enable_sensor_acc = enable_sensor & (SHUB_ACTIVE_ACC | SHUB_ACTIVE_SHEX_ACC);
    if(enable_sensor_acc == (SHUB_ACTIVE_ACC | SHUB_ACTIVE_SHEX_ACC)) {
        s_sensor_delay_us.acc = SHUB_MIN(shub_get_acc_delay_ms() * 1000, s_sensor_delay_us.acc);
        s_sensor_delay_us.acc = SHUB_MIN(shub_get_exif_delay_ms() * 1000, s_sensor_delay_us.acc);
    }
    else if(enable_sensor_acc == SHUB_ACTIVE_ACC) {
        if(arg_iSensType == SHUB_ACTIVE_SHEX_ACC) {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_acc_delay_ms() * 1000, MEASURE_MAX_US);
        }
        else {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_acc_delay_ms() * 1000, s_sensor_delay_us.acc);
        }
    }
    else if(enable_sensor_acc == SHUB_ACTIVE_SHEX_ACC) {
        if(arg_iSensType == SHUB_ACTIVE_ACC) {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_exif_delay_ms() * 1000, MEASURE_MAX_US);
        }
        else {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_exif_delay_ms() * 1000, s_sensor_delay_us.acc);
        }
    }
    else {
        s_sensor_delay_us.acc = MEASURE_MAX_US;
    }
/* SHMDS_HUB_0313_01 mod E */
/* SHMDS_HUB_0206_03 add E */

    if(enable_sensor & ACTIVE_ACC_GROUP_MASK){ /* SHMDS_HUB_0201_01 mod */
        iEnableSensor |= HC_ACC_VALID;
    }
    if(enable_sensor & ACTIVE_MAG_GROUP_MASK){
        //gyro calibrataion needs magnetic field
        iEnableSensor |= HC_MAG_VALID;
    }
    if(enable_sensor & ACTIVE_GYRO_GROUP_MASK){
        iEnableSensor |= HC_GYRO_VALID;
    }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    if(enable_sensor & BARO_GROUP_MASK){
        iEnableSensor |= HC_BARO_VALID;
    }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */

    if(iCurrentSensorEnable & ACTIVE_MAG_GROUP_MASK){
        setCal |= HC_MAG_VALID;
    }
//  if(iCurrentSensorEnable & ACTIVE_GYRO_GROUP_MASK){                        /* SHMDS_HUB_0344_01 mod */
    if(iCurrentSensorEnable & (ACTIVE_GYRO_GROUP_MASK & ~SHUB_ACTIVE_TWIST)){ /* SHMDS_HUB_0344_01 mod */
        setCal |= HC_GYRO_VALID;
    }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    if(iCurrentSensorEnable & BARO_GROUP_MASK){
        setCal |= HC_BARO_VALID;
    }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */

    if(arg_iEnable == POWER_ENABLE && iEnableSensor == 0){
        /*none*/
        DBG(DBG_LV_ERROR, "error sensor measure_normal_report unkonw sensor type[%x]\n", arg_iSensType);
        return SHUB_RC_ERR;
    }

    atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable);
    /* update period */
    ret = shub_set_delay_exec(0,0);
    if(ret != SHUB_RC_OK) {
        atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
        return SHUB_RC_ERR;
    }

    /***** Set Mag calibration *****/
    if((s_exist_sensor & HC_MAG_VALID) != 0){
        cmd.cmd.u16 = HC_MAG_SET_CAL;
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] =(setCal & HC_MAG_VALID) != 0 ? 1 : 0; 
        if(s_micon_param.mag_cal != cmd.prm.u8[1]){
            DBG(DBG_LV_INFO, "notify calib MAG=%d ", cmd.prm.u8[1]);
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_MAG_SET_CAL err(%x)\n", res.err.u16);
                atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
                return SHUB_RC_ERR;
            }
        }
        s_micon_param.mag_cal = cmd.prm.u8[1];
    }

    /***** Set Gyro calibration *****/
    if((s_exist_sensor & HC_GYRO_VALID) != 0){
        cmd.cmd.u16 = HC_GYRO_SET_CAL;
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] = (setCal & HC_GYRO_VALID) != 0 ? 1 : 0;
        if(s_micon_param.gyro_cal != cmd.prm.u8[1] ){
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
            DBG(DBG_LV_INFO, "GYRO=%d\n", cmd.prm.u8[1]);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_GYRO_SET_CAL err(%x)\n", res.err.u16);
                atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
                return SHUB_RC_ERR;
            }
        }
        s_micon_param.gyro_cal = cmd.prm.u8[1] ;
    }
/* SHMDS_HUB_0120_10 add S */
#ifdef CONFIG_BARO_SENSOR
    if((enable_sensor & SHUB_ACTIVE_BARO) != 0 && (iCurrentSensorEnable_tmp & SHUB_ACTIVE_BARO) == 0) {
        memset(&s_tLatestBaroData, 0x00 , sizeof(s_tLatestBaroData));
    }
#endif
/* SHMDS_HUB_0120_10 add E */

/* SHMDS_HUB_0120_01 mod S  SHMDS_HUB_0122_01 add S  SHMDS_HUB_0332_01 SHMDS_HUB_2304_01 mod S */
#ifdef CONFIG_BARO_SENSOR
/* SHMDS_HUB_0317_01 add S */
    if ( enable_sensor &
        ( SHUB_ACTIVE_ACC | GYRO_GROUP_MASK | MAG_GROUP_MASK | SHUB_ACTIVE_BARO | SHUB_ACTIVE_RV_NONGYRO | SHUB_ACTIVE_RV_NONMAG | FUSION_9AXIS_GROUP_MASK | FUSION_6AXIS_GYRO_MAG_MASK) ||
        ((enable_sensor & SHUB_ACTIVE_MOTIONDEC) && shub_mot_still_enable_flag) ) {
#else
    if ( enable_sensor &
        ( SHUB_ACTIVE_ACC | GYRO_GROUP_MASK | MAG_GROUP_MASK | SHUB_ACTIVE_RV_NONGYRO | SHUB_ACTIVE_RV_NONMAG | FUSION_9AXIS_GROUP_MASK | FUSION_6AXIS_GYRO_MAG_MASK) ||
        ((enable_sensor & SHUB_ACTIVE_MOTIONDEC) && shub_mot_still_enable_flag) ) {
#endif
/* SHMDS_HUB_0120_01 mod E  SHMDS_HUB_0122_01 add E  SHMDS_HUB_0332_01 SHMDS_HUB_2304_01 mod E */
        newAccMode = 0;
        newDeepSleepMode = 0;
    } else {
        newAccMode = 1;
        newDeepSleepMode = 1;
    }

/* SHMDS_HUB_0326_02 add S */
#ifdef CONFIG_ACC_U2DH
    if(s_sensor_task_delay_us < 8000){
        newAccMode = 0;
    }
#endif
/* SHMDS_HUB_0326_02 add E */
    
    /* SHMDS_HUB_0318_01 del */
    /***** Set DeepSleep Disable *****/
    if((newDeepSleepMode == 0) && (shub_lowpower_mode != newDeepSleepMode)){ /* SHMDS_HUB_3101_01 mod */
        memset(lpParam, 0, sizeof(lpParam));
        ret = shub_get_param_exec(APP_LPM_PARAM, lpParam);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "HC_GET_LPM_PARAM err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }
        if(lpParam[0] != newDeepSleepMode){
            lpParam[0] = newDeepSleepMode;
            ret = shub_set_param_exec(APP_LPM_PARAM, lpParam);
            if(SHUB_RC_OK != ret){
                DBG(DBG_LV_ERROR, "HC_SET_LPM_PARAM err(%x)\n", res.err.u16);
                atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
                return SHUB_RC_ERR;
            }
/* SHMDS_HUB_0318_01 add S */
            cmd.cmd.u16 = HC_CLR_LOW_POWER_MODE;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_CLR_LOW_POWER_MODE err(%x)\n", res.err.u16);
                return SHUB_RC_ERR;
            }
/* SHMDS_HUB_0318_01 add E */
        }
    }
/* SHMDS_HUB_0317_01 add E */
/* SHMDS_HUB_0318_01 SHMDS_HUB_3101_01 add S */
    /***** Set Freerun *****/
    if(shub_operation_mode != newAccMode){
        cmd.cmd.u16 = HC_ACC_SET_OPERATION_MODE;
        cmd.prm.u8[0] = newAccMode;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_SET_OPERATION_MODE err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }
        shub_operation_mode = newAccMode; /* SHMDS_HUB_0701_09 add */
    }
/* SHMDS_HUB_0318_01 SHMDS_HUB_3101_01 add E */


    /***** Set sensor *****/
    cmd.cmd.u16 = HC_SENSOR_SET_PARAM;
    cmd.prm.u8[0] = iEnableSensor & s_exist_sensor;
    cmd.prm.u8[1] = 0;
    if(s_micon_param.sensors != cmd.prm.u8[0]){ 
        DBG(DBG_LV_INFO, "Enable Sens=%x\n" ,cmd.prm.u8[0]);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SENSOR_SET_PARAM err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }
        if(((s_micon_param.sensors & HC_GYRO_VALID) == 0) && 
                ((cmd.prm.u8[0] & HC_GYRO_VALID) == HC_GYRO_VALID)){
            //Gyro Off->On
            enableGyro = 1;
        }
    }
    s_micon_param.sensors = cmd.prm.u8[0];

    {
        uint32_t using_gyro = ACTIVE_GYRO_GROUP_MASK;
        bool gyroSensorOnFlg = 
            ((arg_iSensType & using_gyro) != 0)
            &&
            (arg_iEnable == POWER_ENABLE);

        bool gyroSensorEnabledFlg =
            ((iCurrentLoggingEnable | iCurrentSensorEnable_tmp) & using_gyro) == 0;

        if(gyroSensorOnFlg && gyroSensorEnabledFlg){
            DBG(DBG_LV_INFO,  "gyroscope 130ms wait(activate_exec)\n");
            usleep(130 * 1000);//gyroscope 130ms wait /* SHMDS_HUB_0102_11 mod */
        }
    }

    /***** Set Fusion *****/
    cmd.cmd.u16 = HC_SET_FUISON_PARAM;
    /* SHMDS_HUB_0214_01 mod S */
    //  cmd.prm.u8[0] = ((enable_sensor & FUSION_GROUP_MASK) != 0)?1:0;
    cmd.prm.u8[0] = 0;
    if((enable_sensor & SHUB_ACTIVE_ORI ) != 0) {
        cmd.prm.u8[0] |= HC_ORI_VALID;
    }
    if((enable_sensor & SHUB_ACTIVE_GRAVITY) != 0){
        cmd.prm.u8[0] |= HC_GRAVITY_VALID;
    }
    if((enable_sensor & SHUB_ACTIVE_LACC) != 0){
        cmd.prm.u8[0] |= HC_LACC_VALID;
    }
    if((enable_sensor & SHUB_ACTIVE_RV) != 0){
        cmd.prm.u8[0] |= HC_RV_VALID;
    }
    if((enable_sensor & SHUB_ACTIVE_RV_NONMAG) != 0){
        cmd.prm.u8[0] |= HC_RV_NONMAG_VALID;
    }
    if((enable_sensor & SHUB_ACTIVE_RV_NONGYRO) != 0){
        cmd.prm.u8[0] |= HC_RV_NONGYRO_VALID;
    }
    /* SHMDS_HUB_0214_01 mod E */
    if(s_micon_param.fusion != cmd.prm.u8[0]){  
        DBG(DBG_LV_INFO, "Enable fusion=%x\n",cmd.prm.u8[0]);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_FUISON_PARAM err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }

        /* SHMDS_HUB_0214_01 mod S */
        //if(cmd.prm.u8[0]== 1){
        if((cmd.prm.u8[0] | s_micon_param.fusion) != s_micon_param.fusion){
        /* SHMDS_HUB_0214_01 mod E */
            waitFusion = 1;
        }
    }

    /* SHMDS_HUB_0214_01 del S */
    //if((s_micon_param.fusion == 1) && (cmd.prm.u8[0] == 1) && (enableGyro == 1)){
    //    //Change Fusion mode
    //    //Enable 9-Axis
    //    if(((arg_iSensType & GYRO_GROUP_MASK) == 0) && (arg_iEnable == POWER_ENABLE)){
    //        waitFusion = 1;
    //    }
    //}
    /* SHMDS_HUB_0214_01 del E */
    s_micon_param.fusion = cmd.prm.u8[0];

    /***** Set Task *****/
    cmd.cmd.u16 = HC_TSK_EXECUTE;
    cmd.prm.u8[0] =  (enable_sensor != 0)?1:0;
    cmd.prm.u8[1] = ((enable_sensor & APPTASK_GROUP_MASK) != 0)?1:0;
    cmd.prm.u8[2] = ((enable_sensor & ACTIVE_MAG_GROUP_MASK) != 0)?1:0;
    if(s_micon_param.task[0] != cmd.prm.u8[0] ||
            s_micon_param.task[1] != cmd.prm.u8[1] || 
            s_micon_param.task[2] != cmd.prm.u8[2] ){
        DBG(DBG_LV_INFO, "Enable TASK sens=%x app=%x fusion=%x\n" ,cmd.prm.u8[0],cmd.prm.u8[1],cmd.prm.u8[2]);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 3);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_TSK_EXECUTE err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }
    }
    s_micon_param.task[0] = cmd.prm.u8[0];
    s_micon_param.task[1] = cmd.prm.u8[1];
    s_micon_param.task[2] = cmd.prm.u8[2];

    if(waitFusion == 1){
        usleep(80 * 1000);//Fusion 80ms wait  /* SHMDS_HUB_0102_11 mod */
        DBG(DBG_LV_INFO,  "Fusion 80ms wait\n");
    }

/* SHMDS_HUB_0317_01 add S */
    /***** Set DeepSleep Enable *****/
    if((newDeepSleepMode == 1) && (shub_lowpower_mode != newDeepSleepMode)){ /* SHMDS_HUB_3101_01 mod */
        memset(lpParam, 0, sizeof(lpParam));
        ret = shub_get_param_exec(APP_LPM_PARAM, lpParam);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "HC_GET_LPM_PARAM err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }
        if(lpParam[0] != newDeepSleepMode){
            lpParam[0] = newDeepSleepMode;
            ret = shub_set_param_exec(APP_LPM_PARAM, lpParam);
            if(SHUB_RC_OK != ret) {
                DBG(DBG_LV_ERROR, "HC_SET_LPM_PARAM err(%x)\n", res.err.u16);
                atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
                return SHUB_RC_ERR;
            }
        }
    }
/* SHMDS_HUB_0317_01 add E */

/* SHMDS_HUB_0901_01 SHMDS_HUB_3703_01 add S */
#if defined(CONFIG_SHARP_SHTERM) && defined(SHUB_SW_SHTERM)
    if(enable_sensor & ACTIVE_ACC_GROUP_MASK){
        shterm_k_set_info(SHTERM_INFO_ACCELE, 1);
    }else{
        shterm_k_set_info(SHTERM_INFO_ACCELE, 0);
    }

    if(enable_sensor & ACTIVE_MAG_GROUP_MASK){
        shterm_k_set_info(SHTERM_INFO_COMPS, 1);
    }else{
        shterm_k_set_info(SHTERM_INFO_COMPS, 0);
    }

    if(enable_sensor & ACTIVE_GYRO_GROUP_MASK){
        shterm_k_set_info(SHTERM_INFO_GYRO, 1);
    }else{
        shterm_k_set_info(SHTERM_INFO_GYRO, 0);
    }
#endif /* CONFIG_SHARP_SHTERM && SHUB_SW_SHTERM */
/* SHMDS_HUB_0901_01 SHMDS_HUB_3703_01 add E */
    return SHUB_RC_OK;
}

static int32_t shub_activate_logging_exec(int32_t arg_iSensType, int32_t arg_iEnable)
{
    int32_t ret;
    int32_t set_time_flag = 0; /* SHMDS_HUB_0312_01 add */
    HostCmd cmd;
    HostCmdRes res;
    uint16_t hcSensor;
    uint16_t hcFusion;
    int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    int32_t iCurrentLoggingEnable_tmp = iCurrentLoggingEnable;
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    uint16_t iTimerCnt = 0;   /* SHMDS_HUB_2604_01 add */

    DBG(DBG_LV_INFO, "####%s %s[%x] en=%d [logging=%x]\n",
            __FUNCTION__,
            shub_get_active_sensor_name(arg_iSensType),
            arg_iSensType, 
            arg_iEnable,
            iCurrentLoggingEnable
       );

    if(arg_iEnable == POWER_ENABLE){
        iCurrentLoggingEnable |= arg_iSensType;
    }else{
        iCurrentLoggingEnable &= ~arg_iSensType;
    }

    hcSensor = 0;
    hcFusion = 0;
    if((iCurrentLoggingEnable & SHUB_ACTIVE_ACC) != 0){
        hcSensor |= HC_ACC_VALID;
    }
    if((iCurrentLoggingEnable & MAG_GROUP_MASK) != 0){
        hcSensor |= HC_MAG_VALID;
    }
    if((iCurrentLoggingEnable & GYRO_GROUP_MASK) != 0){
        hcSensor |= HC_GYRO_VALID;
    }
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    if((iCurrentLoggingEnable & BARO_GROUP_MASK) != 0){
        hcSensor |= HC_BARO_VALID;
    }
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
    if((iCurrentLoggingEnable & SHUB_ACTIVE_ORI) != 0){
        hcFusion |= HC_ORI_VALID;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_GRAVITY) != 0){
        hcFusion |= HC_GRAVITY_VALID;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_LACC) != 0){
        hcFusion |= HC_LACC_VALID;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV) != 0){
        hcFusion |= HC_RV_VALID;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONMAG) != 0){
        hcFusion |= HC_RV_NONMAG_VALID;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONGYRO) != 0){
        hcFusion |= HC_RV_NONGYRO_VALID;
    }

    atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable);

    ret = shub_activate_exec(0, 0);//update sensor measure cycle
    if(SHUB_RC_OK != ret){
        DBG(DBG_LV_ERROR, "shub_activate_exec\n");
        atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable_tmp);
        return SHUB_RC_ERR;
    }
    
/* SHMDS_HUB_0120_10 add S */
#ifdef CONFIG_BARO_SENSOR
    if((iCurrentLoggingEnable & SHUB_ACTIVE_BARO) != 0 && (iCurrentLoggingEnable_tmp & SHUB_ACTIVE_BARO) == 0) {
        memset(&s_tLoggingBaroData, 0x00 , sizeof(s_tLoggingBaroData));
    }
#endif
/* SHMDS_HUB_0120_10 add E */

    cmd.cmd.u16 = HC_LOGGING_SENSOR_SET_PARAM;
    cmd.prm.u16[0] = hcSensor & s_exist_sensor;
    if(s_micon_param.logg_sensors != cmd.prm.u16[0]){ 
        DBG(DBG_LV_INFO, "Enable Logging Sens=%x " ,cmd.prm.u16[0]);
        shub_basetime_req = 1;                               /* SHMDS_HUB_0312_01 add */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_SENSOR_SET_PARAM err(%x)\n", res.err.u16);
            shub_basetime_req = 0;                           /* SHMDS_HUB_0312_01 add */
            atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable_tmp);
            return SHUB_RC_ERR;
        }
        iTimerCnt = (uint16_t)RESU8_TO_X16(res,4);           /* SHMDS_HUB_2604_01 add */
        set_time_flag = 1;                                   /* SHMDS_HUB_0312_01 add */
    }
    s_micon_param.logg_sensors = cmd.prm.u16[0];

    {
        uint32_t using_gyro = ACTIVE_GYRO_GROUP_MASK;
        bool gyroSensorOnFlg = 
            ((arg_iSensType & using_gyro) != 0)
            &&
            (arg_iEnable == POWER_ENABLE);

        bool gyroSensorEnabledFlg =
            ((iCurrentLoggingEnable_tmp | iCurrentSensorEnable) & using_gyro) == 0;

        if(gyroSensorOnFlg && gyroSensorEnabledFlg){
//          DBG(DBG_LV_INFO,  "gyroscope 130ms wait(logging_exec)\n");             /* SHMDS_HUB_0312_01 del */
//          usleep(130 * 1000);//gyroscope 130ms wait  /* SHMDS_HUB_0102_11 mod */ /* SHMDS_HUB_0312_01 del */
            shub_set_gyroEnable_time(&shub_baseTime, iTimerCnt);                   /* SHMDS_HUB_2604_01 mod */
        }
    }

    cmd.cmd.u16 = HC_LOGGING_FUSION_SET_PARAM;
    cmd.prm.u16[0] = hcFusion;
    if(s_micon_param.logg_fusion != cmd.prm.u16[0]){ 
        DBG(DBG_LV_INFO, " Fusion=%x\n" ,cmd.prm.u16[0]);
        shub_basetime_req = 1;                               /* SHMDS_HUB_0312_01 add */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_FUSION_SET_PARAM err(%x)\n", res.err.u16);
            shub_basetime_req = 0;                           /* SHMDS_HUB_0312_01 add */
            atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable_tmp);
            return SHUB_RC_ERR;
        }
        iTimerCnt = (uint16_t)RESU8_TO_X16(res,4);           /* SHMDS_HUB_2604_01 add */
        set_time_flag = 1;                                   /* SHMDS_HUB_0312_01 add */
    }
    s_micon_param.logg_fusion = cmd.prm.u16[0];

/* SHMDS_HUB_3101_01 mod S */
    cmd.cmd.u16 = HC_LOGGING_SET_PEDO;
    if((iCurrentLoggingEnable & PEDOM_GROUP_MASK) != 0){
        cmd.prm.u16[0] = HC_FLG_LOGGING_PEDO;
    }else{
        cmd.prm.u16[0] = 0;
    }
    if(s_micon_param.logg_pedo != cmd.prm.u16[0]){ 
        DBG(DBG_LV_INFO, "Logging Pedo=%x\n", cmd.prm.u16[0]);
        /* SHMDS_HUB_0312_01 add  S */
        if(set_time_flag == 0){
            shub_basetime_req = 1;
        }
        /* SHMDS_HUB_0312_01 add  E */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_SET_PEDO err(%x)\n", res.err.u16);
            shub_basetime_req = 0;                               /* SHMDS_HUB_0312_01 add */
            update_base_time(ACTIVE_FUNC_MASK,NULL);
            atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable_tmp);
            return SHUB_RC_ERR;
        }
        /* SHMDS_HUB_2604_01 add S */
        if(set_time_flag == 0){
            iTimerCnt = (uint16_t)RESU8_TO_X16(res,4);
        }
        /* SHMDS_HUB_2604_01 add E */
    }
    s_micon_param.logg_pedo = cmd.prm.u16[0];
/* SHMDS_HUB_3101_01 mod E */

    if(arg_iEnable == POWER_ENABLE && 
            (iCurrentLoggingEnable_tmp & arg_iEnable) == 0 ){
//      pending_base_time(arg_iSensType);                    /* SHMDS_HUB_0312_01 del */
//      pending_base_time(arg_iSensType, &shub_baseTime);    /* SHMDS_HUB_0312_01 SHMDS_HUB_2604_01 */
        pending_base_time(arg_iSensType, &shub_baseTime, iTimerCnt); /* SHMDS_HUB_2604_01 */
        update_base_time(arg_iSensType,NULL);
    }

    logging_flush_exec();
    return SHUB_RC_OK;
}

#ifndef NO_LINUX
static int32_t shub_gpio_init(void)
{
    int32_t ret;

#ifndef SHUB_SW_GPIO_PMIC /* SHMDS_HUB_0104_03 del */
#ifdef USE_RESET_SIGNAL   
    ret = shub_gpio_request(SHUB_GPIO_PIN_RESET);  /* SHMDS_HUB_0110_01 mod */
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_request ret=%d\n",ret);
        return ret;
    }

#ifndef SHUB_SW_PINCTRL 
    ret = shub_gpio_direction_output(SHUB_GPIO_PIN_RESET, 1);  /* SHMDS_HUB_0110_01 mod */
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_request ret=%d\n",ret);
        goto ERROR;
    }
#endif /* SHUB_SW_PINCTRL */
#endif
#endif /* SHUB_SW_GPIO_PMIC */

#ifdef USE_REMAP_SIGNAL
    ret = shub_gpio_request(SHUB_GPIO_PIN_BRMP);  /* SHMDS_HUB_0110_01 mod */
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_request ret=%d\n",ret);
        return ret;
    }

#ifndef SHUB_SW_PINCTRL 
    ret = shub_gpio_direction_output(SHUB_GPIO_PIN_BRMP, 1);  /* SHMDS_HUB_0110_01 mod */
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_request ret=%d\n",ret);
        goto ERROR;
    }
#endif
#endif

    g_nIntIrqNo = shub_gpio_to_irq(SHUB_GPIO_PIN_INT0);  /* SHMDS_HUB_0110_01 mod */
    atomic_set(&g_bIsIntIrqEnable, true);
    ret = shub_gpio_request(SHUB_GPIO_PIN_INT0);  /* SHMDS_HUB_0110_01 mod */
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_request ret=%d\n",ret);
        goto ERROR;
    }
    ret = shub_gpio_direction_input(SHUB_GPIO_PIN_INT0);  /* SHMDS_HUB_0110_01 mod */
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_direction_input ret=%d\n",ret);
        goto ERROR;
    }

    ret = request_any_context_irq(g_nIntIrqNo, shub_irq_handler, IRQF_TRIGGER_LOW, SHUB_GPIO_INT0_NAME, NULL);
    if(ret < 0) {
        DBG(DBG_LV_ERROR, "Failed request_any_context_irq. ret=%x\n", ret);
        goto ERROR;
    }

    return SHUB_RC_OK;

ERROR:
    shub_gpio_free(SHUB_GPIO_PIN_INT0);  /* SHMDS_HUB_0110_01 mod */

#ifndef SHUB_SW_GPIO_PMIC /* SHMDS_HUB_0104_03 del */
#ifdef USE_RESET_SIGNAL   
    shub_gpio_free(SHUB_GPIO_PIN_RESET);  /* SHMDS_HUB_0110_01 mod */
#endif
#endif

#ifdef USE_REMAP_SIGNAL
    shub_gpio_free(SHUB_GPIO_PIN_BRMP);  /* SHMDS_HUB_0110_01 mod */
#endif
    return -ENODEV;
}
#endif

static void shub_workqueue_init(void)
{
    int32_t i;

    for(i=0; i<ACC_WORK_QUEUE_NUM; i++){
        cancel_work_sync(&s_tAccWork[i].work);
        s_tAccWork[i].status = false;
    }
    s_nAccWorkCnt = 0;
}

static int32_t shub_workqueue_create( struct workqueue_struct *queue, void (*func)(struct work_struct *) )
{
    int32_t ret = SHUB_RC_ERR;
    unsigned long flags;

    if((queue == NULL) || (func == NULL)){
        return SHUB_RC_ERR;
    }

    spin_lock_irqsave(&acc_lock, flags);

    if(s_tAccWork[s_nAccWorkCnt].status == false){

        INIT_WORK( &s_tAccWork[s_nAccWorkCnt].work, func );

        ret = queue_work( queue, &s_tAccWork[s_nAccWorkCnt].work );

        if (ret == 1) {
            s_tAccWork[s_nAccWorkCnt].status = true;

            if(++s_nAccWorkCnt >= ACC_WORK_QUEUE_NUM){
                s_nAccWorkCnt = 0;
            }
            ret = SHUB_RC_OK;

        }else{
            DBG(DBG_LV_ERROR, "ACC %s[%d] queue_work Non Create(%d) \n",__FUNCTION__, __LINE__, ret);
        }

    }else{
        DBG(DBG_LV_ERROR, "ACC queue_work[%d] used!! \n", s_nAccWorkCnt);
    }

    spin_unlock_irqrestore(&acc_lock, flags);

    return ret;
}

static void shub_workqueue_delete(struct work_struct *work)
{
    int32_t i;
    unsigned long flags;

    spin_lock_irqsave(&acc_lock, flags);

    for(i=0; i<ACC_WORK_QUEUE_NUM; i++){

        if(&s_tAccWork[i].work == work){
            s_tAccWork[i].status = false;
            break;
        }
    }

    spin_unlock_irqrestore(&acc_lock, flags);
    return ;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//
// PUBLIC SYMBOL
//
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* SHMDS_HUB_0103_01 add S */
static HostCmdRes direct_cmdres;

int32_t shub_direct_sendcmd(uint16_t cmd, const uint8_t *prm)
{
    int i;
    int32_t ret;
    HostCmd hcmd;

/* SHMDS_HUB_0801_01 mod S */
    if(prm == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0801_01 mod E */

    for(i = 0; i < 9; i++){
        direct_cmdres.res.u8[i] = 0;
    }
    direct_cmdres.err.u16 = 0;
    
    hcmd.cmd.u16 = cmd;
    memcpy(hcmd.prm.u8, prm, 16);

    ret = shub_hostcmd(&hcmd, &direct_cmdres, EXE_HOST_ALL, 16);

    return ret;
}

int32_t shub_noreturn_sendcmd(uint16_t cmd, const uint8_t *prm)
{
    int i;
    int32_t ret;
    HostCmd hcmd;

/* SHMDS_HUB_0801_01 mod S */
    if(prm == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0801_01 mod E */

    for(i = 0; i < 9; i++){
        direct_cmdres.res.u8[i] = 0;
    }
    direct_cmdres.err.u16 = 0;
    
    hcmd.cmd.u16 = cmd;
    memcpy(hcmd.prm.u8, prm, 16);

    ret = shub_hostcmd(&hcmd, &direct_cmdres, 0, 16);

    return ret;
}

int32_t shub_direct_get_error(uint16_t *error)
{
/* SHMDS_HUB_0801_01 mod S */
    if(error == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0801_01 mod E */

    *error = direct_cmdres.err.u16;
    return 0;
}

int32_t shub_direct_get_result(uint8_t *result)
{
    int i;

/* SHMDS_HUB_0801_01 mod S */
    if(result == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0801_01 mod E */

    for(i = 0; i < 2; i++){
        result[i] = direct_cmdres.res.u8[i];
    }
    return 0;
}

int32_t shub_direct_multi_get_result(uint8_t *result)
{
    int i;

/* SHMDS_HUB_0801_01 mod S */
    if(result == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0801_01 mod E */

    for(i = 0; i < 9; i++){
        result[i] = direct_cmdres.res.u8[i];
    }
    return 0;
}

int32_t shub_hostif_write(uint8_t adr, const uint8_t *data, uint16_t size)
{
    int32_t i;
    int32_t ret = SHUB_RC_OK;

/* SHMDS_HUB_0801_01 mod S */
    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0801_01 mod E */
    SHUB_DBG_SPIW(adr, data, size); // SHMDS_HUB_0104_08 add
    for(i=0; i<ACC_SPI_RETRY_NUM; i++)
    {
        ret = hostif_write_exe(adr, data, size); /* SHMDS_HUB_0705_01 mod */
        if(ret == 0){
            return 0;

        }else if(ret == -EBUSY){
            DBG(DBG_LV_ERROR, "write EBUSY error(Retry:%d, addr=0x%02x, size=%d)\n",i,adr,size); /* SHMDS_HUB_0701_12 mod */
            usleep(100 * 1000);

        }else{
            DBG(DBG_LV_ERROR, "write Other error(addr=0x%02x, size=%d)\n",adr,size); /* SHMDS_HUB_0701_12 mod */
            break;
        }
    }

    return ret;
}

int32_t shub_hostif_read(uint8_t adr, uint8_t *data, uint16_t size)
{
    int32_t i;
    int32_t ret = SHUB_RC_OK;

/* SHMDS_HUB_0801_01 mod S */
    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0801_01 mod E */

    for(i=0; i<ACC_SPI_RETRY_NUM; i++)
    {
        ret = hostif_read_exe(adr, data, size); /* SHMDS_HUB_0705_01 mod */
        if(ret == 0){
            SHUB_DBG_SPIR(adr, data, size); // SHMDS_HUB_0104_08 add
            return 0;

        }else if(ret == -EBUSY){
            DBG(DBG_LV_ERROR, "read EBUSY error(Retry:%d, addr=0x%02x, size=%d)\n",i,adr,size); /* SHMDS_HUB_0701_12 mod */
            usleep(100 * 1000);

        }else{
            DBG(DBG_LV_ERROR, "read Other error(addr=0x%02x, size=%d)\n",adr,size); /* SHMDS_HUB_0701_12 mod */
            break;
        }
    }

    return ret;
}

int32_t shub_cmd_wite(struct IoctlDiagCmdReq *arg_cmd)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    int32_t i;
    uint8_t flag = EXE_HOST_ALL;
    uint8_t err_intreq[4];
    
/* SHMDS_HUB_0801_01 mod S */
    if(arg_cmd == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0801_01 mod E */

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return 0;
    }
    
    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));
    
    cmd.cmd.u16 = (uint16_t)arg_cmd->m_Cmd;
    for(i=0; i<arg_cmd->m_req_size; i++) {
        cmd.prm.u8[i] = arg_cmd->m_buf[i];
    }
    
    if(cmd.cmd.u16 == HC_MCU_ASSERT_INT){
        flag = 0;
        DISABLE_IRQ;
    }
    
    if((cmd.cmd.u16 == HC_MCU_DEASSERT_INT) || (cmd.cmd.u16 == HC_MCU_SOFT_RESET)){
        flag = 0;
    }
    
    ret = shub_hostcmd(&cmd, &res, flag, arg_cmd->m_req_size);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "error Command Write %x\n", res.err.u16);
        return SHUB_RC_ERR;
    }
    
    if(cmd.cmd.u16 == HC_MCU_DEASSERT_INT){
        hostif_read(ERROR0, err_intreq, 4);
        ENABLE_IRQ;
    }
    
    return ret;
}

int32_t shub_cmd_read(struct IoctlDiagCmdReq *arg_cmd)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    int32_t i;
    
/* SHMDS_HUB_0801_01 mod S */
    if(arg_cmd == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0801_01 mod E */

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return 0;
    }
    
    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));
    
    cmd.cmd.u16 = (uint16_t)arg_cmd->m_Cmd;
    for(i=0; i<arg_cmd->m_req_size; i++) {
        cmd.prm.u8[i] = arg_cmd->m_buf[i];
    }

    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, arg_cmd->m_req_size);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "error Command Read %x\n", res.err.u16);
        return SHUB_RC_ERR;
    }
    
    arg_cmd->m_res_size = res.res_size;
    
    for(i=0; i<res.res_size; i++) {
        arg_cmd->m_buf[i] = res.res.u8[i];
    }
    return ret;
}

static int32_t shub_int_enable(int enable)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[16];
    
    memset(prm, 0, sizeof(prm));
    
    if((enable != 0) && (enable != 1)){
        DBG(DBG_LV_ERROR, "%s error enable=%d\n", __func__, enable);
        return -1;
    }
    
    if(enable == 1){
        // GPIO66(PU -> PD)
        shub_gpio_tlmm_config(SHUB_GPIO_PIN_INT0, SHUB_GPIO_PULL_DOWN); /* SHMDS_HUB_0110_01 mod */
        
        prm[0] = 1;
        ret = shub_direct_sendcmd(HC_MCU_SET_INT1, prm);
        shub_direct_get_error(&errRes);
        if((ret != 0) || (errRes != 0)) {
            DBG(DBG_LV_ERROR, "%s(1) error ret=%d,errRes=%d\n", __func__, ret, errRes);
            return -1;
        }
    }else{
        // GPIO66(PD -> PU)
        shub_gpio_tlmm_config(SHUB_GPIO_PIN_INT0, SHUB_GPIO_PULL_UP); /* SHMDS_HUB_0110_01 mod */
        
        prm[0] = 0;
        ret = shub_direct_sendcmd(HC_MCU_SET_INT1, prm);
        shub_direct_get_error(&errRes);
        if((ret != 0) || (errRes != 0)) {
            printk("[shub]%s(0) error ret=%d,errRes=%d\n", __func__, ret, errRes);
            return -1;
        }
    }
    return ret;
}

#ifdef CONFIG_ACC_U2DH /* SHMDS_HUB_0119_01 SHMDS_HUB_2901_01 */
static int32_t shub_lis2dh_low_power(void)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[16];
    
    memset(prm, 0, sizeof(prm));
    prm[0] = 0x00;
    prm[1] = 0x01;
    prm[2] = 0x1E;
    prm[3] = 0x90;
    ret = shub_direct_sendcmd(HC_MCU_ACCESS_SENSOR, prm);
    shub_direct_get_error(&errRes);
    if((ret != 0) || (errRes != 0)) {
        DBG(DBG_LV_ERROR, "%s error ret=%d,errRes=%d\n", __func__, ret, errRes);
        return -1;
    }

    return 0;
}
#endif

int32_t shub_get_int_state(int *state)
{
    int32_t ret;
    int32_t ret_val = 0;
    int gpio_state;
    uint8_t err_intreq[4];
    
/* SHMDS_HUB_0801_01 mod S */
    if(state == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0801_01 mod E */

    ret = shub_int_enable(1);
    if(ret != 0) {
        ret_val = -1;
    }
    
    usleep(1 * 1000); /* SHMDS_HUB_0102_11 mod */
    
#ifdef SHUB_SW_GPIO_INT1 /* SHMDS_HUB_0115_01 mod */
    gpio_state = shub_gpio_get_value(SHUB_GPIO_PIN_INT0) | (shub_gpio_get_value(SHUB_GPIO_PIN_INT1) << 1); /* SHMDS_HUB_0110_01 mod */
#else
    gpio_state = shub_gpio_get_value(SHUB_GPIO_PIN_INT0) | 0x02;
#endif
    
    ret = shub_int_enable(0);
    if(ret != 0) {
        ret_val = -1;
    }
    
    hostif_read(ERROR0, err_intreq, 4);
    
    *state = gpio_state;
    
    return ret_val;
}
/* SHMDS_HUB_0103_01 add E */

int32_t shub_init_common( void )
{
//  uint8_t reg = 0xFF; /* SHMDS_HUB_0322_03 del */
//  int32_t cnt = 1;    /* SHMDS_HUB_0322_03 SHMDS_HUB_0322_04 del */
    int32_t iWakeupSensor;
//  Word sreg;          /* SHMDS_HUB_0322_03 del */

    DBG(DBG_LV_INFO, "%s : register_init\n", __FUNCTION__);

    DISABLE_IRQ;

    atomic_set(&g_CurrentSensorEnable,ACTIVE_OFF);
    atomic_set(&g_CurrentLoggingSensorEnable,ACTIVE_OFF);
    iWakeupSensor = atomic_read(&g_WakeupSensor);
    atomic_set(&g_WakeupSensor,iWakeupSensor & ~ACTIVE_FUNC_MASK);
    s_lsi_id = LSI_ML630Q790;
    memset(&s_micon_param, 0x00, sizeof(s_micon_param));
    s_micon_param.gyro_filter=1;

#ifdef SHUB_SUSPEND
    s_is_suspend = false;
#endif
    mutex_lock(&s_tDataMutex);
    memset(&s_tLatestAccData       , 0x00 , sizeof(s_tLatestAccData));
    memset(&s_tLatestGyroData      , 0x00 , sizeof(s_tLatestGyroData));
    memset(&s_tLatestMagData       , 0x00 , sizeof(s_tLatestMagData));
    memset(&s_tLatestBaroData      , 0x00 , sizeof(s_tLatestBaroData));  /* SHMDS_HUB_0120_01 add */
    memset(&s_tLoggingBaroData     , 0x00 , sizeof(s_tLoggingBaroData)); /* SHMDS_HUB_0120_10 add */
    memset(&s_tLatestOriData       , 0x00 , sizeof(s_tLatestOriData));
    memset(&s_tLatestGravityData   , 0x00 , sizeof(s_tLatestGravityData));
    memset(&s_tLatestLinearAccData , 0x00 , sizeof(s_tLatestLinearAccData));
    memset(&s_tLatestRVectData     , 0x00 , sizeof(s_tLatestRVectData));
    memset(&s_tLatestGameRVData    , 0x00 , sizeof(s_tLatestGameRVData));
    memset(&s_tLatestGeoRVData     , 0x00 , sizeof(s_tLatestGeoRVData));
    memset(&s_tLatestStepCountData   , 0x00 , sizeof(s_tLatestStepCountData));
    mutex_unlock(&s_tDataMutex);
    s_enable_notify_step= false;

/* SHMDS_HUB_0322_03 del S */
// #ifdef USE_RESET_SIGNAL
// #ifdef USE_REMAP_SIGNAL
//     shub_gpio_set_value(SHUB_GPIO_PIN_BRMP, 0);  /* SHMDS_HUB_0110_01 mod */
// #endif
//     shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 0); /* SHMDS_HUB_0110_01 mod */
//     usleep(SHUB_RESET_PLUSE_WIDTH * 1000);       /* SHMDS_HUB_0102_11 mod */
//     shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 1); /* SHMDS_HUB_0110_01 mod */
// 
//     usleep(SHUB_RESET_TIME * 1000);              /* SHMDS_HUB_0102_11 mod */
// #endif
// 
// #ifndef NO_HOST
//     while(1) {
//         hostif_read(STATUS, &reg, sizeof(reg));
//         if(reg == 0x00) {
//             DBG(DBG_LV_INFO, "STATUS OK!!\n");
//             break;
//         }
// 
//         usleep(SHUB_RESET_RETRY_TIME * 1000); /* SHMDS_HUB_0102_11 SHMDS_HUB_0114_04 mod */
//         if(cnt++ >= 10) {               /* SHMDS_HUB_0322_04 mod */
//             DBG(DBG_LV_ERROR, "shub_initialize:STATUS read TimeOut. reg=%x \n", reg);
//             return SHUB_RC_ERR_TIMEOUT;
//         }
//     }
// #endif
//     reg = 0x04;
//     hostif_write(CFG, &reg, sizeof(reg));
// 
//     hostif_read(INTREQ0, sreg.u8, 2);
// 
//     reg = 0x00;
//     hostif_write(INTMASK0, &reg, sizeof(reg));
//     hostif_write(INTMASK1, &reg, sizeof(reg));
// 
//     ENABLE_IRQ;
/* SHMDS_HUB_0322_03 del E */

    return SHUB_RC_OK;
}

int32_t shub_init_param( void )
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    // Get Sensor Version
    cmd.cmd.u16 = HC_MCU_GET_VERSION;
    ret = shub_hostcmd(&cmd, &res, (EXE_HOST_ALL | EXE_HOST_EX_NO_RECOVER), 0);  /* SHMDS_HUB_0343_01 mod */
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "FW Version Get Error HC_MCU_GET_VERSION(%d) err %x\n",cmd.prm.u8[0], res.err.u16);
        return SHUB_RC_ERR;
    }
    s_lsi_id = (uint32_t)RESU8_TO_X16(res, 6);
    DBG(DBG_LV_ERROR, "Sensor User FW Version %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
            res.res.u8[0],res.res.u8[1],res.res.u8[2],res.res.u8[3],
            res.res.u8[4],res.res.u8[5],res.res.u8[6],res.res.u8[7]);
    DBG(DBG_LV_ERROR, "Sensor Id = 0x%04x\n", s_lsi_id);

    // Get Sensor Info
    cmd.cmd.u16 = HC_MCU_GET_EX_SENSOR;
    ret = shub_hostcmd(&cmd, &res, (EXE_HOST_ALL | EXE_HOST_EX_NO_RECOVER), 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "%s : Get Sensor Info Error(%d) err %x\n", __FUNCTION__, cmd.prm.u8[0], res.err.u16);
        return SHUB_RC_ERR;
    }
    s_exist_sensor = res.res.u8[0]; 
    DBG(DBG_LV_ERROR, "Sensor Info = 0x%04x\n", s_exist_sensor);

#ifdef CONFIG_ACC_U2DH /* SHMDS_HUB_0119_01 SHMDS_HUB_2901_01 */
    ret = shub_lis2dh_low_power();
    if(SHUB_RC_OK != ret) {
        DBG(DBG_LV_ERROR, "%s : lis2dh_low_power Error\n", __FUNCTION__);
        return SHUB_RC_ERR;
    }
#endif

/* SHMDS_HUB_3001_01 SHMDS_HUB_3001_02 add S */
#ifdef CONFIG_PWM_LED  // SHMDS_HUB_3001_03 mod
    cmd.cmd.u16 = HC_MCU_GET_PCON;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_MCU_GET_PCON err(res=0x%x,ret=%d)\n", res.err.u16, ret);
    } else {
        cmd.cmd.u16 = HC_MCU_SET_PCON;
        memcpy(cmd.prm.u8, res.res.u8, 4);
        cmd.prm.u8[1] = ((cmd.prm.u8[1] & 0xF0) | 0x03); /* PORT2=CMOS */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_MCU_SET_PCON err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_RC_ERR;
        }
    }
#endif                                                        // SHMDS_HUB_3003_01 add
#if defined (CONFIG_PWM_LED) || defined (CONFIG_BKL_PWM_LED)  // SHMDS_HUB_3003_01 add
    cmd.cmd.u16 = HC_MCU_GET_PERI;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_MCU_GET_PERI err(res=0x%x,ret=%d)\n", res.err.u16, ret);
    } else {
        cmd.cmd.u16 = HC_MCU_SET_PERI;
        memcpy(cmd.prm.u8, res.res.u8, 6);
        cmd.prm.u8[5] = 0; /* PWM Enable */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_MCU_SET_PERI err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_RC_ERR;
        }
    }
#endif                                                         // SHMDS_HUB_3003_01 add
#ifdef CONFIG_PWM_LED                                          // SHMDS_HUB_3003_01 add
    cmd.cmd.u16 = HC_MCU_GET_PWM_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_MCU_GET_PWM_PARAM err(res=0x%x,ret=%d)\n", res.err.u16, ret);
    } else {
        cmd.cmd.u16 = HC_MCU_SET_PWM_PARAM;
        memcpy(cmd.prm.u8, res.res.u8, SHUB_SIZE_PWM_PARAM);
        cmd.prm.u8[0] = 2; /* PORT2 */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PWM_PARAM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_MCU_SET_PWM_PARAM err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_RC_ERR;
        }
        memcpy(s_pwm_param, cmd.prm.u8, SHUB_SIZE_PWM_PARAM);
    }
    shub_pwm_enable = 0;
#endif
/* SHMDS_HUB_3001_01 SHMDS_HUB_3001_02 add E */

/* SHMDS_HUB_3003_01 add S */
#ifdef CONFIG_BKL_PWM_LED
    cmd.cmd.u16 = HC_MCU_GET_BLPWM_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_MCU_GET_BLPWM_PARAM err(res=0x%x,ret=%d)\n", res.err.u16, ret);
    } else {
        cmd.cmd.u16 = HC_MCU_SET_BLPWM_PARAM; /* SHMDS_HUB_3003_02 mod */
//      memcpy(s_bklpwm_param, res.res.u8, SHUB_SIZE_BKLPWM_PARAM); /* SHMDS_HUB_3001_04 del */
        memcpy(cmd.prm.u8, res.res.u8, SHUB_SIZE_BKLPWM_PARAM);     /* SHMDS_HUB_3001_04 mod */
/* SHMDS_HUB_3003_02 add  S */
#if 1
        cmd.prm.u8[0x0c] = 1; /* HTBCLK */
#else
        cmd.prm.u8[0x0c] = 0; /* LSCLK */
#endif
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_BKLPWM_PARAM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_MCU_SET_BLPWM_PARAM err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_RC_ERR;
        }
        memcpy(s_bklpwm_param, cmd.prm.u8, SHUB_SIZE_BKLPWM_PARAM); /* SHMDS_HUB_3001_04 add */
/* SHMDS_HUB_3003_02 add  E */
    }
    cmd.cmd.u16 = HC_MCU_GET_BLPWM_PORT;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_MCU_GET_BLPWM_PORT err(res=0x%x,ret=%d)\n", res.err.u16, ret);
    } else {
        s_bkl_pwm_port = res.res.u8[0];
    }
    shub_bkl_pwm_enable = 0;
#endif
/* SHMDS_HUB_3003_01 add E */

/* SHMDS_HUB_2801_01 add SHMDS_HUB_3001_02 mod S */
#if defined (CONFIG_PICKUP_PROX) || defined (CONFIG_PWM_LED)  // SHMDS_HUB_3001_03 mod
    cmd.cmd.u16 = HC_MCU_GET_PDIR;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_MCU_GET_PDIR err(res=0x%x,ret=%d)\n", res.err.u16, ret);
    } else {
        cmd.cmd.u16 = HC_MCU_SET_PDIR;
        memcpy(cmd.prm.u8, res.res.u8, 7);
#if defined (CONFIG_PWM_LED)  // SHMDS_HUB_3001_03 mod
        cmd.prm.u8[2] = 0;
#endif
#if defined (CONFIG_PICKUP_PROX)
        cmd.prm.u8[3] = 1;
#endif
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_MCU_SET_PDIR err(res=0x%x,ret=%d)\n", res.err.u16, ret);
            return SHUB_RC_ERR;
        }
    }
#endif
/* SHMDS_HUB_2801_01 add SHMDS_HUB_3001_02 mod E */

#ifdef CONFIG_SET_AXIS_VAL

//    if(sh_boot_get_handset() == 1) { /* SHMDS_HUB_0109_02 add */
        cmd.cmd.u16 = HC_ACC_SET_POSITION;
/* SHMDS_HUB_0109_02 mod S */
//      cmd.prm.u8[0] = SHUB_ACC_AXIS_VAL;
        cmd.prm.u8[0] = shub_get_acc_axis_val();
/* SHMDS_HUB_0109_02 mod E */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res)); /* SHMDS_HUB_0322_01 add */
            DBG(DBG_LV_ERROR, "error enable HC_ACC_SET_POSITION 0x%x\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        if((s_exist_sensor & HC_MAG_VALID) != 0){ /* SHMDS_HUB_0116_01 add */
            cmd.cmd.u16 = HC_MAG_SET_POSITION;
/* SHMDS_HUB_0109_02 mod S */
//          cmd.prm.u8[0] = SHUB_MAG_AXIS_VAL;
            cmd.prm.u8[0] = shub_get_mag_axis_val();
/* SHMDS_HUB_0109_02 mod E */
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res)); /* SHMDS_HUB_0322_01 add */
                DBG(DBG_LV_ERROR, "error HC_MAG_SET_POSITION 0x%x\n", res.err.u16);
                return SHUB_RC_ERR;
            }
        }
        if((s_exist_sensor & HC_GYRO_VALID) != 0){ /* SHMDS_HUB_0117_01 add */
            cmd.cmd.u16 = HC_GYRO_SET_POSITION;
/* SHMDS_HUB_0109_02 mod S */
//          cmd.prm.u8[0] = SHUB_GYRO_AXIS_VAL;
            cmd.prm.u8[0] = shub_get_gyro_axis_val();
/* SHMDS_HUB_0109_02 mod E */
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res)); /* SHMDS_HUB_0322_01 add */
                DBG(DBG_LV_ERROR, "error HC_GYRO_SET_POSITION 0x%x\n", res.err.u16);
                return SHUB_RC_ERR;
            }
        }
//    } /* SHMDS_HUB_0109_02 add */
#endif

    /***** Set Mag calibration *****/
    if((s_exist_sensor & HC_MAG_VALID) != 0){
        cmd.cmd.u16 = HC_MAG_SET_CAL;
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] = 0; 
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res)); /* SHMDS_HUB_0322_01 add */
            DBG(DBG_LV_ERROR, "HC_MAG_SET_CAL err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        s_micon_param.mag_cal = cmd.prm.u8[1];
    }

    /***** Set Gyro calibration *****/
    if((s_exist_sensor & HC_GYRO_VALID) != 0){
        cmd.cmd.u16 = HC_GYRO_SET_CAL;
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] = 0;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res)); /* SHMDS_HUB_0322_01 add */
            DBG(DBG_LV_ERROR, "HC_GYRO_SET_CAL err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        s_micon_param.gyro_cal = cmd.prm.u8[1] ;
    }

/* SHMDS_HUB_1601_01 SHMDS_HUB_0204_14 SHMDS_HUB_0204_17 mod S */
#ifdef SHUB_SW_GYRO_ENABLE
    cmd.cmd.u16 = HC_GYRO_GET_CALIB;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res)); /* SHMDS_HUB_0322_01 add */
        DBG(DBG_LV_ERROR, "HC_GYRO_GET_CALIB err(%x)\n", res.err.u16);
    } else {
        cmd.cmd.u16 = HC_GYRO_SET_CALIB;
        cmd.prm.u8[0] = 0x4C;
        cmd.prm.u8[1] = 0x1D;
        cmd.prm.u8[2] = 0xA8;  /* SHMDS_HUB_0354_01 mod (0x04->0xA8) */
        cmd.prm.u8[3] = 0x61;  /* SHMDS_HUB_0354_01 mod (0x29->0x61) */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res)); /* SHMDS_HUB_0322_01 add */
            DBG(DBG_LV_ERROR, "HC_GYRO_SET_CALIB err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
#endif
/* SHMDS_HUB_1601_01 SHMDS_HUB_0204_14 SHMDS_HUB_0204_17 mod E */

/* SHMDS_HUB_2001_01 mod S */
#ifdef SHUB_SW_GYRO_ENABLE
    cmd.cmd.u16 = HC_GYRO_GET_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res)); /* SHMDS_HUB_0322_01 add */
        DBG(DBG_LV_ERROR, "HC_GYRO_GET_PARAM err(%x)\n", res.err.u16);
    } else {
        cmd.cmd.u16 = HC_GYRO_SET_PARAM;
        cmd.prm.u8[0] = res.res.u8[0];
        cmd.prm.u8[1] = res.res.u8[1];
        cmd.prm.u8[2] = res.res.u8[2];
        cmd.prm.u8[3] = res.res.u8[3];
        cmd.prm.u8[4] = 0;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 5);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res)); /* SHMDS_HUB_0322_01 add */
            DBG(DBG_LV_ERROR, "HC_GYRO_SET_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
#endif
/* SHMDS_HUB_2001_01 mod E */

/* SHMDS_HUB_0132_01 add S */
    // Device Orientation
    cmd.cmd.u16 = HC_ACC_SET_ANDROID_XY;
    cmd.prm.u8[0] = 0;
    cmd.prm.u8[1] = 1;
//  cmd.prm.u8[2] = 43; /* SHMDS_HUB_0132_03 mod (30->43) SHMDS_HUB_2605_01 del */
    cmd.prm.u8[3] = 9;  /* SHMDS_HUB_0132_03 mod (4->14)  SHMDS_HUB_2605_01 mod (14->6) SHMDS_HUB_0132_04 mod (6->9) */
    cmd.prm.u8[4] = 15; /* SHMDS_HUB_2605_01 add SHMDS_HUB_0132_04 mod (30->15) */
    cmd.prm.u8[5] = 50; /* SHMDS_HUB_2605_01 add */
    cmd.prm.u8[6] = 15; /* SHMDS_HUB_2605_01 add SHMDS_HUB_0132_04 mod (20->15) */
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_DEV_ORI_PARAM); /* SHMDS_HUB_3303_01 mod */
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res)); /* SHMDS_HUB_0322_01 add */
        DBG(DBG_LV_ERROR, "HC_ACC_SET_ANDROID_XY err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0132_01 add E */

/* SHMDS_HUB_2605_01 add S */
    cmd.cmd.u16 = HC_ACC_GET_AUTO_MEASURE;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_ACC_GET_AUTO_MEASURE err(%x)\n", res.err.u16);
    } else {
        cmd.cmd.u16 = HC_ACC_SET_AUTO_MEASURE;
        cmd.prm.u8[0] = res.res.u8[0];
        cmd.prm.u8[1] = res.res.u8[1];
        cmd.prm.u8[2] = res.res.u8[2];
        cmd.prm.u8[3] = res.res.u8[3];
        cmd.prm.u8[4] = 0xF0;  /* SHMDS_HUB_0132_04 mod (0x40->0xF0) */
        cmd.prm.u8[5] = 0x00;  /* SHMDS_HUB_0132_04 mod (0x01->0x00) */
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_ACC_SET_AUTO_MEASURE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
/* SHMDS_HUB_2605_01 add E */

/* SHMDS_HUB_0120_07 add S SHMDS_HUB_0120_08 del S */
//#ifdef CONFIG_BARO_SENSOR
//    cmd.cmd.u16 = HC_BARO_GET_PARAM;
//    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
//    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
//        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
//        DBG(DBG_LV_ERROR, "HC_BARO_GET_PARAM err(%x)\n", res.err.u16);
//    } else {
//        cmd.cmd.u16 = HC_BARO_SET_PARAM;
//        cmd.prm.u8[0] = 1;
//        cmd.prm.u8[1] = 1;
//        cmd.prm.u8[2] = 0;
//        cmd.prm.u8[3] = 5;
//        cmd.prm.u8[4] = res.res.u8[4];
//        cmd.prm.u8[5] = res.res.u8[5];
//        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
//        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
//            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
//            DBG(DBG_LV_ERROR, "HC_BARO_SET_PARAM err(%x)\n", res.err.u16);
//            return SHUB_RC_ERR;
//        }
//    }
//#endif
/* SHMDS_HUB_0120_07 add E SHMDS_HUB_0120_08 del E */

/* SHMDS_HUB_0353_02 mod S */
/* SHMDS_HUB_0353_01 add S */
    if(shub_acc_offset_flg) {
//      shub_set_acc_offset(shub_acc_offset);
        cmd.cmd.u16 = HC_ACC_SET_OFFSET;
        cmd.prm.s16[0] = (int16_t)shub_acc_offset[0];
        cmd.prm.s16[1] = (int16_t)shub_acc_offset[1];
        cmd.prm.s16[2] = (int16_t)shub_acc_offset[2];
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_ACC_SET_OFFSET err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
    
    if(shub_mag_axis_flg) {
//      shub_cal_mag_axis_interfrence(shub_mag_axis_interfrence);
        cmd.cmd.u16 = HC_MAG_SET_STATIC_MAT;
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] = 0;
        cmd.prm.s16[1] = shub_mag_axis_interfrence[0];
        cmd.prm.s16[2] = shub_mag_axis_interfrence[1];
        cmd.prm.s16[3] = shub_mag_axis_interfrence[2];
        cmd.prm.s16[4] = shub_mag_axis_interfrence[3];
        cmd.prm.s16[5] = shub_mag_axis_interfrence[4];
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 12);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_MAG_SET_STATIC_MAT err1(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        
        cmd.cmd.u16 = HC_MAG_SET_STATIC_MAT;
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] = 1;
        cmd.prm.s16[1] = shub_mag_axis_interfrence[5];
        cmd.prm.s16[2] = shub_mag_axis_interfrence[6];
        cmd.prm.s16[3] = shub_mag_axis_interfrence[7];
        cmd.prm.s16[4] = shub_mag_axis_interfrence[8];
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 12);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_MAG_SET_STATIC_MAT err2(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
/* SHMDS_HUB_0353_01 add E */
/* SHMDS_HUB_0353_02 mod E */

/* SHMDS_HUB_0201_01 mod S */
    ret = shub_set_default_parameter();
    if(ret != 0) {
        shub_set_error_code(SHUB_FUP_ERR_SET_PARAM); /* SHMDS_HUB_0322_01 add */
        DBG(DBG_LV_ERROR, "Failed shub_set_default_parameter. ret=%x\n", ret);
        return SHUB_RC_ERR;
    }
/* SHMDS_HUB_0201_01 mod E */
    
    shub_dbg_clr_irq_log();             // SHMDS_HUB_0701_03 add

    shub_mot_still_enable_flag = false; /* SHMDS_HUB_0332_01 add */

    return SHUB_RC_OK;
}

/* SHMDS_HUB_0322_01 add S */
void shub_set_error_code(uint32_t mode){
    shub_err_code = mode;
    return;
}

uint32_t shub_get_error_code(void){
    return shub_err_code;
}
/* SHMDS_HUB_0322_01 add E */

int32_t shub_initialize( void )
{
    int32_t ret;

    // Init Common
    ret = shub_init_common();
    if(ret != SHUB_RC_OK){
        return ret;
    }

#ifndef NO_HOST
    // check access
    ret = shub_check_access();
    if(ret != SHUB_RC_OK) {
        return ret;
    }
    shub_connect_flg = true;


    // Init Param
    // SHMDS_HUB_0319_01 mod S
    ret = shub_init_param();
    if(ret != SHUB_RC_OK) {
        shub_failed_init_param++;
        DBG(DBG_LV_ERROR, "Failed shub_init_param. ret=%x, cnt=%d\n", ret, shub_failed_init_param);
        //return ret;
    }
    // SHMDS_HUB_0319_01 mod E
#endif

    shub_exif_input_val_init(); // SHMDS_HUB_0304_01 add

    return SHUB_RC_OK;
}

/* SHMDS_HUB_0304_02 add S */
static int32_t shub_fw_reset_exe(int mode)
{
    uint8_t reg = 0xFF; /* SHMDS_HUB_0322_03 add */
    int32_t cnt = 1;    /* SHMDS_HUB_0322_03 SHMDS_HUB_0322_04 add */
    Word sreg;          /* SHMDS_HUB_0322_03 add */
    
#ifdef USE_RESET_SIGNAL
#ifdef USE_REMAP_SIGNAL
    if(mode == SHUB_FW_RESET_BOOT) {
        shub_gpio_set_value(SHUB_GPIO_PIN_BRMP, 1);  /* SHMDS_HUB_0110_01 mod */
    }else{
        shub_gpio_set_value(SHUB_GPIO_PIN_BRMP, 0);  /* SHMDS_HUB_0110_01 mod */
    }
#endif
    shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 0);     /* SHMDS_HUB_0110_01 mod */
    usleep(SHUB_RESET_PLUSE_WIDTH * 1000);           /* SHMDS_HUB_0102_11 mod */
    shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 1);     /* SHMDS_HUB_0110_01 mod */
    usleep(SHUB_RESET_TIME * 1000);                  /* SHMDS_HUB_0102_11 mod */
#endif
    
#ifndef NO_HOST
    if(mode == SHUB_FW_RESET_USER) {
        while(1) {
            hostif_read(STATUS, &reg, sizeof(reg));
            if(reg == 0x00) {
                DBG(DBG_LV_INFO, "STATUS OK!!\n");
                break;
            }
            usleep(SHUB_RESET_RETRY_TIME * 1000); /* SHMDS_HUB_0102_11 SHMDS_HUB_0114_04 mod */
            if(cnt++ >= 10) {               /* SHMDS_HUB_0322_04 mod */
                DBG(DBG_LV_ERROR, "shub_initialize:STATUS read TimeOut. reg=%x \n", reg);
                return SHUB_RC_ERR_TIMEOUT;
            }
        }
    }
#endif
    
    reg = 0x04;
    hostif_write(CFG, &reg, sizeof(reg));
    reg = 0x00;
    hostif_read(CFG, &reg, sizeof(reg));
    if(reg != 0x04) {
        DBG(DBG_LV_ERROR, "[shub] shub_initialize: shub_fw_reset_exe. CFG read error. reg=%x \n", reg);
        return SHUB_RC_ERR;
    }
    hostif_read(INTREQ0, sreg.u8, 2);
    
    reg = 0x00;
    hostif_write(INTMASK0, &reg, sizeof(reg));
    hostif_write(INTMASK1, &reg, sizeof(reg));
    
    shub_recovery_flg = 0; /* SHMDS_HUB_3301_01 add */
    ENABLE_IRQ;
    return SHUB_RC_OK;
}

static int32_t shub_fw_reset_retry(int mode)
{
    int32_t ret;
    int32_t i;
    
    for(i=0; i<SHUB_FW_RESET_RETRY_NUM; i++) {
        ret = shub_fw_reset_exe(mode);
        if(ret == SHUB_RC_OK) {
            if(i > 0) {
                DBG(DBG_LV_ERROR, "%s : FW Reset OK( mode=%d, retry=%d )\n", __FUNCTION__, mode, i);
            }
            return ret;
        }else{
            /* timeout */
            DBG(DBG_LV_ERROR, "%s : FW Reset Error( mode=%d, retry=%d )\n", __FUNCTION__, mode, i);
        }
    }
    /* retry over!! */
    DBG(DBG_LV_ERROR, "%s : FW Reset Retry over!!( mode=%d, retry=%d, ret=%d )\n", __FUNCTION__, mode, i, ret);
    return ret;
}

static int32_t shub_fw_reset(int mode)
{
    int32_t ret;
    
    mutex_lock(&s_hostCmdMutex);
    
    ret = shub_fw_reset_retry(mode);
    
    mutex_unlock(&s_hostCmdMutex);
    return ret;
}

/* SHMDS_HUB_0304_03 del S */
// static int32_t shub_fw_update(void)
// {
//     uint8_t reg = 0xFF;
//     Word    sreg;
//     int32_t ret;
//     
//     ENABLE_IRQ;
//     
//     cmd.cmd.u16 = HC_MCU_FUP_START;
//     cmd.prm.u8[0] = 0x55;
//     cmd.prm.u8[1] = 0xAA;
//     ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 2);
//     if(ret != SHUB_RC_OK) {
//         DBG(DBG_LV_ERROR, "Communication Error!\n");
//         ret = SHUB_RC_ERR;
//         goto ERROR;
//     }
//     if(res.err.u16 == ERROR_FUP_CERTIFICATION) {
//         DBG(DBG_LV_ERROR, "Certification Error!\n");
//         ret = SHUB_RC_ERR;
//         goto ERROR;
//     }
//     usleep(SHUB_RESET_TIME * 1000); /* SHMDS_HUB_0102_11 mod */
//     
//     reg = 0x04;
//     hostif_write(CFG, &reg, sizeof(reg));
//     hostif_read(INTREQ0, sreg.u8, 2);
//     
//     reg = 0x00;
//     hostif_write(INTMASK0, &reg, sizeof(reg));
//     hostif_write(INTMASK1, &reg, sizeof(reg));
// 
//     shub_recovery_flg = 0; /* SHMDS_HUB_3301_01 add */
//     ENABLE_IRQ;
//     return SHUB_RC_OK;
// }
/* SHMDS_HUB_0304_03 del E */
/* SHMDS_HUB_0304_02 add E */

static int32_t shub_update_firmware(bool boot, uint8_t *arg_iDataPage1, uint8_t *arg_iDataPage2, uint32_t arg_iLen)
{
    uint8_t reg = 0xFF;
    Word    sreg;
    uint32_t i;
    int32_t ret;
    int32_t remain_size = arg_iLen;
    uint32_t write_size;
    uint32_t write_pos;
    HostCmd cmd;
    HostCmdRes res;
    uint32_t chksum;

    DBG(DBG_LV_INFO, "### Start Firmware Update ### boot=%d, DataPage1=%lx DataPage2=%lx Len=%d \n"
            , boot, (long)arg_iDataPage1, (long)arg_iDataPage2, arg_iLen); // SHMDS_HUB_0111_01 mod
    atomic_set(&g_FWUpdateStatus,true);

    //  if((arg_iDataPage1 == NULL) || (arg_iLen == 0)){
    //      DBG(DBG_LV_ERROR, "arg error\n");
    //      ret = SHUB_RC_ERR;
    //      goto ERROR;
    //  }

    shub_fw_reset(SHUB_FW_RESET_BOOT); /* SHMDS_HUB_0304_02 SHMDS_HUB_0304_03 add */

/* SHMDS_HUB_0304_02 del S */
//     if(boot){
// #ifdef USE_RESET_SIGNAL
// #ifdef USE_REMAP_SIGNAL
//         shub_gpio_set_value(SHUB_GPIO_PIN_BRMP, 1);  /* SHMDS_HUB_0110_01 mod */
// #endif
//         shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 0); /* SHMDS_HUB_0110_01 mod */
//         usleep(SHUB_RESET_PLUSE_WIDTH * 1000);       /* SHMDS_HUB_0102_11 mod */
//         shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 1); /* SHMDS_HUB_0110_01 mod */
// #endif
//         usleep(SHUB_RESET_TIME * 1000);              /* SHMDS_HUB_0102_11 mod */
//     }else{
//         ENABLE_IRQ;
// 
//         cmd.cmd.u16 = HC_MCU_FUP_START;
//         cmd.prm.u8[0] = 0x55;
//         cmd.prm.u8[1] = 0xAA;
//         ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 2);
//         if(ret != SHUB_RC_OK) {
//             DBG(DBG_LV_ERROR, "Communication Error!\n");
//             ret = SHUB_RC_ERR;
//             goto ERROR;
//         }
//         if(res.err.u16 == ERROR_FUP_CERTIFICATION) {
//             DBG(DBG_LV_ERROR, "Certification Error!\n");
//             ret = SHUB_RC_ERR;
//             goto ERROR;
//         }
//         usleep(SHUB_RESET_TIME * 1000); /* SHMDS_HUB_0102_11 mod */
//     } 
//     reg = 0x04;
//     hostif_write(CFG, &reg, sizeof(reg));
// 
//     hostif_read(INTREQ0, sreg.u8, 2);
// 
//     reg = 0x00;
//     hostif_write(INTMASK0, &reg, sizeof(reg));
//     hostif_write(INTMASK1, &reg, sizeof(reg));
// 
//     shub_recovery_flg = 0; /* SHMDS_HUB_3301_01 add */
//     ENABLE_IRQ;
/* SHMDS_HUB_0304_02 del E */

    DBG(DBG_LV_INFO, "Check Firmware Mode.\n");
    cmd.cmd.u16 = HC_MCU_GET_VERSION;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL_RSLT, 0);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Communication Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }
    if(res.res.u8[2] != 0x01){
        DBG(DBG_LV_ERROR, "Version check Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }

    DBG(DBG_LV_INFO, "Flash Clear.\n");
    cmd.cmd.u16 = HC_MCU_FUP_ERASE;
    cmd.prm.u8[0] = 0xAA;
    cmd.prm.u8[1] = 0x55;
    ret = shub_hostcmd(&cmd, &res, (EXE_HOST_WAIT | EXE_HOST_ERR | EXE_HOST_FUP_ERASE), 2);  /* SHMDS_HUB_0343_01 mod */
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Communication Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }
    if(res.err.u16 == ERROR_FUP_CERTIFICATION) {
        DBG(DBG_LV_ERROR, "Certification Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }

    write_pos=0;
    while(remain_size > 0){
        if(remain_size > FIFO_SIZE){
            write_size = FIFO_SIZE;
            remain_size -= FIFO_SIZE;
        }else{
            write_size = remain_size;
            remain_size = 0;
        }
        if(write_pos < (64*1024)){
            ret = hostif_write(FIFO, &arg_iDataPage1[write_pos], write_size);
        }else{
            ret = hostif_write(FIFO, &arg_iDataPage2[write_pos - (64*1024)], write_size);
        }
        write_pos+= write_size;

        cmd.cmd.u16 = HC_MCU_FUP_WRITE_FIFO;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 0);
        if(ret != SHUB_RC_OK || res.err.u16 != 0) {
            DBG(DBG_LV_ERROR, "HC_MCU_FUP_WRITE_FIFO err(%x)\n", res.err.u16);
            ret = SHUB_RC_ERR;
            goto ERROR;
        }
    }


    chksum=0;
    for(i= 0; i < arg_iLen; i++){
        if(i < (64*1024)){
            chksum += (uint32_t)arg_iDataPage1[i];
        }else{
            chksum += (uint32_t)arg_iDataPage2[i - (64*1024)];
        }
    }
    DBG(DBG_LV_INFO, "Write CheckSum Value=[0x%08X].\n", chksum);
    hostif_write(FIFO, (uint8_t *)&chksum, (int32_t)sizeof(chksum));
    cmd.cmd.u16 = HC_MCU_FUP_WRITE_FIFO;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 0);
    if(ret != SHUB_RC_OK || res.err.u16 != 0) {
        DBG(DBG_LV_ERROR, "Fifo Write Cmd Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }

    DBG(DBG_LV_INFO, "SelfTest.\n");
    cmd.cmd.u16 = HC_MCU_FUP_SELFTEST;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_RES|EXE_HOST_WAIT, 0);
    if(ret != SHUB_RC_OK || res.res.u8[0] == 0x01) {
        DBG(DBG_LV_ERROR, "SelfTest Error! [Firmware Update Mode]\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }

    DISABLE_IRQ;

    DBG(DBG_LV_INFO, "End Firmware Update.\n");
    cmd.cmd.u16 = HC_MCU_FUP_END;
    shub_hostcmd(&cmd, &res, 0, 0);

    usleep(SHUB_RESET_TIME * 1000); /* SHMDS_HUB_0102_11 mod */

    DBG(DBG_LV_INFO, "Initialize.\n");
    reg = 0x04;
    hostif_write(CFG, &reg, sizeof(reg));

    hostif_read(INTREQ0, sreg.u8, 2);

    reg = 0x00;
    hostif_write(INTMASK0, &reg, sizeof(reg));
    hostif_write(INTMASK1, &reg, sizeof(reg));

    ENABLE_IRQ;

    DBG(DBG_LV_INFO, "Check User program mode.\n");
    cmd.cmd.u16 = HC_MCU_GET_VERSION;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Communication Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }

    if(res.res.u8[2] != 0x00){
        DBG(DBG_LV_ERROR, "Version check Error!");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }
    ret = SHUB_RC_OK;
ERROR:
    atomic_set(&g_FWUpdateStatus,false);

    return ret;
}

int32_t shub_update_fw_exe(bool boot, uint8_t *arg_iDataPage1, uint8_t *arg_iDataPage2, uint32_t arg_iLen) /* SHMDS_HUB_0304_02 mod */
{
    int32_t ret;
    int32_t i;
    int32_t retry_count = 0;
    
    //    if((arg_iDataPage1 == NULL) || (arg_iLen == 0)){
    if((arg_iDataPage1 == NULL) || (arg_iDataPage2 == NULL) || (arg_iLen == 0)){

        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    for(i=0; i<SHUB_CMD_RETRY_NUM; i++) {
        ret = shub_update_firmware(boot, arg_iDataPage1, arg_iDataPage2, arg_iLen);
        if(SHUB_RC_OK != ret) {
            DBG(DBG_LV_ERROR, "%s : FW update Error(%d) Retry=%d\n", __FUNCTION__, ret, i);
            ret = SHUB_RC_ERR;
        }else{
            retry_count = i;
            i = SHUB_CMD_RETRY_NUM;
            DBG(DBG_LV_ERROR, "%s : FW update OK!!( Retry=%d )\n", __FUNCTION__, retry_count);
            ret = SHUB_RC_OK;
        }
    }
    if(ret != SHUB_RC_OK) {
        shub_set_error_code(SHUB_FUP_ERR_WRITE); /* SHMDS_HUB_0322_01 add */
        DBG(DBG_LV_ERROR, "%s : FW update Retry over!!( Retry=%d, ret=%d )\n", __FUNCTION__, retry_count, ret);
        return ret;
    }

    ret = shub_initialize();
    if(ret != SHUB_RC_OK) {
/* SHMDS_HUB_0322_01 mod S */
        if(ret == SHUB_RC_ERR){
            shub_set_error_code(SHUB_FUP_ERR_CHECK_ACCESS);
            DBG(DBG_LV_ERROR, "%s : shub_initialize(check_access) Error( ret=%d )\n", __FUNCTION__, ret);
        } else{
            shub_set_error_code(SHUB_FUP_ERR_INIT_COMMON);
            DBG(DBG_LV_ERROR, "%s : shub_initialize(init_common) Error( ret=%d )\n", __FUNCTION__, ret);
        }
        return SHUB_RC_ERR;
/* SHMDS_HUB_0322_01 mod E */
    }
    shub_fw_write_flg = false;
    return ret;
}

/* SHMDS_HUB_0304_02 add S */
int32_t shub_update_fw(bool boot, uint8_t *arg_iDataPage1, uint8_t *arg_iDataPage2, uint32_t arg_iLen)
{
    int32_t ret;
    
    shub_access_flg |= SHUB_ACCESS_FW_UPDATE;
    
    ret = shub_update_fw_exe(boot, arg_iDataPage1, arg_iDataPage2, arg_iLen);
    
    shub_access_flg &= ~SHUB_ACCESS_FW_UPDATE;
    
    return ret;
}
/* SHMDS_HUB_0304_02 add E */

static int32_t shub_check_access(void)
{
    int32_t ret;

    // user FW check
    ret = shub_user_fw_check();
    if(ret == SHUB_RC_OK) {
        return SHUB_RC_OK;
    }

    // boot FW check
    ret = shub_boot_fw_check();
    if(ret == SHUB_RC_OK){
        DBG(DBG_LV_ERROR, "%s : Boot FW Update Flag On!!\n", __FUNCTION__);
        shub_fw_write_flg = true;
        shub_connect_flg = true;
    }
    return SHUB_RC_ERR;
}

static int32_t shub_boot_fw_check(void)
{
//  uint8_t reg = 0xFF; /* SHMDS_HUB_0304_02 del */
//  Word    sreg;       /* SHMDS_HUB_0304_02 del */
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;
    int32_t i;
    int32_t retry_count = 0;

    DBG(DBG_LV_INFO, "boot start\n");
    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    shub_fw_reset(SHUB_FW_RESET_BOOT); /* SHMDS_HUB_0304_02 add */

/* SHMDS_HUB_0304_02 del S */
// #ifdef USE_RESET_SIGNAL
// #ifdef USE_REMAP_SIGNAL
//     shub_gpio_set_value(SHUB_GPIO_PIN_BRMP, 1);  /* SHMDS_HUB_0110_01 mod */
// #endif
//     shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 0); /* SHMDS_HUB_0110_01 mod */
//     usleep(SHUB_RESET_PLUSE_WIDTH * 1000);       /* SHMDS_HUB_0102_11 mod */
//     shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 1); /* SHMDS_HUB_0110_01 mod */
// #endif
//     usleep(SHUB_RESET_TIME * 1000);              /* SHMDS_HUB_0102_11 mod */
// 
//     reg = 0x04;
//     hostif_write(CFG, &reg, sizeof(reg));
// 
//     hostif_read(INTREQ0, sreg.u8, 2);
// 
//     reg = 0x00;
//     hostif_write(INTMASK0, &reg, sizeof(reg));
//     hostif_write(INTMASK1, &reg, sizeof(reg));
// 
//     shub_recovery_flg = 0; /* SHMDS_HUB_3301_01 add */
//     ENABLE_IRQ;
/* SHMDS_HUB_0304_02 del E */

    // check boot version
    for(i=0; i<SHUB_CMD_RETRY_NUM; i++) {
        cmd.cmd.u16 = HC_MCU_GET_VERSION;
        ret = shub_hostcmd(&cmd, &res, (EXE_HOST_ALL_RSLT), 0); /* SHMDS_HUB_0316_01 mod */ /* SHMDS_HUB_0343_01 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : FW Version Get Error(ret=%d, err=0x%x, cnt=%d)\n", __FUNCTION__, ret, res.err.u16, i);
            ret = SHUB_RC_ERR;
        }else{
            retry_count = i;
            i = SHUB_CMD_RETRY_NUM;
            DBG(DBG_LV_ERROR, "Sensor Boot FW Version(%d) %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n", retry_count,
                    res.res.u8[0],res.res.u8[1],res.res.u8[2],res.res.u8[3],
                    res.res.u8[4],res.res.u8[5],res.res.u8[6],res.res.u8[7]);
            ret = SHUB_RC_OK;
        }
    }
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "%s : Boot FW Version Retry over!!( Retry=%d, ret=%d )\n", __FUNCTION__, i, ret);
    }

    shub_init_common();
    return ret;
}

static int32_t shub_user_fw_check(void)
{
    int32_t i;
    int32_t retry_count = 0;
    int32_t ret = SHUB_RC_OK;
    HostCmd cmd;
    HostCmdRes res;
//  uint8_t reg = 0xFF; /* SHMDS_HUB_0322_03 add */
//  int32_t cnt = 1;    /* SHMDS_HUB_0322_03 SHMDS_HUB_0322_04 add */
//  Word sreg;          /* SHMDS_HUB_0322_03 add */

/* SHMDS_HUB_0304_02 add S */
    ret = shub_fw_reset(SHUB_FW_RESET_USER);
    if(ret != SHUB_RC_OK) {
        return ret;
    }
/* SHMDS_HUB_0304_02 add E */

/* SHMDS_HUB_0304_02 del S */
// /* SHMDS_HUB_0322_03 add S */
// #ifdef USE_RESET_SIGNAL
// #ifdef USE_REMAP_SIGNAL
//     shub_gpio_set_value(SHUB_GPIO_PIN_BRMP, 0);  /* SHMDS_HUB_0110_01 mod */
// #endif
//     shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 0); /* SHMDS_HUB_0110_01 mod */
//     usleep(SHUB_RESET_PLUSE_WIDTH * 1000);       /* SHMDS_HUB_0102_11 mod */
//     shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 1); /* SHMDS_HUB_0110_01 mod */
//     usleep(SHUB_RESET_TIME * 1000);              /* SHMDS_HUB_0102_11 mod */
// #endif
// 
// #ifndef NO_HOST
//     while(1) {
//         hostif_read(STATUS, &reg, sizeof(reg));
//         if(reg == 0x00) {
//             DBG(DBG_LV_INFO, "STATUS OK!!\n");
//             break;
//         }
//         usleep(SHUB_RESET_RETRY_TIME * 1000); /* SHMDS_HUB_0102_11 SHMDS_HUB_0114_04 mod */
//         if(cnt++ >= 10) {               /* SHMDS_HUB_0322_04 mod */
//             DBG(DBG_LV_ERROR, "shub_initialize:STATUS read TimeOut. reg=%x \n", reg);
//             return SHUB_RC_ERR_TIMEOUT;
//         }
//     }
// #endif
//     reg = 0x04;
//     hostif_write(CFG, &reg, sizeof(reg));
//     hostif_read(INTREQ0, sreg.u8, 2);
//     
//     reg = 0x00;
//     hostif_write(INTMASK0, &reg, sizeof(reg));
//     hostif_write(INTMASK1, &reg, sizeof(reg));
//     
//     shub_recovery_flg = 0; /* SHMDS_HUB_3301_01 add */
//     ENABLE_IRQ;
// /* SHMDS_HUB_0322_03 add E */
/* SHMDS_HUB_0304_02 del S */

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }
    // check version
    for(i=0; i<SHUB_CMD_RETRY_NUM; i++) {
        cmd.cmd.u16 = HC_MCU_GET_VERSION;
        ret = shub_hostcmd(&cmd, &res, (EXE_HOST_ALL | EXE_HOST_EX_NO_RECOVER), 0);  /* SHMDS_HUB_0343_01 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : FW Version Get Error(ret=%d, err=0x%x, cnt=%d)\n", __FUNCTION__, ret, res.err.u16, i);
            ret = SHUB_RC_ERR;
        }else{
            retry_count = i;
            i = SHUB_CMD_RETRY_NUM;
            DBG(DBG_LV_INFO, "Sensor User FW Version(%d) %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n", retry_count,
                    res.res.u8[0],res.res.u8[1],res.res.u8[2],res.res.u8[3],
                    res.res.u8[4],res.res.u8[5],res.res.u8[6],res.res.u8[7]);
            ret = SHUB_RC_OK;
        }
    }
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "%s : FW Version Retry over!!( Retry=%d, ret=%d )\n", __FUNCTION__, i, ret);
        return SHUB_RC_ERR;
    }
    s_lsi_id = (uint32_t)RESU8_TO_X16(res, 6);
    // check sum
#ifndef NO_LINUX
#ifndef SHUB_SW_FACTORY_MODE /* SHMDS_HUB_0322_06 add */
    cmd.cmd.u16 = HC_MCU_FUP_SELFTEST;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_RES|EXE_HOST_WAIT, 0);
    if(ret != SHUB_RC_OK || res.res.u8[0] == 0x01) {
        DBG(DBG_LV_ERROR, "%s : SelfTest Error(ret=%d)\n", __FUNCTION__, ret);
        return SHUB_RC_ERR;
    }
#endif
#endif
    return SHUB_RC_OK;
}

int32_t shub_get_fw_version(uint8_t *arg_iData)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;

    if(arg_iData == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    cmd.cmd.u16 = HC_MCU_GET_VERSION;

    ret = shub_hostcmd(&cmd, &res, (EXE_HOST_ALL | EXE_HOST_EX_NO_RECOVER), 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "FW Version Get Error HC_MCU_GET_VERSION(%d) err %x\n",cmd.prm.u8[0], res.err.u16);
        return SHUB_RC_ERR;
    }

    arg_iData[0] = res.res.u8[0];
    arg_iData[1] = res.res.u8[1];
    arg_iData[2] = res.res.u8[2];
    arg_iData[3] = res.res.u8[3];

    DBG(DBG_LV_INFO, "FW Version=%02x %02x %02x %02x\n", arg_iData[0], arg_iData[1], arg_iData[2], arg_iData[3]);
    return SHUB_RC_OK;
}

int32_t shub_get_sensors_data(int32_t type, int32_t* data)
{
    int32_t ret = SHUB_RC_OK;
    struct timespec ts;
    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

/* SHMDS_HUB_0403_01 del S */
// #ifdef SHUB_SUSPEND
//     if(s_is_suspend){
//         return SHUB_RC_OK;
//     }
// #endif
/* SHMDS_HUB_0403_01 del E */

    mutex_lock(&userReqMutex);
    ret = shub_read_sensor_data(type);
    ts = shub_get_timestamp();
    mutex_unlock(&userReqMutex);

    mutex_lock(&s_tDataMutex);
    switch(type){
        case SHUB_ACTIVE_ACC:
            data[0] = s_tLatestAccData.nX;
            data[1] = s_tLatestAccData.nY;
            data[2] = s_tLatestAccData.nZ;
            data[3] = ts.tv_sec;
            data[4] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_GYRO:
            data[0] = s_tLatestGyroData.nX;
            data[1] = s_tLatestGyroData.nY;
            data[2] = s_tLatestGyroData.nZ;
            data[3] = s_tLatestGyroData.nAccuracy;
            data[4] = ts.tv_sec;
            data[5] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_GYROUNC:
            data[0] = s_tLatestGyroData.nX + s_tLatestGyroData.nXOffset;
            data[1] = s_tLatestGyroData.nY + s_tLatestGyroData.nYOffset;
            data[2] = s_tLatestGyroData.nZ + s_tLatestGyroData.nZOffset;
            data[3] = s_tLatestGyroData.nAccuracy;
            data[4] = s_tLatestGyroData.nXOffset;
            data[5] = s_tLatestGyroData.nYOffset;
            data[6] = s_tLatestGyroData.nZOffset;
            data[7] = ts.tv_sec;
            data[8] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_MAG:
            data[0] = s_tLatestMagData.nX;
            data[1] = s_tLatestMagData.nY;
            data[2] = s_tLatestMagData.nZ;
            data[3] = s_tLatestMagData.nAccuracy;
            data[4] = ts.tv_sec;
            data[5] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_MAGUNC:
            data[0] = s_tLatestMagData.nX + s_tLatestMagData.nXOffset;
            data[1] = s_tLatestMagData.nY + s_tLatestMagData.nYOffset;
            data[2] = s_tLatestMagData.nZ + s_tLatestMagData.nZOffset;
            data[3] = s_tLatestMagData.nXOffset;
            data[4] = s_tLatestMagData.nYOffset;
            data[5] = s_tLatestMagData.nZOffset;
            data[6] = s_tLatestMagData.nAccuracy;
            data[7] = ts.tv_sec;
            data[8] = ts.tv_nsec;
            break;

/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
        case SHUB_ACTIVE_BARO:
            data[0] = (int32_t)(s_tLatestBaroData.pressure);
            data[1] = ts.tv_sec;
            data[2] = ts.tv_nsec;
            break;
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */

        case SHUB_ACTIVE_ORI:
            data[0] = s_tLatestOriData.pitch;
            data[1] = s_tLatestOriData.roll;
            data[2] = s_tLatestOriData.yaw;
            data[3] = s_tLatestOriData.nAccuracy;
            data[4] = ts.tv_sec;
            data[5] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_GRAVITY:
            data[0] = s_tLatestGravityData.nX;
            data[1] = s_tLatestGravityData.nY;
            data[2] = s_tLatestGravityData.nZ;
            data[3] = ts.tv_sec;
            data[4] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_LACC:
            data[0] = s_tLatestLinearAccData.nX;
            data[1] = s_tLatestLinearAccData.nY;
            data[2] = s_tLatestLinearAccData.nZ;
            data[3] = ts.tv_sec;
            data[4] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_RV:
            data[0] = s_tLatestRVectData.nX;
            data[1] = s_tLatestRVectData.nY;
            data[2] = s_tLatestRVectData.nZ;
            data[3] = s_tLatestRVectData.nS;
            data[4] = s_tLatestRVectData.nAccuracy;
            data[5] = ts.tv_sec;
            data[6] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_RV_NONMAG:
            data[0] = s_tLatestGameRVData.nX;
            data[1] = s_tLatestGameRVData.nY;
            data[2] = s_tLatestGameRVData.nZ;
            data[3] = s_tLatestGameRVData.nS;
            data[4] = ts.tv_sec;
            data[5] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_RV_NONGYRO:
            data[0] = s_tLatestGeoRVData.nX;
            data[1] = s_tLatestGeoRVData.nY;
            data[2] = s_tLatestGeoRVData.nZ;
            data[3] = s_tLatestGeoRVData.nS;
            data[4] = s_tLatestGeoRVData.nAccuracy;
            data[5] = ts.tv_sec;
            data[6] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_PEDOM:
        case SHUB_ACTIVE_PEDOM_NO_NOTIFY:
        case SHUB_ACTIVE_PEDODEC:
        case SHUB_ACTIVE_PEDODEC_NO_NOTIFY:
            // SHMDS_HUB_0303_01 mod S
//            data[0] = (uint32_t)(s_tLatestStepCountData.step - s_tLatestStepCountData.stepOffset);
            data[0] = (uint32_t)(s_tLatestStepCountData.step);
            // SHMDS_HUB_0303_01 mod E
            data[1] = ts.tv_sec;
            data[2] = ts.tv_nsec;
            break;
/* SHMDS_HUB_0132_01 add S */
        case SHUB_ACTIVE_DEVICE_ORI:
            data[2] = ts.tv_sec;
            data[3] = ts.tv_nsec;
            break;
/* SHMDS_HUB_0132_01 add E */

        case SHUB_ACTIVE_SIGNIFICANT:
            data[0] = ts.tv_sec;
            data[1] = ts.tv_nsec;
            break;
        default:
            ret =  SHUB_RC_OK;
            break;
    }
    mutex_unlock(&s_tDataMutex);

    return ret;

}

int32_t shub_activate(int32_t arg_iSensType, int32_t arg_iEnable)
{
    int32_t ret;
    uint8_t notify=0;
    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return 0;
    }

    mutex_lock(&userReqMutex);

    ret = shub_activate_pedom_exec(arg_iSensType, arg_iEnable);
    if(SHUB_RC_OK != ret) {
        goto ERROR;
    }

    ret = shub_activate_significant_exec(arg_iSensType, arg_iEnable, &notify);
    if(SHUB_RC_OK != ret) {
        goto ERROR;
    }

    ret = shub_activate_exec(arg_iSensType, arg_iEnable);
    if(SHUB_RC_OK != ret) {
        goto ERROR;
    }

/* SHMDS_HUB_0311_01 add S */
    shub_sensor_info[SHUB_SAME_NOTIFY_GYRO] = atomic_read(&g_CurrentSensorEnable) & GYRO_GROUP_MASK;
    if(shub_sensor_info[SHUB_SAME_NOTIFY_GYRO] != GYRO_GROUP_MASK){
        if(shub_sensor_info[SHUB_SAME_NOTIFY_GYRO] == 0){
            shub_sensor_first_measure_info[SHUB_SAME_NOTIFY_GYRO] = 0;
        }
        else {
            shub_sensor_first_measure_info[SHUB_SAME_NOTIFY_GYRO] = shub_sensor_info[SHUB_SAME_NOTIFY_GYRO];
        }
    }
    shub_sensor_info[SHUB_SAME_NOTIFY_MAG] = atomic_read(&g_CurrentSensorEnable) & MAG_GROUP_MASK;
    if(shub_sensor_info[SHUB_SAME_NOTIFY_MAG] != MAG_GROUP_MASK){
        if(shub_sensor_info[SHUB_SAME_NOTIFY_MAG] == 0){
            shub_sensor_first_measure_info[SHUB_SAME_NOTIFY_MAG] = 0;
        }
        else {
            shub_sensor_first_measure_info[SHUB_SAME_NOTIFY_MAG] = shub_sensor_info[SHUB_SAME_NOTIFY_MAG];
        }
    }
/* SHMDS_HUB_0311_01 add E */

    if(notify==1){
#ifdef NO_LINUX
        shub_significant_work_func(NULL);
#else
        schedule_work(&significant_work);
#endif
    }

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_activate_logging(int32_t arg_iSensType, int32_t arg_iEnable)
{
    int32_t ret;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return 0;
    }

    mutex_lock(&userReqMutex);
    ret = shub_activate_pedom_exec(arg_iSensType, arg_iEnable);
    if(SHUB_RC_OK != ret) {
        goto ERROR;
    }

    ret = shub_activate_logging_exec(arg_iSensType, arg_iEnable);
    if(SHUB_RC_OK != ret) {
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);

    return ret;
}

int32_t shub_set_delay(int32_t arg_iSensType, int32_t delay)
{
    int32_t ret = SHUB_RC_OK;
    int32_t max_delay = MEASURE_MAX_US; /* SHMDS_HUB_0120_08 add */

    delay *= 1000;

    if(delay < 0){
        delay = 0; 
    }

/* SHMDS_HUB_0120_08 add S */
#ifdef CONFIG_BARO_SENSOR
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        max_delay = SENSOR_BARO_MAX_DELAY;
    }
#endif
/* SHMDS_HUB_0120_08 add E */

/* SHMDS_HUB_0120_08 mod S */
    if(delay > max_delay){
        delay = max_delay; 
    }
/* SHMDS_HUB_0120_08 mod E */

    mutex_lock(&userReqMutex);

    DBG(DBG_LV_INFO, "####%s [%s](%dus)\n", __FUNCTION__, shub_get_active_sensor_name (arg_iSensType), delay);

    if(arg_iSensType & SHUB_ACTIVE_ACC){
/* SHMDS_HUB_0313_01 add S */
        int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
        if(iCurrentSensorEnable & SHUB_ACTIVE_SHEX_ACC) {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_exif_delay_ms() * 1000, delay);
        }
        else {
            s_sensor_delay_us.acc = delay;
        }
/* SHMDS_HUB_0313_01 add E */
/* SHMDS_HUB_0206_03 mod S */
//        s_sensor_delay_us.acc = delay;
//        s_sensor_delay_us.acc = SHUB_MIN(shub_get_exif_delay_ms() * 1000, delay);     /* SHMDS_HUB_0313_01 del */
/* SHMDS_HUB_0206_03 mod E */
    }else if(arg_iSensType & SHUB_ACTIVE_GYRO){
        s_sensor_delay_us.gyro = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_MAG){
        s_sensor_delay_us.mag = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_GYROUNC){
        s_sensor_delay_us.gyro_uc = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_MAGUNC){
        s_sensor_delay_us.mag_uc = delay;
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    }else if(arg_iSensType & SHUB_ACTIVE_BARO){
        s_sensor_delay_us.baro = delay;
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
    }else if(arg_iSensType & SHUB_ACTIVE_ORI){
        s_sensor_delay_us.orien = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        s_sensor_delay_us.grav = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_LACC){
        s_sensor_delay_us.linear = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV){
        s_sensor_delay_us.rot = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        s_sensor_delay_us.rot_gyro = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        s_sensor_delay_us.rot_mag = delay;
/* SHMDS_HUB_0201_01 add S */
    }else if(arg_iSensType & SHUB_ACTIVE_SHEX_ACC){
/* SHMDS_HUB_0313_01 add S */
        int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
        if(iCurrentSensorEnable & SHUB_ACTIVE_ACC) {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_acc_delay_ms() * 1000, delay);
        }
        else {
            s_sensor_delay_us.acc = delay;
        }
/* SHMDS_HUB_0313_01 add E */
/* SHMDS_HUB_0206_03 mod S */
//        s_sensor_delay_us.acc = delay;
//        s_sensor_delay_us.acc = SHUB_MIN(delay, s_sensor_delay_us.acc);       /* SHMDS_HUB_0313_01 del */
/* SHMDS_HUB_0206_03 mod E */
/* SHMDS_HUB_0201_01 add E */
    }

/* SHMDS_HUB_0311_01 add S */
    if(s_sensor_delay_us.gyro == s_sensor_delay_us.gyro_uc){
        shub_sensor_same_delay_flg[SHUB_SAME_NOTIFY_GYRO] = 1;
    }
    else {
        shub_sensor_same_delay_flg[SHUB_SAME_NOTIFY_GYRO] = 0;
    }
    if(s_sensor_delay_us.mag == s_sensor_delay_us.mag_uc){
        shub_sensor_same_delay_flg[SHUB_SAME_NOTIFY_MAG] = 1;
    }
    else {
        shub_sensor_same_delay_flg[SHUB_SAME_NOTIFY_MAG] = 0;
    }
/* SHMDS_HUB_0311_01 add E */

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    ret = shub_set_delay_exec(arg_iSensType,0);
    if(ret != SHUB_RC_OK) {
        goto ERROR;
    }

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}


int32_t shub_set_delay_logging(int32_t arg_iSensType, int32_t delay)
{
    int32_t ret = SHUB_RC_OK;
    int32_t max_delay = MEASURE_MAX_US; /* SHMDS_HUB_0120_08 add */

    mutex_lock(&userReqMutex);

    if(delay < 0){
        delay = 0; 
    }

    delay /= 1000;

/* SHMDS_HUB_0120_08 add S */
#ifdef CONFIG_BARO_SENSOR
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        max_delay = SENSOR_BARO_MAX_DELAY;
    }
#endif
/* SHMDS_HUB_0120_08 add E */

/* SHMDS_HUB_0120_08 mod S */
    if(delay > max_delay){
        delay = max_delay; 
    }
/* SHMDS_HUB_0120_08 mod E */

    DBG(DBG_LV_INFO, "####%s(0x%x, %dus)\n", __FUNCTION__, arg_iSensType, delay);

    if(arg_iSensType & SHUB_ACTIVE_ACC){
        s_logging_delay_us.acc = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_GYRO){
        s_logging_delay_us.gyro = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_MAG){
        s_logging_delay_us.mag = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_GYROUNC){
        s_logging_delay_us.gyro_uc = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_MAGUNC){
        s_logging_delay_us.mag_uc = delay;
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    }else if(arg_iSensType & SHUB_ACTIVE_BARO){
        s_logging_delay_us.baro = delay;
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
    }else if(arg_iSensType & SHUB_ACTIVE_ORI){
        s_logging_delay_us.orien = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        s_logging_delay_us.grav = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_LACC){
        s_logging_delay_us.linear = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV){
        s_logging_delay_us.rot = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        s_logging_delay_us.rot_gyro = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        s_logging_delay_us.rot_mag = delay;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        ret =  SHUB_RC_ERR;
        goto ERROR;
    }

    ret = shub_set_delay_exec(0, arg_iSensType);
    if(ret != SHUB_RC_OK) {
        goto ERROR;
    }

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_current_active(void)
{
    int32_t iCurrentEnable;

    iCurrentEnable = atomic_read(&g_CurrentSensorEnable) ;

    return (iCurrentEnable & ACTIVE_FUNC_MASK);
}

int32_t shub_get_current_active_logging(void)
{
    int32_t iCurrentEnable;

    iCurrentEnable = atomic_read(&g_CurrentLoggingSensorEnable) ;

    return (iCurrentEnable & ACTIVE_FUNC_MASK);
}

int32_t shub_set_param(int32_t type,int32_t* data)
{
    int32_t ret = SHUB_RC_OK;
    IoCtlParam local_param;     /* SHMDS_HUB_0331_01 add */

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    memcpy(local_param.m_iParam, data, sizeof(local_param.m_iParam));       /* SHMDS_HUB_0331_01 add */

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }
    mutex_lock(&userReqMutex);
    shub_set_param_check_exif(type, local_param.m_iParam);      /* SHMDS_HUB_0207_01 add  SHMDS_HUB_0331_01 mod */
    ret = shub_set_param_exec(type, local_param.m_iParam);      /* SHMDS_HUB_0331_01 mod */
/* SHMDS_HUB_0207_01 add S */
    if((ret == SHUB_RC_OK) && (type == APP_PEDOMETER)) {
        shub_set_enable_ped_exif_flg(local_param.m_iParam[0]);  /* SHMDS_HUB_0331_01 mod */
    }
/* SHMDS_HUB_0207_01 add E */
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_param(int32_t type,int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_param_exec(type,data);
//  shub_get_param_check_exif(type, data);      // SHMDS_HUB_0207_01 add  SHMDS_HUB_0206_07 del
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_clear_data(int32_t type)
{
    int32_t ret = SHUB_RC_OK;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_clear_data_app_exec(type);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_init_app(int32_t type)
{
    int32_t ret = SHUB_RC_OK;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_init_app_exec(type);
    mutex_unlock(&userReqMutex);
    
    /* SHMDS_HUB_0205_01 add S */
    if(ret == SHUB_RC_OK) {
/* SHMDS_HUB_0204_19 mod S */
        //ret = shub_set_default_parameter();
        ret = shub_set_default_ped_parameter();
/* SHMDS_HUB_0204_19 mod E */
        if(ret != 0) {
/* SHMDS_HUB_0204_19 mod S */
            //DBG(DBG_LV_ERROR, "Failed shub_set_default_parameter. ret=%x\n", ret);
            DBG(DBG_LV_ERROR, "Failed shub_set_default_ped_parameter. ret=%x\n", ret);
/* SHMDS_HUB_0204_19 mod E */
            return SHUB_RC_ERR;
        }
    }
    /* SHMDS_HUB_0205_01 add E */

    return ret;
}


int32_t shub_get_data_pedometer(int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_data_app_exec(APP_PEDOMETER, data);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_data_normal_pedometer(int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_data_app_exec(APP_NORMAL_PEDOMETER, data);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_data_motion(int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_data_app_exec(APP_MOTDTECTION, data);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_data_low_power(int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_data_app_exec(APP_LOW_POWER, data);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_data_act_detection(int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_data_app_exec(APP_RUN_DETECTION, data);
    ret = shub_get_data_app_exec(APP_TOTAL_STATUS_DETECTION, &data[1]);
    ret = shub_get_data_app_exec(APP_VEICHLE_DETECTION, &data[7]);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_direct_hostcmd(uint16_t cmd, const uint8_t *prm, uint8_t *rslt)
{
    HostCmd hcmd;
    HostCmdRes res;
    int32_t ret;

    if((prm == NULL) || (rslt == NULL)) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_OK;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    hcmd.cmd.u16 = cmd;
    memcpy(hcmd.prm.u8, prm, 16);

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 16);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, " err(%x)\n", res.err.u16);
        return g_hostcmdErr;
    }

    memcpy(rslt, res.res.u8, res.res_size);

    return g_hostcmdErr;
}


int32_t shub_logging_flush(void)
{
    int32_t ret;
    mutex_lock(&userReqMutex);
    ret = logging_flush_exec();
    mutex_unlock(&userReqMutex);
    return ret;
}

void shub_logging_clear(void)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    mutex_lock(&userReqMutex);
    DBG(DBG_LV_INFO, "[DBG]FIFO clear start\n");
    cmd.cmd.u16 = HC_LOGGING_GET_RESULT;
    cmd.prm.u8[0] = 0x01;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_LOGGING_GET_RESULT err(%x)\n", res.err.u16);
        mutex_unlock(&userReqMutex);
        return;
    }
    DBG(DBG_LV_INFO, "[DBG] FIFO clear end\n");
    mutex_unlock(&userReqMutex);
}

int32_t shub_write_sensor(uint8_t type, uint8_t addr ,uint8_t data)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_MCU_ACCESS_SENSOR;
    hcmd.prm.u8[0] = type;
    hcmd.prm.u8[1] = 1;//write/8bit 
    hcmd.prm.u8[2] = addr;
    hcmd.prm.u8[3] = data;
    hcmd.prm.u8[4] = 0;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 5);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)|| (0 != res.res.u8[0])) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_read_sensor(uint8_t type, uint8_t addr ,uint8_t *data)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_MCU_ACCESS_SENSOR;
    hcmd.prm.u8[0] = type;
    hcmd.prm.u8[1] = 0;//write/8bit 
    hcmd.prm.u8[2] = addr;
    hcmd.prm.u8[3] = 0;
    hcmd.prm.u8[4] = 0;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 5);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
    *data= res.res.u8[0];
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

/* SHMDS_HUB_2201_01 add S */
void shub_change_acc_axis(int32_t* offsets)
{
    int acc_axis;
    int32_t temp;

    acc_axis = shub_get_acc_axis_val();
    DBG(DBG_LV_INFO, "%s : x=%d, y=%d, z=%d, axis=%d\n", __FUNCTION__, offsets[0], offsets[1], offsets[2], acc_axis);

    switch(acc_axis){
    case 1:
        offsets[2] *= -1;              // z
        offsets[0] *= -1;              // x
        break;
    case 2:
        offsets[2] *= -1;              // z
        temp        = offsets[0];      // 
        offsets[0]  = offsets[1];      // x
        offsets[1]  = temp;            // y
        break;
    case 3:
        offsets[2] *= -1;              // z
        offsets[1] *= -1;              // y
        break;
    case 4:
        offsets[2] *= -1;              // z
        temp        = offsets[0] * -1; // 
        offsets[0]  = offsets[1] * -1; // x
        offsets[1]  = temp;             // y
        break;
    default:
        break;
    }
    DBG(DBG_LV_INFO, "%s : x=%d, y=%d, z=%d\n", __FUNCTION__, offsets[0], offsets[1], offsets[2]);
}
/* SHMDS_HUB_2201_01 add E */

int32_t shub_set_acc_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    shub_change_acc_axis(offsets); /* SHMDS_HUB_2201_01 add */

    hcmd.cmd.u16 = HC_ACC_SET_OFFSET;
    hcmd.prm.s16[0] = (int16_t)offsets[0];
    hcmd.prm.s16[1] = (int16_t)offsets[1];
    hcmd.prm.s16[2] = (int16_t)offsets[2];

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 6);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

// SHMDS_HUB_0701_05 add S
    shub_acc_offset[0] = hcmd.prm.s16[0];
    shub_acc_offset[1] = hcmd.prm.s16[1];
    shub_acc_offset[2] = hcmd.prm.s16[2];
// SHMDS_HUB_0701_05 add E
    shub_acc_offset_flg = true;  /* SHMDS_HUB_0353_01 add */

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_acc_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_ACC_GET_OFFSET;
    ret =  shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    offsets[0] = (int32_t)res.res.s16[0];
    offsets[1] = (int32_t)res.res.s16[1];
    offsets[2] = (int32_t)res.res.s16[2];

    shub_change_acc_axis(offsets); /* SHMDS_HUB_2201_01 add */

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_set_acc_position(int32_t type)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_ACC_SET_POSITION;
    hcmd.prm.u8[0] = (uint8_t)type;

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_set_mag_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_MAG_SET_OFFSET;
    hcmd.prm.s16[0] = (int16_t)offsets[0];
    hcmd.prm.s16[1] = (int16_t)offsets[1];
    hcmd.prm.s16[2] = (int16_t)offsets[2];

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 6);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_mag_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_MAG_GET_OFFSET;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    offsets[0] = (int32_t)res.res.s16[0];
    offsets[1] = (int32_t)res.res.s16[1];
    offsets[2] = (int32_t)res.res.s16[2];
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_set_mag_position(int32_t type)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_MAG_SET_POSITION;
    hcmd.prm.u8[0] = (uint8_t)type;

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_cal_mag_axis_interfrence(int32_t* mat)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    if(mat == NULL){
        hcmd.cmd.u16 = HC_MAG_SET_STATIC_MAT;
        hcmd.prm.u8[0] = 0;
        ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 12);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            ret= SHUB_RC_ERR;
            goto ERROR;
        }

    }else{
        hcmd.cmd.u16 = HC_MAG_SET_STATIC_MAT;
        hcmd.prm.u8[0] = 1;
        hcmd.prm.u8[1] = 0;
        hcmd.prm.s16[1] = mat[0];
        hcmd.prm.s16[2] = mat[1];
        hcmd.prm.s16[3] = mat[2];
        hcmd.prm.s16[4] = mat[3];
        hcmd.prm.s16[5] = mat[4];
        ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 12);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            ret= SHUB_RC_ERR;
            goto ERROR;
        }

// SHMDS_HUB_0701_05 add S
        shub_mag_axis_interfrence[0] = hcmd.prm.s16[1];
        shub_mag_axis_interfrence[1] = hcmd.prm.s16[2];
        shub_mag_axis_interfrence[2] = hcmd.prm.s16[3];
        shub_mag_axis_interfrence[3] = hcmd.prm.s16[4];
        shub_mag_axis_interfrence[4] = hcmd.prm.s16[5];
// SHMDS_HUB_0701_05 add E

        hcmd.cmd.u16 = HC_MAG_SET_STATIC_MAT;
        hcmd.prm.u8[0] = 1;
        hcmd.prm.u8[1] = 1;
        hcmd.prm.s16[1] = mat[5];
        hcmd.prm.s16[2] = mat[6];
        hcmd.prm.s16[3] = mat[7];
        hcmd.prm.s16[4] = mat[8];
        ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 12);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            ret= SHUB_RC_ERR;
            goto ERROR;
        }
        
// SHMDS_HUB_0701_05 add S
        shub_mag_axis_interfrence[5] = hcmd.prm.s16[1];
        shub_mag_axis_interfrence[6] = hcmd.prm.s16[2];
        shub_mag_axis_interfrence[7] = hcmd.prm.s16[3];
        shub_mag_axis_interfrence[8] = hcmd.prm.s16[4];
// SHMDS_HUB_0701_05 add E
        shub_mag_axis_flg = true;  /* SHMDS_HUB_0353_01 add */
        
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_set_gyro_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_GYRO_SET_OFFSET;
    hcmd.prm.s16[0] = (int16_t)offsets[0];
    hcmd.prm.s16[1] = (int16_t)offsets[1];
    hcmd.prm.s16[2] = (int16_t)offsets[2];

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 6);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_gyro_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_GYRO_GET_OFFSET;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    offsets[0] = (int32_t)res.res.s16[0];
    offsets[1] = (int32_t)res.res.s16[1];
    offsets[2] = (int32_t)res.res.s16[2];

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_set_gyro_position(int32_t type)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_GYRO_SET_POSITION;
    hcmd.prm.u8[0] = (uint8_t)type;

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        mutex_unlock(&userReqMutex);
        return SHUB_RC_ERR;
    }

    mutex_unlock(&userReqMutex);
    return ret;
}

/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
int32_t shub_set_baro_offset(int32_t offset)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_BARO_SET_OFFSET;
    hcmd.prm.s16[0] = (int16_t)offset;

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 2);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_baro_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_BARO_GET_OFFSET;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    offsets[0] = (int32_t)res.res.s16[0];
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */

int32_t shub_get_task_cycle(int32_t* cycle)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_SENSOR_TSK_GET_CYCLE;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    cycle[0] = (int32_t)res.res.u16[0] * 10;
    cycle[1] = (int32_t)res.res.u16[1] * 10;
    cycle[2] = (int32_t)res.res.u16[2] * 10;
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

void shub_debug_level_chg(int32_t lv)
{
#ifdef CONFIG_ML630Q790_DEBUG
    dbg_level = lv;
    printk("shub_debug_level_chg level:%x\n", lv);
#endif
}

#ifndef NO_LINUX
#ifdef CONFIG_HOSTIF_I2C
int32_t shub_suspend( struct i2c_client *client, pm_message_t mesg )
#endif
#ifdef CONFIG_HOSTIF_SPI
//int32_t shub_suspend( struct spi_device *client, pm_message_t mesg ) /* SHMDS_HUB_0130_01 del */
int32_t shub_suspend( struct device *dev, pm_message_t mesg )          /* SHMDS_HUB_0130_01 add */
#endif
{
    SHUB_DBG_TIME_INIT     /* SHMDS_HUB_1801_01 add */
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret=0;
    shub_suspend_call_flg = true;     // SHMDS_HUB_0402_01 add  

    SHUB_DBG_TIME_START    /* SHMDS_HUB_1801_01 add */
    suspendBatchingProc();
#ifdef SHUB_SUSPEND
    shub_sensor_timer_stop(); /* SHMDS_HUB_3301_01 mod */
    s_is_suspend = true;
#endif 
    cmd.cmd.u16 = HC_LOGGING_SET_NOTIFY;
    cmd.prm.u8[0] = 0; // Disable
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 1);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Communication Error!\n");
        goto ERROR;
    }

    if(s_enable_notify_step){
        /* Enable/Disable */
        cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            goto ERROR;
        }

        memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0335_01 mod */
        cmd.prm.u8[3] = 0;

        cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0335_01 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            goto ERROR;
        }
    }

/* SHMDS_HUB_0132_02 add S */
    if(s_setcmd_data.cmd_dev_ori[0x00] & 0x01){
        cmd.cmd.u16 = HC_ACC_GET_ANDROID_XY;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_GET_ANDROID_XY err(%x)\n", res.err.u16);
            goto ERROR;
        }

        memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_DEV_ORI_PARAM);
        cmd.prm.u8[0] = 0; // Disable

        cmd.cmd.u16 = HC_ACC_SET_ANDROID_XY;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_DEV_ORI_PARAM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_SET_ANDROID_XY err(%x)\n", res.err.u16);
            goto ERROR;
        }
    }
/* SHMDS_HUB_0132_02 add E */

ERROR:
/* SHMDS_HUB_0403_01 del S */
// #ifdef SHUB_SUSPEND
//     shub_suspend_sensor_exec();
// #endif
/* SHMDS_HUB_0403_01 del E */

/* SHMDS_HUB_0401_01 add S */
    disable_irq(g_nIntIrqNo);
    enable_irq_wake(g_nIntIrqNo);
/* SHMDS_HUB_0401_01 add E */
/* SHMDS_HUB_0701_03 add S */
    shub_dbg_out_irq_log();
    shub_dbg_clr_irq_log();
/* SHMDS_HUB_0701_03 add S */
    shub_suspend_call_flg = false;     // SHMDS_HUB_0402_01 add

/* SHMDS_HUB_0345_01 add S */
    if(atomic_read(&g_bIsIntIrqEnable) == false) {
        DBG(DBG_LV_INFO, "shub_suspend() : wake_lock\n");
        shub_wake_lock_start(&shub_int_wake_lock);
        shub_wake_lock_end(&shub_int_wake_lock);
    }
/* SHMDS_HUB_0345_01 add E */

    SHUB_DBG_TIME_END((SHUBIO<<8)+9999) /* SHMDS_HUB_1801_01 add */
    return ret;
}

#ifdef CONFIG_HOSTIF_I2C
int32_t shub_resume( struct i2c_client *client )
#endif
#ifdef CONFIG_HOSTIF_SPI
//int32_t shub_resume( struct spi_device *client ) /* SHMDS_HUB_0130_01 del */
int32_t shub_resume( struct device *dev )          /* SHMDS_HUB_0130_01 add */
#endif
{
    SHUB_DBG_TIME_INIT     /* SHMDS_HUB_1801_01 add */
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret;
    int32_t data[3]={0};
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    uint8_t reg = 0xFF;    /* SHMDS_HUB_0347_02 add */

    SHUB_DBG_TIME_START    /* SHMDS_HUB_1801_01 add */
/* SHMDS_HUB_0401_01 add S */
    disable_irq_wake(g_nIntIrqNo);
    enable_irq(g_nIntIrqNo);
/* SHMDS_HUB_0401_01 add E */

#ifdef SHUB_SUSPEND
    shub_sensor_timer_start(); /* SHMDS_HUB_3301_01 mod */
    s_is_suspend = false;
#endif

/* SHMDS_HUB_0347_02 add S */
    /* dummy read */
    ret = hostif_read(STATUS, &reg, sizeof(reg));
    if(SHUB_RC_OK != ret){
        DBG(DBG_LV_ERROR, "%s : dummy read Error!\n", __FUNCTION__);
    }
/* SHMDS_HUB_0347_02 add E */

    cmd.cmd.u16 = HC_LOGGING_SET_NOTIFY;
    cmd.prm.u8[0] = 1; // Enable
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 1);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Communication Error!\n");
    }

    if(s_enable_notify_step){
        /* Enable/Disable */
        cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0335_01 mod */
        cmd.prm.u8[3] = 1;

        cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM); /* SHMDS_HUB_0335_01 mod */
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        if((iCurrentSensorEnable & SHUB_ACTIVE_PEDOM) != 0){
            shub_get_sensors_data(SHUB_ACTIVE_PEDOM, data);
            shub_input_report_stepcnt(data);
        }
    }

/* SHMDS_HUB_0132_02 add S */
    if(s_setcmd_data.cmd_dev_ori[0x00] & 0x01){
        cmd.cmd.u16 = HC_ACC_GET_ANDROID_XY;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_GET_ANDROID_XY err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_DEV_ORI_PARAM);
        cmd.prm.u8[0] = 1; // Enable

        cmd.cmd.u16 = HC_ACC_SET_ANDROID_XY;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_DEV_ORI_PARAM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_SET_ANDROID_XY err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
/* SHMDS_HUB_0132_02 add E */

/* SHMDS_HUB_0403_01 del S */
// #ifdef SHUB_SUSPEND
//     shub_activate_exec(0, 0);//update
// #endif
/* SHMDS_HUB_0403_01 del E */

    resumeBatchingProc();

    SHUB_DBG_TIME_END((SHUBIO<<8)+9998) /* SHMDS_HUB_1801_01 add */
    return SHUB_RC_OK;
}
#endif

// SHMDS_HUB_0701_05 add S
static int sensor_rep_show(struct seq_file *s, void *what)
{
/* SHMDS_HUB_3801_02 add S */
#ifdef SHUB_SW_FREE_FALL_DETECT
    shub_durable_read_hist_dat();
#endif
/* SHMDS_HUB_3801_02 add E */
/* SHMDS_HUB_0701_11 mod S */
    seq_printf(s, "[ml630q790 ]SensorEnable=0x%08x, Logging=0x%08x\n", atomic_read(&g_CurrentSensorEnable), atomic_read(&g_CurrentLoggingSensorEnable));
    seq_printf(s, "[ml630q790 ][Task]");
    seq_printf(s, "Sensor=%s(%dus), ", ((s_micon_param.task[0] != 0)?"on":"off"), ((s_micon_param.task[0] != 0)?(s_micon_param.task_cycle[0]*10):0));
    seq_printf(s, "App=%s(%dus), ",    ((s_micon_param.task[1] != 0)?"on":"off"), ((s_micon_param.task[1] != 0)?(s_micon_param.task_cycle[1]*10):0));
    seq_printf(s, "Fusion=%s(%dus)\n", ((s_micon_param.task[2] != 0)?"on":"off"), ((s_micon_param.task[2] != 0)?(s_micon_param.task_cycle[2]*10):0));
    seq_printf(s, "[ml630q790 ][NoBatch]");
    seq_printf(s, "Acc=%s(%dus), ",  (((s_micon_param.sensors & HC_ACC_VALID)  != 0)?"on":"off"), (((s_micon_param.sensors & HC_ACC_VALID)  != 0)?s_micon_param.s_cycle[0]:0));
    seq_printf(s, "Mag=%s(%dus), ",  (((s_micon_param.sensors & HC_MAG_VALID)  != 0)?"on":"off"), (((s_micon_param.sensors & HC_MAG_VALID)  != 0)?s_micon_param.s_cycle[2]:0));
    seq_printf(s, "Gyro=%s(%dus), ", (((s_micon_param.sensors & HC_GYRO_VALID) != 0)?"on":"off"), (((s_micon_param.sensors & HC_GYRO_VALID) != 0)?s_micon_param.s_cycle[3]:0));
    seq_printf(s, "Baro=%s(%dus)\n", (((s_micon_param.sensors & HC_BARO_VALID) != 0)?"on":"off"), (((s_micon_param.sensors & HC_BARO_VALID) != 0)?s_micon_param.s_cycle[1]:0));
    seq_printf(s, "[ml630q790 ]");
    seq_printf(s, "fusion=%d\n",     s_micon_param.fusion);
    seq_printf(s, "[ml630q790 ][Logging]");
    seq_printf(s, "Acc=%s(%dus), ",  (((s_micon_param.logg_sensors & HC_ACC_VALID)  != 0)?"on":"off"), (((s_micon_param.logg_sensors & HC_ACC_VALID)  != 0)?shub_logging_cycle.acc :0));
    seq_printf(s, "Mag=%s(%dus), ",  (((s_micon_param.logg_sensors & HC_MAG_VALID)  != 0)?"on":"off"), (((s_micon_param.logg_sensors & HC_MAG_VALID)  != 0)?shub_logging_cycle.mag :0));
    seq_printf(s, "Gyro=%s(%dus), ", (((s_micon_param.logg_sensors & HC_GYRO_VALID) != 0)?"on":"off"), (((s_micon_param.logg_sensors & HC_GYRO_VALID) != 0)?shub_logging_cycle.gyro:0));
    seq_printf(s, "Baro=%s(%dus)\n", (((s_micon_param.logg_sensors & HC_BARO_VALID) != 0)?"on":"off"), (((s_micon_param.logg_sensors & HC_BARO_VALID) != 0)?shub_logging_cycle.baro:0));
    seq_printf(s, "[ml630q790 ][Logging]");
    seq_printf(s, "orien=%s(%dus), ",    (((s_micon_param.logg_fusion & HC_ORI_VALID)        != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_ORI_VALID)        != 0)?shub_logging_cycle.orien   :0));
    seq_printf(s, "grav=%s(%dus), ",     (((s_micon_param.logg_fusion & HC_GRAVITY_VALID)    != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_GRAVITY_VALID)    != 0)?shub_logging_cycle.grav    :0));
    seq_printf(s, "linear=%s(%dus), ",   (((s_micon_param.logg_fusion & HC_LACC_VALID)       != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_LACC_VALID)       != 0)?shub_logging_cycle.linear  :0));
    seq_printf(s, "rot=%s(%dus), ",      (((s_micon_param.logg_fusion & HC_RV_VALID)         != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_RV_VALID)         != 0)?shub_logging_cycle.rot     :0));
    seq_printf(s, "rot_gyro=%s(%dus), ", (((s_micon_param.logg_fusion & HC_RV_NONMAG_VALID)  != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_RV_NONMAG_VALID)  != 0)?shub_logging_cycle.rot_gyro:0));
    seq_printf(s, "rot_mag=%s(%dus)\n",  (((s_micon_param.logg_fusion & HC_RV_NONGYRO_VALID) != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_RV_NONGYRO_VALID) != 0)?shub_logging_cycle.rot_mag :0));
    seq_printf(s, "[ml630q790 ][Calibration]");
    seq_printf(s, "mag_cal=%d, ",     s_micon_param.mag_cal);
    seq_printf(s, "gyro_cal=%d, ",    s_micon_param.gyro_cal);
    seq_printf(s, "gyro_filter=%d\n", s_micon_param.gyro_filter);
    seq_printf(s, "[ml630q790 ]g_bIsIntIrqEnable=%d\n",           atomic_read(&g_bIsIntIrqEnable));
//  seq_printf(s, "[ml630q790 ]g_WakeupSensor=%x\n",              atomic_read(&g_WakeupSensor)); /* SHMDS_HUB_0701_08 mod */
    seq_printf(s, "[ml630q790 ]g_FWUpdateStatus=%d, ",            atomic_read(&g_FWUpdateStatus));
    seq_printf(s, "shub_access_flg=0x%x\n",                       shub_access_flg);     /* SHMDS_HUB_0304_02 add */
    seq_printf(s, "[ml630q790 ]LowPowerMode=%d, ",                shub_lowpower_mode);  /* SHMDS_HUB_0701_09 add */
    seq_printf(s, "OperationMode=%d\n",                           shub_operation_mode); /* SHMDS_HUB_0701_09 add */
    seq_printf(s, "[ml630q790 ]pickup_enable=%d, ",               shub_pickup_enable);  /* SHMDS_HUB_1701_15 add */
    seq_printf(s, "pickup_flg=0x%x\n",                            shub_pickup_setflg);  /* SHMDS_HUB_1701_15 add */
#ifdef SHUB_SW_FREE_FALL_DETECT                                                                /* SHMDS_HUB_3801_02 add */
    seq_printf(s, "[ml630q790 ]free fall cnt=%d\n",               s_shub_fall_hist.h_idx);     /* SHMDS_HUB_3801_02 add */
#endif                                                                                         /* SHMDS_HUB_3801_02 add */
#ifdef CONFIG_PWM_LED                                                                          /* SHMDS_HUB_3004_01 add */
    seq_printf(s, "[ml630q790 ]pwm_enable=%d, ",                  shub_pwm_enable);  /* SHMDS_HUB_3001_02 add */
    seq_printf(s, "pwm_port=%d, ",                                s_pwm_param[0]); /* SHMDS_HUB_3001_02 add */
    seq_printf(s, "pwm_initstatus=%d, ",                          s_pwm_param[1]); /* SHMDS_HUB_3001_02 add */
    seq_printf(s, "pwm_high=%d, ",                                s_pwm_param[2]); /* SHMDS_HUB_3001_02 add */
    seq_printf(s, "pwm_total=%d\n",                               s_pwm_param[3]); /* SHMDS_HUB_3001_02 add */
#endif                                                                                         /* SHMDS_HUB_3004_01 add */
#ifdef CONFIG_BKL_PWM_LED                                                                      /* SHMDS_HUB_3004_01 add */
    seq_printf(s, "[ml630q790 ][bkl_pwm]enable=%d, ",             shub_bkl_pwm_enable);                                 /* SHMDS_HUB_3003_01 add */
    seq_printf(s, "logic=%d, ",                                   s_bklpwm_param[0]);                                   /* SHMDS_HUB_3003_01 add */
    seq_printf(s, "stat=%d, ",                                    s_bklpwm_param[1]);                                   /* SHMDS_HUB_3003_01 add */
    seq_printf(s, "num=%d, ",                                     s_bklpwm_param[2]);                                   /* SHMDS_HUB_3003_01 add */
    seq_printf(s, "high1=%d, ",                                   s_bklpwm_param[3] | (s_bklpwm_param[4] & 0xff) << 8); /* SHMDS_HUB_3003_01 add */
    seq_printf(s, "total1=%d, ",                                  s_bklpwm_param[5] | (s_bklpwm_param[6] & 0xff) << 8); /* SHMDS_HUB_3003_01 add */
    seq_printf(s, "high2=%d, ",                                   s_bklpwm_param[7] | (s_bklpwm_param[8] & 0xff) << 8); /* SHMDS_HUB_3003_01 add */
    seq_printf(s, "total2=%d, ",                                  s_bklpwm_param[9] | (s_bklpwm_param[10] & 0xff) << 8);/* SHMDS_HUB_3003_01 add */
    seq_printf(s, "cnt=%d, ",                                     s_bklpwm_param[11]);                                  /* SHMDS_HUB_3003_02 add */
    seq_printf(s, "port=%d\n",                                    s_bkl_pwm_port);                                      /* SHMDS_HUB_3003_01 add */
#endif                                                                                         /* SHMDS_HUB_3004_01 add */
    seq_printf(s, "[ml630q790 ]connect_flg=%d, ",                 shub_connect_flg);       /* SHMDS_HUB_0319_01 add */
    seq_printf(s, "fw_write_flg=%d, ",                            shub_fw_write_flg);      /* SHMDS_HUB_0319_01 add */
    seq_printf(s, "FailedInitParam=%d, ",                         shub_failed_init_param); /* SHMDS_HUB_0319_01 add */
    seq_printf(s, "recovery_cnt=%d, ",                            shub_recovery_cnt);      /* SHMDS_HUB_3301_01 add */
    seq_printf(s, "user_reset=%d, ",                              shub_reset_cnt);         /* SHMDS_HUB_0304_02 add */
    seq_printf(s, "err_cmd=0x%x, ",                               (shub_err_code >> 16) & 0xFFFF); /* SHMDS_HUB_0322_02 add */
    seq_printf(s, "err_code=0x%x\n",                              (shub_err_code & 0xFFFF));       /* SHMDS_HUB_0322_02 add */
/* SHMDS_HUB_0701_11 mod E */

    seq_printf(s, "[ml630q790 ]s_sensor_delay_us ");
    seq_printf(s, "acc=%d, ",          s_sensor_delay_us.acc);
    seq_printf(s, "mag=%d, ",          s_sensor_delay_us.mag);
    seq_printf(s, "mag_uc=%d, ",       s_sensor_delay_us.mag_uc);
    seq_printf(s, "gyro=%d, ",         s_sensor_delay_us.gyro);
    seq_printf(s, "gyro_uc=%d, ",      s_sensor_delay_us.gyro_uc);
    seq_printf(s, "baro=%d, ",         s_sensor_delay_us.baro);     /* SHMDS_HUB_0120_01 add */
    seq_printf(s, "orien=%d, ",        s_sensor_delay_us.orien);
    seq_printf(s, "grav=%d\n",         s_sensor_delay_us.grav);      /* SHMDS_HUB_0701_13 mod */
    seq_printf(s, "[---       ]");                                   /* SHMDS_HUB_0701_13 add */
    seq_printf(s, "linear=%d, ",       s_sensor_delay_us.linear);
    seq_printf(s, "rot=%d, ",          s_sensor_delay_us.rot);
    seq_printf(s, "rot_gyro=%d, ",     s_sensor_delay_us.rot_gyro);
    seq_printf(s, "rot_mag=%d, ",      s_sensor_delay_us.rot_mag);
    seq_printf(s, "pedocnt=%d, ",      s_sensor_delay_us.pedocnt);
    seq_printf(s, "total_status=%d, ", s_sensor_delay_us.total_status);
    seq_printf(s, "pedocnt2=%d, ",     s_sensor_delay_us.pedocnt2);
    seq_printf(s, "total_status2=%d\n",s_sensor_delay_us.total_status2);
    
    seq_printf(s, "[ml630q790 ]s_logging_delay_us ");
    seq_printf(s, "acc=%d, ",          s_logging_delay_us.acc);
    seq_printf(s, "mag=%d, ",          s_logging_delay_us.mag);
    seq_printf(s, "mag_uc=%d, ",       s_logging_delay_us.mag_uc);
    seq_printf(s, "gyro=%d, ",         s_logging_delay_us.gyro);
    seq_printf(s, "gyro_uc=%d, ",      s_logging_delay_us.gyro_uc);
    seq_printf(s, "baro=%d, ",         s_logging_delay_us.baro);     /* SHMDS_HUB_0120_01 add */
    seq_printf(s, "orien=%d, ",        s_logging_delay_us.orien);
    seq_printf(s, "grav=%d\n",         s_logging_delay_us.grav);     /* SHMDS_HUB_0701_13 mod */
    seq_printf(s, "[---       ]");                                   /* SHMDS_HUB_0701_13 add */
    seq_printf(s, "linear=%d, ",       s_logging_delay_us.linear);
    seq_printf(s, "rot=%d, ",          s_logging_delay_us.rot);
    seq_printf(s, "rot_gyro=%d, ",     s_logging_delay_us.rot_gyro);
    seq_printf(s, "rot_mag=%d, ",      s_logging_delay_us.rot_mag);
    seq_printf(s, "pedocnt=%d, ",      s_logging_delay_us.pedocnt);
    seq_printf(s, "total_status=%d, ", s_logging_delay_us.total_status);
    seq_printf(s, "pedocnt2=%d, ",     s_logging_delay_us.pedocnt2);
    seq_printf(s, "total_status2=%d\n",s_logging_delay_us.total_status2);
    
    seq_printf(s, "[ml630q790 ]shub_dbg_cnt ");
    seq_printf(s, "irq=%lld, ",        shub_dbg_cnt_irq);
    seq_printf(s, "cmd=%lld, ",        shub_dbg_cnt_cmd);
    seq_printf(s, "acc=%lld, ",        shub_dbg_cnt_acc);
    seq_printf(s, "mag=%lld, ",        shub_dbg_cnt_mag);
    seq_printf(s, "gyro=%lld, ",       shub_dbg_cnt_gyro);
    seq_printf(s, "baro=%lld, ",       shub_dbg_cnt_baro);    /* SHMDS_HUB_0120_01 add */
    seq_printf(s, "fusion=%lld, ",     shub_dbg_cnt_fusion);
    seq_printf(s, "cust=%lld, ",       shub_dbg_cnt_cust);
    seq_printf(s, "other=%lld, ",      shub_dbg_cnt_other);
    seq_printf(s, "err=%lld, ",        shub_dbg_cnt_err);     /* SHMDS_HUB_3301_01 add */
    seq_printf(s, "none=%lld\n",       shub_dbg_cnt_none);    /* SHMDS_HUB_0704_01 add */

    seq_printf(s, "[ml630q790 ]shub_acc_offset ");
    seq_printf(s, "[0]=%d, ",          shub_acc_offset[0]);
    seq_printf(s, "[1]=%d, ",          shub_acc_offset[1]);
    seq_printf(s, "[2]=%d\n",          shub_acc_offset[2]);

    seq_printf(s, "[ml630q790 ]shub_mag_axis_interfrence ");
    seq_printf(s, "[0]=%d, ",          shub_mag_axis_interfrence[0]);
    seq_printf(s, "[1]=%d, ",          shub_mag_axis_interfrence[1]);
    seq_printf(s, "[2]=%d, ",          shub_mag_axis_interfrence[2]);
    seq_printf(s, "[3]=%d, ",          shub_mag_axis_interfrence[3]);
    seq_printf(s, "[4]=%d, ",          shub_mag_axis_interfrence[4]);
    seq_printf(s, "[5]=%d, ",          shub_mag_axis_interfrence[5]);
    seq_printf(s, "[6]=%d, ",          shub_mag_axis_interfrence[6]);
    seq_printf(s, "[7]=%d, ",          shub_mag_axis_interfrence[7]);
    seq_printf(s, "[8]=%d\n",          shub_mag_axis_interfrence[8]);

    seq_printf(s, "[ml630q790 ]LatestACC  ");
    seq_printf(s, "X=%d, ",            s_tLatestAccData.nX); /* SHMDS_HUB_0701_07 mod */
    seq_printf(s, "Y=%d, ",            s_tLatestAccData.nY); /* SHMDS_HUB_0701_07 mod */
    seq_printf(s, "Z=%d\n",            s_tLatestAccData.nZ); /* SHMDS_HUB_0701_07 mod */
    
    seq_printf(s, "[ml630q790 ]LatestGYRO ");
    seq_printf(s, "X=%d, ",            s_tLatestGyroData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestGyroData.nY);
    seq_printf(s, "Z=%d, ",            s_tLatestGyroData.nZ);
    seq_printf(s, "OX=%d, ",           s_tLatestGyroData.nXOffset);
    seq_printf(s, "OY=%d, ",           s_tLatestGyroData.nYOffset);
    seq_printf(s, "OZ=%d, ",           s_tLatestGyroData.nZOffset);
    seq_printf(s, "Accuracy=%d\n",     s_tLatestGyroData.nAccuracy);

    seq_printf(s, "[ml630q790 ]LatestMAG  ");
    seq_printf(s, "X=%d, ",            s_tLatestMagData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestMagData.nY);
    seq_printf(s, "Z=%d, ",            s_tLatestMagData.nZ);
    seq_printf(s, "OX=%d, ",           s_tLatestMagData.nXOffset);
    seq_printf(s, "OY=%d, ",           s_tLatestMagData.nYOffset);
    seq_printf(s, "OZ=%d, ",           s_tLatestMagData.nZOffset);
    seq_printf(s, "Accuracy=%d\n",     s_tLatestMagData.nAccuracy);

/* SHMDS_HUB_0120_01 add S */
    seq_printf(s, "[ml630q790 ]LatestBARO ");
    seq_printf(s, "Pressure=%d\n",     s_tLatestBaroData.pressure);
/* SHMDS_HUB_0120_01 add E */
    
    seq_printf(s, "[ml630q790 ]LatestORI  ");
    seq_printf(s, "P=%d, ",            s_tLatestOriData.pitch);
    seq_printf(s, "R=%d, ",            s_tLatestOriData.roll);
    seq_printf(s, "Y=%d, ",            s_tLatestOriData.yaw);
    seq_printf(s, "Accuracy=%d\n",     s_tLatestOriData.nAccuracy);

    seq_printf(s, "[ml630q790 ]LatestGRV  ");
    seq_printf(s, "X=%d, ",            s_tLatestGravityData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestGravityData.nY);
    seq_printf(s, "Z=%d\n",            s_tLatestGravityData.nZ);

    seq_printf(s, "[ml630q790 ]LatestLACC ");
    seq_printf(s, "X=%d, ",            s_tLatestLinearAccData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestLinearAccData.nY);
    seq_printf(s, "Z=%d\n",            s_tLatestLinearAccData.nZ);

    seq_printf(s, "[ml630q790 ]LatestRv   ");
    seq_printf(s, "X=%d, ",            s_tLatestRVectData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestRVectData.nY);
    seq_printf(s, "Z=%d, ",            s_tLatestRVectData.nZ);
    seq_printf(s, "S=%d, ",            s_tLatestRVectData.nS);
    seq_printf(s, "Accuracy=%d\n",     s_tLatestRVectData.nAccuracy);

    seq_printf(s, "[ml630q790 ]LatestGRV  ");
    seq_printf(s, "X=%d, ",            s_tLatestGeoRVData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestGeoRVData.nY);
    seq_printf(s, "Z=%d, ",            s_tLatestGeoRVData.nZ);
    seq_printf(s, "S=%d, ",            s_tLatestGeoRVData.nS);
    seq_printf(s, "Accuracy=%d\n",     s_tLatestGeoRVData.nAccuracy);
    
    seq_printf(s, "[ml630q790 ]LatestSTEP ");
    seq_printf(s, "S=%llu, ",          s_tLatestStepCountData.step);
    seq_printf(s, "OS=%llu\n",         s_tLatestStepCountData.stepOffset);

/* SHMDS_HUB_0705_01 add S */
    seq_printf(s, "[ml630q790 ]Suspend_RW ");
    seq_printf(s, "write=%lld, ",      shub_dbg_host_wcnt);
    seq_printf(s, "read=%lld\n",       shub_dbg_host_rcnt);
/* SHMDS_HUB_0705_01 add E */

/* SHMDS_HUB_0703_01 add S */
    seq_printf(s, "[ml630q790 ]Cmd_Timeout_Err(1st)  ");
    seq_printf(s, "cmd=0x%04x, ",      s_tTimeoutCmd[0]);
    seq_printf(s, "IrqEnable=%d, ",    s_tTimeoutIrqEnable[0]);
    shub_print_time(s, s_tTimeoutTimestamp[0]);                 /* SHMDS_HUB_0703_02 mod */
    seq_printf(s, "\n");                                        /* SHMDS_HUB_0703_02 mod */

    seq_printf(s, "[ml630q790 ]Cmd_Timeout_Err(Last) ");
    seq_printf(s, "cmd=0x%04x, ",      s_tTimeoutCmd[1]);
    seq_printf(s, "IrqEnable=%d, ",    s_tTimeoutIrqEnable[1]);
    shub_print_time(s, s_tTimeoutTimestamp[1]);                 /* SHMDS_HUB_0703_02 mod */
    seq_printf(s, ", cnt=%u\n",        s_tTimeoutErrCnt);       /* SHMDS_HUB_0703_02 mod */

    seq_printf(s, "[ml630q790 ]Hosif_RW_Err(1st)  ");
    seq_printf(s, "addr=0x%02x, ",     s_tRwErrAdr[0]);
    seq_printf(s, "size=%d, ",         s_tRwErrSize[0]);
    seq_printf(s, "ret=%d, ",          s_tRwErrCode[0]);        /* SHMDS_HUB_0703_03 mod */
    shub_print_time(s, s_tRwErrTimestamp[0]);                   /* SHMDS_HUB_0703_02 mod */
    seq_printf(s, "\n");                                        /* SHMDS_HUB_0703_02 mod */

    seq_printf(s, "[ml630q790 ]Hosif_RW_Err(Last) ");
    seq_printf(s, "addr=0x%02x, ",     s_tRwErrAdr[1]);
    seq_printf(s, "size=%d, ",         s_tRwErrSize[1]);
    seq_printf(s, "ret=%d, ",          s_tRwErrCode[1]);        /* SHMDS_HUB_0703_03 mod */
    shub_print_time(s, s_tRwErrTimestamp[1]);                   /* SHMDS_HUB_0703_02 mod */
    seq_printf(s, ", cnt=%u\n",          s_tRwErrCnt);          /* SHMDS_HUB_0703_02 mod */
/* SHMDS_HUB_0703_01 add E */

    shub_sensor_rep_input_exif(s);
    shub_sensor_rep_input_acc(s);
    shub_sensor_rep_input_gyro(s);
    shub_sensor_rep_input_gyro_uncal(s);
    shub_sensor_rep_input_mag(s);
    shub_sensor_rep_input_mag_uncal(s);
    shub_sensor_rep_input_orien(s);
    shub_sensor_rep_input_grav(s);
    shub_sensor_rep_input_linear(s);
    shub_sensor_rep_input_rot(s);
    shub_sensor_rep_input_game_rot_gyro(s);
    shub_sensor_rep_input_rot_mag(s);
    shub_sensor_rep_input_pedo(s);
    shub_sensor_rep_input_pedodect(s);
    shub_sensor_rep_input_mcu(s);
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add S */
#ifdef CONFIG_BARO_SENSOR
    shub_sensor_rep_input_baro(s);
#endif
/* SHMDS_HUB_0120_01  SHMDS_HUB_0122_01 add E */
/* SHMDS_HUB_0114_01 mod S */
#ifdef CONFIG_HOSTIF_I2C
#endif
#ifdef CONFIG_HOSTIF_SPI
    shub_sensor_rep_spi(s);
#endif
/* SHMDS_HUB_0114_01 mod E */

    return 0;
}

static int sensor_rep_open(struct inode *inode, struct file *file)
{
    return single_open(file, sensor_rep_show, NULL);
}

static const struct file_operations sensor_rep_ops = {
    .open       = sensor_rep_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
// SHMDS_HUB_0701_05 add E

/* SHMDS_HUB_0702_01 add S */
#ifdef CONFIG_ANDROID_ENGINEERING
static int shub_sus_flag = 0;

static int sensor_sus_show(struct seq_file *s, void *what)
{
    pm_message_t msg;
    
    if(shub_sus_flag == 0){
        shub_suspend(client_mcu, msg);
        shub_sus_flag = 1;
        DBG(DBG_LV_ERROR, "sensor_sus_show : shub_suspend()\n");
    }else{
        shub_resume(client_mcu);
        shub_sus_flag = 0;
        DBG(DBG_LV_ERROR, "sensor_sus_show : shub_resume()\n");
    }
    return 0;
}

static int sensor_sus_open(struct inode *inode, struct file *file)
{
    return single_open(file, sensor_sus_show, NULL);
}

static const struct file_operations sensor_sus_ops = {
    .open       = sensor_sus_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
#endif
/* SHMDS_HUB_0702_01 add E */

/* SHMDS_HUB_3301_01 add S */
#ifdef CONFIG_ANDROID_ENGINEERING
static int sensor_recovery_show(struct seq_file *s, void *what)
{
    seq_printf(s, "%s : start\n", __FUNCTION__);
    shub_wake_lock_start(&shub_recovery_wake_lock);
    
    shub_recovery();
    
    shub_wake_lock_end(&shub_recovery_wake_lock);
    seq_printf(s, "%s : end\n", __FUNCTION__);
    return 0;
}

static int sensor_recovery_open(struct inode *inode, struct file *file)
{
    return single_open(file, sensor_recovery_show, NULL);
}

static const struct file_operations sensor_recovery_ops = {
    .open       = sensor_recovery_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
#endif
/* SHMDS_HUB_3301_01 add E */

int32_t shub_probe(void)
{
    SHUB_DBG_TIME_INIT     /* SHMDS_HUB_1801_01 add */
    int32_t ret;
    struct proc_dir_entry *entry;  // SHMDS_HUB_0701_05 add
    /* SHMDS_HUB_3003_03 add S */
#if defined(CONFIG_SHARP_DISPLAY) && defined(SHUB_SW_FLBL)  // SHMDS_HUB_3003_04 add
    int rc = 0;
    static struct flbl_cb_tbl cb_tbl = {0};
#endif  /* CONFIG_SHARP_DISPLAY, SHUB_SW_FLBL */  // SHMDS_HUB_3003_04 add
    /* SHMDS_HUB_3003_03 add E */
    SHUB_DBG_TIME_START    /* SHMDS_HUB_1801_01 add */
    printk("[shub] shub_probe start\n");
    shub_wake_lock_init();     // SHMDS_HUB_0402_01 add
    shub_qos_init();           // SHMDS_HUB_1101_01 add
    accsns_wq_int = create_singlethread_workqueue("accsns_wq_int");
    if(!accsns_wq_int)
    {
        DBG(DBG_LV_ERROR, "can't create interrupt queue : accsns_wq_int \n");
        goto REGIST_ERR;
    }
    
    initBatchingProc();
// SHMDS_HUB_0701_05 add S
    entry = proc_create("driver/sensor_rep", S_IRUGO, NULL, &sensor_rep_ops);
    if (!entry)
        DBG(DBG_LV_ERROR, "shub proc_create error\n");
/* SHMDS_HUB_0702_01 add E */

/* SHMDS_HUB_0702_01 add S */
#ifdef CONFIG_ANDROID_ENGINEERING
    entry = proc_create("driver/sensor_sus", S_IRUGO, NULL, &sensor_sus_ops);
    if (!entry){
        DBG(DBG_LV_ERROR, "shub proc_create error : sensor_sus\n");
    }
#endif
/* SHMDS_HUB_0702_01 add E */

/* SHMDS_HUB_3301_01 add S */
#ifdef CONFIG_ANDROID_ENGINEERING
    entry = proc_create("driver/sensor_recovery", S_IRUGO, NULL, &sensor_recovery_ops);
    if (!entry){
        DBG(DBG_LV_ERROR, "shub proc_create error : sensor_recovery\n");
    }
#endif
/* SHMDS_HUB_3301_01 add E */

/* SHMDS_HUB_3801_02 add S */
#ifdef SHUB_SW_FREE_FALL_DETECT
#ifdef CONFIG_ANDROID_ENGINEERING
    entry = proc_create("driver/sensor_freefall", S_IRUGO, NULL, &sensor_freefall_ops);
    if (!entry){
        DBG(DBG_LV_ERROR, "shub proc_create error : sensor_freefall\n");
    }
#endif
#endif
/* SHMDS_HUB_3801_02 add E */

    SHUB_DBG_TIME_END((SHUBIO<<8)+9997) /* SHMDS_HUB_1801_01 add */


    g_nIntIrqNo = -1;
    g_nIntIrqFlg = 0;

    s_sensor_delay_us.acc       = SENSOR_ACC_MIN_DELAY;
    s_sensor_delay_us.mag       = SENSOR_MAG_MIN_DELAY;
    s_sensor_delay_us.mag_uc    = SENSOR_MAGUC_MIN_DELAY;
    s_sensor_delay_us.gyro      = SENSOR_GYRO_MIN_DELAY;
    s_sensor_delay_us.gyro_uc   = SENSOR_GYROUC_MIN_DELAY;
    s_sensor_delay_us.baro      = SENSOR_BARO_MIN_DELAY;    /* SHMDS_HUB_0120_01 add */
    s_sensor_delay_us.orien     = FUSION_MIN_DELAY;
    s_sensor_delay_us.grav      = FUSION_MIN_DELAY;
    s_sensor_delay_us.linear    = FUSION_MIN_DELAY;
    s_sensor_delay_us.rot       = FUSION_MIN_DELAY;
    s_sensor_delay_us.rot_gyro  = FUSION_MIN_DELAY;
    s_sensor_delay_us.rot_mag   = FUSION_MIN_DELAY;
    s_sensor_delay_us.pedocnt   = APP_MIN_DELAY;

    s_logging_delay_us.acc      = SENSOR_ACC_MIN_DELAY;
    s_logging_delay_us.mag      = SENSOR_MAG_MIN_DELAY;
    s_logging_delay_us.mag_uc   = SENSOR_MAGUC_MIN_DELAY;
    s_logging_delay_us.gyro     = SENSOR_GYRO_MIN_DELAY;
    s_logging_delay_us.gyro_uc  = SENSOR_GYROUC_MIN_DELAY;
    s_logging_delay_us.baro     = SENSOR_BARO_MIN_DELAY;    /* SHMDS_HUB_0120_01 add */
    s_logging_delay_us.orien    = FUSION_MIN_DELAY;
    s_logging_delay_us.grav     = FUSION_MIN_DELAY;
    s_logging_delay_us.linear   = FUSION_MIN_DELAY;
    s_logging_delay_us.rot      = FUSION_MIN_DELAY;
    s_logging_delay_us.rot_gyro = FUSION_MIN_DELAY;
    s_logging_delay_us.rot_mag  = FUSION_MIN_DELAY;
    s_logging_delay_us.pedocnt  = APP_MIN_DELAY;
    shub_logging_cycle = s_logging_delay_us; /* SHMDS_HUB_0701_11 add */

    s_sensor_task_delay_us = SENSOR_TSK_DEFALUT_US;

    atomic_set(&g_CurrentSensorEnable,ACTIVE_OFF);
    atomic_set(&g_CurrentLoggingSensorEnable,ACTIVE_OFF);
    atomic_set(&g_WakeupSensor,ACTIVE_OFF);
    atomic_set(&g_FWUpdateStatus,false);

    shub_lowpower_mode = 0;  /* SHMDS_HUB_3101_01 add */
/* SHMDS_HUB_2901_03 mod S */
#ifdef CONFIG_ACC_U2DH
    shub_operation_mode = 0;
#else
    shub_operation_mode = 1; /* SHMDS_HUB_3101_01 add */
#endif
/* SHMDS_HUB_2901_03 mod E */
#ifdef CONFIG_BARO_SENSOR
    shub_baro_odr = 0;       /* SHMDS_HUB_3101_01 add */
#endif

    memset(&s_tLatestAccData       , 0x00 , sizeof(s_tLatestAccData));
    memset(&s_tLatestGyroData      , 0x00 , sizeof(s_tLatestGyroData));
    memset(&s_tLatestMagData       , 0x00 , sizeof(s_tLatestMagData));
    memset(&s_tLatestBaroData      , 0x00 , sizeof(s_tLatestBaroData));    /* SHMDS_HUB_0120_01 add */
    memset(&s_tLoggingBaroData     , 0x00 , sizeof(s_tLoggingBaroData));   /* SHMDS_HUB_0120_10 add */
    memset(&s_tLatestOriData       , 0x00 , sizeof(s_tLatestOriData));
    memset(&s_tLatestGravityData   , 0x00 , sizeof(s_tLatestGravityData));
    memset(&s_tLatestLinearAccData , 0x00 , sizeof(s_tLatestLinearAccData));
    memset(&s_tLatestRVectData     , 0x00 , sizeof(s_tLatestRVectData));
    memset(&s_tLatestGameRVData    , 0x00 , sizeof(s_tLatestGameRVData));
    memset(&s_tLatestGeoRVData     , 0x00 , sizeof(s_tLatestGeoRVData));
    memset(&s_tLatestStepCountData   , 0x00 , sizeof(s_tLatestStepCountData));
    memset(&s_setcmd_data, 0x00, sizeof(s_setcmd_data));                   /* SHMDS_HUB_3301_01 add */
    memset(&s_recovery_data, 0x00, sizeof(s_recovery_data));               /* SHMDS_HUB_3301_01 add */

    init_waitqueue_head(&s_tWaitInt);
    shub_workqueue_init();
    INIT_WORK(&acc_irq_work, shub_int_acc_work_func);
    INIT_WORK(&significant_work, shub_significant_work_func);
    INIT_WORK(&mag_irq_work, shub_int_mag_work_func);
    INIT_WORK(&customer_irq_work, shub_int_customer_work_func);
    INIT_WORK(&gyro_irq_work, shub_int_gyro_work_func);
    INIT_WORK(&fusion_irq_work, shub_int_fusion_work_func);
    INIT_WORK(&recovery_irq_work, shub_int_recovery_work_func);            /* SHMDS_HUB_3301_01 add */

    mutex_init(&s_tDataMutex);
    mutex_init(&s_hostCmdMutex);
    mutex_init(&userReqMutex);

    spin_lock_init( &s_intreqData );

#ifndef NO_LINUX
    if(g_nIntIrqNo == -1){
        ret = shub_gpio_init();
        if(ret != SHUB_RC_OK){
            DBG(DBG_LV_ERROR, "Failed shub_gpio_init. ret=%x\n", ret);
            goto REGIST_ERR;
        }
        DISABLE_IRQ;
    }
#endif

    shub_qos_start();    // SHMDS_HUB_1101_01 add
    ret = shub_initialize();
    shub_qos_end();      // SHMDS_HUB_1101_01 add
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Failed shub_initialize. ret=%x\n", ret);
        return -ENODEV;
    }

#if defined(CONFIG_SHARP_DISPLAY) && defined(SHUB_SW_FLBL)  // SHMDS_HUB_3003_04 add
    /* SHMDS_HUB_3003_03 add S */
    cb_tbl.set_param_bkl_pwm = shub_api_set_param_bkl_pwm;
    cb_tbl.enable_bkl_pwm = shub_api_enable_bkl_pwm;
    cb_tbl.set_port_bkl_pwm = shub_api_set_port_bkl_pwm;
    rc = flbl_register_cb(&cb_tbl);
    if(0 != rc) {
        DBG(DBG_LV_ERROR, "Failed flbl_register_cb. rc=%x\n", rc);
    }
    /* SHMDS_HUB_3003_03 add E */
#endif  /* CONFIG_SHARP_DISPLAY, SHUB_SW_FLBL */  // SHMDS_HUB_3003_04 add

    DBG(DBG_LV_INFO, "Init Complete!!!\n");
    return 0;

REGIST_ERR:
    if(accsns_wq_int != NULL){
        flush_workqueue(accsns_wq_int);
        destroy_workqueue(accsns_wq_int);
        accsns_wq_int = NULL;
    }

    return 0;

}

int32_t shub_remove(void)
{

    DISABLE_IRQ;
    
    shub_wake_lock_destroy();     // SHMDS_HUB_0402_01 add
    shub_qos_destroy();           // SHMDS_HUB_1101_01 add

    cancel_work_sync(&acc_irq_work);
    cancel_work_sync(&significant_work);
    cancel_work_sync(&fusion_irq_work);
    cancel_work_sync(&gyro_irq_work);
    cancel_work_sync(&mag_irq_work);
    cancel_work_sync(&customer_irq_work);
    cancel_work_sync(&recovery_irq_work); /* SHMDS_HUB_3301_01 add */

    if(accsns_wq_int != NULL){
        flush_workqueue(accsns_wq_int);
        destroy_workqueue(accsns_wq_int);
        accsns_wq_int = NULL;
    }

    shub_gpio_free(SHUB_GPIO_PIN_INT0); /* SHMDS_HUB_0110_01 mod */

#ifndef SHUB_SW_GPIO_PMIC /* SHMDS_HUB_0104_03 del */
#ifdef USE_RESET_SIGNAL   
    shub_gpio_free(SHUB_GPIO_PIN_RESET); /* SHMDS_HUB_0110_01 mod */
#endif
#endif

#ifdef USE_REMAP_SIGNAL
    shub_gpio_free(SHUB_GPIO_PIN_BRMP);  /* SHMDS_HUB_0110_01 mod */
#endif

    remove_proc_entry("driver/sensor_rep", NULL);  /* SHMDS_HUB_0706_01 mod */
/* SHMDS_HUB_0706_01 add S */
#ifdef CONFIG_ANDROID_ENGINEERING
    remove_proc_entry("driver/sensor_sus", NULL);
    remove_proc_entry("driver/sensor_recovery", NULL);
#endif
/* SHMDS_HUB_0706_01 add E */
    return 0;
}

/* SHMDS_HUB_3701_01 add S */
static int32_t __init shub_init(void)
{
    int ret;
    
    DBG(DBG_LV_ERROR, "shub_init start\n");
    
#ifdef CONFIG_HOSTIF_SPI
    ret = shub_spi_init();
#endif
    ret = shub_mcu_init();
    ret = shub_exif_init();
    ret = shub_acc_init();
    ret = shub_gyro_init();
    ret = shub_mag_init();
    ret = shub_orien_init();
    ret = shub_linearacc_init();
#ifdef CONFIG_BARO_SENSOR
    ret = shub_baro_init();
#endif
    ret = shub_rot_gyro_init();
    ret = shub_grav_init();
    ret = shub_gyrounc_init();
    ret = shub_rot_mag_init();
    ret = shub_mag_unc_init();
    ret = shub_rot_init();
    ret = shub_pedo_init();
    ret = shub_pedodet_init();
    ret = shub_signif_init();
    ret = shub_devori_init();
    ret = shub_diag_init();
    
    DBG(DBG_LV_ERROR, "shub_init end\n");
    return ret;
}

static void __exit shub_exit(void)
{
    shub_diag_exit();
    shub_devori_exit();
    shub_signif_exit();
    shub_pedodet_exit();
    shub_pedo_exit();
    shub_rot_exit();
    shub_mag_unc_exit();
    shub_rot_mag_exit();
    shub_gyrounc_exit();
    shub_grav_exit();
    shub_rot_gyro_exit();
#ifdef CONFIG_BARO_SENSOR
    shub_baro_exit();
#endif
    shub_linearacc_exit();
    shub_orien_exit();
    shub_mag_exit();
    shub_gyro_exit();
    shub_acc_exit();
    shub_exif_exit();
    shub_mcu_exit();
#ifdef CONFIG_HOSTIF_SPI
    shub_spi_exit();
#endif
}

module_init(shub_init);
module_exit(shub_exit);

EXPORT_SYMBOL(shub_api_get_acc_info);
/* SHMDS_HUB_3701_01 add E */

#ifndef NO_LINUX
MODULE_AUTHOR("LAPIS SEMICONDUCTOR");
MODULE_DESCRIPTION("SensorHub Driver");
MODULE_LICENSE("GPL v2");
#endif

