// requires cbt.glsl
uniform int u_CbtID = 0;
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = CBT_DISPATCHER_BUFFER_BINDING)
buffer DispatchIndirectCommandBuffer {
    uint u_CbtDispatchBuffer[];
};

void main()
{
    const int cbtID = u_CbtID;
    uint nodeCount = cbt_NodeCount(cbtID);

    u_CbtDispatchBuffer[0] = max(nodeCount >> 8, 1u);
}
