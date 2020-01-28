/* cbt.h - public domain library for creating and processing binary trees in parallel
by Jonathan Dupuy

   INTERFACING
   define CBT_ASSERT(x) to avoid using assert.h.
   define CBT_LOG(format, ...) to use your own logger (default prints to stdout)
   define CBT_MALLOC(x) to use your own memory allocator
   define CBT_FREE(x) to use your own memory deallocator
*/

#ifndef CBT_INCLUDE_CBT_H
#define CBT_INCLUDE_CBT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CBT_STATIC
#define CBTDEF static
#else
#define CBTDEF extern
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct CBT_Tree CBT_Tree;
typedef struct {
    uint32_t id;
    int32_t depth;
} CBT_Node;

// create / destroy tree
CBTDEF CBT_Tree *CBT_Create(int32_t maxDepth);
CBTDEF CBT_Tree *CBT_CreateMinMax(int32_t minDepth,
                                  int32_t maxDepth);
CBTDEF CBT_Tree *CBT_CreateMinMaxDepth(int32_t minDepth,
                                       int32_t maxDepth,
                                       int32_t depth);
CBTDEF void CBT_Release(CBT_Tree *tree);

// loaders
CBTDEF void CBT_ResetToMinDepth(CBT_Tree *tree);
CBTDEF void CBT_ResetToMaxDepth(CBT_Tree *tree);
CBTDEF void CBT_ResetToDepth(CBT_Tree *tree,
                             int32_t depth);

// manipulation
CBTDEF void CBT_SplitNode_Fast(CBT_Tree *tree,
                               const CBT_Node node);
CBTDEF void CBT_SplitNode(CBT_Tree *tree,
                          const CBT_Node node);
CBTDEF void CBT_MergeNode_Fast(CBT_Tree *tree,
                               const CBT_Node node);
CBTDEF void CBT_MergeNode(CBT_Tree *tree,
                          const CBT_Node node);
typedef void (*CBT_UpdateCallback)(CBT_Tree *tree, const CBT_Node node);
CBTDEF void CBT_Update(CBT_Tree *tree, CBT_UpdateCallback updater);

// O(1) queries
CBTDEF int32_t CBT_MinDepth(const CBT_Tree *tree);
CBTDEF int32_t CBT_MaxDepth(const CBT_Tree *tree);
CBTDEF int32_t CBT_NodeCount(const CBT_Tree *tree);
CBTDEF bool CBT_IsLeafNode(const CBT_Tree *tree, const CBT_Node node);
CBTDEF bool CBT_IsRootNode(const CBT_Tree *tree, const CBT_Node node);
CBTDEF bool CBT_IsCeilNode(const CBT_Tree *tree, const CBT_Node node);
CBTDEF bool CBT_IsNullNode(                      const CBT_Node node);

// node constructors
CBTDEF CBT_Node CBT_ParentNode(const CBT_Node node);
CBTDEF CBT_Node CBT_SiblingNode(const CBT_Node node);
CBTDEF CBT_Node CBT_LeftSiblingNode(const CBT_Node node);
CBTDEF CBT_Node CBT_RightSiblingNode(const CBT_Node node);
CBTDEF CBT_Node CBT_LeftChildNode(const CBT_Node node);
CBTDEF CBT_Node CBT_RightChildNode(const CBT_Node node);

// O(depth) queries
CBTDEF CBT_Node CBT_DecodeNode(const CBT_Tree *tree, int32_t handle);
CBTDEF int32_t CBT_EncodeNode(const CBT_Tree *tree, const CBT_Node node);

// serialization
CBTDEF int32_t CBT_HeapByteSize(const CBT_Tree *tree);
CBTDEF const char *CBT_GetHeap(const CBT_Tree *tree);
CBTDEF void CBT_SetHeap(CBT_Tree *tree, const char *heap);


#ifdef __cplusplus
} // extern "C"
#endif

//
//
//// end header file ///////////////////////////////////////////////////////////
#endif // CBT_INCLUDE_CBT_H


#if 1
//#ifdef CBT_IMPLEMENTATION

#ifndef CBT_ASSERT
#    include <assert.h>
#    define CBT_ASSERT(x) assert(x)
#endif

#ifndef CBT_LOG
#    include <stdio.h>
#    define CBT_LOG(format, ...) do { fprintf(stdout, format, ##__VA_ARGS__); fflush(stdout); } while(0)
#endif

