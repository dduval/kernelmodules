/*
 * By Dominic Duval, Red Hat Inc.
 * dduval@redhat.com
 * 
 * Fragments physical memory by allocating chunks 
 * of 2M at 2M intervals.
 *
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>

LIST_HEAD (frag_list);
int order=9;

struct fragnode 
{
    struct list_head list;
    void * nodeaddr;
};

static int __init frag_init(void)
{
	int size=0, a=0;
	void *cur;
	struct fragnode *thisnode;
	struct list_head *tmp; 
	struct list_head *list; 


	printk("Fragmenting memory...");

	while (1) {

		// Get a chunk of pages
		if (!(cur = (void *)__get_free_pages(GFP_ATOMIC, order)))
			break;

		// Keep track of the memory area we just obtained
		thisnode = kmalloc (sizeof (struct fragnode), GFP_KERNEL);
		list_add (&thisnode->list, &frag_list);

		
		size += (1UL << order); // Just allocated 2M, keep track of it (in pages)
		printk("Got 2M at 0x%p, size=%d\n",cur, size);
			
		thisnode->nodeaddr = cur;
	}
		
	a=0;
	// This is where we get rid of half the memory areas we allocated
	list_for_each_safe (list, tmp, &frag_list) {
		a++;
		if (a % 2) {
			
			thisnode = list_entry (list, struct fragnode, list);
			printk("Freeing at %p, order %d...",thisnode->nodeaddr,order);	
			free_pages((unsigned long)(thisnode->nodeaddr),order);
			list_del(&thisnode->list);
			kfree(thisnode);
		}
	}

	printk("Done! Touched %d pages total\n", size);
	return 0;
}

static void __exit frag_exit(void)
{
	struct list_head *list; 
	struct list_head *tmp; 
	printk("Defragmenting memory...\n");

	if (list_empty (&frag_list))
	        return ;

	// Free all the pages we obtained previously and get rid of the corresponding list entries
	list_for_each_safe (list, tmp, &frag_list) {
		struct fragnode *thisnode = list_entry (list, struct fragnode, list);
		free_pages((unsigned long)(thisnode->nodeaddr),order);
		list_del(&thisnode->list);
		kfree(thisnode);
	}
	printk("Done!\n");
}

module_init(frag_init);
module_exit(frag_exit);
MODULE_AUTHOR("Dominic Duval");
MODULE_DESCRIPTION("Memory Fragmenter");
MODULE_LICENSE("GPL v2");
