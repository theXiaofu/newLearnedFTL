/**
 * Filename: ld-tpftl.c
 * Author: Shengzhe Wang
 * Date Created: 2022-05
 * Version: 1.0
 *
 * Brief Description:
 *     This file implementing the main function of LearnedFTL (HPCA' 24).
 *
 * Copyright Notice:
 *     Copyright (C) 2023 Shengzhe Wang. All rights reserved.
 *     License: Distributed under the GPL License.
 *
 */

// #pragma GCC push_options
// #pragma GCC optimize(0)

#include "Segmentftl.h"
#include "util.h"
#include"string.h"
// static int hit_num = 0;
// static int gc_num = 0;
// static int gc_line_num = 0;
static int gc_threshold = 4;   // ! gc参数：当一个gtd_wp使用了多少个Line时开始
                                                        //与Segmentftl.h中的Gc_threshold  5一样因为写Gc_threshold  femu无法运行
static int free_line_threshold = 3;    // ! gc参数：当还剩多少未使用的free_line时开始GC
//static int level_compress_threshold=20;//当每一个node节点的层级大于这个值时将进行压缩
// static int train_num = 0;

// static FILE* gc_fp;

// static uint64_t tmp_counter = 0;
// static uint64_t counter = 0;
// static int line_init = 0;
// static int batch_do_gtd_gc = 0;
// static int actual_but_not_bitmap = 0;
// static int all_zero_num = 0;
// static int total_no_pred_num = 0;
// static int total_pred_num = 0;
// static int no_model_num = 0;

static int func(int *c) {
    printf("test\n");
    return *c;
}
static int line_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, struct line *victim_line);
static int gtd_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, struct line *victim_line, bool delete);
static void batch_gtd_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, int num, struct line *delete_line);
static int batch_line_do_gc(struct ssd* ssd, bool force, struct write_pointer *wpp, struct line *delete_line);
// static void should_do_gc(struct ssd *ssd, struct write_pointer *wpp);
// static struct line *select_victim_line(struct ssd *ssd, bool force);
static void insert_wp_lines(struct write_pointer *wpp);
static bool should_do_gc_v3(struct ssd *ssd, struct write_pointer *wpp);

//将新段插入到这一层中同时对这层旧段进行修改和从上到下的压缩，如果new_seg为NULL，那么只进行压缩。
//如果返回值表示这一层的起始段。
//bit_map包含上层和new_seg的位图。
static struct Seg* insert_seg2level(struct Seg *new_seg, struct Seg *start_seg,bool *bit_map,struct Senode *node)
{
    if(start_seg==NULL)
    {
        if(new_seg)
        node->l++;//建立一个新层级
        return new_seg;
    }
    struct Seg*nextl_start_seg = start_seg->next_level;//the starting segment of the next level
    struct Seg *pre_seg = NULL;
    struct Seg *tmp_seg = start_seg;
    struct Seg*next_seg =NULL;
    struct Seg* nextl_newseg=NULL;
    uint64_t x1,x2;
    
   while(tmp_seg)
    {
        x1 = tmp_seg->x1;
        x2 = tmp_seg->x2;
        next_seg = tmp_seg->next;
        //去除重复的部分因为这部分有最新的lpn-ppn的映射值去除即可。
        while(x1<=x2&&bit_map[x1])
        {
            x1++;
        }
        while(x2>=x1&&bit_map[x2])
        {
            x2--;
        }

        if(x1>x2)
        {//删除这个段  修改Senode 参数
            if(start_seg == tmp_seg)
            {
                if(next_seg)
                next_seg->next_level = start_seg->next_level;//连接到下一层
                start_seg = next_seg;//更改头
            }
            else
            {
                pre_seg ->next = next_seg;
            }
            node->seg_count--;
            g_free(tmp_seg);
        }
        else
        {
            tmp_seg->sppn += x1-tmp_seg->x1;
            tmp_seg->x1=x1;
            tmp_seg->x2 = x2;
            if(new_seg)
            {
                if(x1 < new_seg->x1 && x2 > new_seg->x2)
                {//如果new_seg被全包含直接把这个段加入到下一层中
                    nextl_newseg = tmp_seg;
                    new_seg->next = tmp_seg->next;
                    if(tmp_seg == start_seg)
                    {
                        new_seg->next_level = start_seg->next_level;
                        start_seg = new_seg;

                    }
                    else
                    {
                        pre_seg->next = new_seg;
                    }
                    tmp_seg->next = tmp_seg->next_level = NULL;
                    tmp_seg = new_seg;
                    new_seg = NULL;
                }
                else
                {
                    if(x1>new_seg->x2)
                    {
                        new_seg->next = tmp_seg;
                        if(tmp_seg == start_seg)
                        {
                            new_seg->next_level = start_seg->next_level;
                            tmp_seg->next_level = NULL;
                            start_seg = new_seg;

                        }
                        else
                        {
                            pre_seg->next = new_seg;
                        }
                        new_seg = NULL;
                    }
                }
            }

            while(x1<=x2)
            {
                bit_map[x1]=true;
                x1++;
            }
            pre_seg = tmp_seg;
        }
        tmp_seg = next_seg;
    }
    if(pre_seg)
    {
        if(new_seg)
        {
            if(pre_seg->x2<new_seg->x1)
            {
                pre_seg->next = new_seg;
            }
            else
            {
                printf("segment error:%d!!!!!!!!!!!!!!!!!",__LINE__);
            }
        }
        start_seg->next_level = insert_seg2level(nextl_newseg,nextl_start_seg,bit_map,node);
        return start_seg;
    }
    else
    {
        node->l--;
        return insert_seg2level(new_seg,nextl_start_seg,bit_map,node);
    }
    return NULL;
}

// static void print_senode(struct Seg* head)
// {
//     //struct Seg* tmp;
//     if(head)
//     {
//         struct Seg*next = head->next_level;
//         printf("slpn:\telpn:\tsppn:\t\n");
//         while(head)
//         {
            
//             printf("%lld\t%lld\t%lld\t\n",(long long)head->x1,(long long)head->x2,(long long)head->sppn);
//             //tmp = head;
//             head=head->next;
//         }
//         printf("\n");
//         print_senode(next);
        
//     }
// }

//通过slpn elpn插入到日志段中，其中slpn和elpn是组内的偏移
static int insert_seg2senode(struct ssd *ssd,uint64_t slpn, uint64_t elpn, uint64_t sppn, uint64_t tvpn)
{
    struct Seg *new_seg = g_malloc0(sizeof(struct Seg));
    if(slpn>ssd->sp.ents_per_pg||elpn>ssd->sp.ents_per_pg)
    {
        printf("slpn||elpn>ents_per_pg\n");
    }
    

    if(ssd->model_used)
    {
        
        int interval_size = ssd->sp.interval_size;
        int s =slpn;
        int k = s/interval_size;
        lr_breakpoint * lr_b;
        lr_node*lr_n=&(ssd->lr_nodes[tvpn]);
        uint16_t mask;
        int nextend = (k + 1)*interval_size;
        int count;
        while(s<=elpn)
        {
            lr_b = &(lr_n->brks[k]);
            if(nextend>elpn)
                nextend = elpn+1;

            
            mask = ((1 << (nextend - s)) - 1) << s; // 创建掩码
            lr_b->bitmap &= ~mask; // 清除对应的位
            count = ssd->bitmap_table[lr_b->bitmap & 0xFF] + ssd->bitmap_table[(lr_b->bitmap>>8)&0xFF];
            if(count + s < nextend)
            {
                lr_b->bitmap = mask;
                lr_b->b = (int)sppn-s;
            }

            s = nextend;
            k++;
            nextend += interval_size;
        }
    }
    
    
    new_seg->x1=slpn;
    new_seg->x2=elpn;
    //new_seg->next_avail_time = next_avail_time;
    new_seg->sppn = sppn;
    new_seg->next = new_seg->next_level = NULL;
    struct Senode *node = &(ssd->senodes[tvpn]);
    int old_count = node->seg_count;
    node->seg_count++;
    bool *bitmap = ssd->seg_bitmaps;
    // for(int i = 0;i<ssd->sp.ents_per_pg;++i)
    // {
    //     bitmap[i]=0;
    // }
    memset(bitmap,0,sizeof(bool)*(ssd->sp.ents_per_pg));
    for(int i = slpn;i<=elpn;++i)
    bitmap[i]=true;
    node->head = insert_seg2level(new_seg,node->head,bitmap,node);
    // if(node->l > 1)
    // {
    //     printf("cnt:%d\tlevel:%d\n",node->seg_count,node->l);
    //     //print_senode(node->head);
    // }
    return node->seg_count - old_count;
}



static void *ftl_thread(void *arg);

static inline uint64_t se_hash(uint64_t tvpn)
{
    return tvpn % Se_HASH_SIZE;
}


static void insert_se_hashtable(hash_table *ht, Cmt_Senode *cmt_senode) 
{
    uint64_t pos = se_hash(cmt_senode->tvpn);
    cmt_senode->next = ht->cmt_se_table[pos];
    ht->cmt_se_table[pos] = cmt_senode;
}


static struct Cmt_Senode* find_hash_cmt_senode(hash_table *ht, uint64_t tvpn)
{
    uint64_t pos = se_hash(tvpn);
    struct Cmt_Senode *cmt_senode = ht->cmt_se_table[pos];
    while (cmt_senode != NULL && cmt_senode->tvpn != tvpn) {
        cmt_senode = cmt_senode->next;
    }
    return cmt_senode;
}

static bool delete_cmt_senode_hashnode(hash_table *ht, Cmt_Senode *cmt_senode)
{
    uint64_t pos = se_hash(cmt_senode->tvpn);
    Cmt_Senode *tmp_cmtnode = ht->cmt_se_table[pos], *pre_se;
    if (tmp_cmtnode == cmt_senode) {
        ht->cmt_se_table[pos] = tmp_cmtnode->next;
        tmp_cmtnode->next = NULL;
    } else {
        pre_se = tmp_cmtnode;
        tmp_cmtnode = tmp_cmtnode->next;
        while (tmp_cmtnode != NULL && tmp_cmtnode != cmt_senode) {
            pre_se = tmp_cmtnode;
            tmp_cmtnode= tmp_cmtnode->next;
        }
        if (tmp_cmtnode == NULL)
            return false;
        pre_se->next = tmp_cmtnode->next;
        tmp_cmtnode->next = NULL;
    }
    return true;
}

//这个函数没用注释掉
// static inline bool should_gc(struct ssd *ssd)
// {
//     return (ssd->lm.free_line_cnt <= ssd->sp.gc_thres_lines);
// }

static inline struct ppa get_maptbl_ent(struct ssd *ssd, uint64_t lpn)
{
    return ssd->maptbl[lpn];
}

static inline void set_maptbl_ent(struct ssd *ssd, uint64_t lpn, struct ppa *ppa)
{
    ftl_assert(lpn < ssd->sp.tt_pgs);
    ssd->maptbl[lpn] = *ppa;
}

static uint64_t ppa2pgidx(struct ssd *ssd, struct ppa *ppa)
{
    struct ssdparams *spp = &ssd->sp;
    uint64_t pgidx;

    pgidx = ppa->g.ch  * spp->pgs_per_ch  + \
            ppa->g.lun * spp->pgs_per_lun + \
            ppa->g.pl  * spp->pgs_per_pl  + \
            ppa->g.blk * spp->pgs_per_blk + \
            ppa->g.pg;

    ftl_assert(pgidx < spp->tt_pgs);

    return pgidx;
}

// * new method for ppa to vppn
static uint64_t ppa2vppn(struct ssd *ssd, struct ppa *ppa) {
    
    struct ssdparams *spp = &ssd->sp;
    uint64_t vppn;
    vppn = ppa->g.ch + \
            ppa->g.lun * spp->chn_per_lun + \
            ppa->g.pl * spp->chn_per_pl + \
            ppa->g.pg * spp->chn_per_pg + \
            ppa->g.blk * spp->chn_per_blk;
    
    return vppn;
}

static struct ppa vppn2ppa(struct ssd *ssd, uint64_t vppn) {
    struct ppa ppa;
    struct ssdparams *spp = &ssd->sp;
    ppa.g.blk = vppn / spp->chn_per_blk;
    vppn -= ppa.g.blk*spp->chn_per_blk;
    ppa.g.pg = vppn / spp->chn_per_pg;
    vppn -= ppa.g.pg * spp->chn_per_pg;
    ppa.g.pl = vppn / spp->chn_per_pl;
    vppn -= ppa.g.pl * spp->chn_per_pl;
    ppa.g.lun = vppn / spp->chn_per_lun;
    ppa.g.ch = vppn - ppa.g.lun * spp->chn_per_lun;

    return ppa;
}

// static struct ppa lpn2ppn(struct ssd *ssd,uint64_t lpn)
// {
//     uint64_t tvpn = lpn/(ssd->sp.ents_per_pg);
//     lpn = lpn-tvpn*(ssd->sp.ents_per_pg);
//     struct Seg *head = ssd->senodes[tvpn].head,*tmp;
//     while(head)
//     {
//         tmp = head;
//         while(tmp)
//         {
//             if(tmp->x2>=lpn&&tmp->x1<=lpn)
//             {
//                 return vppn2ppa(ssd,tmp->sppn+lpn-tmp->x1);
//             }
//             tmp = tmp->next;
//         }
//         head = head->next_level;
//     }
//     struct ppa ppa0;
//     ppa0.ppa= UNMAPPED_PPA;
//     return ppa0;

// }

//返回的Seg中的x1x2是组内偏移elpn也是组内偏移
static struct Seg* lpn2seg(struct ssd *ssd,uint64_t lpn,uint64_t*elpn)
{
    uint64_t tvpn = lpn/(ssd->sp.ents_per_pg);
    lpn = lpn-tvpn*(ssd->sp.ents_per_pg);
    struct Seg *head = ssd->senodes[tvpn].head,*tmp;
    tmp = NULL;
    *elpn = 1000000;//elpn为无穷大
    while(head)
    {
        tmp = head;
        while(tmp)
        {
            
            if(tmp->x2>=lpn&&tmp->x1<=lpn)
            {
                return tmp;
            }
            if(tmp->x1>lpn)
            {
                if(*elpn > tmp->x1)
                {
                    *elpn = tmp->x1 - 1;       
                }
                tmp = NULL;
            }
            else
            tmp = tmp->next;
        }
        head = head->next_level;
    }
    return NULL;

}

// static struct Seg* seg_compress(struct Seg*start_seg,struct Senode *node)
// {
//     struct Seg*head1,*head2,*tmp1,*tmp2,*pre1,*pre2,*next1,*next2,*pre;
//     head1 = head2=NULL;
//     struct Seg s1,s2;
//     head1 = &s1;//设置哨兵节点方便处理
//     head2 = &s2;

//     tmp1 = start_seg;
//     pre1 = NULL;
//     while(tmp1)//将链表层级反转
//     {
//         next1 = tmp1->next_level;
//         tmp1->next_level = pre1;
//         pre1 = tmp1;
//         tmp1 = next1;
//     }
//     tmp1 = pre1->next_level;
//     tmp2 = pre1;
//     pre = NULL;
//     while(tmp1)
//     {
//         tmp2->next_level = NULL;
//         head2->next_level = pre;//pre为未反转前tmp2的下一层
//         head2->next=tmp2;

//         head1->next_level = tmp1->next_level;//记录未反转前的上一层
//         tmp1->next_level = NULL;
//         head1->next = tmp1;

//         pre1 = head1;
//         pre2 = head2;
        
