__kernel void applyVorticityAndViscosity(const __global float4 *predicted,
    const __global float4 *velocities,
    __global float4 *deltaVelocities,
#if defined(USE_LINKEDCELL)
    const __global int *cells,
    const __global int *particles_list,
#else
    const __global int2 *radixCells,
    const __global int2 *foundCells,
#endif // USE_LINKEDCELL
    const int N) {
  const int i = get_global_id(0);
  if (i >= N) return;

  const int END_OF_CELL_LIST = -1;

  int current_cell[3];

  current_cell[0] = (int) ( (predicted[i].x - SYSTEM_MIN_X)
                            / CELL_LENGTH_X );
  current_cell[1] = (int) ( (predicted[i].y - SYSTEM_MIN_Y)
                            / CELL_LENGTH_Y );
  current_cell[2] = (int) ( (predicted[i].z - SYSTEM_MIN_Z)
                            / CELL_LENGTH_Z );

  float4 viscosity_sum = (float4) 0.0f;
  float3 omega_i = (float3) 0.0f;

  for (int x = -1; x <= 1; ++x) {
    for (int y = -1; y <= 1; ++y) {
      for (int z = -1; z <= 1; ++z) {
        int neighbour_cell[3];

        neighbour_cell[0] = current_cell[0] + x;
        neighbour_cell[1] = current_cell[1] + y;
        neighbour_cell[2] = current_cell[2] + z;

        if (neighbour_cell[0] < 0 || neighbour_cell[0] >= NUMBER_OF_CELLS_X ||
            neighbour_cell[1] < 0 || neighbour_cell[1] >= NUMBER_OF_CELLS_Y ||
            neighbour_cell[2] < 0 || neighbour_cell[2] >= NUMBER_OF_CELLS_Z) {
          continue;
        }

        uint cell_index = neighbour_cell[0] +
                          neighbour_cell[1] * NUMBER_OF_CELLS_X +
                          neighbour_cell[2] * NUMBER_OF_CELLS_X * NUMBER_OF_CELLS_Y;

#if defined(USE_LINKEDCELL)
        int next = cells[cell_index];

        while (next != END_OF_CELL_LIST) {
          if (i != next) {
            float4 r = predicted[i] - predicted[next];
            float r_length_2 = (r.x * r.x + r.y * r.y + r.z * r.z);

            if (r_length_2 > 0.0f && r_length_2 < PBF_H_2) {
              float4 v = velocities[next] - velocities[i];
              float poly6 = POLY6_FACTOR * (PBF_H_2 - r_length_2)
                            * (PBF_H_2 - r_length_2)
                            * (PBF_H_2 - r_length_2);

                // equation 15
                float r_length = sqrt(r_length_2);
                float4 gradient_spiky = -1.0f * r / (r_length)
                                        * GRAD_SPIKY_FACTOR
                                        * (PBF_H - r_length)
                                        * (PBF_H - r_length);
                // the gradient has to be negated because it is with respect to p_j
                // this could be done directly when calculating it, but for now we explicitly
                // keep it to improve understanding
                omega_i += cross(v.xyz,-gradient_spiky.xyz);

              viscosity_sum += (1.0f / predicted[next].w) * v * poly6;

              // #if defined(USE_DEBUG)
              // printf("viscosity: i,j: %d,%d result: [%f,%f,%f] density: %f\n", i, next,
              //        v.x, v.y, v.z, predicted[j].w);
              // #endif // USE_DEBUG
            }
          }

          next = particles_list[next];
        }
#else
        int2 cellRange = foundCells[cell_index];
        if (cellRange.x == END_OF_CELL_LIST) continue;

        for (uint n = cellRange.x; n <= cellRange.y; ++n) {
          const int next = radixCells[n].y;

          if (i != next) {
            float4 r = predicted[i] - predicted[next];
            float r_length_2 = (r.x * r.x + r.y * r.y + r.z * r.z);

            if (r_length_2 > 0.0f && r_length_2 < h2) {
              float4 v = velocities[next] - velocities[i];
              float poly6 = poly6_factor * (h2 - r_length_2)
                            * (h2 - r_length_2)
                            * (h2 - r_length_2);

              viscosity_sum += (1.0f / predicted[next].w) * v * poly6;

              // #if defined(USE_DEBUG)
              // printf("viscosity: i,j: %d,%d result: [%f,%f,%f] density: %f\n", i, next,
              //        v.x, v.y, v.z, predicted[j].w);
              // #endif // USE_DEBUG
            }
          }
        }
#endif
      }
    }
  }

  const float c = 0.00f;
  deltaVelocities[i] = c * viscosity_sum;

  // vorticity
    float omega_length = length(omega_i);
    float3 eta = (float3)0.0f;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            for (int z = -1; z <= 1; ++z) {
                int neighbour_cell[3];

                neighbour_cell[0] = current_cell[0] + x;
                neighbour_cell[1] = current_cell[1] + y;
                neighbour_cell[2] = current_cell[2] + z;

                if (neighbour_cell[0] < 0 || neighbour_cell[0] >= NUMBER_OF_CELLS_X ||
                    neighbour_cell[1] < 0 || neighbour_cell[1] >= NUMBER_OF_CELLS_Y ||
                    neighbour_cell[2] < 0 || neighbour_cell[2] >= NUMBER_OF_CELLS_Z) {
                  continue;
                }

                uint cell_index = neighbour_cell[0] +
                                  neighbour_cell[1] * NUMBER_OF_CELLS_X +
                                  neighbour_cell[2] * NUMBER_OF_CELLS_X * NUMBER_OF_CELLS_Y;

                int next = cells[cell_index];

                while (next != END_OF_CELL_LIST) {
                    if (i != next) {
                        float4 r = predicted[i] - predicted[next];
                        float r_length_2 = (r.x * r.x + r.y * r.y + r.z * r.z);

                        if (r_length_2 > 0.0f && r_length_2 < PBF_H_2) {
                            float r_length = sqrt(r_length_2);
                            float4 gradient_spiky = -1.0f * r / (r_length)
                                                    * GRAD_SPIKY_FACTOR
                                                    * (PBF_H - r_length)
                                                    * (PBF_H - r_length);

                            eta += (omega_length / predicted[next].w) * gradient_spiky.xyz;
                        }
                    }

                  next = particles_list[next];
                }
            }
        }
    }

    float3 eta_N = normalize(eta);
    const float epsilon = 0.000001f;
    float3 vorticityForce = epsilon * cross(eta_N, omega_i);
    float3 vorticityVelocity = vorticityForce * TIMESTEP;
    // if(i==0) {
    //     printf("VORTICITY: %d velocity:[%f,%f,%f]\n", i, vorticityVelocity.x, vorticityVelocity.y, vorticityVelocity.z);
    // }
    deltaVelocities[i] += (float4)(vorticityVelocity.x, vorticityVelocity.y, vorticityVelocity.z, 0.0f); 

  // float4 gradient_spiky = -1.0f * r / (r_length)
  //                                       * GRAD_SPIKY_FACTOR
  //                                       * (PBF_H - omega_length)
  //                                       * (PBF_H - omega_length);

  // #if defined(USE_DEBUG)
  // printf("viscosity: i: %d sum:%f result: [%f,%f,%f]\n", i,
  //        c * viscosity_sum.x, c * viscosity_sum.y, c * viscosity_sum.z);
  // #endif // USE_DEBUG
}
