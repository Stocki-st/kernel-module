#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/slab.h>  //kmalloc
#include <linux/uaccess.h> //for copy to/from userspace
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "is18_ioctl.h"

MODULE_LICENSE("Dual BSD/GPL");


#define MINOR_COUNT 5
#define DRVNAME "is18drv"
#define BUFFER_SIZE 16 // should be 1024--> 16 choosen just for testing purpose
#define PROC_FILE "is18/info"

static int is18_open(struct inode *inode, struct file *filp);
static int is18_close(struct inode *inode, struct file *filp);
static ssize_t is18_read(struct file *filp, char __user *buff, size_t count, loff_t *offset);
static ssize_t is18_write(struct file *filp, const char __user *buff, size_t count, loff_t *offset);
static long is18_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

// Die Struktur file_operations besitzt als Member Variablen
// pro moeglichen System call (read, write, etc.) einen Funktionszeiger.
// Den Funktionszeigern muss die Funktionsaddresse unserer
// Systemcall Implementierungen fuer open, read, write, etc. zugewiesen
// werden. In unserem Fall wird eine Instanz is18_fcalls von der Struktur
// file_operations angelegt und den einzelnen Funktionszeigern
// (Member Variablen der Struktur) werden unsere eigenen Systemcall Funktionen
// zugewiesen (is18_open, is18_close, ...)
static struct file_operations is18_fcalls = {
    .owner = THIS_MODULE,
    .open = is18_open,
    .release = is18_close,
    .read = is18_read,
    .write = is18_write,
    .unlocked_ioctl = is18_ioctl,
};

// Pro Device gibt es eine Instanz dieser Struktur.
struct is18_cdev
{
    struct semaphore sem_sync; //synchronisation for accessing critical section
    int next_read_index;
    int next_write_index;
    // number of bytes which are still waiting for be read
    //size of buffer is BUFFER_SIZE, allowed values between 0 and BUFFER_SIZE
    int current_pipe_bytes; // number of unread bytes in fifo
    int current_open_read_cnt;
    int current_open_write_cnt;
    int device_number;

    char* buffer;
    wait_queue_head_t wq_free_space_available;
    wait_queue_head_t wq_read_data_available;
    struct completion comp_buffer_initialized;
    struct cdev chdev; // wird vom driver benoetigt. MUSS vorhanden sein!
};

static struct class *is18_class;

static dev_t dev_num; // dev_t = __kernel_dev_t = __u32 = unsigned int bei x86/amd64

static struct is18_cdev is18_devs[MINOR_COUNT];

// procfs functions
static int is18_seq_open (struct inode *, struct file *);
static void *is18_start (struct seq_file *, loff_t *);
static void is18_stop (struct seq_file *, void *);
static void *is18_next (struct seq_file *, void *, loff_t *);
static int is18_show (struct seq_file *, void *);

// procfs file ops
static struct proc_ops is18_proc_fcalls = {
    .proc_open = is18_seq_open,
    .proc_read = seq_read, // predefined
    .proc_lseek=seq_lseek, // --"--
    .proc_release=seq_release // --"-
};
// seq operations
static struct seq_operations is18_proc_seq_ops = {
    .start = is18_start,
    .stop = is18_stop,
    .next = is18_next,
    .show = is18_show
};

