/*
 *  shub-input_gyro.c - Linux kernel modules for interface of ML630Q790
 *
 *  Copyright (C) 2012-2014 LAPIS SEMICONDUCTOR CO., LTD.
 *
 *  This file is based on :
 *    alps-input.c - Linux kernel modules for interface of ML610Q792
 *    http://android-dev.kyocera.co.jp/source/versionSelect_URBANO.html
 *    (C) 2012 KYOCERA Corporation
 *    Copyright (C) 2010 ALPS
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
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <asm/uaccess.h> 

#include "shub_io.h"
#include "ml630q790.h"

// SHMDS_HUB_0701_01 add S
#ifdef CONFIG_ANDROID_ENGINEERING
static int shub_gyrounc_log = 0;
module_param(shub_gyrounc_log, int, 0600);
#define DBG_GYROUNC_IO(msg, ...) {                      \
    if(shub_gyrounc_log & 0x01)                         \
        printk("[shub][gyrounc] " msg, ##__VA_ARGS__);  \
}
#define DBG_GYROUNC_DATA(msg, ...) {                    \
    if(shub_gyrounc_log & 0x02)                         \
        printk("[shub][gyrounc] " msg, ##__VA_ARGS__);  \
}
#else
#define DBG_GYROUNC_IO(msg, ...)
#define DBG_GYROUNC_DATA(msg, ...)
#endif
// SHMDS_HUB_0701_01 add E

#define GYROUNC_MAX       28571    /* SHMDS_HUB_0123_01 mod ( 20000 ->  28571) */
#define GYROUNC_MIN       -28571   /* SHMDS_HUB_0123_01 mod (-20000 -> -28571) */
#define INDEX_X            0
#define INDEX_Y            1
#define INDEX_Z            2
#define INDEX_ACC          3
#define INDEX_X_BIAS       4
#define INDEX_Y_BIAS       5
#define INDEX_Z_BIAS       6
#define INDEX_TM           7
#define INDEX_TMNS         8
#define INDEX_SUM          9
#define INPUT_DEV_NAME "shub_gyrounc"
#define INPUT_DEV_PHYS "shub_gyrounc/input0"
#define MISC_DEV_NAME  "shub_io_gyrounc"
#define SHUB_EVENTS_PER_PACKET ((LOGGING_RAM_SIZE/(DATA_SIZE_GYRO+1))*2) /*+1 is ID size*/
#define SHUB_ACTIVE_SENSOR SHUB_ACTIVE_GYROUNC
static DEFINE_MUTEX(shub_lock);

static struct platform_device *pdev;
static struct input_dev *shub_idev;
static int32_t        power_state     = 0;
static int32_t        delay           = 200;//200ms
static IoCtlBatchInfo batch_param     = { 0, 0, 0 };
static struct work_struct sensor_poll_work;

static void shub_sensor_poll_work_func(struct work_struct *work);
static void shub_set_sensor_poll(int32_t en);
#if 0  // SHMDS_HUB_0601_01 del S
static void shub_set_abs_params(void);
#endif // SHMDS_HUB_0601_01 del E
static int32_t currentActive;

/* SHMDS_HUB_0321_01 add S */
static int32_t input_gyrounc[INDEX_SUM]= {0};
static int32_t input_flg = 0;
static int32_t xyz_gyro[6]= {0};
/* SHMDS_HUB_0321_01 add E */

static struct hrtimer poll_timer;
extern int32_t setMaxBatchReportLatency(uint32_t sensor, int64_t latency);

static int32_t shub_probe_gyrounc(struct platform_device *pfdev);
static int32_t shub_remove_gyrounc(struct platform_device *pfdev);

#ifdef CONFIG_OF
    static struct of_device_id shub_of_match_tb_gyrounc[] = {
        { .compatible = "sharp,shub_gyrounc" ,},
        {}
    };
#else
    #define shub_of_match_tb_gyrounc NULL
#endif

