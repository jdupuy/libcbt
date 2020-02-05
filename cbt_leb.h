#ifndef LEB_INCLUDE_LEB_H
#define LEB_INCLUDE_LEB_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LEB_STATIC
#define LEBDEF static
#else
#define LEBDEF extern
#endif

#include "cbt.h"

typedef cbt_Tree leb_Heap;
typedef cbt_Node leb_Node;
typedef struct {

} leb_NodeAndNeighbors;

typedef struct {
    leb_Node node, edge;
} leb_MergeNode;

// create / destroy LEB
#define leb_Create          cbt_Ctreate
#define leb_CreateAtDepth   cbt_CreateAtDepth
#define leb_Release         cbt_Release

// loaders
#define leb_ResetToRoot     cbt_ResetToRoot
#define leb_ResetToCeil     cbt_ResetToCeil
#define leb_ResetToDepth    cbt_ResetToDepth

// manipulation
LEBDEF void leb_SplitNode2D(leb_Heap *leb, const leb_Node node);
LEBDEF void leb_MergeNode2D(leb_Heap *leb, const leb_Node node);
typedef void (*leb_SplitUpdateCallback)(leb_Heap *leb, const leb_Node node);
CBTDEF void leb_SplitUpdate(leb_Heap *leb, leb_SplitUpdateCallback updater);
typedef void (*leb_MergeUpdateCallback)(leb_Heap *leb, const leb_DiamondParent node);
CBTDEF void leb_MergeUpdate(leb_Heap *leb, leb_MergeUpdateCallback updater);

// subdivision routines
LEBDEF void leb_DecodeNodeAttributeArray2D(const leb_Node node,
                                           int32_t attributeArraySize,
                                           float attributeArray[][3]);

// intersection test O(depth)
LEBDEF bool leb_BoundingNode2D(const leb_Heap *leb, float x, float y);

#ifdef __cplusplus
} // extern "C"
#endif

//
//
//// end header file ///////////////////////////////////////////////////////////
#endif // LEB_INCLUDE_LEB_H

#if 1
//#ifdef LEB_IMPLEMENTATION

#ifndef LEB_ASSERT
#    include <assert.h>
#    define LEB_ASSERT(x) assert(x)
#endif

#ifndef LEB_LOG
#    include <stdio.h>
#    define LEB_LOG(format, ...) do { fprintf(stdout, format, ##__VA_ARGS__); fflush(stdout); } while(0)
#endif

#ifndef LEB_MALLOC
#    include <stdlib.h>
#    define LEB_MALLOC(x) (malloc(x))
#    define LEB_FREE(x) (free(x))
#else
#    ifndef LEB_FREE
#        error LEB_MALLOC defined without LEB_FREE
#    endif
#endif

/*******************************************************************************
 * SplitNode2D -- Splits a node while producing a conforming LEB
 *
 */
LEBDEF void leb_SplitNode2D(leb_Heap *leb, const leb_Node node)
{
    if (!leb_IsCeilNode(leb, node)) {
        const uint32_t minNodeID = 1u << leb->minDepth;
        leb_Node nodeIterator = node;

        leb__SplitNode(leb, nodeIterator);
        nodeIterator = leb__EdgeNode(nodeIterator);

        while (nodeIterator.id >= minNodeID) {
            leb__SplitNode(leb, nodeIterator);
            nodeIterator = leb_ParentNode(nodeIterator);
            leb__SplitNode(leb, nodeIterator);
            nodeIterator = leb__EdgeNode(nodeIterator);
        }
    }
}


/*******************************************************************************
 * MergeNode2D -- Merges a node while producing a conforming LEB
 *
 * This routines makes sure that the children of a diamond (including the
 * input node) all exist in the LEB before calling a merge.
 *
 */
LEBDEF void
leb_MergeNode2D(
    leb_Heap *leb,
    const leb_Node node,
    const leb_DiamondParent diamond
) {
    if (!leb_IsRootNode(leb, node)) {
        leb_Node dualNode = leb__RightChildNode(diamond.top);
        bool b1 = leb_IsLeafNode(leb, leb__SiblingNode_Fast(node));
        bool b2 = leb_IsLeafNode(leb, dualNode);
        bool b3 = leb_IsLeafNode(leb, leb__SiblingNode(dualNode));

        if (b1 && b2 && b3) {
            leb__MergeNode(leb, node);
            leb__MergeNode(leb, dualNode);
        }
    }
}


#endif // LEB_IMPLEMENTATION
