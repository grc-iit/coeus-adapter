@CLAUDE.md 

Let's build an HDF5 assimilator path based on omni/format/hdf5_dataset_client.cc

Identify each dataset in the HDF5 file. We will use serial HDF5, not parallel, to avoid MPI dependency.

For each dataset, we will:
1. Create a tag for the specific dataset. It should be globally unique, so it should include the url (minus hdf5::).
2. Create a blob named description that will store the format of the dataset. The format should be a human-readable string roughly in the format: tensor<type, dim1, dim2, ...>.
3. divide into chunks, where each chunk is up to 1MB in size.
