/*
 * FPC1020 Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the FPC fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, controlling GPIOs such as sensor reset
 * line, sensor IRQ line.
 *
 * The driver will expose most of its available functionality in sysfs which
 * enables dynamic control of these features from eg. a user space process.
 *
 * The sensor's IRQ events will be pushed to Kernel's event handling system and
 * are exposed in the drivers event node.
 *
 * This driver will NOT send any commands to the sensor it only controls the
 * electrical parts.
 *
 *
 * Copyright (c) 2015 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#ifndef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
#include <linux/wakelock.h>
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
#include <linux/ktime.h>
#include <linux/device.h>

#ifdef CONFIG_SHARP_SHTERM
#include "misc/shterm_k.h"
#endif /* CONFIG_SHARP_SHTERM */
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */

#define FPC_TTW_HOLD_TIME 1000

#define RESET_LOW_SLEEP_MIN_US 5000
#define RESET_LOW_SLEEP_MAX_US (RESET_LOW_SLEEP_MIN_US + 100)
#define RESET_HIGH_SLEEP1_MIN_US 100
#define RESET_HIGH_SLEEP1_MAX_US (RESET_HIGH_SLEEP1_MIN_US + 100)
#define RESET_HIGH_SLEEP2_MIN_US 5000
#define RESET_HIGH_SLEEP2_MAX_US (RESET_HIGH_SLEEP2_MIN_US + 100)
#define PWR_ON_SLEEP_MIN_US 100
#define PWR_ON_SLEEP_MAX_US (PWR_ON_SLEEP_MIN_US + 900)

#define NUM_PARAMS_REG_ENABLE_SET 2

#define RELEASE_WAKELOCK_W_V "release_wakelock_with_verification"
#define RELEASE_WAKELOCK "release_wakelock"
#define START_IRQS_RECEIVED_CNT "start_irqs_received_counter"

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
static int sh_debug_fpsensor = 0;
module_param(sh_debug_fpsensor, int, 0664);


enum {
	WAKE_LOCK_SUSPEND
};

struct wake_lock {
	struct wakeup_source ws;
};

static inline void wake_lock_init(struct wake_lock *lock, int type,
				  const char *name)
{
	wakeup_source_init(&lock->ws, name);
}

static inline void wake_lock_destroy(struct wake_lock *lock)
{
	wakeup_source_trash(&lock->ws);
}

static inline void wake_lock(struct wake_lock *lock)
{
	__pm_stay_awake(&lock->ws);
}

static inline void wake_lock_timeout(struct wake_lock *lock, long timeout)
{
	__pm_wakeup_event(&lock->ws, jiffies_to_msecs(timeout));
}

static inline void wake_unlock(struct wake_lock *lock)
{
	__pm_relax(&lock->ws);
}

static inline int wake_lock_active(struct wake_lock *lock)
{
	return lock->ws.active;
}

#ifdef dev_info
#undef dev_info
#endif

