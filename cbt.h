/* cbt.h - public domain library for creating and processing binary trees in parallel
by Jonathan Dupuy

   INTERFACING
   define CBT_LOG(format, ...) to use your own logger (default prints to stdout)
   define CBT_ASSERT(x) to avoid using assert.h
   define CBT_MALLOC(x) to use your own memory allocator
   define CBT_FREE(x) to use your own memory deallocator
   define CBT_MEMSET(ptr, value, num) to use your own memset routine
   define CBT_MEMCPY(dst, src, num) to use your own memcpy routine
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

typedef struct cbt_Tree cbt_Tree;
typedef struct {
    uint32_t id;
    int32_t depth;
} cbt_Node;

// create / destroy tree
CBTDEF cbt_Tree *cbt_Create(int32_t maxDepth);
CBTDEF cbt_Tree *cbt_CreateAtDepth(int32_t maxDepth, int32_t depth);
CBTDEF void cbt_Release(cbt_Tree *tree);

// loaders
CBTDEF void cbt_ResetToMaxDepth(cbt_Tree *tree);
CBTDEF void cbt_ResetToDepth(cbt_Tree *tree, int32_t depth);

// manipulation
CBTDEF void cbt_SplitNode_Fast(cbt_Tree *tree, const cbt_Node node);
CBTDEF void cbt_SplitNode     (cbt_Tree *tree, const cbt_Node node);
CBTDEF void cbt_MergeNode_Fast(cbt_Tree *tree, const cbt_Node node);
CBTDEF void cbt_MergeNode     (cbt_Tree *tree, const cbt_Node node);
typedef void (*cbt_UpdateCallback)(cbt_Tree *tree, const cbt_Node node);
CBTDEF void cbt_Update(cbt_Tree *tree, cbt_UpdateCallback updater);

// O(1) queries
CBTDEF int32_t cbt_MaxDepth(const cbt_Tree *tree);
CBTDEF int32_t cbt_NodeCount(const cbt_Tree *tree);
CBTDEF bool cbt_IsLeafNode(const cbt_Tree *tree, const cbt_Node node);
CBTDEF bool cbt_IsRootNode(const cbt_Tree *tree, const cbt_Node node);
CBTDEF bool cbt_IsCeilNode(const cbt_Tree *tree, const cbt_Node node);
CBTDEF bool cbt_IsNullNode(                      const cbt_Node node);

// node constructors
CBTDEF cbt_Node cbt_ParentNode(const cbt_Node node);
CBTDEF cbt_Node cbt_SiblingNode(const cbt_Node node);
CBTDEF cbt_Node cbt_LeftSiblingNode(const cbt_Node node);
CBTDEF cbt_Node cbt_RightSiblingNode(const cbt_Node node);
CBTDEF cbt_Node cbt_LeftChildNode(const cbt_Node node);
CBTDEF cbt_Node cbt_RightChildNode(const cbt_Node node);

// O(depth) queries
CBTDEF cbt_Node cbt_DecodeNode(const cbt_Tree *tree, int32_t handle);
CBTDEF int32_t cbt_EncodeNode(const cbt_Tree *tree, const cbt_Node node);

// serialization
CBTDEF int32_t cbt_HeapByteSize(const cbt_Tree *tree);
CBTDEF const char *cbt_GetHeap(const cbt_Tree *tree);
CBTDEF void cbt_SetHeap(cbt_Tree *tree, const char *heap);


#ifdef __cplusplus
} // extern "C"
#endif

//
//
//// end header file ///////////////////////////////////////////////////////////
#endif // CBT_INCLUDE_CBT_H

#ifdef CBT_IMPLEMENTATION

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

#ifndef CBT_MEMCPY
#    include <string.h>
#    define CBT_MEMCPY(dst, src, num) memcpy(dst, src, num)
#endif

#ifndef _OPENMP
#   define CBT_ATOMIC
#   define CBT_PARALLEL_FOR
#   define CBT_BARRIER
#else
#   define CBT_ATOMIC          _Pragma("omp atomic" )
#   define CBT_PARALLEL_FOR    _Pragma("omp parallel for")
#   define CBT_BARRIER         _Pragma("omp barrier")
#endif

/*******************************************************************************
 * MinValue -- Returns the minimum value between two inputs
 *
 */
