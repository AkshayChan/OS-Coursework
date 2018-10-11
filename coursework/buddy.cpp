/*
 * Buddy Page Allocation Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (2)
 */

/*
 * STUDENT NUMBER: s1558717
 */
#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

#define MAX_ORDER	17

/**
 * A buddy page allocation algorithm.
 */
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
private:
	/**
	 * Returns the number of pages that comprise a 'block', in a given order.
	 * @param order The order to base the calculation off of.
	 * @return Returns the number of pages in a block, in the order.
	 */
	static inline constexpr uint64_t pages_per_block(int order)
	{
		/* The number of pages per block in a given order is simply 1, shifted left by the order number.
		 * For example, in order-2, there are (1 << 2) == 4 pages in each block.
		 */
		return (1 << order);
	}
	
	/**
	 * Returns TRUE if the supplied page descriptor is correctly aligned for the 
	 * given order.  Returns FALSE otherwise.
	 * @param pgd The page descriptor to test alignment for.
	 * @param order The order to use for calculations.
	 */
	static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
	{
		// Calculate the page-frame-number for the page descriptor, and return TRUE if
		// it divides evenly into the number pages in a block of the given order.
		return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
	}
	
	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	 * to the left or the right of PGD, in the given order.
	 * @param pgd The page descriptor to find the buddy for.
	 * @param order The order in which the page descriptor lives.
	 * @return Returns the buddy of the given page descriptor, in the given order.
	 */
	PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
	{
		// (1) Make sure 'order' is within range
		if (order >= MAX_ORDER) {
			return NULL;
		}

		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order)) {
			return NULL;
		}
				
		// (3) Calculate the page-frame-number of the buddy of this page.
		// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
		// * If it's not aligned, then the buddy must be the previous block in THIS order.
		uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ?
			sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) : 
			sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);
		
		// (4) Return the page descriptor associated with the buddy page-frame-number.
		return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
	}
	
	/**
	 * Inserts a block into the free list of the given order.  The block is inserted in ascending order.
	 * @param pgd The page descriptor of the block to insert.
	 * @param order The order in which to insert the block.
	 * @return Returns the slot (i.e. a pointer to the pointer that points to the block) that the block
	 * was inserted into.
	 */
	PageDescriptor **insert_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, find the slot in which the page descriptor
		// should be inserted.
		PageDescriptor **slot = &_free_areas[order];
		
		// Iterate whilst there is a slot, and whilst the page descriptor pointer is numerically
		// greater than what the slot is pointing to.
		while (*slot && pgd > *slot) {
			slot = &(*slot)->next_free;
		}
		
		// Insert the page descriptor into the linked list.
		pgd->next_free = *slot;
		*slot = pgd;
		
		// Return the insert point (i.e. slot)
		return slot;
	}
	
	/**
	 * Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
	 * the system will panic.
	 * @param pgd The page descriptor of the block to remove.
	 * @param order The order in which to remove the block from.
	 */
	void remove_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, iterate until the block has been located in the linked-list.
		PageDescriptor **slot = &_free_areas[order];
		while (*slot && pgd != *slot) {
			slot = &(*slot)->next_free;
		}

		// Make sure the block actually exists.  Panic the system if it does not.
		assert(*slot == pgd);
		
		// Remove the block from the free list.
		*slot = pgd->next_free;
		pgd->next_free = NULL;
	}
	
	/**
	 * Given a pointer to a block of free memory in the order "source_order", this function will
	 * split the block in half, and insert it into the order below.
	 * @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	 * @param source_order The order in which the block of free memory exists.  Naturally,
	 * the split will insert the two new blocks into the order below.
	 * @return Returns the left-hand-side of the new block.
	 */
	PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order)
	{
		// Make sure there is an incoming pointer.
		assert(*block_pointer);
		
		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

		
		// TODO: Implement this function
		//We cannot split blocks of zero order, returns the original block
		if (source_order == 0)  
			return *block_pointer; 
		
		//Declare its buddy block pointer for the lower order to the original
		//pointer plus the pages per block in the lower order
		//We know the buddy in the lower order is the next block 
		PageDescriptor *original = *block_pointer;
		PageDescriptor *buddy_in_lower = *block_pointer + pages_per_block(source_order-1); 
		
		//Removing the block from the higher order and adding two blocks to the lower order
		remove_block(original, source_order);
		insert_block(original, source_order-1);
		insert_block(buddy_in_lower, source_order-1);
		
		assert(is_correct_alignment_for_order(original, source_order-1));
		assert(is_correct_alignment_for_order(buddy_in_lower, source_order-1));		
		
		return *block_pointer;		
	}
	
	/**
	 * Takes a block in the given source order, and merges it (and it's buddy) into the next order.
	 * This function assumes both the source block and the buddy block are in the free list for the
	 * source order.  If they aren't this function will panic the system.
	 * @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	 * @param source_order The order in which the pair of blocks live.
	 * @return Returns the new slot that points to the merged block.
	 */
	PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order)
	{
		assert(*block_pointer);
		
		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

		// TODO: Implement this function
		//We cannot merge the block in the top order
		if(source_order == MAX_ORDER-1) 
			return block_pointer;
		
		//We don't know if the buddy is the next block or previous block in this order
		PageDescriptor *original = *block_pointer;
		PageDescriptor *buddy_in_higher = buddy_of(*block_pointer, source_order);
		
		//removing blocks from the lower order 
		remove_block(original, source_order);
		remove_block(buddy_in_higher, source_order);

		PageDescriptor **slot;

		//Checking to see which buddy aligns with the upper order after merging 
		if (is_correct_alignment_for_order(original, source_order+1))		
			slot = insert_block(original, source_order + 1);
		else
			slot = insert_block(buddy_in_higher, source_order + 1);

		return slot;	
	}
	
