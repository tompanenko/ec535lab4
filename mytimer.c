/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/system_misc.h> /* cli(), *_flags */
#include <linux/uaccess.h>
#include <asm/uaccess.h> /* copy_from/to_user */
#include <uapi/linux/string.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/vmalloc.h>
#include <asm/siginfo.h>    //siginfo
#include <linux/rcupdate.h> //rcu_read_lock
#include <linux/sched.h>    //find_task_by_pid_type
#include <linux/sched/signal.h>
#include <linux/seq_file.h>

MODULE_LICENSE("Dual BSD/GPL");

/* Declaration of memory.c functions */
static int nibbler_open(struct inode *inode, struct file *filp);
static int nibbler_release(struct inode *inode, struct file *filp);
static ssize_t nibbler_read(struct file *filp,
                char *buf, size_t count, loff_t *f_pos);
static ssize_t nibbler_write(struct file *filp,
                const char *buf, size_t count, loff_t *f_pos);
static void nibbler_exit(void);
static int fasync_example_fasync(int fd, struct file *filp, int mode);
static int nibbler_init(void);
static int open_proc(struct inode*, struct file*);
static int read_proc(struct seq_file *m, void *v);


//static struct proc_dir_entry *proc_write_entry;

static int initial_time;

struct timer_inst{
        struct timer_list timer;
        char message[256];
        int duration;
        int pid;
        int used;
};
static struct timer_inst timer_instances[2];
static int max_timers = 1;
static int num_timers = 0;
//static char proc_data[MAX_PROC_SIZE];

static int open_proc(struct inode *inode, struct file *file) {
    return single_open(file, read_proc, NULL);
}


static int read_proc(struct seq_file *m, void *v) {
    /* Wrap-around */
        int time_elapsed = 1000*(jiffies - initial_time)/HZ;

        if(!timer_instances[0].used && !timer_instances[1].used){
                seq_printf(m,"ktimer %d\n",time_elapsed);
        }

        else if(timer_instances[0].used && !timer_instances[1].used){
                int time_remaining = (timer_instances[0].timer.expires - jiffies)/HZ;
                seq_printf(m,"ktimer %d\n%d mytimer %d\n",time_elapsed,timer_instances[0].pid,time_remaining);
        }
        else if(timer_instances[1].used && !timer_instances[0].used){
                int time_remaining = (timer_instances[1].timer.expires - jiffies)/HZ;
                seq_printf(m,"ktimer %d\n%d mytimer %d\n",time_elapsed,timer_instances[1].pid,time_remaining);
        }
        else{
                int time_remaining = (timer_instances[0].timer.expires - jiffies)/HZ;
                int time_remaining2 = (timer_instances[1].timer.expires - jiffies)/HZ;
                seq_printf(m,"ktimer %d\n%d mytimer %d\n%d mytimer %d\n",time_elapsed,timer_instances[0].pid,time_remaining,timer_instances[1].pid,time_remaining2);
        }
         return 0;
}

/* Structure that declares the usual file */
/* access functions */
struct file_operations nibbler_fops = {
        read: nibbler_read,
        write: nibbler_write,
        open: nibbler_open,
        release: nibbler_release,
        fasync: fasync_example_fasync

};

struct file_operations proc_fops = {
read:   seq_read,
open: open_proc,
release: single_release
};

/* Declaration of the init and exit functions */
module_init(nibbler_init);
module_exit(nibbler_exit);

static unsigned capacity = 128;
static unsigned bite = 128;
module_param(capacity, uint, S_IRUGO);
module_param(bite, uint, S_IRUGO);

/* Global variables of the driver */
/* Major number */
static int nibbler_major = 61;
/* length of the current message */
static int nibbler_len;
struct fasync_struct *async_queue;
static char newbuffer[256];

void timer_function(struct timer_list *timer){
        
        int i;
        //printk(KERN_INFO "in timer callback\n");
        for(i = 0; i < max_timers; i++){
                if(timer_instances[i].used && &timer_instances[i].timer == timer){
                        
                        //printk(KERN_INFO "found matching timer");
                        num_timers--;
                        del_timer(timer);
                        timer_instances[i].used = 0;
                        if (async_queue){
                                kill_fasync(&async_queue, SIGIO, POLL_IN);
                        }
                        
                        // printk(KERN_ALERT "%s", timer_instances[i].message);
                        
                }
        }
}

