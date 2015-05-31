/*
 *  GPIO IRQ DHT sensor module for AR9331
 *
 *  Copyright (C) 2015 Dmitriy Zherebkov <dzh@black-swift.com>
 *  Copyright (C) 2015 Oleg Artamonov <oleg@black-swift.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#include <asm/mach-ath79/ar71xx_regs.h>
#include <asm/mach-ath79/ath79.h>
#include <asm/mach-ath79/irq.h>

#include <asm/delay.h>
#include <asm/siginfo.h>

#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

//#define DEBUG_OUT

#ifdef	DEBUG_OUT
#define debug(fmt,args...)	printk (KERN_INFO fmt ,##args)
#else
#define debug(fmt,args...)
#endif	/* DEBUG_OUT */

//#define SIG_DHT_IRQ	(SIGRTMIN+14)	// SIGRTMIN is different in Kernel and User modes
#define SIG_DHT_IRQ	44				// So we have to hardcode this value

////////////////////////////////////////////////////////////////////////////////////////////

#define DRV_NAME	"GPIO IRQ DHT"
#define FILE_NAME	"irq-dht"

////////////////////////////////////////////////////////////////////////////////////////////

#define GPIO_OFFS_READ		0x04
#define GPIO_OFFS_SET		0x0C
#define GPIO_OFFS_CLEAR		0x10

////////////////////////////////////////////////////////////////////////////////////////////

void __iomem *gpio_addr=NULL;
void __iomem *gpio_readdata_addr=NULL;
void __iomem *gpio_setdataout_addr=NULL;
void __iomem *gpio_cleardataout_addr=NULL;

////////////////////////////////////////////////////////////////////////////////////////////

#define DHT11 11
#define DHT22 22

////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	int				gpio;
	int				irq;
	int				counter;
	int				value[43];
} _gpio_handler;

static _gpio_handler	_ghandler;

static struct dentry* in_file;

////////////////////////////////////////////////////////////////////////////////////////////

static int is_space(char symbol)
{
	return (symbol == ' ') || (symbol == '\t');
}

////////////////////////////////////////////////////////////////////////////////////////////

static int is_digit(char symbol)
{
	return (symbol >= '0') && (symbol <= '9');
}

////////////////////////////////////////////////////////////////////////////////////////////

static irqreturn_t gpio_edge_interrupt(int irq, void* dev_id)
{
	_gpio_handler* handler=(_gpio_handler*)dev_id;

	if(handler && (handler->irq == irq) && (handler->counter < 43))
	{
//		debug("Got _handler!\n");

		if (((__raw_readl(gpio_addr + GPIO_OFFS_READ) >> handler->gpio) & 1) == 1)
		{
			udelay(35);
			handler->value[handler->counter] = (__raw_readl(gpio_addr + GPIO_OFFS_READ) >> handler->gpio) & 1;
			handler->counter++;
		}
	}
	else
	{
		debug("IRQ %d event - no handlers found!\n",irq);
	}

	return (IRQ_HANDLED);
}

////////////////////////////////////////////////////////////////////////////////////////////

