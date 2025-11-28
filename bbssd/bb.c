#include "../nvme.h"
#include "./Segtable.h"
// #include "./SegtableBuffer.h"
// #include "./ld-tpftl.h"
// #include "./sftl.h"
// #include "./ld-tpftl.h"
// #include "./tpftl.h"
// #include "./dftl.h"


static void bb_init_ctrl_str(FemuCtrl *n)
{
    static int fsid_vbb = 0;
    const char *vbbssd_mn = "FEMU BlackBox-SSD Controller";
    const char *vbbssd_sn = "vSSD";

    nvme_set_ctrl_name(n, vbbssd_mn, vbbssd_sn, &fsid_vbb);
}

// static void print2file(uint64_t* size_migrate){
//     // 打开CSV文件
//     FILE *file = fopen("/home/fxx/桌面/migrate_size.csv", "w");
//     if (file == NULL) {
//         fprintf(stderr, "无法创建文件\n");
//         return ;
//     }

//     // 写入表头（可选）
//     fprintf(file, "value\n");
//     // 逐行写入数据
//     for (int i = 0; i < 100000; i++) {
//         fprintf(file, "%lld\n", (long long)size_migrate[i]);
//     }

//     fclose(file);

//     printf("数据已成功输出到output.csv\n");
//     return ;

// }

// static void print2file_model_insert_time(uint64_t* model_insert_time){
//     // 打开CSV文件
//     FILE *file = fopen("/home/fxx/桌面/model_insert_time.csv", "w");
//     if (file == NULL) {
//         fprintf(stderr, "无法创建文件\n");
//         return ;
//     }
//     printf("开始写入model_insert_time.csv\n");

//     // 写入表头（可选）
//     fprintf(file, "value\n");
//     // 逐行写入数据
//     for (int i = 0; i < 100000; i++) {
//         fprintf(file, "%lld\n", (long long)model_insert_time[i]);
//     }

//     fclose(file);

//     printf("数据已成功输出到model_insert_time.csv\n");
//     return ;

// }


/* bb <=> black-box */
static void bb_init(FemuCtrl *n, Error **errp)
{
    struct ssd *ssd = n->ssd = g_malloc0(sizeof(struct ssd));

    bb_init_ctrl_str(n);

    ssd->dataplane_started_ptr = &n->dataplane_started;
    ssd->ssdname = (char *)n->devname;
    femu_debug("Starting FEMU in Blackbox-SSD mode ...\n");
    ssd_init(n);
}