static int __init is18drv_init(void)
{
    int rv;
    int i, ii;
    printk(KERN_INFO "Hello from my character driver %s!\n", DRVNAME);
    rv = alloc_chrdev_region(&dev_num, 0 /* first minor nr */, MINOR_COUNT, DRVNAME);
    if (rv) {
        goto err1;
    }
    printk(KERN_INFO "major nr: %d, start with minor nr: %d\n",
           MAJOR(dev_num), MINOR(dev_num));

    is18_class = class_create(THIS_MODULE, "is18_driver_class");
    if(IS_ERR(is18_class)) {
        goto err1b;
    }

    if(NULL == (proc_create (PROC_FILE, 0, NULL, &is18_proc_fcalls))) {
        printk(KERN_WARNING "is18drv: unable to create proc file\n");
        goto err1c;
    }

    for (i = 0; i < MINOR_COUNT; ++i) {
        dev_t cur_devnr = MKDEV(MAJOR(dev_num), MINOR(dev_num) + i);
        cdev_init(&is18_devs[i].chdev, &is18_fcalls);
        // init member
        sema_init(&is18_devs[i].sem_sync, 1);
        is18_devs[i].chdev.owner = THIS_MODULE;
        is18_devs[i].next_read_index = 0;
        is18_devs[i].next_write_index = 0;
        is18_devs[i].current_pipe_bytes = 0;
        is18_devs[i].current_open_read_cnt = 0;
        is18_devs[i].current_open_write_cnt = 0;
        is18_devs[i].device_number = MINOR(cur_devnr);

        is18_devs[i].buffer = NULL;
        init_waitqueue_head(&is18_devs[i].wq_free_space_available);
        init_waitqueue_head(&is18_devs[i].wq_read_data_available);
        init_completion(&is18_devs[i].comp_buffer_initialized);

        // device file anlegen
        if(IS_ERR(device_create(is18_class,NULL,cur_devnr,NULL, "is18dev%d",i))) {
            rv = -ENODEV;
            goto err2;
        }

        // fuegt das device zum system
        rv = cdev_add(&is18_devs[i].chdev, cur_devnr, 1);
        if (rv < 0) {
            //device_destroy(is00_class, is00_devs[i].chdev.dev);
            device_destroy(is18_class, cur_devnr);
            printk(KERN_WARNING "cdev_add failed\n");
            goto err2;
        }
        printk(KERN_INFO "new device with major nr: %d, minor nr: %d\n",
               MAJOR(cur_devnr), MINOR(cur_devnr));
    }
    return 0;
err2:
    for (ii = 0; ii < i; ++ii) {
        device_destroy(is18_class, is18_devs[ii].chdev.dev);
        cdev_del(&is18_devs[ii].chdev);
    }
    class_destroy(is18_class);
err1c:
    remove_proc_entry(PROC_FILE, NULL);
err1b:
    unregister_chrdev_region(dev_num, MINOR_COUNT);
err1:
    return rv;
}

static void __exit is18drv_exit(void)
{
    int i;
    for (i = 0; i < MINOR_COUNT; i++) {
        device_destroy(is18_class, is18_devs[i].chdev.dev);
        cdev_del(&is18_devs[i].chdev);
        if(is18_devs[i].buffer) {
            printk("free buffer of device %d\n", i);
            kfree(is18_devs[i].buffer);
        }
        printk("cleanup device %d\n", i);
    }

    class_destroy(is18_class);
    unregister_chrdev_region(dev_num, MINOR_COUNT);
    remove_proc_entry(PROC_FILE, NULL);
    printk(KERN_INFO "Remove my character driver %s\n", DRVNAME);
}

static int is18_open(struct inode *inode, struct file *filp) {
    // container_of returns start adress of my device based on the offset from inode->i_cdev
    struct is18_cdev *dev = container_of(inode->i_cdev, struct is18_cdev, chdev);
    //remember device in pricate data of device
    //enables easier access is is18_read & is18_write
    filp->private_data = dev;


    if(down_interruptible(&dev->sem_sync)) {
        return -ERESTARTSYS;
    }

    if(filp->f_mode & FMODE_WRITE) {
        // file opened with write rights
        ++dev->current_open_write_cnt;
        if(!dev->buffer) {
            dev->buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
        }
        complete_all(&dev->comp_buffer_initialized);
    }


    if(filp->f_mode & FMODE_READ) {
        if(!dev->buffer) {
            if((filp->f_flags & O_NONBLOCK) | (filp->f_flags & O_NDELAY )) {
                // no blocking/waiting allowed
                printk(KERN_INFO "is18drv: open in NON-blocking mode");
                up(&dev->sem_sync);
                return -EAGAIN;
            } else {
                // Readers that are not also writers and want to block until a buffer is available should wait here.
                if(!(filp->f_mode & FMODE_WRITE)) {
                    up(&dev->sem_sync);
                    printk(KERN_INFO "is18drv: 'open' will be delayed - waiting for init buffer completed\n");

                    if(wait_for_completion_interruptible(&dev->comp_buffer_initialized) == -ERESTARTSYS) {
                        return -ERESTARTSYS;
                    }
                    printk(KERN_INFO "is18drv: init buffer completed - will open now\n");

                    if(down_interruptible(&dev->sem_sync)) {
                        return -ERESTARTSYS;
                    }
                }
            }
        }
        // file opened with read rights
        ++dev->current_open_read_cnt;
    }

    printk(KERN_INFO "is18drv: 'open' is called! read_cnt: %d, write_cnt: %d\n", dev->current_open_read_cnt, dev->current_open_write_cnt);
    up(&dev->sem_sync);

    return 0;
}

