/*
 * By Dominic Duval, Red Hat Inc.
 * dduval@redhat.com
 * 
 * Allocates a reserve of 4M memory areas
 *
 * Default is 8 areas allocated.
 *
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/list.h>

#define PROCNODE "reservebuckets"
static struct proc_dir_entry *rb_proc;

LIST_HEAD(large_list);
int order = 10;
static int areas = 8;

struct largenode {
	struct list_head list;
	void *nodeaddr;
};

void adjust_reserve(int newreserve, int oldreserve)
{
	int size = 0, n;
	void *cur;
	struct list_head *list;
	struct list_head *tmp;
	struct largenode *thisnode;
	printk("Adjusting reserve from %d to %d\n", oldreserve,
	       newreserve);

	if (newreserve > oldreserve) {

		for (n = 0; n < (newreserve - oldreserve); n++) {

			if (!  (cur = (void *) __get_free_pages(GFP_KERNEL, order)))
				 return;

			thisnode = kmalloc(sizeof(struct largenode), GFP_KERNEL);
			list_add(&thisnode->list, &large_list);

			size += (1UL << order);
			areas++;
			printk ("Got order-%d area at 0x%p, size of reserve so far is %d pages.\n",
			     order, cur, areas*(1 << order) );
			thisnode->nodeaddr = cur;
		}


		printk ("Done! Allocated %d pages total out of %d requested.\n",
		     size,
		     (int) (1UL << order) * (newreserve - oldreserve));


	} else if (newreserve < oldreserve) {
	//Need to be careful not to remove areas we don't have in memory

		if (list_empty(&large_list)) {
			printk("List empty, exiting now.\n");
			return;
		}

		n=0;

		list_for_each_safe(list, tmp, &large_list) {
			struct largenode *thisnode = list_entry(list, struct largenode, list);
			n++;
			free_pages((unsigned long) (thisnode->nodeaddr),
				   order);
			printk("Freeing order-%d area at 0x%p.\n", order, thisnode->nodeaddr);
			list_del(&thisnode->list);
			kfree(thisnode);

			if ( ((oldreserve - n) == newreserve) || list_empty(&large_list)) {
				areas = newreserve;
				return;
			}
		}
		printk("Done!\n");
	}

}




static int
rb_proc_read(char *page, char **start, off_t off, int count,
	     int *eof, void *data)
{
	*eof = 1;
	return sprintf(page, "%d\n", areas);
}

static int
rb_proc_write(struct file *file, const char __user * buffer,
	      unsigned long count, void *data)
{
	char *str;
	int newareas;
	str = kmalloc((size_t) count, GFP_KERNEL);
	if (copy_from_user(str, buffer, count)) {
		kfree(str);
		return -EFAULT;
	}
	sscanf(str, "%d", &newareas);
	printk("New reserve has been set to %d\n", newareas);
	kfree(str);

	adjust_reserve(newareas, areas);
	
	return count;
}


static int __init large_init(void)
{
	int size = 0, n;
	void *cur;
	struct largenode *thisnode;


	printk("Trying to allocate memory reserve (%d x 4M). \n", areas);

	for (n = 0; n < areas; n++) {

		if (!(cur = (void *) __get_free_pages(GFP_KERNEL, order)))
			break;

		thisnode = kmalloc(sizeof(struct largenode), GFP_KERNEL);
		list_add(&thisnode->list, &large_list);

		size += (1UL << order);	// Just allocated 4, keep track of it (in pages)
		printk
		    ("Got order-%d area at 0x%p, size of reserve so far is %d pages.\n",
		     order, cur, size);

		thisnode->nodeaddr = cur;
	}
	

	printk("Done! Allocated %d pages total out of %d requested.\n",
	       size, (int) (1UL << order) * areas);

	areas = size / (1UL << order);

	rb_proc = create_proc_entry(PROCNODE, S_IRUGO | S_IWUSR, NULL);
	if (!rb_proc) {
		printk("I failed to make %s\n", PROCNODE);
		return 0;
	}
	printk("Created %s /proc/entry.\n", PROCNODE);
	rb_proc->read_proc = rb_proc_read;
	rb_proc->write_proc = rb_proc_write;
	rb_proc->owner = THIS_MODULE;


	return 0;
}

static void __exit large_exit(void)
{
	struct list_head *list;
	struct list_head *tmp;

	if (rb_proc) {
		remove_proc_entry(PROCNODE, NULL);
		printk("Removed %s /proc/entry.\n", PROCNODE);
	}


	printk("Freeing memory...\n");

	if (list_empty(&large_list)) {
		printk("List empty, exiting now.\n");
		return;
	}

	list_for_each_safe(list, tmp, &large_list) {
		struct largenode *thisnode =
		    list_entry(list, struct largenode, list);
		free_pages((unsigned long) (thisnode->nodeaddr), order);
		printk("Freeing order-%d area at 0x%p.\n", order,
		       thisnode->nodeaddr);
		list_del(&thisnode->list);
		kfree(thisnode);
	}
	printk("Done!\n");
}


module_init(large_init);
module_exit(large_exit);
module_param(areas, int, S_IRUGO);
MODULE_AUTHOR("Dominic Duval");
MODULE_DESCRIPTION("High Order Reserve Memory Allocator");
MODULE_LICENSE("GPL v2");
