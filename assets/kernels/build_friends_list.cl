// Important: below arrays are Row Major (http://en.wikipedia.org/wiki/Row-major_order)
// friendsData = struct
// {
//     Uint32 friendsCount[MAX_FRIENDS_CIRCLES][MAX_PARTICLES_COUNT];
//     Uint32 friendIndex[MAX_FRIENDS_CIRCLES][MAX_FRIENDS_IN_CIRCLE][MAX_PARTICLES_COUNT];
// }
// 
// Related defines
//     MAX_PARTICLES_COUNT          Defines how many particles exists in the simulation
//     MAX_FRIENDS_CIRCLES          Defines how many friends circle are we going to scan for
//     MAX_FRIENDS_IN_CIRCLE        Defines the max number of particles per cycle
//     
// Cached values
//     FRIENDS_BLOCK_SIZE = MAX_PARTICLES_COUNT * MAX_FRIENDS_CIRCLES;
// 
// Access patterns
//     friendsCount[CircleIndex][ParticleIndex]             ==> Data[CircleIndex * MAX_PARTICLES_COUNT + ParticleIndex];
//     friendIndex[CircleIndex][FriendIndex][ParticleIndex] ==> Data[FRIENDS_BLOCK_SIZE +                                             // Skip friendsCount block
//                                                                   CircleIndex * (MAX_PARTICLES_COUNT * MAX_FRIENDS_IN_CIRCLE) +    // Offset to relevent circle
//                                                                   FriendIndex * (MAX_PARTICLES_COUNT)                              // Offset to relevent friend_index
//                                                                   ParticleIndex];                                                  // Offset to particle_index

__kernel void buildFriendsList(__constant struct Parameters *Params,
                               const __global float4 *predicted,
                               const __global uint *cells,
                               __global int *friends_list,
                               const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;
    
    const float MIN_R = 0.3f * Params->h;

    // Define circle particle counter varible
    __private int circleParticles[MAX_FRIENDS_CIRCLES];
    for (int j = 0; j < MAX_FRIENDS_CIRCLES; j++)
        circleParticles[j] = 0;

    // Start grid scan
    int3 current_cell = convert_int3(predicted[i].xyz / Params->h);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int z = -1; z <= 1; ++z)
            {
                uint cell_index = calcGridHash(current_cell + (int3)(x, y, z));

                // find first and last particle in this cell
                uint2 cell_boundary = (uint2)(cells[cell_index*2+0],cells[cell_index*2+1]);
				
				// skip empty cells
				if(cell_boundary.x == END_OF_CELL_LIST) continue;
					
				// iterate over all particles in this cell
				for(int j_index = cell_boundary.x; j_index <= cell_boundary.y; ++j_index) {
                    // Skip self
                    if (i == j_index)
                        continue;

                    // Ignore unfriendly particles (r > h)
                    const float3 r = predicted[i].xyz - predicted[j_index].xyz;
                    const float  r_length_2 = dot(r, r);
                    if (r_length_2 >= Params->h_2)
                        continue;

                    // Find particle circle
                    const float adjusted_r = max(0.0f, (sqrt(r_length_2) - MIN_R) / (Params->h - MIN_R));
                    const int j_circle = min(convert_int(adjusted_r * adjusted_r * adjusted_r * MAX_FRIENDS_CIRCLES), MAX_FRIENDS_CIRCLES - 1);

                    // Make sure particle doesn't have too many friends
                    if (circleParticles[j_circle] >= MAX_FRIENDS_IN_CIRCLE)
                    {
                        // printf("Damn! we need a bigger MAX_FRIENDS_IN_CIRCLE\n");
                        continue;
                    }

                    // Increments friends in circle counter
                    int friendIndex = circleParticles[j_circle]++;

                    // Add friend to relevent circle
                    int index = FRIENDS_BLOCK_SIZE +                                          // Skip friendsCount block
                                j_circle    * (MAX_PARTICLES_COUNT * MAX_FRIENDS_IN_CIRCLE) + // Offset to relevent circle
                                friendIndex * (MAX_PARTICLES_COUNT) +                         // Offset to relevent friend_index
                                i;                                                            // Offset to particle_index                    
                    
                    friends_list[index] = j_index;
                }
            }
        }
    }

    // Save counters
    for (int iCircle = 0; iCircle < MAX_FRIENDS_CIRCLES; iCircle++)
    {
        friends_list[iCircle * MAX_PARTICLES_COUNT + i] = circleParticles[iCircle];
    }
}

/*
byte cellToList[27] = 0; 
int listsCount = 0;
int listStart[27];
byte listLength[27];

// Reset cell2List (-1 means no list)
for (int i=0;i<27;i++)
  cellToList[i] = -1;

for (int x= -1; x >= +1; x++)
{
  for (int y= -1; y >= +1; z++)
  {
    for (int z= -1; z >= +1; z++)
    {
        // [A] Check if cell was already counted
        int cubeIndex = x * 9 + y * 6 + z;
        if (cellToList[cubeIndex] != -1)
            continue;
       
        // Mark cell as used
        cellToList[cubeIndex] = listsCount;
           
        // Get cell hash
        int cell_hash = morton(x,y,z) % gridsize;

        // [B] Add new list
        listStart[listsCount] = cell_start_index(cell_hash);
        listLength[listsCount] = ...;
        listsCount++;
        
        // [C] Check backwards
        int 
        while (true)
        {
            // Find the cell index of the particle prior to current location
            vec3 prevParticle = get_particle_pos(listStart[listsCount] - 1);
            
            // get grid cell position
            ivec3 prevGridPos = prevParticle / ...;
            
            // [E] Check if in range of current cube (3x3x3)
            if (!...)
                break;
            
            // [F] Check if already added
            int prevCubeIndex = out_x * 9 + out_y * 6 + out_z;
            if (cellToList[prevCubeIndex] != -1)
            {
                // [G] add our list to the end of the list
                listLength[cellToList[prevCubeIndex]] += listLength[listsCount];
                
                // Cancel new list
                listsCount--;
            }
        }
        
        // Check forward
        // ...
    }
  }
}
*/
