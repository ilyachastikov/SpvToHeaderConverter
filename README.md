# SpvToHeaderConverter

The SpvToHeaderConverter is utility to convert all SPIRV files in directory to C++ module ixx file with arrays of these SPIRVs.
It can run GLSLC to compile shader files to SPIRV files. The shader files must have ".vert", ".frag", ".comp", ".tesc", ".tese", ".rgen", ".rchit", ".rahit", ".rmiss", ".rint", ".rcall", ".mesh", ".task" extensions. To do this is needed to use "-compile_all" command line to compile all shders in directory  or "-compile_files" command with needed shaders files separated wih comma withot spaces to compile only needed shaders.
You can also to crate in directory configuration file with name "SpvToHeaderConverter.config". Two parameterss are available:
* "glslc_path" is path to GLSLC compiler
* "save_module_path" is path to place where ixx module must saved  

It needs C++ 20 to compile and CMAKE 3.21 to configure and can be run on Windows and Linux. 