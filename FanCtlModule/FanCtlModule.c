#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/thermal.h>

#include <linux/clk.h>
#include <linux/cpu_cooling.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/types.h>

#define REG_SET		0x4
#define REG_CLR		0x8
#define REG_TOG		0xc

#define TEMPSENSE0			0x0180
#define TEMPSENSE0_ALARM_VALUE_SHIFT	20
#define TEMPSENSE0_ALARM_VALUE_MASK	(0xfff << TEMPSENSE0_ALARM_VALUE_SHIFT)
#define TEMPSENSE0_TEMP_CNT_SHIFT	8
#define TEMPSENSE0_TEMP_CNT_MASK	(0xfff << TEMPSENSE0_TEMP_CNT_SHIFT)
#define TEMPSENSE0_FINISHED		(1 << 2)
#define TEMPSENSE0_MEASURE_TEMP		(1 << 1)
#define TEMPSENSE0_POWER_DOWN		(1 << 0)

static struct kset *fc_kset;

struct thermal_zone_device *tz;
struct imx_thermal_data {
	struct thermal_zone_device *tz;
	struct thermal_cooling_device *cdev;
	enum thermal_device_mode mode;
	struct regmap *tempmon;
	u32 c1, c2; /* See formula in imx_get_sensor_data() */
	int temp_passive;
	int temp_critical;
	int temp_max;
	int alarm_temp;
	int last_temp;
	bool irq_enabled;
	int irq;
	struct clk *thermal_clk;
	const struct thermal_soc_data *socdata;
	const char *temp_grade;
};
static int imx_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct imx_thermal_data *data = tz->devdata;
	struct regmap *map = data->tempmon;
	unsigned int n_meas;
	bool wait;
	u32 val;

	if (data->mode == THERMAL_DEVICE_ENABLED) {
		/* Check if a measurement is currently in progress */
		regmap_read(map, TEMPSENSE0, &val);
		wait = !(val & TEMPSENSE0_FINISHED);
	} else {
		/*
		 * Every time we measure the temperature, we will power on the
		 * temperature sensor, enable measurements, take a reading,
		 * disable measurements, power off the temperature sensor.
		 */
		regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_POWER_DOWN);
		regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_MEASURE_TEMP);

		wait = true;
	}

	/*
	 * According to the temp sensor designers, it may require up to ~17us
	 * to complete a measurement.
	 */
	if (wait)
		usleep_range(20, 50);

	regmap_read(map, TEMPSENSE0, &val);

	if (data->mode != THERMAL_DEVICE_ENABLED) {
		regmap_write(map, TEMPSENSE0 + REG_CLR, TEMPSENSE0_MEASURE_TEMP);
		regmap_write(map, TEMPSENSE0 + REG_SET, TEMPSENSE0_POWER_DOWN);
	}

	if ((val & TEMPSENSE0_FINISHED) == 0) {
		dev_dbg(&tz->device, "temp measurement never finished\n");
		return -EAGAIN;
	}

	n_meas = (val & TEMPSENSE0_TEMP_CNT_MASK) >> TEMPSENSE0_TEMP_CNT_SHIFT;

	/* See imx_get_sensor_data() for formula derivation */
	*temp = data->c2 - n_meas * data->c1;

	/* Update alarm value to next higher trip point for TEMPMON_IMX6Q */
	/*if (data->socdata->version == TEMPMON_IMX6Q) {
		if (data->alarm_temp == data->temp_passive &&
			*temp >= data->temp_passive)
			imx_set_alarm_temp(data, data->temp_critical);
		if (data->alarm_temp == data->temp_critical &&
			*temp < data->temp_passive) {
			imx_set_alarm_temp(data, data->temp_passive);
			dev_dbg(&tz->device, "thermal alarm off: T < %d\n",
				data->alarm_temp / 1000);
		}
	}*/

	if (*temp != data->last_temp) {
		dev_dbg(&tz->device, "millicelsius: %d\n", *temp);
		data->last_temp = *temp;
	}

	/* Reenable alarm IRQ if temperature below alarm temperature */
	/*if (!data->irq_enabled && *temp < data->alarm_temp) {
		data->irq_enabled = true;
		enable_irq(data->irq);
	}*/

	return 0;
}

static int temp = 0;

static ssize_t temp_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", temp);
}
static ssize_t temp_store(struct kobject *kobj, struct kobj_attribute *attr,
                                   const char *buf, size_t count){
   sscanf(buf, "%du", &temp);
   return count;
}

static struct kobj_attribute count_attr = __ATTR(temp, 0666, temp_show, temp_store);

static struct attribute *fc_attrs[] = {
      &count_attr.attr,                 
      NULL,
};

static struct attribute_group attr_group = {
      .attrs = fc_attrs,                ///< The attributes array defined just above
};

static struct kobject *sensor_kobj;



// Inicio del modulo
int fan_init(void)
{
	int result = 0;

	printk(KERN_INFO "FanCtlModule: Se cargo el modulo\n");

	// Creando kset para FanController en /sys/fan_controller
       	fc_kset = kset_create_and_add("fan_controller", NULL, NULL);
	if (!fc_kset)
		return -ENOMEM;

	sensor_kobj = kobject_create_and_add("sensor", &fc_kset->kobj); // kernel_kobj points to /sys/kernel
	if(!sensor_kobj){
		printk(KERN_ALERT "EBB Button: failed to create kobject mapping\n");
	return -ENOMEM;
	}
        
	result = sysfs_create_group(sensor_kobj, &attr_group);
	if(result) {
		printk(KERN_ALERT "FanCtl: failed to create sysfs group\n");
		kobject_put(sensor_kobj);// clean up -- remove the kobject sysfs entry
	return result;
   	}
	imx_get_temp(tz, &temp);
	

	return 0;
}

// Salida del modulo
void fan_cleanup(void)
{
  // Destruyendo kset
  kobject_put(sensor_kobj);
  kset_unregister(fc_kset);
  printk(KERN_INFO "FanCtlModule: Se libero el modulo\n");
}

// Especificando las funciones de inicio y fin del modulo
module_init(fan_init);
module_exit(fan_cleanup);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Carlos Acosta,Jesus Flores,Roberto Guerrero <joy_warmgun@hotmail.com>");
