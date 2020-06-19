// requires cbt.glsl
#ifndef CBT_LOCAL_SIZE_X
#   define CBT_LOCAL_SIZE_X 256
#endif
uniform int u_CbtID = 0;
uniform int u_PassID;
layout (local_size_x = CBT_LOCAL_SIZE_X,
        local_size_y = 1,
        local_size_z = 1) in;

void main(void)
{
    const int cbtID = u_CbtID;
    uint cnt = (1u << u_PassID);
    uint threadID = gl_GlobalInvocationID.x;

    if (threadID < cnt) {
        uint nodeID = threadID + cnt;
        uint x0 = cbt_HeapRead(cbtID, cbt_CreateNode(nodeID << 1u     , u_PassID + 1));
        uint x1 = cbt_HeapRead(cbtID, cbt_CreateNode(nodeID << 1u | 1u, u_PassID + 1));

        cbt__HeapWrite(cbtID, cbt_CreateNode(nodeID, u_PassID), x0 + x1);
    }
}
