/*
 * By Dominic Duval, Red Hat Inc.
 * dduval@redhat.com
 * 
 * Allocates 4M memory areas at a high priority
 *
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>

LIST_HEAD (nofail_list);
int order=10;

struct nofailnode 
{
    struct list_head list;
    void * nodeaddr;
};

static int __init nofail_init(void)
{
	int size=0;
	void *cur;
	struct nofailnode *thisnode;


	printk("Fragmenting memory...");

	while (1) {

		if (!(cur = (void *)__get_free_pages(GFP_ATOMIC | __GFP_NOFAIL , order)))
			break;

		thisnode = kmalloc (sizeof (struct nofailnode), GFP_KERNEL);
		list_add (&thisnode->list, &nofail_list);

		size += (1UL << order); // Just allocated 4, keep track of it (in pages)
		printk("Got 4M at 0x%p, size=%d\n",cur, size);
			
		thisnode->nodeaddr = cur;
	}
		
	printk("Done! Touched %d pages total\n", size);
	return 0;
}

static void __exit nofail_exit(void)
{
	struct list_head *list; 
	struct list_head *tmp; 
	printk("Denofailmenting memory...\n");

	if (list_empty (&nofail_list))
	        return ;

	list_for_each_safe (list, tmp, &nofail_list) {
		struct nofailnode *thisnode = list_entry (list, struct nofailnode, list);
		free_pages((unsigned long)(thisnode->nodeaddr),order);
		list_del(&thisnode->list);
		kfree(thisnode);
	}
	printk("Done!\n");
}

module_init(nofail_init);
module_exit(nofail_exit);
MODULE_AUTHOR("Dominic Duval");
MODULE_DESCRIPTION("High Order High Priority Memory Allocator");
MODULE_LICENSE("GPL v2");
