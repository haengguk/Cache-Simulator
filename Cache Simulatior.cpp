#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 캐시 블록 구조체
typedef struct {
    unsigned int tag;              // 태그 (메모리 주소의 일부)
    int valid;                     // 유효 비트 (이 블록이 유효한지 여부)
    int dirty;                     // 더티 비트 (이 블록이 수정되었는지 여부)
    unsigned long long lru_counter;// LRU 카운터 (사용 빈도 추적)
} CacheBlock;

// 캐시 세트 구조체
typedef struct {
    CacheBlock* blocks;            // 세트 내의 캐시 블록 배열
} CacheSet;

// 캐시 구조체
typedef struct {
    CacheSet* sets;                // 캐시 세트 배열
    int num_sets;                  // 세트의 수
    int blocks_per_set;            // 세트당 블록의 수
    int bytes_per_block;           // 블록당 바이트 수
    int write_allocate;            // write-allocate 여부
    int write_back;                // write-back 여부
    unsigned long long lru_counter;// 전체 LRU 카운터
} Cache;

// 캐시 초기화 함수
Cache* initialize_cache(int num_sets, int blocks_per_set, int bytes_per_block, int write_allocate, int write_back) {
    Cache* cache = (Cache*)malloc(sizeof(Cache));      // 캐시 구조체 할당
    cache->num_sets = num_sets;                        
    cache->blocks_per_set = blocks_per_set;            
    cache->bytes_per_block = bytes_per_block;          
    cache->write_allocate = write_allocate;            
    cache->write_back = write_back;                    
    cache->lru_counter = 0;                            // LRU 카운터 초기화
    cache->sets = (CacheSet*)malloc(num_sets * sizeof(CacheSet)); // 캐시 세트 배열 할당

    for (int i = 0; i < num_sets; i++) {
        cache->sets[i].blocks = (CacheBlock*)malloc(blocks_per_set * sizeof(CacheBlock)); // 각 세트에 대해 캐시 블록 배열 할당
        for (int j = 0; j < blocks_per_set; j++) {
            cache->sets[i].blocks[j].valid = 0;       // 블록의 유효 비트 초기화
            cache->sets[i].blocks[j].dirty = 0;       // 블록의 더티 비트 초기화
            cache->sets[i].blocks[j].lru_counter = 0; // 블록의 LRU 카운터 초기화
        }
    }
    return cache; // 초기화된 캐시 반환
}

// 캐시 메모리 해제 함수
void free_cache(Cache* cache) {
    for (int i = 0; i < cache->num_sets; i++) {
        free(cache->sets[i].blocks); // 각 세트의 블록 메모리 해제
    }
    free(cache->sets); // 세트 배열 메모리 해제
    free(cache); // 캐시 구조체 메모리 해제
}

// 주소에서 태그와 세트 인덱스를 추출하는 함수
void get_tag_and_set(Cache* cache, unsigned int address, unsigned int* tag, unsigned int* set_index) {
    int block_offset_bits = (int)log2(cache->bytes_per_block); // 블록 오프셋 비트 수 계산
    int set_index_bits = (int)log2(cache->num_sets);           // 세트 인덱스 비트 수 계산
    *set_index = (address >> block_offset_bits) & (cache->num_sets - 1); // 세트 인덱스 추출
    *tag = address >> (block_offset_bits + set_index_bits);    // 태그 추출
}

