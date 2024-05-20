#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ĳ�� ��� ����ü
typedef struct {
    unsigned int tag;              // �±� (�޸� �ּ��� �Ϻ�)
    int valid;                     // ��ȿ ��Ʈ (�� ����� ��ȿ���� ����)
    int dirty;                     // ��Ƽ ��Ʈ (�� ����� �����Ǿ����� ����)
    unsigned long long lru_counter;// LRU ī���� (��� �� ����)
} CacheBlock;

// ĳ�� ��Ʈ ����ü
typedef struct {
    CacheBlock* blocks;            // ��Ʈ ���� ĳ�� ��� �迭
} CacheSet;

// ĳ�� ����ü
typedef struct {
    CacheSet* sets;                // ĳ�� ��Ʈ �迭
    int num_sets;                  // ��Ʈ�� ��
    int blocks_per_set;            // ��Ʈ�� ����� ��
    int bytes_per_block;           // ��ϴ� ����Ʈ ��
    int write_allocate;            // ���� �Ҵ� ����
    int write_back;                // ���� �� ����
    unsigned long long lru_counter;// ��ü LRU ī����
} Cache;

// ĳ�� �ʱ�ȭ �Լ�
Cache* initialize_cache(int num_sets, int blocks_per_set, int bytes_per_block, int write_allocate, int write_back) {
    Cache* cache = (Cache*)malloc(sizeof(Cache));      // ĳ�� ����ü �Ҵ�
    cache->num_sets = num_sets;                        // ��Ʈ�� �� ����
    cache->blocks_per_set = blocks_per_set;            // ��Ʈ�� ����� �� ����
    cache->bytes_per_block = bytes_per_block;          // ��ϴ� ����Ʈ �� ����
    cache->write_allocate = write_allocate;            // ���� �Ҵ� ���� ����
    cache->write_back = write_back;                    // ���� �� ���� ����
    cache->lru_counter = 0;                            // LRU ī���� �ʱ�ȭ
    cache->sets = (CacheSet*)malloc(num_sets * sizeof(CacheSet)); // ĳ�� ��Ʈ �迭 �Ҵ�

    for (int i = 0; i < num_sets; i++) {
        cache->sets[i].blocks = (CacheBlock*)malloc(blocks_per_set * sizeof(CacheBlock)); // �� ��Ʈ�� ���� ĳ�� ��� �迭 �Ҵ�
        for (int j = 0; j < blocks_per_set; j++) {
            cache->sets[i].blocks[j].valid = 0;       // ����� ��ȿ ��Ʈ �ʱ�ȭ
            cache->sets[i].blocks[j].dirty = 0;       // ����� ��Ƽ ��Ʈ �ʱ�ȭ
            cache->sets[i].blocks[j].lru_counter = 0; // ����� LRU ī���� �ʱ�ȭ
        }
    }
    return cache; // �ʱ�ȭ�� ĳ�� ��ȯ
}

// ĳ�� �޸� ���� �Լ�
void free_cache(Cache* cache) {
    for (int i = 0; i < cache->num_sets; i++) {
        free(cache->sets[i].blocks); // �� ��Ʈ�� ��� �޸� ����
    }
    free(cache->sets); // ��Ʈ �迭 �޸� ����
    free(cache); // ĳ�� ����ü �޸� ����
}

// �ּҿ��� �±׿� ��Ʈ �ε����� �����ϴ� �Լ�
void get_tag_and_set(Cache* cache, unsigned int address, unsigned int* tag, unsigned int* set_index) {
    int block_offset_bits = (int)log2(cache->bytes_per_block); // ��� ������ ��Ʈ �� ���
    int set_index_bits = (int)log2(cache->num_sets);           // ��Ʈ �ε��� ��Ʈ �� ���
    *set_index = (address >> block_offset_bits) & (cache->num_sets - 1); // ��Ʈ �ε��� ����
    *tag = address >> (block_offset_bits + set_index_bits);    // �±� ����
}

