# SciVis QA Benchmark — evaluation artifact

Benchmark of 3,947 question–answer pairs over 683 figures extracted from 114
peer-reviewed papers in 10 scientific domains, used to measure how well
vision-language models understand figures from scientific simulation and
visualization work.

## Contents

| File | Role |
|---|---|
| `qa_dataset.jsonl` | The benchmark: one record per figure, with its QA pairs |
| `by_category/*.jsonl` | The same QA pairs split by category (L1–L6) |
| `metadata.json` | Dataset statistics |
| `references.csv` / `references.md` | Source paper for every figure |
| `config.yaml` | Configuration for all three execution stages |
| `generate_qa.py` | Dataset construction (rule-based extraction from captions) |
| `eval_scivis_benchmark.py` | **T1** — VLM inference |
| `judge_free_text.py` | **T2** — LLM-as-judge scoring of open-ended answers |
| `aggregate_results.py` | **T3** — result tables |
| `make_reference_list.py` | Rebuilds `references.{csv,md}` from the paper sources |

## Composition

3,947 QA pairs across three answer formats:

| Format | Count | Scoring |
|---|---:|---|
| `yes_no` | 1,366 | Exact match, auto-scored in T1 |
| `multiple_choice` | 972 | Letter match, auto-scored in T1 |
| `free_text` | 1,609 | LLM-as-judge, 0–2, in T2 — **1,479 judged**, see below |

Of the 1,609 free-text items, 1,479 are judged: `L1_perception` (130) is
excluded, matching the published local-model runs. It asks "How many panels
does this figure contain?" but is phrased so models answer it as a binary,
which is why both local models score 0% on it. 1,609 − 130 = 1,479 is the
"~1,480 open-ended responses per model" in the artifact description, and it
reproduces the open-ended sample sizes in Table 2's Gemma and Qwen columns.

Nine categories: `L1_yesno` (1366), `L2_multichoice` (857), `L2_description`
(683), `L5_domain` (267), `L3_comparison` (261), `L6_spatial` (238),
`L1_perception` (130), `L1_multichoice` (115), `L4_reasoning` (30).
Ten domains, led by scientific-visualization (225 figures), ml-simulation
(118), and geoscience (85). Difficulty: 2,294 easy / 1,356 medium / 297 hard.

## Installation

```bash
module load python/3.11.9
module load cuda-compat/13.0          # required: driver 570.x needs this for torch 2.11+cu130
export HF_HOME=/work/hdd/bekn/hxu13/hf_cache
export HF_HUB_DISABLE_XET=1

pip install torch==2.11.0+cu130 torchvision==0.26.0+cu130 transformers==5.5.0
pip install pyyaml pillow einops
pip install anthropic                 # T2 only; not currently installed
```

`torchvision` must match `torch` exactly, or model loading fails with
`RuntimeError: operator torchvision::nms does not exist`. Without
`cuda-compat/13.0`, `torch.cuda.is_available()` silently returns `False`.
There is no GPU on the login node — test CUDA only inside an `srun` allocation.

Set `paths.collection_root` in `config.yaml` if the collection moves; every
`image_path` in the dataset is resolved relative to it.

## Artifact execution

### T1 — VLM inference

```bash
srun --account=bekn-dtai-gh --partition=ghx4 --nodes=1 --gpus=1 \
     --cpus-per-task=16 --mem=80G --time=4:00:00 \
     python eval_scivis_benchmark.py --model qwen3.5-vl
```

Models load in bf16 with `device_map="auto"` and decode greedily
(`do_sample=False`, `num_beams=1`). Writes `results/<model_key>.jsonl`, one
record per question, with binary and multiple-choice answers auto-scored
inline.

The output file is an append-only checkpoint flushed and `fsync`ed after every
record — re-running the same command skips completed questions, so a job killed
by the wall clock resumes rather than restarting. `--limit N` caps the run,
`--dry-run` prints prompts without loading a model, `--overwrite` starts fresh.