public:
	/**
	 * Constructs a new instance of the Buddy Page Allocator.
	 */
	BuddyPageAllocator() {
		// Iterate over each free area, and clear it.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			_free_areas[i] = NULL;
		}
	}
	
	/**
	 * Allocates 2^order number of contiguous pages
	 * @param order The power of two, of the number of contiguous pages to allocate.
	 * @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	 * allocation failed.
	 */
	PageDescriptor *alloc_pages(int order) override
	{
		//Here we find the highest order which has a block and is non empty to start with, making sure we
		//don't cross maximum order 
		int x = order;
		while( _free_areas[x] == NULL && x < MAX_ORDER) {
			x++;
		}

		PageDescriptor *block_pointer = _free_areas[x];
		
		//Till we don't reach our required order containing the block of 2^order pages
		//Since we're allocating anything, don't need to check for buddies 
		for(int j = x; j > order; j--) {
			block_pointer = split_block(&block_pointer, j);
		}
		
		//Remove the block of contiguous pages as it has been allocated
		remove_block(block_pointer, order);
		return block_pointer;	 	  		
	}
	
	/**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
	void free_pages(PageDescriptor *pgd, int order) override
	{
		// Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		assert(is_correct_alignment_for_order(pgd, order));
		

		//We first insert the block back into free memory and get its slot
		PageDescriptor **slot = insert_block(pgd, order);

		//Then we declare it's buddy and the pointer to the first block in that order
		PageDescriptor *buddy_in_order = buddy_of(*slot, order);
		PageDescriptor *block_pointer = _free_areas[order];	
 		
		//We check instead if the block pointer actually exists
		//Block pointer can be next pointer in loop or pointer to first block in next order 
		//Also make sure the order we're checking is less than the maximum order
		for(int x = order; block_pointer && x < MAX_ORDER;) {
			//If we can't find the blocks buddy			
			if (buddy_in_order != block_pointer) {
				
				//Is the next pointer the buddy? Keep looking
				block_pointer = block_pointer->next_free;
				
				}
			else {	
				//merged with buddy accrodingly in that order and slot updated, increment to next order
				slot = merge_block(slot, x);
				x++;

				//Reassign buddy and block pointer according to higher order
				buddy_in_order = buddy_of(*slot, x);
				block_pointer = _free_areas[x];
			}
		}
	}
	
	/**
	 * Reserves a specific page, so that it cannot be allocated.
	 * @param pgd The page descriptor of the page to reserve.
	 * @return Returns TRUE if the reservation was successful, FALSE otherwise.
	 */
	bool reserve_page(PageDescriptor *pgd)
	{
		PageDescriptor *pageblock;
		bool flag = false;

		//Iterate through all orders
		for (int x = 0; flag == false && x < MAX_ORDER; ) {
			PageDescriptor *block_pointer = _free_areas[x];

			//Till we haven't reached a NULL free memory or last block for order
			while(block_pointer != NULL) {

			//If the block "contains" the page
				if (block_pointer <= pgd && pgd < (block_pointer + pages_per_block(x))) {
					pageblock = block_pointer;

					//Keep splitting until we get down to page
					for (int i = x; i>0;) {
						pageblock = split_block(&pageblock, i);
						i--;

						PageDescriptor *pagebuddy = buddy_of(pageblock, i);
						//Making sure page not gone into buddy
						if(pagebuddy <= pgd && pgd < (pagebuddy + pages_per_block(i))) {
							pageblock = pagebuddy;		
						}
					}
					//If we've come into this loop, we've found the page in a block, so break. 
					flag = true;
					break;
				}
				else {
					//Go to the next block
					block_pointer = block_pointer->next_free;
				}
			}
			x++;
		}
		//Found the page in a block
		if (flag == true) {
			remove_block(pgd, 0);
			return true;
		}
		else {
			return false;
		}
	}
	
	/**
	 * Initialises the allocation algorithm.
	 * @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	 */
	bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override
	{
		mm_log.messagef(LogLevel::DEBUG, "Buddy Allocator Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors);
		
		// TODO: Initialise the free area linked list for the maximum order
		// to initialise the allocation algorithm.
		
		int order = MAX_ORDER - 1;

		//Blocksize in the last order
		int blocksize = pages_per_block(order);

		//Number of blocks in the maximum order
		int blocks = nr_page_descriptors/blocksize;

		PageDescriptor *build = page_descriptors;
	
		//For the maximum order we make the pointer equal to the pointer to the first page descriptor
		//then build the remaining list
		_free_areas[order] = build;
		
		//Building the list		
		int x = 0;
		while(x < blocks) {
	
			//Joining build to the next block in our order
			build->next_free = build + blocksize;
						
			//Moving onto the next pointer
			build = build->next_free; 
	
			x++;
		}

		//The final block is linked to nothing
		build->next_free = NULL;

		return true;
		
	}

	/**
	 * Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "buddy"; }
	
	/**
	 * Dumps out the current state of the buddy system
	 */
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");
		
		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);
						
			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg) {
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}
			
			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
		}
	}

	
private:
	PageDescriptor *_free_areas[MAX_ORDER];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
 * Allocation algorithm registration framework
 */
RegisterPageAllocator(BuddyPageAllocator);