//         while(tmp1&&tmp2)
//         {
//             next1 = tmp1->next;
//             next2 = tmp2->next;
//             if(tmp2->x1<tmp1->x1)
//             {
//                 if(tmp2->x2<tmp1->x1)
//                 {
//                     tmp2->next = tmp1;
//                     pre1->next = tmp2;
//                     pre1 = tmp2;
//                     pre2->next = next2;
//                     tmp2 = next2;
//                 }
//                 else
//                 {
//                     //这里说明tmp2包含tmp1
//                     pre2 = tmp2;
//                     tmp2 =  next2;
//                     pre1 = tmp1;
//                     tmp1 = next1;
//                 }
//             }
//             else
//             {
//                 pre1 = tmp1;
//                 tmp1 = next1;
//             }
//         }
//         if(tmp2)
//         {
//             if(tmp2->x1<pre1->x2)
//             {
//                 printf("compress error:%d!!!!!\n",__LINE__);
//             }
//             pre1->next = tmp2;
//             pre2->next = NULL;
//         }
//         if(head2->next)
//         {
//             head2->next->next_level = pre;
//             pre = head2->next;
//             tmp2 = head1->next;
//             tmp2->next_level = pre;
//             tmp1=head1->next_level;
//         }
//         else
//         {
//             node->l--;
//             tmp2=head1->next;
//             tmp2->next_level = pre;
//             tmp1 = head1->next_level;
//         }
//     }
//     return tmp2;
// }

// static void node_compress_level(struct ssd*ssd,uint64_t tvpn)
// {//不需要压缩这个函数没有用
//     struct Senode *senode = &(ssd->senodes[tvpn]);
//     if(senode->l > level_compress_threshold)
//     {
//         senode->head = seg_compress(senode->head,senode);
//     }
// }

static inline uint64_t get_rmap_ent(struct ssd *ssd, struct ppa *ppa)
{
    uint64_t pgidx = ppa2pgidx(ssd, ppa);

    return ssd->rmap[pgidx];
}

/* set rmap[page_no(ppa)] -> lpn */
static inline void set_rmap_ent(struct ssd *ssd, uint64_t lpn, struct ppa *ppa)
{
    uint64_t pgidx = ppa2pgidx(ssd, ppa);

    ssd->rmap[pgidx] = lpn;
}

static inline int victim_line_cmp_pri(pqueue_pri_t next, pqueue_pri_t curr)
{
    return (next > curr);
}

static inline pqueue_pri_t victim_line_get_pri(void *a)
{
    return ((struct line *)a)->vpc;
}

static inline void victim_line_set_pri(void *a, pqueue_pri_t pri)
{
    ((struct line *)a)->vpc = pri;
}

static inline size_t victim_line_get_pos(void *a)
{
    return ((struct line *)a)->pos;
}

static inline void victim_line_set_pos(void *a, size_t pos)
{
    ((struct line *)a)->pos = pos;
}

static void ssd_init_lines(struct ssd *ssd)
{
    struct ssdparams *spp = &ssd->sp;
    struct line_mgmt *lm = &ssd->lm;
    struct line *line;

    lm->tt_lines = spp->blks_per_pl;
    ftl_assert(lm->tt_lines == spp->tt_lines);
    lm->lines = g_malloc0(sizeof(struct line) * lm->tt_lines);

    QTAILQ_INIT(&lm->free_line_list);
    lm->victim_line_pq = pqueue_init(spp->tt_lines, victim_line_cmp_pri,
            victim_line_get_pri, victim_line_set_pri,
            victim_line_get_pos, victim_line_set_pos);
    QTAILQ_INIT(&lm->full_line_list);
    
    // * modified
    QTAILQ_INIT(&lm->victim_list);

    lm->free_line_cnt = 0;
    for (int i = 0; i < lm->tt_lines; i++) {
        line = &lm->lines[i];
        line->id = i;
        line->rest = spp->pgs_per_line;
        line->ipc = 0;
        line->vpc = 0;
        line->pos = 0;
        line->type = UNUSED;
        // line->rest = spp
        /* initialize all the lines as free lines */
        QTAILQ_INSERT_TAIL(&lm->free_line_list, line, entry);
        lm->free_line_cnt++;
    }

    ftl_assert(lm->free_line_cnt == lm->tt_lines);
    lm->victim_line_cnt = 0;
    lm->full_line_cnt = 0;

}



static void init_line_write_pointer(struct ssd *ssd, struct write_pointer *wpp, bool gc_flag)
{
    struct line_mgmt *lm = &ssd->lm;
    struct line *curline = NULL;
    
    if (gc_flag) {
        if (lm->free_line_cnt < free_line_threshold) {
            // struct timespec time1, time2;

            // clock_gettime(CLOCK_MONOTONIC, &time1);
            bool res = should_do_gc_v3(ssd, wpp);
            // clock_gettime(CLOCK_MONOTONIC, &time2);
                
            // ssd->stat.GC_time += ((time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));
            if (res) {
                return;
            }
        } 
    }

    if (lm->free_line_cnt < 2) {
        printf("lines are less!\n");
    }
        

    curline = QTAILQ_FIRST(&lm->free_line_list);
    if (!curline) {
        printf("No free lines left in [%s]31232131231321 !!!!\n", ssd->ssdname);
        ftl_err("No free lines left in [%s]31232131231321 !!!!\n", ssd->ssdname);
        return;
    }
    QTAILQ_REMOVE(&lm->free_line_list, curline, entry);
    lm->free_line_cnt--;

    /* wpp->curline is always our next-to-write super-block */
    wpp->curline = curline;
    wpp->ch = 0;
    wpp->lun = 0;
    wpp->pg = 0;
    wpp->blk = curline->id;
    wpp->pl = 0;
    ssd->line2write_pointer[wpp->curline->id] = wpp;
    if (&ssd->trans_wp == wpp) {
        wpp->curline->type = GTD;
    } else {
        wpp->curline->type = DATA;
        // printf("write pointer %d init line id: %d\n", wpp->id, wpp->curline->id);
    }

    if (wpp->curline->rest == 0) {
        func(&wpp->curline->rest);
        printf("what's up?\n");
    }    

    insert_wp_lines(wpp);
    

}

static void ssd_init_write_pointer(struct ssd *ssd)
{
    ssd->gtd_wps = g_malloc0(sizeof(struct write_pointer) * ssd->sp.tt_line_wps);
    
    for (int i = 0; i < ssd->sp.tt_line_wps; i++) {
        ssd->gtd_wps[i].curline = NULL;
        ssd->gtd_wps[i].wpl = g_malloc0(sizeof(struct wp_lines));
        ssd->gtd_wps[i].wpl->line = NULL;
        ssd->gtd_wps[i].wpl->next = NULL;
        ssd->gtd_wps[i].vic_cnt = 0;
        ssd->gtd_wps[i].id = i;
    }
    ssd->trans_wp.wpl = g_malloc0(sizeof(struct wp_lines));
    ssd->trans_wp.wpl->line = NULL;
    ssd->trans_wp.wpl->next = NULL;
    ssd->trans_wp.vic_cnt = 0;
    ssd->trans_wp.id = ssd->sp.tt_lines;
    // init the rmap for lines
    ssd->line2write_pointer = g_malloc0(sizeof(struct write_pointer *) * ssd->sp.tt_lines);
    init_line_write_pointer(ssd, &ssd->trans_wp, false);


}

static inline void check_addr(int a, int max)
{
    ftl_assert(a >= 0 && a < max);
}

// static struct line *get_next_free_line(struct ssd *ssd)
// {
//     struct line_mgmt *lm = &ssd->lm;
//     struct line *curline = NULL;

//     curline = QTAILQ_FIRST(&lm->free_line_list);
//     if (!curline) {
//         ftl_err("No free lines left in [%s] !!!!\n", ssd->ssdname);
//         return NULL;
//     }

//     QTAILQ_REMOVE(&lm->free_line_list, curline, entry);
//     lm->free_line_cnt--;
//     return curline;
// }



static void insert_wp_lines(struct write_pointer *wpp) {
    struct wp_lines *wpl = g_malloc0(sizeof(struct wp_lines));
    wpl->line = wpp->curline;
    wpl->next = wpp->wpl->next;
    wpp->wpl->next = wpl;
    wpp->vic_cnt++;
    return;
}


// TODO: borrow or gc function
// static bool borrow_or_gc(struct ssd *ssd, struct write_pointer *wpp) {
    
//     struct line_mgmt *lm = &ssd->lm;

//     if (lm->free_line_cnt < 4) {



//     } else {
//         // * 1. first check is there is a no full line
//         int min_remain_pgs = ssd->sp.pgs_per_line;
//         int min_remain_line_id = -1;
//         for (int i = 0; i < lm->tt_lines; i++) {
//             if (lm->lines[i].rest < min_remain_line_id) {
//                 min_remain_line_id = i;
//             }
//         }

//         wpp->invade_lines ++;
//         // * 2. second push this no full line to the end of the queue

//         // * 3. third add the wpl to this wpp

//         // * note that since one line may contain different pages belong to different wpp,
//         // * so need to modify the GC write back function
//     }
    
    
// }

static bool should_do_gc_v3(struct ssd *ssd, struct write_pointer *wpp) {
    struct line_mgmt *lm = &ssd->lm;
    //printf("GC happens?\n");
    // if (ssd->lm.free_line_cnt < 4) {
    //     printf("what's wrong?\n");
    // }
    
    if (wpp && wpp->vic_cnt >= gc_threshold) {
        // if (wpp->id != 256) {
        //     printf("line %d do batch gc\n", wpp->id);
        // }
        // * 如果一个写指针对应的Line的数量超过4，就必须GC
        init_line_write_pointer(ssd, wpp, false);
        // wpp->vic_cnt++;
        if (&ssd->trans_wp == wpp) {
            batch_gtd_do_gc(ssd, true, wpp, wpp->vic_cnt, NULL);
        } else {
            batch_line_do_gc(ssd, true, wpp, NULL);
        }

        if (wpp->curline->rest == 0) {
            func(&wpp->curline->rest);
            printf("what's up?\n");
        }   

        return true;
        
    } else if (ssd->trans_wp.vic_cnt >= gc_threshold) {

        // * 如果gtd写指针的line的数量大于=阈值，对其进行GC

        QTAILQ_INSERT_TAIL(&lm->victim_list, ssd->trans_wp.curline, entry);
        lm->victim_line_cnt++;

        init_line_write_pointer(ssd, &ssd->trans_wp, false);

        // ssd->trans_wp.vic_cnt++;

        // * put this line to the victim lines the line write pointer belongs to
        batch_gtd_do_gc(ssd, true, &ssd->trans_wp, ssd->trans_wp.vic_cnt, NULL);

    } else if (lm->free_line_cnt < 10) {

        struct line *tvl = QTAILQ_FIRST(&lm->victim_list);
        struct write_pointer *write_back_wp = NULL;
        struct line *vl = NULL;

        
        // QTAILQ_REMOVE(&lm->victim_list, vl, entry);
        // int max_vic = 1;
        if (lm->free_line_cnt < free_line_threshold) {
            while (tvl) {
                struct write_pointer *tmp_wp = ssd->line2write_pointer[tvl->id];
                if (tmp_wp->vic_cnt > 1) {
                    write_back_wp = tmp_wp;
                    vl = tvl;
                    break;
                }

                tvl = tvl->entry.tqe_next;
            }
            if (!tvl) {
                tvl = QTAILQ_FIRST(&lm->victim_list);
                write_back_wp = ssd->line2write_pointer[tvl->id];
            }
            if (write_back_wp->vic_cnt == 1) {
                printf("???\n");
            }

            // if (&ssd->trans_wp == wpp) {
            //     printf("trans wp is doing gc\n");
            // }
            
            if (vl) { 
                
                // if (write_back_wp->curline && write_back_wp->curline->rest == vl->vpc) {
                //     printf("some pages are not successfully invalidated! \n");
                // }
                if (write_back_wp->curline && write_back_wp->curline->rest >= vl->vpc) {
                    if (vl->type == GTD) {
                        
                        gtd_do_gc(ssd, true, write_back_wp, vl, true);
                        write_back_wp->vic_cnt--;

                    } else if (vl->type == DATA) {
                        // fprintf(gc_fp, "%ld\n",counter);
                        line_do_gc(ssd, true, write_back_wp, vl);
                        write_back_wp->vic_cnt--;
                    }

                    if (write_back_wp == wpp) {
                        if (wpp->curline->rest == 0) {
                            init_line_write_pointer(ssd, wpp, false);
                                // printf("what's up?\n");
                        }   
                        return true;
                    }
                } else {
                    if (write_back_wp != wpp) {
                        QTAILQ_INSERT_TAIL(&lm->victim_list, write_back_wp->curline, entry);
                    // printf("batch gtd write id: %d\n", write_back_wp->curline->id);
                        lm->victim_line_cnt++;
                    }
                    init_line_write_pointer(ssd, write_back_wp, false);
                    // write_back_wp->vic_cnt++;

                    if (vl->type == GTD) {
                        
                        // printf("gtd batch do gc\n");
                        batch_gtd_do_gc(ssd, true, write_back_wp, write_back_wp->vic_cnt, vl);
                    } else if (vl->type == DATA) {
                        // printf("line %d do batch gc\n", write_back_wp->id);
                        // * model rebuilding
                        batch_line_do_gc(ssd, true, write_back_wp, vl);

                        if (write_back_wp == wpp) {
                            if (wpp->curline->rest == 0) {
                                func(&wpp->curline->rest);
                                printf("what's up?\n");
                            }   
                            return true;
                        }
                    }
                }
                if (write_back_wp == wpp) {
                    if (wpp->curline->rest == 0) {
                        func(&wpp->curline->rest);
                        printf("what's up?\n");
                    }   
                    return true;
                }
            }
        }
        
        

    }
    return false;
}


static void clear_one_write_pointer_victim_lines(struct wp_lines *wpl, struct line *victim_line) {
    struct wp_lines *tmp;
    while (wpl) {
        if (wpl->next && wpl->next->line == victim_line) {
            tmp = wpl->next;
            wpl->next = tmp->next;
            
            g_free(tmp);
            break;
        }
        wpl = wpl->next;
    }
    
}

// * clean the victim_lines recorder wp_lines since all lines are collected
static void clear_all_write_pointer_victim_lines(struct wp_lines *wpl, struct write_pointer *wpp) {
    wpl = wpp->wpl->next;
    struct wp_lines *wpp_wpl = NULL, *tmp;
    while (wpl) {
        tmp = wpl;
        wpl = wpl->next;
        if (tmp->line == wpp->curline) {
            wpp_wpl = tmp; 
        } else {
            g_free(tmp);
        }
    }
    wpp_wpl->next = NULL;
    wpp->wpl->next = wpp_wpl;
    wpp->vic_cnt = 1;
    
}

/**
 * @brief for each line write pointer, advance them
 * @author: fxx
 * @param ssd 
 */