#ifndef CBT_MALLOC
#    include <stdlib.h>
#    define CBT_MALLOC(x) (malloc(x))
#    define CBT_FREE(x) (free(x))
#else
#    ifndef CBT_FREE
#        error CBT_MALLOC defined without CBT_FREE
#    endif
#endif

#ifndef CBT_MEMSET
#    include <string.h>
#    define CBT_MEMSET(ptr, value, num) memset(ptr, value, num)
#endif

/*******************************************************************************
 * MinValue -- Returns the minimum value between two inputs
 *
 */
static inline uint32_t CBT__MinValue(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}


/*******************************************************************************
 * SetBitValue -- Sets the value of a bit stored in a bitfield
 *
 */
static void
CBT__SetBitValue(uint32_t *bitField, int32_t bitID, uint32_t bitValue)
{
    const uint32_t bitMask = ~(1u << bitID);

#if 0
    (*bitField) = (*bitField & bitMask) | (bitValue << bitID);
#else
#pragma omp atomic
    (*bitField)&= bitMask;
#pragma omp atomic
    (*bitField)|= (bitValue << bitID);
#endif
}


/*******************************************************************************
 * BitfieldInsert -- Inserts data in range [offset, offset + count - 1]
 *
 */
static inline void
CBT__BitFieldInsert(
    uint32_t *bitField,
    int32_t  bitOffset,
    int32_t  bitCount,
    uint32_t bitData
) {
    CBT_ASSERT(bitOffset < 32 && bitCount <= 32 && bitOffset + bitCount <= 32);
    uint32_t bitMask = ~(~(0xFFFFFFFFu << bitCount) << bitOffset);
#if 0
    (*bitField) = (*bitField & bitMask) | (bitData << bitOffset);
#else
#pragma omp atomic
    (*bitField)&= bitMask;
#pragma omp atomic
    (*bitField)|= (bitData << bitOffset);
#endif
}


/*******************************************************************************
 * BitFieldExtract -- Extracts bits [bitOffset, bitOffset + bitCount - 1] from
 * a bitfield, returning them in the least significant bits of the result.
 *
 */
static inline uint32_t
CBT__BitFieldExtract(
    const uint32_t bitField,
    int32_t bitOffset,
    int32_t bitCount
) {
    CBT_ASSERT(bitOffset < 32 && bitCount < 32 && bitOffset + bitCount <= 32);
    uint32_t bitMask = ~(0xFFFFFFFFu << bitCount);

    return (bitField >> bitOffset) & bitMask;
}


/*******************************************************************************
 * Parallel Binary Tree Data-Structure
 *
 */
struct CBT_Tree {
    uint32_t *heap;
    int32_t minDepth,
            maxDepth;
};


/*******************************************************************************
 * CreateNode -- Constructor for the Node data structure
 *
 */
CBT_Node CBT__CreateNode(uint32_t id, int32_t depth)
{
    CBT_Node node;

    node.id = id;
    node.depth = depth;

    return node;
}


/*******************************************************************************
 * IsCeilNode -- Checks if a node is a ceil node, i.e., that can not split further
 *
 */
CBTDEF bool CBT_IsCeilNode(const CBT_Tree *tree, const CBT_Node node)
{
    return (node.depth == tree->maxDepth);
}


/*******************************************************************************
 * IsRootNode -- Checks if a node is a root node
 *
 */
CBTDEF bool CBT_IsRootNode(const CBT_Tree *tree, const CBT_Node node)
{
    return (node.depth == tree->minDepth);
}


/*******************************************************************************
 * IsNullNode -- Checks if a node is a null node
 *
 */
CBTDEF bool CBT_IsNullNode(const CBT_Node node)
{
    return (node.id == 0);
}


/*******************************************************************************
 * ParentNode -- Computes the parent of the input node
 *
 */
static CBT_Node CBT__ParentNode_Fast(const CBT_Node node)
{
    return CBT__CreateNode(node.id >> 1u, node.depth - 1);
}

CBTDEF CBT_Node CBT_ParentNode(const CBT_Node node)
{
     return CBT_IsNullNode(node) ? node : CBT__ParentNode_Fast(node);
}


/*******************************************************************************
 * CeilNode -- Returns the associated ceil node, i.e., the deepest possible leaf
 *
 */
