#!/bin/bash
# One-shot TGV in-situ agent launch for end-to-end verification (run on the
# consumer node, ares-comp-26, where pvserver+bridge live on :11112).
set -u
CAT=/mnt/common/hxu40/coeus/iowarp/coeus-adapter/test/real_apps/Xcompact3d/catalyst
AGENT=/mnt/common/hxu40/coeus/iowarp/coeus-adapter/test/insitu_agent/insitu_agent.py
RUNDIR=/mnt/common/hxu40/incompact3d/output/verify_run
PV=/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/paraview-5.13.3-ssmv5hp4czyfvuu5eps6s2ljpug7lkus
PY=/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/python-3.12.12-mmtjcqm5y4jajxdmcpgbzbnocjfk4afq/bin/python3

export ANTHROPIC_API_KEY=$(cat ~/.anthropic_key)
export PYTHONPATH="$PV/lib/python3.12/site-packages:/mnt/common/hxu40/software/paraview_mcp"
export LD_LIBRARY_PATH="$PV/lib"

exec "$PY" "$AGENT" \
  --provider anthropic --model claude-haiku-4-5-20251001 \
  --mcp-server-script "$CAT/tgv_insitu_mcp_server.py" \
  --system-prompt-file "$CAT/tgv_agent_system_prompt.txt" \
  --server-host localhost --server-port 11112 \
  --status-file "$CAT/tgv_streaming_status.json" \
  --screenshot-file "$CAT/tgv_bridge_view.png" \
  --results-dir "$RUNDIR" \
  --max-wall-seconds 420 \
  --prompt "A numerical-dissipation trigger just fired (Red: the under-resolved vortex cascade has begun). The stream is paused before the first inspect-window step. Advance through the 3 inspect-window steps one at a time: after each advance_step, check get_streaming_status until the step counter increases, then take a screenshot and describe the vortex structures. After the 3rd step, create a vort isosurface at a value you judge appropriate from what you saw, screenshot it, and summarize how the cascade evolved across the window."