static int nibbler_init(void)
{
        int result;
        int i;
        initial_time = jiffies;
        proc_create("mytimer",0,NULL,&proc_fops);

        for(i = 0; i < 2; i++){
                timer_instances[i].used = 0;
        }

        // proc_write_entry = create_proc_entry("proc_entry",0666,NULL);
        // if(!proc_write_entry)
        //       {
        //     printk(KERN_INFO "Error creating proc entry");
        //     return -ENOMEM;
        //     }
        // proc_write_entry->read_proc = read_proc ;
        // proc_write_entry->write_proc = write_proc;
        //printk(KERN_INFO "proc initialized");

        /* Registering device */
        result = register_chrdev(nibbler_major, "mytimer", &nibbler_fops);
        if (result < 0)
        {
                printk(KERN_ALERT
                        "nibbler: cannot obtain major number %d\n", nibbler_major);
                return result;
        }

        /* Allocating nibbler for the buffer */
        // nibbler_buffer = kmalloc(capacity, GFP_KERNEL);
        // if (!nibbler_buffer)
        // {
        //         printk(KERN_ALERT "Insufficient kernel memory\n");
        //         result = -ENOMEM;
        //         goto fail;
        // }
        // memset(nibbler_buffer, 0, capacity);
        nibbler_len = 0;

        //printk(KERN_ALERT "Inserting nibbler module\n");

        return 0;

// fail:
//         nibbler_exit();
//         return result;
}

static void nibbler_exit(void)
{
        /* Freeing the major number */
        unregister_chrdev(nibbler_major, "mytimer");
        remove_proc_entry("proc_entry",NULL);
        /* Freeing buffer memory */
        // if (nibbler_buffer)
        // {
        //         kfree(nibbler_buffer);
        // }


        //printk(KERN_ALERT "Removing nibbler module\n");

}

static int nibbler_open(struct inode *inode, struct file *filp)
{
        //printk(KERN_INFO "open called: process id %d, command %s\n",
          //      current->pid, current->comm);
        /* Success */
        return 0;
}

static int fasync_example_fasync(int fd, struct file *filp, int mode) {
    return fasync_helper(fd, filp, mode, &async_queue);
}

static int nibbler_release(struct inode *inode, struct file *filp)
{
        fasync_example_fasync(-1, filp, 0);
        // printk(KERN_INFO "release called: process id %d, command %s\n",
        //         current->pid, current->comm);
        /* Success */
        return 0;
}



static ssize_t nibbler_read(struct file *filp, char *buf,
                                                        size_t count, loff_t *f_pos)
{

        // printk(KERN_INFO "in read %s\n",newbuffer);
        copy_to_user(buf,newbuffer,100);
        return 256;
        // /* end of buffer reached */
        // if (*f_pos >= nibbler_len)
        // {
        //         return 0;
        // }

        // /* do not go over then end */
        // if (count > nibbler_len - *f_pos)
        //         count = nibbler_len - *f_pos;

        // /* do not send back more than a bite */
        // if (count > bite) count = bite;

        // for(int i = 0; i < max_timers; i++){
        //         char single_timer[256];
        //         char time_remaining[256];
        //         if(timer_instances[i].active == 1){
        //                 sprintf(single_timer,"%s",timer_instances[i].message);
        //                 strreplace(single_timer,'\n',' ');
        //                 strcat(nibbler_buffer,single_timer);
        //                 sprintf(time_remaining,"%ld\n",(timer_instances[i].timer.expires - jiffies)/HZ);
        //                 strcat(nibbler_buffer,time_remaining);
        //         }
        // }
        // strcat(nibbler_buffer,"\0");
        // count = strlen(nibbler_buffer)+1;
        // /* Transfering data to user space */
        // if (copy_to_user(buf, nibbler_buffer, count))
        // {
        //         return -EFAULT;
        // }

        // /* Changing reading position as best suits */
        // *f_pos += count;
        // return count;
}