static inline uint32_t cbt__MinValue(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}


/*******************************************************************************
 * SetBitValue -- Sets the value of a bit stored in a bitfield
 *
 */
static void
cbt__SetBitValue(uint32_t *bitField, int32_t bitID, uint32_t bitValue)
{
    const uint32_t bitMask = ~(1u << bitID);

CBT_ATOMIC
    (*bitField)&= bitMask;
CBT_ATOMIC
    (*bitField)|= (bitValue << bitID);
}


/*******************************************************************************
 * BitfieldInsert -- Inserts data in range [offset, offset + count - 1]
 *
 */
static inline void
cbt__BitFieldInsert(
    uint32_t *bitField,
    int32_t  bitOffset,
    int32_t  bitCount,
    uint32_t bitData
) {
    CBT_ASSERT(bitOffset < 32 && bitCount <= 32 && bitOffset + bitCount <= 32);
    uint32_t bitMask = ~(~(0xFFFFFFFFu << bitCount) << bitOffset);
CBT_ATOMIC
    (*bitField)&= bitMask;
CBT_ATOMIC
    (*bitField)|= (bitData << bitOffset);
}


/*******************************************************************************
 * BitFieldExtract -- Extracts bits [bitOffset, bitOffset + bitCount - 1] from
 * a bitfield, returning them in the least significant bits of the result.
 *
 */
