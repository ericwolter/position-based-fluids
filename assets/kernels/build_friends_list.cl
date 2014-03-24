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
//		lists[ListIndex][ParticleIndex][Start/Length]	==> Data[MAX_PARTICLES_COUNT +          				// Skip listsCount block
//																		ListIndex * 2 * MAX_PARTICLES_COUNT +	// Offset to relevant particle
//																		ParticleIndex * 2 +						// Offset to relevant list
//																		Start/Length];							// Offset to either start/length

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
	int cellToList[27];
	
	// the required number of lists required to map all cells
	// in an ideal world all 27 neighbor cells would be in sequence in memory
	// and thus can all be collapsed into a single list
	int listsCount = 0;
	
	// contains the pointer into the sorted particle buffer where the respective list start
	int listsStart[27];
	
	// contains the number of particles in the sorted particle buffer belonging to the respective list
	int listsLength[27];
	
	for (int c=0;c<27;++c) {
		cellToList[c] = CELL_NOT_VISITED;
		listsStart[c] = CELL_NOT_VISITED;
		listsLength[c] = CELL_NOT_VISITED;
	}

    // Start grid scan
    int3 current_cell = convert_int3(predicted[i].xyz / Params->h);
	
	int refNumNeighbors = 0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int z = -1; z <= 1; ++z)
            {
				// {
					// // reference neighbors
					// uint cell_index = calcGridHash(current_cell + (int3)(x, y, z));

					// // find first and last particle in this cell
					// uint2 cell_boundary = (uint2)(cells[cell_index*2+0],cells[cell_index*2+1]);
					
					// // skip empty cells
					// if(cell_boundary.x == END_OF_CELL_LIST) continue;
						
					// // iterate over all particles in this cell
					// for(int j_index = cell_boundary.x; j_index <= cell_boundary.y; ++j_index) {
						// refNumNeighbors++;
					// }
				// }
			
				// if (i==PRINT_PARTICLE) {
					// printf("%d: cube: (%d,%d,%d) cell: (%d,%d,%d)\n",i,x,y,z,current_cell.x,current_cell.y,current_cell.z);
				// }
				int cubeIndex = GET_CUBE_INDEX(x,y,z);
				// if (i==PRINT_PARTICLE) {
					// printf("%d: cube: (%d,%d,%d) index: %d\n",i,x,y,z,cubeIndex);
				// }
				
				// skip cell which was already visited and thus already included
				// in the lists
				if(cellToList[cubeIndex] != CELL_NOT_VISITED) {
					// if (i==PRINT_PARTICLE) {
						// printf("%d: cube cell already visited\n",i);
					// }
					continue;
				}
				
				// calculate the hash value of the current cell in the cube
				int3 current_cube_cell = current_cell + (int3)(x, y, z);
                uint cell_index = calcGridHash(current_cube_cell);
				// find first and last particle in this cell
                uint2 cell_boundary = (uint2)(cells[cell_index*2+0],cells[cell_index*2+1]);
				// if (i==PRINT_PARTICLE) {
					// //printf("%d: cell: (%d,%d,%d) index: %d\n",i,current_cube_cell.x,current_cube_cell.y,current_cube_cell.z,cell_index);
					// printf("%d: cell: (%d,%d,%d) boundary: (%d,%d)\n",i,current_cube_cell.x,current_cube_cell.y,current_cube_cell.z,cell_boundary.x,cell_boundary.y);
				// }
				
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
					// if (i==PRINT_PARTICLE) {
						// printf("%d: prevStep: %d\n",i,prevStep);
					// }
					// if (i==PRINT_PARTICLE) {
						// printf("%d: currentListIndex: %d\n",i,currentListIndex);
					// }
				
					// find cell index of the particle right before the current list starts
					int particleIndexBefore = listsStart[currentListIndex] - 1;
					// if (i==PRINT_PARTICLE) {
						// printf("%d: particleIndexBefore: %d\n",i,particleIndexBefore);
					// }
					
					// check for special case if the current start particle has index 0
					// which would result in the previous particle having an invalid index of -1
					if(particleIndexBefore < 0) {
						break;
					}
					float4 particleBefore = predicted[particleIndexBefore];
					
					// get grid cell position
					int3 cellBefore = convert_int3(particleBefore.xyz / Params->h);
					// if (i==PRINT_PARTICLE) {
						// printf("%d: cellBefore: (%d,%d,%d)\n",i,cellBefore.x,cellBefore.y,cellBefore.z);
					// }
					
					// check if cell belongs to the current cube
					int3 cellDifference = cellBefore - current_cell;
					// if (i==PRINT_PARTICLE) {
						// printf("%d: cellDifference: (%d,%d,%d)\n",i,cellDifference.x,cellDifference.y,cellDifference.z);
					// }
					
					if(abs(cellDifference.x) > 1 || abs(cellDifference.y) > 1 || abs(cellDifference.z) > 1) {
						// if (i==PRINT_PARTICLE) {
							// printf("%d: cell does not belong to current cube\n",i);
						// }
						break;
					}
					
					int cubeIndexBefore = GET_CUBE_INDEX(cellDifference.x, cellDifference.y, cellDifference.z);
					// if (i==PRINT_PARTICLE) {
						// printf("%d: cubeIndexBefore: %d\n",i,cubeIndexBefore);
					// }
					
					// if the cell was already visited and has thus an associated list
					// merge it with the current cells list
					int listIndexBefore = cellToList[cubeIndexBefore];
					if(listIndexBefore != CELL_NOT_VISITED) {
						
						// if (i==PRINT_PARTICLE) {
							// printf("%d: cell already visited, listIndexBefore: %d\n",i,listIndexBefore);
							// printf("%d: old listsLength: %d\n",i,listsLength[listIndexBefore]);
							// printf("%d: current listsLength: %d\n",i,listsLength[currentListIndex]);
						// }
					
						// add current list to the already existing list
						listsLength[listIndexBefore] += listsLength[currentListIndex];
						
						// if (i==PRINT_PARTICLE) {
							// printf("%d: new listsLength: %d\n",i,listsLength[listIndexBefore]);
						// }
                
						// remove assumed new list
						listsStart[currentListIndex] = -2;
						listsLength[currentListIndex] = -2;
						cellToList[cubeIndex] = listIndexBefore;
						currentListIndex = listIndexBefore;
						
						listsCount--;
					} else {
						// the cell before has not been iterated over yet by the 3x3x3 cube
						// so in order to avoid checking forward as well just add the cell right here
						// if (i==PRINT_PARTICLE) {
							// printf("%d: cell not yet visited\n",i);
						// }
						
						// get the boundaries for the previous (not yet iterated) cell
						uint cellHashBefore = calcGridHash(cellBefore);
						uint2 cellBoundaryBefore = (uint2)(cells[cellHashBefore*2+0],cells[cellHashBefore*2+1]);
						// if (i==PRINT_PARTICLE) {
							// printf("%d: cellBefore: (%d,%d,%d) boundary: (%d,%d)\n",i,cellBefore.x,cellBefore.y,cellBefore.z,cellBoundaryBefore.x,cellBoundaryBefore.y);
						// }
						
						// add list before to the current list
						listsStart[currentListIndex] = cellBoundaryBefore.x;
						listsLength[currentListIndex] += cellBoundaryBefore.y - cellBoundaryBefore.x + 1;
						
						// mark the cell before as visited so when the 3x3x3 cube iteration tries
						// to check it again it will be skipped
						cellToList[cubeIndexBefore] = currentListIndex;
					}
					
					prevStep++;
				}
				
				// int testNumNeighbors = 0;
				// for (int c=0;c<27;++c) {
					// if (i==PRINT_PARTICLE) {
						// printf("%d: list %d: (%d,%d)\n",i,c,listsStart[c],listsLength[c]);
					// }
					// testNumNeighbors += listsLength[c] < 0 ? 0 : listsLength[c];
				// }
				// if (i==PRINT_PARTICLE) {
					// printf("%d: during neighbors %d = %d\n",i,refNumNeighbors,testNumNeighbors);
					// printf("%d: listsCount: %d\n",i,listsCount);
				// }  
			}
        }
    }
	
	// int testNumNeighbors = 0;
	friends_list[i] = listsCount;
	// if (i==PRINT_PARTICLE) {
		// printf("%d: friend list count %d:\n",i,listsCount);
	// }
	for (int listIndex=0;listIndex<listsCount;++listIndex) {
		// if (i==PRINT_PARTICLE) {
			// printf("%d: friend list %d: (%d,%d)\n",i,listIndex,listsStart[listIndex],listsLength[listIndex]);
		// }
		// testNumNeighbors += listsLength[listIndex] < 0 ? 0 : listsLength[listIndex];
		
		int startIndex = MAX_PARTICLES_COUNT + listIndex * 2 * MAX_PARTICLES_COUNT + i * 2 + 0;
		int lengthIndex = startIndex + 1;
		friends_list[startIndex] = listsStart[listIndex];
		friends_list[lengthIndex] = listsLength[listIndex];
		
		// if (i==PRINT_PARTICLE) {
			// printf("%d: friend list indices %d: (%d,%d)\n",i,listIndex,startIndex,lengthIndex);
		// }
	}
	
	// if (i==PRINT_PARTICLE) {
		// printf("%d: neighbors %d = %d\n",i,refNumNeighbors,testNumNeighbors);
	// }
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