static ssize_t nibbler_write(struct file *filp, const char *buf,
                                                        size_t count, loff_t *f_pos)
{
        int i;
        if (copy_from_user(newbuffer,buf,count)){
                return -EFAULT;
        }
        //printk(KERN_INFO "%s\n",newbuffer);

        if(*newbuffer == 'X'){
                int pid;
                sscanf(newbuffer,"X %d",&pid);
                if(timer_instances[0].pid == pid){
                        sprintf(newbuffer,"Y");
                }
                else{
                        sprintf(newbuffer,"N");
                }
        }

        if(*newbuffer == 'L'){
                char finalstring[256];

                if(!timer_instances[0].used && !timer_instances[1].used){
                        //printk(KERN_INFO "neither used");
                        sprintf(finalstring,"");
                }

                else if(timer_instances[0].used && !timer_instances[1].used){
                        int time_remaining = (timer_instances[0].timer.expires - jiffies)/HZ;
                        //printk(KERN_INFO "first used");
                        sprintf(finalstring,"%s %d\n",timer_instances[0].message,time_remaining);
                }
                else if(timer_instances[1].used && !timer_instances[0].used){
                        int time_remaining = (timer_instances[1].timer.expires - jiffies)/HZ;
                        //printk(KERN_INFO "second used");
                        sprintf(finalstring,"%s %d\n",timer_instances[1].message,time_remaining);
                }
                else{
                        int time_remaining = (timer_instances[0].timer.expires - jiffies)/HZ;
                        int time_remaining2 = (timer_instances[1].timer.expires - jiffies)/HZ;
                        //printk(KERN_INFO "both used");
                        sprintf(finalstring,"%s %d\n%s %d\n",timer_instances[0].message,time_remaining,timer_instances[1].message,time_remaining2);
                }

                sprintf(newbuffer,"%s",finalstring);
                //result = copy_to_user(buf,finalstring,256);
        }
        else if(*newbuffer == 'S'){
                pid_t pid = 0;
                int time = 0;
                char message[256];
                int found = 0;

                
                
               // printk(KERN_INFO "buffer is %s\n",newbuffer);
                sscanf(newbuffer,"S %d %d %s",&pid,&time,&message);
               // printk(KERN_INFO "time is %d\n",time);
                for(i = 0; i < max_timers; i++){
                        //printk(KERN_INFO "used %d and message %s and new message %s\n",timer_instances[i].used,timer_instances[i].message,message);
                        if(timer_instances[i].used && strcmp(message,timer_instances[i].message) == 0){
                                //result = copy_to_user(buf,"U",1);
                                sprintf(newbuffer,"U");
                                mod_timer(&(timer_instances[i].timer), jiffies + msecs_to_jiffies(time * 1000));
                                return 2;
                        }
                }

                if(num_timers == max_timers){
                        sprintf(newbuffer,"C");
                        return 1;
                }

                if(!found){
                        //printk(KERN_INFO "in not found loop\n");
                        for(i = 0; i < max_timers; i++){
                                if(!timer_instances[i].used){
                                        //printk(KERN_INFO "in unused timer with time %d\n",time);
                                        timer_setup(&(timer_instances[i].timer), timer_function, 0);
                                        mod_timer(&timer_instances[i].timer, jiffies + msecs_to_jiffies(time*1000));
                                        strcpy(timer_instances[i].message,message);
                                        timer_instances[i].duration = time;
                                        timer_instances[i].pid = pid;
                                        timer_instances[i].used = 1;
                                }
                        }
                        num_timers++;
                        sprintf(newbuffer,"N");
                        // printk("in s %s\n",newbuffer);
                        //result = copy_to_user(buf,"S",1);
                }
                

        }
        else if(*newbuffer == 'R'){
                for(i = 0; i < max_timers; i++){
                        if(timer_instances[i].used){
                                del_timer(&timer_instances[i].timer);
                                del_timer_sync(&timer_instances[i].timer);
                                timer_instances[i].used = 0;
                        }
                }
                num_timers = 0;
        }

        // else if(*newbuffer == 'M'){
        //         int num = 0;
        //         sscanf(newbuffer,"M %d",&num);
        //         max_timers = num;
        // }

        /* end of buffer reached */
        // if (*f_pos >= capacity)
        // {
        //         printk(KERN_INFO
        //                 "write called: process id %d, command %s, count %d, buffer full\n",
        //                 current->pid, current->comm, count);
        //         return -ENOSPC;
        // }

        // /* do not eat more than a bite */
        // if (count > bite) count = bite;

        // /* do not go over the end */
        // if (count > capacity - *f_pos)
        //         count = capacity - *f_pos;

        // if (copy_from_user(nibbler_buffer + *f_pos, buf, count))
        // {
        //         return -EFAULT;
        // }

        
        
        // long seconds=0;
        // char finalmessage[256] 
        // char* p = tbuf;

        // for (char* p = *f_pos; p < count + *f_pos; p++) {
        //         tbptr2 += sprintf(finalmessage, "%c", mytimer_buffer[p]);
        //         msg_len += 1;
        // }
        // while(i < active_timers){
        //         for (i = 0; i < max_count ; i++) {
        //                 timer_setup(&(timer_instances[i].timer), timer_function, 0);
        //                 mod_timer(&(timer_instances[i].timer), jiffies + msecs_to_jiffies(seconds * 1000));
        //                 active_timers += 1;
        //                 sprintf(timer_instances[i].timer, "%s",finalmessage);
        //                 timer_instances[i].is_active = 1;
        //                 timer_instances[i].duration = seconds;
        //         }
        // }
        // *f_pos += count;
        // nibbler_len = *f_pos;


        count = 5;

        return count;
}