static int is18_close(struct inode *inode, struct file *filp) {
    struct is18_cdev *dev = filp->private_data;

    if(down_interruptible(&dev->sem_sync)) {
        return -ERESTARTSYS;
    }

    if(filp->f_mode & FMODE_READ) {
        // file with read rights closed
        --dev->current_open_read_cnt;
    }

    if(filp->f_mode & FMODE_WRITE) {
        // file with write rights closed
        --dev->current_open_write_cnt;
    }

    printk(KERN_INFO "is18drv: 'close' is called! read_cnt: %d, write_cnt: %d\n", dev->current_open_read_cnt, dev->current_open_write_cnt);

    up(&dev->sem_sync);

    return 0;
}

static ssize_t is18_read(struct file *filp, char __user *buff, size_t count, loff_t *offset) {
    int i;
    ssize_t copied = 0;
    struct is18_cdev *dev = filp->private_data;

    printk(KERN_INFO "is18drv: 'read' is called!\n");

    if(down_interruptible(&dev->sem_sync)) {
        return -ERESTARTSYS;
    }

    for(i = 0; i < count; ++i) {
        if(dev->current_pipe_bytes <= 0) {
            // --> pipe is empty
            if((filp->f_flags & O_NONBLOCK) | (filp->f_flags & O_NDELAY )) {
                // no blocking/waiting allowed
                printk(KERN_INFO "read in NON-blocking mode");
                up(&dev->sem_sync); // Semaphore freigeben
                return copied;
            }
            printk(KERN_INFO "read in blocking mode - nonblock: %d - ndelay: %d \n", filp->f_flags & O_NONBLOCK,  filp->f_flags & O_NDELAY );
            while(dev->current_pipe_bytes < 1) {
                //wait for content
                pr_info("is18drv: please wait, currently no data for reading available...\n");
                // release sem before waiting
                up(&dev->sem_sync);
                // wait until space is available again
                if(wait_event_interruptible(dev->wq_read_data_available,
                                            (dev->current_pipe_bytes > 0)) != 0 ) {
                    return -ERESTARTSYS;
                }
                // space is available again --> get semaphore again
                if(down_interruptible(&dev->sem_sync)) {
                    return -ERESTARTSYS;
                }
            }


        }
        //still free space for at least 1 byte
        if (put_user(dev->buffer[dev->next_read_index], buff + i) != 0) {
            if (!copied) {
                copied = -EFAULT;
            }
            break;
        }

        ++copied;
        --dev->current_pipe_bytes;
        ++dev->next_read_index;
        dev->next_read_index %= BUFFER_SIZE;

        printk(KERN_INFO "is18drv: copied %ld\n",copied);
        printk(KERN_INFO "is18drv: dev->current_pipe_bytes %d\n",dev->current_pipe_bytes);
        printk(KERN_INFO "is18drv: dev->next_write_index %d\n",dev->next_write_index);
        wake_up(&dev->wq_free_space_available);
    }

    up(&dev->sem_sync);

    return copied;
}

