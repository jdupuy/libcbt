/* leb.h - public domain library for creating longest edge bisection tessellations
by Jonathan Dupuy

   INTERFACING
   define LEB_LOG(format, ...) to use your own logger (default prints to stdout)
   define LEB_ASSERT(x) to avoid using assert.h
*/

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

// data structures
typedef struct {
    uint64_t left, right, edge, node;
} leb_SameDepthNeighborIDs;
typedef struct {
    cbt_Node base, top;
} leb_DiamondParent;
typedef struct {
    cbt_Node left, right, edge, node;
} leb_NodeAndNeighbors;

// manipulation
LEBDEF void leb_SplitNode2D(cbt_Tree *leb,
                            const cbt_Node node);
LEBDEF void leb_MergeNode2D(cbt_Tree *leb,
                            const cbt_Node node,
                            const leb_DiamondParent diamond);

// O(depth) queries
LEBDEF leb_NodeAndNeighbors leb_DecodeNodeAndNeighbors(const cbt_Tree *leb,
                                                       int64_t handle);
LEBDEF leb_SameDepthNeighborIDs leb_DecodeSameDepthNeighborIDs(const cbt_Node node);
LEBDEF leb_DiamondParent leb_DecodeDiamondParent(const cbt_Node node);

// subdivision routines
LEBDEF void leb_DecodeNodeAttributeArray2D(const cbt_Node node,
                                           int64_t attributeArraySize,
                                           float attributeArray[][3]);

#ifdef __cplusplus
} // extern "C"
#endif

//
//
//// end header file ///////////////////////////////////////////////////////////
#endif // LEB_INCLUDE_LEB_H

#ifdef LEB_IMPLEMENTATION

#ifndef LEB_ASSERT
#    include <assert.h>
#    define LEB_ASSERT(x) assert(x)
#endif

#ifndef LEB_LOG
#    include <stdio.h>
#    define LEB_LOG(format, ...) do { fprintf(stdout, format, ##__VA_ARGS__); fflush(stdout); } while(0)
#endif


/*******************************************************************************
 * GetBitValue -- Returns the value of a bit stored in a 64-bit word
 *
 */
static uint64_t leb__GetBitValue(const uint64_t bitField, int64_t bitID)
{
    return ((bitField >> bitID) & 1u);
}


/*******************************************************************************
 * CreateSameDepthNeighborIDs -- Constructor for SameDepthNeighborIDs struct
 *
 */
static leb_SameDepthNeighborIDs
leb__CreateSameDepthNeighborIDs(
    uint64_t left,
    uint64_t right,
    uint64_t edge,
    uint64_t node
) {
    leb_SameDepthNeighborIDs ids;

    ids.left = left;
    ids.right = right;
    ids.edge = edge;
    ids.node = node;

    return ids;
}


/*******************************************************************************
 * CreateSameDepthNeighborIDs -- Constructor for DiamondParent struct
 *
 */
static leb_DiamondParent leb__CreateDiamondParent(cbt_Node base, cbt_Node top)
{
    leb_DiamondParent diamondParent;

    diamondParent.base = base;
    diamondParent.top = top;

    return diamondParent;
}


/*******************************************************************************
 * EdgeNode -- Computes the neighbour of the input node wrt to its longest edge
 *
 */
static cbt_Node leb__EdgeNode(const cbt_Node node)
{
    uint64_t nodeID = leb_DecodeSameDepthNeighborIDs(node).edge;

    return cbt_CreateNode(nodeID, (nodeID == 0u) ? 0 : node.depth);
}


/*******************************************************************************
 * SplitNode2D -- Splits a node while producing a conforming LEB
 *
 */
