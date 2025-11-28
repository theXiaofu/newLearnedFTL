
#ifndef _L_FEMU_FTL_H
#define _L_FEMU_FTL_H
#include "../nvme.h"



#define INVALID_PPA     (~(0ULL))
#define INVALID_LPN     (~(0ULL))
#define UNMAPPED_PPA    (~(0ULL))


#define Se_HASH_SIZE (24593ULL)
#define ENT_PER_TP (2048ULL)
// #define GC_THRESH 3 //GC阈值 当一个gtd_wp使用了多少个Line时开始GC说明超过4个就需要进行GC最多存在4个如果设置为

enum {
    NAND_READ =  0,
    NAND_WRITE = 1,
    NAND_ERASE = 2,

    NAND_READ_LATENCY = 40000,
    NAND_PROG_LATENCY =  200000,
    NAND_ERASE_LATENCY = 2000000,
};

enum {
    CLEAN = 0,
    DIRTY = 1
};

enum {
    HEAD = 0,
    TAIL = 1
};

enum {
    USER_IO = 0,
    GC_IO = 1,
};

enum {
    GTD = 0,
    DATA = 1,
    UNUSED = 2,
};

enum {
    SEC_FREE = 0,
    SEC_INVALID = 1,
    SEC_VALID = 2,

    PG_FREE = 0,
    PG_INVALID = 1,
    PG_VALID = 2
};

enum {
    FEMU_ENABLE_GC_DELAY = 1,
    FEMU_DISABLE_GC_DELAY = 2,

    FEMU_ENABLE_DELAY_EMU = 3,
    FEMU_DISABLE_DELAY_EMU = 4,

    FEMU_RESET_ACCT = 5,
    FEMU_ENABLE_LOG = 6,
    FEMU_DISABLE_LOG = 7,
    FEMU_RESET_STAT = 8,
    FEMU_PRINT_STAT = 9,
    FEMU_MODEL = 10,
    FEMU_MODEL_UNUSE = 11
};


#define BLK_BITS    (16)
#define PG_BITS     (16)
#define SEC_BITS    (8)
#define PL_BITS     (8)
#define LUN_BITS    (8)
#define CH_BITS     (7)

// #define CMT_NUM 16*1024
// #define CMT_NUM 4*1024
#define MAX_INTERVALS 16         // ! 模型参数：一个模型中包含几个段
#define INTERVAL_NUM 60         // ! 模型参数：忘了，没啥用应该，后面没用到
#define TRAIN_THRESHOLD 30      // ! 模型参数：对于整个模型，当有多少有效数据时进行模型训练
#define Gc_threshold  8   // ! gc参数：当一个gtd_wp使用了多少个Line时开始GC说明超过8个就需要进行GC最多存在8个



// //表示没有对应任何的PPN无效的映射
// extern const uint8_t NO_PPN ;
// extern const uint32_t INVALID_POS_ENTRY ;
// //表示无效的段
// extern const uint32_t INVALID_SEG;
// //读写缓存空间判断高5位全为1表示位于写缓存中 否则位于读缓存中 
// // 这是为了区分INVALID_POS_ENTRY不过两者不存在冲突
// extern const uint8_t WRITE_CACHE_SPACE;
// //dirty信息
// extern const uint32_t DIRTY_INFO;

// extern const uint8_t LRU_TABLE_FLAG;

// //反向索引
// extern const uint32_t REVERSE_INDEX;

//把上边的const转换为宏
#define NO_PPN 0xff
#define INVALID_POS_ENTRY 0xffffffff
#define INVALID_SEG 0x0000ffff


#define LRU_TABLE_FLAG 0xff
#define WRITE_CACHE_SPACE 0xfe

#define BLKS_PER_LINE 1024

////表示这个seg的段是无效段
//const int NO_PPN_seg = 0x00ffffff;
//cache中用于存储写的空间

//LRU结构
// #pragma pack(1)
typedef struct  {

    int pre;
    int nex;

    //所处的空间subspace的起始地址
    uint32_t pos_entry_number;
    //段的数量
    uint8_t seg_num;

}Seg_LRU;
// #pragma pack()



// 4字节数据类型（1字节slpn，1字节elpn，2字节ppn）
typedef struct {

    uint16_t vppn;
} VPPN;

// 段结构体定义
typedef struct {
    uint8_t slpn;  // 起始地址（1字节）
    uint8_t elpn;  // 结束地址（1字节）
    VPPN ppn;      // 起始值（2字节）
} Seg;

