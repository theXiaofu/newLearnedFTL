/**
 * Filename: Segmentftl.c
 * Author: Xuanxuan Fu
 * Date Created: 2024-10
 * Version: 1.0
 *
 * Brief Description:
 *     This file implementing the main function of S+Mftl .
 *
 * Copyright Notice:
 *     Copyright (C) 2023 Shengzhe Wang. All rights reserved.
 *     License: Distributed under the GPL License.
 *
 */

// #pragma GCC push_options
// #pragma GCC optimize(0)

#include "Segtable.h"
#include "util.h"
#include"string.h"
// static int hit_num = 0;
// static int gc_num = 0;
// static int gc_line_num = 0;
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


static void print_read_cache_space(FTL_Map* ftl_map){
    Read_Cache* read_cache = ftl_map->cache->read_cache;
    Read_Cache_Space* read_space = read_cache->read_cache_space;
    for(int i = 0;i<read_cache->space_num;++i)
    {
        printf("---------------[%d]-------------------\n",i);
        printf("read_space[%d].size = %d\n",i,read_space[i].size);
        printf("read_space[%d].max_seg_num = %d\n",i,read_space[i].max_seg_num);
        printf("read_space[%d].min_seg_num = %d\n",i,read_space[i].min_seg_num);
        printf("read_space[%d].num = %d\n",i,read_space[i].num);
        printf("read_space[%d].st = %d\n",i,read_space[i].st);
        printf("read_space[%d].end = %d\n",i,read_space[i].end);
    }
}

static void print_read_cache_space_by_index(FTL_Map* ftl_map,int i){
    Read_Cache* read_cache = ftl_map->cache->read_cache;
    Read_Cache_Space* read_space = read_cache->read_cache_space;
    printf("---------------[%d]-------------------\n",i);
    printf("read_space[%d].size = %d\n",i,read_space[i].size);
    printf("read_space[%d].max_seg_num = %d\n",i,read_space[i].max_seg_num);
    printf("read_space[%d].min_seg_num = %d\n",i,read_space[i].min_seg_num);
    printf("read_space[%d].num = %d\n",i,read_space[i].num);
    printf("read_space[%d].st = %d\n",i,read_space[i].st);
    printf("read_space[%d].end = %d\n",i,read_space[i].end);
}

//不会用完因为SSD 假设容量为256G可用映射只有200多G
static Seg_LRU* init_seg_LRU(int total_num){
    Seg_LRU* seg_LRU = g_malloc0(sizeof(Seg_LRU)*(total_num));
    for(int i = 0;i<total_num;++i)
    {
        //表示没有插入到任何的LRU中
        seg_LRU[i].pos_entry_number = INVALID_POS_ENTRY;
    }
    //读写缓存LRU的头节点
    seg_LRU[total_num-1].nex = seg_LRU[total_num-1].pre = total_num-1;
    seg_LRU[total_num-2].nex = seg_LRU[total_num-2].pre = total_num-2;
    return seg_LRU;
}

// Pos_Entry* init_pos_entry(int total_num){
//     Pos_Entry* pos_entry = g_malloc0(sizeof(Pos_Entry)*(total_num));
//     return pos_entry;
// }

//缓存大小
static uint8_t* init_cache(int cache_size,int write_cache_size){
    Cache* cache_struct = g_malloc0(sizeof(Cache));
    cache_struct->cache_size = cache_size;

    // //下边这几个参数没用可以删除掉
    // cache_struct->lpn_size = 1;
    // cache_struct->ppn_size = sizeof(VPPN);
    // cache_struct->seg_size = cache_struct->lpn_size*2+cache_struct->ppn_size;
    
    uint8_t* cache = g_malloc0(sizeof(uint8_t)*(cache_size));
    cache_struct->cache = cache;
    cache_struct->header_size = sizeof(uint32_t);
    cache_struct->table_size = sizeof(Table);
    cache_struct->seg_size = sizeof(Seg);

    printf("header_size %d\n",cache_struct->header_size);
    printf("table_size %d\n",cache_struct->table_size);
    printf("seg_size %d\n",cache_struct->seg_size);
    printf("cache_size %d\n",cache_size);

    //初始化write_cache
    Write_Cache* write_cache= g_malloc0(sizeof(Write_Cache));
    cache_struct->write_cache = write_cache;
    //写缓存指针
    write_cache->write_point = 0;

    //写缓存空间
    write_cache->write_table = cache;
    //写缓存传统table的数目 当write_point等于这个值就满了要进行LRU驱逐淘汰
    write_cache->write_table_capacity = write_cache_size/sizeof(Table);

    //修改写缓存的大小
    write_cache_size = write_cache->write_table_capacity*sizeof(Table);
    printf("write_cache_size %d\n",write_cache_size);
    printf("write_cache_capacity %d\n",write_cache->write_table_capacity);

    //初始化读缓存
    Read_Cache* read_cache = (Read_Cache*)g_malloc0(sizeof(Read_Cache));
    cache_struct->read_cache = read_cache;

    //空间利用率 默认设置为0.1表示至少有90%的空间被使用
    read_cache->cache_ratio = 0.1;
    //读缓存大小
    read_cache->cache_size = cache_size-write_cache_size;
    //读缓存空间
    read_cache->read_cache = cache+write_cache_size;



    //段空间+一个传统table空间
   
    read_cache->max_seg_size = (sizeof(Table)-sizeof(uint32_t)) / sizeof(Seg);
    //下标映射从1开始
    read_cache->segsize2space = (uint32_t*)g_malloc0(sizeof(uint32_t)*(256+1));

    int total = 0;
    for (int i = 1; i <= read_cache->max_seg_size; ++i)
    {
        int k = i + (i * read_cache->cache_ratio);
        printf("%d %d\n",i,k);

        if (k >  read_cache->max_seg_size - 3)
            k =  read_cache->max_seg_size;

        for (int j = i; j <= k; ++j)
        {
            read_cache->segsize2space[j] = total;
        }
        i = k;
        total++;
    }
    printf("total %d\n",total);
    printf("read_cache->max_seg_size %d\n", read_cache->max_seg_size);
    for(int i = read_cache->max_seg_size+1;i<=256;++i)
    {
        read_cache->segsize2space[i]=total;
    }


    //输出映segsize2space射表的参数
    printf("segsize2space:\n");
    for(int i = 0;i<=256;++i)
    {
        printf("i: %d  -> %d \n",i,read_cache->segsize2space[i]);
    }
    

    total++;
    //设置子空间的数量
    read_cache->space_num = total;
    Read_Cache_Space* read_space = (Read_Cache_Space*)g_malloc0(sizeof(Read_Cache_Space)*total);
    read_cache->read_cache_space = read_space;

    int init_seg_num = read_cache->cache_size/sizeof(Table)/read_cache->space_num;

    printf("init_seg_num:%d\n",init_seg_num);

    read_cache->max_evict_size = 0;
    //初始化子空间
    total = 0;
    for (int i = 1; i <= read_cache->max_seg_size; ++i)
    {
        int k = i + i * read_cache->cache_ratio;

        if (k >  read_cache->max_seg_size - 3)
            k =  read_cache->max_seg_size;

        for (int j = i; j <= k; ++j)
        {
            read_cache->segsize2space[j] = total;
        }
        //i = k;
        if (total == 0)
        {
            read_space[total].st = read_space[total].end = 0;
            read_space[total].max_seg_num = k;
            read_space[total].min_seg_num = i;
            read_space[total].size = k*sizeof(Seg)+sizeof(uint32_t);//头节点加seg的长度
            read_space[total].num = 0;
        }
        else
        {

            read_space[total].size = i*sizeof(Seg)+sizeof(uint32_t);
            read_space[total].max_seg_num = k;
            read_space[total].min_seg_num = i;
            read_space[total].num = 0;
            read_space[total].st = read_space[total].end= total*init_seg_num*read_space[total].size;
        }
        read_cache->max_evict_size += read_space[total].size*2;
        i=k;
        total++;
    }
    read_space[total].st=  read_space[total].end = read_cache->cache_size;
    read_space[total].max_seg_num = 256;
    read_space[total].min_seg_num = 256;
    read_space[total].num = 0;
    read_space[total].size = sizeof(Table);
    read_cache->max_evict_size += read_space[total].size*2;
    //输出init_seg_num
    printf("init_seg_num:%d\n",init_seg_num);

    //输出每个子空间的参数
    for (int i = 0; i <= total; i++)
    {
        printf("read_space[%d].size = %d\n",i,read_space[i].size);
        printf("read_space[%d].max_seg_num = %d\n",i,read_space[i].max_seg_num);
        printf("read_space[%d].min_seg_num = %d\n",i,read_space[i].min_seg_num);
        printf("read_space[%d].num = %d\n",i,read_space[i].num);
        printf("read_space[%d].st = %d\n",i,read_space[i].st);
        printf("read_space[%d].end = %d\n",i,read_space[i].end);
    }
    printf("read_cache->max_evict_size = %d\n",read_cache->max_evict_size);
    return cache_struct;
}

static FTL_Map* init_FTL_Map(FemuCtrl *n){
    FTL_Map*ftl_map = g_malloc0(sizeof(FTL_Map));
    int cache_size = n->ssd->sp.tt_cmt_size;
    int tt_blk = n->ssd->sp.tt_blks;
    //这里根据SSD配置设置使用的时候需要修改
    ftl_map->seg_LRU = init_seg_LRU(tt_blk);
    ftl_map->read_cache_LRU_head = tt_blk-1;
    ftl_map->write_cache_LRU_head = tt_blk-2;
    

    // ftl_map->pos_entry = init_pos_entry(1<<19);
    //ftl_map->cache = init_cache(1<<26,1<<20);
    ftl_map->cache = init_cache(cache_size,cache_size>>5);
    ftl_map->g_map = g_malloc0(sizeof(G_map)*tt_blk);
    for(int i = 0;i<tt_blk;++i)
    {
        ftl_map->g_map[i].seg_num = LRU_TABLE_FLAG;
        ftl_map->g_map[i].next_avail_time=0;
        memset(ftl_map->g_map[i].table.bitmap,0,32);
        ftl_map->g_map[i].table.table_head = i;
    }


    return ftl_map;
}