static void advance_line_write_pointer (struct ssd *ssd, struct write_pointer *wpp) {
    struct ssdparams *spp = &ssd->sp;
    struct line_mgmt *lm = &ssd->lm;

    another_try:

    //先channel++,再lun++,再block++
    check_addr(wpp->ch, spp->nchs);
    wpp->ch++;
    if (wpp->ch == spp->nchs) {
        wpp->ch = 0;
        check_addr(wpp->lun, spp->luns_per_ch);
        wpp->lun++;
        /* in this case, we should go to next lun */
        if (wpp->lun == spp->luns_per_ch) {
            wpp->lun = 0;
            /* go to next page in the block */
            check_addr(wpp->pg, spp->pgs_per_blk);
            wpp->pg++;
            if (wpp->pg == spp->pgs_per_blk) {
                
                wpp->pg = 0;

                QTAILQ_INSERT_TAIL(&lm->victim_list, wpp->curline, entry);
                // pqueue_insert(lm->victim_line_pq, wpp->curline);
                // wpp->vic_cnt++;
                

                lm->victim_line_cnt++;

                // TODO: do the group-borrow work here;
                // bool res = borrow_or_gc(ssd, wpp);
                // struct timespec time1, time2;

                // clock_gettime(CLOCK_MONOTONIC, &time1);
                bool res = should_do_gc_v3(ssd, wpp);
                // clock_gettime(CLOCK_MONOTONIC, &time2);
                
                // ssd->stat.GC_time += ((time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));
                
                
                
                
                if (!res)
                    init_line_write_pointer(ssd, wpp, false);
                else if (wpp != &ssd->trans_wp) {
                    if (wpp->curline->rest == 0) {
                        func(&wpp->curline->rest);
                        printf("what's up?\n");
                    }

                    goto another_try;
                }
            }
        }
    }
}

// static void advance_gc_trans_write_pointer (struct ssd *ssd, struct write_pointer *wpp) {
//     struct ssdparams *spp = &ssd->sp;
//     struct line_mgmt *lm = &ssd->lm;

//     //先channel++,再lun++,再block++
//     check_addr(wpp->ch, spp->nchs);
//     wpp->ch++;
//     if (wpp->ch == spp->nchs) {
//         wpp->ch = 0;
//         check_addr(wpp->lun, spp->luns_per_ch);
//         wpp->lun++;
//         /* in this case, we should go to next lun */
//         if (wpp->lun == spp->luns_per_ch) {
//             wpp->lun = 0;
//             /* go to next page in the block */
//             check_addr(wpp->pg, spp->pgs_per_blk);
//             wpp->pg++;
//             if (wpp->pg == spp->pgs_per_blk) {
                
//                 wpp->pg = 0;
//                 QTAILQ_INSERT_TAIL(&lm->victim_list, wpp->curline, entry);
//                 // pqueue_insert(lm->victim_line_pq, wpp->curline);
//                 wpp->vic_cnt++;
//                 lm->victim_line_cnt++;

//                 init_line_write_pointer(ssd, wpp, false);
//                 wpp->vic_cnt++;
//                 batch_gtd_do_gc(ssd, true, wpp, wpp->vic_cnt, NULL);
//             }
//         }
//     }
// }

static struct ppa get_new_line_page(struct ssd *ssd, struct write_pointer *wpp)
{
    struct ppa ppa;
    ppa.ppa = 0;
    ppa.g.ch = wpp->ch;
    ppa.g.lun = wpp->lun;
    ppa.g.pg = wpp->pg;
    ppa.g.blk = wpp->blk;
    ppa.g.pl = wpp->pl;
    wpp->curline->rest--;
    if (wpp->curline->rest < 0) {
        printf("buduijin\n");
        func(&wpp->curline->rest);
    }
    ftl_assert(ppa.g.pl == 0);

    return ppa;
}

static void check_params(struct ssdparams *spp)
{
    /*
     * we are using a general write pointer increment method now, no need to
     * force luns_per_ch and nchs to be power of 2
     */

    //ftl_assert(is_power_of_2(spp->luns_per_ch));
    //ftl_assert(is_power_of_2(spp->nchs));
}

static void ssd_init_params(struct ssdparams *spp)
{
    spp->secsz = 512;
    spp->secs_per_pg = 8;/*pagesize=4k*/
    spp->pgs_per_blk = 512;/*a page group has 512 pages   */
    spp->blks_per_pl = 512; /*default 256，8GB */
    spp->pls_per_lun = 1;    /*8=4*2*1   a big block group has 8 blocks*/
    spp->luns_per_ch = 2;   /* default 8 */
    spp->nchs = 4;          /* default 8 */

    spp->pg_rd_lat = NAND_READ_LATENCY;
    spp->pg_wr_lat = NAND_PROG_LATENCY;
    spp->blk_er_lat = NAND_ERASE_LATENCY;
    spp->ch_xfer_lat = 0;

    /* calculated values */
    spp->secs_per_blk = spp->secs_per_pg * spp->pgs_per_blk;
    spp->secs_per_pl = spp->secs_per_blk * spp->blks_per_pl;
    spp->secs_per_lun = spp->secs_per_pl * spp->pls_per_lun;
    spp->secs_per_ch = spp->secs_per_lun * spp->luns_per_ch;
    spp->tt_secs = spp->secs_per_ch * spp->nchs;

    spp->pgs_per_pl = spp->pgs_per_blk * spp->blks_per_pl;
    spp->pgs_per_lun = spp->pgs_per_pl * spp->pls_per_lun;
    spp->pgs_per_ch = spp->pgs_per_lun * spp->luns_per_ch;
    spp->tt_pgs = spp->pgs_per_ch * spp->nchs;

    spp->blks_per_lun = spp->blks_per_pl * spp->pls_per_lun;
    spp->blks_per_ch = spp->blks_per_lun * spp->luns_per_ch;
    spp->tt_blks = spp->blks_per_ch * spp->nchs;

    spp->pls_per_ch =  spp->pls_per_lun * spp->luns_per_ch;
    spp->tt_pls = spp->pls_per_ch * spp->nchs;

    spp->tt_luns = spp->luns_per_ch * spp->nchs;

    /* line is special, put it at the end */
    spp->blks_per_line = spp->tt_luns; /* TODO: to fix under multiplanes */
    spp->pgs_per_line = spp->blks_per_line * spp->pgs_per_blk;
    spp->secs_per_line = spp->pgs_per_line * spp->secs_per_pg;
    spp->tt_lines = spp->blks_per_lun; /* TODO: to fix under multiplanes */

    spp->gc_thres_pcent = 0.75;
    spp->gc_thres_lines = (int)((1 - spp->gc_thres_pcent) * spp->tt_lines);
    spp->gc_thres_pcent_high = 0.95;
    spp->gc_thres_lines_high = (int)((1 - spp->gc_thres_pcent_high) * spp->tt_lines);
    spp->enable_gc_delay = true;


    //=================modified===================
    // for dftl, means how many addresses one page has
    // 512 = 4KB / 8Byte 
    spp->addr_size = 8;
    spp->pg_size = spp->secsz * spp->secs_per_pg;
    spp->ents_per_pg = spp->pg_size / spp->addr_size;
    spp->tt_trans_pgs = spp->tt_pgs / spp->ents_per_pg;
    // one translation page can accomodate 512 pages(1 blocks), so 8 trans pages combine a line
    spp->trans_per_line = spp->pgs_per_line / spp->ents_per_pg;
    spp->tt_line_wps = spp->tt_trans_pgs/spp->trans_per_line;
    printf("total pages: %d\n", spp->tt_line_wps);

    spp->interval_size = spp->ents_per_pg%MAX_INTERVALS ? spp->ents_per_pg/MAX_INTERVALS+1 : spp->ents_per_pg/MAX_INTERVALS ;
    spp->tt_gtd_size = spp->tt_pgs / spp->ents_per_pg;
    /* because a segment using  4Byte and model using half CMT,tt_blks should / 2*/
    spp->tt_cmt_size = spp->tt_blks/2;/*原文为一半*/
    //spp->enable_request_prefetch = true;    /* cannot set false! */
    //spp->enable_select_prefetch = true;

    // * for virtual ppn
    spp->chn_per_lun = spp->nchs;
    spp->chn_per_pl = spp->nchs * spp->luns_per_ch;
    spp->chn_per_pg = spp->chn_per_pl * spp->pls_per_lun;
    spp->chn_per_blk = spp->chn_per_pg * spp->pgs_per_blk;

    check_params(spp);
}

static void ssd_init_nand_page(struct nand_page *pg, struct ssdparams *spp)
{
    pg->nsecs = spp->secs_per_pg;
    pg->sec = g_malloc0(sizeof(nand_sec_status_t) * pg->nsecs);
    for (int i = 0; i < pg->nsecs; i++) {
        pg->sec[i] = SEC_FREE;
    }
    pg->status = PG_FREE;
}

static void ssd_init_nand_blk(struct nand_block *blk, struct ssdparams *spp)
{
    blk->npgs = spp->pgs_per_blk;
    blk->pg = g_malloc0(sizeof(struct nand_page) * blk->npgs);
    for (int i = 0; i < blk->npgs; i++) {
        ssd_init_nand_page(&blk->pg[i], spp);
    }
    blk->ipc = 0;
    blk->vpc = 0;
    blk->erase_cnt = 0;
    blk->wp = 0;
}

static void ssd_init_nand_plane(struct nand_plane *pl, struct ssdparams *spp)
{
    pl->nblks = spp->blks_per_pl;
    pl->blk = g_malloc0(sizeof(struct nand_block) * pl->nblks);
    for (int i = 0; i < pl->nblks; i++) {
        ssd_init_nand_blk(&pl->blk[i], spp);
    }
}

static void ssd_init_nand_lun(struct nand_lun *lun, struct ssdparams *spp)
{
    lun->npls = spp->pls_per_lun;
    lun->pl = g_malloc0(sizeof(struct nand_plane) * lun->npls);
    for (int i = 0; i < lun->npls; i++) {
        ssd_init_nand_plane(&lun->pl[i], spp);
    }
    lun->next_lun_avail_time = 0;
    lun->busy = false;
}

static void ssd_init_ch(struct ssd_channel *ch, struct ssdparams *spp)
{
    ch->nluns = spp->luns_per_ch;
    ch->lun = g_malloc0(sizeof(struct nand_lun) * ch->nluns);
    for (int i = 0; i < ch->nluns; i++) {
        ssd_init_nand_lun(&ch->lun[i], spp);
    }
    ch->next_ch_avail_time = 0;
    ch->busy = 0;
}

static void ssd_init_maptbl(struct ssd *ssd)
{
    struct ssdparams *spp = &ssd->sp;

    ssd->maptbl = g_malloc0(sizeof(struct ppa) * spp->tt_pgs);
    for (int i = 0; i < spp->tt_pgs; i++) {
        ssd->maptbl[i].ppa = UNMAPPED_PPA;
        //ssd->maptbl[i].next_avail_time = 0;
    }

    // init the gtd
    //ssd->lr_nodes = g_malloc0(sizeof(struct lr_node) * spp->tt_trans_pgs);
    ssd->gtd = g_malloc0(sizeof(struct ppa) * spp->tt_trans_pgs);
    ssd->lr_nodes = g_malloc0(sizeof(struct lr_node) * spp->tt_trans_pgs);
    ssd->gtd_usage = g_malloc0(sizeof(uint64_t) * spp->tt_trans_pgs);

    ssd->valid_lines = g_malloc0(sizeof(uint8_t) * spp->tt_lines);
    for (int i = 0; i < spp->blks_per_pl; i++) 
        ssd->valid_lines[i] = 0;

    // init the cmt

    for (int i = 0; i < spp->tt_trans_pgs; i++) {
        ssd->gtd[i].ppa = UNMAPPED_PPA;
        ssd->lr_nodes[i].u = 1;     // * the bit denote if the model is used
        
        // the min-max attributes

        ssd->gtd_usage[i] = 512;    // how many translations in one page is used
    }
}

static void ssd_init_Senode(struct ssd*ssd)
{
    struct ssdparams *spp = &ssd->sp;
    ssd->senodes = g_malloc0(sizeof(struct Senode) * spp->tt_gtd_size);
    for(int i = 0;i < spp->tt_gtd_size;++i)
    {
        ssd->senodes[i].head = NULL;
        ssd->senodes[i].l = 0;
        ssd->senodes[i].seg_count = 0;
        ssd->senodes[i].tvpn = i; 
    }
}

static void ssd_init_cmt(struct ssd *ssd)
{
    struct ssdparams *spp = &ssd->sp;
    struct cmt_mgmt *cm = &ssd->cm;
    struct hash_table *ht = &cm->ht;
    
    cm->tt_entries = spp->tt_cmt_size;//how many segments can be used
    cm->tt_Senodes = 0;

    QTAILQ_INIT(&cm->Cmt_Senode_list);

    for (int i = 0; i < Se_HASH_SIZE; i++) {
        ht->cmt_se_table[i] = NULL;
    }
}

static void ssd_init_rmap(struct ssd *ssd)
{
    struct ssdparams *spp = &ssd->sp;

    ssd->rmap = g_malloc0(sizeof(uint64_t) * spp->tt_pgs);
    for (int i = 0; i < spp->tt_pgs; i++) {
        ssd->rmap[i] = INVALID_LPN;
    }
}

static void ssd_init_bitmap(struct ssd *ssd) {
    struct ssdparams *spp = &ssd->sp;
    //ssd->bitmaps = g_malloc0(sizeof(uint8_t)*spp->tt_pgs);
    ssd->seg_bitmaps = g_malloc0(sizeof(bool)*spp->ents_per_pg);
    ssd->bitmap_table[0]=0;
    for (int i = 0; i < 256; i++)
        ssd->bitmap_table[i] = ssd->bitmap_table[i>>1] + (1&i);

}

// * 将所有的模型都初始化为y=x
static void ssd_init_all_models(struct ssd *ssd) {
    struct ssdparams* sp = &ssd->sp;
    //int avg_valid_cnt = sp->ents_per_pg / MAX_INTERVALS;

    for (int i = 0; i < sp->tt_gtd_size; i++) {

        ssd->lr_nodes[i].u = 1;
        for (int j = 0; j < MAX_INTERVALS; j++) {
            lr_breakpoint* brk = &ssd->lr_nodes[i].brks[j];
            brk->b = 0;
            // * all models' valid cnt is zero, to facilitate the model sequential initilization
            brk->bitmap = 0;
            
            //brk->valid_cnt = 0;
        }
    }
}

static void ssd_init_statistics(struct ssd *ssd)
{
    struct statistics *st = &ssd->stat;

    st->cmt_hit_cnt = 0;
    st->cmt_miss_cnt = 0;
    st->cmt_hit_ratio = 0;
    st->access_cnt = 0;
    st->model_hit_num = 0;
    st->model_use_num = 0;
    st->model_out_range = 0;
    st->predict_time = 0;
    st->calculate_time = 0;
    st->sort_time = 0;
    st->model_training_nums = 0;
    // st->max_read_lpn = 0;
    // st->min_read_lpn = INVALID_LPN;
    // st->max_write_lpn = 0;
    // st->min_write_lpn = INVALID_LPN;
    st->read_joule = 0;
    st->write_joule = 0;
    st->erase_joule = 0;
    st->joule = 0;
}

void ssd_init(FemuCtrl *n)
{
    printf("1111111************************11111\n");
    struct ssd *ssd = n->ssd;
    struct ssdparams *spp = &ssd->sp;

    ftl_assert(ssd);

    ssd_init_params(spp);

    /* initialize ssd internal layout architecture */
    ssd->ch = g_malloc0(sizeof(struct ssd_channel) * spp->nchs);
    for (int i = 0; i < spp->nchs; i++) {
        ssd_init_ch(&ssd->ch[i], spp);
    }

    /* initialize maptbl */
    ssd_init_maptbl(ssd);

    /* initialize rmap */
    ssd_init_rmap(ssd);

    // ! initialize bitmap
    ssd_init_bitmap(ssd);

    /* initialize all the lines */
    ssd_init_lines(ssd);
    
    //init Senodes
    ssd_init_Senode(ssd);
    
    // * init cmt
    ssd_init_cmt(ssd);

    /* initialize write pointer, this is how we allocate new pages for writes */
    ssd_init_write_pointer(ssd);

    ssd_init_statistics(ssd);

    ssd_init_all_models(ssd);

    ssd->model_used = true;

    qemu_thread_create(&ssd->ftl_thread, "FEMU-FTL-Thread", ftl_thread, n,
                       QEMU_THREAD_JOINABLE);
}