#define dev_info(dev, fmt, ...) \
    if (sh_debug_fpsensor) {\
            dev_printk(KERN_INFO, dev, fmt, ##__VA_ARGS__);\
    }


#ifdef dev_dbg
#undef dev_dbg
#endif

#define dev_dbg(dev, fmt, ...) \
    if (sh_debug_fpsensor) {\
            dev_printk(KERN_DEBUG, dev, fmt, ##__VA_ARGS__);\
    }
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */

static const char * const pctl_names[] = {
	"fpc1020_reset_reset",
	"fpc1020_reset_active",
	"fpc1020_irq_active",
};

struct vreg_config {
	char *name;
	unsigned long vmin;
	unsigned long vmax;
	int ua_load;
};

static const struct vreg_config const vreg_conf[] = {
#ifndef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	{ "vdd_ana", 1800000UL, 1800000UL, 6000, },
	{ "vcc_spi", 1800000UL, 1800000UL, 10, },
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	{ "vdd_io", 1800000UL, 1800000UL, 6000, },
};

struct fpc1020_data {
	struct device *dev;

	struct pinctrl *fingerprint_pinctrl;
	struct pinctrl_state *pinctrl_state[ARRAY_SIZE(pctl_names)];
	struct regulator *vreg[ARRAY_SIZE(vreg_conf)];

	struct wake_lock ttw_wl;
	int irq_gpio;
	int rst_gpio;

	int nbr_irqs_received;
	int nbr_irqs_received_counter_start;

	struct mutex lock; /* To set/get exported values in sysfs */
	bool prepared;
	atomic_t wakeup_enabled; /* Used both in ISR and non-ISR */
#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	int irqcounter_enable;
#endif
};

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
static int sh_fpsensor_irqcount = 0;
#endif

static int vreg_setup(struct fpc1020_data *fpc1020, const char *name,
	bool enable)
{
	size_t i;
	int rc;
	struct regulator *vreg;
	struct device *dev = fpc1020->dev;

	for (i = 0; i < ARRAY_SIZE(fpc1020->vreg); i++) {
		const char *n = vreg_conf[i].name;

		if (!strncmp(n, name, strlen(n)))
			goto found;
	}

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	dev_dbg(dev, "Regulator %s not found\n", name);
	return 0;
#else /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	dev_err(dev, "Regulator %s not found\n", name);
	return -EINVAL;
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */

found:
#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	dev_dbg(dev, "[S]%s name=%s enable=%d\n", __func__, name, enable);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	vreg = fpc1020->vreg[i];
	if (enable) {
		if (!vreg) {
			vreg = regulator_get(dev, name);
			if (IS_ERR(vreg)) {
				dev_err(dev, "Unable to get %s\n", name);
				return PTR_ERR(vreg);
			}
		}

		if (regulator_count_voltages(vreg) > 0) {
			rc = regulator_set_voltage(vreg, vreg_conf[i].vmin,
					vreg_conf[i].vmax);
			if (rc)
				dev_err(dev,
					"Unable to set voltage on %s, %d\n",
					name, rc);
		}

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
		rc = regulator_set_load(vreg, vreg_conf[i].ua_load);
#else
		rc = regulator_set_optimum_mode(vreg, vreg_conf[i].ua_load);
#endif
		if (rc < 0)
			dev_err(dev, "Unable to set current on %s, %d\n",
					name, rc);

		rc = regulator_enable(vreg);
		if (rc) {
			dev_err(dev, "error enabling %s: %d\n", name, rc);
			regulator_put(vreg);
			vreg = NULL;
		}
		fpc1020->vreg[i] = vreg;
#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
		dev_dbg(dev, "enable %s\n", name);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	} else {
		if (vreg) {
			if (regulator_is_enabled(vreg)) {
				regulator_disable(vreg);
				dev_dbg(dev, "disabled %s\n", name);
			}
			regulator_put(vreg);
			fpc1020->vreg[i] = NULL;
		}
		rc = 0;
	}
#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
#ifdef CONFIG_SHARP_SHTERM
	if (fpc1020->vreg[i] != NULL) {
		shterm_k_set_info( SHTERM_INFO_FINGER_AUTH, 1);
	} else {
		shterm_k_set_info( SHTERM_INFO_FINGER_AUTH, 0);
	}
#endif
#endif

	return rc;
}

/**
 * sysfs node for controlling clocks.
 *
 * This is disabled in platform variant of this driver but kept for
 * backwards compatibility. Only prints a debug print that it is
 * disabled.
 */
static ssize_t clk_enable_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	dev_dbg(dev,
		"clk_enable sysfs node not enabled in platform driver\n");

	return count;
}
static DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);

/**
 * Will try to select the set of pins (GPIOS) defined in a pin control node of
 * the device tree named @p name.
 *
 * The node can contain several eg. GPIOs that is controlled when selecting it.
 * The node may activate or deactivate the pins it contains, the action is
 * defined in the device tree node itself and not here. The states used
 * internally is fetched at probe time.
 *
 * @see pctl_names
 * @see fpc1020_probe
 */
