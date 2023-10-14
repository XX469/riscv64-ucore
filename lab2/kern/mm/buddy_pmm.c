#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>
#include <stdio.h>


struct buddy {
    size_t size; //sum pages
    size_t nodenum; //node nums
    size_t longest[40000]; //complete_binary_tree index and their pagenumbers
    size_t start_page_index[40000]; //first page index that belong to this node
};

struct buddy buddy_tree; //use a complete binary tree to manage pages
struct Page* pages_start; //allocate first page

free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)
// define tree nodes
#define LEFT_CHILD(index) ((index) * 2 + 1)
#define RIGHT_CHILD(index) ((index) * 2 + 2)
#define PARENT(index) ( ((index) + 1) / 2 - 1)
// define some useful function
#define IS_POWER_OF_2(x) (!((x)&((x)-1)))  //check if x is 2^n
#define MAX(a, b) ((a) > (b) ? (a) : (b))
static size_t CLOSEST_POWER_OF_2_BELOW(size_t n)
{
    size_t res = 1;
    if (!IS_POWER_OF_2(n)) {
        while (n) {
            n = n >> 1;
            res = res << 1;
        }
        return res>>1; 
    }
    else {
        return n;
    }
}
static size_t CLOSEST_POWER_OF_2_ABOVE(size_t n)
{
    size_t res = 1;
    if (!IS_POWER_OF_2(n)) {
        while (n) {
            n = n >> 1;
            res = res << 1;
        }
        return res; 
    }
    else {
        return n;
    }
}
static size_t find_alloc_index(size_t n) //find a node whose size==n
{
    if(buddy_tree.longest[0] < n) // too large
    {
        return -1;
    }
    int index=0;
    size_t curr_size;
    //DFS find index
    for(curr_size=buddy_tree.size;curr_size!=n;curr_size /= 2)
    {
        if(buddy_tree.longest[LEFT_CHILD(index)] >= n)
        {
            //struct Page* pl = pages_start + buddy_tree.start_page_index[LEFT_CHILD(index)];
            index = LEFT_CHILD(index);
        }
        else
        {
            index = RIGHT_CHILD(index);
        }
    }
    //update longest array
    buddy_tree.longest[index]=0;
    int i=index;
    while(i)
    {
        i=PARENT(i);
        buddy_tree.longest[i]=MAX(buddy_tree.longest[LEFT_CHILD(i)],buddy_tree.longest[RIGHT_CHILD(i)]);
    }
    return index;
}
static void print_tree_info(size_t n) //print node0~n and other info about buddy tree
{
    cprintf("tree size:%d\n node num:%d\n",buddy_tree.size,buddy_tree.nodenum);
    for(int i=0;i<n;i++)
    {
        cprintf("index:%d curr_size:%d page_index:%d\n",i,buddy_tree.longest[i],buddy_tree.start_page_index[i]);
    }
}
//===================================================================================
static void
buddy_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

static void
buddy_init_memmap(struct Page *base, size_t n) //init pages and buddy tree
{
    assert(n>0); //check whether page number>0
    size_t page_num = CLOSEST_POWER_OF_2_BELOW(n); //n = 32256 -> page num = 16384(2^14)
    struct Page *p = base;
    for (; p != base + page_num; p ++) { //init each page
        //assert(PageReserved(p)); // check whether this page is valid
        p->flags = p->property = 0; 
        set_page_ref(p, 0); //page->ref=0; because now p is free and no reference
    }
    base->property = page_num;  //base page set totalnum of block
    SetPageProperty(base);  //set base->flags->property bit to 1
    nr_free += page_num; //update num free blocks
    // insert this page to free_list
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
    pages_start = base;
    //init buddy tree
    buddy_tree.size=page_num;
    size_t node_num = 2*page_num-1;
    buddy_tree.nodenum=node_num;
    buddy_tree.longest[0]=page_num;
    buddy_tree.start_page_index[0]=0;
    for(int i=1;i<node_num;i++)
    {
        buddy_tree.longest[i] = buddy_tree.longest[PARENT(i)] / 2;
        if(i%2!=0)
        {
            buddy_tree.start_page_index[i] = buddy_tree.start_page_index[PARENT(i)];
        }
        else
        {
            buddy_tree.start_page_index[i] = buddy_tree.start_page_index[PARENT(i)] + buddy_tree.longest[i];
        }
    }
}

static struct Page *
buddy_alloc_pages(size_t n)
{
    assert(n>0);
    size_t alloc_size = CLOSEST_POWER_OF_2_ABOVE(n);
    size_t index = find_alloc_index(alloc_size);
    if(index==-1)
    {
        return NULL;
    }
    struct Page *p = pages_start + buddy_tree.start_page_index[index] ;
    p->property = alloc_size;
    ClearPageProperty(p);
    nr_free -= alloc_size;
    return p;
}

