

#define CBT_IMPLEMENTATION
#include "cbt.h"
#undef CBT_IMPLEMENTATION

void update(cbt_Tree *tree, const cbt_Node node)
{
    if ((node.id & 1) == 0 && !cbt_IsCeilNode(tree, node)) {
        cbt_SplitNode_Fast(tree, node);
    }
}

void update2(cbt_Tree *tree, const cbt_Node node)
{
    if ((node.id & 1) == 0) {
        cbt_MergeNode_Fast(tree, node);
    }
}

int main(int argc, const char **argv)
{
    int64_t depth = 12;
    printf("=> %li\n", cbt__HeapByteSize(depth));
    cbt_Tree *tree = cbt_Create(depth);

    cbt_ResetToDepth(tree, 8);
    printf("node_count: %li\n", cbt_NodeCount(tree));
    
    cbt_ResetToDepth(tree, 12);
    printf("node_count: %li\n", cbt_NodeCount(tree));
    
    cbt_Update(tree, update);
    printf("node_count: %li\n", cbt_NodeCount(tree));

    cbt_Update(tree, update2);
    printf("node_count: %li\n", cbt_NodeCount(tree));

    cbt_ResetToDepth(tree, depth);
    printf("node_count: %li (%li)\n", cbt_NodeCount(tree), 1LL << (depth));

    cbt_Release(tree);
}
