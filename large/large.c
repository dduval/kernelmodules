/*
 * By Dominic Duval, Red Hat Inc.
 * dduval@redhat.com
 * 
 * Allocates 4M memory areas
 *
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>

LIST_HEAD (large_list);
int order=10;

struct largenode 
{
    struct list_head list;
    void * nodeaddr;
};

static int __init large_init(void)
{
	int size=0;
	void *cur;
	struct largenode *thisnode;


	printk("Fragmenting memory...");

	while (1) {

		if (!(cur = (void *)__get_free_pages(GFP_ATOMIC, order)))
			break;

		thisnode = kmalloc (sizeof (struct largenode), GFP_KERNEL);
		list_add (&thisnode->list, &large_list);

		size += (1UL << order); // Just allocated 4, keep track of it (in pages)
		printk("Got 4M at 0x%p, size=%d\n",cur, size);
			
		thisnode->nodeaddr = cur;
	}
		
	printk("Done! Touched %d pages total\n", size);
	return 0;
}

static void __exit large_exit(void)
{
	struct list_head *list; 
	struct list_head *tmp; 
	printk("Freeing memory...\n");

	if (list_empty (&large_list))
	        return ;

	list_for_each_safe (list, tmp, &large_list) {
		struct largenode *thisnode = list_entry (list, struct largenode, list);
		free_pages((unsigned long)(thisnode->nodeaddr),order);
		list_del(&thisnode->list);
		kfree(thisnode);
	}
	printk("Done!\n");
}

module_init(large_init);
module_exit(large_exit);
MODULE_AUTHOR("Dominic Duval");
MODULE_DESCRIPTION("High Order Memory Allocator");
MODULE_LICENSE("GPL v2");
