#include "disk.h"

MODULE_LICENSE("GPL");


/* superblock:
 * 	int free_block_number;
 * 	short free_inode_number;
 * 	unused;
 */

/* RAM disk */
unsigned char *RAM;
/* superblock */
unsigned char *Su_blk;
/* inode blocks */
struct I_node *Inode_blk;
/* bitmap */
unsigned char *M_blk;
/* file blocks */
struct File_t *F_blk;


struct Proc_Files *proc_root = NULL, *proc_cur = NULL;


/* used by OPEN op */
void add_open_file(pid_t id, int fd)
{
	if(proc_cur == NULL){	/* first process */
		proc_cur = (struct Proc_Files *)kmalloc(sizeof(struct Proc_Files), GFP_KERNEL);
		proc_root = proc_cur;
		proc_cur->pid = id;
		proc_cur->data_cur = (struct File_Table *)kmalloc(sizeof(struct File_Table), GFP_KERNEL);
		proc_cur->data_start = proc_cur->data_cur;
		proc_cur->data_cur->pos = 0;
		proc_cur->data_cur->index = fd;
		proc_cur->data_cur->next = NULL;
		proc_cur->next = NULL;
	}
	else{
		struct Proc_Files *temp = proc_root;
		while(temp != NULL){
			if(temp->pid == id){	/* existed before */
				temp->data_cur->next = (struct File_Table *)kmalloc(sizeof(struct File_Table), GFP_KERNEL);
				temp->data_cur = temp->data_cur->next;
				temp->data_cur->pos = 0;
				temp->data_cur->index = fd;
				temp->data_cur->next = NULL;
				break;
			}
			else temp = temp->next;
		}
		
		if(temp == NULL){	/* new process */
			proc_cur->next = (struct Proc_Files *)kmalloc(sizeof(struct Proc_Files), GFP_KERNEL);
			proc_cur = proc_cur->next;
			proc_cur->pid = id;
			proc_cur->data_cur = (struct File_Table *)kmalloc(sizeof(struct File_Table), GFP_KERNEL);
			proc_cur->data_start = proc_cur->data_cur;
			proc_cur->data_cur->pos = 0;
			proc_cur->data_cur->index = fd;
			proc_cur->data_cur->next = NULL;
			proc_cur->next = NULL;
		}
	}
}


/* used by CLOSE op, return -1 if fd refers to a file not opened by calling process, return 0 on success */
int close(pid_t id, int fd)
{
	struct Proc_Files *temp = proc_root, *p_pre = NULL;
	while(temp != NULL){
		if(temp->pid == id){
			struct File_Table *move = temp->data_start, *f_pre = NULL;
			while(move != NULL){
				if(move->index == fd){	/* found fd */
					/* delete file descriptor record */
					if(f_pre == NULL) temp->data_start = move->next;
					else f_pre->next = move->next;
					if(move->next == NULL) temp->data_cur = f_pre;
					kfree(move);

					if(temp->data_start == NULL){
						/* no open file, delete process record */
						if(p_pre == NULL) proc_root = temp->next;
						else p_pre->next = temp->next;
						if(temp->next == NULL) proc_cur = p_pre;
						kfree(temp);
					}

					return 0;
				}
				f_pre = move;
				move = move->next;
			}

			/* fd not opened by id process */
			return -1;
		}
		p_pre = temp;
		temp = temp->next;
	}

	/* process record not found */
	return -1;
}


/* set corresponding bit in the bitmap; block_num starts from 0 */
void setbit(int block_num)
{
	int byte_pos = block_num / 8, bit_pos = block_num % 8;
	switch(bit_pos){
	
	case 0: M_blk[byte_pos] = M_blk[byte_pos] | 0x01; break;
	case 1: M_blk[byte_pos] = M_blk[byte_pos] | 0x02; break;
	case 2: M_blk[byte_pos] = M_blk[byte_pos] | 0x04; break;
	case 3: M_blk[byte_pos] = M_blk[byte_pos] | 0x08; break;
	case 4: M_blk[byte_pos] = M_blk[byte_pos] | 0x10; break;
	case 5: M_blk[byte_pos] = M_blk[byte_pos] | 0x20; break;
	case 6: M_blk[byte_pos] = M_blk[byte_pos] | 0x40; break;
	case 7: M_blk[byte_pos] = M_blk[byte_pos] | 0x80; break;

	}
}


