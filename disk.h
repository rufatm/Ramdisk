#ifndef _DISK_H
#define _DISK_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h> /* error codes */
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/slab_def.h>


#define MY_CREATE	0
#define MY_MKDIR	1
#define MY_OPEN		2
#define MY_CLOSE	3
#define MY_READ		4
#define MY_WRITE	5
#define MY_LSEEK	6
#define MY_UNLINK	7
#define MY_READDIR	8

#define SIZE	1024 * 1024 * 2
#define BLOCK	256

#define MY_DIR	1
#define MY_REG	2

/* 
 * inode:
 * 	unsigned char type;
 * 	int size;
 * 	int[10];
 * 	unused;
 */
struct I_node{
	unsigned char data[64];
};

struct File_t{
	unsigned char data[BLOCK];
};

struct File_Table{
	int pos;
	int index;
	struct File_Table *next;
};

struct Proc_Files{
	pid_t pid;
	struct File_Table *data_start;
	struct File_Table *data_cur;
	struct Proc_Files *next;
};

static int fileops(struct inode *inode, struct file *file,
			       unsigned int cmd, unsigned long arg);

static void init_disk();

void add_open_file(pid_t id, int fd);

int close(pid_t id, int fd);

void setbit(int block_num);

void clearbit(int block_num);

#endif
