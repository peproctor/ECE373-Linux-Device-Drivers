/*
 * Philippe Proctor
 * 5/26/2019
 * ECE 373
 *
 * Hw 5: Blinking ethernet controller LEDs with a timer.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/jiffies.h>

#define DEVCNT 1
#define DEVNAME "blink_driver"
#define NODENAME "ece_led"
#define LED_ON 0x0E
#define LED_OFF 0x0F
#define LED_REG 0xE00

/*Start of private driver data struct, passed around inside driver*/

/*Allows user to change param w/ in driver module*/
static int blink_rate = 2;
unsigned long duration;
module_param(blink_rate, int, S_IRUSR | S_IWUSR);
char blink_driver_name[] = DEVNAME;

/*Start of private driver data struct, passed around inside driver*/
static struct mydev_dev {
        struct cdev cdev;
	struct class *my_class;
        int syscall_val;
        long led_initial_val;
	long led_reg;
	dev_t my_node;

} mydev;


static struct myPci {
	struct pci_dev *pdev;
	void *hw_addr;

} myPci;

//Timer struct 
static struct timer_list my_timer;

//Timer callback func
void timer_blink(struct timer_list *list) {

	/*LED register value */
	mydev.led_reg = ioread32(myPci.hw_addr + LED_REG);
	
	if(mydev.led_reg == LED_ON) {
		//Turn off LED0
		iowrite32(LED_OFF, (myPci.hw_addr + LED_REG));
	}
	else {	
		//Turn on LED0
		iowrite32(LED_ON, (myPci.hw_addr + LED_REG));
	}


	/*Timer control*/ 
	if(blink_rate < 0) {
		printk_ratelimited(KERN_ERR "EINVAL\n");
		mod_timer(&my_timer, jiffies + msecs_to_jiffies(duration));
	}
	else if(blink_rate == 0) {
		mod_timer(&my_timer, jiffies + msecs_to_jiffies(duration));
	}
	else {
		duration = (2*1000) / blink_rate;
		mod_timer(&my_timer, jiffies + msecs_to_jiffies(duration));
	
	} 

}

static int blink_driver_open(struct inode *inode, struct file *file){
	
	if(blink_rate <= 0) {
		blink_rate = 2;
	}	

	/*Duration calculation to get 50% duty cycle */
	duration = (2*1000) / blink_rate; 

	//Starting timer at 1 second times 50% of the specified rate
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(duration));

	//Turn on LED0
	iowrite32(LED_ON, (myPci.hw_addr + LED_REG));
	
	return 0;

}

//Read callback 
static ssize_t blink_driver_read(struct file *file, char __user *buf, size_t len, loff_t *offset){


	/*Get local kernel buffer set aside */
	int ret;
	int *output_value = &blink_rate;

	/*Safety check that offset is less than int */
	if(*offset >= sizeof(int)){
		return 0;
	}

	/*Check if device passed null value to buffer */
	if(!buf) {
		ret = -EINVAL;
		goto out;		//undo allocations
	}


	/*Copy value from driver struct to user space */
	if(copy_to_user(buf, output_value, sizeof(int))){
		ret = -EFAULT;
		goto out;
	}

	/*Update offset by */
	ret = sizeof(int);
	*offset += len;

	printk(KERN_INFO "Current blink rate: %d\n", blink_rate);

out:
	return ret;
}


static ssize_t blink_driver_write(struct file *file, const char __user *buf, size_t len, loff_t *offset ){
	
	/*Local kernel memory*/
	char *kern_buf;
	int ret;


	/*Check if user passed null value to buffer*/
	if(!buf){
		ret = -EINVAL;
		goto out;
	}

	/*Get memory to copy into...*/
	kern_buf = kmalloc(len, GFP_KERNEL);

	/*Getting value from userspace to kernel space */
	if(copy_from_user(kern_buf, buf, len)){
		ret = -EFAULT;
		goto mem_out;
	}

	ret = len;
	
	if(*kern_buf < 0){
		printk(KERN_ERR "EINVAL\n");
		
		}
	else if(*kern_buf == 0){
		printk("Blink rate set to default\n");
		
	}
	else{	
		printk("Blink rate set to %d \n", *kern_buf);
		blink_rate = *kern_buf;
	}

	printk(KERN_INFO "Userspace wrote %d to us\n", *kern_buf);


mem_out:
	kfree(kern_buf);
	
out:
	return ret;

}