Per-model configuration in `config.yaml` covers two things that silently
produce garbage if wrong:

- **`chat_template_kwargs`** — Qwen3.5 requires `enable_thinking: false`. With
  thinking on, the token budget is consumed by the reasoning preamble and the
  answer never appears.
- **`dtype_kwarg`** — some loaders expect `dtype`, others only honour
  `torch_dtype` and load fp32 when given the wrong one. The script prints the
  loaded parameter width and warns if it is not 2 bytes.

`trust_remote_code` models are the most fragile part of this stage: they can
fail per-item without raising. The script records a per-record `error` field
and the coverage table in T3 reports how many items failed, so a silent
collapse shows up as a coverage number rather than a plausible-looking score.

### T2 — LLM-as-judge

```bash
python judge_free_text.py --model qwen3.5-vl            # concurrent
python judge_free_text.py --model qwen3.5-vl --batch    # Batches API, 50% cheaper
```

Scores each of the ~1,609 open-ended answers per model 0–2 against the
reference answer using `claude-sonnet-4-6`. Writes
`judged/<model_key>.jsonl`, resumable on the same basis as T1.

- Thinking is left unset (on Sonnet 4.6 that means off) and `effort` is `low` —
  a rubric-scoring task does not need more, and it materially cuts cost at this
  volume.
- The rubric is sent as a cached system prompt. Sonnet 4.6's minimum cacheable
  prefix is 1,024 tokens; if the rubric is shorter the cache is a silent no-op,
  so check `usage.cache_read_input_tokens` on a real run before assuming it
  landed.
- Sonnet 4.6 does not support the structured-outputs API, so scores are parsed
  from the reply defensively (raw JSON → fenced JSON → embedded `"score": N` →
  bare digit). Unparseable replies are recorded with `judge_score: null` and
  counted in the T3 coverage table rather than silently dropped.
- `--batch` uses the Message Batches API: half price, results within 24h.
- Credentials resolve as the SDK does — `ANTHROPIC_API_KEY`, then
  `ANTHROPIC_AUTH_TOKEN`, then an `ant auth login` profile. If no key is set,
  run `ant auth status`; an active profile needs no environment variable.

The rubric instructs the judge to grade meaning rather than wording, not to
penalise brevity, and to treat a malformed reference answer as such — the
reference answers are auto-extracted from captions and some are truncated or
answer a slightly different question than the one asked.

### T3 — Aggregation

```bash
python aggregate_results.py                       # all models with results
python aggregate_results.py --models qwen3.5-vl   # a subset
```

Writes CSV and markdown to `tables/`: `overall`, `by_domain` (per-domain ×
per-question-type, i.e. Table 2), `by_category`, `by_difficulty`, and
`coverage`.

Scoring conventions:

- `yes_no` / `multiple_choice` — accuracy in [0,1]. An answer the parser could
  not resolve counts as **incorrect**, and the parse rate is reported
  separately in `coverage`, so a low score caused by an unparseable output
  format is distinguishable from a low score caused by wrong answers.
- `free_text` — the judge's 0–2 score rescaled to [0,1]. Items the judge could
  not score are excluded from the mean and counted in `coverage`.

## Notes on the benchmark as specified

Five things worth knowing before reporting numbers from this artifact.

**The paper count is wrong; the free-text count was right.** Section 5.3, the
Table 2 caption, and the artifact description all state **95** papers. The
dataset actually draws on **114** paper folders (of 119 in the collection; five
contributed no captioned figures). Every other figure in those documents checks
out — all 683 figures, all 3,947 QA pairs, and every per-domain × per-format
sample size in Table 2 reproduces exactly from `qa_dataset.jsonl`. The "~1,480"
open-ended count is 1,479 and is correct once the `L1_perception` exclusion is
applied. The paper count is the one number to fix.