static inline bool valid_ppa(struct ssd *ssd, struct ppa *ppa)
{
    struct ssdparams *spp = &ssd->sp;
    int ch = ppa->g.ch;
    int lun = ppa->g.lun;
    int pl = ppa->g.pl;
    int blk = ppa->g.blk;
    int pg = ppa->g.pg;
    int sec = ppa->g.sec;

    if (ch >= 0 && ch < spp->nchs && lun >= 0 && lun < spp->luns_per_ch && pl >=
        0 && pl < spp->pls_per_lun && blk >= 0 && blk < spp->blks_per_pl && pg
        >= 0 && pg < spp->pgs_per_blk && sec >= 0 && sec < spp->secs_per_pg)
        return true;

    return false;
}

static inline bool valid_lpn(struct ssd *ssd, uint64_t lpn)
{
    return (lpn < ssd->sp.tt_pgs);
}

static inline bool mapped_ppa(struct ppa *ppa)
{
    return !(ppa->ppa == UNMAPPED_PPA);
}

static inline struct ssd_channel *get_ch(struct ssd *ssd, struct ppa *ppa)
{
    return &(ssd->ch[ppa->g.ch]);
}

static inline struct nand_lun *get_lun(struct ssd *ssd, struct ppa *ppa)
{
    struct ssd_channel *ch = get_ch(ssd, ppa);
    return &(ch->lun[ppa->g.lun]);
}

static inline struct nand_plane *get_pl(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_lun *lun = get_lun(ssd, ppa);
    return &(lun->pl[ppa->g.pl]);
}

static inline struct nand_block *get_blk(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_plane *pl = get_pl(ssd, ppa);
    return &(pl->blk[ppa->g.blk]);
}

static inline struct line *get_line(struct ssd *ssd, struct ppa *ppa)
{
    return &(ssd->lm.lines[ppa->g.blk]);
}

static inline struct nand_page *get_pg(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_block *blk = get_blk(ssd, ppa);
    return &(blk->pg[ppa->g.pg]);
}

static inline struct ppa get_gtd_ent(struct ssd *ssd, uint64_t tvpn) {
    //struct ssdparams *spp = &ssd->sp;
    //int gtd_index = lpn / spp->ents_per_pg;
    return ssd->gtd[tvpn];
}

static inline struct ppa get_gtd_ent_index(struct ssd *ssd, uint64_t index) {
    return ssd->gtd[index];
}

static inline void set_gtd_ent(struct ssd *ssd, struct ppa *gtd_ppa, uint64_t index) {

    ssd->gtd[index] = *gtd_ppa;
}

static uint64_t ssd_advance_status(struct ssd *ssd, struct ppa *ppa, struct
        nand_cmd *ncmd)
{
    int c = ncmd->cmd;
    uint64_t cmd_stime = (ncmd->stime == 0) ? \
        qemu_clock_get_ns(QEMU_CLOCK_REALTIME) : ncmd->stime;
    uint64_t nand_stime;
    struct ssdparams *spp = &ssd->sp;
    struct nand_lun *lun = get_lun(ssd, ppa);
    uint64_t lat = 0;
    // bool flag=false;
    // if (ppa->g.ch==0&&ppa->g.lun==0)
    //     flag=true;

    switch (c) {
    case NAND_READ:
        /* read: perform NAND cmd first */
        nand_stime = (lun->next_lun_avail_time < cmd_stime) ? cmd_stime : \
                     lun->next_lun_avail_time;
        lun->next_lun_avail_time = nand_stime + spp->pg_rd_lat;
        lat = lun->next_lun_avail_time - cmd_stime;
        ssd->stat.read_joule += 3.5;
#if 0
        lun->next_lun_avail_time = nand_stime + spp->pg_rd_lat;

        /* read: then data transfer through channel */
        chnl_stime = (ch->next_ch_avail_time < lun->next_lun_avail_time) ? \
            lun->next_lun_avail_time : ch->next_ch_avail_time;
        ch->next_ch_avail_time = chnl_stime + spp->ch_xfer_lat;

        lat = ch->next_ch_avail_time - cmd_stime;
#endif
        break;

    case NAND_WRITE:
        ssd->stat.write_num++;
        /* write: transfer data through channel first */
        nand_stime = (lun->next_lun_avail_time < cmd_stime) ? cmd_stime : \
                     lun->next_lun_avail_time;
        if (ncmd->type == USER_IO) {
            lun->next_lun_avail_time = nand_stime + spp->pg_wr_lat;
        } else {
            lun->next_lun_avail_time = nand_stime + spp->pg_wr_lat;
        }
        lat = lun->next_lun_avail_time - cmd_stime;
        ssd->stat.write_joule += 16.7;

#if 0
        chnl_stime = (ch->next_ch_avail_time < cmd_stime) ? cmd_stime : \
                     ch->next_ch_avail_time;
        ch->next_ch_avail_time = chnl_stime + spp->ch_xfer_lat;

        /* write: then do NAND program */
        nand_stime = (lun->next_lun_avail_time < ch->next_ch_avail_time) ? \
            ch->next_ch_avail_time : lun->next_lun_avail_time;
        lun->next_lun_avail_time = nand_stime + spp->pg_wr_lat;

        lat = lun->next_lun_avail_time - cmd_stime;
#endif
        break;

    case NAND_ERASE:
        /* erase: only need to advance NAND status */
        nand_stime = (lun->next_lun_avail_time < cmd_stime) ? cmd_stime : \
                     lun->next_lun_avail_time;
        lun->next_lun_avail_time = nand_stime + spp->blk_er_lat;
        // if (flag) {
        //         fprintf(gc_fp, "%ld\n",lun->next_lun_avail_time);
        //         flag = true;
        // }

        lat = lun->next_lun_avail_time - cmd_stime;
        ssd->stat.erase_joule += 132;
        break;

    default:
        ftl_err("Unsupported NAND command: 0x%x\n", c);
    }

    return lat;
}

/* update SSD status about one page from PG_VALID -> PG_VALID */
static void mark_page_invalid(struct ssd *ssd, struct ppa *ppa)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_block *blk = NULL;
    struct nand_page *pg = NULL;
    struct line *line;

    /* update corresponding page status */
    pg = get_pg(ssd, ppa);
    ftl_assert(pg->status == PG_VALID);
    pg->status = PG_INVALID;

    /* update corresponding block status */
    blk = get_blk(ssd, ppa);
    ftl_assert(blk->ipc >= 0 && blk->ipc < spp->pgs_per_blk);
    blk->ipc++;
    ftl_assert(blk->vpc > 0 && blk->vpc <= spp->pgs_per_blk);
    blk->vpc--;

    /* update corresponding line status */
    line = get_line(ssd, ppa);
    ftl_assert(line->ipc >= 0 && line->ipc < spp->pgs_per_line);
    if (line->vpc == spp->pgs_per_line) {
        ftl_assert(line->ipc == 0);
        // was_full_line = true;
    }
    line->ipc++;
    line->vpc--;
    ftl_assert(line->vpc > 0 && line->vpc <= spp->pgs_per_line);
    /* Adjust the position of the victime line in the pq under over-writes */
    // if (line->pos) {
    //     /* Note that line->vpc will be updated by this call */
    //     pqueue_change_priority(lm->victim_line_pq, line->vpc - 1, line);
    // } else {
    //     line->vpc--;
    // }

    // if (was_full_line) {
        /* move line: "full" -> "victim" */
        // QTAILQ_REMOVE(&lm->full_line_list, line, entry);
        // lm->full_line_cnt--;
        // pqueue_insert(lm->victim_line_pq, line);
        // lm->victim_line_cnt++;
    // }
}

static void mark_page_valid(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_block *blk = NULL;
    struct nand_page *pg = NULL;
    struct line *line;

    /* update page status */
    pg = get_pg(ssd, ppa);
    ftl_assert(pg->status == PG_FREE);
    pg->status = PG_VALID;

    /* update corresponding block status */
    blk = get_blk(ssd, ppa);
    ftl_assert(blk->vpc >= 0 && blk->vpc < ssd->sp.pgs_per_blk);
    blk->vpc++;

    /* update corresponding line status */
    line = get_line(ssd, ppa);
    ftl_assert(line->vpc >= 0 && line->vpc < ssd->sp.pgs_per_line);
    line->vpc++;
}

static void mark_block_free(struct ssd *ssd, struct ppa *ppa)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_block *blk = get_blk(ssd, ppa);
    struct nand_page *pg = NULL;

    for (int i = 0; i < spp->pgs_per_blk; i++) {
        /* reset page status */
        pg = &blk->pg[i];
        ftl_assert(pg->nsecs == spp->secs_per_pg);
        pg->status = PG_FREE;
    }

    /* reset block status */
    ftl_assert(blk->npgs == spp->pgs_per_blk);
    blk->ipc = 0;
    blk->vpc = 0;
    blk->erase_cnt++;
}

static struct Cmt_Senode *cmt_hit_no_move(struct ssd *ssd, uint64_t lpn)
{

    //struct Seg *seg = NULL;
    struct hash_table *ht = &ssd->cm.ht;
    uint64_t tvpn = lpn / (ssd->sp.ents_per_pg);

    return find_hash_cmt_senode(ht, tvpn);

    // QTAILQ_FOREACH(curTP, &cm->TPnode_list, entry) {
    //     if (curTP->tvpn == tvpn) {
    //         QTAILQ_FOREACH(cmt_entry, &curTP->cmt_entry_list, entry) {
    //             if (cmt_entry->lpn == lpn) break;
    //         }
    //         break;
    //     }
    // }

    // curTP = QTAILQ_FIRST(&cm->TPnode_list);
    // if (curTP->tvpn == tvpn) {
    //     QTAILQ_FOREACH(cmt_entry, &curTP->cmt_entry_list, entry) {
    //         if (cmt_entry->lpn == lpn) break;
    //     }
    // } else {
    //     printf("error in cmt_hit_no_move! TPnode not in the first\n");
    // }


}

static uint64_t translation_write_page(struct ssd *ssd, uint64_t tvpn)
{
    //struct ssdparams *spp = &ssd->sp;
    struct ppa new_gtd_ppa = get_new_line_page(ssd, &ssd->trans_wp);

    // * 4.6. update the gtd
    
    //uint64_t index = tvpn;
    set_gtd_ent(ssd, &new_gtd_ppa, tvpn);
    set_rmap_ent(ssd, tvpn, &new_gtd_ppa);
    mark_page_valid(ssd, &new_gtd_ppa);
    // struct nand_cmd srd;
    // srd.type = USER_IO;
    // srd.cmd = NAND_WRITE;
    // srd.stime = 0;  // req->stime?
    // ssd_advance_status(ssd, &new_gtd_ppa, &srd);

    advance_line_write_pointer(ssd, &ssd->trans_wp);

    return 0;
}


static inline uint64_t translation_read_page_no_req(struct ssd *ssd, struct ppa *ppa)
{
    uint64_t lat = 0;
    struct nand_cmd trd;
    trd.type = USER_IO;
    trd.cmd = NAND_READ;
    trd.stime = 0;
    lat = ssd_advance_status(ssd, ppa, &trd);
    
    return lat;
}

static uint64_t translation_read_page(struct ssd *ssd, NvmeRequest *req, struct ppa *ppa)
{
    uint64_t lat = 0;
    struct nand_cmd trd;
    trd.type = USER_IO;
    trd.cmd = NAND_READ;
    trd.stime = 0;
    lat = ssd_advance_status(ssd, ppa, &trd);
    
    return lat;
}

//这个函数返回CMT_Senode*为空表示没有命中
static struct Cmt_Senode*cmt_hit(struct ssd *ssd, uint64_t tvpn)
{
    //struct ssdparams *spp = &ssd->sp;
    struct cmt_mgmt *cm = &ssd->cm;
    //uint64_t tvpn = lpn / spp->ents_per_pg;
    struct Cmt_Senode *curSe = NULL;
    struct hash_table *ht = &cm->ht;

    curSe = find_hash_cmt_senode(ht, tvpn);
    
    if (curSe != NULL) {
        QTAILQ_REMOVE(&cm->Cmt_Senode_list, curSe, entry);
        QTAILQ_INSERT_HEAD(&cm->Cmt_Senode_list, curSe, entry);
    }
    return curSe;
}

//将CMT_Senode插入到CMT中并将cm中tt_entries减去Senode的大小
static void insert_cmt_senode_to_cmt(struct ssd *ssd, uint64_t tvpn,uint64_t next_avail_time)
{
    //struct ssdparams *spp = &ssd->sp;
    struct cmt_mgmt *cm = &ssd->cm;
    struct hash_table *ht = &cm->ht;
    struct Cmt_Senode *cmt_senode;
    
    //struct Seg *cmt_seg;
    //uint64_t tvpn = lpn / spp->ents_per_pg;

    // cmt_entry = QTAILQ_FIRST(&cm->free_cmt_entry_list);
    // if (cmt_entry == NULL) {
    //     ftl_err("no cmt_entry in free cmt entry list");
    // }
    // QTAILQ_REMOVE(&cm->free_cmt_entry_list, cmt_entry, entry);
    // cm->free_cmt_entry_cnt--;
    // cmt_entry->lpn = lpn;
    // cmt_entry->ppn = ppn;
    // cmt_entry->dirty = CLEAN;
    // cmt_entry->prefetch = prefetch;
    // cmt_entry->next_avail_time = next_avail_time;
    // cmt_entry->next = NULL;
 
    //find tpnode
    // QTAILQ_FOREACH(tpnode, &cm->TPnode_list, entry) {
    //     if (tpnode->tvpn == tvpn) {
    //         //insert entry
    //         if (pos == HEAD) {
    //             QTAILQ_INSERT_HEAD(&tpnode->cmt_entry_list, cmt_entry, entry);
    //         } else {
    //             QTAILQ_INSERT_TAIL(&tpnode->cmt_entry_list, cmt_entry, entry);
    //         }
    //         tpnode->cmt_entry_cnt++;
    //         break;
    //     }
    // }
    /* already pretreat TPnode position */
    cmt_senode = QTAILQ_FIRST(&cm->Cmt_Senode_list);
    if (cmt_senode == NULL || cmt_senode->tvpn != tvpn) {
        //create and insert tpnode to TPnode_list
        cmt_senode = g_malloc0(sizeof(struct Cmt_Senode));
        cmt_senode->tvpn = tvpn;
        cmt_senode->next = NULL;
        cmt_senode->next_avail_time = next_avail_time;
        cmt_senode->dirty = CLEAN;
        
        // QTAILQ_INIT(&tpnode->cmt_entry_list);
        // QTAILQ_INSERT_HEAD(&tpnode->cmt_entry_list, cmt_entry, entry);
        // if (pos == HEAD)
        QTAILQ_INSERT_HEAD(&cm->Cmt_Senode_list, cmt_senode, entry);
        // else
        //     QTAILQ_INSERT_TAIL(&cm->TPnode_list, tpnode, entry);

        insert_se_hashtable(ht,cmt_senode);
        cm->tt_entries -= ssd->senodes[tvpn].seg_count;
        cm->tt_Senodes++;
        // cm->counter++;
        // if (cm->counter == 3) {
        //     cm->counter = 0;
        //     spp->enable_select_prefetch = false;
        // }
    } 
    // else
    //  {
    //     //insert entry
    //     if (pos == HEAD) {
    //         QTAILQ_INSERT_HEAD(&tpnode->cmt_entry_list, cmt_entry, entry);
    //     } else {
    //         QTAILQ_INSERT_TAIL(&tpnode->cmt_entry_list, cmt_entry, entry);
    //     }
    //     tpnode->cmt_entry_cnt++;
    // }

    return ;
    // tpnode->exist_ent[lpn % spp->ents_per_pg] = 1;
    // cm->used_cmt_entry_cnt++;
    // insert_cmt_hashtable(ht, cmt_entry);
}

