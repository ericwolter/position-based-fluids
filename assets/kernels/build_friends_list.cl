// friendsData = struct
// {
//		Uint32 listsCount[MAX_PARTICLES_COUNT];
//		Uint32 lists[MAX_PARTICLES_COUNT*27*2];
// }
// 
// Related defines
//		MAX_PARTICLES_COUNT          Defines how many particles exists in the simulation
//     
// Access patterns
//		listsCount[ParticleIndex]             			==> Data[ParticleIndex];
//		lists[ParticleIndex][ListIndex][Start/Length]	==> Data[MAX_PARTICLES_COUNT +          // Skip listsCount block
//																		ParticleIndex * 27*2 +	// Offset to relevant particle
//																		ListIndex * 2 +			// Offset to relevant list
//																		Start/Length];			// Offset to either start/length

#define CELL_NOT_VISITED (-1)
#define GET_CUBE_INDEX(x,y,z) (9 * (x+1) + 3 * (y+1) + (z+1))

#define PRINT_PARTICLE (12167)

__kernel void buildFriendsList(__constant struct Parameters *Params,
                               const __global float4 *predicted,
                               const __global uint *cells,
                               __global uint *friends_list,
                               const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;
	
	// flag array to save already visited cells
	char cellToList[27];
	
	// the required number of lists required to map all cells
	// in an ideal world all 27 neighbor cells would be in sequence in memory
	// and thus can all be collapsed into a single list
	int listsCount = 0;
	
	// contains the pointer into the sorted particle buffer where the respective list start
	int listsStart[27];
	
	// contains the number of particles in the sorted particle buffer belonging to the respective list
	char listsLength[27];
	
	// initialize all cube cells as unvisited
	for (int c=0;c<27;++c) {
		cellToList[c] = CELL_NOT_VISITED;
	}

    // Start grid scan
    int3 current_cell = convert_int3(predicted[i].xyz / Params->h);

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int z = -1; z <= 1; ++z)
            {
				int cubeIndex = GET_CUBE_INDEX(x,y,z);
				
				// skip cell which was already visited and thus already included
				// in the lists
				if(cellToList[cubeIndex] != CELL_NOT_VISITED) {
					continue;
				}
				
				// calculate the hash value of the current cell in the cube
				int3 current_cube_cell = current_cell + (int3)(x, y, z);
                uint cell_index = calcGridHash(current_cube_cell);
				// find first and last particle in this cell
                uint2 cell_boundary = (uint2)(cells[cell_index*2+0],cells[cell_index*2+1]);
				
				// skip empty cells
				if(cell_boundary.x == END_OF_CELL_LIST) continue;

				// mark cell as visited
				cellToList[cubeIndex] = listsCount;
				
				// add a new list
				listsStart[listsCount] = cell_boundary.x;
				
				// cell boundaries are inclusive so +1 is needed to obtain
				// the number of particles in this cell
				listsLength[listsCount] = cell_boundary.y - cell_boundary.x + 1;
				int currentListIndex = listsCount;
				
				// by default assume worst case and start new list
				listsCount++;
				
				// check backwards
				int prevStep = 0;
				while(true) {
					// find cell index of the particle right before the current list starts
					int particleIndexBefore = listsStart[currentListIndex] - 1;
					
					// check for special case if the current start particle has index 0
					// which would result in the previous particle having an invalid index of -1
					if(particleIndexBefore < 0) {
						break;
					}
					float4 particleBefore = predicted[particleIndexBefore];
					
					// get grid cell position
					int3 cellBefore = convert_int3(particleBefore.xyz / Params->h);
					
					// check if cell belongs to the current cube
					int3 cellDifference = cellBefore - current_cell;
					if(abs(cellDifference.x) > 1 || abs(cellDifference.y) > 1 || abs(cellDifference.z) > 1) {
						break;
					}
					
					int cubeIndexBefore = GET_CUBE_INDEX(cellDifference.x, cellDifference.y, cellDifference.z);
					
					// if the cell was already visited and has thus an associated list
					// merge it with the current cells list
					int listIndexBefore = cellToList[cubeIndexBefore];
					if(listIndexBefore != CELL_NOT_VISITED) {
						// add current list to the already existing list
						listsLength[listIndexBefore] += listsLength[currentListIndex];
						
						// remove assumed new list
						listsStart[currentListIndex] = -2;
						listsLength[currentListIndex] = -2;
						cellToList[cubeIndex] = listIndexBefore;
						currentListIndex = listIndexBefore;
						
						listsCount--;
					} else {
						// the cell before has not been iterated over yet by the 3x3x3 cube
						// so in order to avoid checking forward as well just add the cell right here
						
						// get the boundaries for the previous (not yet iterated) cell
						uint cellHashBefore = calcGridHash(cellBefore);
						uint2 cellBoundaryBefore = (uint2)(cells[cellHashBefore*2+0],cells[cellHashBefore*2+1]);
						
						// add list before to the current list
						listsStart[currentListIndex] = cellBoundaryBefore.x;
						listsLength[currentListIndex] += cellBoundaryBefore.y - cellBoundaryBefore.x + 1;
						
						// mark the cell before as visited so when the 3x3x3 cube iteration tries
						// to check it again it will be skipped
						cellToList[cubeIndexBefore] = currentListIndex;
					}
					
					prevStep++;
				}
			}
        }
    }
	
	friends_list[i] = listsCount;
	for (int listIndex=0;listIndex<listsCount;++listIndex) {
		int startIndex = MAX_PARTICLES_COUNT + i * (27*2) + listIndex * 2 + 0;
		int lengthIndex = startIndex + 1;
		friends_list[startIndex] = listsStart[listIndex];
		friends_list[lengthIndex] = listsLength[listIndex];
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