static int select_pin_ctl(struct fpc1020_data *fpc1020, const char *name)
{
	size_t i;
	int rc;
	struct device *dev = fpc1020->dev;

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	dev_dbg(dev, "[S]%s name=%s\n", __func__, name);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	for (i = 0; i < ARRAY_SIZE(fpc1020->pinctrl_state); i++) {
		const char *n = pctl_names[i];

		if (!strncmp(n, name, strlen(n))) {
			rc = pinctrl_select_state(fpc1020->fingerprint_pinctrl,
					fpc1020->pinctrl_state[i]);
			if (rc)
				dev_err(dev, "cannot select '%s'\n", name);
			else
				dev_dbg(dev, "Selected '%s'\n", name);
			goto exit;
		}
	}

	rc = -EINVAL;
	dev_err(dev, "%s:'%s' not found\n", __func__, name);

exit:
	return rc;
}

static ssize_t pinctl_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	int rc;

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	dev_dbg(dev, "[S]%s count=%d\n", __func__, (int)count);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	mutex_lock(&fpc1020->lock);
	rc = select_pin_ctl(fpc1020, buf);
	mutex_unlock(&fpc1020->lock);

	return rc ? rc : count;
}
static DEVICE_ATTR(pinctl_set, S_IWUSR, NULL, pinctl_set);

static ssize_t regulator_enable_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	char op;
	char name[16];
	int rc;
	bool enable;

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	dev_dbg(fpc1020->dev, "[S]%s count=%d\n", __func__, (int)count);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */

	if (NUM_PARAMS_REG_ENABLE_SET != sscanf(buf, "%15[^,],%c", name, &op))
		return -EINVAL;
	if (op == 'e')
		enable = true;
	else if (op == 'd')
		enable = false;
	else
		return -EINVAL;

	mutex_lock(&fpc1020->lock);
	rc = vreg_setup(fpc1020, name, enable);
	mutex_unlock(&fpc1020->lock);

	return rc ? rc : count;
}
static DEVICE_ATTR(regulator_enable, S_IWUSR, NULL, regulator_enable_set);

static int hw_reset(struct fpc1020_data *fpc1020)
{
	int irq_gpio;
	struct device *dev = fpc1020->dev;
	int rc = select_pin_ctl(fpc1020, "fpc1020_reset_active");

	if (rc)
		goto exit;
	usleep_range(RESET_HIGH_SLEEP1_MIN_US, RESET_HIGH_SLEEP1_MAX_US);

	rc = select_pin_ctl(fpc1020, "fpc1020_reset_reset");
	if (rc)
		goto exit;
	usleep_range(RESET_LOW_SLEEP_MIN_US, RESET_LOW_SLEEP_MAX_US);

	rc = select_pin_ctl(fpc1020, "fpc1020_reset_active");
	if (rc)
		goto exit;
	usleep_range(RESET_HIGH_SLEEP2_MIN_US, RESET_HIGH_SLEEP2_MAX_US);

	irq_gpio = gpio_get_value(fpc1020->irq_gpio);
	dev_info(dev, "IRQ after reset %d\n", irq_gpio);

exit:
	return rc;
}

static ssize_t hw_reset_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	dev_dbg(dev, "[S]%s name=%s count=%d\n", __func__, buf, (int)count);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	if (!strncmp(buf, "reset", strlen("reset"))) {
		mutex_lock(&fpc1020->lock);
		rc = hw_reset(fpc1020);
		mutex_unlock(&fpc1020->lock);
	} else {
#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
		dev_dbg(dev, "[E]%s name=%s not found\n", __func__, buf);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
		return -EINVAL;
	}

	return rc ? rc : count;
}
static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, hw_reset_set);