//如果cm的tt_entries<0就进行evict直到>0
static  void evict_CMT_Senode_from_cmt(struct ssd *ssd)
{
    //struct ssdparams *spp = &ssd->sp;
    struct cmt_mgmt *cm = &ssd->cm;
    struct Cmt_Senode *cmt_senode;
    // struct cmt_entry *cmt_entry, *tmp_entry;
    uint64_t tvpn;
    struct ppa gtd_ppa;
    // int tpnode_evict_flag = 0;
    struct hash_table *ht = &cm->ht;

    
    while(cm->tt_entries<0)
    {
        //find coldest tpnode
        cmt_senode = QTAILQ_LAST(&cm->Cmt_Senode_list);


        if(cmt_senode->dirty==DIRTY)
        {
            tvpn = cmt_senode->tvpn;
            gtd_ppa = get_gtd_ent(ssd, tvpn);
        if (mapped_ppa(&gtd_ppa)) {
            translation_read_page_no_req(ssd, &gtd_ppa);

            mark_page_invalid(ssd, &gtd_ppa);
            set_rmap_ent(ssd, INVALID_LPN, &gtd_ppa);
        }
        //printf("write page 000\n");
        translation_write_page(ssd, tvpn);
        //printf("write page 111\n");
        }

        cm->tt_entries += ((ssd->senodes)[cmt_senode->tvpn]).seg_count;
        QTAILQ_REMOVE(&cm->Cmt_Senode_list, cmt_senode, entry);
        delete_cmt_senode_hashnode(ht,cmt_senode);
        cm->tt_Senodes--;
        if(cm->tt_Senodes<0)
        {
            printf("error:%d: tt_Senode < 0!!!\n",__LINE__);
        }
        

        g_free(cmt_senode);
        cmt_senode = NULL;
    }
    
    
    // if (tpnode->cmt_entry_cnt == 0)
    //     ftl_err("tpnode cannot be empty!");
    // //find coldest clean entry
    // QTAILQ_FOREACH_REVERSE(cmt_entry, &tpnode->cmt_entry_list, entry) {
    //     if (cmt_entry->dirty == CLEAN) break;
    // }
    // //if no clean entry exists, find first dirty entry
    // if (cmt_entry == NULL) {
    //     cmt_entry = QTAILQ_LAST(&tpnode->cmt_entry_list);
    // }
    // //evict entry in tpnode
    // QTAILQ_REMOVE(&tpnode->cmt_entry_list, cmt_entry, entry);
    // tpnode->cmt_entry_cnt--;

    // tpnode->exist_ent[cmt_entry->lpn % spp->ents_per_pg] = 0;

    // if (cmt_entry->dirty == DIRTY) {
    //     tvpn = cmt_entry->lpn;
    //     gtd_ppa = get_gtd_ent(ssd, tvpn);
    //     if (mapped_ppa(&gtd_ppa)) {
    //         translation_read_page_no_req(ssd, &gtd_ppa);

    //         mark_page_invalid(ssd, &gtd_ppa);
    //         set_rmap_ent(ssd, INVALID_LPN, &gtd_ppa);
    //     }
        

        
    //     translation_write_page(ssd, tvpn);

    //     //all dirty entry in TPnode update and write to a new translation page
    //     QTAILQ_FOREACH(tmp_entry, &tpnode->cmt_entry_list, entry) {
    //         if (tmp_entry->dirty == DIRTY)
    //             tmp_entry->dirty = CLEAN;
    //     }
    // }

    // delete_cmt_hashnode(ht, cmt_entry);

    // //insert evicted entry to free_entry_list
    // cmt_entry->dirty = CLEAN;
    // cmt_entry->lpn = INVALID_LPN;
    // cmt_entry->ppn = UNMAPPED_PPA;
    // cmt_entry->prefetch = false;
    // cmt_entry->next_avail_time = 0;

    // QTAILQ_INSERT_TAIL(&cm->free_cmt_entry_list, cmt_entry, entry);
    // cm->free_cmt_entry_cnt++;
    // cm->used_cmt_entry_cnt--;
    // //if no entry in tpnode, evict tpnode
    // if (tpnode->cmt_entry_cnt == 0) {
    //     QTAILQ_REMOVE(&cm->TPnode_list, tpnode, entry);

    //     delete_tp_hashnode(ht, tpnode);

    //     cm->tt_TPnodes--;
    //     g_free(tpnode);
    //     tpnode = NULL;
    //     tpnode_evict_flag = 1;
    //     cm->counter--;
    //     if (cm->counter == -3) {
    //         cm->counter = 0;
    //         spp->enable_select_prefetch = true;
    //     }
    // }

    // return tpnode_evict_flag;
}

//读tvpn对应的翻译页同时将Senode插入到CMT中
static struct nand_lun *process_translation_page_read(struct ssd *ssd, NvmeRequest *req, uint64_t tvpn)
{
    //struct ssdparams *spp = &ssd->sp;
    struct ppa  tppa;
    //uint64_t lpn = start_lpn, new_lpn = start_lpn + 1, last_lpn, ppn, new_ppn;
    struct cmt_mgmt *cm = &ssd->cm;
    //struct Cmt_Senode *cmt_senode;
    // struct cmt_entry *cmt_entry;
    uint64_t  next_avail_time;
    //int terminate_flag = 0;
    struct nand_lun *old_lun;

    //get gtd mapping physical page
    //tvpn = lpn / spp->ents_per_pg;

    tppa = get_gtd_ent_index(ssd, tvpn);
    if (!mapped_ppa(&tppa) || !valid_ppa(ssd, &tppa)) {
        //printf("%s,lpn(%" PRId64 ") not mapped to valid ppa\n", ssd->ssdname, lpn);
        //printf("Invalid ppa,ch:%d,lun:%d,blk:%d,pl:%d,pg:%d,sec:%d\n",
        //ppa.g.ch, ppa.g.lun, ppa.g.blk, ppa.g.pl, ppa.g.pg, ppa.g.sec);
        return NULL;
    }

    //get real lpn-ppn
    // ppa = get_maptbl_ent(ssd, lpn);
    // if (!mapped_ppa(&ppa) || !valid_ppa(ssd, &ppa)) {
    //     // translation_read_page(ssd, req, &tppa);
    //     return NULL;
    // } else {
        //read latency
    translation_read_page(ssd, req, &tppa);
    old_lun = get_lun(ssd, &tppa);
    next_avail_time = old_lun->next_lun_avail_time;
    //ppn = ppa2pgidx(ssd, &ppa);
    insert_cmt_senode_to_cmt(ssd,tvpn,next_avail_time);
    //insert_entry_to_cmt(ssd, tvpn, next_avail_time);

     if (cm->tt_entries<0) {
        //terminate_flag = evict_CMT_Senode_from_cmt(ssd);
        evict_CMT_Senode_from_cmt(ssd);
    }
    //}

    // /* when tpnode has not been evicted, execute request-level prefetching, 
    //    end_lpn is the end of translation page or end of request, when evict
    //    a TPnode or at the end of translation page/request, stop */
    // if (spp->enable_request_prefetch && !terminate_flag) {
    //     for (new_lpn = start_lpn + 1; new_lpn <= end_lpn; new_lpn++) {
    //         if (cmt_hit_no_move(ssd, new_lpn)) continue;

    //         new_ppa = get_maptbl_ent(ssd, new_lpn);
    //         if (!mapped_ppa(&new_ppa) || !valid_ppa(ssd, &new_ppa)) {
    //             //printf("%s,lpn(%" PRId64 ") not mapped to valid ppa\n", ssd->ssdname, lpn);
    //             //printf("Invalid ppa,ch:%d,lun:%d,blk:%d,pl:%d,pg:%d,sec:%d\n",
    //             //ppa.g.ch, ppa.g.lun, ppa.g.blk, ppa.g.pl, ppa.g.pg, ppa.g.sec);
    //             continue;
    //         }
    //         new_ppn = ppa2pgidx(ssd, &new_ppa);
    //         if (cm->used_cmt_entry_cnt == cm->tt_entries) {
    //             terminate_flag = evict_entry_from_cmt(ssd);
    //         }
    //         insert_entry_to_cmt(ssd, new_lpn, new_ppn, TAIL, true, next_avail_time);
    //         if (terminate_flag) break;
    //     }
    // }

    // /* when tpnode has not been evicted, execute selective prefetching 
    //    when at the end of a translation page or a TPnode is evicted, stop */
    // if (spp->enable_select_prefetch && !terminate_flag) {
    //     last_lpn = (lpn / spp->ents_per_pg + 1) * spp->ents_per_pg - 1;
    //     /* last requeset lpn still small than the end of translation page */
    //     if (new_lpn <= last_lpn) {
    //         // int exist_ent[spp->ents_per_pg], 
    //         int index = lpn % spp->ents_per_pg - 1, cnt = 0;
    //         // for (int i = 0; i < spp->ents_per_pg; i++) {
    //         //     exist_ent[i] = 0;
    //         // }
    //         // QTAILQ_FOREACH(tpnode, &cm->TPnode_list, entry) {
    //         //     if (tpnode->tvpn == tvpn) {
    //         //         QTAILQ_FOREACH(cmt_entry, &tpnode->cmt_entry_list, entry) {
    //         //             exist_ent[cmt_entry->lpn % spp->ents_per_pg] = 1;
    //         //         }
    //         //         while (index >= 0 && exist_ent[index] == 1) {
    //         //             index--;
    //         //             cnt++;
    //         //         }
    //         //         break;
    //         //     }
    //         // }
    //         tpnode = QTAILQ_FIRST(&cm->TPnode_list);
    //         if (tpnode->tvpn == tvpn) {
    //             // QTAILQ_FOREACH(cmt_entry, &tpnode->cmt_entry_list, entry) {
    //             //     exist_ent[cmt_entry->lpn % spp->ents_per_pg] = 1;
    //             // }
    //             while (index >= 0 && tpnode->exist_ent[index] == 1) {
    //                 index--;
    //                 cnt++;
    //             }
    //         } else {
    //             printf("error! tpnode is not in the first of list\n");
    //         }

    //         if (lpn + cnt >= new_lpn) {
    //             last_lpn = (lpn + cnt) > last_lpn ? last_lpn : (lpn + cnt);
    //             for (; new_lpn <= last_lpn; new_lpn++) {
    //                 // if (cnt == 0) break;
    //                 // cnt--;
    //                 if (cmt_hit_no_move(ssd, new_lpn)) continue;
    //                 new_ppa = get_maptbl_ent(ssd, new_lpn);
    //                 if (cm->used_cmt_entry_cnt == cm->tt_entries) {
    //                     terminate_flag = evict_entry_from_cmt(ssd);
    //                 }
    //                 if (!mapped_ppa(&new_ppa) || !valid_ppa(ssd, &new_ppa)) {
    //                     insert_entry_to_cmt(ssd, new_lpn, UNMAPPED_PPA, TAIL, true, next_avail_time);
    //                 } else {
    //                     new_ppn = ppa2pgidx(ssd, &new_ppa);
    //                     insert_entry_to_cmt(ssd, new_lpn, new_ppn, TAIL, true, next_avail_time);
    //                 }

    //                 if (terminate_flag) break;
    //             }
    //         }
    //     }
    // }

    return old_lun;
}

//将翻译页对应的senode加入到CMT中，如果不存在那么就直接加入一个空的就行
static struct nand_lun *process_translation_page_write(struct ssd *ssd, NvmeRequest *req, uint64_t tvpn)
{
    
    //struct ssdparams *spp = &ssd->sp;
    struct ppa ppa;
    //uint64_t lpn = start_lpn, new_lpn = start_lpn + 1, last_lpn, ppn, new_ppn;
    struct cmt_mgmt *cm = &ssd->cm;
    //struct Cmt_Senode *cmt_senode;
    
    // struct cmt_entry *cmt_entry;
    uint64_t next_avail_time;
    //int terminate_flag = 0;
    struct nand_lun *old_lun;
    
    //get gtd mapping physical page
    //tvpn = lpn / spp->ents_per_pg;
    ppa = get_gtd_ent_index(ssd, tvpn);
    
    // if (cm->used_cmt_entry_cnt == cm->tt_entries) {
    //     terminate_flag = evict_entry_from_cmt(ssd);
    // }
    /* if it is a new write, not an update */
    if (!mapped_ppa(&ppa) || !valid_ppa(ssd, &ppa)) {
        //insert_entry_to_cmt(ssd, lpn, UNMAPPED_PPA, HEAD, false, 0);
        if(((ssd->senodes)[tvpn]).seg_count!=0)
        {
            printf("error:%d:tvpn is UNmapped but seg_count not zero!!!\n",__LINE__);
        }
        insert_cmt_senode_to_cmt(ssd,tvpn,0);
        old_lun = NULL;
    }
    else 
    {
        translation_read_page(ssd, req, &ppa);
        old_lun = get_lun(ssd, &ppa);
        next_avail_time = old_lun->next_lun_avail_time;
        //ppn = ppa2pgidx(ssd, &ppa);
        insert_cmt_senode_to_cmt(ssd,tvpn,next_avail_time);
        //insert_entry_to_cmt(ssd, tvpn, next_avail_time);
        //printf("0000000\n");
         if (cm->tt_entries<0) {
            //terminate_flag = evict_CMT_Senode_from_cmt(ssd);
            //printf("11**111\n");
            evict_CMT_Senode_from_cmt(ssd);
            //printf("22**222\n");
        }
        //printf("00001\n");
    }
        //read latency
        // translation_read_page(ssd, req, &ppa);
        // old_lun = get_lun(ssd, &ppa);
        // next_avail_time = old_lun->next_lun_avail_time;
        // //get real lpn-ppn
        // ppa = get_maptbl_ent(ssd, lpn);

        // if (!mapped_ppa(&ppa) || !valid_ppa(ssd, &ppa)) {
        //     //insert_entry_to_cmt(ssd, lpn, UNMAPPED_PPA, HEAD, false, next_avail_time);
        //     insert_cmt_senode_to_cmt(ssd,tvpn,next_avail_time);
        // } else {
        //     ppn = ppa2pgidx(ssd, &ppa);

        //     insert_entry_to_cmt(ssd, lpn, ppn, HEAD, false, next_avail_time);
            
        // }
    //     /* when tpnode has not been evicted, execute request-level prefetching, 
    //     end_lpn is the end of translation page or end of request, when evict
    //     a TPnode or at the end of translation page/request, stop */
    //     if (spp->enable_request_prefetch && !terminate_flag) {
    //         for (new_lpn = start_lpn + 1; new_lpn <= end_lpn; new_lpn++) {
    //             if (cmt_hit_no_move(ssd, new_lpn)) continue;

