#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/thermal.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>


static struct kset *fc_kset;
struct task_struct *task;
int result = 0;
int data;
unsigned long temp_get;

static unsigned long temp = 0;



static ssize_t temp_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%lu\n", temp);
}
static ssize_t temp_store(struct kobject *kobj, struct kobj_attribute *attr,
                                   const char *buf, size_t count){
   sscanf(buf, "%luu", &temp);
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
static struct thermal_zone_device *tz;

int thread_function(void *data){
		int var=1;
		while(!kthread_should_stop()){
		result=thermal_zone_get_temp(tz,&temp_get);
		if(result){
			printk(KERN_INFO "error al leer temp\n");
		}
		//printk (KERN_INFO "temp_get:%lu\n",temp_get);
		temp=temp_get;
		mdelay(1000);
		schedule();
	}
	return var;
}

// Inicio del modulo
int fan_init(void)
{
	
	data=20;
	
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
	tz=thermal_zone_get_zone_by_name ("imx_thermal_zone");//obtiene el thermal_zone_device con nombre "imx_thermal_zone" que genera el 
	printk(KERN_INFO "type: %s\n",tz->type);
	printk(KERN_INFO "trips: %d\n",tz->trips);

	task=kthread_run(&thread_function,&data,"FanCtlModule");

	return 0;
}

// Salida del modulo
void fan_cleanup(void)
{
  // Destruyendo kset
  kobject_put(sensor_kobj);
  kset_unregister(fc_kset);
  printk(KERN_INFO "FanCtlModule: Se libero el modulo\n");
  kthread_stop(task);
}

// Especificando las funciones de inicio y fin del modulo
module_init(fan_init);
module_exit(fan_cleanup);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Carlos Acosta,Jesus Flores,Roberto Guerrero <joy_warmgun@hotmail.com>");