static void free_FTL_Map(FTL_Map* ftl_map)
{
    g_free(ftl_map->seg_LRU);
    // g_free(ftl_map->pos_entry);
    g_free(ftl_map->cache->read_cache->read_cache_space);
    g_free(ftl_map->cache->read_cache->segsize2space);
    g_free(ftl_map->cache->read_cache);
    g_free(ftl_map->cache->write_cache);
    g_free(ftl_map->cache->cache);
    g_free(ftl_map->cache);
    g_free(ftl_map);
}





static int func(int *c) {
    printf("test\n");
    return *c;
}
static int line_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, struct line *victim_line);
static int gtd_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, struct line *victim_line);
static void batch_gtd_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, int num, struct line *delete_line);
static int batch_line_do_gc(struct ssd* ssd, bool force, struct write_pointer *wpp, struct line *delete_line);
// static void should_do_gc(struct ssd *ssd, struct write_pointer *wpp);
// static struct line *select_victim_line(struct ssd *ssd, bool force);
static void insert_wp_lines(struct write_pointer *wpp);
static bool should_do_gc_v3(struct ssd *ssd, struct write_pointer *wpp);

//将新段插入到这一层中同时对这层旧段进行修改和从上到下的压缩，如果new_seg为NULL，那么只进行压缩。
//如果返回值表示这一层的起始段。
//bit_map包含上层和new_seg的位图。
static struct Seg* insert_seg2level(struct Seg *new_seg, struct Seg *start_seg,bool *bit_map, struct Senode *node,int k)
{
    if(start_seg==NULL)
    {
        if(new_seg)
        node->small_senode[k].l++;//建立一个新层级
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
        start_seg->next_level = insert_seg2level(nextl_newseg,nextl_start_seg,bit_map,node,k);
        return start_seg;
    }
    else
    {
        node->small_senode[k].l--;
        return insert_seg2level(new_seg,nextl_start_seg,bit_map,node,k);
    }
    return NULL;
}






static inline struct ppa get_maptbl_ent(struct ssd *ssd, uint64_t lpn)
{
    return ssd->maptbl[lpn];
}

static inline void set_maptbl_ent(struct ssd *ssd, uint64_t lpn, struct ppa *ppa)
{
    ftl_assert(lpn < ssd->sp.tt_pgs);

    if(ssd->maptbl[lpn].ppa == UNMAPPED_PPA)
    {
        ssd->stat.all_count++;
    }
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
static uint64_t ppa2vppn(struct ssd *ssd, struct ppa *ppa,uint64_t su_bl_id) {
    
    struct ssdparams *spp = &ssd->sp;
    uint64_t vppn;
    uint64_t i ;
    i = 0;
    for(;i<Gc_threshold;++i)
    {
        if(ssd->gtd_wps[su_bl_id].line_id[i]&&ssd->gtd_wps[su_bl_id].line_id[i]->id == ppa->g.blk)
        {
            
            break;
        }
    }
    if(i==Gc_threshold)
    {
        printf("erro: %d:line_id:%lld not exit!\n",__LINE__,(long long)ppa->g.blk);
        
    }
    //printf("erro: %d:line_id not exit!\n",__LINE__);
    vppn = ppa->g.ch + \
            ppa->g.lun * spp->chn_per_lun + \
            ppa->g.pl * spp->chn_per_pl + \
            ppa->g.pg * spp->chn_per_pg + \
            i * spp->chn_per_blk;
    
    return vppn;
}

static struct ppa vppn2ppa(struct ssd *ssd, uint64_t vppn,uint64_t su_bl_id) {
    struct ppa ppa;
    //printf("vppn2ppa\n");
    struct ssdparams *spp = &ssd->sp;
    int blk = vppn / spp->chn_per_blk;
    if(blk>=Gc_threshold)
    {
        printf("error:__LINE__:%d   blk:%d  >= Gcthreshold\n",__LINE__,blk);
    }
    
    if(!ssd->gtd_wps[su_bl_id].line_id[blk])
    {
        printf("error:__LINE__:%d:line id not exit!!!\n",__LINE__);
    }
    ppa.g.blk = ssd->gtd_wps[su_bl_id].line_id[blk]->id;

    vppn -= ppa.g.blk*spp->chn_per_blk;
    ppa.g.pg = vppn / spp->chn_per_pg;
    vppn -= ppa.g.pg * spp->chn_per_pg;
    ppa.g.pl = vppn / spp->chn_per_pl;
    vppn -= ppa.g.pl * spp->chn_per_pl;
    ppa.g.lun = vppn / spp->chn_per_lun;
    ppa.g.ch = vppn - ppa.g.lun * spp->chn_per_lun;

    return ppa;
}






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
        for(int j = 0;j<Gc_threshold;++j)
        {
            ssd->gtd_wps[i].line_id[j] = NULL;
        }
        ssd->gtd_wps[i].wpl = g_malloc0(sizeof(struct wp_lines));
        ssd->gtd_wps[i].wpl->line = NULL;
        ssd->gtd_wps[i].wpl->next = NULL;
        ssd->gtd_wps[i].vic_cnt = 0;
        ssd->gtd_wps[i].id = i;
        ssd->gtd_wps[i].curline_pos = 0;
    }
    ssd->trans_wp.wpl = g_malloc0(sizeof(struct wp_lines));
    ssd->trans_wp.wpl->line = NULL;
    ssd->trans_wp.wpl->next = NULL;
    ssd->trans_wp.vic_cnt = 0;
    ssd->trans_wp.id = ssd->sp.tt_lines;
    ssd->trans_wp.curline_pos = 0;
    // init the rmap for lines
    ssd->line2write_pointer = g_malloc0(sizeof(struct write_pointer *) * ssd->sp.tt_lines);
    init_line_write_pointer(ssd, &ssd->trans_wp, false);


}

static inline void check_addr(int a, int max)
{
    ftl_assert(a >= 0 && a < max);
}





static void insert_wp_lines(struct write_pointer *wpp) {
    struct wp_lines *wpl = g_malloc0(sizeof(struct wp_lines));
    wpl->line = wpp->curline;
    wpl->next = wpp->wpl->next;
    wpp->wpl->next = wpl;
    wpp->vic_cnt++;
    return;
}



//这个到时候优化一下从高到底找一个victim最大的块
static bool should_do_gc_v3(struct ssd *ssd, struct write_pointer *wpp) {
    struct line_mgmt *lm = &ssd->lm;
    //printf("GC happens?\n");
    // if (ssd->lm.free_line_cnt < 4) {
    //     printf("what's wrong?\n");
    // }
    
    if (wpp && wpp->vic_cnt >= Gc_threshold) {
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
        //printf("Gc end1\n");
        return true;
        
    } else if (ssd->trans_wp.vic_cnt >= Gc_threshold) {

        // * 如果gtd写指针的line的数量大于=阈值，对其进行GC
        //printf("trans_wp vic_cnt >Gcthreshold\n");
        QTAILQ_INSERT_TAIL(&lm->victim_list, ssd->trans_wp.curline, entry);
        lm->victim_line_cnt++;

        init_line_write_pointer(ssd, &ssd->trans_wp, false);

        // ssd->trans_wp.vic_cnt++;

        // * put this line to the victim lines the line write pointer belongs to
        batch_gtd_do_gc(ssd, true, &ssd->trans_wp, ssd->trans_wp.vic_cnt, NULL);

    } else if (lm->free_line_cnt < 10) {

        //这个到时候可以修改设置以下分为7个链表从高到低来找到一个victim最大的块
        struct line *tvl = QTAILQ_FIRST(&lm->victim_list);
        struct write_pointer *write_back_wp = NULL;
        struct line *vl = NULL;

        
        // QTAILQ_REMOVE(&lm->victim_list, vl, entry);
        // int max_vic = 1;
        if (lm->free_line_cnt < free_line_threshold) {
            if(lm->victim_line_cnt==0)
            {
                printf("error:no lm->victimline\n");//这里会用完  用完就会报错
            }
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
                        
                        gtd_do_gc(ssd, true, write_back_wp, vl);
                        //write_back_wp->vic_cnt--;

                    } else if (vl->type == DATA) {
                        // fprintf(gc_fp, "%ld\n",counter);
                        //printf("line_do_gc\n");
                        line_do_gc(ssd, true, write_back_wp, vl);
                        //write_back_wp->vic_cnt--;
                    }

                    if (write_back_wp == wpp) {
                        if (wpp->curline->rest == 0) {
                            init_line_write_pointer(ssd, wpp, false);
                            //printf("here line:%d what's up?\n",__LINE__);
                        }
                        //printf("Gc end2\n");   
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
                        
                        //printf("gtd batch do gc\n");
                        batch_gtd_do_gc(ssd, true, write_back_wp, write_back_wp->vic_cnt, vl);
                    } else if (vl->type == DATA) {
                        //printf("line %d do batch gc victim cnt:%d\n", write_back_wp->id,write_back_wp->vic_cnt);
                        // * model rebuilding
                        batch_line_do_gc(ssd, true, write_back_wp, vl);

                        //printf("line %d do batch gc victim cnt:%d\n", write_back_wp->id,write_back_wp->vic_cnt);
                        if (write_back_wp == wpp) {
                            if (wpp->curline->rest == 0) {
                                func(&wpp->curline->rest);
                                printf("what's up?\n");
                            }
                            //printf("Gc end3\n");   
                            return true;
                        }
                    }
                }
                if (write_back_wp == wpp) {
                    if (wpp->curline->rest == 0) {
                        func(&wpp->curline->rest);
                        printf("what's up?\n");
                    }
                    //printf("Gc end4\n");   
                    return true;
                }
            }
        }
        
        

    }
    //printf("Gc end5\n");
    return false;
}