static CBT_Node CBT__CeilNode_Fast(const CBT_Tree *tree, const CBT_Node node)
{
    return CBT__CreateNode(node.id << (tree->maxDepth - node.depth),
                           tree->maxDepth);
}

static CBT_Node CBT__CeilNode(const CBT_Tree *tree, const CBT_Node node)
{
    return CBT_IsNullNode(node) ? node : CBT__CeilNode_Fast(tree, node);
}


/*******************************************************************************
 * SiblingNode -- Computes the sibling of the input node
 *
 */
static CBT_Node CBT__SiblingNode_Fast(const CBT_Node node)
{
    return CBT__CreateNode(node.id ^ 1u, node.depth);
}

CBTDEF CBT_Node CBT_SiblingNode(const CBT_Node node)
{
    return CBT_IsNullNode(node) ? node : CBT__SiblingNode_Fast(node);
}


/*******************************************************************************
 * RightSiblingNode -- Computes the right sibling of the input node
 *
 */
static CBT_Node CBT__RightSiblingNode_Fast(const CBT_Node node)
{
    return CBT__CreateNode(node.id | 1u, node.depth);
}

CBTDEF CBT_Node CBT_RightSiblingNode(const CBT_Node node)
{
    return CBT_IsNullNode(node) ? node : CBT__RightSiblingNode_Fast(node);
}


/*******************************************************************************
 * LeftSiblingNode -- Computes the left sibling of the input node
 *
 */
static CBT_Node CBT__LeftSiblingNode_Fast(const CBT_Node node)
{
    return CBT__CreateNode(node.id & (~1u), node.depth);
}

CBTDEF CBT_Node CBT_LeftSiblingNode(const CBT_Node node)
{
    return CBT_IsNullNode(node) ? node : CBT__LeftSiblingNode_Fast(node);
}


/*******************************************************************************
 * RightChildNode -- Computes the right child of the input node
 *
 */
static CBT_Node CBT__RightChildNode_Fast(const CBT_Node node)
{
    return CBT__CreateNode(node.id << 1u | 1u, node.depth + 1);
}

CBTDEF CBT_Node CBT_RightChildNode(const CBT_Node node)
{
    return CBT_IsNullNode(node) ? node : CBT__RightChildNode_Fast(node);
}


/*******************************************************************************
 * LeftChildNode -- Computes the left child of the input node
 *
 */
static CBT_Node CBT__LeftChildNode_Fast(const CBT_Node node)
{
    return CBT__CreateNode(node.id << 1u, node.depth + 1);
}

CBTDEF CBT_Node CBT_LeftChildNode(const CBT_Node node)
{
    return CBT_IsNullNode(node) ? node : CBT__LeftChildNode_Fast(node);
}


/*******************************************************************************
 * HeapBitSize -- Computes the number of bits to allocate for the buffer
 *
 * For a tree of max depth D, the number of bits is 2^(D+2).
 * Note that 2 bits are "wasted" in the sense that they only serve
 * to round the required number of bits to a power of two.
 *
 */
static inline int32_t CBT__HeapBitSize(int32_t treeMaxDepth)
{
    return 1u << (treeMaxDepth + 2u);
}


/*******************************************************************************
 * HeapUint32Size -- Computes the number of uints to allocate for the bitfield
 *
 */
static inline int32_t CBT__HeapUint32Size(uint32_t treeMaxDepth)
{
    return CBT__HeapBitSize(treeMaxDepth) >> 5u;
}


/*******************************************************************************
 * HeapByteSize -- Computes the number of Bytes to allocate for the bitfield
 *
 */
static int32_t CBT__HeapByteSize(uint32_t treeMaxDepth)
{
    return CBT__HeapUint32Size(treeMaxDepth) * sizeof(uint32_t);
}


/*******************************************************************************
 * NodeBitID -- Returns the bit index that stores data associated with a given node
 *
 * For a tree of max depth D and given an index in [0, 2^(D+1) - 1], this
 * functions is used to emulate the behaviour of a lookup in an array, i.e.,
 * uint32[nodeID]. It provides the first bit in memory that stores
 * information associated with the element of index nodeID.
 *
 * For data located at level d, the bit offset is 2^d x (3 - d + D)
 * We then offset this quantity by the index by (nodeID - 2^d) x (D + 1 - d)
 * Note that the null index (nodeID = 0) is also supported.
 *
 */