static void reset_stat(struct ssd *ssd)
{
     struct statistics *st = &ssd->stat;

    // st->access_cnt = 0;
    // st->cmt_hit_cnt  = 0;    
    // st->model_out_range  = 0;
    // st->write_cache_hit = 0;
    // st->should_write_num = 0;



    /*FTL*/
    // st->read_joule = 0;
    // st->write_joule = 0;
    // st->erase_joule = 0;
    // st->joule = 0;

    /*DFTL */
    //  st->cmt_hit_cnt = 0;
    //  st->cmt_miss_cnt = 0;
    //  st->cmt_hit_ratio = 0;
    //  st->access_cnt = 0;

    //  st->write_num = 0;
    //  st->should_write_num = 0;
    //  st->erase_cnt = 0;


    //  st->read_joule = 0;
    //  st->write_joule = 0;
    //  st->erase_joule = 0;
    //  st->joule = 0;

    /*SFTL */
    //  st->cmt_hit_cnt = 0;
    //  st->cmt_miss_cnt = 0;
    //  st->cmt_hit_ratio = 0;
    //  st->access_cnt = 0;

    //  st->write_num = 0;
    //  st->should_write_num = 0;
    //  st->erase_cnt = 0;


    //  st->read_joule = 0;
    //  st->write_joule = 0;
    //  st->erase_joule = 0;
    //  st->joule = 0;
    //  st->write_process_count=0;
    //  st->write_process_read_count=0;


    /*TPFTL*/
    // st->cmt_hit_cnt = 0;
    // st->cmt_miss_cnt = 0;
    // st->cmt_hit_ratio = 0;
    // st->write_num = 0;
    // st->should_write_num = 0;
    // st->erase_cnt = 0;
    // st->access_cnt = 0;
    // st->write_cache_hit = 0;
    
    // st->read_joule = 0;
    // st->write_joule = 0;
    // st->erase_joule = 0;
    // st->joule = 0;
    
    /*LeaFTL*/
    // st->cmt_hit_cnt = 0;
    // st->cmt_miss_cnt = 0;
    // st->cmt_hit_ratio = 0;
    // st->access_cnt = 0;
    // st->model_hit = 0;
    // st->write_cnt = 0;
    // st->wa_cnt = 0;
    // st->gc_cnt = 0;
    // st->read_joule = 0;
    // st->write_joule = 0;
    // st->erase_joule = 0;
    // st->joule = 0;
    // count_segments(ssd);

    /*LearnedFTL*/
    //  st->cmt_hit_cnt = 0;
    //  st->cmt_miss_cnt = 0;
    //  st->cmt_hit_ratio = 0;
    //  st->access_cnt = 0;
    // st->write_cache_hit = 0;

    //  st->write_num = 0;
    //  st->should_write_num = 0;
    //  st->erase_cnt = 0;
    //  st->model_training_nums=0;
    //  st->model_training_write=0;

    //  st->model_hit_num = 0;
    //  st->model_use_num = 0;
    //  st->read_joule = 0;
    //  st->write_joule = 0;
    //  st->erase_joule = 0;
    //  st->joule = 0;

    /*LearnedFTL*/

    // st->cmt_hit_cnt = 0;
    //  st->cmt_miss_cnt = 0;
    //  st->cmt_hit_ratio = 0;
    //  st->access_cnt = 0;

    //  st->write_num = 0;
    //  st->should_write_num = 0;
    //  st->erase_cnt = 0;
    // st->write_cache_hit = 0;
    
    // st->sort_time = 0;
    // st->GC_time=0;

    // st->write_time=0;
    // st->read_time=0;
    

    //  st->model_hit_num = 0;
    //  st->model_use_num = 0;
    //  st->read_joule = 0;
    //  st->write_joule = 0;
    //  st->erase_joule = 0;
    //  st->joule = 0;



    /*S+MFTL*/
     st->cmt_hit_cnt = 0;
     st->cmt_miss_cnt = 0;
     st->cmt_hit_ratio = 0;
     st->access_cnt = 0;
    st->write_cache_hit = 0;

     st->write_num = 0;
     st->should_write_num = 0;
     st->erase_cnt = 0;
     st->max_lpn = 0;
     st->min_lpn = INVALID_LPN;
    
    st->sort_time = 0;
    st->GC_erase_time=0;
    st->GC_read_time=0;
    st->GC_write_time=0;
    st->GC_insert_time=0;
    st->GC_time=0;

    st->write_time=0;
    st->read_time=0;
    st->read_CMT_time=0;
    

     st->model_hit_num = 0;
     st->model_use_num = 0;
     st->read_joule = 0;
     st->write_joule = 0;
     st->erase_joule = 0;
     st->joule = 0;
}

//SFTL
//获取平均表中的段的数目
// static void get_cmt_seg_average_size(struct ssd *ssd){
//     int count = ssd->sp.tt_blks - ssd->cm.free_cmt_entry_cnt;
//     double total_size = (double)1.0*ssd->cm.used_cmt_entry_cnt;
//     if(count ==0){
//         printf("cmt is empty\n");
//         return;
//     }
//     printf("total seg size:%lf\n", total_size);
//     printf("total seg count:%d\n", count);
//     printf("average seg size:%lf\n", total_size/(double)count);
//     //return total_size/(double)count;
// }