static long shub_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    int32_t ret = -1, tmpval = 0;
    switch (cmd) {
        case SHUBIO_GYROUNCAL_ACTIVATE:
            ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
            if (ret) {
                printk("error : shub_ioctl(cmd = SHUBIO_GYROUNCAL_ACTIVATE)\n");
                return -EFAULT;
            }
            DBG_GYROUNC_IO("ioctl(cmd = Set_Active) : val=%d\n", tmpval); // SHMDS_HUB_0701_01 add
            mutex_lock(&shub_lock);
            currentActive = shub_get_current_active() & SHUB_ACTIVE_SENSOR;
            if((batch_param.m_Latency != 0) && (currentActive == 0)){
                //polling off and batch enable/disable
                if(tmpval != 0){
                    //batch start/stop
                    if(batch_param.m_Latency > 0){
                        ret = shub_activate_logging(SHUB_ACTIVE_SENSOR, 1);
                    }else{
                        ret = shub_activate_logging(SHUB_ACTIVE_SENSOR, 0);
                    }
                }else{
                    //batch stop
                    ret = shub_activate_logging(SHUB_ACTIVE_SENSOR, 0);
                    setMaxBatchReportLatency(SHUB_ACTIVE_SENSOR, 0);
                }
                //set polling stop 
                shub_set_sensor_poll(0);
            }else{
                //set mcu sensor measure
                ret = shub_activate( SHUB_ACTIVE_SENSOR, tmpval);
                currentActive = shub_get_current_active() & SHUB_ACTIVE_SENSOR;
                //set polling start 
                shub_set_sensor_poll(tmpval);

                //batch stop 
                ret = shub_activate_logging(SHUB_ACTIVE_SENSOR, 0);
                setMaxBatchReportLatency(SHUB_ACTIVE_SENSOR, 0);
            }
            if(ret != -1){
                power_state = tmpval;
            }else{
                mutex_unlock(&shub_lock);
                return -EFAULT;
            }
            mutex_unlock(&shub_lock);
            break;

        case SHUBIO_GYROUNCAL_SET_FREQ:
            {
                ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
                if (ret) {
                    printk( "error : shub_ioctl(cmd = SHUBIO_GYROUNCAL_SET_FREQ)\n" );
                    return -EFAULT;
                }
                DBG_GYROUNC_IO("ioctl(cmd = Set_Delay) : delay=%d\n", tmpval); // SHMDS_HUB_0701_01 add
                mutex_lock(&shub_lock);
                delay = tmpval;
                delay = (delay > SHUB_TIMER_MAX) ? SHUB_TIMER_MAX : delay;
                shub_set_delay(SHUB_ACTIVE_SENSOR, delay);
                if(currentActive != 0){
                    shub_set_sensor_poll(1);
                }
                mutex_unlock(&shub_lock);
            }
            break;

        case SHUBIO_GYROUNCAL_SET_BATCH :
            {
                IoCtlBatchInfo param;
                uint64_t delayNs;
                ret = copy_from_user(&param, argp, sizeof(param));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_GYROUNCAL_SET_BATCH\n" );
                    return -EFAULT;
                }
                DBG_GYROUNC_IO("ioctl(cmd = Set_Batch) : flg=%d, Period=%lld, Latency=%lld\n", param.m_Flasg, param.m_PeriodNs, param.m_Latency); // SHMDS_HUB_0701_01 add
                if((param.m_Flasg & 0x01) == 0x01){
                    return 0;
                }
                mutex_lock(&shub_lock);
                delayNs = param.m_PeriodNs;
                delay = (int32_t)do_div(delayNs, 1000000);
                delay = (int32_t)delayNs;
                delay = (delay > SHUB_TIMER_MAX) ? SHUB_TIMER_MAX : delay;
                if(power_state != 0){
                    //poll on -> batch on
                    if((batch_param.m_Latency == 0) && (param.m_Latency > 0))
                    {
                        batch_param.m_Flasg    = param.m_Flasg;
                        batch_param.m_PeriodNs = param.m_PeriodNs;
                        batch_param.m_Latency  = param.m_Latency;

                        //pause poll 
                        currentActive = 0;
                        shub_set_sensor_poll(0);

                        //enable batch
                        shub_set_delay_logging(SHUB_ACTIVE_SENSOR, batch_param.m_PeriodNs);
                        ret = shub_activate_logging(SHUB_ACTIVE_SENSOR, 1);
                        if(ret == -1){
                            mutex_unlock(&shub_lock);
                            return -EFAULT;                   
                        }

                        //disable poll 
                        ret = shub_activate( SHUB_ACTIVE_SENSOR, 0);
                        if(ret == -1){
                            mutex_unlock(&shub_lock);
                            return -EFAULT;
                        }

                        //start batch
                        setMaxBatchReportLatency(SHUB_ACTIVE_SENSOR, batch_param.m_Latency);

                        mutex_unlock(&shub_lock);
                        return 0;
                    }
                    //batch on -> poll on
                    if((batch_param.m_Latency > 0) && (param.m_Latency == 0))
                    {
                        batch_param.m_Flasg    = param.m_Flasg;
                        batch_param.m_PeriodNs = param.m_PeriodNs;
                        batch_param.m_Latency  = param.m_Latency;

                        //pause batch
                        setMaxBatchReportLatency(SHUB_ACTIVE_SENSOR, batch_param.m_Latency);

                        //enable poll
                        shub_set_delay(SHUB_ACTIVE_SENSOR, delay);
                        ret = shub_activate( SHUB_ACTIVE_SENSOR, 1);

                        //disable batch
                        ret = shub_activate_logging(SHUB_ACTIVE_SENSOR, 0);
                        if(ret == -1){
                            mutex_unlock(&shub_lock);
                            return -EFAULT;
                        }

                        //start poll
                        currentActive = shub_get_current_active() & SHUB_ACTIVE_SENSOR;
                        if(ret == -1){
                            mutex_unlock(&shub_lock);
                            return -EFAULT;
                        }
                        shub_set_sensor_poll(1);
                        mutex_unlock(&shub_lock);
                        return 0;
                    }
                }
                /* flag SENSORS_BATCH_DRY_RUN is OFF */
                batch_param.m_Flasg    = param.m_Flasg;
                batch_param.m_PeriodNs = param.m_PeriodNs;
                batch_param.m_Latency  = param.m_Latency;

                if(param.m_Latency == 0){
                    shub_set_delay(SHUB_ACTIVE_SENSOR, delay);
                    if(currentActive != 0){
                        shub_set_sensor_poll(1);
                    }
                }else{
                    shub_set_delay_logging(SHUB_ACTIVE_SENSOR, batch_param.m_PeriodNs);
                }
                setMaxBatchReportLatency(SHUB_ACTIVE_SENSOR, batch_param.m_Latency);
                mutex_unlock(&shub_lock);
            }
            break;

        case SHUBIO_GYROUNCAL_FLUSH :
            {
                DBG_GYROUNC_IO("ioctl(cmd = Flush)\n"); // SHMDS_HUB_0701_01 add
                mutex_lock(&shub_lock);
                if(power_state != 0){
                    shub_logging_flush();
                    setMaxBatchReportLatency(SHUB_ACTIVE_SENSOR, batch_param.m_Latency);
                    shub_input_sync_init(shub_idev); /* SHMDS_HUB_0602_01 mod */
// SHMDS_HUB_0309_01 mod S
//                  input_event(shub_idev, EV_SYN, SYN_REPORT, 2);
                    shub_input_first_report(shub_idev, 1); /* SHMDS_HUB_0308_01 add */
                    input_event(shub_idev, EV_SYN, SYN_REPORT, (SHUB_INPUT_META_DATA | SHUB_INPUT_GYROUNC));
// SHMDS_HUB_0309_01 mod E
                }else{
                    mutex_unlock(&shub_lock);
                    return -EFAULT;
                }
                mutex_unlock(&shub_lock);
                break;

            }
        default:
            return -ENOTTY;
    }
    return 0;
}

