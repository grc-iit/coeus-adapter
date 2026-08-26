# Check for OS (Windows, Linux, Mac OS)
ifeq ($(OS),Windows_NT)
	DETECTED_OS:= Windows
else
	DETECTED_OS:= $(shell uname)
endif

# Set compiler and flags
ifeq ($(DETECTED_OS),Windows)
CXX= g++
else
CXX= mpicxx
endif
CXXFLAGS+= -std=c++14

# Set application executable name
APP= lbmcfd

# Set source and output directories
SRCDIR= src
OBJDIR= obj
BINDIR= bin

# Set up include and libray directories
ifeq ($(DETECTED_OS),Windows)
	MPI_INC= $(patsubst %\,%,$(MSMPI_INC))
	MPI_LIB= $(patsubst %\,%,$(MSMPI_LIB64))

	INC= -I"$(MPI_INC)" -I"$(MPI_INC)\x64" -I.\include
	LIB= -L"$(MPI_LIB)" -lmsmpi
else
	INC= -I./include
	LIB= 
endif

# Link Ascent if ASCENT_DIR is set
ifneq ($(ASCENT_DIR),)
    include $(ASCENT_DIR)/share/ascent/ascent_config.mk
    ASCENT_INC_FLAGS= $(ASCENT_INCLUDE_FLAGS)
    ASCENT_LNK_FLAGS= $(ASCENT_LINK_RPATH) $(ASCENT_MPI_LIB_FLAGS)
    CXXFLAGS += -DASCENT_ENABLED
endif

# Link ADIOS2 if ADIOS2_DIR is set (SST streaming / BP5 file output).
# ADIOS2_DIR is the install prefix, e.g. `spack location -i adios2-coeus@vigil`.
ifneq ($(ADIOS2_DIR),)
    ADIOS2_INC_FLAGS= $(shell $(ADIOS2_DIR)/bin/adios2-config --cxx-flags)
    ADIOS2_LNK_FLAGS= $(shell $(ADIOS2_DIR)/bin/adios2-config --cxx-libs)
    CXXFLAGS += -DADIOS2_ENABLED
endif

# Create output directories and set output file names
ifeq ($(DETECTED_OS),Windows)
	mkobjdir:= $(shell if not exist $(OBJDIR) mkdir $(OBJDIR))
	mkbindir:= $(shell if not exist $(BINDIR) mkdir $(BINDIR))

	OBJS= $(addprefix $(OBJDIR)\, main.o)
	EXEC= $(addprefix $(BINDIR)\, $(APP).exe)
else
	mkdirs:= $(shell mkdir -p $(OBJDIR) $(BINDIR))
	
	OBJS= $(addprefix $(OBJDIR)/, main.o)
	EXEC= $(addprefix $(BINDIR)/, $(APP))
endif


# BUILD EVERYTHING
all: $(EXEC)

$(EXEC): $(OBJS)
	$(CXX) -o $@ $^ $(LIB) $(ASCENT_LNK_FLAGS) $(ADIOS2_LNK_FLAGS)

ifeq ($(DETECTED_OS),Windows)
$(OBJDIR)\\%.o: $(SRCDIR)\%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INC) $(ASCENT_INC_FLAGS)
else
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INC) $(ASCENT_INC_FLAGS) $(ADIOS2_INC_FLAGS)
endif


# REMOVE OLD FILES
ifeq ($(DETECTED_OS),Windows)
clean:
	del $(OBJS) $(EXEC)
else
clean:
	rm -f $(OBJS) $(EXEC)
endif

# Generate PVD files for ParaView
pvd:
	@if ls paraview/*.vts >/dev/null 2>&1; then \
		echo "Generating PVD files for ParaView..."; \
		python3 create_pvd_from_vts.py --input-dir paraview; \
		echo "PVD files ready for ParaView."; \
	else \
		echo "No VTS files found in paraview/ - run simulation with --output-vts first"; \
	fi

.PHONY: all clean pvd