/**
 * Will setup GPIOs, and regulators to correctly initialize the touch sensor to
 * be ready for work.
 *
 * In the correct order according to the sensor spec this function will
 * enable/disable regulators, and reset line, all to set the sensor in a
 * correct power on or off state "electrical" wise.
 *
 * @see  device_prepare_set
 * @note This function will not send any commands to the sensor it will only
 *       control it "electrically".
 */
static int device_prepare(struct fpc1020_data *fpc1020, bool enable)
{
	int rc;

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	dev_dbg(fpc1020->dev, "[S]%s enable=%d\n", __func__, enable);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	mutex_lock(&fpc1020->lock);
	if (enable && !fpc1020->prepared) {
		fpc1020->prepared = true;
		select_pin_ctl(fpc1020, "fpc1020_reset_reset");

		rc = vreg_setup(fpc1020, "vcc_spi", true);
		if (rc)
			goto exit;

		rc = vreg_setup(fpc1020, "vdd_io", true);
		if (rc)
			goto exit_1;

		rc = vreg_setup(fpc1020, "vdd_ana", true);
		if (rc)
			goto exit_2;

		usleep_range(PWR_ON_SLEEP_MIN_US, PWR_ON_SLEEP_MAX_US);

		/* As we can't control chip select here the other part of the
		 * sensor driver eg. the TEE driver needs to do a _SOFT_ reset
		 * on the sensor after power up to be sure that the sensor is
		 * in a good state after power up. Okeyed by ASIC. */

		(void)select_pin_ctl(fpc1020, "fpc1020_reset_active");
	} else if (!enable && fpc1020->prepared) {
		rc = 0;
		(void)select_pin_ctl(fpc1020, "fpc1020_reset_reset");

		usleep_range(PWR_ON_SLEEP_MIN_US, PWR_ON_SLEEP_MAX_US);

		(void)vreg_setup(fpc1020, "vdd_ana", false);
exit_2:
		(void)vreg_setup(fpc1020, "vdd_io", false);
exit_1:
		(void)vreg_setup(fpc1020, "vcc_spi", false);
exit:
		fpc1020->prepared = false;
	} else {
		rc = 0;
	}
	mutex_unlock(&fpc1020->lock);

	return rc;
}

/**
 * sysfs node to enable/disable (power up/power down) the touch sensor
 *
 * @see device_prepare
 */
static ssize_t device_prepare_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	dev_dbg(fpc1020->dev, "[S]%s count=%d\n", __func__, (int)count);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	if (!strncmp(buf, "enable", strlen("enable")))
		rc = device_prepare(fpc1020, true);
	else if (!strncmp(buf, "disable", strlen("disable")))
		rc = device_prepare(fpc1020, false);
	else
#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	{
		dev_dbg(fpc1020->dev, "[E]%s -EINVAL\n", __func__);
		return -EINVAL;
	}
#else /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
		return -EINVAL;
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */

	return rc ? rc : count;
}
static DEVICE_ATTR(device_prepare, S_IWUSR, NULL, device_prepare_set);

/**
 * sysfs node for controlling whether the driver is allowed
 * to wake up the platform on interrupt.
 */
static ssize_t wakeup_enable_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	ssize_t ret = count;

	mutex_lock(&fpc1020->lock);
	if (!strncmp(buf, "enable", strlen("enable")))
		atomic_set(&fpc1020->wakeup_enabled, 1);
	else if (!strncmp(buf, "disable", strlen("disable")))
		atomic_set(&fpc1020->wakeup_enabled, 0);
	else
		ret = -EINVAL;
	mutex_unlock(&fpc1020->lock);

	return ret;
}
static DEVICE_ATTR(wakeup_enable, S_IWUSR, NULL, wakeup_enable_set);

/**
 * sysfs node for controlling the wakelock.
 */