static inline uint32_t CBT__NodeBitID(const CBT_Tree *tree, const CBT_Node node)
{
    uint32_t tmp1 = 2u << node.depth;
    uint32_t tmp2 = 1u + (uint32_t)(tree->maxDepth - node.depth);

    return tmp1 + node.id * tmp2;
}


/*******************************************************************************
 * NodeBitID_BitField -- Computes the bitfield bit location associated to a node
 *
 * Here, the node is converted into a final node and its bit offset is
 * returned, which is finalNodeID + 2^{D + 1}
 */
static uint32_t
CBT__NodeBitID_BitField(const CBT_Tree *tree, const CBT_Node node)
{
    return CBT__NodeBitID(tree, CBT__CeilNode(tree, node));
}


/*******************************************************************************
 * NodeBitSize -- Returns the number of bits storing the input node value
 *
 */
static inline
int32_t CBT__NodeBitSize(const CBT_Tree *tree, const CBT_Node node)
{
    return tree->maxDepth - node.depth + 1;
}


/*******************************************************************************
 * HeapArgs
 *
 * The LEB heap data structure uses an array of 32-bit words to store its data.
 * Whenever we need to access a certain bit range, we need to query two such
 * words (because sometimes the requested bit range overlaps two 32-bit words).
 * The HeapArg data structure provides arguments for reading from and/or
 * writing to the two 32-bit words that bound the queries range.
 *
 */
typedef struct {
    uint32_t *bitFieldLSB, *bitFieldMSB;
    uint32_t bitOffsetLSB;
    uint32_t bitCountLSB, bitCountMSB;
} CBT__HeapArgs;

CBT__HeapArgs
CBT__CreateHeapArgs(const CBT_Tree *tree, const CBT_Node node, int32_t bitCount)
{
    uint32_t alignedBitOffset = CBT__NodeBitID(tree, node);
    uint32_t maxBufferIndex = CBT__HeapUint32Size(tree->maxDepth) - 1u;
    uint32_t bufferIndexLSB = (alignedBitOffset >> 5u);
    uint32_t bufferIndexMSB = CBT__MinValue(bufferIndexLSB + 1, maxBufferIndex);
    CBT__HeapArgs args;

    args.bitOffsetLSB = alignedBitOffset & 31u;
    args.bitCountLSB = CBT__MinValue(32u - args.bitOffsetLSB, bitCount);
    args.bitCountMSB = bitCount - args.bitCountLSB;
    args.bitFieldLSB = &tree->heap[bufferIndexLSB];
    args.bitFieldMSB = &tree->heap[bufferIndexMSB];

    return args;
}


/*******************************************************************************
 * HeapWrite -- Sets bitCount bits located at nodeID to bitData
 *
 * Note that this procedure writes to at most two uint32 elements.
 * Two elements are relevant whenever the specified interval overflows 32-bit
 * words.
 *
 */
static void
CBT__HeapWriteExplicit(
    CBT_Tree *tree,
    const CBT_Node node,
    int32_t bitCount,
    uint32_t bitData
) {
    CBT__HeapArgs args = CBT__CreateHeapArgs(tree, node, bitCount);

    CBT__BitFieldInsert(args.bitFieldLSB,
                        args.bitOffsetLSB,
                        args.bitCountLSB,
                        bitData);
    CBT__BitFieldInsert(args.bitFieldMSB,
                        0u,
                        args.bitCountMSB,
                        bitData >> args.bitCountLSB);
}

static void CBT__HeapWrite(CBT_Tree *tree, const CBT_Node node, uint32_t bitData)
{
    CBT__HeapWriteExplicit(tree, node, CBT__NodeBitSize(tree, node), bitData);
}


/*******************************************************************************
 * HeapRead -- Returns bitCount bits located at nodeID
 *
 * Note that this procedure writes to at most two uint32 elements.
 * Two elements are relevant whenever the specified interval overflows 32-bit
 * words.
 *
 */
static uint32_t
CBT__HeapReadExplicit(
    const CBT_Tree *tree,
    const CBT_Node node,
    int32_t bitCount
) {
    CBT__HeapArgs args = CBT__CreateHeapArgs(tree, node, bitCount);
    uint32_t lsb = CBT__BitFieldExtract(*args.bitFieldLSB,
                                        args.bitOffsetLSB,
                                        args.bitCountLSB);
    uint32_t msb = CBT__BitFieldExtract(*args.bitFieldMSB,
                                        0u,
                                        args.bitCountMSB);

    return (lsb | (msb << args.bitCountLSB));
}