// SHMDS_HUB_1101_01 add S
static long shub_ioctl_wrapper(struct file *filp, unsigned int cmd, unsigned long arg)
{
    SHUB_DBG_TIME_INIT     /* SHMDS_HUB_1801_01 add */
    long ret = 0;

    shub_qos_start();
    SHUB_DBG_TIME_START    /* SHMDS_HUB_1801_01 add */
    ret = shub_ioctl(filp, cmd , arg);
    SHUB_DBG_TIME_END(cmd) /* SHMDS_HUB_1801_01 add */
    shub_qos_end();

    return ret;
}
// SHMDS_HUB_1101_01 add E

/* SHMDS_HUB_0311_01 add S */
static struct timespec shub_normal_last_ts;
static struct timespec shub_local_ts;
static struct timespec shub_get_timestamp(void)
{
    struct timespec ts;
    ktime_get_ts(&ts);
    monotonic_to_bootbased(&ts);
    return ts;
}
static int64_t shub_local_ts_ns_old = 0;		/* SHMDS_HUB_0311_02 add */
/* SHMDS_HUB_0311_01 add E */

static void shub_sensor_poll_work_func(struct work_struct *work)
{
    int32_t xyz[INDEX_SUM]= {0};
    if(currentActive != 0){
        mutex_lock(&shub_lock);
        shub_qos_start();    // SHMDS_HUB_1101_01 add
        shub_get_sensors_data(SHUB_ACTIVE_SENSOR, xyz);
/* SHMDS_HUB_0311_01 mod S */
        if((shub_get_sensor_activate_info(SHUB_SAME_NOTIFY_GYRO) == GYRO_GROUP_MASK) && (shub_get_sensor_same_delay_flg(SHUB_SAME_NOTIFY_GYRO) == 1)){
            if(shub_get_sensor_first_measure_info(SHUB_SAME_NOTIFY_GYRO) & SHUB_ACTIVE_SENSOR) {
                
                struct timespec tmp_ts;
/* SHMDS_HUB_0311_02 mod S */
                int64_t local_ts_ns;
                memcpy(&tmp_ts, &shub_local_ts, sizeof(struct timespec));
                local_ts_ns = timespec_to_ns(&tmp_ts); /* SHMDS_HUB_0325_01 mod */
                if( (shub_local_ts_ns_old < (local_ts_ns - delay * 600000)) && 
				    ((timespec_to_ns(&shub_normal_last_ts)) < (local_ts_ns - delay * 600000)) ){ /* SHMDS_HUB_0325_01 mod */
/* SHMDS_HUB_0311_02 mod E */
                    xyz[7] = tmp_ts.tv_sec;
                    xyz[8] = tmp_ts.tv_nsec;
                }
                else {
					input_flg = 0; /* SHMDS_HUB_0321_01 add */
                    shub_qos_end();      // SHMDS_HUB_1101_01 add
                    mutex_unlock(&shub_lock);
                    return ;
                }
                xyz_gyro[0] = xyz[0] - xyz[4];
                xyz_gyro[1] = xyz[1] - xyz[5];
                xyz_gyro[2] = xyz[2] - xyz[6];
                xyz_gyro[3] = xyz[3];
/* SHMDS_HUB_0321_01 add S */
				input_gyrounc[0] = xyz[0];
            	input_gyrounc[1] = xyz[1];
            	input_gyrounc[2] = xyz[2];
            	input_gyrounc[3] = xyz[3];
            	input_gyrounc[4] = xyz[4];
            	input_gyrounc[5] = xyz[5];
            	input_gyrounc[6] = xyz[6];
//                xyz_gyro[4] = xyz[7];
//                xyz_gyro[5] = xyz[8];
//                shub_input_report_gyro(xyz_gyro);
//                shub_input_report_gyro_uncal(xyz);
				input_flg = 2;
                shub_local_ts_ns_old = local_ts_ns;		/* SHMDS_HUB_0311_02 add */
            }else{
				input_flg = 0;
			}
/* SHMDS_HUB_0321_01 add E */
        }
        else {
            shub_normal_last_ts.tv_sec = xyz[7];
            shub_normal_last_ts.tv_nsec = xyz[8];
/* SHMDS_HUB_0321_01 add S */
            input_gyrounc[0] = xyz[0];
            input_gyrounc[1] = xyz[1];
            input_gyrounc[2] = xyz[2];
            input_gyrounc[3] = xyz[3];
            input_gyrounc[4] = xyz[4];
            input_gyrounc[5] = xyz[5];
            input_gyrounc[6] = xyz[6];
//            input_gyrounc[7] = xyz[7];
//            input_gyrounc[8] = xyz[8];
//          shub_input_report_gyro_uncal(xyz);
			input_flg = 1;
/* SHMDS_HUB_0321_01 add E */
            shub_local_ts_ns_old = 0;		/* SHMDS_HUB_0311_02 add */
        }
/* SHMDS_HUB_0311_01 mod E */
        shub_qos_end();      // SHMDS_HUB_1101_01 add
        mutex_unlock(&shub_lock);
    }
}