static ssize_t is18_write(struct file *filp, const char __user *buff, size_t count, loff_t *offset) {
    int i;
    ssize_t copied = 0;
    struct is18_cdev *dev = filp->private_data;

    printk(KERN_INFO "is18drv: 'write' is called!\n");

    if(down_interruptible(&dev->sem_sync)) {
        return -ERESTARTSYS;
    }
    for(i = 0; i < count; ++i) {
        while(dev->current_pipe_bytes >= BUFFER_SIZE) {
            printk(KERN_INFO "Pipe is full\n");
            // --> pipe is full
            if((filp->f_flags & O_NONBLOCK) | (filp->f_flags & O_NDELAY ))  {
                // no blocking/waiting allowed
                up(&dev->sem_sync); // Semaphore freigeben
                return copied ? copied : -ENOSPC;
            }
            pr_info("please wait, currently no space available...");
            // release sem before waiting
            up(&dev->sem_sync);
            // wait until space is available again
            if(wait_event_interruptible(dev->wq_free_space_available,
                                        (dev->current_pipe_bytes < BUFFER_SIZE )) != 0 ) {
                return -ERESTARTSYS;
            }
            // space is available again --> get semaphore again
            if(down_interruptible(&dev->sem_sync)) {
                return -ERESTARTSYS;
            }
        }
        //just like memcpy:
        // copy_from_user(dev->buffer + dev->next_write_index, //to here
        // buff + i, // from here
        // 1); <-- num ob bytes --> could be tricky with fifo
        // returns: num of NOT copied bytes

        //still free space for at least 1 byte
        if (get_user(dev->buffer[dev->next_write_index], buff + i) != 0) {
            if (!copied) {
                copied = -EFAULT;
            }
            break;
        }

        ++copied;
        ++dev->current_pipe_bytes;
        ++dev->next_write_index;
        dev->next_write_index %= BUFFER_SIZE;
        printk(KERN_INFO "is18drv: copied %ld\n",copied);
        printk(KERN_INFO "is18drv: dev->current_pipe_bytes %d\n",dev->current_pipe_bytes);
        printk(KERN_INFO "is18drv: dev->next_write_index %d\n",dev->next_write_index);

        wake_up(&dev->wq_read_data_available);

    }
    up(&dev->sem_sync);

    return copied ? copied : -ENOSPC;
}