LEBDEF void leb_SplitNode2D(cbt_Tree *leb, const cbt_Node node)
{
    if (!cbt_IsCeilNode(leb, node)) {
        const uint64_t minNodeID = 1u;
        cbt_Node nodeIterator = node;

        cbt_SplitNode_Fast(leb, nodeIterator);
        nodeIterator = leb__EdgeNode(nodeIterator);

        while (nodeIterator.id >= minNodeID) {
            cbt_SplitNode_Fast(leb, nodeIterator);
            nodeIterator = cbt_ParentNode(nodeIterator);
            cbt_SplitNode_Fast(leb, nodeIterator);
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
    cbt_Tree *leb,
    const cbt_Node node,
    const leb_DiamondParent diamond
) {
    if (!cbt_IsRootNode(node)) {
        cbt_Node dualNode = cbt_RightChildNode(diamond.top);
        bool b1 = cbt_IsLeafNode(leb, cbt_SiblingNode_Fast(node));
        bool b2 = cbt_IsLeafNode(leb, dualNode);
        bool b3 = cbt_IsLeafNode(leb, cbt_SiblingNode_Fast(dualNode));

        if (b1 && b2 && b3) {
            cbt_MergeNode_Fast(leb, node);
            cbt_MergeNode_Fast(leb, dualNode);
        }
    }
}


/*******************************************************************************
 * SplitNodeIDs -- Updates the IDs of neighbors after one LEB split
 *
 * This code applies the following rules:
 * Split left:
 * LeftID  = 2 * NodeID + 1
 * RightID = 2 * EdgeID + 1
 * EdgeID  = 2 * RightID + 1
 *
 * Split right:
 * LeftID  = 2 * EdgeID
 * RightID = 2 * NodeID
 * EdgeID  = 2 * LeftID
 *
 * The node channel stores NodeID, which is required for applying the
 * rules.
 *
 */
static leb_SameDepthNeighborIDs
leb__SplitNodeIDs(
    const leb_SameDepthNeighborIDs nodeIDs,
    const uint64_t splitBit
) {
    uint64_t n1 = nodeIDs.left, n2 = nodeIDs.right,
             n3 = nodeIDs.edge, n4 = nodeIDs.node;
    uint64_t b2 = (n2 == 0u) ? 0u : 1u,
             b3 = (n3 == 0u) ? 0u : 1u;

    if (splitBit == 0u) {
        return leb__CreateSameDepthNeighborIDs(n4 << 1 | 1, n3 << 1 | b3, n2 << 1 | b2, n4 << 1    );
    } else {
        return leb__CreateSameDepthNeighborIDs(n3 << 1    , n4 << 1     , n1 << 1     , n4 << 1 | 1);
    }
}


/*******************************************************************************
 * DecodeSameDepthNeighborIDs -- Decodes the IDs of the leb_Nodes neighbour to node
 *
 * The IDs are associated to the depth of the input node. As such, they
 * don't necessarily exist in the LEB subdivision.
 */
LEBDEF leb_SameDepthNeighborIDs
leb_DecodeSameDepthNeighborIDs(const cbt_Node node)
{
    leb_SameDepthNeighborIDs nodeIDs = {0ULL, 0ULL, 0ULL, 1ULL};

    for (int64_t bitID = node.depth - 1; bitID >= 0; --bitID) {
        nodeIDs = leb__SplitNodeIDs(nodeIDs, leb__GetBitValue(node.id, bitID));
    }

    return nodeIDs;
}


/*******************************************************************************
 * SameDepthNeighborIDs -- Computes the IDs of the same-level neighbors of a node
 *
 */
LEBDEF leb_SameDepthNeighborIDs
leb_GetSameDepthNeighborIDs(const leb_NodeAndNeighbors nodes)
{
    uint64_t edgeID = nodes.edge.id << (nodes.node.depth - nodes.edge.depth);
    uint64_t leftID = nodes.left.id >> (nodes.left.depth - nodes.node.depth);
    uint64_t rightID = nodes.right.id >> (nodes.right.depth - nodes.node.depth);

    return leb__CreateSameDepthNeighborIDs(leftID, rightID, edgeID, nodes.node.id);
}


/*******************************************************************************
 * DecodeDiamondParent -- Decodes the upper Diamond associated to the leb_Node
 *
 * If the neighbour part does not exist, the parentNode is copied instead.
 *
 */
LEBDEF leb_DiamondParent leb_DecodeDiamondParent(const cbt_Node node)
{
    cbt_Node parentNode = cbt_ParentNode(node);
    uint64_t diamondNodeID = leb_DecodeSameDepthNeighborIDs(parentNode).edge;
    cbt_Node diamondNode = cbt_CreateNode(
        diamondNodeID > 0u ? diamondNodeID : parentNode.id,
        parentNode.depth
    );

    return leb__CreateDiamondParent(parentNode, diamondNode);
}


/******************************************************************************/
/* Standalone matrix 3x3 API
 *
 */
typedef float leb__Matrix3x3[3][3];


/*******************************************************************************
 * IdentityMatrix3x3 -- Sets a 3x3 matrix to identity
 *
 */
static void leb__IdentityMatrix3x3(leb__Matrix3x3 m)
{
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f;
}


/*******************************************************************************
 * TransposeMatrix3x3 -- Transposes a 3x3 matrix
 *
 */
static void leb__TransposeMatrix3x3(const leb__Matrix3x3 m, leb__Matrix3x3 out)
{
    for (int64_t i = 0; i < 3; ++i)
        for (int64_t j = 0; j < 3; ++j)
            out[i][j] = m[j][i];
}


/*******************************************************************************
 * DotProduct -- Returns the dot product of two vectors
 *
 */
static float leb__DotProduct(int argSize, const float *x, const float *y)
{
    float dp = 0.0f;

    for (int64_t i = 0; i < argSize; ++i)
        dp+= x[i] * y[i];

    return dp;
}


/*******************************************************************************
 * MulMatrix3x3 -- Computes the product of two 3x3 matrices
 *
 */
static void
leb__Matrix3x3Product(
    const leb__Matrix3x3 m1,
    const leb__Matrix3x3 m2,
    leb__Matrix3x3 out
) {
    leb__Matrix3x3 tra;

    leb__TransposeMatrix3x3(m2, tra);

    for (int j = 0; j < 3; ++j)
    for (int i = 0; i < 3; ++i)
        out[j][i] = leb__DotProduct(3, m1[j], tra[i]);
}


/*******************************************************************************
 * SplitMatrix3x3 -- Computes a LEB splitting matrix from a split bit
 *
 */
static void
leb__SplittingMatrix(leb__Matrix3x3 splittingMatrix, uint64_t splitBit)
{
    float b = (float)splitBit;
    float c = 1.0f - b;
    leb__Matrix3x3 splitMatrix = {
        {c   , b   , 0.0f},
        {0.5f, 0.0f, 0.5f},
        {0.0f,    c,    b}
    };
    leb__Matrix3x3 tmp;

    memcpy(tmp, splittingMatrix, sizeof(tmp));
    leb__Matrix3x3Product(splitMatrix, tmp, splittingMatrix);
}


/*******************************************************************************
 * DecodeTransformationMatrix -- Computes the matrix associated to a LEB
 * node
 *
 */
static void
leb__DecodeTransformationMatrix(
    const cbt_Node node,
    leb__Matrix3x3 splittingMatrix
) {
    leb__IdentityMatrix3x3(splittingMatrix);

    for (int64_t bitID = node.depth - 1; bitID >= 0; --bitID) {
        leb__SplittingMatrix(splittingMatrix, leb__GetBitValue(node.id, bitID));
    }
}


/*******************************************************************************
 * DecodeNodeAttributeArray -- Compute the triangle attributes at the input node
 *
 */
LEBDEF void
leb_DecodeNodeAttributeArray(
    const cbt_Node node,
    int attributeArraySize,
    float attributeArray[][3]
) {
    LEB_ASSERT(attributeArraySize > 0);

    leb__Matrix3x3 m;
    float attributeVector[3];

    leb__DecodeTransformationMatrix(node, m);

    for (int64_t i = 0; i < attributeArraySize; ++i) {
        memcpy(attributeVector, attributeArray[i], sizeof(attributeVector));
        attributeArray[i][0] = leb__DotProduct(3, m[0], attributeVector);
        attributeArray[i][1] = leb__DotProduct(3, m[1], attributeVector);
        attributeArray[i][2] = leb__DotProduct(3, m[2], attributeVector);
    }
}


#endif // LEB_IMPLEMENTATION
