/*
 * Philippe Proctor
 * 4/17/2019
 * ECE 373
 *
 * Hw 2 part 1: Setup driver module
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DEVCNT 1
#define DEVNAME "p_dev"

/*Start of private driver data struct, passed around inside driver*/


static struct mydev_dev {
	struct cdev cdev;
	int syscall_val;
} mydev;

static dev_t mydev_node;

/*Allows user to change param w/ in driver module*/
static int input = 40;
module_param(input, int, S_IRUSR | S_IWUSR);

static int p_dev_open(struct inode *inode, struct file *file){
	
	printk(KERN_INFO "Succesfully opened");
	mydev.syscall_val = input;

	return 0;

}

//Read callback 
static ssize_t p_dev_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
	
	/*Get local kernel buffer set aside */
	int ret;


	/*Safety check that offset is less than int */
	if(*offset >= sizeof(int)){
		return 0;
	}

	/*Check if user passed null value to buffer */
	if(!buf) {
		ret = -EINVAL;
		goto out;		//undo allocations
	}

	printk(KERN_INFO "Inside read function");

	/*Copy value from driver struct to user space */
	if(copy_to_user(buf, &mydev.syscall_val, sizeof(int))){
		ret = -EFAULT;
		goto out;
	}

	/*Update offset by */
	ret = sizeof(int);
	*offset += len;

	printk(KERN_INFO "User got from us %d\n", mydev.syscall_val);
out:
	return ret;
};

static ssize_t p_dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset ){
	
	/*Local kernel memory*/
	char *kern_buf;
	int ret;

	printk(KERN_INFO "Inside write function");

	/*Check if device passed null value to buffer*/
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
	
	mydev.syscall_val = *kern_buf;

	printk(KERN_INFO "Userspace wrote %s to us\n",kern_buf );

mem_out:
	kfree(kern_buf);
	
out:
	return ret;

}


static struct file_operations mydev_fops = {
	//fields
	.owner = THIS_MODULE,
	.open = p_dev_open,
	.read = p_dev_read,
	.write = p_dev_write,

};



static int __init p_device_init(void) {

	printk(KERN_INFO "p_device loading...");

	if (alloc_chrdev_region(&mydev_node, 0, DEVCNT, DEVNAME)) {	
		printk(KERN_ERR "alloc_chrdev_region() failed!\n");
		return -1;
	}

	printk(KERN_INFO "Allocated %d devices at major:%d\n",DEVCNT, MAJOR(mydev_node));


	/*Initialize the char device and add it to the kernel*/

	cdev_init(&mydev.cdev, &mydev_fops);
	mydev.cdev.owner = THIS_MODULE;

	if (cdev_add(&mydev.cdev, mydev_node, DEVCNT)) {
		printk(KERN_ERR "cdev_add() failed!\n");


		/*clean up chrdev alloc */
		unregister_chrdev_region(mydev_node, DEVCNT);
		return -1;
	}

	return 0;
}

static void __exit p_device_exit(void) {


	/*destroy the cdev */
	cdev_del(&mydev.cdev);

	/*clean up the devices */
	unregister_chrdev_region(mydev_node, DEVCNT);

	printk(KERN_INFO "%s module unloaded!\n", DEVNAME);
}


MODULE_AUTHOR("Philippe Proctor");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");
module_init(p_device_init);
module_exit(p_device_exit);