static long is18_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct is18_cdev *dev = filp->private_data;
    long rv = 0; // return value

    if (_IOC_TYPE(cmd) != IS18_IOC_MY_MAGIC) {
        //wrong magic numbe --> someone opened ioctl on my device
        return -1;
    }

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // ACHTUNG: Sobald auf das Device zugegriffen wird, muss gesynct werden!!!!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    switch (_IOC_NR(cmd)) {
    case IS18_IOC_NR_OPENREADCNT:
        if (_IOC_DIR(cmd) != _IOC_NONE) {
            // wrong direction. Must be "no data transfer" (because arg is not used)
            // ...
            break;
        }
        printk(KERN_INFO "is18drv: called IS18_IOC_OPENREADCNT via ioctl\n");

        if(down_interruptible(&dev->sem_sync)) {
            return -ERESTARTSYS;
        }
        rv = dev->current_open_read_cnt;
        up(&dev->sem_sync);

        break;
    case IS18_IOC_NR_OPENWRITECNT:
        if (_IOC_DIR(cmd) != _IOC_NONE) {
            // wrong direction. Must be "no data transfer" (because arg is not used)
            // ...
            break;
        }

        printk(KERN_INFO "is18drv: called IS18_IOC_OPENWRITECNT via ioctl\n");
        if(down_interruptible(&dev->sem_sync)) {
            return -ERESTARTSYS;
        }
        rv = dev->current_open_write_cnt;
        up(&dev->sem_sync);

        break;
    case IS18_IOC_NR_DEL_COUNT:
    {
        int del_count;
        if (_IOC_DIR(cmd) != _IOC_WRITE) {
            // wrong direction. Must be "writing to the device"
            printk(KERN_ERR "is18drv: WRONG direction for ioctl with is18_IOC_DEL_COUNT\n");
            // ...
            break;
        }
        printk(KERN_INFO "is18drv: called IS18_IOC_DEL_COUNT via ioctl\n");

        // access_ok() muss hier NICHT verwendet werden.
        // Wird nur benoetigt, wenn ein Puffer per arg uebergeben wird. (Also
        // wenn arg als Zeiger verwendet wird. Ist aber hier nicht der Fall.
        // es wurde einfach nur ein Integerwert uebergeben.)
        del_count = arg;
        printk("is18drv: ioctl from user space use the value %d\n", del_count);
        // ....
        break;
    }
    case IS18_IOC_NR_READ_INDEX:
        if (_IOC_DIR(cmd) != _IOC_NONE) {
            // wrong direction. Must be "no data transfer" (because arg is not used)
            // ...
            break;
        }
        printk(KERN_INFO "is18drv: called IS18_IOC_READ_INDEX via ioctl\n");

        if(down_interruptible(&dev->sem_sync)) {
            return -ERESTARTSYS;
        }
        rv = dev->next_read_index;
        up(&dev->sem_sync);

        break;
    case IS18_IOC_NR_WRITE_INDEX :
        if (_IOC_DIR(cmd) != _IOC_NONE) {
            // wrong direction. Must be "no data transfer" (because arg is not used)
            // ...
            break;
        }
        printk(KERN_INFO "is18drv: called IS18_IOC_WRITE_INDEX via ioctl\n");


        if(down_interruptible(&dev->sem_sync)) {
            return -ERESTARTSYS;
        }
        rv = dev->next_write_index;
        up(&dev->sem_sync);

        break;
    case IS18_IOC_NR_NUM_BUFFERED_BYTES:
        if (_IOC_DIR(cmd) != _IOC_NONE) {
            // wrong direction. Must be "no data transfer" (because arg is not used)
            // ...
            break;
        }
        printk(KERN_INFO "is18drv: called IS18_IOC_NUM_BUFFERED_BYTES via ioctl\n");


        if(down_interruptible(&dev->sem_sync)) {
            return -ERESTARTSYS;
        }
        rv = dev->current_pipe_bytes;
        up(&dev->sem_sync);

        break;
    case IS18_IOC_NR_EMPTY_BUFFER:
        if (_IOC_DIR(cmd) != _IOC_NONE) {
            // wrong direction. Must be "no data transfer" (because arg is not used)
            // ...
            break;
        }
        printk(KERN_INFO "is18drv: called IS18_IOC_EMPTY_BUFFER via ioctl\n");


        if(down_interruptible(&dev->sem_sync)) {
            return -ERESTARTSYS;
        }
        //set read/write index and number of bytes in buffer to 0 --> empty
        dev->next_read_index = 0;
        dev->next_write_index = 0;
        dev->current_pipe_bytes = 0;
        rv = 0;

        up(&dev->sem_sync);
        break;
    default:
        break;
        // ...
    }

    return rv;
}


static int is18_seq_open (struct inode *inode, struct file *filp) {
    return seq_open(filp, &is18_proc_seq_ops);
}

// If *off is zero, returns a pointer to the first is18_devs entry.
// Otherwise, returns NULL.
static void *is18_start (struct seq_file *sf, loff_t *pos) {
    printk(KERN_INFO "is18drv: is18_start() called with offset %llu\n", *pos);
    if(!(*pos)) {
        return is18_devs;
    }
    return NULL;
}

static void is18_stop (struct seq_file *sf, void *it) {
    printk(KERN_INFO "is18drv: is18_stop() called\n");
}

//  get next character device
static void *is18_next (struct seq_file *sf, void *it, loff_t *pos) {
    // return 0, when there are no more devices
    if(++*pos >= MINOR_COUNT) {
        return NULL;
    }

    return is18_devs + *pos;
}
// show device details
static int is18_show (struct seq_file *sf, void *it) {
    struct is18_cdev *dev = it;
    if(down_interruptible(&dev->sem_sync)) {
        return -ERESTARTSYS;
    }

    // print device state
    seq_printf(sf, "# device: %d \n - buffered bytes: %d\n - read index: %d\n - write index: %d\n - open read cnt: %d\n - open write cnt: %d\n\n", dev->device_number, dev->current_pipe_bytes, dev->next_read_index, dev->next_write_index, dev->current_open_read_cnt, dev->current_open_write_cnt);
    up(&dev->sem_sync);

    return 0;
}

// Driver initialization entry point
// Function to be run at kernel boot time or module insertion.
module_init(is18drv_init);
// Driver exit entry point
// Function to be run when driver is removed.
module_exit(is18drv_exit);


