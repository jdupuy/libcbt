
#define CBT_IMPLEMENTATION
#include "../cbt.h"

typedef struct {
    cbt_Tree *cbt;
    leb_Heap *neighbors[3];
} leb_Heap;

typedef struct {
    uint32_t left, right, edge;
} leb_Neighbor;

typedef struct {
    cbt_Tree *subd;
    leb_Neighbor neighbor;
} leb_Face;

typedef struct {
    int faceCount;
    leb_Face *faces;
} leb_Mesh;

struct Mesh {
    cbt_Tree **trees;
    uint32_t *neighbors[3];
};

struct node {
    int heapID;
    int id, depth;
};

#if 1
void leb_Split(leb_Mesh *mesh, cbt_Node node)
{
    if (!leb_IsCeilNode(face.subd, node)) {
        cbt_Node nodeIterator = node;

        leb_SplitNode(face.subd, nodeIterator);
        nodeIterator = leb__EdgeNode(nodeIterator);

        while (nodeIterator.id >= 0u) {
            leb__SplitNode(leb, nodeIterator);
            nodeIterator = cbt_ParentNode(nodeIterator);
            leb__SplitNode(leb, nodeIterator);
            nodeIterator = leb__EdgeNode(nodeIterator);
        }
    }
}
#endif

int32_t leb__NeighborFaceID(const leb_Mesh *m, uint32_t nodeID)
{
    return 0;
}

// subdivide the mesh
void leb_Update(leb_Mesh *m)
{
    // for each face
    for (int faceID = 0; faceID < m->faceCount; ++faceID) {
        cbt_Tree *subd = m->faces[faceID].subd;

        // update local subd
        for (int handleID = 0; handleID < cbt_NodeCount(subd); ++handleID) {
            cbt_Node node = cbt_DecodeNode(subd, handleID);

            cbt_SplitNode(subd, node);
        }
    }
}

int main(int argc, char **argv)
{


    return 0;
}

#if 0
typedef struct {
    uint32_t left, right, edge, node;
} SameDepthNeighborIDs;

typedef struct {
    uint32_t id;
    int32_t depth;
} Node;


SameDepthNeighborIDs
CreateSameDepthNeighborIDs(uint32_t left, uint32_t right, uint32_t edge, uint32_t node)
{
    SameDepthNeighborIDs nodeIDs;

    nodeIDs.left = left;
    nodeIDs.right = right;
    nodeIDs.edge = edge;
    nodeIDs.node = node;

    return nodeIDs;
}

uint32_t GetBitValue(const uint32_t bitField, uint32_t bitID)
{
    return ((bitField >> bitID) & 1u);
}

SameDepthNeighborIDs
SplitNodeIDs(const SameDepthNeighborIDs nodeIDs, const uint32_t splitBit)
{
    uint32_t n1 = nodeIDs.left, n2 = nodeIDs.right,
             n3 = nodeIDs.edge, n4 = nodeIDs.node;
    uint32_t b2 = (n2 == 0u) ? 0u : 1u,
             b3 = (n3 == 0u) ? 0u : 1u;

    if (splitBit == 0u) {
        return CreateSameDepthNeighborIDs(
            n4 << 1 | 1, n3 << 1 | b3, n2 << 1 | b2, n4 << 1
        );
    } else {
        return CreateSameDepthNeighborIDs(
            n3 << 1    , n4 << 1     , n1 << 1     , n4 << 1 | 1
        );
    }
}

SameDepthNeighborIDs
DecodeSameDepthNeighborIDs(const Node node, SameDepthNeighborIDs nodeIDs)
{
    for (int bitID = node.depth - 1; bitID >= 0; --bitID) {
        nodeIDs = SplitNodeIDs(nodeIDs, GetBitValue(node.id, bitID));
    }

    return nodeIDs;
}

int main(int argc, char **argv)
{
    SameDepthNeighborIDs init = CreateSameDepthNeighborIDs(2, 5, 0, 1);
    Node n1 = {2, 1}, n2 = {3, 1};

    SameDepthNeighborIDs d1 = DecodeSameDepthNeighborIDs(n1, init);
    SameDepthNeighborIDs d2 = DecodeSameDepthNeighborIDs(n2, init);

    printf("d1: %i %i %i %i\n", d1.left, d1.right, d1.edge, d1.node);
    printf("d2: %i %i %i %i\n", d2.left, d2.right, d2.edge, d2.node);

    return 0;
}
#endif