// 캐시에 접근하는 함수
int access_cache(Cache* cache, char type, unsigned int address, int* load_hits, int* load_misses, int* store_hits, int* store_misses, unsigned long long* total_cycles) {
    unsigned int tag, set_index;
    get_tag_and_set(cache, address, &tag, &set_index); // 주소에서 태그와 세트 인덱스 추출
    CacheSet* set = &cache->sets[set_index];           // 해당 세트 참조
    cache->lru_counter++;                              // 전체 LRU 카운터 증가

    // 캐시 블록 탐색
    for (int i = 0; i < cache->blocks_per_set; i++) {
        if (set->blocks[i].valid && set->blocks[i].tag == tag) { // 유효하고 태그가 일치하는 블록을 찾음
            set->blocks[i].lru_counter = cache->lru_counter;     // LRU 카운터 갱신
            if (type == 'l') {
                (*load_hits)++;      // 로드 히트 증가
                *total_cycles += 1;  // 로드 사이클 증가
            }
            else if (type == 's') {
                set->blocks[i].dirty = 1; // 더티 비트 설정
                (*store_hits)++;          // 스토어 히트 증가
                *total_cycles += 1;       // 스토어 사이클 증가
            }
            return 1; // 캐시 히트 반환
        }
    }

    // 미스 처리
    if (type == 'l') {
        (*load_misses)++;                 // 로드 미스 증가
        *total_cycles += 100 * (cache->bytes_per_block / 4); // 메모리 접근 사이클 증가
    }
    else if (type == 's') {
        (*store_misses)++;                // 스토어 미스 증가
        *total_cycles += 100 * (cache->bytes_per_block / 4); // 메모리 접근 사이클 증가
    }

    int lru_index = 0; // LRU 인덱스 초기화
    unsigned long long min_lru = set->blocks[0].lru_counter; // 최소 LRU 값 초기화

    for (int i = 1; i < cache->blocks_per_set; i++) {
        if (!set->blocks[i].valid) { // 유효하지 않은 블록이 있으면 교체 대상으로 선택
            lru_index = i;
            break;
        }
        if (set->blocks[i].lru_counter < min_lru) { // LRU 값이 더 작은 블록을 찾음
            lru_index = i;
            min_lru = set->blocks[i].lru_counter;
        }
    }

    if (set->blocks[lru_index].valid && set->blocks[lru_index].dirty && cache->write_back) { // 교체할 블록이 유효하고 더티 비트가 설정되어 있으며 write-back 모드일 때
        *total_cycles += 100 * (cache->bytes_per_block / 4); // 메모리로 write-back 사이클 증가
    }

    set->blocks[lru_index].valid = 1;          // 교체할 블록을 유효하게 설정
    set->blocks[lru_index].tag = tag;          // 새로운 태그 설정
    set->blocks[lru_index].dirty = (type == 's'); // 스토어일 경우 더티 비트 설정
    set->blocks[lru_index].lru_counter = cache->lru_counter; // LRU 카운터 갱신

    if (type == 's' && !cache->write_allocate) { // write 할당이 아닌 경우
        set->blocks[lru_index].valid = 0; // 블록을 무효화
    }

    return 0; // 캐시 미스 반환
}

int main() {
    int num_sets = 256;           
    int blocks_per_set = 4;       
    int bytes_per_block = 16;     
    int write_allocate = 1;       
    int write_back = 1;           
    const char* tracefile = "gcc.trace"; 

    Cache* cache = initialize_cache(num_sets, blocks_per_set, bytes_per_block, write_allocate, write_back); // 캐시 초기화

    FILE* file = fopen(tracefile, "r"); // 트레이스 파일 열기
    if (file == NULL) {                 // 파일 열기 오류 처리
        fprintf(stderr, "Error: Unable to open file %s\n", tracefile);
        free_cache(cache);              // 캐시 메모리 해제
        return 1;
    }

    char type;             // 접근 타입 (Load 또는 Store)
    unsigned int address;  // 메모리 주소
    int result;            
    int load_hits = 0, load_misses = 0; 
    int store_hits = 0, store_misses = 0; 
    unsigned long long total_cycles = 0; 
    int total_loads = 0, total_stores = 0; 

    while (fscanf(file, " %c %x %d", &type, &address, &result) == 3) { // 트레이스 파일에서 한 줄씩 읽기
        if (type == 'l') {         // 로드일 경우
            total_loads++;
        }
        else if (type == 's') {  // 스토어일 경우
            total_stores++;
        }
        access_cache(cache, type, address, &load_hits, &load_misses, &store_hits, &store_misses, &total_cycles); // 캐시 접근
    }

    fclose(file); // 트레이스 파일 닫기

    printf("Total loads: %d\n", total_loads);
    printf("Total stores: %d\n", total_stores);
    printf("Load hits: %d\n", load_hits);
    printf("Load misses: %d\n", load_misses);
    printf("Store hits: %d\n", store_hits);
    printf("Store misses: %d\n", store_misses);
    printf("Total cycles: %llu\n", total_cycles);

    free_cache(cache); // 캐시 메모리 해제
    return 0;
}