static void
buddy_free_pages(struct Page *base, size_t n)
{
    assert(n>0);
    size_t alloc_size = CLOSEST_POWER_OF_2_ABOVE(n);
    struct Page *p = base;
    for (; p != base + alloc_size; p ++) {
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = alloc_size;
    SetPageProperty(base);
    // find tree node index
    size_t curr_size;
    size_t index = 0;
    size_t page_i = base - pages_start;
    curr_size=buddy_tree.size;
    while(1)
    {
        if(curr_size < alloc_size)
        {
            return;
        }
        if((curr_size == alloc_size) && (buddy_tree.start_page_index[index]==page_i))
        {
            break;
        }
        else
        {
            if(buddy_tree.start_page_index[RIGHT_CHILD(index)] > page_i)
            {
                index = LEFT_CHILD(index);
                curr_size /= 2;
            }
            else
            {
                index = RIGHT_CHILD(index);
                curr_size /= 2;
            }
        }
    }
    if(buddy_tree.longest[index]!=0) //this node wasn't alloced before
    {
        return;
    }
    //update parents
    //cprintf("find index:%d\n",index);
    buddy_tree.longest[index]=alloc_size;
    size_t left_longest,right_longest;
    while(index)
    {
        index=PARENT(index);
        alloc_size*=2;
        left_longest = buddy_tree.longest[LEFT_CHILD(index)];
        right_longest = buddy_tree.longest[RIGHT_CHILD(index)];
        if(left_longest+right_longest==alloc_size)
        {
            buddy_tree.longest[index]=alloc_size;
        }
        else
        {
            buddy_tree.longest[index]=MAX(left_longest,right_longest);
        }
    }
    nr_free += alloc_size;
}

static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}

static void 
buddy_check(void)
{
    assert(alloc_pages(20000)==NULL);
    struct Page* p0;
    // 分配p0并检查分配的块是否正确
    p0 = alloc_pages(2000);
    assert(p0!=NULL && p0->property==2048);
    assert(buddy_tree.longest[7]==0 && buddy_tree.start_page_index[7] == p0 - pages_start);
    // 检查父节点是否更新
    assert(buddy_tree.longest[3]==2048 && buddy_tree.longest[1]==4096 && buddy_tree.longest[0]==8192);
    // 依次分配p1,p2,p3,p4
    struct Page* p1 = alloc_pages(8000);
    assert(p1 != NULL && p1->property == 8192);
    assert(buddy_tree.longest[2]==0 && buddy_tree.start_page_index[2] == p1-pages_start);
    assert(buddy_tree.longest[0]==4096);

    struct Page* p2 = alloc_pages(4000);
    assert(p2 != NULL && p2->property == 4096);
    assert(buddy_tree.longest[4]==0 && p2 - pages_start == 4096);
    assert(buddy_tree.longest[1]==2048 && buddy_tree.longest[0]==2048);

    struct Page* p3 = alloc_pages(4000);
    assert(p3==NULL);

    struct Page* p4 = alloc_pages(2000);
    assert(p4!=NULL && p4->property==2048);
    assert(buddy_tree.longest[8]==0 && p4 - pages_start == 2048);
    assert(buddy_tree.longest[3]==0 && buddy_tree.longest[1]==0 && buddy_tree.longest[0]==0);
    //此时内存状态：
    //| p0 | p4 |    p2     |           p1             |
    //|2048|2048|   4096    |          8192            |

    //释放p0,p4，前两块合并
    free_pages(p0,2000);
    free_pages(p4,2000);
    assert(buddy_tree.longest[7]==2048 && buddy_tree.longest[8]==2048);
    assert(buddy_tree.longest[3]==4096); //检查是否合并
    assert(buddy_tree.longest[1]==4096 && buddy_tree.longest[0]==4096);
    //此时内存状态：
    //|    空    |    p2     |           p1             |
    //|   4096   |   4096    |          8192            |
    
    //检查释放后能否重新分配p5
    struct Page* p5 = alloc_pages(3000);
    assert(p5 != NULL && p5->property==4096);
    assert(buddy_tree.longest[3]==0 && p5==pages_start);
    assert(buddy_tree.longest[1]==0 && buddy_tree.longest[0]==0);
    //此时内存状态：
    //|    p5    |    p2     |           p1             |
    //|   4096   |   4096    |          8192            |

    // 释放失败
    free_pages(p2+10,4000);
    assert(buddy_tree.longest[4]!=4096);
    // 全部释放
    free_pages(p2,4000);
    free_pages(p1,8000);
    free_pages(p5,3000);
    assert(buddy_tree.longest[0]==buddy_tree.size);

};

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};