// static long long total_table_num(struct ssd *ssd)
// {
//     FTL_Map *ftl_map = ssd->ftl_map;
//     int lru_head = (ftl_map)->read_cache_LRU_head;
//     Seg_LRU* seg_LRU = ftl_map->seg_LRU;
//     int nex = seg_LRU[lru_head].nex;
//     long long total = 0;
//     while (nex!=lru_head)
//     {
//         nex = seg_LRU[nex].nex;
//         total++;
//    }
//     return total;
// }

 //S+MFTL打印一下平均大小
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
//     for(int i =0;i<ftl_map->cache->read_cache->space_num;++i)
//     {
//         size += ftl_map->cache->read_cache->read_cache_space[i].end - ftl_map->cache->read_cache->read_cache_space[i].st;
//     }
//     printf("read_cache_LRU_size:%lf B\n", size);
//     printf("read_cache_LRU_size average size:%lf B\n", size/count);
// }
static void print_stat(struct ssd *ssd)
{
   struct statistics *st = &ssd->stat;
    



    /*ftl*/
    // st->joule = st->read_joule + st->write_joule + st->erase_joule;
    // printf("read joule: %Lf\n", st->read_joule);
    // printf("write joule: %Lf\n", st->write_joule);
    // printf("erase joule: %Lf\n", st->erase_joule);
    // printf("All joule: %Lf\n", st->joule);

    /*DFLT*/

    // if (st->access_cnt == 0) {
    //     st->cmt_hit_ratio = 0;
    // } else {
    //     st->cmt_hit_ratio = (double)st->cmt_hit_cnt / st->access_cnt;
    // }
    // st->joule = st->read_joule + st->write_joule + st->erase_joule;
    // printf("CMT hit count: %lu\n", st->cmt_hit_cnt);
    // printf("CMT miss count: %lu\n", st->cmt_miss_cnt);
    // printf("CMT access count: %lu\n", st->access_cnt);
    // printf("CMT hit ratio: %lf\n", st->cmt_hit_ratio);
    // printf("write cnt: %lld\n", (long long)ssd->stat.write_num);
    // printf("should_write cnt: %lld\n", (long long)ssd->stat.should_write_num);
    // printf("erase cnt: %lld\n", (long long)ssd->stat.erase_cnt);
    // printf("read joule: %Lf\n", st->read_joule);
    // printf("write joule: %Lf\n", st->write_joule);
    // printf("erase joule: %Lf\n", st->erase_joule);
    // printf("All joule: %Lf\n", st->joule);

    /*SFLT*/

    // if (st->access_cnt == 0) {
    //     st->cmt_hit_ratio = 0;
    // } else {
    //     st->cmt_hit_ratio = (double)st->cmt_hit_cnt / st->access_cnt;
    // }
    // st->joule = st->read_joule + st->write_joule + st->erase_joule;
    // printf("CMT hit count: %lu\n", st->cmt_hit_cnt);
    // printf("CMT miss count: %lu\n", st->cmt_miss_cnt);
    // printf("CMT access count: %lu\n", st->access_cnt);
    // printf("CMT hit ratio: %lf\n", st->cmt_hit_ratio);
    // printf("write cnt: %lld\n", (long long)ssd->stat.write_num);
    // printf("should_write cnt: %lld\n", (long long)ssd->stat.should_write_num);
    // printf("erase cnt: %lld\n", (long long)ssd->stat.erase_cnt);
    // printf("read joule: %Lf\n", st->read_joule);
    // printf("write joule: %Lf\n", st->write_joule);
    // printf("erase joule: %Lf\n", st->erase_joule);
    // printf("All joule: %Lf\n", st->joule);
    // printf("write process count: %lu\n", st->write_process_count);
    // printf("write process read count: %lu\n", st->write_process_read_count);
    // get_cmt_seg_average_size(ssd);
    

    /*tpftl*/
    // if (st->access_cnt == 0) {
    //     st->cmt_hit_ratio = 0;
    // } else {
    //     //这个算得不对要加上write_cache_hit
    //     st->cmt_hit_ratio = (double)st->cmt_hit_cnt / st->access_cnt;
    // }
    // st->joule = st->read_joule + st->write_joule + st->erase_joule;

    // printf("CMT hit count: %lu\n", st->cmt_hit_cnt);
    // printf("CMT miss count: %lu\n", st->cmt_miss_cnt);
    // printf("CMT access count: %lu\n", st->access_cnt);
    // printf("CMT hit ratio: %lf\n", st->cmt_hit_ratio);
    // printf("write cnt: %lld\n", (long long)ssd->stat.write_num);
    //  printf("should_write cnt: %lld\n", (long long)ssd->stat.should_write_num);
    //  printf("erase cnt: %lld\n", (long long)ssd->stat.erase_cnt);
    // printf("write cache hit:%lld\n",(long long)ssd->stat.write_cache_hit );

    // printf("read joule: %Lf\n", st->read_joule);
    // printf("write joule: %Lf\n", st->write_joule);
    // printf("erase joule: %Lf\n", st->erase_joule);
    // printf("All joule: %Lf\n", st->joule);

    /*LeaFTL*/
    // st->joule = st->read_joule + st->write_joule + st->erase_joule;
    // printf("total cnt: %lld\n", (long long)ssd->stat.access_cnt);
    // printf("cmt cnt: %lld\n", (long long)ssd->stat.cmt_hit_cnt);
    // printf("model cnt: %lld\n", (long long)ssd->stat.model_hit);
    // printf("write cnt: %lld\n", (long long)ssd->stat.write_num);
    // printf("should_write cnt: %lld\n", (long long)ssd->stat.should_write_num);
    // printf("erase cnt: %lld\n", (long long)ssd->stat.erase_cnt);
    // printf("read joule: %Lf\n", st->read_joule);
    // printf("write joule: %Lf\n", st->write_joule);
    // printf("erase joule: %Lf\n", st->erase_joule);
    // printf("All joule: %Lf\n", st->joule);
    // count_segments(ssd);

    /*LearnedFTL*/
    //  st->joule = st->read_joule + st->write_joule + st->erase_joule;
    //  printf("total cnt: %lld\n", (long long)ssd->stat.access_cnt);
    //  printf("cmt cnt: %lld\n", (long long)ssd->stat.cmt_hit_cnt);
    //  printf("model cnt: %lld\n", (long long)ssd->stat.model_hit_num);

    //  printf("write cnt: %lld\n", (long long)ssd->stat.write_num);
    //  printf("should_write cnt: %lld\n", (long long)ssd->stat.should_write_num);
    //  printf("erase cnt: %lld\n", (long long)ssd->stat.erase_cnt);
    // printf("write cache hit:%lld",(long long)ssd->stat.write_cache_hit );
    // printf("model training num:%lld\n",(long long) ssd->stat.model_training_nums);
    // printf("model training write:%lld\n",(long long) ssd->stat.model_training_write);

     
    //  printf("read joule: %Lf\n", st->read_joule);
    //  printf("write joule: %Lf\n", st->write_joule);
    //  printf("erase joule: %Lf\n", st->erase_joule);
    //  printf("All joule: %Lf\n", st->joule);

    /*S+MFTL*/
     st->joule = st->read_joule + st->write_joule + st->erase_joule;
     printf("total cnt: %lld\n", (long long)ssd->stat.access_cnt);
     printf("cmt cnt: %lld\n", (long long)ssd->stat.cmt_hit_cnt);
     printf("model cnt: %lld\n", (long long)ssd->stat.model_hit_num);

     printf("write cnt: %lld\n", (long long)ssd->stat.write_num);
     printf("should_write cnt: %lld\n", (long long)ssd->stat.should_write_num);
     printf("erase cnt: %lld\n", (long long)ssd->stat.erase_cnt);
    printf("write cache hit:%lld\n",(long long)ssd->stat.write_cache_hit );
    printf("max lpn: %llu\n",(unsigned long long)ssd->stat.max_lpn );
    printf("min lpn: %llu\n",(unsigned long long)ssd->stat.min_lpn );
     
     printf("all_count : %lld\n", (long long)ssd->stat.all_count);
     printf("seg_count : %lld\n", (long long)ssd->stat.seg_count);
     printf("sort_time : %lld\n", (long long)ssd->stat.sort_time);
     printf("GC_erase_time : %lld\n", (long long)ssd->stat.GC_erase_time);
     printf("GC_read_time : %lld\n", (long long)ssd->stat.GC_read_time);
     printf("GC_write_time : %lld\n", (long long)ssd->stat.GC_write_time);
     printf("GC_insert_time : %lld\n", (long long)ssd->stat.GC_insert_time);
     printf("GC_time : %lld\n", (long long)ssd->stat.GC_time);

     printf("write_time : %lld\n", (long long)ssd->stat.write_time);

     printf("read_time : %lld\n", (long long)ssd->stat.read_time);
     printf("read_CMT_time : %lld\n", (long long)ssd->stat.read_CMT_time);
     



    //  printf("total table num: %lld\n", total_table_num(ssd));


    //  print_read_cache_LRU(ssd->ftl_map);
     
    // //  print2file(ssd->ftl_map->size_migrate);
    // printf("model_insert_pos:%d\n", ssd->stat.model_insert_pos);

    //  for(int i = 0;i<ssd->ftl_map->cache->read_cache->space_num;i++)
    //  {
    //     //输出space的各个参数
    //     printf("read_cache_space[%d].st = %d\n",i,ssd->ftl_map->cache->read_cache->read_cache_space[i].st);
    //     printf("read_cache_space[%d].end = %d\n",i,ssd->ftl_map->cache->read_cache->read_cache_space[i].end);
    //     printf("read_cache_space[%d].max_seg_num = %d\n",i,ssd->ftl_map->cache->read_cache->read_cache_space[i].max_seg_num);
    //     printf("read_cache_space[%d].min_seg_num = %d\n",i,ssd->ftl_map->cache->read_cache->read_cache_space[i].min_seg_num);
    //     printf("read_cache_space[%d].size = %d\n",i,ssd->ftl_map->cache->read_cache->read_cache_space[i].size);
    //     printf("read_cache_space[%d].num  = %d\n",i,ssd->ftl_map->cache->read_cache->read_cache_space[i].num);
        
    //  }
   
    


    //  printf("read joule: %Lf\n", st->read_joule);
    //  printf("write joule: %Lf\n", st->write_joule);
    //  printf("erase joule: %Lf\n", st->erase_joule);
    //  printf("All joule: %Lf\n", st->joule);
}