static ssize_t handle_wakelock_cmd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct  fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	ssize_t ret = count;

	mutex_lock(&fpc1020->lock);
	if (!strncmp(buf, RELEASE_WAKELOCK_W_V,
		min(count, strlen(RELEASE_WAKELOCK_W_V)))) {
		if (fpc1020->nbr_irqs_received_counter_start ==
				fpc1020->nbr_irqs_received) {
			wake_unlock(&fpc1020->ttw_wl);
		} else {
			dev_dbg(dev, "Ignore releasing of wakelock %d != %d",
				fpc1020->nbr_irqs_received_counter_start,
				fpc1020->nbr_irqs_received);
		}
	} else if (!strncmp(buf, RELEASE_WAKELOCK, min(count,
				strlen(RELEASE_WAKELOCK)))) {
		wake_unlock(&fpc1020->ttw_wl);
	} else if (!strncmp(buf, START_IRQS_RECEIVED_CNT,
			min(count, strlen(START_IRQS_RECEIVED_CNT)))) {
		fpc1020->nbr_irqs_received_counter_start =
		fpc1020->nbr_irqs_received;
	} else
		ret = -EINVAL;
	mutex_unlock(&fpc1020->lock);

	return ret;
}
static DEVICE_ATTR(handle_wakelock, S_IWUSR, NULL, handle_wakelock_cmd);

/**
 * sysf node to check the interrupt status of the sensor, the interrupt
 * handler should perform sysf_notify to allow userland to poll the node.
 */
static ssize_t irq_get(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	int irq = gpio_get_value(fpc1020->irq_gpio);

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	ssize_t ret = 0;
	dev_dbg(fpc1020->dev, "[S]%s\n", __func__);
	ret =scnprintf(buf, PAGE_SIZE, "%i\n", irq);
	return ret;
#else /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	return scnprintf(buf, PAGE_SIZE, "%i\n", irq);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
}

/**
 * writing to the irq node will just drop a printk message
 * and return success, used for latency measurement.
 */
static ssize_t irq_ack(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

	dev_dbg(fpc1020->dev, "%s\n", __func__);

	return count;
}
static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
static ssize_t sh_irqcounter_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	ssize_t ret = count;

	dev_dbg(fpc1020->dev, "[S]%s count=%d\n", __func__, (int)count);
	mutex_lock(&fpc1020->lock);
	if (!strncmp(buf, "enable", strlen("enable"))) {
		dev_dbg(fpc1020->dev, "irq enable\n");
		fpc1020->irqcounter_enable = 1;
		sh_fpsensor_irqcount = 0;
	}
	else if (!strncmp(buf, "disable", strlen("disable"))) {
		dev_dbg(fpc1020->dev, "irq disable\n");
		fpc1020->irqcounter_enable = 0;
		sh_fpsensor_irqcount = 0;
	}
	else {
		ret = -EINVAL;
	}
	mutex_unlock(&fpc1020->lock);

	return ret;
}
static DEVICE_ATTR(irqcounter_enable, S_IWUSR, NULL, sh_irqcounter_enable);

static ssize_t sh_irqcount_get(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	if(sh_fpsensor_irqcount)
		return scnprintf(buf, PAGE_SIZE, "\n\t(2/2)Fingerprint IRQ Test OK! irq_count = %d\n", sh_fpsensor_irqcount);

	return scnprintf(buf, PAGE_SIZE, "\n\t(2/2)Fingerprint IRQ Test NG! irq_count = %d\n", sh_fpsensor_irqcount);
#else /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	return scnprintf(buf, PAGE_SIZE, "%d\n", shfp_irqcount);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
}
static DEVICE_ATTR(irqcount, S_IRUSR, sh_irqcount_get, NULL);


static ssize_t sh_is_power_enabled(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", fpc1020->prepared);
}
static DEVICE_ATTR(is_power_enabled, S_IRUSR, sh_is_power_enabled, NULL);


#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */

static struct attribute *attributes[] = {
	&dev_attr_pinctl_set.attr,
	&dev_attr_device_prepare.attr,
	&dev_attr_regulator_enable.attr,
	&dev_attr_hw_reset.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_handle_wakelock.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_irq.attr,
#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	&dev_attr_irqcounter_enable.attr,
	&dev_attr_irqcount.attr,
	&dev_attr_is_power_enabled.attr,
#endif
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};

static irqreturn_t fpc1020_irq_handler(int irq, void *handle)
{
	struct fpc1020_data *fpc1020 = handle;

	dev_dbg(fpc1020->dev, "%s\n", __func__);

	mutex_lock(&fpc1020->lock);
	if (atomic_read(&fpc1020->wakeup_enabled)) {
		fpc1020->nbr_irqs_received++;
		wake_lock_timeout(&fpc1020->ttw_wl,
					msecs_to_jiffies(FPC_TTW_HOLD_TIME));
	}
	mutex_unlock(&fpc1020->lock);

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	if (fpc1020->irqcounter_enable) {
		dev_dbg(fpc1020->dev, "irq count up\n");
		sh_fpsensor_irqcount++;
	}
#endif

	sysfs_notify(&fpc1020->dev->kobj, NULL, dev_attr_irq.attr.name);

	return IRQ_HANDLED;
}

static int fpc1020_request_named_gpio(struct fpc1020_data *fpc1020,
	const char *label, int *gpio)
{
	struct device *dev = fpc1020->dev;
	struct device_node *np = dev->of_node;
	int rc = of_get_named_gpio(np, label, 0);

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	dev_dbg(dev, "[S]%s label=%s gpio=%d\n", __func__, label, *gpio);
#endif /* CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE */
	if (rc < 0) {
		dev_err(dev, "failed to get '%s'\n", label);
		return rc;
	}
	*gpio = rc;

	rc = devm_gpio_request(dev, *gpio, label);
	if (rc) {
		dev_err(dev, "failed to request gpio %d\n", *gpio);
		return rc;
	}
	dev_dbg(dev, "%s %d\n", label, *gpio);

	return 0;
}

static int fpc1020_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc = 0;
	size_t i;
	int irqf;
	struct device_node *np = dev->of_node;
	struct fpc1020_data *fpc1020 = devm_kzalloc(dev, sizeof(*fpc1020),
			GFP_KERNEL);

	dev_info(dev, "%s start\n", __func__);

	if (!fpc1020) {
		dev_err(dev,
			"failed to allocate memory for struct fpc1020_data\n");
		rc = -ENOMEM;
		goto exit;
	}

	fpc1020->dev = dev;
	platform_set_drvdata(pdev, fpc1020);

	if (!np) {
		dev_err(dev, "no of node found\n");
		rc = -EINVAL;
		goto exit;
	}

	rc = fpc1020_request_named_gpio(fpc1020, "fpc,gpio_irq",
			&fpc1020->irq_gpio);
	if (rc)
		goto exit;
	rc = fpc1020_request_named_gpio(fpc1020, "fpc,gpio_rst",
			&fpc1020->rst_gpio);
	if (rc)
		goto exit;

	fpc1020->fingerprint_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(fpc1020->fingerprint_pinctrl)) {
		if (PTR_ERR(fpc1020->fingerprint_pinctrl) == -EPROBE_DEFER) {
			dev_info(dev, "pinctrl not ready\n");
			rc = -EPROBE_DEFER;
			goto exit;
		}
		dev_err(dev, "Target does not use pinctrl\n");
		fpc1020->fingerprint_pinctrl = NULL;
		rc = -EINVAL;
		goto exit;
	}

	for (i = 0; i < ARRAY_SIZE(fpc1020->pinctrl_state); i++) {
		const char *n = pctl_names[i];
		struct pinctrl_state *state =
			pinctrl_lookup_state(fpc1020->fingerprint_pinctrl, n);
		if (IS_ERR(state)) {
			dev_err(dev, "cannot find '%s'\n", n);
			rc = -EINVAL;
			goto exit;
		}
		dev_info(dev, "found pin control %s\n", n);
		fpc1020->pinctrl_state[i] = state;
	}

	rc = select_pin_ctl(fpc1020, "fpc1020_reset_reset");
	if (rc)
		goto exit;
	rc = select_pin_ctl(fpc1020, "fpc1020_irq_active");
	if (rc)
		goto exit;

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	atomic_set(&fpc1020->wakeup_enabled, 1);
#else
	atomic_set(&fpc1020->wakeup_enabled, 0);