static enum hrtimer_restart shub_sensor_poll(struct hrtimer *tm)
{
    shub_local_ts = shub_get_timestamp();       /* SHMDS_HUB_0311_01 add */
    schedule_work(&sensor_poll_work);
/* SHMDS_HUB_0321_01 add S */
    if(input_flg == 1){
		input_gyrounc[7] = shub_local_ts.tv_sec;
        input_gyrounc[8] = shub_local_ts.tv_nsec;
        shub_input_report_gyro_uncal(input_gyrounc);
	}else if(input_flg == 2) {
        xyz_gyro[4] = shub_local_ts.tv_sec;
	    xyz_gyro[5] = shub_local_ts.tv_nsec;
	    input_gyrounc[7] = shub_local_ts.tv_sec;
	    input_gyrounc[8] = shub_local_ts.tv_nsec;
		shub_input_report_gyro(xyz_gyro);
	    shub_input_report_gyro_uncal(input_gyrounc);
    }else{
		DBG_GYROUNC_DATA("not report\n");
	}
/* SHMDS_HUB_0321_01 add E */
    hrtimer_forward_now(&poll_timer, ns_to_ktime((int64_t)delay * NSEC_PER_MSEC));
    return HRTIMER_RESTART;
}

void shub_suspend_gyro_uncal(void)
{
    if(currentActive != 0){
        shub_set_sensor_poll(0);
        cancel_work_sync(&sensor_poll_work);
    }
}