static void bb_flip(FemuCtrl *n, NvmeCmd *cmd)
{
    struct ssd *ssd = n->ssd;
    int64_t cdw10 = le64_to_cpu(cmd->cdw10);
    printf("888881118\n");
    switch (cdw10) {
    case FEMU_ENABLE_GC_DELAY:
        ssd->sp.enable_gc_delay = true;
        femu_log("%s,FEMU GC Delay Emulation [Enabled]!\n", n->devname);
        break;
    case FEMU_DISABLE_GC_DELAY:
        ssd->sp.enable_gc_delay = false;
        femu_log("%s,FEMU GC Delay Emulation [Disabled]!\n", n->devname);
        break;
    case FEMU_ENABLE_DELAY_EMU:
        ssd->sp.pg_rd_lat = NAND_READ_LATENCY;
        ssd->sp.pg_wr_lat = NAND_PROG_LATENCY;
        ssd->sp.blk_er_lat = NAND_ERASE_LATENCY;
        ssd->sp.ch_xfer_lat = 0;
        femu_log("%s,FEMU Delay Emulation [Enabled]!\n", n->devname);
        break;
    case FEMU_DISABLE_DELAY_EMU:
        ssd->sp.pg_rd_lat = 0;
        ssd->sp.pg_wr_lat = 0;
        ssd->sp.blk_er_lat = 0;
        ssd->sp.ch_xfer_lat = 0;
        femu_log("%s,FEMU Delay Emulation [Disabled]!\n", n->devname);
        break;
    case FEMU_RESET_ACCT:
        n->nr_tt_ios = 0;
        n->nr_tt_late_ios = 0;
        femu_log("%s,Reset tt_late_ios/tt_ios,%lu/%lu\n", n->devname,
                n->nr_tt_late_ios, n->nr_tt_ios);
        break;
    case FEMU_ENABLE_LOG:
        n->print_log = true;
        femu_log("%s,Log print [Enabled]!\n", n->devname);
        break;
    case FEMU_DISABLE_LOG:
        n->print_log = false;
        femu_log("%s,Log print [Disabled]!\n", n->devname);
        break;
    case FEMU_RESET_STAT:
        reset_stat(ssd);
        femu_log("%s,Reset statistics!\n", n->devname);
        break;
    case FEMU_PRINT_STAT:
        print_stat(ssd);
        femu_log("%s,Statistics print!\n", n->devname);
        break;
    default:
        printf("FEMU:%s,Not implemented flip cmd (%lu)\n", n->devname, cdw10);
    }
}

