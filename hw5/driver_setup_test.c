/*
 * Philippe Proctor
 * 4/28/2019
 * ECE 373
 *
 * Hw 3 part 1: Setup driver for LED blinking
 */

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

#define DEVCNT 1
#define DEVNAME "blinkDriver"
#define NODENAME "ece_led"

/*Start of private driver data struct, passed around inside driver*/

/*Allows user to change param w/ in driver module*/

static int blink_rate = 2;
module_param(blink_rate, int, S_IRUSR);
char blink_driver_name[] = DEVNAME;

static dev_t mydev_node;


/*structure definitions */
static struct mydev_dev {
        struct cdev cdev;
	struct class myclass;
        int syscall_val;
        long led_initial_val;
	long led_reg;

} mydev;


static struct myPci {
	struct pci_dev *pdev;
	void *hw_addr;

} myPci;


static int blink_driver_open(struct inode *inode, struct file *file){
	
//	printk(KERN_INFO "Succesfully opened");
	mydev.syscall_val = blink_rate;

	return 0;

}

//Read callback 
static ssize_t blink_driver_read(struct file *file, char __user *buf, size_t len, loff_t *offset){


	/*Get local kernel buffer set aside */
	int ret;

	/*LED register value */
	mydev.led_reg =  ioread32(myPci.hw_addr + 0xE00);

	/*Safety check that offset is less than int */
	if(*offset >= sizeof(int)){
		return 0;
	}

	/*Check if device passed null value to buffer */
	if(!buf) {
		ret = -EINVAL;
		goto out;		//undo allocations
	}

//	printk(KERN_INFO "Inside read function");


	/*Copy value from driver struct to user space */
	if(copy_to_user(buf, &mydev.led_reg, sizeof(int))){
		ret = -EFAULT;
		goto out;
	}

	/*Update offset by */
	ret = sizeof(int);
	*offset += len;

	printk(KERN_INFO "User got from us %lx\n", mydev.led_reg);
out:
	return ret;
}


static ssize_t blink_driver_write(struct file *file, const char __user *buf, size_t len, loff_t *offset ){
	
	/*Local kernel memory*/
	char *kern_buf;
	int ret;

//	printk(KERN_INFO "Inside write function");

	/*Check if user passed null value to buffer*/
	if(!buf){
		ret = -EINVAL;
		goto out;
	}

	/*Get memory to copy into...*/
	kern_buf = kmalloc(len, GFP_KERNEL);


	if(copy_from_user(kern_buf, buf, len)){
		ret = -EFAULT;
		goto mem_out;
	}

	ret = len;
	
//	mydev.syscall_val = *kern_buf;
	iowrite32(*kern_buf, (myPci.hw_addr + 0xE00));

	printk(KERN_INFO "Userspace wrote %lx to us\n",(unsigned long) *kern_buf );

	msleep(2000);

mem_out:
	kfree(kern_buf);
	
out:
	return ret;

}

/*Setting up driver to connect down to the device */
static int pci_blink_driver_probe(struct pci_dev *pdev, const struct pci_device_id *ent) {
	
	resource_size_t mmio_start, mmio_len;
	unsigned long barMask;

//	printk(KERN_INFO "Blink Driver PCI Probe called \n");

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
	
	printk(KERN_INFO "mmio start: %lx", (unsigned long) mmio_start);
	printk(KERN_INFO "mmio len: %lx", (unsigned long) mmio_len);

	if(!(myPci.hw_addr = ioremap(mmio_start, mmio_len))){
		
		printk(KERN_ERR "IOREMAP failed \n");
		goto unregister_ioremap;
	};

	mydev.led_initial_val = ioread32(myPci.hw_addr + 0xE00);
	printk(KERN_INFO "initial val is %lx\n", mydev.led_initial_val);

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
	.name = "Blink Driver",
	.id_table = pci_blink_driver_table,
	.probe = pci_blink_driver_probe,
	.remove = pci_blink_driver_remove,
};


/*Setting up driver to communicate up to userspace */
static int __init blink_driver_init(void) {

	printk(KERN_INFO "blink_driver loading...");

	if (alloc_chrdev_region(&mydev_node, 0, DEVCNT, DEVNAME)) {	
		printk(KERN_ERR "alloc_chrdev_region() failed!\n");
		goto exit;
	}

	printk(KERN_INFO "Allocated %d devices at major:%d\n",DEVCNT, MAJOR(mydev_node));


	/*Initialize the char device and add it to the kernel*/

	cdev_init(&mydev.cdev, &blink_driver_fops);
	mydev.cdev.owner = THIS_MODULE;

	if (cdev_add(&mydev.cdev, mydev_node, DEVCNT)) {
		printk(KERN_ERR "cdev_add() failed!\n");

		/*clean up chrdev alloc */
		goto unreg_chrdev_region;
	}

	/*Registering the PCI driver */
	if(pci_register_driver(&pci_blink_driver)) {

		printk(KERN_ERR "pci_register driver failed\n");
		goto unreg_pci_driver;
	}
/*
	Create a dev node 
	if((mydev.my_class = class_create(THIS_MODULE, "led_dev")) == NULL) {

		printk(KERN_ERR "class_create failed\n");
		goto destroy_class;
	}
	
	Equivalent of mknod
	if(device_create(mydev.my_class, NULL, mydev.mydev_node, NULL, NODENAME) == NULL) {

		printk(KERN_ERR, "device_create failed\n");
		goto unreg_dev_create;
	}
		

	return 0;
*/
unreg_pci_driver:
	pci_unregister_driver(&pci_blink_driver);

unreg_chrdev_region:
	unregister_chrdev_region(mydev_node, DEVCNT);
/*
//clean up node
unreg_dev_create:
        device_destroy(mydev.my_class, myDev_node);

destroy_class:
	class_destroy(mydev.my_class);
 */	
exit:
	return -1;
}

static void __exit blink_driver_exit(void) {

	/*Unregister PCI driver */
	pci_unregister_driver(&pci_blink_driver);
	/*destroy the cdev */
	cdev_del(&mydev.cdev);

	/*clean up the devices */
	unregister_chrdev_region(mydev_node, DEVCNT);

	printk(KERN_INFO "%s module unloaded!\n", DEVNAME);
}


MODULE_AUTHOR("Philippe Proctor");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");
module_init(blink_driver_init);
module_exit(blink_driver_exit);