// // bitmap段结构体定义
// typedef struct {
//     uint8_t slpn;  // 起始地址（1字节）
//     //uint8_t elpn;  // 结束地址（1字节）
//     VPPN ppn;      // 起始值（2字节）
// } Segb;



//带header的段结构体定义，用于写入到读缓存
typedef struct{
    uint32_t header;//用于反向索引和dirty信息
    Seg seg[256];
}Header_Seg;

// //带bitmap header的段结构体定义，用于写入到读缓存
// typedef struct{
//     uint32_t header;//用于反向索引和dirty信息
//     //每个block有256个page 256/8=32个bitmap
//     uint32_t bitmap[8];
//     Segb seg[256];
// }Header_Seg_b;

//传统table结构体定义
typedef struct{
    uint32_t table_head;//用于反向索引
    //每个block有256个page 256/8=32个bitmap
    uint32_t bitmap[8];
    VPPN l2p[256]; 
}Table;

typedef struct {
    
    uint8_t seg_num;
    Table table;
    Header_Seg header_seg;
    uint64_t next_avail_time;
}G_map;


//写缓存表
typedef struct {

    //写缓存 写指针达到write_table_capacity时写满了需要进行驱逐
    uint32_t write_point;
    //cache中用于存储写缓存可用于存储最大的table的数量
    uint32_t write_table_capacity;
    //写缓存
    Table* write_table;

}Write_Cache;

typedef struct  {
    uint32_t st;//左边界
    uint32_t end;// 右边界 end=seg_size*num +st
    uint32_t max_seg_num;//段的最大数量
    uint32_t min_seg_num;//段的最小数量
    uint32_t size;//每个seg的大小 加上header反向索引  也可能表示传统的table映射表的大小占用多少个字节
    int32_t num;//已存在的seg的数量 占用空间为seg_size*num
    
}Read_Cache_Space;


//读缓存结构体定义
typedef struct{

    //read cache的利用率不能低于这个值
    float cache_ratio;
    //读缓存空间
    uint8_t* read_cache;
    //读缓存空间大小占用多少字节
    uint32_t cache_size;
        //可用空间大小是多少
    uint32_t free_size;
    //读缓存划分空间数量
    uint32_t space_num;
    //读缓存划分空间
    Read_Cache_Space*  read_cache_space;
    
    //将对应的段大小 段可能有256个映射到对应的空间的下标
    uint32_t* segsize2space;

    uint32_t max_seg_size;//如果大于这个值那么seg将会被转换为table的形式

    //读缓存淘汰的最大空间
    uint32_t max_evict_size;

}Read_Cache;



//缓存结构体定义
typedef struct{
    
    //缓存大小
    uint32_t cache_size;
    //写缓存大小
    uint32_t writeCacheSize;
    //读缓存大小
    uint32_t readCacheSize;
    //缓存
    uint8_t* cache;
    //写缓存
    Write_Cache* write_cache;
    //读缓存
    Read_Cache* read_cache;
    uint32_t table_size;
    uint32_t seg_size;
    uint32_t header_size;


}Cache;




typedef struct{
    Seg_LRU *seg_LRU;
    // struct Pos_Entry* pos_entry;
    Cache* cache;
    int write_cache_LRU_head;
    int read_cache_LRU_head;
    G_map *g_map;
    uint64_t* size_migrate;
    uint64_t nex_migrate;

} FTL_Map;





// static FTL_Map* init_FTL_Map(FemuCtrl *n);
// static void free_FTL_Map(FTL_Map* ftl_map);
// static void seg2table(Seg* seg_st,int max_size, Table* l2p);
// static int table2seg(Seg* seg_st, Table* l2p);
// static void insert_seg_to_read_cache(struct ssd* ssd, void* header_seg_table,int seg_num);
// static void insert_table_to_write_table(struct ssd*ssd,Table* table_st);
// static uint16_t read_SegTable(struct ssd *ssd, NvmeRequest *req,uint32_t lpn);
// static void write_SegTable(struct ssd* ssd,NvmeRequest *req ,uint32_t* lpn,uint16_t* ppn,int num);

// static void print_read_cache_space_by_index(FTL_Map* ftl_map,int i);
// static void print_read_cache_space(FTL_Map* ftl_map);






typedef struct lr_breakpoint {
    uint16_t b;
    uint16_t bitmap;
}lr_breakpoint;

typedef struct lr_node {
    lr_breakpoint brks[MAX_INTERVALS];//4*16=64个字节
    uint8_t u;
}lr_node;


