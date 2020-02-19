

#define CBT_IMPLEMENTATION
#include "cbt.h"
#undef CBT_IMPLEMENTATION

#define LEB_IMPLEMENTATION
#include "leb.h"
#undef LEB_IMPLEMENTATION

static void update(cbt_Tree *tree, const cbt_Node node, const void *userData)
{
    (void)userData;
    
    if ((node.id & 1) == 0 && !cbt_IsCeilNode(tree, node)) {
        cbt_SplitNode_Fast(tree, node);
    }
}

static void update2(cbt_Tree *tree, const cbt_Node node, const void *userData)
{
    (void)userData;
    
    if ((node.id & 1) == 0) {
        cbt_MergeNode_Fast(tree, node);
    }
}

void PrintBits(uint64_t x)
{
    for (int i = 0; i < 64; ++i)
        printf("%li", (x >> (63 - i)) & 1);
    printf("\n");
}

uint64_t BitCount(uint64_t x)
{
    x = (x & 0x5555555555555555ULL) + ((x >>  1) & 0x5555555555555555ULL);
    PrintBits(x);
    x = (x & 0x3333333333333333ULL) + ((x >>  2) & 0x3333333333333333ULL);
    PrintBits(x);
    x = (x & 0x0F0F0F0F0F0F0F0FULL) + ((x >>  4) & 0x0F0F0F0F0F0F0F0FULL);
    PrintBits(x);
    x = (x & 0x00FF00FF00FF00FFULL) + ((x >>  8) & 0x00FF00FF00FF00FFULL);
    PrintBits(x);
    x = (x & 0x0000FFFF0000FFFFULL) + ((x >> 16) & 0x0000FFFF0000FFFFULL);
    PrintBits(x);
    x = (x & 0x00000000FFFFFFFFULL) + ((x >> 32) & 0x00000000FFFFFFFFULL);
    PrintBits(x); printf("\n");

    return x;
}

uint64_t BitCount2(uint64_t x)
{
    uint64_t y;
    x = (x & 0x5555555555555555ULL) + ((x >>  1) & 0x5555555555555555ULL);
    PrintBits(x); printf("\n");
    x = (x & 0x3333333333333333ULL) + ((x >>  2) & 0x3333333333333333ULL);
    y = ((x >>  0) & (7ULL <<  0))
      | ((x >>  1) & (7ULL <<  3))
      | ((x >>  2) & (7ULL <<  6))
      | ((x >>  3) & (7ULL <<  9))
      | ((x >>  4) & (7ULL << 12))
      | ((x >>  5) & (7ULL << 15))
      | ((x >>  6) & (7ULL << 18))
      | ((x >>  7) & (7ULL << 21))
      | ((x >>  8) & (7ULL << 24))
      | ((x >>  9) & (7ULL << 27))
      | ((x >> 10) & (7ULL << 30))
      | ((x >> 11) & (7ULL << 33))
      | ((x >> 12) & (7ULL << 36))
      | ((x >> 13) & (7ULL << 39))
      | ((x >> 14) & (7ULL << 42))
      | ((x >> 15) & (7ULL << 45));
    PrintBits(x);
    PrintBits(y); printf("\n");
    x = (x & 0x0F0F0F0F0F0F0F0FULL) + ((x >>  4) & 0x0F0F0F0F0F0F0F0FULL);
    y = ((x >>  0) & (15ULL <<  0))
      | ((x >>  4) & (15ULL <<  4))
      | ((x >>  8) & (15ULL <<  8))
      | ((x >> 12) & (15ULL << 12))
      | ((x >> 16) & (15ULL << 16))
      | ((x >> 20) & (15ULL << 20))
      | ((x >> 24) & (15ULL << 24))
      | ((x >> 28) & (15ULL << 28));
    PrintBits(x);
    PrintBits(y); printf("\n");
    x = (x & 0x00FF00FF00FF00FFULL) + ((x >>  8) & 0x00FF00FF00FF00FFULL);
    y = ((x >>  0) & (31ULL <<  0))
      | ((x >> 11) & (31ULL <<  5))
      | ((x >> 22) & (31ULL << 10))
      | ((x >> 33) & (31ULL << 15));
    PrintBits(x);
    PrintBits(y); printf("\n");
    x = (x & 0x0000FFFF0000FFFFULL) + ((x >> 16) & 0x0000FFFF0000FFFFULL);
    y = ((x >>  0) & (63ULL << 0))
      | ((x >> 26) & (63ULL << 6));
    PrintBits(x);
    PrintBits(y); printf("\n");
    x = (x & 0x00000000FFFFFFFFULL) + ((x >> 32) & 0x00000000FFFFFFFFULL);
    PrintBits(x); printf("\n");

    return x;
}

int main()
{
#if 0
    printf("%li\n", BitCount2(0xFFFFFFFFFFFFFFFFULL));
    printf("%li\n", BitCount(0x00000000FFFFFFFFULL));
    printf("%li\n", BitCount(31LL << 32));
#endif

#if 1
    int64_t depth = 32;
    printf("=> %li\n", cbt__HeapByteSize(depth) >> 30);
    cbt_Tree *tree = cbt_Create(depth);

    cbt_ResetToDepth(tree, 8);
    printf("node_count: %li\n", cbt_NodeCount(tree));
    
    cbt_ResetToDepth(tree, 12);
    printf("node_count: %li\n", cbt_NodeCount(tree));
    
    cbt_Update(tree, update, NULL);
    printf("node_count: %li\n", cbt_NodeCount(tree));

    cbt_Update(tree, update2, NULL);
    printf("node_count: %li\n", cbt_NodeCount(tree));

    cbt_ResetToDepth(tree, depth);
    printf("node_count: %li (%li)\n", cbt_NodeCount(tree), 1L << (depth));

    cbt_ResetToDepth(tree, depth / 2);
    printf("node_count: %li (%li)\n", cbt_NodeCount(tree), 1L << (depth / 2));

    cbt_Release(tree);
#endif
}