#endif

#ifdef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE
	fpc1020->irqcounter_enable = 0;
#endif

	irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
	if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
		irqf |= IRQF_NO_SUSPEND;
		device_init_wakeup(dev, 1);
	}

	mutex_init(&fpc1020->lock);
	rc = devm_request_threaded_irq(dev, gpio_to_irq(fpc1020->irq_gpio),
			NULL, fpc1020_irq_handler, irqf,
			dev_name(dev), fpc1020);
	if (rc) {
		dev_err(dev, "could not request irq %d\n",
				gpio_to_irq(fpc1020->irq_gpio));
		goto exit;
	}

	dev_dbg(dev, "requested irq %d\n", gpio_to_irq(fpc1020->irq_gpio));

	/* Request that the interrupt should be wakeable */
	enable_irq_wake(gpio_to_irq(fpc1020->irq_gpio));

	wake_lock_init(&fpc1020->ttw_wl, WAKE_LOCK_SUSPEND, "fpc_ttw_wl");

	rc = sysfs_create_group(&dev->kobj, &attribute_group);
	if (rc) {
		dev_err(dev, "could not create sysfs\n");
		goto exit;
	}

	if (of_property_read_bool(dev->of_node, "fpc,enable-on-boot")) {
		dev_info(dev, "Enabling hardware\n");
		(void)device_prepare(fpc1020, true);
	}

#ifndef CONFIG_SENSORS_FPRINT_SHARP_CUSTOMIZE // Delete of midpoint potential.
	rc = hw_reset(fpc1020);
#endif

	dev_info(dev, "%s: ok\n", __func__);

exit:
	dev_info(dev, "%s end %d\n", __func__, rc);
	return rc;
}

static int fpc1020_remove(struct platform_device *pdev)
{
	struct fpc1020_data *fpc1020 = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &attribute_group);
	mutex_destroy(&fpc1020->lock);
	wake_lock_destroy(&fpc1020->ttw_wl);
	(void)vreg_setup(fpc1020, "vdd_ana", false);
	(void)vreg_setup(fpc1020, "vdd_io", false);
	(void)vreg_setup(fpc1020, "vcc_spi", false);
	dev_info(&pdev->dev, "%s\n", __func__);

	return 0;
}

static struct of_device_id fpc1020_of_match[] = {
	{ .compatible = "fpc,fpc1020", },
	{}
};
MODULE_DEVICE_TABLE(of, fpc1020_of_match);

static struct platform_driver fpc1020_driver = {
	.driver = {
		.name	= "fpc1020",
		.owner	= THIS_MODULE,
		.of_match_table = fpc1020_of_match,
	},
	.probe	= fpc1020_probe,
	.remove	= fpc1020_remove,
};

static int __init fpc1020_init(void)
{
	int rc = platform_driver_register(&fpc1020_driver);

	if (!rc)
		pr_info("%s OK\n", __func__);
	else
		pr_err("%s %d\n", __func__, rc);

	return rc;
}

static void __exit fpc1020_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&fpc1020_driver);
}

module_init(fpc1020_init);
module_exit(fpc1020_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Aleksej Makarov");
MODULE_AUTHOR("Henrik Tillman <henrik.tillman@fingerprints.com>");
MODULE_DESCRIPTION("FPC1020 Fingerprint sensor device driver.");