static uint32_t CBT__HeapRead(const CBT_Tree *tree, const CBT_Node node)
{
    return CBT__HeapReadExplicit(tree, node, CBT__NodeBitSize(tree, node));
}


/*******************************************************************************
 * HeapWrite_BitField -- Sets the bit associated to a leaf node to bitValue
 *
 * This is a dedicated routine to write directly to the bitfield.
 *
 */
static void
CBT__HeapWrite_BitField(
    CBT_Tree *tree,
    const CBT_Node node,
    const uint32_t bitValue
) {
    uint32_t bitID = CBT__NodeBitID_BitField(tree, node);

    CBT__SetBitValue(&tree->heap[bitID >> 5u], bitID & 31u, bitValue);
}


/*******************************************************************************
 * IsLeafNode -- Checks if a node is a leaf node, i.e., that has no descendants
 *
 */
CBTDEF bool CBT_IsLeafNode(const CBT_Tree *tree, const CBT_Node node)
{
    return (CBT__HeapRead(tree, node) == 1u);
}


/*******************************************************************************
 * ClearData -- Resets the data buffer stored by a LEB
 *
 */
static void CBT__ClearBuffer(CBT_Tree *tree)
{
    CBT_MEMSET(tree->heap, 0, CBT__HeapByteSize(tree->maxDepth));
}


/*******************************************************************************
 * GetHeapMemory -- Returns a read-only pointer to the heap memory
 *
 */
CBTDEF const char *CBT_GetHeapMemory(const CBT_Tree *tree)
{
    return (char *)tree->heap;
}


/*******************************************************************************
 * SetHeapMemory -- Sets the heap memory from a read-only buffer
 *
 */
CBTDEF void CBT_SetHeapMemory(CBT_Tree *tree, const char *buffer)
{
    memcpy(tree->heap, buffer, CBT_HeapByteSize(tree));
}


/*******************************************************************************
 * HeapByteSize -- Returns the amount of bytes consumed by the LEB heap
 *
 */
CBTDEF int32_t CBT_HeapByteSize(const CBT_Tree *tree)
{
    return CBT__HeapByteSize(tree->maxDepth);
}


/*******************************************************************************
 * UpdateBuffer -- Sums the 2 elements below the current slot
 *
 */
static void CBT__ComputeSumReduction(CBT_Tree *tree)
{
    int32_t depth = tree->maxDepth;
    uint32_t minNodeID = (1u << depth);
    uint32_t maxNodeID = (2u << depth);

    // prepass: processes deepest levels in parallel
#pragma omp parallel for
    for (uint32_t nodeID = minNodeID; nodeID < maxNodeID; nodeID+= 32u) {
        uint32_t alignedBitOffset = CBT__NodeBitID(tree,
                                                   CBT__CreateNode(nodeID, depth));
        uint32_t bitField = tree->heap[alignedBitOffset >> 5u];
        uint32_t bitData = 0u;

        // 2-bits
        bitField = (bitField & 0x55555555u) + ((bitField >> 1u) & 0x55555555u);
        bitData = bitField;
        tree->heap[(alignedBitOffset - minNodeID) >> 5u] = bitData;

        // 3-bits
        bitField = (bitField & 0x33333333u) + ((bitField >>  2u) & 0x33333333u);
        bitData = ((bitField >> 0u) & (7u <<  0u))
                | ((bitField >> 1u) & (7u <<  3u))
                | ((bitField >> 2u) & (7u <<  6u))
                | ((bitField >> 3u) & (7u <<  9u))
                | ((bitField >> 4u) & (7u << 12u))
                | ((bitField >> 5u) & (7u << 15u))
                | ((bitField >> 6u) & (7u << 18u))
                | ((bitField >> 7u) & (7u << 21u));
        CBT__HeapWriteExplicit(tree, CBT__CreateNode(nodeID >> 2u, depth - 2), 24u, bitData);

        // 4-bits
        bitField = (bitField & 0x0F0F0F0Fu) + ((bitField >>  4u) & 0x0F0F0F0Fu);
        bitData = ((bitField >>  0u) & (15u <<  0u))
                | ((bitField >>  4u) & (15u <<  4u))
                | ((bitField >>  8u) & (15u <<  8u))
                | ((bitField >> 12u) & (15u << 12u));
        CBT__HeapWriteExplicit(tree, CBT__CreateNode(nodeID >> 3u, depth - 3), 16u, bitData);

        // 5-bits
        bitField = (bitField & 0x00FF00FFu) + ((bitField >>  8u) & 0x00FF00FFu);
        bitData = ((bitField >>  0u) & (31u << 0u))
                | ((bitField >> 11u) & (31u << 5u));
        CBT__HeapWriteExplicit(tree, CBT__CreateNode(nodeID >> 4u, depth - 4), 10u, bitData);

        // 6-bits
        bitField = (bitField & 0x0000FFFFu) + ((bitField >> 16u) & 0x0000FFFFu);
        bitData = bitField;
        CBT__HeapWriteExplicit(tree, CBT__CreateNode(nodeID >> 5u, depth - 5),  6u, bitData);
    }
#pragma omp barrier
    depth-= 5;

    // iterate over elements atomically
    while (--depth >= 0) {
        uint32_t minNodeID = 1u << depth;
        uint32_t maxNodeID = 2u << depth;

#pragma omp parallel for
        for (uint32_t j = minNodeID; j < maxNodeID; ++j) {
            uint32_t x0 = CBT__HeapRead(tree, CBT__CreateNode(j << 1u     , depth + 1));
            uint32_t x1 = CBT__HeapRead(tree, CBT__CreateNode(j << 1u | 1u, depth + 1));

            CBT__HeapWrite(tree, CBT__CreateNode(j, depth), x0 + x1);
        }
#pragma omp barrier
    }
}