static void clear_one_write_pointer_victim_lines(  struct line *victim_line, struct write_pointer *wpp) {
    struct wp_lines *tmp;
    struct wp_lines *wpl;
    wpl = wpp->wpl;
    int i = 0;
    for(;i < Gc_threshold; ++i)
    {
        if(wpp->line_id[i]&&wpp->line_id[i]==victim_line)
        {
            wpp->line_id[i]=NULL;
            break;
        }
    }
    if(i==Gc_threshold)
    {
        if(victim_line->type==DATA)
        {
            printf("erro:%d :victim line hasn't lineid  \n",__LINE__);
        }
        
    }

    while (wpl) {
        if (wpl->next && wpl->next->line == victim_line) {
            
            tmp = wpl->next;
            wpl->next = tmp->next;
            wpp->vic_cnt--;
            g_free(tmp);
            break;
        }
        wpl = wpl->next;
    }
    
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
                    if(wpp->curline->rest!=ssd->sp.pgs_per_line)
                    {
                        goto another_try;
                    }
                    
                }
            }
        }
    }
}



static struct ppa get_new_line_page(struct ssd *ssd, struct write_pointer *wpp)
{
    struct ppa ppa;
    int j;
    //printf("00000000\n");
    if(!wpp->line_id[wpp->curline_pos]||wpp->line_id[wpp->curline_pos]!=wpp->curline)
    {//第一次被使用要给其分配pos
        for( j = 0; j < Gc_threshold; ++j)
        {
            if(!wpp->line_id[j])
            {
                wpp->line_id[j]=wpp->curline;
                wpp->curline_pos = j;
                break;
            }
            
        }
        if(j==Gc_threshold)
        {
            printf("error:%d:cur_line  is not allocated!\n",__LINE__);
        }
    }
    ppa.ppa = 0;
    ppa.g.ch = wpp->ch;
    ppa.g.lun = wpp->lun;
    ppa.g.pg = wpp->pg;
    ppa.g.blk = wpp->blk;
    ppa.g.pl = wpp->pl;
    wpp->curline->rest--;
    if (wpp->curline->rest < 0) {
        if(&ssd->trans_wp==wpp)
        printf("trans_wp\n");
        else
        printf("data_wp\n");

        printf("buduijin rest is %d\n",wpp->curline->rest);
        func(&wpp->curline->rest);
    }
    ftl_assert(ppa.g.pl == 0);
    //printf("001111111\n");
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


//这里修改参数
static void ssd_init_params(struct ssdparams *spp)
{
    spp->secsz = 512;
    spp->secs_per_pg = 8;/*pagesize=4k*/
    spp->pgs_per_blk = 256;/*a page group has 512 pages   */
    spp->blks_per_pl = 256; /*default 256，8GB */
    spp->pls_per_lun = 1;    /*8=4*2*1   a big block group has 8 blocks*/
    spp->luns_per_ch = 4;   /* default 8 */
    spp->nchs = 8;          /* default 8 */

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
    //这里要修改
    spp->ents_per_pg = spp->pgs_per_blk;
    spp->tt_trans_pgs = spp->tt_pgs / spp->ents_per_pg;
    // one translation page can accomodate 512 pages(1 blocks), so 8 trans pages combine a line
    spp->trans_per_line = spp->pgs_per_line / spp->ents_per_pg;
    spp->tt_line_wps = spp->tt_trans_pgs/spp->trans_per_line;
    printf("total pages: %d\n", spp->tt_line_wps);

    spp->interval_size = spp->ents_per_pg%MAX_INTERVALS ? spp->ents_per_pg/MAX_INTERVALS+1 : spp->ents_per_pg/MAX_INTERVALS ;
    spp->tt_gtd_size = spp->tt_pgs / spp->ents_per_pg;
    
    spp->tt_cmt_size = 1<<19;/*1MB的空间 512KB用于lr_node 512KB用于cmt*/
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

    ssd->valid_lines = g_malloc0(sizeof(uint8_t) * spp->tt_lines);
    for (int i = 0; i < spp->blks_per_pl; i++) 
        ssd->valid_lines[i] = 0;

    // init the cmt

    for (int i = 0; i < spp->tt_trans_pgs; i++) {
        ssd->gtd[i].ppa = UNMAPPED_PPA;
        ssd->lr_nodes[i].u = 1;     // * the bit denote if the model is used
        

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
    ssd->seg_bitmaps = g_malloc0(sizeof(bool)*(spp->interval_size));
    ssd->bitmap_table[0]=0;
    for (int i = 0; i < 256; i++)
    {
        ssd->bitmap_table[i] = ssd->bitmap_table[i>>1] + (1&i);
    }
        

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
    st->model_training_nums = 0;

    st->all_count = 0;
    st->seg_count=0;
    st->sort_time = 0;
    st->GC_erase_time=0;
    st->GC_read_time=0;
    st->GC_write_time=0;
    st->GC_insert_time=0;
    st->GC_time=0;

    st->write_time=0;
    st->insert_CMT_model_time=0;
    st->read_time=0;
    st->read_CMT_time=0;


    st->write_num = 0;
    st->should_write_num = 0;
    st->erase_cnt = 0;
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
    
    /*init FTL_map*/

    ssd->ftl_map= init_FTL_Map(n);

    

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
        ssd->stat.erase_cnt++;
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

//这个可能后边要改
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


//将段转换为传统的映射表
static void seg2table(Seg* seg_st,int max_size, Table* table_st)
{
    // 初始化所有table_st为无效状态（byte2=0xFF）
    table_st->table_head = INVALID_POS_ENTRY;
    //table_st->l2p[255].byte0 = table_st->l2p[255].byte1 = table_st->l2p[255].byte2 = 0;
    //memset(table_st->l2p, NO_PPN, 256 * sizeof(VPPN));
   //把他们初始化为无效位 
    memset(table_st->bitmap, 0,  32);

    


    //VPPN* l2p = table_st->l2p;
    // printf("table_st->l2p[1].byte0 = %x\n",l2p[1].byte0 );
    // printf("table_st->l2p[1].byte1 = %x\n",l2p[1].byte1 );
    // printf("table_st->l2p[1].byte2 = %x\n",l2p[1].byte2 );
    
    //VPPN ppn_store;
    uint16_t ppn;
    int slpn,elpn;
    for(int cnt = 0; cnt<max_size;++cnt)
    {
        Seg* current_seg = &(seg_st[cnt]);
        
        // 遇到无效段标记时终止处理
        if ( current_seg->slpn == 0&&cnt!=0)
        {
            break;
        }

        // ppn_store= current_seg->ppn;
        //利用小端的序只关心第三位即可，会产生越界但是无影响
        ppn = current_seg->ppn.vppn;

        // ppn = (uint32_t)ppn_store.byte0|
        //     ((uint32_t)ppn_store.byte1<<8)|
        //     ((uint32_t)ppn_store.byte2<<16);

        slpn = current_seg->slpn;
        elpn = current_seg->elpn;

        for (int i = slpn; i <= elpn; ++i)
        {
            // 分解为3字节（保持低位在前）
            // ppn_store.byte0 = ppn;
            // ppn_store.byte1 = ppn >> 8;
            // ppn_store.byte2 = ppn >> 16;
            // printf("i = %d\n",i);
            // printf("ppn_store.byte0 = %x\n", ppn_store.byte0);
            // printf("ppn_store.byte1 = %x\n", ppn_store.byte1);
            // printf("ppn_store.byte2 = %x\n", ppn_store.byte2);
            table_st->l2p[i].vppn=ppn;;
            table_st->bitmap[i>>5] |= (1<<(i&31));
            // printf("i = %d\n",i);
            // printf("l2p[i].byte0 = %x\n", l2p[i].byte0);
            // printf("l2p[i].byte1 = %x\n", l2p[i].byte1);
            // printf("l2p[i].byte2 = %x\n", l2p[i].byte2);
            
            ppn++;
        }
    }
}

static int table2seg(Seg* seg_st, Table* table_st) {
    //const int MAX_ADDR = 255;  // 地址范围0-255
    int segment_count = 0;     // 段计数器
    int start_idx = -1;        // 段起始索引
    uint32_t prev_val = 0;     // 前一个值
    VPPN* l2p = table_st->l2p;

    for (int i = 0; i <= 255; i++) {
        // 检查当前VPPN有效性
        if ((table_st->bitmap[i>>5]&(1<<(i&31))) ==0) {
            if (start_idx != -1) {  // 遇到无效标记时结束段
                seg_st[segment_count].slpn = start_idx;
                seg_st[segment_count].elpn = i - 1;
                seg_st[segment_count].ppn = l2p[start_idx];
                segment_count++;
                start_idx = -1;
            }
            continue;
        }

        // 计算当前VPPN值
        uint32_t current_val =  l2p[i].vppn;
        // current_val &= 0x00FFFFFF;
        // uint32_t current_val = ((uint32_t)table_st[i].byte2 << 16) |
        //     ((uint32_t)table_st[i].byte1 << 8) |
        //     table_st[i].byte0;

        if (start_idx == -1) {  // 新段开始
            start_idx = i;
            prev_val = current_val;
        }
        else if (current_val == prev_val + 1) {  // 连续序列
            prev_val = current_val;
        }
        else {  // 序列中断
            seg_st[segment_count].slpn = start_idx;
            seg_st[segment_count].elpn = i - 1;
            seg_st[segment_count].ppn = l2p[start_idx];
            segment_count++;
            start_idx = i;
            prev_val = current_val;
        }
    }

    // 处理最后一个段
    if (start_idx != -1) {
        seg_st[segment_count].slpn = start_idx;
        seg_st[segment_count].elpn = 255;
        seg_st[segment_count].ppn = l2p[start_idx];
        segment_count++;
    }

    return segment_count;  // 返回生成的段数量
}

//写吐了

//将段插入到readcache子空间中.注意这里的LRU的顺序在这里边被更新
//给定header有反向索引和dirty信息  不要提前加入到LRU中在这个函数中加入
static void insert_seg_to_read_cache(struct ssd* ssd, void* header_seg_table,int seg_num){

    FTL_Map* ftl_map = ssd->ftl_map;
    Read_Cache* read_cache = ftl_map->cache->read_cache;
    Read_Cache_Space* read_space = read_cache->read_cache_space;
    uint8_t* cache = ftl_map->cache->read_cache->read_cache;
    Seg_LRU* seg_lru = ftl_map->seg_LRU;
    //根据seg_st的大小找到对应的子空间
    uint32_t space_idx = read_cache->segsize2space[seg_num];
    uint32_t seg_idx;
    uint32_t tmp = read_space[space_idx].max_seg_num;
    uint32_t header;

    Header_Seg* header_seg;
    Table* table;

    //说明传入的是seg如果等于256说明是table
    if(tmp<256)
    {
        header = ((Header_Seg*)header_seg_table)->header;
            //把seg_num设置为无效段
        for(int i = seg_num;i<tmp;++i)
        {
            //将多余的段设置为无效段便于后续判断和二分查找
            ((Header_Seg*)header_seg_table)->seg[i].slpn=0;
        }
    }
    else
    {
        header = ((Table*)header_seg_table)->table_head;
    }

    seg_idx = header&REVERSE_INDEX;
    // printf("seg_idx:%d\n",seg_idx);
    //将seg_idx插入到LRU中
    ADD_READ_CACHE_LRU(ftl_map,seg_idx);

  

    //判断对应的子空间能否向右边直接扩张
    if(space_idx+1<read_cache->space_num&&
    read_space[space_idx].end+read_space[space_idx].size<=read_space[space_idx+1].st)
    {

        //可以直接扩张
        memcpy(cache+read_space[space_idx].end,header_seg_table,read_space[space_idx].size);
        //更新LRU对应的位置  
        seg_lru[seg_idx].pos_entry_number = read_space[space_idx].end;
        seg_lru[seg_idx].seg_num  = seg_num;

        read_space[space_idx].end+=read_space[space_idx].size;
        read_space[space_idx].num++;
        
        return ;
    }
    //判断对应的子空间能否向左边直接扩张
    if(space_idx>0&&read_space[space_idx-1].end+read_space[space_idx].size<=read_space[space_idx].st)
    {
        //可以向左直接扩张
        read_space[space_idx].st-=read_space[space_idx].size;
        read_space[space_idx].num++;

       

        memcpy(cache+read_space[space_idx].st,header_seg_table,read_space[space_idx].size);
        //更新LRU对应的位置 
        seg_lru[seg_idx].pos_entry_number = read_space[space_idx].st;
        seg_lru[seg_idx].seg_num  = seg_num;
       
        return ;
    }

    //扩张时可能会出现空间为0的情况这时我们不考虑认为这个空间至少有一个虽然会浪费部分内存但是最多有33
    //个空间最多浪费33*548=17.44k
    //向左向右直接扩张失败   那么向右间接扩张 扩充右边的区域获得更大的空间
    int i = space_idx+1;
    for(i = space_idx+1;i+1<read_cache->space_num;++i)
    {
        if(read_space[i+1].st>=read_space[i].end+read_space[i].size)
        {
            break;    
        }
    }
    if(i+1<read_cache->space_num)
    {
        //可以向右间接扩张
        while(i>space_idx)
        {
            if(read_space[i].num>0)
            {
                //把i空间左边的内容移到右边
                memcpy(cache+read_space[i].end,cache+read_space[i].st,read_space[i].size);
                //获取反向索引 
                header = *((uint32_t*)(cache+read_space[i].st));
                //更新LRU对应的位置
                seg_lru[header&REVERSE_INDEX].pos_entry_number = read_space[i].end;
            }
            
            read_space[i].st = read_space[i].st+read_space[i].size;
            read_space[i].end = read_space[i].end+read_space[i].size;
            i--;
        }
        //此时右边一定能够扩张
        memcpy(cache+read_space[i].end,header_seg_table,read_space[i].size);

        //更新LRU对应的位置//更新LRU对应的位置
        header = *((uint32_t*)(header_seg_table));
        seg_lru[seg_idx].pos_entry_number = read_space[i].end;
        seg_lru[seg_idx].seg_num = seg_num;

        read_space[i].end = read_space[i].end+read_space[i].size;
        read_space[i].num++;
        
        return ;
    }
     //向右不能间接扩张 则向左间接扩张
     //记录需要扩张的大小
     uint32_t nead_size[256];
     int pre=space_idx-1;
     nead_size[pre] = read_space[space_idx].size+read_space[pre].end-read_space[space_idx].st;
    //  print_read_cache_space_by_index(ftl_map,pre);
    //  print_read_cache_space_by_index(ftl_map,space_idx);
    //  printf("nead_size[%d] = %d\n",pre,nead_size[pre]);


     //因为扩充空间只能是space_size的整数倍
     if(nead_size[pre]>read_space[pre].size)
     {
        nead_size[pre] = read_space[pre].size<<1;
     }
     else
     {
        nead_size[pre] = read_space[pre].size;
     }

     for(i = space_idx-1;i>0;i=pre)
     {
        pre = i-1;
        if(read_space[pre].end+nead_size[i]<=read_space[i].st)
        {
            
            break;
        }
        //修改nead_size
        //这里先不修改
        nead_size[pre] = (nead_size[i]+read_space[pre].end-read_space[i].st);
        tmp = nead_size[pre]%read_space[pre].size;
        nead_size[pre] = tmp==0?nead_size[pre]:nead_size[pre]-tmp+read_space[pre].size;
     }

    uint32_t move_size,move_begin;
     if(i>0)
     {
        //可以向左间接扩张
        while(i<space_idx)
        {
            //把i空间右边的内容移到左边
            if(nead_size[i]+read_space[i].st>read_space[i].end)
            {
                //向左移动的段数
                tmp = read_space[i].num;
                //向左移动的空间大小
                move_size = read_space[i].end-read_space[i].st;
                move_begin = read_space[i].st;
            }
            else
            {
                tmp = nead_size[i]/read_space[i].size;
                move_size = nead_size[i];
                move_begin = read_space[i].end-nead_size[i];
            }
            read_space[i].st = read_space[i].st-nead_size[i];
            read_space[i].end = read_space[i].end-nead_size[i];
            if(tmp>0)
            memcpy(cache+read_space[i].st,cache+move_begin,move_size);
            //更新LRU对应的位置
            uint32_t nex_st = read_space[i].st;
            for(int j = 0;j<tmp;++j)
            {
                header = *((uint32_t*)(cache+nex_st));
                seg_lru[header&REVERSE_INDEX].pos_entry_number = nex_st;
                nex_st+=read_space[i].size;
            }
            i++;
        }

        //插入到space_idx左边空间
        read_space[space_idx].st = read_space[space_idx].st-read_space[space_idx].size;
        memcpy(cache+read_space[space_idx].st,header_seg_table,read_space[space_idx].size);
        read_space[space_idx].num++;
        //更新LRU对应的位置
        seg_lru[seg_idx].pos_entry_number = read_space[space_idx].st;
        seg_lru[seg_idx].seg_num  = seg_num;
        return;
     }

//如果空间不足那么使用LRU驱逐一直驱逐直到空间足够
while(TRUE)
{
    uint32_t evict_size = read_cache->max_evict_size;
    while(evict_size>0)
    {
        //获取LRU中要被驱逐的受害者段
        int victim_seg_idx = seg_lru[ftl_map->read_cache_LRU_head].pre;
    //获得子空间号
        uint32_t victim_space_idx = read_cache->segsize2space[seg_lru[victim_seg_idx].seg_num];
        
        move_begin = seg_lru[victim_seg_idx].pos_entry_number;
        //将victim_seg_idx设置为无效段
        //从LRU中删除
        REMOVE_READ_CACHE_LRU(ftl_map,victim_seg_idx);
        seg_lru[victim_seg_idx].pos_entry_number = INVALID_POS_ENTRY;
        ftl_map->g_map[victim_seg_idx].next_avail_time=0;
        
        
        if(*((uint32_t*)(cache+move_begin))&DIRTY_INFO)
        {
            //需要写回到flash中申请新页并更新GTD申请新页

            struct ppa gtd_ppa;
            gtd_ppa = get_gtd_ent(ssd, victim_seg_idx);
            if (mapped_ppa(&gtd_ppa)) {
                translation_read_page_no_req(ssd, &gtd_ppa);

                mark_page_invalid(ssd, &gtd_ppa);
                set_rmap_ent(ssd, INVALID_LPN, &gtd_ppa);
            }
            //printf("write page 000\n");
            translation_write_page(ssd, victim_seg_idx);

            //写回到闪存中
            if(seg_lru[victim_seg_idx].seg_num == LRU_TABLE_FLAG)
            {
                ftl_map->g_map[victim_seg_idx].seg_num = LRU_TABLE_FLAG ;
                ftl_map->g_map[victim_seg_idx].table=*((Table*)(cache+move_begin));
                //擦除dirty标记
                ftl_map->g_map[victim_seg_idx].table.table_head &= REVERSE_INDEX;
            }
            else
            {
                ftl_map->g_map[victim_seg_idx].seg_num = seg_lru[victim_seg_idx].seg_num;
                ftl_map->g_map[victim_seg_idx].header_seg=*((Header_Seg*)(cache+move_begin));
                //擦除dirty标记
                ftl_map->g_map[victim_seg_idx].table.table_head &= REVERSE_INDEX;
            }
            
        }
        
        
        if(victim_space_idx>space_idx)
        {
            //说明有空间肯定足够了 在space中删除 如果不在左端将左端的seg移到这个位置
            if(move_begin>read_space[victim_space_idx].st)
            {
                memcpy(cache+move_begin,cache+read_space[victim_space_idx].st,read_space[victim_space_idx].size);
                //更新lru中的信息
                header = *((uint32_t*)(cache+move_begin));
                seg_lru[header&REVERSE_INDEX].pos_entry_number = move_begin;
            }
            //减小子空间中的段数量
            read_space[victim_space_idx].num--;
            read_space[victim_space_idx].st += read_space[victim_space_idx].size;
            evict_size -= read_space[victim_space_idx].size;
        }
        if(victim_space_idx<space_idx)
        {
            //减小子空间中的段数量
            read_space[victim_space_idx].num--;
            read_space[victim_space_idx].end -= read_space[victim_space_idx].size;
            //说明需要删除一个段 如果不在右端将右端的段移到这个位置
            if(move_begin<read_space[victim_space_idx].end)
            {
                memcpy(cache+move_begin,cache+read_space[victim_space_idx].end,read_space[victim_space_idx].size);
                //更新lru中的信息
                header = *((uint32_t*)(cache+move_begin));
                seg_lru[header&REVERSE_INDEX].pos_entry_number = move_begin;
            }

            evict_size -= read_space[victim_space_idx].size;
        }

        if(victim_space_idx==space_idx)
        {
            //说明空间足够了直接插入到这个位置即可
            memcpy(cache+move_begin,header_seg_table,read_space[space_idx].size);
            //不用修改space的任何信息
            //更新lru中的信息
            // header = *((uint32_t*)(header_seg_table));
            seg_lru[seg_idx].pos_entry_number = move_begin;
            seg_lru[seg_idx].seg_num = seg_num;
            return;
        }
    }

    //这里可以分为两个函数一个evict一个insert 但是我直接把上边的代码复制下来重新判断
    //虽然两个函数可能更简单也更清晰但是我因为之前的逻辑是写在一起的 现在的evict的方式和之前不同
    //所以我就不分开写了

       //判断对应的子空间能否向右边直接扩张
    if(space_idx+1<read_cache->space_num&&
    read_space[space_idx].end+read_space[space_idx].size<=read_space[space_idx+1].st)
    {

        //可以直接扩张
        memcpy(cache+read_space[space_idx].end,header_seg_table,read_space[space_idx].size);
        //更新LRU对应的位置  
        seg_lru[seg_idx].pos_entry_number = read_space[space_idx].end;
        seg_lru[seg_idx].seg_num  = seg_num;

        read_space[space_idx].end+=read_space[space_idx].size;
        read_space[space_idx].num++;
        
        return ;
    }
    //判断对应的子空间能否向左边直接扩张
    if(space_idx>0&&read_space[space_idx-1].end+read_space[space_idx].size<=read_space[space_idx].st)
    {
        //可以向左直接扩张
        read_space[space_idx].st-=read_space[space_idx].size;
        read_space[space_idx].num++;

       

        memcpy(cache+read_space[space_idx].st,header_seg_table,read_space[space_idx].size);
        //更新LRU对应的位置 
        seg_lru[seg_idx].pos_entry_number = read_space[space_idx].st;
        seg_lru[seg_idx].seg_num  = seg_num;
       
        return ;
    }

    //扩张时可能会出现空间为0的情况这时我们不考虑认为这个空间至少有一个虽然会浪费部分内存但是最多有33
    //个空间最多浪费33*548=17.44k
    //向左向右直接扩张失败   那么向右间接扩张 扩充右边的区域获得更大的空间
    i = space_idx+1;
    for(i = space_idx+1;i+1<read_cache->space_num;++i)
    {
        if(read_space[i+1].st>=read_space[i].end+read_space[i].size)
        {
            break;    
        }
    }
    if(i+1<read_cache->space_num)
    {
        //可以向右间接扩张
        while(i>space_idx)
        {
            if(read_space[i].num>0)
            {
                //把i空间左边的内容移到右边
                memcpy(cache+read_space[i].end,cache+read_space[i].st,read_space[i].size);
                //获取反向索引 
                header = *((uint32_t*)(cache+read_space[i].st));
                //更新LRU对应的位置
                seg_lru[header&REVERSE_INDEX].pos_entry_number = read_space[i].end;
            }
            
            read_space[i].st += read_space[i].size;
            read_space[i].end += read_space[i].size;
            i--;
        }
        //此时右边一定能够扩张
        memcpy(cache+read_space[i].end,header_seg_table,read_space[i].size);

        //更新LRU对应的位置//更新LRU对应的位置
        header = *((uint32_t*)(header_seg_table));
        seg_lru[seg_idx].pos_entry_number = read_space[i].end;
        seg_lru[seg_idx].seg_num = seg_num;

        read_space[i].end += read_space[i].size;
        read_space[i].num++;
        
        return ;
    }
     //向右不能间接扩张 则向左间接扩张
     //记录需要扩张的大小

    pre=space_idx-1;
     nead_size[pre] = read_space[space_idx].size+read_space[pre].end-read_space[space_idx].st;
    //  print_read_cache_space_by_index(ftl_map,pre);
    //  print_read_cache_space_by_index(ftl_map,space_idx);
    //  printf("nead_size[%d] = %d\n",pre,nead_size[pre]);


     //因为扩充空间只能是space_size的整数倍
     if(nead_size[pre]>read_space[pre].size)
     {
        nead_size[pre] = read_space[pre].size<<1;
     }
     else
     {
        nead_size[pre] = read_space[pre].size;
     }

     for(i = space_idx-1;i>0;i=pre)
     {
        pre = i-1;
        if(read_space[pre].end+nead_size[i]<=read_space[i].st)
        {
            
            break;
        }
        //修改nead_size
        //这里先不修改
        nead_size[pre] = (nead_size[i]+read_space[pre].end-read_space[i].st);
        tmp = nead_size[pre]%read_space[pre].size;
        nead_size[pre] = tmp==0?nead_size[pre]:nead_size[pre]-tmp+read_space[pre].size;
        //输出nead_size[pre]等信息
        // print_read_cache_space_by_index(ftl_map,pre);
        // print_read_cache_space_by_index(ftl_map,i);
        // printf("nead_size[%d] = %d\n",pre,nead_size[pre]);
        // if(seg_idx>=38)
        // {
        //     print_read_cache_space(ftl_map);
        //     //输出0-32的need_size
        //     for(int j = 0;j<32;++j)
        //     {
        //         printf("nead_size[%d] = %d\n",j,nead_size[j]);
        //     }
        // }
     }

     if(i>0)
     {
        //可以向左间接扩张
        while(i<space_idx)
        {
            //把i空间右边的内容移到左边
            if(nead_size[i]+read_space[i].st>read_space[i].end)
            {
                //向左移动的段数
                tmp = read_space[i].num;
                //向左移动的空间大小
                move_size = read_space[i].end-read_space[i].st;
                move_begin = read_space[i].st;
            }
            else
            {
                tmp = nead_size[i]/read_space[i].size;
                move_size = nead_size[i];
                move_begin = read_space[i].end-nead_size[i];
            }
            read_space[i].st = read_space[i].st-nead_size[i];
            read_space[i].end = read_space[i].end-nead_size[i];
            if(tmp>0)
            memcpy(cache+read_space[i].st,cache+move_begin,move_size);
            //更新LRU对应的位置
            uint32_t nex_st = read_space[i].st;
            for(int j = 0;j<tmp;++j)
            {
                header = *((uint32_t*)(cache+nex_st));
                seg_lru[header&REVERSE_INDEX].pos_entry_number = nex_st;
                nex_st+=read_space[i].size;
            }
            i++;
        }

        //插入到space_idx左边空间
        read_space[space_idx].st -= read_space[space_idx].size;
        memcpy(cache+read_space[space_idx].st,header_seg_table,read_space[space_idx].size);
        read_space[space_idx].num++;
        //更新LRU对应的位置
        seg_lru[seg_idx].pos_entry_number = read_space[space_idx].st;
        seg_lru[seg_idx].seg_num  = seg_num;
        return;
     }

}
    
}


// 自定义安全内存拷贝函数（带地址合法性检查）
static int safe_memcpy(void *dest, const void *src, size_t n) {
    // 检查指针是否为空
    if (!dest || !src) {
        fprintf(stderr, "Error: NULL pointer detected! dest=%p, src=%p\n", dest, src);
        return -1;
    }

    // 检查目标地址是否可写（通过尝试写入临时数据验证）
    volatile char test_write = 0xAA;
    for (size_t i = 0; i < n; i++) {
        ((char*)dest)[i] = test_write;
        if (((char*)dest)[i] != test_write) {
            fprintf(stderr, "Error: Destination memory is not writable at %p\n", (char*)dest + i);
            return -1;
        }
    }

    // 检查源地址是否可读（通过尝试读取数据验证）
    volatile char test_read = 0;
    for (size_t i = 0; i < n; i++) {
        test_read = ((char*)src)[i];
        if (test_read == 0xAA) {  // 若读取到测试写入的值，说明地址重叠
            fprintf(stderr, "Error: Source and destination memory overlap at %p\n", src);
            return -1;
        }
    }

    // 执行实际拷贝
    memcpy(dest, src, n);
    return 0;
}

static void insert_table_to_write_table(struct ssd*ssd,Table* table_st)
{
    FTL_Map* ftl_map = ssd->ftl_map;
    Write_Cache* write_cache = ftl_map->cache->write_cache;
    Seg_LRU* seg_lru = ftl_map->seg_LRU;
    Header_Seg header_seg;
    uint32_t seg_idx = table_st->table_head&REVERSE_INDEX;
    
    if(write_cache->write_point==write_cache->write_table_capacity)
    {
        //需要驱逐

        //驱逐的策略是驱逐最久未使用的段
        //找到最久未使用的段
        int victim_seg_idx = seg_lru[ ftl_map->write_cache_LRU_head].pre;
        //获取在write_table中的位置
        uint32_t table_idx = seg_lru[victim_seg_idx].pos_entry_number;
        //从lru中删除
        REMOVE_WRITE_CACHE_LRU(ftl_map,victim_seg_idx);
        seg_lru[victim_seg_idx].pos_entry_number = INVALID_POS_ENTRY;
        //将这个置为脏
        Table* table = &(write_cache->write_table[table_idx]);
        table->table_head |= DIRTY_INFO;
        //转换为seg
        int seg_num = table2seg(header_seg.seg,table);
        header_seg.header = table->table_head;
        if(seg_num>ftl_map->cache->read_cache->max_seg_size)
        {
            //说明插入的是table
            insert_seg_to_read_cache(ssd,table,LRU_TABLE_FLAG);
        }
        else
        {
            //说明插入的是段
            insert_seg_to_read_cache(ssd,&header_seg,seg_num);
        }
        //将table_st插入到这个位置
        write_cache->write_table[table_idx] = *table_st;
        seg_lru[seg_idx].pos_entry_number = table_idx;
        seg_lru[seg_idx].seg_num = WRITE_CACHE_SPACE;
    }
    else
    {
        //直接插入
        write_cache->write_table[write_cache->write_point] = *table_st;
        seg_lru[seg_idx].pos_entry_number = write_cache->write_point;
        seg_lru[seg_idx].seg_num = WRITE_CACHE_SPACE;
    }

    //更新write_cache的LRU
    ADD_WRITE_CACHE_LRU(ftl_map,seg_idx);
}


//要把LRU的头给更新了
static uint16_t read_SegTable(struct ssd *ssd, NvmeRequest *req,uint32_t lpn)
{
    FTL_Map* ftl_map = ssd->ftl_map;
    Seg_LRU* seg_lru = ftl_map->seg_LRU;
    Write_Cache* write_cache = ftl_map->cache->write_cache;
    Read_Cache* read_cache = ftl_map->cache->read_cache;
    //获取lpn所在的GTD
    uint32_t gtd_idx = lpn>>8;
    //获取lpn在GTD中的偏移
    uint32_t offset = lpn&0xFF;
    //查看是否在缓存中
    if(seg_lru[gtd_idx].pos_entry_number==INVALID_POS_ENTRY)
    {
        //说明不在缓存中
        //从flash中读取
        G_map* g_map = &(ftl_map->g_map[gtd_idx]);
        //记录读延迟
        struct ppa  tppa;
        tppa = get_gtd_ent_index(ssd, tvpn);
        if (mapped_ppa(&tppa) && valid_ppa(ssd, &tppa)) {
            translation_read_page(ssd, req, &tppa);
            g_map->next_avail_time = get_lun(ssd, &tppa)->next_lun_avail_time;
        }


        //插入到read_cache中
        if(g_map->seg_num==LRU_TABLE_FLAG)
        {
            //说明是table
            insert_seg_to_read_cache(ftl_map,&(g_map->table),g_map->seg_num);
        }
        else
        {
            insert_seg_to_read_cache(ftl_map,&(g_map->header_seg),g_map->seg_num);
        }
    }

    //在缓存中
    if(seg_lru[gtd_idx].seg_num==WRITE_CACHE_SPACE)
    {
        //说明在write_cache中
        //从write_cache中读取
        return write_cache->write_table[seg_lru[gtd_idx].pos_entry_number].l2p[offset].vppn;
    }
    else
    {
        //说明在read_cache中
        //更新read_cache的LRU
        REMOVE_READ_CACHE_LRU(ftl_map,gtd_idx);
        ADD_READ_CACHE_LRU(ftl_map,gtd_idx);
        //从read_cache中读取
        if(seg_lru[gtd_idx].seg_num==LRU_TABLE_FLAG)
        {
            //说明是table
            Table* table = (Table*)(read_cache->read_cache+seg_lru[gtd_idx].pos_entry_number);
            return table->l2p[offset].vppn;
        }
        else
        {
            //说明是段
            int seg_num = seg_lru[gtd_idx].seg_num;
            Seg* segs = (Seg*)(read_cache->read_cache+seg_lru[gtd_idx].pos_entry_number);
            //二分查找到最后一个seg的slpn小于等于offset的seg
            int left = 0;
            int right = seg_num;  // 左闭右开区间 [left, right)
            int result = -1;
            while (left < right) {
                int mid = (right + left) >>1;  

                if (segs[mid].slpn <= offset) {
                    result = mid;  // 更新候选结果
                    left = mid + 1;  // 继续向右搜索
                } else {
                    right = mid;  // 缩小到左半区间
                }
            }
            return segs[result].ppn.vppn+offset-segs[result].slpn;
        }
    }
        
}


static void write_SegTable(struct ssd* ssd ,uint32_t* lpn,uint16_t* ppn,int num)
{
    FTL_Map* ftl_map = ssd->ftl_map;
    Seg_LRU* seg_lru = ftl_map->seg_LRU;
    Write_Cache* write_cache = ftl_map->cache->write_cache;
    Read_Cache* read_cache = ftl_map->cache->read_cache;
    uint8_t* read_cache_cache = ftl_map->cache->read_cache->read_cache;
    Read_Cache_Space* read_cache_space = ftl_map->cache->read_cache->read_cache_space;
    //获取lpn所在的GTD
    uint32_t gtd_idx = lpn[0]>>8;
    //获取lpn在GTD中的偏移
    uint32_t offset ;
    Table table;
    Header_Seg header_seg;
    uint32_t header;
    uint32_t pos_entry_number;
    //查看是否在缓存中
    if(seg_lru[gtd_idx].pos_entry_number==INVALID_POS_ENTRY)
    {
        //说明不在缓存中
        //从flash中读取
        //先记录读闪存的时间

    G_map* g_map = &(ftl_map->g_map);
    struct ppa ppa;


    
    ppa = get_gtd_ent_index(ssd, tvpn);
    

    if (mapped_ppa(&ppa) && valid_ppa(ssd, &ppa)) 
    {
        translation_read_page(ssd, req, &ppa);
        g_map->next_avail_time = get_lun(ssd, &ppa)->next_lun_avail_time;
    }


        //插入到write_cache中
        if(g_map[gtd_idx].seg_num==LRU_TABLE_FLAG )
        {
            table = g_map[gtd_idx].table;
        }
        else
        {
            seg2table(&(g_map[gtd_idx].header_seg.seg),g_map[gtd_idx].seg_num,&table);
        }
        for(int i = 0;i<num;++i)
        {
            offset = lpn[i]&0xFF;
            //更新bitmap
            table.bitmap[offset>>5] |= (1<<(offset&31));
            table.l2p[offset].vppn = ppn[i];
        }
        //将table插入到write_cache中
        insert_table_to_write_table(ftl_map,&table);
        return;
    }
    else
    {
        //说明在缓存中
        if(seg_lru[gtd_idx].seg_num==WRITE_CACHE_SPACE)
        {
            //说明在write_cache中
            //直接修改
            //更新write_cache的LRU
            REMOVE_WRITE_CACHE_LRU(ftl_map,gtd_idx);
            ADD_WRITE_CACHE_LRU(ftl_map,gtd_idx);
            pos_entry_number = seg_lru[gtd_idx].pos_entry_number;
            for(int i = 0;i<num;++i)
            {
                offset = lpn[i]&0xFF;
                write_cache->write_table[pos_entry_number].l2p[offset].vppn = ppn[i];
                //更新bitmap
                write_cache->write_table[pos_entry_number].bitmap[offset>>5] |= (1<<(offset&31));
            }
            
            return;
        }
        else
        {
            //说明在read_cache中 要从read_cache中获取到对应的内容加入到write_cache中
            //先从read_cache中获取到对应的内容

            
            //获取到对应的段
            int seg_num = seg_lru[gtd_idx].seg_num;
            uint32_t seg_pos = seg_lru[gtd_idx].pos_entry_number;
            uint32_t space_idx = read_cache->segsize2space[seg_num];
            //seg_lru[seg_idx].pos_entry_number = INVALID_POS_ENTRY;
            //查看段的内容是否需要转换为table
            
            if(seg_num<=read_cache->max_seg_size)
            {
                //需要把段转换为table
                seg2table(read_cache->read_cache+4,seg_num,&table);
                table.table_head = gtd_idx;
            }
            else
            {
                table = *(Table*)(read_cache->read_cache+seg_pos);
            }


            //从read_cache中删除
            REMOVE_READ_CACHE_LRU(ftl_map,gtd_idx);
            if(space_idx+1==read_cache->space_num)
            {
                //把左边的段移到这个位置
                if(seg_pos>read_cache->read_cache_space[space_idx].st)
                {
                    //把左边的段移到这个位置
                    memcpy(read_cache_cache+seg_pos,read_cache_cache+read_cache_space[space_idx].st,read_cache_space[space_idx].size);
                    //更新lru中的信息
                    header = *((uint32_t*)(read_cache_cache+seg_pos));
                    seg_lru[header&REVERSE_INDEX].pos_entry_number = seg_pos;
                }
                read_cache_space[space_idx].st += read_cache_space[space_idx].size;
                read_cache_space[space_idx].num--;

            }
            else
            {
                //把右边的段移到这个位置
                read_cache_space[space_idx].end -= read_cache_space[space_idx].size;
                read_cache_space[space_idx].num--;
                if(seg_pos<read_cache_space[space_idx].end)
                {
                     memcpy(read_cache_cache+seg_pos,read_cache_cache+read_cache_space[space_idx].end,read_cache_space[space_idx].size);
                    //更新lru中的信息
                    header = *((uint32_t*)(read_cache_cache+seg_pos));
                    seg_lru[header&REVERSE_INDEX].pos_entry_number = seg_pos;
                }
               
            }

            for(int i = 0;i<num;++i)
            {
                offset = lpn[i]&0xFF;
                //更新bitmap
                table.bitmap[offset>>5] |= (1<<(offset&31));
                table.l2p[offset].vppn = ppn[i];
            }
            //将table插入到write_cache中
            insert_table_to_write_table(ssd,&table);
            
            return;
        }
        return;
    }
}

static void insert_lr_nodes(struct ssd* ssd,int gtd_index,uint32_t* lpns, uint16_t* vppns, int cnt) {
    if (cnt <= 0) return;
    lr_node* lr_n = &ssd->lr_nodes[gtd_index];
    lr_breakpoint*lr_b;
    for(int i = 0; i < cnt; i++)
    {
        lpn[i]&=255;
    }
    int current_group = (lpns[0]) >>4;
    int start = 0;
    
    for (int i = 1; i <= cnt; i++) {
        // 检查是否到达数组末尾或组发生变化
        if (i == cnt || (lpns[i] >>4)  != current_group) {
            // 处理当前组
            int group_start = start;
            for (int j = start + 1; j <= i; j++) {
                // 检查是否到达组的末尾或不满足连续递增条件
                if (j == i || lpns[j] != lpns[j-1] + 1 || ppns[j] != ppns[j-1] + 1) {
                    // 调用insert函数处理当前连续段
                    
                    uint16_t mask = ((1 << (lpns[j-1]+1 - lpns[group_start])) - 1) << s; // 创建掩码
                    lr_b = &(lr_n[gtd_index].brks);
                    lr_b->bitmap &= ~mask; // 清除对应的位
                    int count = ssd->bitmap_table[lr_b->bitmap & 0xFF] + ssd->bitmap_table[(lr_b->bitmap>>8)&0xFF];
                    if(count + lpns[group_start] <= lpn[j-1])
                    {
                        lr_b->bitmap = mask;
                        lr_b->b = vppns[group_start]-lpns[group_start];
                    }


                    group_start = j;
                }
            }
            
            // 更新下一组的起始位置和当前组
            start = i;
            if (i < cnt) {
                current_group = lpns[i] / 16;
            }
        }
    }
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

    //fxx:这里需要写又注释掉了
    if (ssd->sp.enable_gc_delay) {
        struct nand_cmd gcw;
        gcw.type = GC_IO;
        gcw.cmd = NAND_WRITE;
        gcw.stime = 0;
        ssd_advance_status(ssd, &new_ppa, &gcw);
    }

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


static int gtd_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, struct line *victim_line)
{   
    //printf("7777777777\n");
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
    //先给当前的line_id腾出空间不然再写入没位置了。
    clear_one_write_pointer_victim_lines(victim_line,wpp);
    for (ch = 0; ch < spp->nchs; ch++) {
        for (lun = 0; lun < spp->luns_per_ch; lun++) {
            ppa.g.ch = ch;
            ppa.g.lun = lun;
            ppa.g.pl = 0;

            // 先获取到block,然后消除block,针对每个page都进行重新写入（ssd_write(page)），之后进行擦除
            lunp = get_lun(ssd, &ppa);
            clean_one_trans_block(ssd, &ppa);
            mark_block_free(ssd, &ppa);

            //fxx:这里需要进行擦除操作
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


    


    /* update line status */
    mark_line_free(ssd, &ppa);

    //printf("788888888\n");

    return 0;
}



static void batch_gtd_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, int num, struct line *delete_line) {
    //printf("55555555\n");
    struct line *victim_line=NULL;
    struct wp_lines *wpl ;
    int cnt = 0;
    int vp_count;

    victim_line = wpp->curline;
     //先获取victim_line的个数
    wpl = wpp->wpl->next;
    while(wpl)
    {
        cnt++;
        wpl = wpl->next;
    }

    if(cnt!=wpp->vic_cnt)
    {
        printf("erro: %d :wpp vic_cnt and cnt are different!! \n",__LINE__);
    }

    cnt = wpp->vic_cnt - 1;
    if(cnt>Gc_threshold)
    {
        printf("erro:%d  victim line > Gc_threshold\n",__LINE__);
    }
    
    
    //清除其中有效页最少的一半
    
    if(cnt == 1)
    {
        cnt = 2;
    }
    
    cnt /= 2;
    
    if(cnt<2)
    cnt = 2;

    for(int i = 0;i < cnt;++i)
    {
        wpl = wpp->wpl->next;
        vp_count = ssd->sp.pgs_per_line + 10000000;
        victim_line = NULL;
        while(wpl)
        {
            //victim_line = wpl->line;
            if(wpl->line->id==wpp->curline->id)
            {
                wpl = wpl->next;
                continue;
            }
            if(wpl->line->vpc < vp_count )
            {
                victim_line = wpl->line;
                vp_count = victim_line->vpc;
            }
            wpl=wpl->next;
        }

        

        gtd_do_gc(ssd, force, wpp, victim_line);
        //clear_one_write_pointer_victim_lines(victim_line,wpp);

    }
    //printf("56666666\n");

    //clear_all_write_pointer_victim_lines(wpl, wpp);
    return;

}


//将数据页全部读出同时把这些数据页%nchs*luns_per_ch论文中是64进行分组这样能够提高程序的并行性，start_gtd表示这一个大组最开始的那个gtd号
static void gc_read_all_valid_data(struct ssd *ssd, struct ppa *tppa, uint64_t group_gtd_lpns[][256], int *group_gtd_index, int *start_gtd) {
    const int parallel = ssd->sp.tt_luns;
    struct ssdparams *spp = &ssd->sp;
    struct nand_lun *lunp;
    int ch, lun,max_cnt;
    struct ppa ppa;
    uint64_t tmp_lpn;
    ppa.g.blk = tppa->g.blk;
    uint64_t lat = 0;
    uint64_t use_lat = 0;
    max_cnt=0;
    for (ch = 0; ch < spp->nchs; ch++) {
        for (lun = 0; lun < spp->luns_per_ch; lun++) {
            ppa.g.ch = ch;
            ppa.g.lun = lun;
            ppa.g.pl = 0;

            lunp = get_lun(ssd, &ppa);


            struct nand_page *pg_iter = NULL;
            int cnt = 0;
            cnt=0;
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
                    int gtd_index_loc = gtd_index % spp->trans_per_line;    // gtd_index%8

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
            
            if(max_cnt < cnt)
            {
                max_cnt = cnt;
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
    //因为并行所以我们这里只记录单个lun最大的那个擦出和读时间实际的
    ssd->stat.GC_time += use_lat;//按照这个计算，实际还要加上写操作的不再考虑写操作因为写操作过程中可能还会进行GC了。
    ssd->stat.GC_erase_time+=ssd->sp.blk_er_lat;
    ssd->stat.GC_read_time+=max_cnt*ssd->sp.pg_rd_lat;
}


static void model_training(struct ssd *ssd, struct write_pointer *wpp, uint64_t group_gtd_lpns[][256], int *group_gtd_index, int start_gtd) {
    struct timespec time1, time2;
    //printf("Segmentftl Model Training...\n");
    ssd->stat.model_training_nums++;
    const int trans_ent = ssd->sp.ents_per_pg;
    const int parallel = ssd->sp.tt_luns;
    struct cmt_mgmt *cm = &ssd->cm;
    struct hash_table *ht = &cm->ht;
    //uint64_t train_lpns[parallel][trans_ent];
    uint64_t train_vppns[parallel][trans_ent];
    uint64_t wp_index;
    wp_index = start_gtd / ssd->sp.trans_per_line;
    //int success = 0;
    int total = 0;
    struct Cmt_Senode* cmt_senode;
    // gc_line_num++;
   //const int interval_size = ssd->sp.interval_size;
    //const int segment_max_train_num = interval_size;
    for (int i = 0; i < parallel; i++) {
        total += group_gtd_index[i];

        // * first sort the lpns

        clock_gettime(CLOCK_MONOTONIC, &time1);
        quick_sort(group_gtd_lpns[i], 0, group_gtd_index[i]-1);
        clock_gettime(CLOCK_MONOTONIC, &time2);

        ssd->stat.sort_time += ((time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));

        // * second write them back, collect the ppa
        for (int pgi = 0; pgi < group_gtd_index[i]; pgi++) {
            struct ppa tmp_ppa;
            gc_write_page_through_line_wp(ssd, group_gtd_lpns[i][pgi], &tmp_ppa, wpp);
            train_vppns[i][pgi] = ppa2vppn(ssd, &tmp_ppa,wp_index);
        }
        // for (int pgi = 0; pgi < group_gtd_index[i]; pgi++) {
        //     struct ppa tmp_ppa=get_maptbl_ent(ssd,group_gtd_lpns[i][pgi]);
        //     if(train_vppns[i][pgi]!=ppa2vppn(ssd,&tmp_ppa,wp_index))
        //     {
        //         printf("model_training erro! train_vppns[i]!=maptable\n");
        //     }
        // }
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

                clock_gettime(CLOCK_MONOTONIC, &time1);
                ssd->stat.seg_count += insert_seg2senode(ssd,lpn,en-st-1+lpn,train_vppns[i][st],tvpn);
                clock_gettime(CLOCK_MONOTONIC, &time2);
                ssd->stat.GC_insert_time += ((time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));
                st = en;
            }
        }
        if(group_gtd_index[i]>0)
        {
            //printf("slpn:%lld\tsequence_cnt:%lld\n",(long long)group_gtd_lpns[i][st],(long long)(en-st));
            lpn = group_gtd_lpns[i][st]-slpn;
            clock_gettime(CLOCK_MONOTONIC, &time1);
            ssd->stat.seg_count += insert_seg2senode(ssd,lpn,en-st-1+lpn,train_vppns[i][st],tvpn);
            clock_gettime(CLOCK_MONOTONIC, &time2);
            ssd->stat.GC_insert_time += ((time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));
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
   if(total>0)
   {
        ssd->stat.GC_write_time+=( (total + parallel - 1) /parallel) *ssd->sp.pg_wr_lat;
   }
   
}

static int batch_line_do_gc(struct ssd* ssd, bool force, struct write_pointer *wpp, struct line *delete_line) {
    //printf("111111\n");
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
    int vp_count;
    victim_line = wpl->line;
    //先获取victim_line的个数
    wpl = wpp->wpl->next;
    while(wpl)
    {
        if(wpl->line==wpp->curline)
        {
            wpl = wpl->next;
            continue;
        }
        cnt++;
        
        wpl = wpl->next;
    }

    if(cnt!=wpp->vic_cnt-1)
    {
        printf("erro: %d :wpp vic_cnt and cnt are different!! \n",__LINE__);
    }
    if(cnt>Gc_threshold)
    {
        printf("erro:%d  victim line > Gc_threshold victim line cnt :%d\n",__LINE__,cnt);
    }
    if(cnt==1)
    {
        printf("erro:%d victim cnt = 1\n",__LINE__);
    }
    cnt = wpp->vic_cnt - 1;
    //清除其中有效页最少的一半
    
    if(cnt == 1)
    {
        cnt = 2;
    }
    //明天再改这里的cnt要设置>2最好能全去掉不然容易报错。
    cnt /= 2;
    
    if(cnt<2)
    cnt = 2;

    for(int i = 0;i < cnt;++i)
    {
        wpl = wpp->wpl->next;
        vp_count = ssd->sp.pgs_per_line + 10000000;
        while(wpl)
        {
            //victim_line = wpl->line;
            if(wpl->line->id==wpp->curline->id)
            {
                wpl = wpl->next;
                continue;
            }
            if(wpl->line->vpc < vp_count )
            {
                victim_line = wpl->line;
                vp_count = victim_line->vpc;
            }
            
            wpl=wpl->next;
        }
        //wpp->vic_cnt--;
        ssd->stat.gc_times++;
        ssd->stat.line_gc_times[victim_line->id]++;
        ssd->stat.wp_victims[wpp->id]++;
        ssd->stat.line_wp_gc_times++;
        // int k = 0;
        // for(;k<Gc_threshold;++k)
        // {
        //     if(wpp->line_id[k]==victim_line)
        //     {
        //         printf("has line id\n");
        //         break;
                
        //     }
            
        // }
        // if(k==Gc_threshold)
        // {
        //     printf("hasn't line id\n");
        // }

        // printf("-----------\n");
        ppa.g.blk = victim_line->id;
        gc_read_all_valid_data(ssd, &ppa, group_gtd_lpns, group_gtd_index, &start_gtd);
        

        // k = 0;
        // for(;k<Gc_threshold;++k)
        // {
        //     if(wpp->line_id[k]==victim_line)
        //     {
        //         printf("has line id\n");
        //         break;
                
        //     }
            
        // }
        // if(k==Gc_threshold)
        // {
        //     printf("hasn't line id\n");
        // }

        clear_one_write_pointer_victim_lines(victim_line,wpp);
        mark_line_free(ssd, &ppa);
    }
    
    
    
    

    model_training(ssd, wpp, group_gtd_lpns, group_gtd_index, start_gtd);
    /* update line status */
    
    //printf("111222\n");
    return 0;
    
}

static int line_do_gc(struct ssd *ssd, bool force, struct write_pointer *wpp, struct line *victim_line)
{
    //printf("333333\n");
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

    

    //gc_read_all_valid_data的时候已经free_all_blocks了这里多余了
    //free_all_blocks(ssd, &ppa);

    //struct wp_lines *wpl = wpp->wpl;
    // TODO: evict this line out of the wp_lines of wpp;
    //printf("0000000---111\n");
    clear_one_write_pointer_victim_lines(victim_line,wpp);
    //printf("00000---222\n");
    /* update line status */
    mark_line_free(ssd, &ppa);
    model_training(ssd, wpp, group_gtd_lpns, group_gtd_index, start_gtd);
    //printf("3444444\n");

    return 0;
}

static bool model_predict(struct ssd *ssd, uint64_t lpn, struct ppa *ppa) {
    struct ssdparams *spp = &ssd->sp;
    int gtd_index = lpn/spp->ents_per_pg;

    uint64_t wp_index = gtd_index/spp->trans_per_line;
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
        uint64_t actual_ppa = ppa2vppn(ssd, ppa,wp_index);
        uint64_t read_pred_ppa = pred_ppa ;
        if (read_pred_ppa == actual_ppa) {
            *ppa = get_maptbl_ent(ssd, lpn);
            return true;        
        } else {
            // * 用来排查bitmap[]=1但是测的不准的情况，这里是
            struct ppa real_ppa = vppn2ppa(ssd, read_pred_ppa,wp_index);
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
    FTL_Map *ftl_map = ssd->ftl_map;
    uint32_t pos_entry_number;
    uint64_t lba = req->slba;
    int nsecs = req->nlb;
    struct ppa ppa;
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + nsecs - 1) / spp->secs_per_pg;
    uint64_t tvpn,super_index;
    uint64_t lpn, last_lpn,svpn,slpn;
    uint64_t sublat=0, maxlat = 0;

    struct nand_lun *lun;
    struct nand_cmd srd;
    struct timespec time1, time2;
    long long time000;

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
            ssd->stat.model_out_range++;
            continue;
        }
        ssd->stat.access_cnt++;
        slpn=lpn&(255);//这里相当于lpn-tvpn*spp->ents_per_pg
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
                        
                        continue;
                    }
                }
            }
        }

        // look up in the cmt
        tvpn = lpn/spp->ents_per_pg;
        super_index = tvpn/spp->trans_per_line;
 
        //ssd->stat.cmt_hit_cnt++;
        clock_gettime(CLOCK_MONOTONIC, &time1);
        svpn = read_SegTable(ssd,req,lpn);
        clock_gettime(CLOCK_MONOTONIC, &time2);
        time000 = ((time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));
            
        ssd->stat.read_CMT_time += time000;

        uint64_t actual_vpn = ppa2vppn(ssd,&ppa,super_index);
        if (svpn != actual_vpn) 
        {
            printf("error%d:maptable and segment are inconsistent !!!\n",__LINE__);
            printf("lpn:%lld\ti:%lld\tactual_ppa is %lld\tread_ppa is %lld\n",(long long)lpn,(long long)i,(long long)actual_vpn,(long long)svpn);
        }

        //after reading the translation page, data page can only begin to read
        lun = get_lun(ssd, &ppa);
        lun->next_lun_avail_time = (ftl_map->g_map[tvpn].next_avail_time > lun->next_lun_avail_time) ? \
                                    ftl_map->g_map[tvpn].next_avail_time : lun->next_lun_avail_time;  
        srd.type = USER_IO;
        srd.cmd = NAND_READ;
        srd.stime = req->stime;
        sublat = ssd_advance_status(ssd, &ppa, &srd);
        maxlat = (sublat > maxlat) ? sublat : maxlat;
    }
    // ssd->stat.read_time += (maxlat + (time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));
    return maxlat;
}

static uint64_t ssd_write(struct ssd *ssd, NvmeRequest *req)
{
    struct timespec time1, time2;
    //printf("write start\n");
    uint64_t lba = req->slba;
    struct ssdparams *spp = &ssd->sp;
    struct cmt_mgmt* cm = &ssd->cm;
    int len = req->nlb;
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + len - 1) / spp->secs_per_pg;
    uint64_t end_tmp_lpn;
    struct Cmt_Senode *cmt_senode,*dir_cmsenode;
    // struct ppa ppa;
    struct ppa ppa;
    uint64_t lpn,svpn=0,elpn,slpn=0,ppn,sgtd=0;
    uint64_t curlat = 0, maxlat = 0;
    uint32_t lpns[256];
    uint16_t vppns[256];
    int sequence_cnt = 0;
    int cnt=0;
    ssd->stat.should_write_num +=end_lpn - start_lpn + 1;
    if (end_lpn >= spp->tt_pgs) {
        ftl_err("start_lpn=%"PRIu64",tt_pgs=%d\n", start_lpn, ssd->sp.tt_pgs);
    }
    