/* describe a physical page addr */
struct ppa {
    union {
        struct {
            uint64_t blk : BLK_BITS;
            uint64_t pg  : PG_BITS;
            uint64_t sec : SEC_BITS;
            uint64_t pl  : PL_BITS;
            uint64_t lun : LUN_BITS;
            uint64_t ch  : CH_BITS;
            uint64_t rsv : 1;
        } g;
        
        uint64_t ppa;
    };
};

typedef int nand_sec_status_t;

struct nand_page {
    nand_sec_status_t *sec;
    int nsecs;
    int status;
};

struct nand_block {
    struct nand_page *pg;
    int npgs;
    int ipc; /* invalid page count */
    int vpc; /* valid page count */
    int erase_cnt;
    int wp; /* current write pointer */
};

struct nand_plane {
    struct nand_block *blk;
    int nblks;
};

struct nand_lun {
    struct nand_plane *pl;
    int npls;
    uint64_t next_lun_avail_time;
    bool busy;
    uint64_t gc_endtime;
};

struct ssd_channel {
    struct nand_lun *lun;
    int nluns;
    uint64_t next_ch_avail_time;
    bool busy;
    uint64_t gc_endtime;
};

struct ssdparams {
    int secsz;        /* sector size in bytes */
    int secs_per_pg;  /* # of sectors per page */
    int pgs_per_blk;  /* # of NAND pages per block */
    int blks_per_pl;  /* # of blocks per plane */
    int pls_per_lun;  /* # of planes per LUN (Die) */
    int luns_per_ch;  /* # of LUNs per channel */
    int nchs;         /* # of channels in the SSD */

    int pg_rd_lat;    /* NAND page read latency in nanoseconds */
    int pg_wr_lat;    /* NAND page program latency in nanoseconds */
    int blk_er_lat;   /* NAND block erase latency in nanoseconds */
    int ch_xfer_lat;  /* channel transfer latency for one page in nanoseconds
                       * this defines the channel bandwith
                       */
    
    double gc_thres_pcent;
    int gc_thres_lines;
    double gc_thres_pcent_high;
    int gc_thres_lines_high;
    bool enable_gc_delay;

    /* below are all calculated values */
    int secs_per_blk; /* # of sectors per block */
    int secs_per_pl;  /* # of sectors per plane */
    int secs_per_lun; /* # of sectors per LUN */
    int secs_per_ch;  /* # of sectors per channel */
    int tt_secs;      /* # of sectors in the SSD */

    int pgs_per_pl;   /* # of pages per plane */
    int pgs_per_lun;  /* # of pages per LUN (Die) */
    int pgs_per_ch;   /* # of pages per channel */
    int tt_pgs;       /* total # of pages in the SSD */

    int blks_per_lun; /* # of blocks per LUN */
    int blks_per_ch;  /* # of blocks per channel */
    int tt_blks;      /* total # of blocks in the SSD */

    int secs_per_line;
    int pgs_per_line;
    int blks_per_line;
    int tt_lines;

    int pls_per_ch;   /* # of planes per channel */
    int tt_pls;       /* total # of planes in the SSD */

    int tt_luns;      /* total # of LUNs in the SSD */


    //==================modified====================
    int pg_size;
    int addr_size;
    int addr_per_pg;
    int tt_trans_pgs; 
    int tt_line_wps;
    int trans_per_line;

    int ents_per_pg;
    int tt_cmt_size;
    int tt_gtd_size;
    int interval_size;

    // * the virtual ppn params
    int chn_per_lun;
    int chn_per_pl;
    int chn_per_pg;
    int chn_per_blk;
    
    int write_cache_size;
    int tt_gtdwpp_cnt;
    
    //bool enable_request_prefetch;
    //bool enable_select_prefetch;

};

typedef struct line {
    int id;  /* line id, the same as corresponding block id */
    int ipc; /* invalid page count in this line */
    int vpc; /* valid page count in this line */
    int type;

    // * identify how many entries remain in this line
    int rest;
    
    QTAILQ_ENTRY(line) entry; /* in either {free,victim,full} list */
    /* position in the priority queue for victim lines */
    size_t                  pos;
} line;

// *  modify ====================
struct wp_lines{
    
    struct line *line;
    struct wp_lines *next;
};

/* wp: record next write addr */
struct write_pointer {
    struct line *line_id[Gc_threshold];//This indicates the group number of the physical block group applied for by this logical block group ；Since Gc_threshold-1=4, it can be represented as 2bits
    struct line *curline;
    struct wp_lines *wpl;//使用这个来代替lr_node中的write_pos.
    int vic_cnt;//也表示line_id中的数目
    int ch;
    int lun;
    int pg;
    int blk;
    int pl;
    int id;