static inline uint32_t
cbt__BitFieldExtract(
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
struct cbt_Tree {
    uint32_t *heap;
    int32_t maxDepth;
};


/*******************************************************************************
 * CreateNode -- Constructor for the Node data structure
 *
 */
cbt_Node cbt__CreateNode(uint32_t id, int32_t depth)
{
    cbt_Node node;

    node.id = id;
    node.depth = depth;

    return node;
}


/*******************************************************************************
 * IsCeilNode -- Checks if a node is a ceil node, i.e., that can not split further
 *
 */
CBTDEF bool cbt_IsCeilNode(const cbt_Tree *tree, const cbt_Node node)
{
    return (node.depth == tree->maxDepth);
}


/*******************************************************************************
 * IsRootNode -- Checks if a node is a root node
 *
 */
CBTDEF bool cbt_IsRootNode(const cbt_Tree *tree, const cbt_Node node)
{
    return (node.id == 1u);
}


/*******************************************************************************
 * IsNullNode -- Checks if a node is a null node
 *
 */
CBTDEF bool cbt_IsNullNode(const cbt_Node node)
{
    return (node.id == 0u);
}


/*******************************************************************************
 * ParentNode -- Computes the parent of the input node
 *
 */
static cbt_Node cbt__ParentNode_Fast(const cbt_Node node)
{
    return cbt__CreateNode(node.id >> 1u, node.depth - 1);
}

CBTDEF cbt_Node cbt_ParentNode(const cbt_Node node)
{
     return cbt_IsNullNode(node) ? node : cbt__ParentNode_Fast(node);
}


/*******************************************************************************
 * CeilNode -- Returns the associated ceil node, i.e., the deepest possible leaf
 *
 */
static cbt_Node cbt__CeilNode_Fast(const cbt_Tree *tree, const cbt_Node node)
{
    return cbt__CreateNode(node.id << (tree->maxDepth - node.depth),
                           tree->maxDepth);
}

static cbt_Node cbt__CeilNode(const cbt_Tree *tree, const cbt_Node node)
{
    return cbt_IsNullNode(node) ? node : cbt__CeilNode_Fast(tree, node);
}


/*******************************************************************************
 * SiblingNode -- Computes the sibling of the input node
 *
 */
static cbt_Node cbt__SiblingNode_Fast(const cbt_Node node)
{
    return cbt__CreateNode(node.id ^ 1u, node.depth);
}

CBTDEF cbt_Node cbt_SiblingNode(const cbt_Node node)
{
    return cbt_IsNullNode(node) ? node : cbt__SiblingNode_Fast(node);
}


/*******************************************************************************
 * RightSiblingNode -- Computes the right sibling of the input node
 *
 */
static cbt_Node cbt__RightSiblingNode_Fast(const cbt_Node node)
{
    return cbt__CreateNode(node.id | 1u, node.depth);
}

CBTDEF cbt_Node cbt_RightSiblingNode(const cbt_Node node)
{
    return cbt_IsNullNode(node) ? node : cbt__RightSiblingNode_Fast(node);
}


/*******************************************************************************
 * LeftSiblingNode -- Computes the left sibling of the input node
 *
 */
static cbt_Node cbt__LeftSiblingNode_Fast(const cbt_Node node)
{
    return cbt__CreateNode(node.id & (~1u), node.depth);
}

CBTDEF cbt_Node cbt_LeftSiblingNode(const cbt_Node node)
{
    return cbt_IsNullNode(node) ? node : cbt__LeftSiblingNode_Fast(node);
}


/*******************************************************************************
 * RightChildNode -- Computes the right child of the input node
 *
 */
static cbt_Node cbt__RightChildNode_Fast(const cbt_Node node)
{
    return cbt__CreateNode(node.id << 1u | 1u, node.depth + 1);
}

CBTDEF cbt_Node cbt_RightChildNode(const cbt_Node node)
{
    return cbt_IsNullNode(node) ? node : cbt__RightChildNode_Fast(node);
}


/*******************************************************************************
 * LeftChildNode -- Computes the left child of the input node
 *
 */
static cbt_Node cbt__LeftChildNode_Fast(const cbt_Node node)
{
    return cbt__CreateNode(node.id << 1u, node.depth + 1);
}

CBTDEF cbt_Node cbt_LeftChildNode(const cbt_Node node)
{
    return cbt_IsNullNode(node) ? node : cbt__LeftChildNode_Fast(node);
}


/*******************************************************************************
 * HeapByteSize -- Computes the number of Bytes to allocate for the bitfield
 *
 * For a tree of max depth D, the number of Bytes is 2^(D-1).
 * Note that 2 bits are "wasted" in the sense that they only serve
 * to round the required number of bytes to a power of two.
 *
 */
static int32_t cbt__HeapByteSize(uint32_t treeMaxDepth)
{
    return 1 << (treeMaxDepth - 1);
}


/*******************************************************************************
 * HeapUint32Size -- Computes the number of uints to allocate for the bitfield
 *
 */
static inline int32_t cbt__HeapUint32Size(uint32_t treeMaxDepth)
{
    return cbt__HeapByteSize(treeMaxDepth) >> 2;
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
static inline uint32_t cbt__NodeBitID(const cbt_Tree *tree, const cbt_Node node)
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
cbt__NodeBitID_BitField(const cbt_Tree *tree, const cbt_Node node)
{
    return cbt__NodeBitID(tree, cbt__CeilNode(tree, node));
}


/*******************************************************************************
 * NodeBitSize -- Returns the number of bits storing the input node value
 *
 */
static inline
int32_t cbt__NodeBitSize(const cbt_Tree *tree, const cbt_Node node)
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
} cbt__HeapArgs;

cbt__HeapArgs
cbt__CreateHeapArgs(const cbt_Tree *tree, const cbt_Node node, int32_t bitCount)
{
    uint32_t alignedBitOffset = cbt__NodeBitID(tree, node);
    uint32_t maxBufferIndex = cbt__HeapUint32Size(tree->maxDepth) - 1u;
    uint32_t bufferIndexLSB = (alignedBitOffset >> 5u);
    uint32_t bufferIndexMSB = cbt__MinValue(bufferIndexLSB + 1, maxBufferIndex);
    cbt__HeapArgs args;

    args.bitOffsetLSB = alignedBitOffset & 31u;
    args.bitCountLSB = cbt__MinValue(32u - args.bitOffsetLSB, bitCount);
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
cbt__HeapWriteExplicit(
    cbt_Tree *tree,
    const cbt_Node node,
    int32_t bitCount,
    uint32_t bitData
) {
    cbt__HeapArgs args = cbt__CreateHeapArgs(tree, node, bitCount);

    cbt__BitFieldInsert(args.bitFieldLSB,
                        args.bitOffsetLSB,
                        args.bitCountLSB,
                        bitData);
    cbt__BitFieldInsert(args.bitFieldMSB,
                        0u,
                        args.bitCountMSB,
                        bitData >> args.bitCountLSB);
}

static void cbt__HeapWrite(cbt_Tree *tree, const cbt_Node node, uint32_t bitData)
{
    cbt__HeapWriteExplicit(tree, node, cbt__NodeBitSize(tree, node), bitData);
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
cbt__HeapReadExplicit(
    const cbt_Tree *tree,
    const cbt_Node node,
    int32_t bitCount
) {
    cbt__HeapArgs args = cbt__CreateHeapArgs(tree, node, bitCount);
    uint32_t lsb = cbt__BitFieldExtract(*args.bitFieldLSB,
                                        args.bitOffsetLSB,
                                        args.bitCountLSB);
    uint32_t msb = cbt__BitFieldExtract(*args.bitFieldMSB,
                                        0u,
                                        args.bitCountMSB);

    return (lsb | (msb << args.bitCountLSB));
}

static uint32_t cbt__HeapRead(const cbt_Tree *tree, const cbt_Node node)
{
    return cbt__HeapReadExplicit(tree, node, cbt__NodeBitSize(tree, node));
}


/*******************************************************************************
 * HeapWrite_BitField -- Sets the bit associated to a leaf node to bitValue
 *
 * This is a dedicated routine to write directly to the bitfield.
 *
 */
static void
cbt__HeapWrite_BitField(
    cbt_Tree *tree,
    const cbt_Node node,
    const uint32_t bitValue
) {
    uint32_t bitID = cbt__NodeBitID_BitField(tree, node);

    cbt__SetBitValue(&tree->heap[bitID >> 5u], bitID & 31u, bitValue);
}


/*******************************************************************************
 * IsLeafNode -- Checks if a node is a leaf node, i.e., that has no descendants
 *
 */
CBTDEF bool cbt_IsLeafNode(const cbt_Tree *tree, const cbt_Node node)
{
    return (cbt__HeapRead(tree, node) == 1u);
}


/*******************************************************************************
 * ClearData -- Resets the data buffer stored by a LEB
 *
 */
static void cbt__ClearBuffer(cbt_Tree *tree)
{
    CBT_MEMSET(tree->heap, 0, cbt__HeapByteSize(tree->maxDepth));
}


/*******************************************************************************
 * GetHeapMemory -- Returns a read-only pointer to the heap memory
 *
 */
CBTDEF const char *cbt_GetHeapMemory(const cbt_Tree *tree)
{
    return (char *)tree->heap;
}


/*******************************************************************************
 * SetHeapMemory -- Sets the heap memory from a read-only buffer
 *
 */
CBTDEF void cbt_SetHeapMemory(cbt_Tree *tree, const char *buffer)
{
    CBT_MEMCPY(tree->heap, buffer, cbt_HeapByteSize(tree));
}


/*******************************************************************************
 * HeapByteSize -- Returns the amount of bytes consumed by the LEB heap
 *
 */
CBTDEF int32_t cbt_HeapByteSize(const cbt_Tree *tree)
{
    return cbt__HeapByteSize(tree->maxDepth);
}


/*******************************************************************************
 * UpdateBuffer -- Sums the 2 elements below the current slot
 *
 */
static void cbt__ComputeSumReduction(cbt_Tree *tree)
{
    int32_t depth = tree->maxDepth;
    uint32_t minNodeID = (1u << depth);
    uint32_t maxNodeID = (2u << depth);

    // prepass: processes deepest levels in parallel
CBT_PARALLEL_FOR
    for (uint32_t nodeID = minNodeID; nodeID < maxNodeID; nodeID+= 32u) {
        uint32_t alignedBitOffset = cbt__NodeBitID(tree,
                                                   cbt__CreateNode(nodeID, depth));
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
        cbt__HeapWriteExplicit(tree, cbt__CreateNode(nodeID >> 2u, depth - 2), 24u, bitData);

        // 4-bits
        bitField = (bitField & 0x0F0F0F0Fu) + ((bitField >>  4u) & 0x0F0F0F0Fu);
        bitData = ((bitField >>  0u) & (15u <<  0u))
                | ((bitField >>  4u) & (15u <<  4u))
                | ((bitField >>  8u) & (15u <<  8u))
                | ((bitField >> 12u) & (15u << 12u));
        cbt__HeapWriteExplicit(tree, cbt__CreateNode(nodeID >> 3u, depth - 3), 16u, bitData);

        // 5-bits
        bitField = (bitField & 0x00FF00FFu) + ((bitField >>  8u) & 0x00FF00FFu);
        bitData = ((bitField >>  0u) & (31u << 0u))
                | ((bitField >> 11u) & (31u << 5u));
        cbt__HeapWriteExplicit(tree, cbt__CreateNode(nodeID >> 4u, depth - 4), 10u, bitData);

        // 6-bits
        bitField = (bitField & 0x0000FFFFu) + ((bitField >> 16u) & 0x0000FFFFu);
        bitData = bitField;
        cbt__HeapWriteExplicit(tree, cbt__CreateNode(nodeID >> 5u, depth - 5),  6u, bitData);
    }
CBT_BARRIER
    depth-= 5;

    // iterate over elements atomically
    while (--depth >= 0) {
        uint32_t minNodeID = 1u << depth;
        uint32_t maxNodeID = 2u << depth;

CBT_PARALLEL_FOR
        for (uint32_t j = minNodeID; j < maxNodeID; ++j) {
            uint32_t x0 = cbt__HeapRead(tree, cbt__CreateNode(j << 1u     , depth + 1));
            uint32_t x1 = cbt__HeapRead(tree, cbt__CreateNode(j << 1u | 1u, depth + 1));

            cbt__HeapWrite(tree, cbt__CreateNode(j, depth), x0 + x1);
        }
CBT_BARRIER
    }
}


/*******************************************************************************
 * Buffer Ctor
 *
 */
CBTDEF cbt_Tree *cbt_CreateAtDepth(int32_t maxDepth, int32_t depth)
{
    CBT_ASSERT(maxDepth >=  5 && "maxDepth must be at least 5");
    CBT_ASSERT(maxDepth <= 29 && "maxDepth must be at most 29");
    cbt_Tree *tree = (cbt_Tree *)CBT_MALLOC(sizeof(*tree));

    tree->maxDepth = maxDepth;
    tree->heap = (uint32_t *)CBT_MALLOC(cbt__HeapByteSize(maxDepth));
    CBT_ASSERT(tree->heap != NULL && "Memory allocation failed");
    cbt_ResetToDepth(tree, depth);

    return tree;
}

CBTDEF cbt_Tree *cbt_Create(int maxDepth)
{
    return cbt_CreateAtDepth(maxDepth, 0);
}


/*******************************************************************************
 * Buffer Dtor
 *
 */
CBTDEF void cbt_Release(cbt_Tree *tree)
{
    CBT_FREE(tree->heap);
    CBT_FREE(tree);
}


/*******************************************************************************
 * ResetToDepth -- Initializes a LEB to its a specific subdivision level
 *
 */
CBTDEF void cbt_ResetToDepth(cbt_Tree *tree, int32_t depth)
{
    CBT_ASSERT(depth >= 0 && "depth must be at least equal to 0");
    CBT_ASSERT(depth <= tree->maxDepth && "depth must be at most equal to maxDepth");
    uint32_t minNodeID = 1u << depth;
    uint32_t maxNodeID = 2u << depth;

    cbt__ClearBuffer(tree);

    for (uint32_t nodeID = minNodeID; nodeID < maxNodeID; ++nodeID) {
        cbt_Node node = cbt__CreateNode(nodeID, depth);

        cbt__HeapWrite_BitField(tree, node, 1u);
    }

    cbt__ComputeSumReduction(tree);
}


/*******************************************************************************
 * ResetToMaxDepth -- Initializes a LEB to its maximum subdivision level
 *
 */
CBTDEF void cbt_ResetToMaxDepth(cbt_Tree *tree)
{
    cbt_ResetToDepth(tree, tree->maxDepth);
}


/*******************************************************************************
 * Split -- Subdivides a node in two
 *
 * The _Fast version does not check if the node can actually split, so
 * use it wisely, i.e., when you're absolutely sure the node depth is
 * less than maxDepth.
 *
 */
CBTDEF void cbt_SplitNode_Fast(cbt_Tree *tree, const cbt_Node node)
{
    cbt__HeapWrite_BitField(tree, cbt_RightChildNode(node), 1u);
}

CBTDEF void cbt_SplitNode(cbt_Tree *tree, const cbt_Node node)
{
    if (!cbt_IsCeilNode(tree, node))
        cbt_SplitNode_Fast(tree, node);
}


/*******************************************************************************
 * Merge -- Merges the node with its neighbour
 *
 * The _Fast version does not check if the node can actually merge, so
 * use it wisely, i.e., when you're absolutely sure the node depth is
 * greater than 0.
 *
 */
CBTDEF void cbt_MergeNode_Fast(cbt_Tree *tree, const cbt_Node node)
{
    cbt__HeapWrite_BitField(tree, cbt_RightSiblingNode(node), 0u);
}

CBTDEF void cbt_MergeNode(cbt_Tree *tree, const cbt_Node node)
{
    if (!cbt_IsRootNode(tree, node))
        cbt_MergeNode_Fast(tree, node);
}


/*******************************************************************************
 * MaxDepth -- Returns the max LEB depth
 *
 */
CBTDEF int32_t cbt_MaxDepth(const cbt_Tree *tree)
{
    return tree->maxDepth;
}


/*******************************************************************************
 * NodeCount -- Returns the number of triangles in the LEB
 *
 */
CBTDEF int32_t cbt_NodeCount(const cbt_Tree *tree)
{
    return cbt__HeapRead(tree, cbt__CreateNode(1u, 0));
}


/*******************************************************************************
 * DecodeNode -- Returns the leaf node associated to index nodeID
 *
 * This is procedure is for iterating over the nodes.
 *
 */
CBTDEF cbt_Node cbt_DecodeNode(const cbt_Tree *tree, int32_t handle)
{
    CBT_ASSERT(handle < cbt_NodeCount(tree) && "handle > NodeCount");
    CBT_ASSERT(handle >= 0 && "handle < 0");

    cbt_Node node = cbt__CreateNode(1u, 0);

    while (cbt__HeapRead(tree, node) > 1u) {
        uint32_t cmp = cbt__HeapRead(tree, cbt__CreateNode(node.id<<= 1u, ++node.depth));
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
CBTDEF int32_t cbt_EncodeNode(const cbt_Tree *tree, const cbt_Node node)
{
    CBT_ASSERT(cbt_IsLeafNode(tree, node) && "node is not a leaf");

    int32_t handle = 0u;
    cbt_Node nodeIterator = node;

    while (nodeIterator.id > 1u) {
        cbt_Node sibling = cbt__LeftSiblingNode_Fast(nodeIterator);
        uint32_t nodeCount = cbt__HeapRead(tree, cbt__CreateNode(sibling.id, sibling.depth));

        handle+= (nodeIterator.id & 1u) * nodeCount;
        nodeIterator = cbt__ParentNode_Fast(nodeIterator);
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
CBTDEF void cbt_Update(cbt_Tree *tree, cbt_UpdateCallback updater)
{
CBT_PARALLEL_FOR
    for (int64_t handle = 0; handle < cbt_NodeCount(tree); ++handle) {
        updater(tree, cbt_DecodeNode(tree, handle));
    }
CBT_BARRIER

    cbt__ComputeSumReduction(tree);
}

#undef CBT_ATOMIC
#undef CBT_PARALLEL_FOR
#undef CBT_BARRIER
#endif