    //             new_ppa = get_maptbl_ent(ssd, new_lpn);
    //             if (cm->used_cmt_entry_cnt == cm->tt_entries) {
    //                 terminate_flag = evict_entry_from_cmt(ssd);
    //             }
    //             if (!mapped_ppa(&new_ppa) || !valid_ppa(ssd, &new_ppa)) {
    //                 insert_entry_to_cmt(ssd, new_lpn, UNMAPPED_PPA, TAIL, true, next_avail_time);
    //             } else {
    //                 new_ppn = ppa2pgidx(ssd, &new_ppa);
    //                 insert_entry_to_cmt(ssd, new_lpn, new_ppn, TAIL, true, next_avail_time);
    //             }

    //             if (terminate_flag) break;
    //         }
    //     }
    //     /* when tpnode has not been evicted, execute selective prefetching 
    //     when at the end of a translation page or a TPnode is evicted, stop */
    //     if (spp->enable_select_prefetch && !terminate_flag) {
    //         last_lpn = (lpn / spp->ents_per_pg + 1) * spp->ents_per_pg - 1;
    //         /* last requeset lpn still small than the end of translation page */
    //         if (new_lpn <= last_lpn) {
    //             // int exist_ent[spp->ents_per_pg], 
    //             int index = lpn % spp->ents_per_pg - 1, cnt = 0;
    //             // for (int i = 0; i < spp->ents_per_pg; i++) {
    //             //     exist_ent[i] = 0;
    //             // }
    //             // QTAILQ_FOREACH(tpnode, &cm->TPnode_list, entry) {
    //             //     if (tpnode->tvpn == tvpn) {
    //             //         QTAILQ_FOREACH(cmt_entry, &tpnode->cmt_entry_list, entry) {
    //             //             exist_ent[cmt_entry->lpn % spp->ents_per_pg] = 1;
    //             //         }
    //             //         while (index >= 0 && exist_ent[index] == 1) {
    //             //             index--;
    //             //             cnt++;
    //             //         }
    //             //         break;
    //             //     }
    //             // }
    //             tpnode = QTAILQ_FIRST(&cm->TPnode_list);
    //             if (tpnode->tvpn == tvpn) {
    //                 // QTAILQ_FOREACH(cmt_entry, &tpnode->cmt_entry_list, entry) {
    //                 //     exist_ent[cmt_entry->lpn % spp->ents_per_pg] = 1;
    //                 // }
    //                 while (index >= 0 && tpnode->exist_ent[index] == 1) {
    //                     index--;
    //                     cnt++;
    //                 }
    //             } else {
    //                 printf("error! tpnode not in the first of list\n");
    //             }

    //             if (lpn + cnt >= new_lpn) {
    //                 last_lpn = (lpn + cnt) > last_lpn ? last_lpn : (lpn + cnt);
    //                 for (; new_lpn <= last_lpn; new_lpn++) {
    //                     // if (cnt == 0) break;
    //                     // cnt--;
    //                     if (cmt_hit_no_move(ssd, new_lpn)) continue;

    //                     new_ppa = get_maptbl_ent(ssd, new_lpn);
    //                     if (cm->used_cmt_entry_cnt == cm->tt_entries) {
    //                         terminate_flag = evict_entry_from_cmt(ssd);
    //                     }
    //                     if (!mapped_ppa(&new_ppa) || !valid_ppa(ssd, &new_ppa)) {
    //                         insert_entry_to_cmt(ssd, new_lpn, UNMAPPED_PPA, TAIL, true, next_avail_time);
    //                     } else {
    //                         new_ppn = ppa2pgidx(ssd, &new_ppa);
    //                         insert_entry_to_cmt(ssd, new_lpn, new_ppn, TAIL, true, next_avail_time);
    //                     }

    //                     if (terminate_flag) break;
    //                 }
    //             }
    //         }
    //     }
    // }

    return old_lun;
}


static void gc_read_page(struct ssd *ssd, struct ppa *ppa)
{
    /* advance ssd status, we don't care about how long it takes */
    if (ssd->sp.enable_gc_delay) {
        struct nand_cmd gcr;
        gcr.type = GC_IO;
        gcr.cmd = NAND_READ;
        gcr.stime = 0;
        ssd_advance_status(ssd, ppa, &gcr);
    }
}

/* move valid page data (already in DRAM) from victim line to a new page */
// static uint64_t gc_write_page_through_gtd_wp(struct ssd *ssd, struct ppa *old_ppa, uint64_t lpn, struct write_pointer* wpp)
// {
//     struct ppa new_ppa;
//     struct nand_lun *new_lun;

//     ftl_assert(valid_lpn(ssd, lpn));

//     advance_line_write_pointer(ssd, wpp);

//     // new_ppa = get_new_page(ssd);
//     new_ppa = get_new_line_page(ssd, wpp);
//     /* update maptbl */
//     ssd->gtd[lpn] = new_ppa;
//     /* update rmap */

//     mark_page_valid(ssd, &new_ppa);

//     /* need to advance the write pointer here */
//     // ssd_advance_write_pointer(ssd);
//     // advance_line_write_pointer(ssd, wpp);

//     if (ssd->sp.enable_gc_delay) {
//         struct nand_cmd gcw;
//         gcw.type = GC_IO;
//         gcw.cmd = NAND_WRITE;
//         gcw.stime = 0;
//         ssd_advance_status(ssd, &new_ppa, &gcw);
//     }

//     /* advance per-ch gc_endtime as well */
// #if 0
//     new_ch = get_ch(ssd, &new_ppa);
//     new_ch->gc_endtime = new_ch->next_ch_avail_time;
// #endif

//     new_lun = get_lun(ssd, &new_ppa);
//     new_lun->gc_endtime = new_lun->next_lun_avail_time;

//     return 0;
// }


static uint64_t gc_write_page_through_line_wp(struct ssd *ssd, uint64_t lpn, struct ppa *new_ppa, struct write_pointer* wpp)
{
    // struct ppa new_ppa;
    struct nand_lun *new_lun;
    // uint64_t lpn = get_rmap_ent(ssd, old_ppa);

    ftl_assert(valid_lpn(ssd, lpn));

    if (wpp->curline->rest < (ssd->sp).pgs_per_line)
        advance_line_write_pointer(ssd, wpp);

    // new_ppa = get_new_page(ssd);
    *new_ppa = get_new_line_page(ssd, wpp);
    /* update maptbl */
    set_maptbl_ent(ssd, lpn, new_ppa);
    /* update rmap */
    set_rmap_ent(ssd, lpn, new_ppa);

    mark_page_valid(ssd, new_ppa);

    /* need to advance the write pointer here */
    // ssd_advance_write_pointer(ssd);
    // advance_line_write_pointer(ssd, wpp);

    if (ssd->sp.enable_gc_delay) {
        struct nand_cmd gcw;
        gcw.type = GC_IO;
        gcw.cmd = NAND_WRITE;
        gcw.stime = 0;
        ssd_advance_status(ssd, new_ppa, &gcw);
    }

    /* advance per-ch gc_endtime as well */
#if 0
    new_ch = get_ch(ssd, &new_ppa);
    new_ch->gc_endtime = new_ch->next_ch_avail_time;
#endif

    new_lun = get_lun(ssd, new_ppa);
    new_lun->gc_endtime = new_lun->next_lun_avail_time;

    return 0;
}

// static struct line *select_victim_line(struct ssd *ssd, bool force)
// {
//     struct line_mgmt *lm = &ssd->lm;
//     struct line *victim_line = NULL;

//     victim_line = pqueue_peek(lm->victim_line_pq);
//     if (!victim_line) {
//         return NULL;
//     }

//     if (!force && victim_line->ipc < ssd->sp.pgs_per_line / 8) {
//         return NULL;
//     }

//     pqueue_pop(lm->victim_line_pq);
//     victim_line->pos = 0;
//     lm->victim_line_cnt--;

//     /* victim_line is a danggling node now */
//     return victim_line;
// }

static void mark_line_free(struct ssd *ssd, struct ppa *ppa)
{
    struct line_mgmt *lm = &ssd->lm;
    struct line *line = get_line(ssd, ppa);
    line->ipc = 0;
    line->vpc = 0;
    line->rest = ssd->sp.pgs_per_line;
    /* move this line to free line list */
    QTAILQ_REMOVE(&lm->victim_list, line, entry);

    QTAILQ_INSERT_TAIL(&lm->free_line_list, line, entry);
    lm->victim_line_cnt--;
    lm->free_line_cnt++;
    ssd->line2write_pointer[line->id] = NULL;
}

static uint64_t gc_translation_page_write(struct ssd *ssd, struct ppa *old_ppa)
{
    struct ppa new_ppa;
    struct nand_lun *new_lun;
    uint64_t tvpn = get_rmap_ent(ssd, old_ppa);

    ftl_assert(valid_lpn(ssd, tvpn));
    new_ppa = get_new_line_page(ssd, &ssd->trans_wp);
    /* update GTD */
    set_gtd_ent(ssd, &new_ppa, tvpn);
    /* update rmap */
    set_rmap_ent(ssd, tvpn, &new_ppa);

    mark_page_valid(ssd, &new_ppa);

    /* need to advance the write pointer here */
    // advance_gc_trans_write_pointer(ssd, &ssd->trans_wp);
    advance_line_write_pointer(ssd, &ssd->trans_wp);

    // if (ssd->sp.enable_gc_delay) {
    //     struct nand_cmd gcw;
    //     gcw.type = GC_IO;
    //     gcw.cmd = NAND_WRITE;
    //     gcw.stime = 0;
    //     ssd_advance_status(ssd, &new_ppa, &gcw);
    // }

    /* advance per-ch gc_endtime as well */
#if 0
    new_ch = get_ch(ssd, &new_ppa);
    new_ch->gc_endtime = new_ch->next_ch_avail_time;
#endif

    new_lun = get_lun(ssd, &new_ppa);
    new_lun->gc_endtime = new_lun->next_lun_avail_time;

    return 0;
}

static void clean_one_trans_block(struct ssd *ssd, struct ppa *ppa)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_page *pg_iter = NULL;
    int cnt = 0;
    uint64_t lpn;
    // struct ppa equal_ppa;

    for (int pg = 0; pg < spp->pgs_per_blk; pg++) {
        ppa->g.pg = pg;
        pg_iter = get_pg(ssd, ppa);
        /* there shouldn't be any free page in victim blocks */
        ftl_assert(pg_iter->status != PG_FREE);
        if (pg_iter->status == PG_VALID) {
            gc_read_page(ssd, ppa);
            lpn = get_rmap_ent(ssd, ppa);
            struct ppa *equal_ppa = &ssd->gtd[lpn];
            // // ! some problems happen here!
            // if (!equal_ppa)
            //     continue;
            if (mapped_ppa(equal_ppa) && ppa2pgidx(ssd, equal_ppa) == ppa2pgidx(ssd, ppa)) {
                gc_translation_page_write(ssd, ppa);
            // }
            } else {
                printf("translation block contains data page!\n");
            }
            cnt++;
        }
    }

    ftl_assert(get_blk(ssd, ppa)->vpc == cnt);
}


static int gtd_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, struct line *victim_line, bool delete)
{   
    // printf("gtd do gc: %d\n", gc_line_num++);
    // fprintf(gc_fp, "%ld\n", counter);
    struct ssdparams *spp = &ssd->sp;
    struct nand_lun *lunp;

    struct ppa ppa;
    int ch, lun;
    

    ppa.g.blk = victim_line->id;
    ftl_debug("GC-ing line:%d,ipc=%d,victim=%d,full=%d,free=%d\n", ppa.g.blk,
              victim_line->ipc, ssd->lm.victim_line_cnt, ssd->lm.full_line_cnt,
              ssd->lm.free_line_cnt);

    for (ch = 0; ch < spp->nchs; ch++) {
        for (lun = 0; lun < spp->luns_per_ch; lun++) {
            ppa.g.ch = ch;
            ppa.g.lun = lun;
            ppa.g.pl = 0;

            // 先获取到block,然后消除block,针对每个page都进行重新写入（ssd_write(page)），之后进行擦除
            lunp = get_lun(ssd, &ppa);
            clean_one_trans_block(ssd, &ppa);
            mark_block_free(ssd, &ppa);

            //fxx:感觉这里需要进行擦除操作
            // if (spp->enable_gc_delay) {
            //     struct nand_cmd gce;
            //     gce.type = GC_IO;
            //     gce.cmd = NAND_ERASE;
            //     gce.stime = 0;
            //     ssd_advance_status(ssd, &ppa, &gce);
            // }

            lunp->gc_endtime = lunp->next_lun_avail_time;
        }
    }

    struct wp_lines *wpl = wpp->wpl;
    if (delete) {
        clear_one_write_pointer_victim_lines(wpl, victim_line);
    }

    /* update line status */
    mark_line_free(ssd, &ppa);

    

    return 0;
}



static void batch_gtd_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, int num, struct line *delete_line) {

    struct line *victim_line;
    struct wp_lines *wpl = wpp->wpl->next;
    int cnt = 0;
    while (wpl) {
        victim_line = wpl->line;
        if (victim_line->id == wpp->curline->id) {
            wpl = wpl->next;
            continue;
        }

        
        gtd_do_gc(ssd, force, wpp, victim_line, false);
        wpp->vic_cnt--;
        wpl = wpl->next;
        cnt++;
    }

    clear_all_write_pointer_victim_lines(wpl, wpp);
    return;

}

static void free_all_blocks(struct ssd *ssd, struct ppa *tppa) {
    struct ssdparams *spp = &ssd->sp;
    
    int ch, lun;
    struct ppa ppa;
    ppa.g.blk = tppa->g.blk;
    for (ch = 0; ch < spp->nchs; ch++) {
        for (lun = 0; lun < spp->luns_per_ch; lun++) {
            ppa.g.ch = ch;
            ppa.g.lun = lun;
            ppa.g.pl = 0;

            struct nand_lun *lunp = get_lun(ssd, &ppa);
            mark_block_free(ssd, &ppa);

            if (spp->enable_gc_delay) {
                struct nand_cmd gce;
                gce.type = GC_IO;
                gce.cmd = NAND_ERASE;
                gce.stime = 0;
                ssd_advance_status(ssd, &ppa, &gce);
            }

            lunp->gc_endtime = lunp->next_lun_avail_time;
        }
        
    }
}

