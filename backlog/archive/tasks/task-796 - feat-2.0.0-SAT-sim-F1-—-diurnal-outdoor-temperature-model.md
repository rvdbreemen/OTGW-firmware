---
id: TASK-796
title: 'feat-2.0.0: SAT sim F1 — diurnal outdoor temperature model'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 22:55'
updated_date: '2026-06-01 06:54'
labels:
  - sat
  - simulation
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up F1 from SAT simulation plan (docs/plan/SAT_SIMULATION_CONTRACT_PLAN.md section 12). Replace the constant fSimOutdoorTemp with a diurnal model: sine with configurable amplitude/phase, or pulled from state.sat.weather.fTemperature when valid. Depends on TASK-795. Milestone 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 fSimOutdoorTemp follows a diurnal sine (configurable mean/amplitude/phase consts) in satUpdateSimulation() when bSimulation=true and no valid weather data
- [x] #2 When state.sat.weather.bValid, the fetched weather temperature takes precedence over the synthetic sine (mirrors satGetOutsideTemp real-path precedence)
- [x] #3 No NTP dependency hard-failure: falls back to a free-running phase if wall-clock time is unavailable
- [x] #4 python build.py both targets per-env SUCCESS; evaluate.py --quick no new findings
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Source: plan docs/plan/SAT_SIMULATION_CONTRACT_PLAN.md §12 F1 + §3 (non-goals: dynamic outdoor temp deferred to this task). Depends on TASK-795 (sim boiler model committed).

GOAL: replace the constant state.sat.fSimOutdoorTemp (SATtypes.h, default 5.0f) with a diurnal model so heating-curve recommendation + weather-comp paths see realistic day/night swing under simulation.

DESIGN:
- Add a sine model in satUpdateSimulation() (SATcontrol.ino ~3147): fSimOutdoorTemp = mean + amplitude * sin(2*pi*(tod - phase)/86400), where tod = seconds-into-day from NTP (use existing time source; if no NTP, fall back to a free-running millis()/1000 phase so the bench still oscillates).
- Source priority: when state.sat.weather.bValid (weather API fetched), prefer state.sat.weather.fTemperature over the synthetic sine (matches satGetOutsideTemp() real-path precedence). Only synthesize when no weather data.
- Tuning consts near the SAT_SIM_* block (SATcontrol.ino ~63): SAT_SIM_OUTDOOR_MEAN (e.g. 8.0f), SAT_SIM_OUTDOOR_AMPLITUDE (e.g. 6.0f -> 2..14C swing), SAT_SIM_OUTDOOR_PHASE_S (e.g. coldest ~05:00). Static const for MVP per plan §6 (promote to settings only if beta asks).
- No new settings field unless amplitude/phase must be user-tunable (defer per YAGNI).

OPEN QUESTION (morning): should amplitude/phase be settings-configurable, or static consts sufficient for bench use? Default: static consts.

VERIFY: build.py both targets per-env SUCCESS; evaluate --quick clean; with bSimulation=true observe fSimOutdoorTemp varying over a simulated day (sim_outdoor_temp in REST/MQTT).
<!-- SECTION:PLAN:END -->