void shub_resume_gyro_uncal(void)
{
    if(currentActive != 0){
        shub_set_sensor_poll(1);
    }
}

void shub_input_report_gyro_uncal(int32_t *data)
{
    if(data == NULL) {
        return;
    }

    data[INDEX_X] = shub_adjust_value(GYROUNC_MIN, GYROUNC_MAX,data[INDEX_X]);
    data[INDEX_Y] = shub_adjust_value(GYROUNC_MIN, GYROUNC_MAX,data[INDEX_Y]);
    data[INDEX_Z] = shub_adjust_value(GYROUNC_MIN, GYROUNC_MAX,data[INDEX_Z]);
    data[INDEX_X_BIAS] = shub_adjust_value(GYROUNC_MIN, GYROUNC_MAX,data[INDEX_X_BIAS]);
    data[INDEX_Y_BIAS] = shub_adjust_value(GYROUNC_MIN, GYROUNC_MAX,data[INDEX_Y_BIAS]);
    data[INDEX_Z_BIAS] = shub_adjust_value(GYROUNC_MIN, GYROUNC_MAX,data[INDEX_Z_BIAS]);

// SHMDS_HUB_0701_01 add S
    DBG_GYROUNC_DATA("data X=%d, Y=%d, Z=%d, S=%d, x=%d, y=%d, z=%d, t(s)=%d, t(ns)=%d\n",
                      data[INDEX_X],data[INDEX_Y],data[INDEX_Z],data[INDEX_ACC],data[INDEX_X_BIAS],
                      data[INDEX_Y_BIAS],data[INDEX_Z_BIAS],data[INDEX_TM],data[INDEX_TMNS]);
// SHMDS_HUB_0701_01 add E

#if 1  // SHMDS_HUB_0601_01 mod S
    SHUB_INPUT_VAL_CLEAR(shub_idev, ABS_HAT1X, data[INDEX_X]); /* SHMDS_HUB_0603_01 add */ /* SHMDS_HUB_0603_02 add */
    input_report_abs(shub_idev, ABS_HAT1X, data[INDEX_X]);
    input_report_abs(shub_idev, ABS_HAT1Y, data[INDEX_Y]);
    input_report_abs(shub_idev, ABS_HAT2X, data[INDEX_Z]);
    input_report_abs(shub_idev, ABS_HAT0Y, data[INDEX_ACC]); // SHMDS_HUB_0601_03 add
    input_report_abs(shub_idev, ABS_X, data[INDEX_X_BIAS]);
    input_report_abs(shub_idev, ABS_Y, data[INDEX_Y_BIAS]);
    input_report_abs(shub_idev, ABS_Z, data[INDEX_Z_BIAS]);
    input_report_abs(shub_idev, ABS_HAT3Y, data[INDEX_TM]);
    input_report_abs(shub_idev, ABS_HAT3X, data[INDEX_TMNS]);
    shub_input_sync_init(shub_idev); /* SHMDS_HUB_0602_01 mod */
    input_event(shub_idev, EV_SYN, SYN_REPORT, SHUB_INPUT_GYROUNC);
#else
    SHUB_INPUT_VAL_CLEAR(shub_idev, ABS_RX, data[INDEX_X]); /* SHMDS_HUB_0603_01 add */ /* SHMDS_HUB_0603_02 add */
    input_report_abs(shub_idev, ABS_RX, data[INDEX_X]);
    input_report_abs(shub_idev, ABS_RY, data[INDEX_Y]);
    input_report_abs(shub_idev, ABS_RZ, data[INDEX_Z]);
    input_report_abs(shub_idev, ABS_HAT0X, data[INDEX_ACC]);
    input_report_abs(shub_idev, ABS_X, data[INDEX_X_BIAS]);
    input_report_abs(shub_idev, ABS_Y, data[INDEX_Y_BIAS]);
    input_report_abs(shub_idev, ABS_Z, data[INDEX_Z_BIAS]);
    input_report_abs(shub_idev, ABS_MISC, data[INDEX_TM]);
    input_report_abs(shub_idev, ABS_VOLUME, data[INDEX_TMNS]);
    shub_input_sync_init(shub_idev); /* SHMDS_HUB_0602_01 mod */
    input_event(shub_idev, EV_SYN, SYN_REPORT, 1);
#endif // SHMDS_HUB_0601_01 mod E
}