// ĳ�ÿ� �����ϴ� �Լ�
int access_cache(Cache* cache, char type, unsigned int address, int* load_hits, int* load_misses, int* store_hits, int* store_misses, unsigned long long* total_cycles) {
    unsigned int tag, set_index;
    get_tag_and_set(cache, address, &tag, &set_index); // �ּҿ��� �±׿� ��Ʈ �ε��� ����
    CacheSet* set = &cache->sets[set_index];           // �ش� ��Ʈ ����
    cache->lru_counter++;                              // ��ü LRU ī���� ����

    // ĳ�� ��� Ž��
    for (int i = 0; i < cache->blocks_per_set; i++) {
        if (set->blocks[i].valid && set->blocks[i].tag == tag) { // ��ȿ�ϰ� �±װ� ��ġ�ϴ� ����� ã��
            set->blocks[i].lru_counter = cache->lru_counter;     // LRU ī���� ����
            if (type == 'l') {
                (*load_hits)++;      // �ε� ��Ʈ ����
                *total_cycles += 1;  // �ε� ����Ŭ ����
            }
            else if (type == 's') {
                set->blocks[i].dirty = 1; // ��Ƽ ��Ʈ ����
                (*store_hits)++;          // ����� ��Ʈ ����
                *total_cycles += 1;       // ����� ����Ŭ ����
            }
            return 1; // ĳ�� ��Ʈ ��ȯ
        }
    }

    // �̽� ó��
    if (type == 'l') {
        (*load_misses)++;                 // �ε� �̽� ����
        *total_cycles += 100 * (cache->bytes_per_block / 4); // �޸� ���� ����Ŭ ����
    }
    else if (type == 's') {
        (*store_misses)++;                // ����� �̽� ����
        *total_cycles += 100 * (cache->bytes_per_block / 4); // �޸� ���� ����Ŭ ����
    }

    int lru_index = 0; // LRU �ε��� �ʱ�ȭ
    unsigned long long min_lru = set->blocks[0].lru_counter; // �ּ� LRU �� �ʱ�ȭ

    for (int i = 1; i < cache->blocks_per_set; i++) {
        if (!set->blocks[i].valid) { // ��ȿ���� ���� ����� ������ ��ü ������� ����
            lru_index = i;
            break;
        }
        if (set->blocks[i].lru_counter < min_lru) { // LRU ���� �� ���� ����� ã��
            lru_index = i;
            min_lru = set->blocks[i].lru_counter;
        }
    }

    if (set->blocks[lru_index].valid && set->blocks[lru_index].dirty && cache->write_back) { // ��ü�� ����� ��ȿ�ϰ� ��Ƽ ��Ʈ�� �����Ǿ� ������ write-back ����� ��
        *total_cycles += 100 * (cache->bytes_per_block / 4); // �޸𸮷� ���� �� ����Ŭ ����
    }

    set->blocks[lru_index].valid = 1;          // ��ü�� ����� ��ȿ�ϰ� ����
    set->blocks[lru_index].tag = tag;          // ���ο� �±� ����
    set->blocks[lru_index].dirty = (type == 's'); // ������� ��� ��Ƽ ��Ʈ ����
    set->blocks[lru_index].lru_counter = cache->lru_counter; // LRU ī���� ����

    if (type == 's' && !cache->write_allocate) { // ���� �Ҵ��� �ƴ� ���
        set->blocks[lru_index].valid = 0; // ����� ��ȿȭ
    }

    return 0; // ĳ�� �̽� ��ȯ
}

int main() {
    int num_sets = 256;           // ��Ʈ�� ��
    int blocks_per_set = 4;       // ��Ʈ�� ����� ��
    int bytes_per_block = 16;     // ��ϴ� ����Ʈ ��
    int write_allocate = 1;       // ���� �Ҵ� ��� (1: write-allocate)
    int write_back = 1;           // ���� �� ��� (1: write-back)
    const char* tracefile = "gcc.trace"; // �޸� ���� Ʈ���̽� ����

    Cache* cache = initialize_cache(num_sets, blocks_per_set, bytes_per_block, write_allocate, write_back); // ĳ�� �ʱ�ȭ

    FILE* file = fopen(tracefile, "r"); // Ʈ���̽� ���� ����
    if (file == NULL) {                 // ���� ���� ���� ó��
        fprintf(stderr, "Error: Unable to open file %s\n", tracefile);
        free_cache(cache);              // ĳ�� �޸� ����
        return 1;
    }

    char type;             // ���� Ÿ�� (�ε� �Ǵ� �����)
    unsigned int address;  // �޸� �ּ�
    int result;            // ��� (������� ����)
    int load_hits = 0, load_misses = 0; // �ε� ��Ʈ �� �̽� ī��Ʈ
    int store_hits = 0, store_misses = 0; // ����� ��Ʈ �� �̽� ī��Ʈ
    unsigned long long total_cycles = 0; // �� ����Ŭ ��
    int total_loads = 0, total_stores = 0; // �� �ε� �� ����� ��

    while (fscanf(file, " %c %x %d", &type, &address, &result) == 3) { // Ʈ���̽� ���Ͽ��� �� �پ� �б�
        if (type == 'l') {         // �ε��� ���
            total_loads++;
        }
        else if (type == 's') {  // ������� ���
            total_stores++;
        }
        access_cache(cache, type, address, &load_hits, &load_misses, &store_hits, &store_misses, &total_cycles); // ĳ�� ����
    }

    fclose(file); // Ʈ���̽� ���� �ݱ�

    // ��� ���
    printf("Total loads: %d\n", total_loads);
    printf("Total stores: %d\n", total_stores);
    printf("Load hits: %d\n", load_hits);
    printf("Load misses: %d\n", load_misses);
    printf("Store hits: %d\n", store_hits);
    printf("Store misses: %d\n", store_misses);
    printf("Total cycles: %llu\n", total_cycles);

    free_cache(cache); // ĳ�� �޸� ����
    return 0;
}

