#include <sbunix/sbunix.h>
#include <sbunix/string.h>
#include <sbunix/mm/physmem.h>
#include <sbunix/mm/page_alloc.h>


/**
* Deals with bootstrapping physical memory management.
*
* The pzones are stored in a static array (of length PZONE_MAX_NUM).
* The steps to initialize physical memory:
*   1. pzone_new() -- with increasing addresses
*   2. pzone_remove() -- with ranges that are not usable (optional)
*      (currently for the kernel's memory, physbase to physfree)
*   3. physmem_init() -- creates a ppage{} for each phys page.
*/


/**
* Array of pzone{}'s
* To simplify allocation of zones on startup this is a static array for now.
*/
#define PZONE_MAX_NUM 10
static size_t pzone_num = 0; /* Number of pzones (and first free slot) */
static struct pzone pzones[PZONE_MAX_NUM];

/* Private functions. */
static void _ppage_new(struct ppage *base, size_t nppages);
static void _pzone_fill_entry(size_t i, uint64_t startpage, uint64_t endpage, uint32_t zflags);
void _pzone_ppages_init(struct pzone *pzone);
void _create_free_page_list(struct pzone *base);


/**
* Create an entry. Caller MUST align startpage/endpage to PAGE_SIZE.
*/
void _pzone_fill_entry(size_t i, uint64_t startpage, uint64_t endpage, uint32_t zflags) {
    pzones[i].zflags = zflags;
    pzones[i].start  = startpage;
    pzones[i].end    = endpage;
    pzones[i].ppages = NULL;
}

/**
* Return the pzone{} which contains the physical address of kvirtpg.
* @kvirtpg: kernel virtual page address
*/
struct pzone* pzone_find(uint64_t kvirtpg) {
    size_t i = 0;

    for(; i < pzone_num; i++) {
        if(pzones[i].start <= kvirtpg && kvirtpg < pzones[i].end)
            return &pzones[i];
    }

    return NULL;
}

/**
* Inform the OS about a range of physical memory.
* Calls to pzone_new() MUST be in increasing order of zones and
* MUST not overlap.
* @startpage:   physical start addr of zone.
* @endpage:     physical first addr after end of zone.
* @zflags:      zflags for new pzone.
*/
void pzone_new(uint64_t startpage, uint64_t endpage, uint32_t zflags) {
    /* todo dynamic pzone creation */
    if(pzone_num >= PZONE_MAX_NUM)
        return;

    startpage = kphys_to_virt(startpage);
    endpage = kphys_to_virt(endpage);
    startpage = ALIGN_UP(startpage, PAGE_SIZE);
    endpage = ALIGN_DOWN(endpage, PAGE_SIZE);

    if(0 == startpage) { /* Disallow page 0 access */
        startpage += PAGE_SIZE;
    }

    if(endpage <= startpage)
        return;

    /* Create new zone */
    _pzone_fill_entry(pzone_num, startpage, endpage, zflags);

    pzone_num++;
}

/**
* Use to remove or split a pzone (e.g. kernel's memory region).
* If this region is currently a pzone, remove or split it up.
* @startpage:   physical start addr of zone.
* @endpage:     physical first addr after end of zone.
* @return:      ptr to list of pzones or NULL.
*/
void pzone_remove(uint64_t startpage, uint64_t endpage) {
    size_t i = 0;

    startpage = kphys_to_virt(startpage);
    endpage = kphys_to_virt(endpage);
    startpage = ALIGN_DOWN(startpage, PAGE_SIZE);
    endpage = ALIGN_UP(endpage, PAGE_SIZE);
    /*debug("Removing 0x%lx-0x%lx\n", startpage, endpage);*/

    if(endpage <= startpage)
        return;

    /* For clarity there are 4 cases for the way the remove region can
     * overlap with the pzone's region.
     * And this code assumes the global pzones may be unordered! */
    while(i < pzone_num) {
        if(startpage <= pzones[i].start && pzones[i].end <= endpage) {
            /* rm whole region */
            /* Delete here, then move the rest of the array back 1 slot */
            memset((pzones+i), 0, sizeof(struct pzone));
            if(pzone_num > i + 1)
                memmove((pzones+i), (pzones+i+1), sizeof(struct pzone) * (pzone_num-i-1));
            pzone_num--;
            continue; /* don't inc i */

        } else if(startpage <= pzones[i].start && pzones[i].start < endpage) {
            /* rm beginning of pzone. */
            pzones[i].start = endpage;

        } else if(startpage < pzones[i].end && pzones[i].end <= endpage) {
            /* rm end of pzone. */
            pzones[i].end = startpage;

        } else if(pzones[i].start < startpage && endpage < pzones[i].end) {
            /* rm middle of pzone. */
            uint64_t oldend = pzones[i].end;
            pzones[i].end = startpage;

            if(pzone_num >= PZONE_MAX_NUM) {
                printk("Splitting region but pzone array full!\n");
                continue; /* We could recover better here */
            }
            /* Move the rest of the array forward 1 slot */
            if(pzone_num > i + 1)
                memmove((pzones+i+2), (pzones+i+1), sizeof(struct pzone) * (pzone_num-i-1));

            _pzone_fill_entry(i + 1, endpage, oldend, pzones[i].zflags);
            pzone_num++;
            i++; /* i should be inc'd twice */
        }
        i++;
    }
}

/**
* Fill array of ppage{}s
* @base:    start address of ppage array.
* @num_ppages: number of ppages in array base[].
*/
void _ppage_new(struct ppage *base, size_t num_ppages) {
    memset(base, 0, num_ppages * sizeof(struct ppage));
}

