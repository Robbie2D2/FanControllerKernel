#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/thermal.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#define MSECS 500 //Numero de milisegundos para dormir el hilo del kernel 

//Estructura de kset
static struct kset *fc_kset;
//Estructura de tarea
struct task_struct *task;
//Estructura de kobject
static struct kobject *sensor_kobj;
//Archivo dentro del kobject
static unsigned long temp = 0;
//Estructura de thermal_zone
static struct thermal_zone_device *tz;
//variable temporal para almacenar temperatura
unsigned long temp_get;

int result = 0;
int data;
//Funcion para leer el archivo
static ssize_t temp_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%lu\n", temp);
}
//Funcion para escribir en el archivo
static ssize_t temp_store(struct kobject *kobj, struct kobj_attribute *attr,
                                   const char *buf, size_t count){
   sscanf(buf, "%luu", &temp);
   return count;
}
//Atributos del archivo
static struct kobj_attribute count_attr = __ATTR(temp, 0666, temp_show, temp_store);
//Arreglo de atributos
static struct attribute *fc_attrs[] = {
      &count_attr.attr,                 
      NULL,
};
//Estructura con el grupo de atributos
static struct attribute_group attr_group = {
      .attrs = fc_attrs,                ///< The attributes array defined just above
};


//Funcion del hilo del kernel
int thread_function(void *data){
		int var=1;
		while(!kthread_should_stop()){
		result=thermal_zone_get_temp(tz,&temp_get);
		if(result){
			printk(KERN_INFO "error al leer temp\n");
		}
		printk (KERN_INFO "temp_get:%lu\n",temp_get);
		temp=temp_get;
		//mdelay(1000);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(MSECS);
	}
	return var;
}

//Inicio del modulo
int fan_init(void)
{
	
	data=20;
	
	printk(KERN_INFO "FanCtlModule: Se cargo el modulo\n");

	//Creando kset para FanController en /sys/fan_controller
       	fc_kset = kset_create_and_add("fan_controller", NULL, NULL);
	if (!fc_kset)
		return -ENOMEM;
	//Creando kobject dentro de kset
	sensor_kobj = kobject_create_and_add("sensor", &fc_kset->kobj);
	if(!sensor_kobj){
		printk(KERN_ALERT "FanCtlModule: failed to create kobject mapping\n");
	return -ENOMEM;
	}
    //Crendo archivo dentro de kobject
	result = sysfs_create_group(sensor_kobj, &attr_group);
	if(result) {
		printk(KERN_ALERT "FanCtl: failed to create sysfs group\n");
		kobject_put(sensor_kobj);// clean up -- remove the kobject sysfs entry
	return result;
   	}
	//Funcion para obtener estructura de thermal_zone buscando de la lista de zonas por el nombre de "imx_thermal_zone"
	tz=thermal_zone_get_zone_by_name ("imx_thermal_zone");
	printk(KERN_INFO "type: %s\n",tz->type);
	printk(KERN_INFO "trips: %d\n",tz->trips);
	//Crea y agrega el hilo de kernel a la lista de procesos
	task=kthread_run(&thread_function,&data,"FanCtlModule");

	return 0;
}

// Salida del modulo
void fan_cleanup(void)
{
  	//Destruye kobject decrementando el contador de instancias
	kobject_put(sensor_kobj);
	//Destruyendo kset
	kset_unregister(fc_kset);
	//Detiene el hilo del kernel
	kthread_stop(task);
	printk(KERN_INFO "FanCtlModule: Se libero el modulo\n");
}

//Especificando las funciones de inicio y fin del modulo
module_init(fan_init);
module_exit(fan_cleanup);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Carlos Acosta,Jesus Flores,Roberto Guerrero <joy_warmgun@hotmail.com>");
