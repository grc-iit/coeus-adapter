@CLAUDE.md 

Remove wrp_launch_cae from the core/util/wrp_cae_launch.cc from cmakes and the filesystem. We will instead be using chimaera_compose from now on. 

Document how to launch the cae with chimaera_compose in @docs/cae/launch.md. Include the paramaters to the CTE, but don't explain them.
The compose is documented in @docs/runtime/module_dev_guide.md.
The cte config is documented in @docs/cte/config.md.

in @docs/cae/omni.md, also document how to use wrp_cae_omni to process omni files after calling chimaera_compose 

