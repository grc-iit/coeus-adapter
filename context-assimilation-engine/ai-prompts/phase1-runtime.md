@CLAUDE.md We want to make the code under omni match our other repos, following a C++ style instead of C. We will use google C++ style guide for this.

# CAE chimod

Let's create a subdirectory called chimods. This will be a chimaera repo. We will create a chimod named cae in this chimod repo. The namespace of the repo should be cae. Please read @docs/runtime/MODULE_DEVELOPMENT_GUIDE.md to see how to initially structure a chimod and repo.

The chimod should expose the following custom methods:
1. ParseOmni: Takes as input a hshm::priv::string containing the contents of a YAML omni file. Based on this omni file, we will divide the omni file assimilation into smaller tasks and schedule them. The smaller tasks are called 

We will also create a utility script under cae/util named wrp_cae_omni. It will take as input the path to an omni file. This utility will call the client API for ParseOmni.

Create another utility script under cae/util named wrp_cae_launch that will simply call the Create method from the cae client you will create. The script should take as input the parameter local/dynamic indicating the type of pool query to use for Create. PoolQuery::Local or PoolQuery::Dynamic.

First and foremost, ensure this compiles