**The judge rubric is described inconsistently across the three documents.**
`paper/05-evaluation.tex` says open-ended responses are scored against a
**0–1** rubric; the artifact description and the local-model report say
**0–2**; the Gemini report specifies **0.0–1.0** with quarter-point anchors
(1.0 / 0.75 / 0.5 / 0.25 / 0.0). Both scales were genuinely used — 0–2 for the
two local models, 0–1 for Gemini — and each was normalised to a percentage
before entering Table 2. That is defensible, but as written the paper describes
only one of the two scales, so a reviewer comparing the open-ended columns is
told they share a rubric they do not share. This harness implements the 0–2
local rubric.

**The reference list needed to be built and is incomplete.** `references.csv`
and `references.md` are generated by `make_reference_list.py`. Titles and
author lines extract cleanly for essentially every paper, but a paper's own DOI
is only recoverable when the publisher stamped it into the front matter —
bibliography DOIs belong to other papers and are deliberately not searched.
**7 of 114 papers currently have a machine-recoverable identifier; the other
107 are flagged `needs_verification=True` and need a DOI or arXiv id filled in
by hand** before the list is publishable. The titles are sufficient to look
each one up.

**`max_new_tokens=15` applies to free text too, and that shapes the result.**
The config keeps 15 across all three formats to reproduce Table 2. Be explicit
about what the open-ended columns then measure: an `L2_description` answer is a
sentence or two, so at 15 tokens the judge is scoring a truncated fragment.
That is the direct cause of the reported "both models score 0 on ~60% of
free-text questions" — it is a decoding-budget artifact as much as a
comprehension result. Raising `free_text` to ~256 measures comprehension
instead, but those numbers no longer correspond to the published open-ended
columns. Run both if the distinction matters to a reviewer.

**Unparseable answers are dropped from the denominator, not scored zero.**
`scoring.unparsed_policy: exclude` reproduces the published tables — Gemma's
multiple-choice n is 874, not 972, because 98 answers were unparseable. Under
`incorrect` (full denominator) the published overalls become 41.5% for Gemma
and 42.6% for Qwen, against 44.1% and 44.0% as published. The Gemma/Qwen tie
survives either way, but the gap to Gemini widens, and under `exclude` a model
is effectively rewarded for abstaining. `aggregate_results.py` prints both
columns so the choice is visible rather than buried in the config.

**Runtime is close to the 4-hour wall clock.** 3,947 questions per model, with
1,609 of them generating up to 256 tokens, lands in the 2–3 hour range on one
GH200 for a mid-size model. Checkpointing makes this safe, but budget for a
second job on the larger models. Prefer ≤31B in bf16 on a single GH200 — 72B
with 4-bit has failed silently here before.

**58% of the benchmark is yes/no or multiple choice, and many free-text
reference answers are verbatim caption text.** A model that has learned caption
*style* can score well on those without reading the figure closely. The levels
that most directly test image understanding — `L4_reasoning` in particular —
are the smallest (30 items), which is too few to separate models reliably.
Read `by_category` alongside `overall`, and treat the `L4_reasoning` column as
indicative rather than conclusive.

**Table 2 mixes two different pipelines.** Gemma and Qwen ran through this
local harness; Gemini 3.1 Flash Lite ran through a separate cloud path
(Google Generative Language API, `temperature=0.0`, `maxOutputTokens=1024`, all
questions for a figure bundled into one call — 683 requests, not 3,947). The
two pipelines differ in decoding budget (15 vs 1,024 tokens), judge rubric
(0–2 vs 0–1), and open-ended denominator (1,479 vs 1,609). Some of Gemini's
lead on the open-ended columns is attributable to those differences rather than
to the model. **This harness does not implement the cloud path**, so it
reproduces the Gemma and Qwen columns of Table 2 but not the Gemini column.

## Rebuilding the dataset

`generate_qa.py` regenerates `qa_dataset.jsonl` from the figure markdown
sidecars. Its `BASE` constant on line 15 still points at
`/home/ubuntu/Desktop/paper_collection`; update it to the current collection
root before running, or it will find zero figures.