//将数据页全部读出同时把这些数据页%nchs*luns_per_ch论文中是64进行分组这样能够提高程序的并行性，start_gtd表示这一个大组最开始的那个gtd号
static void gc_read_all_valid_data(struct ssd *ssd, struct ppa *tppa, uint64_t group_gtd_lpns[][512], int *group_gtd_index, int *start_gtd) {
    const int parallel = ssd->sp.tt_luns;
    struct ssdparams *spp = &ssd->sp;
    struct nand_lun *lunp;
    int ch, lun;
    struct ppa ppa;
    uint64_t tmp_lpn;
    ppa.g.blk = tppa->g.blk;
    uint64_t lat = 0;
    uint64_t use_lat = 0;
    for (ch = 0; ch < spp->nchs; ch++) {
        for (lun = 0; lun < spp->luns_per_ch; lun++) {
            ppa.g.ch = ch;
            ppa.g.lun = lun;
            ppa.g.pl = 0;

            lunp = get_lun(ssd, &ppa);


            struct nand_page *pg_iter = NULL;
            int cnt = 0;

            for (int pg = 0; pg < spp->pgs_per_blk; pg++) {
                ppa.g.pg = pg;
                pg_iter = get_pg(ssd, &ppa);
                /* there shouldn't be any free page in victim blocks */
                ftl_assert(pg_iter->status != PG_FREE);
                if (pg_iter->status == PG_VALID) {

                    // find which gtd a valid page belongs to
                    tmp_lpn = get_rmap_ent(ssd, &ppa);
                    int gtd_index = tmp_lpn/spp->ents_per_pg;
                    *start_gtd = gtd_index - (gtd_index % parallel);      // ! FIXME: 
                    int gtd_index_loc = gtd_index % spp->trans_per_line;    // gtd_index%64

                    // * check if there is invalid page
                    // if (group_gtd_index[gtd_index_loc] >= 512) {
                    //     for (int i = 0; i < 512; i++) {
                    //         if (tmp_lpn == group_gtd_lpns[gtd_index_loc][i]) {
                    //             printf("some pages are not invalid\n");
                    //         }
                    //     }
                    // }

                    group_gtd_lpns[gtd_index_loc][group_gtd_index[gtd_index_loc]++] = tmp_lpn;

                    gc_read_page(ssd, &ppa);
                    cnt++;
                }
            }
            
            mark_block_free(ssd, &ppa);
            
            if (spp->enable_gc_delay) {
                struct nand_cmd gce;
                gce.type = GC_IO;
                gce.cmd = NAND_ERASE;
                gce.stime = 0;
                lat = ssd_advance_status(ssd, &ppa, &gce);
                if (lat > use_lat) {
                    use_lat = lat;
                }
            }

            

            lunp->gc_endtime = lunp->next_lun_avail_time;
            ssd->stat.GC_time += use_lat;
            // clean_one_block_through_line_wp(ssd, &ppa, wpp);
            // mark_block_free(ssd, &ppa);

            // if (spp->enable_gc_delay) {
            //     struct nand_cmd gce;
            //     gce.type = GC_IO;
            //     gce.cmd = NAND_ERASE;
            //     gce.stime = 0;
            //     ssd_advance_status(ssd, &ppa, &gce);
            // }

            // lunp->gc_endtime = lunp->next_lun_avail_time;
        }
    }
}


static void model_training(struct ssd *ssd, struct write_pointer *wpp, uint64_t group_gtd_lpns[][512], int *group_gtd_index, int start_gtd) {
    // struct timespec time1, time2;
    //printf("Segmentftl Model Training...\n");
    ssd->stat.model_training_nums++;
    const int trans_ent = ssd->sp.ents_per_pg;
    const int parallel = ssd->sp.tt_luns;
    struct cmt_mgmt *cm = &ssd->cm;
    struct hash_table *ht = &cm->ht;
    //uint64_t train_lpns[parallel][trans_ent];
    uint64_t train_vppns[parallel][trans_ent];
    //int success = 0;
    int total = 0;
    struct Cmt_Senode* cmt_senode;
    // gc_line_num++;
   //const int interval_size = ssd->sp.interval_size;
    //const int segment_max_train_num = interval_size;
    for (int i = 0; i < parallel; i++) {
        total += group_gtd_index[i];

        // * first sort the lpns

        // clock_gettime(CLOCK_MONOTONIC, &time1);
        quick_sort(group_gtd_lpns[i], 0, group_gtd_index[i]-1);
        // clock_gettime(CLOCK_MONOTONIC, &time2);

        // ssd->stat.sort_time += ((time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));

        // * second write them back, collect the ppa
        for (int pgi = 0; pgi < group_gtd_index[i]; pgi++) {
            struct ppa tmp_ppa;
            gc_write_page_through_line_wp(ssd, group_gtd_lpns[i][pgi], &tmp_ppa, wpp);
            train_vppns[i][pgi] = ppa2vppn(ssd, &tmp_ppa);
        }
        uint64_t st = 0, en;
        uint64_t lpn,tvpn,slpn;
        tvpn = start_gtd + i;
        slpn = tvpn * trans_ent;
        //int old_node_count = ssd->senodes[tvpn].seg_count;
        cmt_senode = find_hash_cmt_senode(ht,tvpn);
        //printf("11111\n");
        if(cmt_senode)
        {
            //如果在cmt中那么需要更新CMT这里先去掉
            cm->tt_entries +=ssd->senodes[tvpn].seg_count;
            //这里应该是dirty但是为了和LearnedFTL算法对比LearnedFTL设计的时候这里没考虑gtd所以这里不用设置为DIRTY也不考虑gtd的重写进行对比。
            //cmt_senode->dirty = CLEAN;//不用设置为CLEAN不用考虑就行 
        }
        for(en=1;en<group_gtd_index[i];en++)
        {
            if(group_gtd_lpns[i][en]!=group_gtd_lpns[i][en-1]+1||train_vppns[i][en]!=train_vppns[i][en-1]+1)
            {
                //printf("slpn:%lld\tsequence_cnt:%lld\n",(long long)group_gtd_lpns[i][st],(long long)(en-st));
                lpn = group_gtd_lpns[i][st]-slpn;
                insert_seg2senode(ssd,lpn,en-st-1+lpn,train_vppns[i][st],tvpn);
                st = en;
            }
        }
        if(group_gtd_index[i]>0)
        {
            //printf("slpn:%lld\tsequence_cnt:%lld\n",(long long)group_gtd_lpns[i][st],(long long)(en-st));
            lpn = group_gtd_lpns[i][st]-slpn;
            insert_seg2senode(ssd,lpn,en-st-1+lpn,train_vppns[i][st],tvpn);
        }
        
        if(cmt_senode)
        {//再加回来
            cm->tt_entries -=ssd->senodes[tvpn].seg_count;
            //这里<0不用管不然会出现前边调用evict_CMT_Senode_from_cmt函数时这里又调用那就会报错，
            //因为前边要删除的CMT_Senode这里已经把它删除了CMT_Senode就会变为无效指针就会出错。
            //同时如果这里的cm->tt_entires<0当再次插入的时候会把它给evict了
            // if(cm->tt_entries<0)
            // evict_CMT_Senode_from_cmt(ssd);
        }
   }
}

static int batch_line_do_gc(struct ssd* ssd, bool force, struct write_pointer *wpp, struct line *delete_line) {

    struct ppa ppa;
    const int trans_ent = ssd->sp.ents_per_pg;
    const int parallel = ssd->sp.tt_luns;
    // printf("line batch do gc\n");
    uint64_t group_gtd_lpns[parallel][trans_ent];
    int group_gtd_index[parallel];
    memset(group_gtd_index, 0, sizeof(group_gtd_index));
    int start_gtd = 0;
    struct wp_lines *wpl = wpp->wpl->next;
    struct line *victim_line;
    int cnt = 0;
    while (wpl) {
        victim_line = wpl->line;
        ppa.g.blk = victim_line->id;
        if (victim_line->id == wpp->curline->id) {
            wpl = wpl->next;
            continue;
        }
        
        cnt++;
        wpp->vic_cnt--;
        ssd->stat.gc_times++;
        ssd->stat.line_gc_times[victim_line->id]++;
        ssd->stat.wp_victims[wpp->id]++;
        ssd->stat.line_wp_gc_times++;
        // fprintf(gc_fp, "%ld\n",counter);
        gc_read_all_valid_data(ssd, &ppa, group_gtd_lpns, group_gtd_index, &start_gtd);

        wpl = wpl->next;
        mark_line_free(ssd, &ppa);
    }

    wpl = wpp->wpl->next;
    clear_all_write_pointer_victim_lines(wpl, wpp);
    
    

    model_training(ssd, wpp, group_gtd_lpns, group_gtd_index, start_gtd);
    /* update line status */
    

    return 0;
    
}

static int line_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, struct line *victim_line)
{
    // printf("line do gc: %d\n", gc_line_num++);
    // struct line *victim_line = NULL;
    // struct ssdparams *spp = &ssd->sp;
    // struct nand_lun *lunp;
    const int trans_ent = ssd->sp.ents_per_pg;
    const int parallel = ssd->sp.tt_luns;
    struct ppa ppa;
    // int ch, lun;
    // printf("%d line do gc %d\n", victim_line->id, gc_num++);
    ppa.g.blk = victim_line->id;
    ftl_debug("GC-ing line:%d,ipc=%d,victim=%d,full=%d,free=%d\n", ppa.g.blk,
              victim_line->ipc, ssd->lm.victim_line_cnt, ssd->lm.full_line_cnt,
              ssd->lm.free_line_cnt);

    uint64_t group_gtd_lpns[parallel][trans_ent];
    int group_gtd_index[parallel];
    memset(group_gtd_index, 0, sizeof(group_gtd_index));
    int start_gtd = 0;

    gc_read_all_valid_data(ssd, &ppa, group_gtd_lpns, group_gtd_index, &start_gtd);

    model_training(ssd, wpp, group_gtd_lpns, group_gtd_index, start_gtd);

    free_all_blocks(ssd, &ppa);

    struct wp_lines *wpl = wpp->wpl;
    // TODO: evict this line out of the wp_lines of wpp;
    clear_one_write_pointer_victim_lines(wpl, victim_line);

    /* update line status */
    mark_line_free(ssd, &ppa);

    return 0;
}

static bool model_predict(struct ssd *ssd, uint64_t lpn, struct ppa *ppa) {
    struct ssdparams *spp = &ssd->sp;
    int gtd_index = lpn/spp->ents_per_pg;

    ssd->stat.model_use_num++;
    // * 1.2.1. do the model prediction
    uint64_t pred_lpn = lpn - gtd_index * spp->ents_per_pg;
    

    // * 1.2.2 traverse the brks and find where the lpn belongs to
    int piece_wise_no = -1;
    lr_node *t = &ssd->lr_nodes[gtd_index];
    // * find which piece the lpn belongs to
    piece_wise_no = pred_lpn/spp->interval_size;
    
    

    // * start predict
    if (piece_wise_no != -1) {

        // * 通过函数得到预测值
        uint64_t pred_ppa = pred_lpn + t->brks[piece_wise_no].b;
        // * pred_ppa只可能在0-512之间，大于是错的
        // if (pred_ppa >= 512) {
        //     return false;
        // }

        // * 按理说这时就应返回true，可能有的算不准，所以需要再验证一下
        *ppa = get_maptbl_ent(ssd, lpn);
        uint64_t actual_ppa = ppa2vppn(ssd, ppa);
        uint64_t read_pred_ppa = pred_ppa ;
        if (read_pred_ppa == actual_ppa) {
            *ppa = get_maptbl_ent(ssd, lpn);
            return true;        
        } else {
            // * 用来排查bitmap[]=1但是测的不准的情况，这里是
            struct ppa real_ppa = vppn2ppa(ssd, read_pred_ppa);
            printf("error:lpn:%lld\tvppn:%lld\tactual_vppn:%lld\n",(long long)lpn,(long long)pred_ppa,(long long)actual_ppa);
            if (get_rmap_ent(ssd, &real_ppa) == INVALID_LPN) {
                ssd->stat.model_out_range++;
            }
        }
        
    }

    return false;
}


void count_segments(struct ssd* ssd) {
    printf("total cnt: %lld\n", (long long)ssd->stat.access_cnt);
    printf("cmt cnt: %lld\n", (long long)ssd->stat.cmt_hit_cnt);
    printf("model cnt: %lld\n", (long long)ssd->stat.model_hit_num);
    ssd->stat.access_cnt = 0;
    ssd->stat.cmt_hit_cnt = 0;
    ssd->stat.model_hit_num = 0;
}

static uint64_t ssd_read(struct ssd *ssd, NvmeRequest *req)
{
    struct ssdparams *spp = &ssd->sp;
    uint64_t lba = req->slba;
    int nsecs = req->nlb;
    struct ppa ppa;
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + nsecs - 1) / spp->secs_per_pg;
    uint64_t tvpn;
    uint64_t lpn, last_lpn,svpn,slpn;
    uint64_t sublat=0, maxlat = 0;
    struct nand_lun *lun;
    struct Seg* seg = NULL;
    struct nand_cmd srd;
    // struct timespec time1, time2;
    

    if (end_lpn >= spp->tt_pgs) {
        ftl_err("start_lpn=%"PRIu64",tt_pgs=%d\n", start_lpn, ssd->sp.tt_pgs);
    }
    

    /* normal IO read path */
    for (lpn = start_lpn; lpn <= end_lpn; lpn++) {
        // clock_gettime(CLOCK_MONOTONIC, &time1);
        //ssd->stat.access_cnt++;
        //sublat = 0;
        ppa=get_maptbl_ent(ssd,lpn);
        if (!mapped_ppa(&ppa) || !valid_ppa(ssd, &ppa)) 
        {
            //printf("%s,lpn(%" PRId64 ") not mapped to valid ppa\n", ssd->ssdname, lpn);
            //printf("Invalid ppa,ch:%d,lun:%d,blk:%d,pl:%d,pg:%d,sec:%d\n",
            //ppa.g.ch, ppa.g.lun, ppa.g.blk, ppa.g.pl, ppa.g.pg, ppa.g.sec);
            //ssd->stat.access_cnt--;
            //ssd->stat.cmt_hit_cnt--;
            ssd->stat.model_out_range++;
            continue;
        }
        // 1.1. first look up in the cmt
        tvpn = lpn/spp->ents_per_pg;
        struct Cmt_Senode *cmt_senode = cmt_hit(ssd,tvpn );

        if (cmt_senode) {
            //ssd->stat.cmt_hit_cnt++;
            seg = lpn2seg(ssd,lpn,&last_lpn);
            svpn = lpn - tvpn * spp->ents_per_pg - seg->x1 + seg->sppn;
            if(last_lpn>seg->x2)
            {
                last_lpn = seg->x2;
            }
            last_lpn +=  tvpn*spp->ents_per_pg;   
            if(last_lpn > end_lpn)
            {
                last_lpn = end_lpn;
            }
            
            for(uint64_t i = lpn;i<=last_lpn;++i)
            {
                sublat = 0;
                ssd->stat.cmt_hit_cnt++;
                ssd->stat.access_cnt++;
                ppa=get_maptbl_ent(ssd,i);
                 if (!mapped_ppa(&ppa) || !valid_ppa(ssd, &ppa)) 
                {
                    //printf("%s,lpn(%" PRId64 ") not mapped to valid ppa\n", ssd->ssdname, lpn);
                    //printf("Invalid ppa,ch:%d,lun:%d,blk:%d,pl:%d,pg:%d,sec:%d\n",
                    //ppa.g.ch, ppa.g.lun, ppa.g.blk, ppa.g.pl, ppa.g.pg, ppa.g.sec);
                    ssd->stat.access_cnt--;
                    ssd->stat.cmt_hit_cnt--;
                    ssd->stat.model_out_range++;
                    continue;
                }
                
                uint64_t actual_ppa = ppa2vppn(ssd,&ppa);
                uint64_t read_ppa = i-lpn+svpn;
                if (read_ppa == actual_ppa) 
                {
                    //ppa = get_maptbl_ent(ssd, lpn);       
                    lun = get_lun(ssd, &ppa);
                    lun->next_lun_avail_time = (cmt_senode->next_avail_time > lun->next_lun_avail_time) ? \
                        cmt_senode->next_avail_time : lun->next_lun_avail_time;
                    
                    srd.type = USER_IO;
                    srd.cmd = NAND_READ;
                    srd.stime = req->stime;
                    sublat = ssd_advance_status(ssd, &ppa, &srd);
                    maxlat = (sublat > maxlat) ? sublat : maxlat;
                }
                else
                {
                    printf("error%d:maptable and segment are inconsistent !!!\n",__LINE__);
                    printf("actual_ppa is %lld\tread_ppa is %lld\n",(long long)actual_ppa,(long long)read_ppa);
                }
            }
            

            

            lpn = last_lpn;
            goto ssd_read_latency;
        } 
        ssd->stat.access_cnt++;
        slpn=lpn&(spp->ents_per_pg-1);//这里相当于lpn-tvpn*spp->ents_per_pg
        //slpn>>4相当于slpn/16  slpn&15相当于slpn%16
        if (ssd->lr_nodes[tvpn].brks[slpn>>4].bitmap&(1<<(slpn&15))) {
            
            int gtd_index = lpn/spp->ents_per_pg;
            if (ssd->lr_nodes[gtd_index].u == 1) {
                
                if (ssd->model_used){
                    
                    bool f = model_predict(ssd, lpn, &ppa);

                    if (f) {
                        ssd->stat.model_hit_num++;
                        srd.type = USER_IO;
                        srd.cmd = NAND_READ;
                        srd.stime = req->stime;
                        sublat = ssd_advance_status(ssd, &ppa, &srd);
                        maxlat = (sublat > maxlat) ? sublat : maxlat;
                        goto ssd_read_latency;
                    }
                }
            }
        }
        
        ssd->stat.cmt_miss_cnt++;
            // pretreat future request
        // last_lpn = (lpn / spp->ents_per_pg + 1) * spp->ents_per_pg - 1;
        // last_lpn = (last_lpn < end_lpn) ? last_lpn : end_lpn;
        // process_translation_page_read(ssd, req, lpn, last_lpn);
        process_translation_page_read(ssd,req,tvpn);
        cmt_senode = cmt_hit_no_move(ssd, lpn);
        ppa = get_maptbl_ent(ssd, lpn);
        if (!mapped_ppa(&ppa) || !valid_ppa(ssd, &ppa)) {
            //printf("%s,lpn(%" PRId64 ") not mapped to valid ppa\n", ssd->ssdname, lpn);
            //printf("Invalid ppa,ch:%d,lun:%d,blk:%d,pl:%d,pg:%d,sec:%d\n",
            //ppa.g.ch, ppa.g.lun, ppa.g.blk, ppa.g.pl, ppa.g.pg, ppa.g.sec);
            ssd->stat.access_cnt--;
            ssd->stat.cmt_miss_cnt--;
            // ssd->stat.model_out_range++;
            continue;
        }
        //after reading the translation page, data page can only begin to read
        lun = get_lun(ssd, &ppa);
        lun->next_lun_avail_time = (cmt_senode->next_avail_time > lun->next_lun_avail_time) ? \
                                    cmt_senode->next_avail_time : lun->next_lun_avail_time;  
        srd.type = USER_IO;
        srd.cmd = NAND_READ;
        srd.stime = req->stime;
        sublat = ssd_advance_status(ssd, &ppa, &srd);
        maxlat = (sublat > maxlat) ? sublat : maxlat;
        goto ssd_read_latency;
        


    ssd_read_latency:

        // if (!mapped_ppa(&ppa) || !valid_ppa(ssd, &ppa)) {
        //     ssd->stat.access_cnt--;
        //     //printf("%s,lpn(%" PRId64 ") not mapped to valid ppa\n", ssd->ssdname, lpn);
        //     //printf("Invalid ppa,ch:%d,lun:%d,blk:%d,pl:%d,pg:%d,sec:%d\n",
        //     //ppa.g.ch, ppa.g.lun, ppa.g.blk, ppa.g.pl, ppa.g.pg, ppa.g.sec);
        //     continue;
        // }

        // struct nand_cmd srd;
        // srd.type = USER_IO;
        // srd.cmd = NAND_READ;
        // srd.stime = req->stime;
        // sublat = ssd_advance_status(ssd, &ppa, &srd);
        maxlat = (sublat > maxlat) ? sublat : maxlat;
        // clock_gettime(CLOCK_MONOTONIC, &time2);
    }
    // ssd->stat.read_time += (maxlat + (time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));
    return maxlat;
}

