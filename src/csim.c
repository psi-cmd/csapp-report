#include "cachelab.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

int verbose, sets_bit, lines, bits;  //是否细节输出、组占位数、行数、块位数
int hits, misses, evictions;  //命中、未命中、清存次数
char line_p[40];   //原始

void cache_init();  //初始化结构体
void process();    //读取数据
void LRU_increment();   //每周期加一
void hit();
void miss();
void eviction();

FILE *f;

struct set {
    unsigned long *tag;    //标志位
    unsigned *valid_bits;     //有效位
    unsigned *LRU_counter;     //LRU 计数器
} *sets;


int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char opt;
    while ((opt = getopt(argc, argv, "s:E:b:t:hv")) != -1) {
        switch (opt) {
            case 'h':
                puts("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n"
                     "Options:\n"
                     " -h\t\tPrint this help message.\n"
                     " -v\t\tOptional verbose flag.\n"
                     " -s <num>\tNumber of set index bits.\n"
                     " -E <num>\tNumber of lines per set.\n"
                     " -b <num>\tNumber of block offser bits.\n"
                     " -t <file>\tTrace file.\n"
                     "\n"
                     "Examples:\n"
                     " linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n"
                     " linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
                return 0;
            case 'v':
                verbose = 1;
                break;
            case 's':
                sscanf(optarg, "%d", &sets_bit);
                break;
            case 'E':
                sscanf(optarg, "%d", &lines);
                break;
            case 'b':
                sscanf(optarg, "%d", &bits);
                break;
            case 't':
                f = fopen(optarg, "r");
                break;
            default:
                printf("Unrecognized arg: %c", opt);
                return 1;
        }
    }
    if (!(sets_bit && lines && bits && f)) {
        puts("No enough args/zero input.");
    }
    cache_init();
    process();
    printSummary(hits, misses, evictions);
    return 0;
}

void cache_init() {
    sets = (struct set *) malloc(sizeof(struct set) * (1 << sets_bit));
    for (int i = 0; i < (1 << sets_bit); i++) {
        sets[i].tag = (unsigned long *) malloc(sizeof(int) * lines);
        sets[i].valid_bits = (unsigned *) malloc(sizeof(int) * lines);
        for (int j = 0; j < lines; j++)
            sets[i].valid_bits[j] = 0;
        sets[i].LRU_counter = (unsigned *) malloc(sizeof(int) * lines);
    }
}

void process() {
    char mode;
    unsigned long hex;
    unsigned size, set_index, miss_state, eviction_state, m, tag;  //外循环变量
    unsigned LRU_high_line, LRU_high_v;  //内循环变量
    while (1) {
        fgets(line_p, 39, f);
        if (feof(f)) break;
        if (line_p[strlen(line_p)-1] == '\n')
            line_p[strlen(line_p)-1] = 0;
        sscanf(line_p, " %c %lx,%d", &mode, &hex, &size);
        switch (mode) {
            case 'I':
                continue;
            case 'M':
                m = 1;
                break;
            default:
                m = 0;
                break;
        }
        if (verbose)
            printf("%s", line_p + 1);
        miss_state = 0;
        eviction_state = 0;
        tag = hex >> (bits + sets_bit);
        set_index = (hex >> bits) ^ (tag << sets_bit); //判断组
        recheck:                //超过本行范围需要重新查找
        LRU_high_line = 0;
        LRU_high_v = 0;
        for (int i = 0; i < lines; ++i) {
            if (!sets[set_index].valid_bits[i]) {   //miss
                miss_state++;
                sets[set_index].tag[i] = tag;
                sets[set_index].valid_bits[i] = 1;
                sets[set_index].LRU_counter[i] = 0;
                goto end;
            }
            if (sets[set_index].tag[i] == tag) {   // hit
                sets[set_index].LRU_counter[i] = 0;
                goto end;
            }
            if (sets[set_index].LRU_counter[i] > LRU_high_v) {      //顺便统计未访问累计最大值
                LRU_high_line = i;
                LRU_high_v = sets[set_index].LRU_counter[i];
            }
        }
        eviction_state++;
        sets[set_index].tag[LRU_high_line] = tag;     // miss eviction
        sets[set_index].LRU_counter[LRU_high_line] = 0;
        end:
        if (eviction_state){
            miss();
            eviction();
        } else if (miss_state)
            miss();
        else
            hit();
        if (m) {
            m = 0;
            miss_state = 0;
            eviction_state = 0;
            goto recheck;
        }
        if (verbose) puts("");
        LRU_increment(sets[set_index]);        //update count
    }
}

inline void LRU_increment(struct set Set) {
    for (int i = 0; i < lines; i++) {
        Set.LRU_counter[i]++;
    }
}

inline void hit() {
    hits++;
    if (verbose) {
        printf(" hit");
    }
}

inline void miss() {
    misses++;
    if (verbose) {
        printf(" miss");
    }
}

inline void eviction() {
    evictions++;
    if (verbose) {
        printf(" eviction");
    }
}