    int write_point_type;//-1表示为data line的指针否则表示翻译页的指针
    int curline_pos;
    // TODO: how many lines this wpp invade
    int invade_lines;
};

struct line_statistic {
    int rest;
};

struct line_mgmt {
    struct line *lines;
    /* free line list, we only need to maintain a list of blk numbers */
    QTAILQ_HEAD(free_line_list, line) free_line_list;
    pqueue_t *victim_line_pq;
    //QTAILQ_HEAD(victim_line_list, line) victim_line_list;
    QTAILQ_HEAD(full_line_list, line) full_line_list;
    QTAILQ_HEAD(victim_list, line) victim_list;
    int tt_lines;
    int free_line_cnt;
    int victim_line_cnt;
    int full_line_cnt;
};

struct nand_cmd {
    int type;
    int cmd;
    int64_t stime; /* Coperd: request arrival time */
};


// a simple hash table for DFTL
struct ht {
    int max_depth;
    int left_num;
    int global_version;
    struct ppa *entry;
    uint64_t *lpns;
    uint32_t *version;
    uint8_t *dirty;

};

struct statistics {
    uint64_t cmt_hit_cnt;
    uint64_t cmt_miss_cnt;
    double cmt_hit_ratio;
    uint64_t access_cnt;
    uint64_t model_hit_num;
    uint64_t model_use_num;
    uint64_t model_out_range;
    uint64_t max_lpn;
    uint64_t min_lpn;
    uint64_t req_num;
    long double average_lat;
    uint64_t gc_times;

    uint64_t write_num;
    uint64_t should_write_num;
    uint64_t erase_cnt;
    uint64_t write_cache_hit;

    uint64_t line_gc_times[BLKS_PER_LINE];
    uint64_t wp_victims[BLKS_PER_LINE];
    uint64_t trans_wp_gc_times;
    uint64_t line_wp_gc_times;


    
    long long calculate_time;
    long long sort_time;
    long long predict_time;

    long long all_count;
    long long seg_count;
    long long GC_erase_time;
    long long GC_write_time;
    long long GC_read_time;
    long long GC_insert_time;
    long long GC_time;


    long long read_CMT_time;
    long long insert_CMT_time;
    long long write_time;
    long long read_time;
    // uint64_t model_insert_time[100000];
    // int model_insert_pos;
    

    long long model_training_nums;
    // uint64_t max_read_lpn;
    // uint64_t min_read_lpn;
    // uint64_t max_write_lpn;
    // uint64_t min_write_lpn;
    long double read_joule;
    long double write_joule;
    long double erase_joule;
    long double joule;
};

struct ssd {
    char *ssdname;
    struct ssdparams sp;
    struct ssd_channel *ch;
    struct ppa *maptbl; /* page level mapping table */
    uint64_t *rmap;     /* reverse mapptbl, assume it's stored in OOB */
    int bitmap_table[256];//8位bitcount表
    bool*seg_bitmaps;
 
    struct line_mgmt lm;

    //===============modified=======================
    struct ppa *gtd;    // global translation blocks
    struct write_pointer *gtd_wps;  // for every 32 translation pags, there is a write pointer
   FTL_Map  *ftl_map;

 


    struct write_pointer* trans_wp; // the write pointer for writing translation pages
    struct lr_node *lr_nodes;  // the linear regression model

    uint64_t num_trans_write;
    uint64_t num_data_write;
    uint64_t num_data_read;

    /* lockless ring for communication with NVMe IO thread */
    struct rte_ring **to_ftl;
    struct rte_ring **to_poller;
    bool *dataplane_started_ptr;
    
    bool model_used;
    uint8_t *valid_lines;

    struct statistics stat;
    QemuThread ftl_thread;

    // * the line-write_pointer mapping
    struct write_pointer **line2write_pointer;

};

void ssd_init(FemuCtrl *n);
void count_segments(struct ssd* ssd);