/*******************************************************************************
 * Buffer Ctor
 *
 */
CBTDEF CBT_Tree *CBT_CreateMinMax(int minDepth, int maxDepth)
{
    CBT_ASSERT(maxDepth >=  5 && "maxDepth must be at least 5");
    CBT_ASSERT(maxDepth <= 28 && "maxDepth must be at most 29");
    CBT_ASSERT(minDepth >=  0 && "minDepth must be at least 0");
    CBT_ASSERT(minDepth <= maxDepth && "minDepth must be less than maxDepth");
    CBT_Tree *tree = (CBT_Tree *)CBT_MALLOC(sizeof(*tree));

    tree->minDepth = minDepth;
    tree->maxDepth = maxDepth;
    tree->heap = (uint32_t *)CBT_MALLOC(CBT__HeapByteSize(maxDepth));
    CBT_ASSERT(tree->heap != NULL && "Memory allocation failed");
    CBT_ResetToMinDepth(tree);

    return tree;
}

CBTDEF CBT_Tree *CBT_Create(int maxDepth)
{
    return CBT_CreateMinMax(0, maxDepth);
}


/*******************************************************************************
 * Buffer Dtor
 *
 */
CBTDEF void CBT_Release(CBT_Tree *tree)
{
    CBT_FREE(tree->heap);
    CBT_FREE(tree);
}


/*******************************************************************************
 * ResetToDepth -- Initializes a LEB to its a specific subdivision level
 *
 */
CBTDEF void CBT_ResetToDepth(CBT_Tree *tree, int32_t depth)
{
    CBT_ASSERT(depth >= tree->minDepth && "depth must be at least equal to minDepth");
    CBT_ASSERT(depth <= tree->maxDepth && "depth must be at most equal to maxDepth");
    uint32_t minNodeID = 1u << depth;
    uint32_t maxNodeID = 2u << depth;

    CBT__ClearBuffer(tree);

    for (uint32_t nodeID = minNodeID; nodeID < maxNodeID; ++nodeID) {
        CBT_Node node = CBT__CreateNode(nodeID, depth);

        CBT__HeapWrite_BitField(tree, node, 1u);
    }

    CBT__ComputeSumReduction(tree);
}


/*******************************************************************************
 * ResetToMinDepth -- Initializes a LEB to its minimum subdivision level
 *
 */
CBTDEF void CBT_ResetToMinDepth(CBT_Tree *tree)
{
    CBT_ResetToDepth(tree, tree->minDepth);
}


/*******************************************************************************
 * ResetToMaxDepth -- Initializes a LEB to its maximum subdivision level
 *
 */
CBTDEF void CBT_ResetToMaxDepth(CBT_Tree *tree)
{
    CBT_ResetToDepth(tree, tree->maxDepth);
}