/* clear corresponding bit in the bitmap; block_num starts from 0 */
void clearbit(int block_num)
{
	int byte_pos = block_num / 8, bit_pos = block_num % 8;
	switch(bit_pos){
	
	case 0: M_blk[byte_pos] = M_blk[byte_pos] & 0xFE; break;
	case 1: M_blk[byte_pos] = M_blk[byte_pos] & 0xFD; break;
	case 2: M_blk[byte_pos] = M_blk[byte_pos] & 0xFB; break;
	case 3: M_blk[byte_pos] = M_blk[byte_pos] & 0xF7; break;
	case 4: M_blk[byte_pos] = M_blk[byte_pos] & 0xEF; break;
	case 5: M_blk[byte_pos] = M_blk[byte_pos] & 0xDF; break;
	case 6: M_blk[byte_pos] = M_blk[byte_pos] & 0xBF; break;
	case 7: M_blk[byte_pos] = M_blk[byte_pos] & 0x7F; break;

	}
}


static struct file_operations pseudo_dev_proc_operations;

static struct proc_dir_entry *proc_entry;

static int __init initialization_routine(void) {
	printk("<1> Loading module\n");

	pseudo_dev_proc_operations.ioctl = fileops;

	/* Start create proc entry */
	proc_entry = create_proc_entry("ramdisk", 0444, NULL);
	if(!proc_entry)
	{
		printk("<1> Error creating /proc entry.\n");
		return 1;
	}

	proc_entry->proc_fops = &pseudo_dev_proc_operations;

	/* initialize disk */
	init_disk();

	return 0;
}


static void __exit cleanup_routine(void) {

	printk("<1> Dumping module\n");
	remove_proc_entry("ramdisk", NULL);

	vfree(RAM);

	return;
}


void init_disk()
{
	/* 4 bytes block number */
	union{
		unsigned int num;
		unsigned char data[sizeof(unsigned int)];	
	}temp;

	/* 2 bytes inode number */
	union{
		unsigned short num;
		unsigned char data[sizeof(unsigned short)];
	}temp2;

	/* bitmap size for SIZE of RAM disk in block unit */
	int map_size = SIZE / (BLOCK * 256 * 8);

	RAM = (unsigned char *)vmalloc(SIZE);
	Su_blk = RAM;
	Inode_blk = (struct I_node *)&RAM[BLOCK];
	M_blk = &RAM[257 * BLOCK];
	F_blk = (struct File_t *)&RAM[(257 + map_size) * BLOCK];
	
	/* fill in superblock */
	/* free blocks */
	temp.num = SIZE / BLOCK - 257 - map_size;
	int i;
	for(i = 0; i < sizeof(int); i++){
		Su_blk[i] = temp.data[i];
	}

	/* free inodes */
	temp2.num = (BLOCK / 64) * 256 - 1;
	for(i = 0; i < sizeof(short); i++){
		Su_blk[i + sizeof(int)] = temp.data[i];
	}

	/* root file in inode blocks */
	/* type: dir */
	Inode_blk[0].data[0] = MY_DIR;
	/* size: 0 */
	for(i = 0; i < sizeof(int); i++){
		Inode_blk[0].data[1 + i] = 0;
	}
	/* location: no entry */
	int j;
	for(i = 0; i < 10; i++){
		for(j = 0; j < sizeof(int); j++)
			Inode_blk[0].data[1 + sizeof(int) + (sizeof(int) * i) + j] = 0;
	}

	/* bitmap */
	for(j = 0; j < map_size; j++){
		for(i = 0; i < BLOCK; i++)
			M_blk[j * BLOCK + i] = 0;
	}
}


/*
 * ioctl() entry point:
 * return value is determined by different ops, return -2 if op code is wrong
 */
int fileops(struct inode *inode, struct file *file,
			       unsigned int cmd, unsigned long arg)
{
	switch(cmd){

	case MY_CREATE: printk("create\n"); break;
	case MY_MKDIR: printk("mkdir\n"); break;
	case MY_OPEN: printk("open\n"); break;
	case MY_CLOSE: printk("close\n"); break;
	case MY_READ: printk("read\n"); break;
	case MY_WRITE: printk("write\n"); break;
	case MY_LSEEK: printk("lseek\n"); break;
	case MY_UNLINK: printk("unlink\n"); break;
	case MY_READDIR: printk("readdir\n"); break;
	default: return -2;	/* illegal command */

	}
}


module_init(initialization_routine); 
module_exit(cleanup_routine); 