static int add_irq(int gpio,void* data)
{
    if(gpio_request(gpio, DRV_NAME) >= 0)
    {
		int irq_number=gpio_to_irq(gpio);

		if(irq_number >= 0)
		{
		    int err = request_irq(irq_number, gpio_edge_interrupt, IRQ_TYPE_EDGE_BOTH, "gpio_irq_handler", data);

		    if(!err)
		    {
		    	debug("Got IRQ %d for GPIO %d\n", irq_number, gpio);
				return irq_number;
		    }
		    else
		    {
		    	debug("GPIO IRQ handler: trouble requesting IRQ %d error %d\n",irq_number, err);
		    }
		}
		else
		{
			debug("Can't map GPIO %d to IRQ : error %d\n",gpio, irq_number);
		}
    }
    else
    {
    	debug("Can't get GPIO %d\n", gpio);
    }

    return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////

static void free_handler(void)
{
	_gpio_handler* handler=&_ghandler;

	if(handler->gpio > 0)
	{
		if(handler->irq >= 0)
		{
			free_irq(handler->irq, (void*)handler);
			handler->irq=-1;
		}

		gpio_free(handler->gpio);
		handler->gpio=-1;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////

static int add_handler(int gpio)
{
	_gpio_handler* handler=&_ghandler;

	if(handler->gpio != gpio)
	{
		int irq=add_irq(gpio, handler);

		if(irq < 0)
		{
			free_handler();
			return -1;
		}

		handler->gpio=gpio;
		handler->irq=irq;

		return 0;
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////

static ssize_t run_command(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
	char buffer[512];
	char line[20];
	char* in_pos=NULL;
	char* end=NULL;
	char* out_pos=NULL;

	int gpio=-1;
	pid_t pid=0;

	if(count > 512)
		return -EINVAL;	//	file is too big

	copy_from_user(buffer, buf, count);
	buffer[count]=0;

	debug("Command is found (%u bytes length):\n%s\n",count,buffer);

	in_pos=buffer;
	end=in_pos+count-1;

	while(in_pos < end)
	{
		gpio=-1;

		while((in_pos < end) && is_space(*in_pos)) ++in_pos;	//	skip whitespace
		if(in_pos >= end) break;

		out_pos=line;
		while((in_pos < end) && is_digit(*in_pos)) *out_pos++=*in_pos++;
		*out_pos=0;

		if(is_digit(line[0]))
		{
			sscanf(line, "%d", &gpio);
		}
		else
		{
			printk(KERN_INFO "Can't read GPIO number.\n");
			break;
		}

		while((in_pos < end) && is_space(*in_pos)) ++in_pos;	//	skip whitespace

		out_pos=line;
		while((in_pos < end) && is_digit(*in_pos)) *out_pos++=*in_pos++;
		*out_pos=0;

		if(is_digit(line[0]))
		{
			sscanf(line, "%u", &pid);
		}

		add_handler(gpio);

		_ghandler.counter=0;
		gpio_direction_output(gpio,0);
		__raw_writel(1 << gpio, gpio_cleardataout_addr);
		udelay(2000);
		__raw_writel(1 << gpio, gpio_setdataout_addr);
		udelay(20);
		gpio_direction_input(gpio);

		udelay(100000); // 100 ms sleep

		free_handler();

		debug("Total %d values:\n",_ghandler.counter);
//		printk("Total: %d\n", _ghandler.counter);
		
		if(_ghandler.counter == 43)
		{
			int b=0;

			int data[5] = { 0,0,0,0,0 };
			int octet=0;

			int i=2; // skip first two pulses
			
			for(; i < _ghandler.counter - 1; ++i)
			{				
				if(_ghandler.value[i])
				{
//					printk("1");
					if(octet < 5)
					{
						data[octet]|=1 << (7-(b % 8));
					}
				}
				else
				{
//					printk("0");
				}

				if((++b % 8) == 0)
				{
					if(octet < 5)
					{
//						printk("=%d",data[octet]);
					}

					++octet;

					if(i < (_ghandler.counter-1))
					{
//						printk(" ");
					}
				}
			}
			printk("\n");

			int type=((data[1] == 0) && (data[3] == 0))?DHT11:DHT22;

			bool isOK=false;

			debug("type: %d\n",type);

			if(	(((data[0]+data[1]+data[2]+data[3]) & 0xff) == data[4]) &&
				(data[0] || data[1] || data[2] || data[3]))
			{
				isOK=true;
			}

			if(isOK)
			{
				if(pid != 0)
				{
					struct siginfo info;
					struct task_struct* ts=NULL;
					unsigned short t=0;
					unsigned short h=0;

					if(type == DHT11)
					{
						t=data[2]*10;
						h=data[0]*10;
					}
					else
					{
						t=((data[2] & 0x7f)*256+data[3]);
						h=data[0]*256+data[1];

						if(data[2] & 0x80) // temperature < 0
						{
							t|=0x8000;
						}
					}

					/* send the signal */
					memset(&info, 0, sizeof(struct siginfo));
					info.si_signo = SIG_DHT_IRQ;
					info.si_code = SI_QUEUE;	// this is bit of a trickery: SI_QUEUE is normally used by sigqueue from user space,
												// and kernel space should use SI_KERNEL. But if SI_KERNEL is used the real_time data
												// is not delivered to the user space signal handler function.

					info.si_int=(h << 16) | t;

					rcu_read_lock();
					ts=pid_task(find_vpid(pid), PIDTYPE_PID);
					rcu_read_unlock();

					if(ts)
					{
						send_sig_info(SIG_DHT_IRQ, &info, ts);    //send the signal
						debug("Signal sent to PID %u with parameter 0x%X\n",pid,info.si_int);
					}
					else
					{
						debug("Process with PID %u is not found.\n",pid);
					}
				}
				else
				{
					//just print results
					if(type == DHT11)
					{
						const char* format="T:%d\tH:%d%%\n";
						printk(format, data[2],data[0]);
					}
					else
					{
						const char* format="T:%d.%1d\tH:%d.%1d%%\n";
						int t=((data[2] & 0x7f)*256+data[3]);
						int h=data[0]*256+data[1];

						if(data[2] & 0x80)
						{
							t=-t;
						}

						printk(format, t/10,t%10, h/10,h%10);
					}
				}
				break;
			}
		}

		if(pid != 0)
		{
			struct siginfo info;
			struct task_struct* ts=NULL;

			/* send the signal */
			memset(&info, 0, sizeof(struct siginfo));
			info.si_signo = SIG_DHT_IRQ;
			info.si_code = SI_QUEUE;	// this is bit of a trickery: SI_QUEUE is normally used by sigqueue from user space,
							// and kernel space should use SI_KERNEL. But if SI_KERNEL is used the real_time data
							// is not delivered to the user space signal handler function.

			info.si_int = 0;	//	means 'error'

			rcu_read_lock();
			ts=pid_task(find_vpid(pid), PIDTYPE_PID);
			rcu_read_unlock();

			if(ts)
			{
				send_sig_info(SIG_DHT_IRQ, &info, ts);    //send the signal
				debug("Error sent to PID %u\n",pid);
			}
			else
			{
				debug("Error, but process with PID %u is not found.\n",pid);
			}
		}
		else
		{
			printk(KERN_INFO "Error.\n");
		}

		break;
	}

	return count;
}

////////////////////////////////////////////////////////////////////////////////////////////

static const struct file_operations irq_fops = {
//	.read = show_handlers,
	.write = run_command,
};

////////////////////////////////////////////////////////////////////////////////////////////

static int __init mymodule_init(void)
{
	gpio_addr = ioremap_nocache(AR71XX_GPIO_BASE, AR71XX_GPIO_SIZE);
    gpio_readdata_addr     = gpio_addr + GPIO_OFFS_READ;
    gpio_setdataout_addr   = gpio_addr + GPIO_OFFS_SET;
    gpio_cleardataout_addr = gpio_addr + GPIO_OFFS_CLEAR;

	_ghandler.gpio=-1;
	_ghandler.irq=-1;
	_ghandler.counter=-1;

	in_file=debugfs_create_file(FILE_NAME, 0666, NULL, NULL, &irq_fops);

	printk(KERN_INFO "Waiting for commands in file /sys/kernel/debug/" FILE_NAME ".\n");

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////

static void __exit mymodule_exit(void)
{
	free_handler();
	debugfs_remove(in_file);
	return;
}

////////////////////////////////////////////////////////////////////////////////////////////

module_init(mymodule_init);
module_exit(mymodule_exit);

////////////////////////////////////////////////////////////////////////////////////////////

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Black Swift team");

////////////////////////////////////////////////////////////////////////////////////////////
