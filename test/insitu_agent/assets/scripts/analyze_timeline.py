#!/usr/bin/env python3
"""
Align the trigger-render-reason pipeline onto a single wall clock and print a
per-stage timeline. All sources use system epoch seconds (engine
system_clock == Python time.time()), so writer, bridge, and agent events line
up directly (assumes NTP-synced nodes).

Sources:
  <pkg_dir>/trigger_timeline.jsonl   engine (rank 0): sim_start, step_begin,
                                     output, fire, ship        [instrumented]
  <RES>/streaming_timing.jsonl       bridge: per flagged step sst/pull/render
  <RES>/frames/frames.jsonl          bridge: per frame saved (wall)
  <RES>/mcp_tool_timing.jsonl        MCP: per tool call (wall)
  <RES>/token_usage.json             agent: start_epoch, end_epoch (verdict)

Usage:
  analyze_timeline.py <RES_dir> [<trigger_timeline.jsonl>]
"""
import json, sys, os

def jl(p):
    out = []
    try:
        for line in open(p):
            line = line.strip()
            if line:
                out.append(json.loads(line))
    except FileNotFoundError:
        pass
    return out

RES = sys.argv[1]
TL = sys.argv[2] if len(sys.argv) > 2 else os.path.join(RES, "trigger_timeline.jsonl")

eng = jl(TL)
btime = jl(os.path.join(RES, "streaming_timing.jsonl"))
frames = jl(os.path.join(RES, "frames", "frames.jsonl"))
mcp = jl(os.path.join(RES, "mcp_tool_timing.jsonl"))
llm = jl(os.path.join(RES, "llm_call_timing.jsonl"))
tok = json.load(open(os.path.join(RES, "token_usage.json")))

T0 = next(e["wall"] for e in eng if e["event"] == "sim_start")
rel = lambda w: w - T0
ev = {}
for e in eng:
    ev.setdefault(e["event"], []).append(e)
begins = {e["step"]: e["wall"] for e in ev.get("step_begin", [])}
outs = {e["step"]: e["wall"] for e in ev.get("output", [])}
fire = ev["fire"][0]

print(f"=== ALIGNED TIMELINE (T=0 at engine sim_start, wall {T0:.3f}) ===\n")
print("WRITER — per output step (begin=compute start, produced=data+derived in CTE):")
prev = None
for s in sorted(begins):
    b, o = begins[s], outs.get(s)
    cad = f"{b-prev:4.1f}s" if prev else "  -  "
    tag = "  <-- FIRE" if s == fire["step"] else ("  (window: ship Blocks)" if 8 <= s <= 11 else "")
    comp = f"{o-b:4.1f}s" if o else "  -  "
    print(f"  step {s:2d}: begin T+{rel(b):6.1f}s  produced T+{rel(o):6.1f}s  compute {comp}  cadence {cad}{tag}")
    prev = b

free = [begins[s+1]-begins[s] for s in range(1, 7) if s+1 in begins]
freecad = sum(free)/len(free)
win = [begins[s+1]-begins[s] for s in range(8, 11) if s+1 in begins]
print(f"\n  mean cadence pre-fire: {freecad:.2f}s/output   in window: {sum(win)/len(win):.2f}s/output")
print(f"\nFIRE step {fire['step']}: T+{rel(fire['wall']):.1f}s  variance={fire['value']:.6f}")
print("SHIP (writer->SST, Block; widening gap = waiting on bridge):")
for e in ev["ship"]:
    print(f"  step {e['step']:2d}: T+{rel(e['wall']):6.1f}s  window_left={int(e['value'])}")
print("\nBRIDGE — receive+render each flagged step -> frame on disk:")
for b, f in zip(btime, frames):
    print(f"  #{b['step']}: T+{rel(b['timestamp']):6.1f}s  sst={b['sst_wait_ms']/1000:.2f}s "
          f"pull={b['pipeline_update_ms']/1000:.2f}s render={b['render_ms']/1000:.2f}s")
print("\nAGENT:")
print(f"  start:  T+{rel(tok['start_epoch']):6.1f}s")
if mcp:
    print(f"  get_flagged_frames returned: T+{rel(mcp[0]['timestamp']):6.1f}s "
          f"(blocked {mcp[0]['mcp_total_ms']/1000:.1f}s for the window)")
print(f"  VERDICT: T+{rel(tok['end_epoch']):6.1f}s  ({tok['num_tool_calls']} tool / "
      f"{tok['num_llm_calls']} LLM calls, {tok['wall_time_s']}s, ${tok['estimated_cost_usd']})")

# Split the agent's wall time into reasoning vs tool work. llm_total_s comes from
# token_usage.json (newer runs); fall back to summing the per-call records.
n_llm = tok.get("num_llm_calls", 0)
llm_s = tok.get("llm_total_s")
if llm_s is None and llm:
    llm_s = sum(c.get("llm_ms", 0) for c in llm) / 1000
if llm_s is not None:
    tool_s = sum(m.get("mcp_total_ms", 0) for m in mcp) / 1000
    mean = llm_s / n_llm if n_llm else 0.0
    print(f"  LLM reasoning: {llm_s:6.1f}s over {n_llm} calls (mean {mean:.1f}s/call)")
    print(f"  MCP tools:     {tool_s:6.1f}s   loop/other: "
          f"{tok['wall_time_s'] - llm_s - tool_s:.1f}s")
    if llm:
        per = "  ".join(f"#{c['call']}:{c['llm_ms']/1000:.1f}s" for c in llm)
        print(f"  per call: {per}")

sim_end = outs[max(outs)]
stall = sum(max(0, (begins[s+1]-begins[s]) - freecad) for s in range(8, 12) if s+1 in begins)
print("\n=== KEY INTERVALS ===")
print(f"  sim_start -> fire:                {rel(fire['wall']):.1f}s")
print(f"  fire -> last frame on disk:       {frames[-1]['timestamp']-fire['wall']:.1f}s")
print(f"  fire -> agent verdict:            {tok['end_epoch']-fire['wall']:.1f}s")
print(f"  sim_start -> sim done (output 20):{rel(sim_end):.1f}s")
print(f"  agent verdict vs sim done: agent {rel(sim_end)-rel(tok['end_epoch']):+.1f}s "
      f"{'BEFORE' if tok['end_epoch'] < sim_end else 'AFTER'} the sim finished")
print(f"  added sim stall during window:    ~{stall:.1f}s  (vs ~65s in synchronous mode)")