/**
* Try to allocate space for pzone->ppages in the first pages of zone.
*/
void _pzone_ppages_init(struct pzone *pz) {
    uint64_t nppages = PZONE_NUM_PAGES(pz);
    uint64_t needbytes = nppages * sizeof(struct ppage);
    uint64_t needpages = ALIGN_UP(needbytes, PAGE_SIZE) >> PAGE_SHIFT;

    /*debug("PZ%ld: [%lx-%lx] %ld pages\n", i, base[i].start, base[i].end, npages);
    debug("PZ%ld: needbytes %ld, needpages %ld.\n", i, needbytes, needpages);*/
    if(needpages >= nppages) {
        pz->zflags &= ~PZONE_USABLE; /* Can't use, no space for ppages. */
        return;
    }

    /* Put ppage array in first pages of the pzone */
    pz->ppages = (struct ppage*)pz->start;
    _ppage_new(pz->ppages, nppages);

    /* Bump up the start address, possibly wasting space. */
    pz->start = ALIGN_UP(pz->start + needbytes, PAGE_SIZE);
}

/**
* Initialize physical memory meta-data.
* @pzonehead:  obtained from a call to pzone_new()
*/
void physmem_init(void) {
    size_t i = 0;
    if(pzone_num == 0)
        kpanic("No physical zones?!?!\n");

    for(;i < pzone_num; i++) {
        _pzone_ppages_init(pzones + i);
        debug("pz%ld: 0x%lx-0x%lx %ld pgs\n", i, pzones[i].start, pzones[i].end, PZONE_NUM_PAGES(pzones + i));
    }

    _create_free_page_list(pzones);
}

/**
* Print known information on physical memory.
*/
void physmem_report(void) {
    size_t i = 0;
    uint64_t totpages = 0, npages;
    char *status;

    for(; i < pzone_num; i++) {
        npages = PZONE_NUM_PAGES(pzones + i);
        if(pzones[i].zflags & PZONE_USABLE) {
            totpages += npages;
            status = "";
        } else {
            status = " UNUSED";
        }
        printk("pz%ld:%s 0x%lx-0x%lx %ld pgs\n", i, status,
               pzones[i].start, pzones[i].end, npages);
    }
    printk("%ld usable ppages across %ld pzones\n", totpages, pzone_num);
}

/**
* Create free list of pages.
* @base: Array of pzone{}'s
*/
void _create_free_page_list(struct pzone *base) {
    size_t i;
    struct freepage *prev = NULL, *curr =NULL;

    if(!base)
        kpanic("No physical zones?!?!\n");

    /* First we find assign the free page head. */
    for(i = 0; i < pzone_num; i++) {
        if(!(base[i].zflags & PZONE_USABLE))
            continue;

        if(PZONE_NUM_PAGES(base + i) <= 0)
            continue;

        freepagehd.freepages = (struct freepage *) base[i].start;
        prev = freepagehd.freepages;
        debug("Setting freepagehd to %p\n", freepagehd.freepages);
        break;
    }

    if(!freepagehd.freepages)
        kpanic("No usable pages?!?!\n");

    /* Now we build the free list. */
    for(; i < pzone_num; i++) {
        if(!(base[i].zflags & PZONE_USABLE))
            continue;

        curr = (struct freepage*) base[i].start;
        /*debug("pz%ld add [%lx-%lx] to freelist.\n", i, base[i].start, base[i].end);*/
        for(;curr < (struct freepage*) base[i].end; prev = curr, curr++) {
            /* debug("%p->next == %p\n", prev, curr); */
            prev->next = curr;
            freepagehd.nfree++;
        }
    }
    prev->next = NULL;

    freepagehd.maxfree = freepagehd.nfree;
    debug("%ld free pages total.\n", freepagehd.nfree);
}

/**
* Mark a kernel virtual address's page's ppage{} as used.
* return: -1 on error, 0 on success
*/
int ppage_mark_used(uint64_t kvirt_addr) {
    struct pzone* pz = pzone_find(kvirt_addr);
    size_t ppagei;
    if(!pz)
        return -1;

    ppagei = (kvirt_addr - pz->start) >> PAGE_SHIFT;

    /* If page already says USED then error */
    if(pz->ppages[ppagei].pflags & PPAGE_USED)
        return -1;

    /* Mark as USED */
    pz->ppages[ppagei].pflags &= PPAGE_USED;

    return 0;
}

/**
 * Return the corresponding ppage struct for this kernel virtual address.
 */
struct ppage *kvirt_to_ppage(uint64_t kvirt_addr) {
    struct pzone *pz = pzone_find(kvirt_addr);
    size_t ppagei;
    if(!pz)
        kpanic("No ppage struct for kernel virt addr %p\n", kvirt_addr);

    ppagei = (kvirt_addr - pz->start) >> PAGE_SHIFT;
    return &pz->ppages[ppagei];
}

/**
 * Return the corresponding ppage struct for this kernel physical address.
 */
struct ppage *kphys_to_ppage(uint64_t kphys_addr) {
    uint64_t kvirt_addr = kphys_to_virt(kphys_addr);
    struct pzone *pz = pzone_find(kvirt_addr);
    size_t ppagei;
    if(!pz)
        kpanic("No ppage struct for phys addr %p\n", kphys_addr);

    ppagei = (kvirt_addr - pz->start) >> PAGE_SHIFT;
    return &pz->ppages[ppagei];
}

/**
 * Increment the map count of this kernel physical page.
 */
void kphys_inc_mapcount(uint64_t kphys_addr) {
    struct ppage *ppage = kphys_to_ppage(kphys_addr);
    ppage->mapcount++;
}

/**
 * Increment the map count of this kernel virtual page.
 */
void kvirt_inc_mapcount(uint64_t kvirt_addr) {
    struct ppage *ppage = kvirt_to_ppage(kvirt_addr);
    ppage->mapcount++;
}