static uint16_t bb_nvme_rw(FemuCtrl *n, NvmeNamespace *ns, NvmeCmd *cmd,
                           NvmeRequest *req)
{
    return nvme_rw(n, ns, cmd, req);
}

static uint16_t bb_io_cmd(FemuCtrl *n, NvmeNamespace *ns, NvmeCmd *cmd,
                          NvmeRequest *req)
{
    switch (cmd->opcode) {
    case NVME_CMD_READ:
    case NVME_CMD_WRITE:
        return bb_nvme_rw(n, ns, cmd, req);
    default:
        return NVME_INVALID_OPCODE | NVME_DNR;
    }
}

static uint16_t bb_admin_cmd(FemuCtrl *n, NvmeCmd *cmd)
{
    switch (cmd->opcode) {
    case NVME_ADM_CMD_FEMU_FLIP:
        bb_flip(n, cmd);
        return NVME_SUCCESS;
    default:
        return NVME_INVALID_OPCODE | NVME_DNR;
    }
}

int nvme_register_bbssd(FemuCtrl *n)
{
    n->ext_ops = (FemuExtCtrlOps) {
        .state            = NULL,
        .init             = bb_init,
        .exit             = NULL,
        .rw_check_req     = NULL,
        .admin_cmd        = bb_admin_cmd,
        .io_cmd           = bb_io_cmd,
        .get_log          = NULL,
    };

    return 0;
}

