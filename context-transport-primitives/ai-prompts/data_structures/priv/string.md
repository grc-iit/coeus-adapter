@CLAUDE.md

Let's implement a hshm::priv::string class.

It should be similar to std::string, but using AllocT* as an input to each constructor.

We should use our hshm::priv::vector class internally to avoid duplicating effort.

Make Short String Optimization (SSO) a template parameter to the string. Let's 
say the default value to this is 32 bytes. If the string size < 

Ensure that both vector and string are GPU-compliant (i.e., using HSHM_CROSS_FUN macros)