    while(start_lpn <=end_lpn)
    {

        uint64_t gtd_index = start_lpn/spp->ents_per_pg;
        uint64_t wp_index = gtd_index/spp->trans_per_line;
        elpn = start_lpn;
        end_tmp_lpn = (gtd_index + 1)*spp->ents_per_pg - 1;
        if(end_tmp_lpn > end_lpn)
        {
            end_tmp_lpn = end_lpn;
        } 
    //printf("write 1\n");
    for (lpn = start_lpn; lpn <= end_tmp_lpn; lpn++) {
            curlat = 0;
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

            
            struct nand_cmd swr;
            swr.type = USER_IO;
            swr.cmd = NAND_WRITE;
            swr.stime = req->stime;
            /* get latency statistics */
            curlat = ssd_advance_status(ssd, &ppa, &swr);
            maxlat = (curlat > maxlat) ? curlat : maxlat;
            // clock_gettime(CLOCK_MONOTONIC, &time2);
        }
    
        for (lpn = start_lpn; lpn <= end_tmp_lpn; lpn++)
        {
            
            elpn = lpn;
            ppa = get_maptbl_ent(ssd, lpn);
            lpns[cnt] = lpn;
            vppns[cnt]=ppa2vppn(ssd,&ppa,wp_index);
            cnt++;
        }
        write_SegTable(ssd,lpns,vppns,cnt);
        if(ssd->lr_nodes[gtd_index].u==1)
        {
            insert_lr_nodes(ssd,gtd_index,lpns,vppns,cnt);
        }
        
        //printf("write 4\n");

        start_lpn = end_tmp_lpn+1;
    }
    

    // ssd->stat.write_time += (maxlat + (time2.tv_sec - time1.tv_sec)*1000000000 + (time2.tv_nsec - time1.tv_nsec));
    //printf("write end \n");
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