/*******************************************************************************
 * Split -- Subdivides a node in two
 *
 * The _Fast version does not check if the node can actually split, so
 * use it wisely, i.e., when you're absolutely sure the node depth is
 * less than maxDepth.
 *
 */
CBTDEF void CBT_SplitNode_Fast(CBT_Tree *tree, const CBT_Node node)
{
    CBT__HeapWrite_BitField(tree, CBT_RightChildNode(node), 1u);
}

CBTDEF void CBT_SplitNode(CBT_Tree *tree, const CBT_Node node)
{
    if (!CBT_IsCeilNode(tree, node))
        CBT_SplitNode_Fast(tree, node);
}


/*******************************************************************************
 * Merge -- Merges the node with its neighbour
 *
 * The _Fast version does not check if the node can actually merge, so
 * use it wisely, i.e., when you're absolutely sure the node depth is
 * greater than minDepth.
 *
 */
CBTDEF void CBT_MergeNode_Fast(CBT_Tree *tree, const CBT_Node node)
{
    CBT__HeapWrite_BitField(tree, CBT_RightSiblingNode(node), 0u);
}

CBTDEF void CBT_MergeNode(CBT_Tree *tree, const CBT_Node node)
{
    if (!CBT_IsRootNode(tree, node))
        CBT_MergeNode_Fast(tree, node);
}


/*******************************************************************************
 * MinDepth -- Returns the min LEB depth
 *
 */
CBTDEF int32_t CBT_MinDepth(const CBT_Tree *tree)
{
    return tree->minDepth;
}


/*******************************************************************************
 * MaxDepth -- Returns the max LEB depth
 *
 */
CBTDEF int32_t CBT_MaxDepth(const CBT_Tree *tree)
{
    return tree->maxDepth;
}


/*******************************************************************************
 * NodeCount -- Returns the number of triangles in the LEB
 *
 */
CBTDEF int32_t CBT_NodeCount(const CBT_Tree *tree)
{
    return CBT__HeapRead(tree, CBT__CreateNode(1u, 0));
}


/*******************************************************************************
 * DecodeNode -- Returns the leaf node associated to index nodeID
 *
 * This is procedure is for iterating over the nodes.
 *
 */
CBTDEF CBT_Node CBT_DecodeNode(const CBT_Tree *tree, int32_t handle)
{
    CBT_ASSERT(handle < CBT_NodeCount(tree) && "handle > NodeCount");
    CBT_ASSERT(handle >= 0 && "handle < 0");

    CBT_Node node = CBT__CreateNode(1u, 0);

    while (CBT__HeapRead(tree, node) > 1u) {
        uint32_t cmp = CBT__HeapRead(tree, CBT__CreateNode(node.id<<= 1u, ++node.depth));
        uint32_t b = handle < cmp ? 0 : 1;

        node.id|= b;
        handle-= cmp * b;
    }

    return node;
}


/*******************************************************************************
 * EncodeNode -- Returns the bit index associated with the Node
 *
 * This does the inverse of the DecodeNode routine.
 *
 */
CBTDEF int32_t CBT_EncodeNode(const CBT_Tree *tree, const CBT_Node node)
{
    CBT_ASSERT(CBT_IsLeafNode(tree, node) && "node is not a leaf");

    int32_t handle = 0u;
    CBT_Node nodeIterator = node;

    while (nodeIterator.id > 1u) {
        CBT_Node sibling = CBT__LeftSiblingNode_Fast(nodeIterator);
        uint32_t nodeCount = CBT__HeapRead(tree, CBT__CreateNode(sibling.id, sibling.depth));

        handle+= (nodeIterator.id & 1u) * nodeCount;
        nodeIterator = CBT__ParentNode_Fast(nodeIterator);
    }

    return handle;
}


/*******************************************************************************
 * Update -- Split or merge each node in parallel
 *
 * The user provides an updater function that is responsible for
 * splitting or merging each node.
 *
 */
CBTDEF void CBT_Update(CBT_Tree *tree, CBT_UpdateCallback updater)
{
#pragma omp parallel for
    for (int64_t handle = 0; handle < CBT_NodeCount(tree); ++handle) {
        updater(tree, CBT_DecodeNode(tree, handle));
    }
#pragma omp barrier

    CBT__ComputeSumReduction(tree);
}


#endif