static int pci_blink_driver_probe(struct pci_dev *pdev, const struct pci_device_id *ent) {
	
	resource_size_t mmio_start, mmio_len;
	unsigned long barMask;

	//Get BAR
	barMask = pci_select_bars(pdev, IORESOURCE_MEM);
	printk(KERN_INFO "barmask %lx", barMask);
	
	//Reserve BAR areas
	if(pci_request_selected_regions(pdev, barMask, blink_driver_name)){
		
		printk(KERN_ERR "request selected regions failed \n");	

		goto unregister_selected_regions;

	};
	
	mmio_start = pci_resource_start(pdev, 0);
	mmio_len = pci_resource_len(pdev,0);
	

	if(!(myPci.hw_addr = ioremap(mmio_start, mmio_len))){
		
		printk(KERN_ERR "IOREMAP failed \n");
		goto unregister_ioremap;
	};

	mydev.led_initial_val = ioread32(myPci.hw_addr + 0xE00);
	return 0;

unregister_ioremap:
	iounmap(myPci.hw_addr);

unregister_selected_regions:
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));

	return 0;
}


static void pci_blink_driver_remove(struct pci_dev *pdev){

	printk(KERN_INFO "blink driver remove PCI called\n");
	
	iounmap(myPci.hw_addr);
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));

}



static struct file_operations blink_driver_fops = {
	//fields
	.owner = THIS_MODULE,
	.open = blink_driver_open,
	.read = blink_driver_read,
	.write = blink_driver_write,

};


static const struct pci_device_id pci_blink_driver_table[] = {
	{ PCI_DEVICE(0x8086, 0x100e) },
	{},

};

static struct pci_driver pci_blink_driver = {
	.name = "blink_driver",
	.id_table = pci_blink_driver_table,
	.probe = pci_blink_driver_probe,
	.remove = pci_blink_driver_remove,
};



static int __init blink_driver_init(void) {

	printk(KERN_INFO "blink_driver loading...");

	if (alloc_chrdev_region(&mydev.my_node, 0, DEVCNT, DEVNAME)) {	
		printk(KERN_ERR "alloc_chrdev_region() failed!\n");
		goto exit;
	}

	/*Setup the timer*/
	timer_setup(&my_timer, timer_blink, 0);

	/*Initialize the char device and add it to the kernel*/
	cdev_init(&mydev.cdev, &blink_driver_fops);
	mydev.cdev.owner = THIS_MODULE;

	if (cdev_add(&mydev.cdev, mydev.my_node, DEVCNT)) {
		printk(KERN_ERR "cdev_add() failed!\n");

		/*clean up chrdev alloc */
		goto unreg_chrdev_region;
	}

	/*Registering the PCI driver */
	if(pci_register_driver(&pci_blink_driver)) {

		printk(KERN_ERR "pci_register driver failed\n");
		goto unreg_pci_driver;
	}

	/*Create a dev node*/
	if((mydev.my_class = class_create(THIS_MODULE, "led_dev")) == NULL) {

		printk(KERN_ERR "class_create failed\n");
	
		goto destroy_class;
	}

	/*Equivalent of mknod*/
	if(device_create(mydev.my_class, NULL, mydev.my_node, NULL, NODENAME) == NULL) {

		printk(KERN_ERR "device_create failed\n");
		goto unreg_dev_create;
	}
	return 0;

unreg_pci_driver:
	pci_unregister_driver(&pci_blink_driver);

unreg_chrdev_region:
	unregister_chrdev_region(mydev.my_node, DEVCNT);

unreg_dev_create:
	device_destroy(mydev.my_class, mydev.my_node);

destroy_class:
	class_destroy(mydev.my_class);
	
exit:
	return -1;
}

static void __exit blink_driver_exit(void) {

	/*Unregister PCI driver */
	pci_unregister_driver(&pci_blink_driver);

	/*destroy the device*/
	device_destroy(mydev.my_class, mydev.my_node);

	/*destroy the class*/
	class_destroy(mydev.my_class);

	/*destroy the cdev */
	cdev_del(&mydev.cdev);

	/*clean up the devices */
	unregister_chrdev_region(mydev.my_node, DEVCNT);

	/*Delete timer*/
	del_timer_sync(&my_timer);

	printk(KERN_INFO "%s module unloaded!\n", DEVNAME);
}


MODULE_AUTHOR("Philippe Proctor");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");
module_init(blink_driver_init);
module_exit(blink_driver_exit);