#ifdef FEMU_DEBUG_FTL
#define ftl_debug(fmt, ...) \
    do { printf("[FEMU] FTL-Dbg: " fmt, ## __VA_ARGS__); } while (0)
#else
#define ftl_debug(fmt, ...) \
    do { } while (0)
#endif

#define ftl_err(fmt, ...) \
    do { fprintf(stderr, "[FEMU] FTL-Err: " fmt, ## __VA_ARGS__); } while (0)

#define ftl_log(fmt, ...) \
    do { printf("[FEMU] FTL-Log: " fmt, ## __VA_ARGS__); } while (0)


/* FEMU assert() */
#ifdef FEMU_DEBUG_FTL
#define ftl_assert(expression) assert(expression)
#else
#define ftl_assert(expression)
#endif





#define ADD_WRITE_CACHE_LRU(ftl_map, pos) \
do { \
    Seg_LRU *_seg_LRU = (ftl_map)->seg_LRU; \
    int _LRU_head = (ftl_map)->write_cache_LRU_head; \
    int _nex = _seg_LRU[_LRU_head].nex; \
    _seg_LRU[(pos)].nex = _nex; \
    _seg_LRU[_nex].pre = (pos); \
    _seg_LRU[_LRU_head].nex = (pos); \
    _seg_LRU[(pos)].pre = _LRU_head; \
    (ftl_map)->cache->write_cache->write_point++; \
} while(0)



#define REMOVE_WRITE_CACHE_LRU(ftl_map, pos) \
do { \
    Seg_LRU *_seg_LRU = (ftl_map)->seg_LRU; \
    (_seg_LRU)[(_seg_LRU)[(pos)].pre].nex = (_seg_LRU)[(pos)].nex; \
    (_seg_LRU)[(_seg_LRU)[(pos)].nex].pre = (_seg_LRU)[(pos)].pre; \
    (ftl_map)->cache->write_cache->write_point--; \
} while(0)

#define ADD_READ_CACHE_LRU(ftl_map, pos) \
do { \
    Seg_LRU *_seg_LRU = (ftl_map)->seg_LRU; \
    int _LRU_head = (ftl_map)->read_cache_LRU_head; \
    int _nex = (_seg_LRU)[_LRU_head].nex; \
    (_seg_LRU)[(pos)].nex = _nex; \
    (_seg_LRU)[_nex].pre = (pos); \
    (_seg_LRU)[_LRU_head].nex = (pos); \
    (_seg_LRU)[(pos)].pre = _LRU_head; \
} while(0)

#define REMOVE_READ_CACHE_LRU(ftl_map, pos) \
do { \
    Seg_LRU *_seg_LRU = (ftl_map)->seg_LRU; \
    (_seg_LRU)[(_seg_LRU)[(pos)].pre].nex = (_seg_LRU)[(pos)].nex; \
    (_seg_LRU)[(_seg_LRU)[(pos)].nex].pre = (_seg_LRU)[(pos)].pre; \
} while(0)


// //打印一下read_cache_LRU链表
// static void print_read_cache_LRU(FTL_Map* ftl_map)
// {
//     Seg_LRU *seg_LRU = ftl_map->seg_LRU;
//     int nex = ftl_map->read_cache_LRU_head; 
//     // printf("read_cache_LRU_head:%d\n", seg_LRU[nex].nex);
//     // printf("read_cache_LRU_tail:%d\n", seg_LRU[nex].pre);

//     uint64_t count = 0;
//     double size =0;
//     do{
//         // printf("pos:%d\n", nex);
//         nex = seg_LRU[nex].nex;
//         if (nex !=ftl_map->read_cache_LRU_head)
//         {
//             count++;
//         }
        
//     }while(nex!=ftl_map->read_cache_LRU_head);
//     printf("read_cache_LRU_count:%lld\n", (long long)count);
//     for(int i =0;i<ftl_map->ftl_map->cache->read_cache->space_num;++i)
//     {
//         size += ftl_map->ftl_map->cache->read_cache->read_cache_space[i].end - ftl_map->ftl_map->cache->read_cache->read_cache_space[i].st;
//     }
//     printf("read_cache_LRU_size:%lf B\n", size);
//     printf("read_cache_LRU_size average size:%lf B\n", size/count);
// }


// //打印一下write_cache_LRU链表
// static void print_write_cache_LRU(FTL_Map* ftl_map)
// {
//     Seg_LRU *seg_LRU = ftl_map->seg_LRU;
//     int nex = ftl_map->write_cache_LRU_head; 
//     printf("write_point:%d\n", ftl_map->cache->write_cache->write_point);
//     printf("write_cache_LRU_head:%d\n", seg_LRU[nex].nex);
//     printf("write_cache_LRU_tail:%d\n", seg_LRU[nex].pre);
//     do{
//         printf("pos:%d\n", nex);
//         nex = seg_LRU[nex].nex;
//     }while(nex!=ftl_map->write_cache_LRU_head);
// }










#endif