#if 0  // SHMDS_HUB_0601_01 del S
static void shub_set_abs_params(void)
{
    input_set_abs_params(shub_idev, ABS_MISC, 0, 0xFFFFFFFF, 0, 0);
    input_set_abs_params(shub_idev, ABS_VOLUME, 0, 0xFFFFFFFF, 0, 0);
    input_set_abs_params(shub_idev, ABS_RX, GYROUNC_MIN, GYROUNC_MAX, 0, 0);
    input_set_abs_params(shub_idev, ABS_RY, GYROUNC_MIN, GYROUNC_MAX, 0, 0);
    input_set_abs_params(shub_idev, ABS_RZ, GYROUNC_MIN, GYROUNC_MAX, 0, 0);
    input_set_abs_params(shub_idev, ABS_HAT0X, 0, 0xFF, 0, 0);
    input_set_abs_params(shub_idev, ABS_X, GYROUNC_MIN, GYROUNC_MAX, 0, 0);
    input_set_abs_params(shub_idev, ABS_Y, GYROUNC_MIN, GYROUNC_MAX, 0, 0);
    input_set_abs_params(shub_idev, ABS_Z, GYROUNC_MIN, GYROUNC_MAX, 0, 0);
    shub_set_param_first(shub_idev); /* SHMDS_HUB_0308_01 add */
}
#endif // SHMDS_HUB_0601_01 del E

static void shub_set_sensor_poll(int32_t en)
{
    hrtimer_cancel(&poll_timer);
    if(en){
        input_flg = 0; /* SHMDS_HUB_0321_01 add */
        hrtimer_start(&poll_timer, ns_to_ktime((int64_t)delay * NSEC_PER_MSEC), HRTIMER_MODE_REL);
    }
}

// SHMDS_HUB_0701_05 add S
void shub_sensor_rep_input_gyro_uncal(struct seq_file *s)
{
    seq_printf(s, "[gyro_uncal]");
    seq_printf(s, "power_state=%d, ",power_state);
    seq_printf(s, "delay=%d, ",delay);
    seq_printf(s, "batch_param.m_Flasg=%d, ",batch_param.m_Flasg);
    seq_printf(s, "batch_param.m_PeriodNs=%lld, ",batch_param.m_PeriodNs);
    seq_printf(s, "batch_param.m_Latency=%lld\n",batch_param.m_Latency);
}
// SHMDS_HUB_0701_05 add E