static uint64_t ssd_write(struct ssd *ssd, NvmeRequest *req)
{
    // struct timespec time1, time2;
    uint64_t lba = req->slba;
    struct ssdparams *spp = &ssd->sp;
    struct cmt_mgmt* cm = &ssd->cm;
    int len = req->nlb;
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + len - 1) / spp->secs_per_pg;
    struct Cmt_Senode *cmt_senode,*dir_cmsenode;
    // struct ppa ppa;
    struct ppa ppa;
    uint64_t lpn,svpn=0,elpn,slpn=0,ppn,sgtd=0;
    uint64_t curlat = 0, maxlat = 0;
    int sequence_cnt = 0;

    if (end_lpn >= spp->tt_pgs) {
        ftl_err("start_lpn=%"PRIu64",tt_pgs=%d\n", start_lpn, ssd->sp.tt_pgs);
    }
    elpn = start_lpn;
    for (lpn = start_lpn; lpn <= end_lpn; lpn++) {
        curlat = 0;
        // clock_gettime(CLOCK_MONOTONIC, &time1);
        // 1. first get the write pointer
        uint64_t gtd_index = lpn/spp->ents_per_pg;
        int wp_index = (int)(gtd_index/spp->trans_per_line);

        ftl_assert(wp_index == ssd->sp.tt_lines-1);

        // * maintain the consistency of bitmap
        // if (ssd->bitmaps[lpn] == 1) {
            // ssd->bitmaps[lpn] = 0;
        // }
        cmt_senode= cmt_hit(ssd,gtd_index);
        //cmt_entry = cmt_hit(ssd, lpn);
        if (cmt_senode) {
            // ssd->stat.cmt_hit_cnt++;
        } else {
            //last_lpn = (lpn / spp->ents_per_pg + 1) * spp->ents_per_pg - 1;
            //last_lpn = (last_lpn < end_lpn) ? last_lpn : end_lpn;
            //printf("1111111\n");
            process_translation_page_write(ssd, req,gtd_index);
            //printf("12222\n");
            //cmt_senode= cmt_hit(ssd,gtd_index);
            ppa = get_maptbl_ent(ssd, lpn);
        }

        ppa = get_maptbl_ent(ssd, lpn);

        //cmt_entry = find_hash_entry(&ssd->cm.ht, lpn);



        if (mapped_ppa(&ppa)) {
            mark_page_invalid(ssd, &ppa);
            set_rmap_ent(ssd, INVALID_LPN, &ppa);
        }

        struct write_pointer *lwp= &ssd->gtd_wps[wp_index];
        if (!lwp->curline) {
            init_line_write_pointer(ssd, lwp, true);
        } else {
            advance_line_write_pointer(ssd, lwp);
        }

        ppa = get_new_line_page(ssd, lwp);
        set_maptbl_ent(ssd, lpn, &ppa);
        //cmt_entry->ppn = ppa2pgidx(ssd, &ppa);
        //cmt_entry->dirty = DIRTY;
        set_rmap_ent(ssd, lpn, &ppa);

        mark_page_valid(ssd, &ppa);

        elpn = lpn;
        
        if(lpn==start_lpn)
        {
            sequence_cnt = 1;
            slpn=start_lpn;
            svpn = ppa2vppn(ssd,&ppa);
            sgtd = gtd_index;
            //先去掉
            cm->tt_entries+=ssd->senodes[sgtd].seg_count;
        }
        else
        {
            ppn = ppa2vppn(ssd,&ppa);
            if(ppn!=svpn+sequence_cnt||sgtd!=gtd_index)
            {
                //printf("slpn:%lld\tsequence_cnt:%lld\n",(long long)slpn,(long long)sequence_cnt);
                if(slpn + sequence_cnt !=elpn)
                {
                    printf("write page sequence_cnt = %d and elpn=%lld,slpn=%lld are inconsistent!!!\n",sequence_cnt,(long long)elpn,(long long)slpn);
                }
                slpn = slpn-sgtd*spp->ents_per_pg;
                
                
                insert_seg2senode(ssd,slpn,slpn+sequence_cnt-1,svpn,sgtd);
                dir_cmsenode = find_hash_cmt_senode(&cm->ht,sgtd);
                dir_cmsenode->dirty = DIRTY;
                //再加回来
                cm->tt_entries-=ssd->senodes[sgtd].seg_count;
                if(cm->tt_entries<0)
                evict_CMT_Senode_from_cmt(ssd);

                //再去掉
                cm->tt_entries += ssd->senodes[gtd_index].seg_count;

                slpn = elpn;
                sgtd = gtd_index;
                svpn = ppn;
                sequence_cnt = 1;
            }
            else
            {
                sequence_cnt++;
            }
        }
        struct nand_cmd swr;
        swr.type = USER_IO;
        swr.cmd = NAND_WRITE;
        swr.stime = req->stime;
        /* get latency statistics */
        curlat = ssd_advance_status(ssd, &ppa, &swr);
        maxlat = (curlat > maxlat) ? curlat : maxlat;
        // clock_gettime(CLOCK_MONOTONIC, &time2);
    }
    //printf("slpn:%lld\tsequence_cnt:%lld\n",(long long)slpn,(long long)sequence_cnt);
    if(slpn + sequence_cnt !=lpn)
    {
        printf("write page sequence_cnt = %d and elpn=%lld,slpn=%lld are inconsistent!!!\n",sequence_cnt,(long long)lpn,(long long)slpn);
    }
    slpn = slpn-sgtd*spp->ents_per_pg;
    
    insert_seg2senode(ssd,slpn,slpn+sequence_cnt-1,svpn,sgtd);
    dir_cmsenode = find_hash_cmt_senode(&cm->ht,sgtd);
    dir_cmsenode->dirty = DIRTY;
    
    //再加回来
    cm->tt_entries-=ssd->senodes[sgtd].seg_count;
    if(cm->tt_entries<0)
    evict_CMT_Senode_from_cmt(ssd);
    // * simulate the model sequential initalization
    // TODO: 如果顺序写长度大于学习模型中该范围的分段函数的有效长度，那么就取代它
    // if (ssd->model_used) {
    //     // for(lpn=start_lpn;lpn<=end_lpn;lpn++)
    //     // {
    //     //     last_lpn = (lpn / spp->ents_per_pg + 1) * spp->ents_per_pg - 1;
    //     //     last_lpn = (last_lpn < end_lpn) ? last_lpn : end_lpn;
    //     //     sequence_cnt = last_lpn - lpn;
    //     //     int gtd_index = lpn/spp->ents_per_pg;
    //     //     lr_node lrn = ssd->lr_nodes[gtd_index];
    //     //     if (lrn.u) {
    //     //         // * this model has been trained, try to find if this sequential write is longer than any segments
    //     //         for (int j = 0; j < MAX_INTERVALS; j++) {
    //     //             if (lrn.brks[j].key >= lpn && lrn.brks[j].valid_cnt - sequence_cnt < sequence_cnt) {

    //     //                 // * modify this model
    //     //                 lr_breakpoint* brk = &lrn.brks[j];
    //     //                 brk->b = 0;
    //     //                 brk->w = 1;
    //     //                 brk->key = start_lpn;
    //     //                 brk->valid_cnt = sequence_cnt;


    //     //                 // * modifiy the next model 
    //     //                 if (j != MAX_INTERVALS-1 && lrn.brks[j+1].key < end_lpn) {
    //     //                     lrn.brks[j+1].key = end_lpn;
    //     //                     lrn.brks[j+1].valid_cnt -= (end_lpn - lrn.brks[j+1].key);
    //     //                 }


    //     //                 // * mark the bitmap valid
    //     //                 for (int j = start_lpn; j < end_lpn; j++)
    //     //                     ssd->bitmaps[j] = 1;

    //     //             }
    //     //         }
    //     //     }
    //     //     lpn=last_lpn;
           
    //     sequence_cnt = end_lpn - start_lpn;
    //     int gtd_index = lpn/spp->ents_per_pg;
    //     lr_node lrn = ssd->lr_nodes[gtd_index];
    //     if (lrn.u) {
    //         // * this model has been trained, try to find if this sequential write is longer than any segments
    //         for (int j = 0; j < MAX_INTERVALS; j++) {
    //             //printf("111111111122222\n");
    //             //经过测试这里的代码并不会被执行因为这个start_lpn的值>key的值而且如果被执行就会乱套，
    //             //因为这个model的设计本身就是在model_train时进行修改的这里修改将不符合论文中的设计
    //             //这里如果进行了修改那么在预测时就会startppn+b在预测时将会出错,
    //             //整个在整个过程中并没有对这个model进行修改，只有在modeltrain中进行了修改。
    //             //简而言之就是论文中所说的对off的修改并没有实现。
    //             if (lrn.brks[j].key >= start_lpn && lrn.brks[j].valid_cnt < sequence_cnt) {
    //                 //printf("111111111122222\n");
    //                 // * modify this model
    //                 lr_breakpoint* brk = &lrn.brks[j];
    //                 brk->b = 0;
    //                 brk->w = 1;
    //                 brk->key = start_lpn;
    //                 brk->valid_cnt = sequence_cnt;


    //                 // * modifiy the next model 
    //                 if (j != MAX_INTERVALS-1 && lrn.brks[j+1].key < end_lpn) {
    //                     lrn.brks[j+1].key = end_lpn;
    //                     lrn.brks[j+1].valid_cnt -= (end_lpn - lrn.brks[j+1].key);
    //                 }


    //                 // * mark the bitmap valid
    //                 for (int j = start_lpn; j < end_lpn; j++)
    //                     ssd->bitmaps[j] = 1;

    //             }
    //         }
    //     }
    //     }
        

    // ssd->stat.write_time += (maxlat + (time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));

    return maxlat;
}

static void *ftl_thread(void *arg)
{
    FemuCtrl *n = (FemuCtrl *)arg;
    struct ssd *ssd = n->ssd;
    NvmeRequest *req = NULL;
    uint64_t lat = 0;
    int rc;
    int i;

    // gc_fp = fopen("/home/astl/wsz/gc_frequency.txt", "w");

    while (!*(ssd->dataplane_started_ptr)) {
        usleep(100000);
    }

    /* FIXME: not safe, to handle ->to_ftl and ->to_poller gracefully */
    ssd->to_ftl = n->to_ftl;
    ssd->to_poller = n->to_poller;

    while (1) {

        // tmp_counter++;
        // if (tmp_counter / 500000000 == 1) {
        //     counter++;
        //     // printf("%lld\n", (long long)counter);
        //     tmp_counter = 0;
        // }
        for (i = 1; i <= n->num_poller; i++) {
            if (!ssd->to_ftl[i] || !femu_ring_count(ssd->to_ftl[i]))
                continue;

            rc = femu_ring_dequeue(ssd->to_ftl[i], (void *)&req, 1);
            if (rc != 1) {
                printf("FEMU: FTL to_ftl dequeue failed\n");
            }

            

            ftl_assert(req);
            switch (req->cmd.opcode) {
            case NVME_CMD_WRITE:
                lat = ssd_write(ssd, req);
                break;
            case NVME_CMD_READ:
                lat = ssd_read(ssd, req);
                break;
            case NVME_CMD_DSM:
                lat = 0;
                break;
            default:
                //ftl_err("FTL received unkown request type, ERROR\n");
                ;
            }

            req->reqlat = lat;
            req->expire_time += lat;

            rc = femu_ring_enqueue(ssd->to_poller[i], (void *)&req, 1);
            if (rc != 1) {
                ftl_err("FTL to_poller enqueue failed\n");
            }

            /* clean one line if needed (in the background) */
            // if (should_gc(ssd)) {
            //     do_gc(ssd, false);
            // }
            
        }

        // struct line *vl = NULL;
        // struct write_pointer *wpp;
        // should_line_gc(ssd, vl, wpp);
        // if (vl) {
        //     line_do_gc(ssd, true, wpp, vl);
        // }
    }
    return NULL;
}

// #pragma GCC pop_options
