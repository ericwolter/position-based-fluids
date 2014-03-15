@echo off

del .\src\code_resource.inc

set SourceMinifier=.\tools\SourceMinifier\SourceMinifier\bin\SourceMinifier

%SourceMinifier%   SwapSet.txt   .\assets\scenarios\dam_coarse.par           .\src\code_resource.inc

%SourceMinifier%   SwapSet.txt   .\assets\shaders\fluid_depth_smoothing.fs   .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\shaders\fluid_final_render.fs      .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\shaders\grid_build.cms             .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\shaders\grid_chain_reset.cms       .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\shaders\grid_reset.cms             .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\shaders\particles.vs               .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\shaders\particles_color.fs         .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\shaders\standard.vs                .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\shaders\standard_color.fs          .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\shaders\standard_copy.fs           .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\shaders\vis_scan.cms               .\src\code_resource.inc

%SourceMinifier%   SwapSet.txt   .\assets\kernels\hesp.hpp                   .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\parameters.hpp             .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\apply_viscosity.cl         .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\apply_vorticity.cl         .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\build_friends_list.cl      .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\compute_delta.cl           .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\compute_scaling.cl         .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\logging.cl                 .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\pack_data.cl               .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\predict_positions.cl       .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\radixsort.cl               .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\reset_grid.cl              .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\update_cells.cl            .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\update_positions.cl        .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\update_predicted.cl        .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\update_velocities.cl       .\src\code_resource.inc
%SourceMinifier%   SwapSet.txt   .\assets\kernels\utilities.cl               .\src\code_resource.inc