// SHMDS_HUB_1101_01 mod S
static struct file_operations shub_fops = {
    .owner   = THIS_MODULE,
    .unlocked_ioctl = shub_ioctl_wrapper,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = shub_ioctl_wrapper,
#endif
};
// SHMDS_HUB_1101_01 mod E

static struct miscdevice shub_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = MISC_DEV_NAME,
    .fops  = &shub_fops,
};

static int32_t shub_probe_gyrounc(struct platform_device *pfdev)
{
    int32_t ret = 0;


    if(!shub_connect_check()){
        printk(KERN_INFO "shub_gyrounc Connect Error!!\n");
        ret = -ENODEV;
        goto out_driver;
    }


    pdev = platform_device_register_simple(INPUT_DEV_NAME, -1, NULL, 0);
    if (IS_ERR(pdev)) {
        ret = PTR_ERR(pdev);
        goto out_driver;
    }

#if 1  // SHMDS_HUB_0601_01 mod S
    shub_idev = shub_com_allocate_device(SHUB_INPUT_GYROUNC, &pdev->dev);
    if (!shub_idev) {
        ret = -ENOMEM;
        goto out_device;
    }
#else
    shub_idev = input_allocate_device();
    if (!shub_idev) {
        ret = -ENOMEM;
        goto out_device;
    }

    shub_idev->name = INPUT_DEV_NAME;
    shub_idev->phys = INPUT_DEV_PHYS;
    shub_idev->id.bustype = BUS_HOST;
    shub_idev->dev.parent = &pdev->dev;
    shub_idev->evbit[0] = BIT_MASK(EV_ABS);

    shub_set_abs_params();
    input_set_events_per_packet(shub_idev, SHUB_EVENTS_PER_PACKET);

    ret = input_register_device(shub_idev);
    if (ret)
        goto out_idev;
#endif // SHMDS_HUB_0601_01 mod E

    ret = misc_register(&shub_device);
    if (ret) {
        printk("shub-init: shub_io_device register failed\n");
        goto exit_misc_device_register_failed;
    }
    INIT_WORK(&sensor_poll_work, shub_sensor_poll_work_func);
    hrtimer_init(&poll_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    poll_timer.function = shub_sensor_poll;
    return 0;

exit_misc_device_register_failed:
#if 0  // SHMDS_HUB_0601_01 del S
out_idev:
    input_free_device(shub_idev);
#endif // SHMDS_HUB_0601_01 del E
out_device:
    platform_device_unregister(pdev);
out_driver:
    return ret;
}

static int32_t shub_remove_gyrounc(struct platform_device *pfdev)
{
    misc_deregister(&shub_device);
#if 1  // SHMDS_HUB_0601_01 mod S
    shub_com_unregister_device(SHUB_INPUT_GYROUNC);
#else
    input_unregister_device(shub_idev);
    input_free_device(shub_idev);
#endif // SHMDS_HUB_0601_01 mod E
    platform_device_unregister(pdev);

    cancel_work_sync(&sensor_poll_work);
    return 0;
}

static struct platform_driver shub_gyrounc_driver = {
    .probe = shub_probe_gyrounc,
    .remove = shub_remove_gyrounc,
    .shutdown = NULL,
    .driver = {
        .name = "shub_dev_gyrounc",
        .of_match_table = shub_of_match_tb_gyrounc,
    },
};

int shub_gyrounc_init(void)          /* SHMDS_HUB_3701_01 mod */
{
    int ret;
    
    ret = platform_driver_register(&shub_gyrounc_driver);

    return ret;
}

void shub_gyrounc_exit(void)         /* SHMDS_HUB_3701_01 mod */
{
    platform_driver_unregister(&shub_gyrounc_driver);
}

// late_initcall(shub_gyrounc_init); /* SHMDS_HUB_3701_01 del */
// module_exit(shub_gyrounc_exit);   /* SHMDS_HUB_3701_01 del */

MODULE_DESCRIPTION("SensorHub Input Device (Gyroscope)");
MODULE_AUTHOR("LAPIS SEMICOMDUCTOR");
MODULE_LICENSE("GPL v2");
